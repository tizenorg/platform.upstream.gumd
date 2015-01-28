/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "config.h"
#include <check.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/gum-dbus.h"
#include "common/gum-error.h"
#include "common/gum-log.h"
#include "common/gum-defines.h"
#include "common/gum-config.h"
#include "common/gum-crypt.h"
#include "common/gum-file.h"
#include "common/gum-user-types.h"
#include "common/gum-group-types.h"
#include "common/dbus/gum-dbus-user-service-gen.h"
#include "common/dbus/gum-dbus-user-gen.h"
#include "common/dbus/gum-dbus-group-service-gen.h"
#include "common/dbus/gum-dbus-group-gen.h"
#include "gum-user.h"
#include "gum-user-service.h"
#include "gum-group.h"

#ifdef GUM_BUS_TYPE_P2P
#  ifdef GUM_SERVICE
#    undef GUM_SERVICE
#  endif
#  define GUM_SERVICE NULL
#endif

gchar *exe_name = 0;

GPid daemon_pid = 0;

static GMainLoop *main_loop = NULL;

gboolean
_create_file (
        const gchar *filename,
        const gchar *orig)
{
    struct stat st;
    gboolean created = TRUE;
    if (stat (filename, &st) < 0) {
        if (!orig) return FALSE;
        gchar *cmd = g_strdup_printf ("cp -p %s %s", orig, filename);
        if (!cmd) return FALSE;
        created = (system(cmd) == 0);
        g_free (cmd);
    }
    return created;
}

static void
_setup_env (void)
{
    gchar *fpath = NULL;
    gchar *cmd = NULL;

    fail_if (g_setenv ("UM_CONF_FILE", GUM_TEST_DATA_DIR, TRUE) == FALSE);
    fail_if (g_setenv ("UM_DAEMON_TIMEOUT", "10", TRUE) == FALSE);
    fail_if (g_setenv ("UM_USER_TIMEOUT", "10", TRUE) == FALSE);
    fail_if (g_setenv ("UM_GROUP_TIMEOUT", "10", TRUE) == FALSE);
    fail_if (g_setenv ("UM_PASSWD_FILE", "/tmp/gum/passwdtest", TRUE) ==
            FALSE);
    fail_if (g_setenv ("UM_SHADOW_FILE", "/tmp/gum/shadowtest", TRUE) ==
            FALSE);
    fail_if (g_setenv ("UM_GROUP_FILE", "/tmp/gum/grouptest", TRUE) == FALSE);
    fail_if (g_setenv ("UM_GSHADOW_FILE", "/tmp/gum/gshadowtest", TRUE) ==
            FALSE);
    fail_if (g_setenv ("UM_HOMEDIR_PREFIX", "/tmp/gum/home", TRUE) == FALSE);
    fail_if (g_setenv ("UM_SKEL_DIR", "/tmp/gum/skel", TRUE) == FALSE);

    if (system("rm -rf /tmp/gum") != 0)
        WARN("failed to remove tmp gum directory");

    fail_if (system("mkdir -m +w -p /tmp/gum") != 0,
            "Failed to create temp gum dir: %s\n", strerror(errno));

    fpath = g_build_filename (GUM_TEST_DATA_DIR, "skel", NULL);
    cmd = g_strdup_printf ("cp -p -r %s/ /tmp/gum/", fpath);
    g_free (fpath);
    fail_if (system(cmd) != 0, "Failed to copy temp skel dir: %s\n",
            strerror(errno));
    g_free (cmd);
    fail_if (system("chmod -R +w /tmp/gum/skel") != 0);

    fpath = g_build_filename (GUM_TEST_DATA_DIR, "passwd", NULL);
    fail_if (_create_file (g_getenv("UM_PASSWD_FILE"), fpath) == FALSE);
    g_free (fpath);

    fpath = g_build_filename (GUM_TEST_DATA_DIR, "shadow", NULL);
    fail_if (_create_file (g_getenv("UM_SHADOW_FILE"), fpath) == FALSE);
    g_free (fpath);

    fpath = g_build_filename (GUM_TEST_DATA_DIR, "group", NULL);
    fail_if (_create_file (g_getenv("UM_GROUP_FILE"), fpath) == FALSE);
    g_free (fpath);

    fpath = g_build_filename (GUM_TEST_DATA_DIR, "gshadow", NULL);
    fail_if (_create_file (g_getenv("UM_GSHADOW_FILE"), fpath) == FALSE);
    g_free (fpath);
}

static void
_unset_env (void)
{
    fail_if (system("rm -rf /tmp/gum") == -1);
    g_unsetenv ("UM_SKEL_DIR");
    g_unsetenv ("UM_HOMEDIR_PREFIX");
    g_unsetenv ("UM_PASSWD_FILE");
    g_unsetenv ("UM_SHADOW_FILE");
    g_unsetenv ("UM_GROUP_FILE");
    g_unsetenv ("UM_GSHADOW_FILE");
    g_unsetenv ("UM_CONF_FILE");
    g_unsetenv ("UM_GROUP_TIMEOUT");
    g_unsetenv ("UM_USER_TIMEOUT");
    g_unsetenv ("UM_DAEMON_TIMEOUT");
}

static void
_stop_mainloop ()
{
    if (main_loop) {
        g_main_loop_quit (main_loop);
    }
}

static void
_create_mainloop ()
{
    if (main_loop == NULL) {
        main_loop = g_main_loop_new (NULL, FALSE);
    }
}

static void
_run_mainloop ()
{
    if (main_loop)
        g_main_loop_run (main_loop);
}

static void
_setup_daemon (void)
{
    DBG ("Programe name : %s\n", exe_name);

    _setup_env ();

    GError *error = NULL;
#ifdef GUM_BUS_TYPE_P2P
    /* start daemon maually */
    gchar *argv[2];
    gchar *test_daemon_path = g_build_filename (g_getenv("UM_BIN_DIR"),
            "gumd", NULL);
    fail_if (test_daemon_path == NULL, "No UM daemon path found");

    argv[0] = test_daemon_path;
    argv[1] = NULL;
    g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
            &daemon_pid, &error);
    g_free (test_daemon_path);
    fail_if (error != NULL, "Failed to span daemon : %s",
            error ? error->message : "");
    sleep (5); /* 5 seconds */
#else
    /* session bus where no GTestBus support */
    GIOChannel *channel = NULL;
    gchar *bus_address = NULL;
    gint tmp_fd = 0;
    gint pipe_fd[2];
    gchar *argv[] = {"dbus-daemon", "--config-file=<<conf-file>>",
            "--print-address=<<fd>>", NULL};
    gsize len = 0;
    const gchar *dbus_monitor = NULL;

    argv[1] = g_strdup_printf ("--config-file=%s%s", GUM_TEST_DATA_DIR,
            "test-gumd-dbus.conf");

    if (pipe(pipe_fd)== -1) {
        WARN("Failed to open temp file : %s", error->message);
        argv[2] = g_strdup_printf ("--print-address=1");
        g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
                NULL, &daemon_pid, NULL, NULL, &tmp_fd, &error);
    } else {
        tmp_fd = pipe_fd[0];
        argv[2] = g_strdup_printf ("--print-address=%d", pipe_fd[1]);
        g_spawn_async(NULL, argv, NULL,
                G_SPAWN_SEARCH_PATH|G_SPAWN_LEAVE_DESCRIPTORS_OPEN, NULL, NULL,
                &daemon_pid, &error);
    }
    fail_if (error != NULL, "Failed to span daemon : %s",
            error ? error->message : "");
    fail_if (daemon_pid == 0, "Failed to get daemon pid");
    g_free (argv[1]);
    g_free (argv[2]);
    sleep (5); /* 5 seconds */

    channel = g_io_channel_unix_new (tmp_fd);
    g_io_channel_read_line (channel, &bus_address, NULL, &len, &error);
    fail_if (error != NULL, "Failed to daemon address : %s",
            error ? error->message : "");
    g_io_channel_unref (channel);
    
    if (pipe_fd[0]) close (pipe_fd[0]);
    if (pipe_fd[1]) close (pipe_fd[1]);

    if (bus_address) bus_address[len] = '\0';
    fail_if(bus_address == NULL || strlen(bus_address) == 0);

    if (GUM_BUS_TYPE == G_BUS_TYPE_SYSTEM)
        fail_if (g_setenv("DBUS_SYSTEM_BUS_ADDRESS", bus_address, TRUE)
                == FALSE);
    else
        fail_if (g_setenv("DBUS_SESSION_BUS_ADDRESS", bus_address, TRUE)
                == FALSE);

    DBG ("Daemon Address : %s\n", bus_address);
    g_free (bus_address);

    if ((dbus_monitor = g_getenv("UM_DBUS_DEBUG")) != NULL && g_strcmp0 (
            dbus_monitor, "0")) {
    	/* start dbus-monitor */
    	char *argv[] = {"dbus-monitor", "<<bus_type>>", NULL };
        argv[1] = GUM_BUS_TYPE == G_BUS_TYPE_SYSTEM ? "--system" : "--session";
    	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
    	        &error);
    	if (error) {
    		DBG ("Error while running dbus-monitor : %s", error->message);
    		g_error_free (error);
    	}
    }
#endif

    DBG ("Daemon PID = %d\n", daemon_pid);
}

static void
_teardown_daemon (void)
{
    if (daemon_pid) kill (daemon_pid, SIGTERM);

    _unset_env ();
}

static void
_on_user_op_success (
        GumUser *user,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error != NULL, "failed user operation : %s",
            error ? error->message : "(null)");
}

static void
_on_user_op_failure (
        GumUser *user,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error == NULL, "user operation should have been failed");
}

static void
_on_user_service_op_success (
        GumUserService *service,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error != NULL, "failed user service operation : %s",
            error ? error->message : "(null)");
}

static void
_on_group_op_success (
        GumGroup *group,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();
    fail_if (error != NULL, "failed group operation : %s",
            error ? error->message : "(null)");
}

static void
_on_group_op_failure (
        GumGroup *user,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error == NULL, "group operation should have been failed");
}

/*
 * User test cases
 */
START_TEST (test_create_new_user)
{
    DBG ("\n");
    GumUser *user = NULL, *user2 = NULL;

    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");

    _run_mainloop ();

    user2 = gum_user_create (_on_user_op_success, NULL);
    fail_if (user2 == NULL, "failed to create new user2");

    _run_mainloop ();

    g_object_unref (user);
    g_object_unref (user2);

    user2 = gum_user_create_sync (FALSE);
    fail_if (user2 == NULL, "failed to create new user sync");
    g_object_unref (user2);

    user2 = gum_user_create_sync (TRUE);
    fail_if (user2 == NULL, "failed to create new user sync -- offline");
    g_object_unref (user2);
}
END_TEST

START_TEST(test_add_user)
{
    GumUser *user = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: add with username */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "username", "test_adduser1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    /* case 2: add with nick */
    g_object_unref (user);
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "nickname", "nick1", "usertype",
            GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user with nick");
    _run_mainloop ();

    /* case 3: add existing user */
    rval = gum_user_add (user, _on_user_op_failure, NULL);
    fail_if (rval == FALSE, "failed to add already existing user");
    _run_mainloop ();

    g_object_unref (user);

    /* case 4: use sync method to add user*/
    user = gum_user_create_sync (TRUE);
    fail_if (user == NULL, "failed to create new user");

    g_object_set (G_OBJECT (user), "username", "test_adduser2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user sync");

    g_object_unref (user);
}
END_TEST

START_TEST(test_get_user_by_uid)
{
    GumUser *user = NULL;
    uid_t uid = GUM_USER_INVALID_UID;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    user = gum_user_get (uid, _on_user_op_failure, NULL);
    fail_if (user == NULL, "failed to get user");
    _run_mainloop ();
    g_object_unref (user);

    /* case 2: success */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "username", "test_getuser1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);
    fail_if (uid == GUM_USER_INVALID_UID, "Invalid uid for the user");
    g_object_unref (user);

    user = gum_user_get (uid, _on_user_op_success, NULL);
    fail_if (user == NULL, "failed to get user");
    _run_mainloop ();

    g_object_unref (user);

    /* case 3: get user sync */
    user = gum_user_create_sync (TRUE);
    fail_if (user == NULL, "failed to create new user -- offline");

    g_object_set (G_OBJECT (user), "username", "test_getuser2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user sync -- offline");

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);
    fail_if (uid == GUM_USER_INVALID_UID, "Invalid uid for the user");

    g_object_unref (user);

    user = gum_user_get_sync (uid, FALSE);
    fail_if (user == NULL, "failed to get user sync");

    g_object_unref (user);
}
END_TEST

START_TEST(test_get_user_by_name)
{
    GumUser *user = NULL;
    gchar *name = "test_getuser_byname1";
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    user = gum_user_get_by_name ("no_user", _on_user_op_failure, NULL);
    fail_if (user == NULL, "failed to get user by name");
    _run_mainloop ();
    g_object_unref (user);

    /* case 2: success */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "username", name,
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();
    g_object_unref (user);

    user = gum_user_get_by_name (name, _on_user_op_success, NULL);
    fail_if (user == NULL, "failed to get user by name");
    _run_mainloop ();

    g_object_unref (user);

    /* case 3: get user sync */
    user = gum_user_create_sync (FALSE);
    fail_if (user == NULL, "failed to create new user");

    g_object_set (G_OBJECT (user), "username", "test_getuser_byname2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user sync");
    g_object_unref (user);

    user = gum_user_get_by_name_sync ("test_getuser_byname2", FALSE);
    fail_if (user == NULL, "failed to get user by name sync");
    g_object_unref (user);
}
END_TEST

START_TEST (test_delete_user)
{
    GumUser *user = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    rval = gum_user_delete (user, TRUE, _on_user_op_failure, NULL);
    fail_if (rval == FALSE, "failed to delete user");
    _run_mainloop ();

    /* case 2: success with remdir = TRUE */
    g_object_set (G_OBJECT (user), "username", "test_deluser1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    rval = gum_user_delete (user, TRUE, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to delete user");
    _run_mainloop ();
    g_object_unref (user);

    /* case 3: success with remdir = FALSE */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();
    g_object_set (G_OBJECT (user), "username", "test_deluser2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    rval = gum_user_delete (user, FALSE, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to delete user");
    _run_mainloop ();
    g_object_unref (user);

    /* case 4: use sync method to delete user*/
    user = gum_user_create_sync (FALSE);
    fail_if (user == NULL, "failed to create new user");

    g_object_set (G_OBJECT (user), "username", "test_deluser3",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user sync");

    rval = gum_user_delete_sync (user, TRUE);
    fail_if (rval == FALSE, "failed to delete user");
    g_object_unref (user);

}
END_TEST

START_TEST (test_update_user)
{
    GumUser *user = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    rval = gum_user_update (user, _on_user_op_failure, NULL);
    fail_if (rval == FALSE, "failed to update user");
    _run_mainloop ();

    /* case 2: success */
    g_object_set (G_OBJECT (user), "username", "test_upuser1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "secret", "23456", NULL);
    rval = gum_user_update (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to update user");
    _run_mainloop ();
    g_object_unref (user);

    /* case 3: use sync method to update user*/
    user = gum_user_create_sync (TRUE);
    fail_if (user == NULL, "failed to create new user --offline");

    g_object_set (G_OBJECT (user), "username", "test_upuser2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user sync");

    g_object_set (G_OBJECT (user), "secret", "23456", NULL);
    rval = gum_user_update_sync (user);
    fail_if (rval == FALSE, "failed to update user");
    g_object_unref (user);
}
END_TEST

/*
 * Group test cases
 */
START_TEST (test_create_new_group)
{
    DBG ("\n");
    GumGroup *group = NULL, *group2 = NULL;

    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");

    _run_mainloop ();

    group2 = gum_group_create (_on_group_op_success, NULL);
    fail_if (group2 == NULL, "failed to create new group2");

    _run_mainloop ();

    g_object_unref (group);
    g_object_unref (group2);

    group2 = gum_group_create_sync (FALSE);
    fail_if (group2 == NULL, "failed to create new group2 sync");
    g_object_unref (group2);

    group2 = gum_group_create_sync (TRUE);
    fail_if (group2 == NULL, "failed to create new group2 sync -- offline");
    g_object_unref (group2);
}
END_TEST

START_TEST(test_add_group)
{
    GumGroup *group = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: add with groupname */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    g_object_set (G_OBJECT (group), "groupname", "test_addgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    /* case 2: add existing group */
    rval = gum_group_add (group, _on_group_op_failure, NULL);
    fail_if (rval == FALSE, "failed to add already existing group");
    _run_mainloop ();

    g_object_unref (group);

    /* case 3: add group sync */
    group = gum_group_create_sync (TRUE);
    fail_if (group == NULL, "failed to create new group --offline");

    g_object_set (G_OBJECT (group), "groupname", "test_addgroup2",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add_sync (group);
    fail_if (rval == FALSE, "failed to add already existing group --offline");

    g_object_unref (group);
}
END_TEST

START_TEST(test_get_group_by_gid)
{
    GumGroup *group = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_get (gid, _on_group_op_failure, NULL);
    fail_if (group == NULL, "failed to get group");
    _run_mainloop ();
    g_object_unref (group);

    /* case 2: success */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    g_object_set (G_OBJECT (group), "groupname", "test_getgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    g_object_get (G_OBJECT (group), "gid", &gid, NULL);
    fail_if (gid == GUM_GROUP_INVALID_GID, "Invalid gid for the group");
    g_object_unref (group);

    group = gum_group_get (gid, _on_group_op_success, NULL);
    fail_if (group == NULL, "failed to get group");
    _run_mainloop ();

    g_object_unref (group);

    /* case 3: get group sync */
    group = gum_group_create_sync (FALSE);
    fail_if (group == NULL, "failed to create new group");

    g_object_set (G_OBJECT (group), "groupname", "test_getgroup2",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add_sync (group);
    fail_if (rval == FALSE, "failed to add group");

    g_object_get (G_OBJECT (group), "gid", &gid, NULL);
    fail_if (gid == GUM_GROUP_INVALID_GID, "Invalid gid for the group");
    g_object_unref (group);

    group = gum_group_get_sync (gid, FALSE);
    fail_if (group == NULL, "failed to get group");

    g_object_unref (group);
}
END_TEST

START_TEST(test_get_group_by_name)
{
    GumGroup *group = NULL;
    gchar *name = "test_getgroup_byname1";
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_get_by_name ("no_group", _on_group_op_failure,
            NULL);
    fail_if (group == NULL, "failed to get group by name");
    _run_mainloop ();
    g_object_unref (group);

    /* case 2: success */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    g_object_set (G_OBJECT (group), "groupname", name,
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();
    g_object_unref (group);

    group = gum_group_get_by_name (name, _on_group_op_success, NULL);
    fail_if (group == NULL, "failed to get group by name");
    _run_mainloop ();

    g_object_unref (group);

    /* case 3: get group by name sync */
    group = gum_group_create_sync (TRUE);
    fail_if (group == NULL, "failed to create new group -- offline");

    g_object_set (G_OBJECT (group), "groupname", "test_getgroup_byname2",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add_sync (group);
    fail_if (rval == FALSE, "failed to add group -- offline");
    g_object_unref (group);

    group = gum_group_get_by_name_sync ("test_getgroup_byname2", TRUE);
    fail_if (group == NULL, "failed to get group by name sync -- offline");

    g_object_unref (group);

}
END_TEST

START_TEST (test_delete_group)
{
    GumGroup *group = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    rval = gum_group_delete (group, _on_group_op_failure, NULL);
    fail_if (rval == FALSE, "failed to delete group");
    _run_mainloop ();

    /* case 2: success */
    g_object_set (G_OBJECT (group), "groupname", "test_delgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    rval = gum_group_delete (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to delete group");
    _run_mainloop ();
    g_object_unref (group);

    /* case 2: delete group sync */
    group = gum_group_create_sync (FALSE);
    fail_if (group == NULL, "failed to create new group");

    g_object_set (G_OBJECT (group), "groupname", "test_delgroup2",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add_sync (group);
    fail_if (rval == FALSE, "failed to add group");

    rval = gum_group_delete_sync (group);
    fail_if (rval == FALSE, "failed to delete group sync");

    g_object_unref (group);
}
END_TEST

START_TEST (test_update_group)
{
    GumGroup *group = NULL;
    gboolean rval = FALSE;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    rval = gum_group_update (group, _on_group_op_failure, NULL);
    fail_if (rval == FALSE, "failed to update group");
    _run_mainloop ();

    /* case 2: success */
    g_object_set (G_OBJECT (group), "groupname", "test_upgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    g_object_set (G_OBJECT (group), "secret", "23456", NULL);
    rval = gum_group_update (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to update group");
    _run_mainloop ();
    g_object_unref (group);

    /* case 3: update group sync */
    group = gum_group_create_sync (TRUE);
    fail_if (group == NULL, "failed to create new group -- offline");

    g_object_set (G_OBJECT (group), "groupname", "test_upgroup2",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add_sync (group);
    fail_if (rval == FALSE, "failed to add group");

    g_object_set (G_OBJECT (group), "secret", "23456", NULL);
    rval = gum_group_update_sync (group);
    fail_if (rval == FALSE, "failed to update group");

    g_object_unref (group);
}
END_TEST

START_TEST (test_add_group_member)
{
    GumGroup *group = NULL;
    GumUser *user = NULL;
    gboolean rval = FALSE;
    uid_t uid = GUM_USER_INVALID_UID;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    rval = gum_group_add_member (group, uid, TRUE, _on_group_op_failure, NULL);
    fail_if (rval == FALSE, "failed to add group member");
    _run_mainloop ();

    /* case 2: success */
    g_object_set (G_OBJECT (group), "groupname", "test_addgrpmem1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "username", "test_addgrpmem_user1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);

    rval = gum_group_add_member (group, uid, TRUE, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group member");
    _run_mainloop ();

    g_object_unref (user);

    /* case 3: add group mem sync */
    user = gum_user_create_sync (FALSE);
    fail_if (user == NULL, "failed to create new user");

    g_object_set (G_OBJECT (user), "username", "test_addgrpmem_user2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user");

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);

    rval = gum_group_add_member_sync (group, uid, TRUE);
    fail_if (rval == FALSE, "failed to add group member sync");

    g_object_unref (user);
    g_object_unref (group);
}
END_TEST

START_TEST (test_delete_group_member)
{
    GumGroup *group = NULL;
    GumUser *user = NULL;
    gboolean rval = FALSE;
    uid_t uid = GUM_USER_INVALID_UID;

    DBG ("\n");

    /* case 1: failure */
    group = gum_group_create (_on_group_op_success, NULL);
    fail_if (group == NULL, "failed to create new group");
    _run_mainloop ();

    rval = gum_group_delete_member (group, uid, _on_group_op_failure, NULL);
    fail_if (rval == FALSE, "failed to delete group member");
    _run_mainloop ();

    /* case 2: success */
    g_object_set (G_OBJECT (group), "groupname", "test_delgrpmem1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);
    rval = gum_group_add (group, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group");
    _run_mainloop ();

    user = gum_user_create (_on_user_op_success, NULL);
    fail_if (user == NULL, "failed to create new user");
    _run_mainloop ();

    g_object_set (G_OBJECT (user), "username", "test_delgrpmem_user1",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add (user, _on_user_op_success, NULL);
    fail_if (rval == FALSE, "failed to add user");
    _run_mainloop ();

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);

    rval = gum_group_add_member (group, uid, TRUE, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to add group member");
    _run_mainloop ();

    rval = gum_group_delete_member (group, uid, _on_group_op_success, NULL);
    fail_if (rval == FALSE, "failed to delete group member");
    _run_mainloop ();

    g_object_unref (user);

    /* case 3: add group mem sync */
    user = gum_user_create_sync (TRUE);
    fail_if (user == NULL, "failed to create new user -- offline");

    g_object_set (G_OBJECT (user), "username", "test_delgrpmem_user2",
            "secret", "123456", "usertype", GUM_USERTYPE_NORMAL, NULL);
    rval = gum_user_add_sync (user);
    fail_if (rval == FALSE, "failed to add user");

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);

    rval = gum_group_add_member_sync (group, uid, TRUE);
    fail_if (rval == FALSE, "failed to add group member sync");

    rval = gum_group_delete_member_sync (group, uid);
    fail_if (rval == FALSE, "failed to delete group member sync");

    g_object_unref (user);
    g_object_unref (group);
}
END_TEST

static void
_on_user_service_user_list_op_success (
        GumUserService *service,
        GumUserList *users,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error != NULL, "failed user service user list operation : %s",
            error ? error->message : "(null)");
    gum_user_service_list_free (users);
}

static void
_on_user_service_user_list_op_failure (
        GumUserService *service,
        GumUserList *users,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    fail_if (error == NULL, "op should have failed for getting user list");
    gum_user_service_list_free (users);
}

START_TEST (test_get_user_list)
{
    GumUserService *service = NULL;
    gboolean rval = FALSE;
    GumUserList *user_list = NULL;

    DBG ("\n");

    /* case 1: create async object */
    service = gum_user_service_create (_on_user_service_op_success, NULL);
    fail_if (service == NULL, "failed to create new user service");
    _run_mainloop ();
    fail_if (gum_user_service_get_dbus_proxy(service) == NULL, "no dbus proxy");
    g_object_unref (service);

    /* case 2: create sync object without offline */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    g_object_unref (service);

    /* case 3 create sync object with offline */
    service = gum_user_service_create_sync (TRUE);
    fail_if (service == NULL, "failed to create new user service");
    fail_if (gum_user_service_get_dbus_proxy(service) == NULL, "dbus proxy");
    g_object_unref (service);

    /* case 4: misc */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    fail_if (gum_user_service_get_dbus_proxy(service) == NULL, "no dbus proxy");
    g_object_unref (service);

    /* case 5: get user list async -- success */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    rval = gum_user_service_get_user_list (service, "normal,system",
            _on_user_service_user_list_op_success, NULL);
    fail_if (rval == FALSE, "failed to get users uids");
    _run_mainloop ();
    g_object_unref (service);

    /* case 6: get user list async -- failure */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    rval = gum_user_service_get_user_list (service, "none",
            _on_user_service_user_list_op_failure, NULL);
    fail_if (rval == FALSE, "failed to get users uids");
    _run_mainloop ();
    g_object_unref (service);

    /* case 7: get user list sync */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    user_list = gum_user_service_get_user_list_sync (service, "none");
    fail_if (user_list != NULL, "failed to get users uids");
    g_object_unref (service);

    /* case 8: get user list sync -- success */
    service = gum_user_service_create_sync (FALSE);
    fail_if (service == NULL, "failed to create new user service");
    user_list = gum_user_service_get_user_list_sync (service, "normal");
    fail_if (user_list == NULL, "failed to get users uids");
    fail_if (g_list_length (user_list) <= 0, "no normal users found");
    gum_user_service_list_free (user_list);
    g_object_unref (service);
}
END_TEST

Suite* daemon_suite (void)
{
    TCase *tc = NULL;

    Suite *s = suite_create ("Gum client");
    
    tc = tcase_create ("Client tests");
    tcase_set_timeout(tc, 15);
    tcase_add_unchecked_fixture (tc, _setup_daemon, _teardown_daemon);
    tcase_add_checked_fixture (tc, _create_mainloop, _stop_mainloop);

    tcase_add_test (tc, test_create_new_user);
    tcase_add_test (tc, test_add_user);
    tcase_add_test (tc, test_get_user_by_uid);
    tcase_add_test (tc, test_get_user_by_name);
    tcase_add_test (tc, test_delete_user);
    tcase_add_test (tc, test_update_user);

    tcase_add_test (tc, test_create_new_group);
    tcase_add_test (tc, test_add_group);
    tcase_add_test (tc, test_get_group_by_gid);
    tcase_add_test (tc, test_get_group_by_name);
    tcase_add_test (tc, test_delete_group);
    tcase_add_test (tc, test_update_group);
    tcase_add_test (tc, test_add_group_member);
    tcase_add_test (tc, test_delete_group_member);

    tcase_add_test (tc, test_get_user_list);
    suite_add_tcase (s, tc);

    return s;
}

int main (int argc, char *argv[])
{
    int number_failed;
    Suite *s = 0;
    SRunner *sr = 0;
   
#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    exe_name = argv[0];

    s = daemon_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
