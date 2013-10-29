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
#include "daemon/gumd-daemon.h"
#include "daemon/gumd-daemon-user.h"
#include "daemon/gumd-daemon-group.h"

#ifdef GUM_BUS_TYPE_P2P
#  ifdef GUM_SERVICE
#    undef GUM_SERVICE
#  endif
#  define GUM_SERVICE NULL
#endif

gchar *exe_name = 0;

#if HAVE_GTESTDBUS
GTestDBus *dbus = NULL;
#else
GPid daemon_pid = 0;
#endif

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
    fail_if (system(cmd) != 0, "Failed to copy temp skel dir: %s:%s\n", cmd,
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
_setup_daemon (void)
{
    DBG ("Programe name : %s\n", exe_name);

    _setup_env ();

#if HAVE_GTESTDBUS
    dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
    fail_unless (dbus != NULL, "could not create test dbus");

    g_test_dbus_add_service_dir (dbus, GUMD_TEST_DBUS_SERVICE_DIR);

    g_test_dbus_up (dbus);
    DBG ("Test dbus server address : %s\n", g_test_dbus_get_bus_address(dbus));
#else
    GError *error = NULL;
#   ifdef GUM_BUS_TYPE_P2P
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
#   else
    /* session bus where no GTestBus support */
    GIOChannel *channel = NULL;
    gchar *bus_address = NULL;
    gint tmp_fd = 0;
    gint pipe_fd[2];
    gchar *argv[] = {"dbus-daemon", "--config-file=<<conf-file>>",
            "--print-address=<<fd>>", NULL};
    gsize len = 0;
    const gchar *dbus_monitor = NULL;

    argv[1] = g_strdup_printf ("--config-file=%s/%s", GUM_TEST_DATA_DIR,
            "gumd-dbus.conf");

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
#   endif

    DBG ("Daemon PID = %d\n", daemon_pid);
#endif
}

static void
_teardown_daemon (void)
{
#if HAVE_GTESTDBUS
    g_test_dbus_down (dbus);
#else
    if (daemon_pid) kill (daemon_pid, SIGTERM);
#endif

    _unset_env ();
}

GDBusConnection *
_get_bus_connection (
        GError **error)
{
#if GUM_BUS_TYPE_P2P
    gchar address[128];
    g_snprintf (address, 127, GUM_DBUS_ADDRESS, g_get_user_runtime_dir());
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
#else
    return g_bus_get_sync (GUM_BUS_TYPE, NULL, error);
#endif
}

GumDbusUserService *
_get_user_service (
        GDBusConnection *connection,
        GError **error)
{
    return gum_dbus_user_service_proxy_new_sync (
            connection, G_DBUS_PROXY_FLAGS_NONE, GUM_SERVICE,
            GUM_USER_SERVICE_OBJECTPATH, NULL, error);
}

GumDbusUser *
_get_user_proxy_for_path (
        GDBusConnection *connection,
        const gchar *proxy_path,
        const gchar *bus_name,
        GError **error)
{
    return gum_dbus_user_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE,
            bus_name, proxy_path, NULL, error);
}

GumDbusUser *
_create_new_user_proxy (
        GumDbusUserService *user_service,
        GError **error)
{
    GDBusConnection *connection = NULL;
    GumDbusUser *user = NULL;
    gchar *proxy_path = NULL;

    if (!gum_dbus_user_service_call_create_new_user_sync (
            user_service, &proxy_path, NULL, error)) {
        DBG (" ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }
    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (user_service));
    user = _get_user_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (user_service)), error);
    g_free (proxy_path);

    return user;
}

GumDbusUser *
_get_user_proxy_by_uid (
        GumDbusUserService *user_service,
        guint32 uid,
        GError **error)
{
    gboolean res = FALSE;
    gchar *proxy_path = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUser *user = NULL;

    res = gum_dbus_user_service_call_get_user_sync(
            user_service, uid, &proxy_path, NULL, error);

    if (res == FALSE || !proxy_path) {
        DBG ("ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (user_service));
    user = _get_user_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (user_service)), error);
    g_free (proxy_path);

    return user;
}

GumDbusUser *
_get_user_proxy_by_name (
        GumDbusUserService *user_service,
        const gchar *username,
        GError **error)
{
    gboolean res = FALSE;
    gchar *proxy_path = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUser *user = NULL;

    res = gum_dbus_user_service_call_get_user_by_name_sync(
            user_service, username, &proxy_path, NULL, error);

    if (res == FALSE || !proxy_path) {
        DBG ("ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (user_service));
    user = _get_user_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (user_service)), error);
    g_free (proxy_path);

    return user;
}

/*
 * User test cases
 */
START_TEST (test_daemon_user)
{
    DBG("");
    GError *error = NULL;
    uid_t uid = 0, admin_uid = 0, norm_uid = 0;
    gid_t gid = 0;
    gchar *str = NULL;
    gchar plain_secret[] = "pass123";
    gchar *hdir = NULL;
    struct stat sb;

    gchar *encr_secret = gum_crypt_encrypt_secret ("pass123",
            GUM_CRYPT_SHA512);

    GumConfig* config = gum_config_new ();
    fail_if(config == NULL);

    GumdDaemonUser* user = gumd_daemon_user_new (config);
    fail_if(user == NULL);

    /* case 1: no property set and calling add */
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_INVALID_USER_TYPE);
    g_error_free (error); error = NULL;

    /* case 2: usertype set and calling add */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL, NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 3: usertype, invalid username set and calling add */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "1cantstartwithdigit", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 4: usertype, invalid username set and calling add */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "1111111111111111111111111111111111111111333333", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 5: usertype, invalid username set and calling add */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 6: usertype, nickname set and calling add for system user */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_SYSTEM,
            "nickname", "nick1", "username", NULL, NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 7: user already exists */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "nickname", NULL, "username", "root", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_ALREADY_EXISTS);
    g_error_free (error); error = NULL;

    /* case 8: usertype, nickname set and calling add for normal user */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "nickname", "nick1", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (error == NULL);

    /* case 9: user already exists */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "nickname", "nick1", "username", NULL, NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_ALREADY_EXISTS);
    g_error_free (error); error = NULL;

    /* case 10: add normal user with valid secret*/
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user1", "secret", plain_secret, "realname",
            "Abcded dfads", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (user), "uid", &uid, "gid", &gid, "realname",
            &str, NULL);
    fail_unless (uid == gid);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "Abcded dfads") == 0);
    g_free (str);
    hdir = g_build_filename (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_HOME_DIR_PREF), "normal_daemon_user1", NULL);
    fail_unless (stat (hdir, &sb) == 0);
    g_free (hdir);
    norm_uid = uid;

    /* case 11: add system user */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_GROUPTYPE_SYSTEM,
            "username", "system_daemon_user1", "secret", plain_secret, "realname",
                        "イムラン",  NULL);
    /*TODO check utf8 string for nick and real names*/
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (user), "uid", &uid, "gid", &gid, "realname",
            &str, NULL);
    fail_unless (uid == gid);
    fail_unless (str != NULL);
    g_free (str);

    /* case 12: add system user2 */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_GROUPTYPE_SYSTEM,
            "username", "system_daemon_user2", "secret", NULL, "realname", NULL, NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (user), "uid", &uid, "gid", &gid,NULL);
    fail_unless (uid == gid);

    /* case 12: add admin user */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_ADMIN,
            "username", "admin_daemon_user1", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (user), "uid", &uid, "gid", &gid, NULL);
    fail_unless (uid == gid);
    hdir = g_build_filename (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_HOME_DIR_PREF), "admin_daemon_user1", NULL);
    fail_unless (stat (hdir, &sb) == 0);
    g_free (hdir);
    admin_uid = uid;

    /* case 13: add guest user */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_GUEST,
            "username", "guest_daemon_user1", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (user), "uid", &uid, "gid", &gid, NULL);
    fail_unless (uid == gid);
    hdir = g_build_filename (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_HOME_DIR_PREF), "guest_daemon_user1", NULL);
    fail_unless (stat (hdir, &sb) != 0);
    g_free (hdir);

    /* case 14: user does not exist and delete */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "no_daemon_user1", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_user_delete (user, TRUE, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 15: user exist and delete (also remove home dir) */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_GUEST,
            "username", "guest_daemon_user1", "uid", uid, "gid", gid, NULL);
    fail_unless (gumd_daemon_user_delete (user, TRUE, &error) == TRUE);
    g_object_get (G_OBJECT (user), "homedir", &str,  NULL);
    fail_unless (str != NULL);
    fail_unless (stat (str, &sb) != 0);
    fail_unless (error == NULL);
    fail_unless (gum_file_getpwnam ("guest_daemon_user1", config) == NULL);
    fail_unless (gum_file_getgrnam ("guest_daemon_user1", config) == NULL);
    g_free (str);

    /* case 16: user exist and delete (dont remove home dir) */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_ADMIN,
            "username", "admin_daemon_user1", "uid", admin_uid, NULL);
    fail_unless (gumd_daemon_user_delete (user, FALSE, &error) == TRUE);
    g_object_get (G_OBJECT (user), "homedir", &str,  NULL);
    fail_unless (str != NULL);
    fail_unless (stat (str, &sb) == 0);
    fail_unless (error == NULL);
    fail_unless (gum_file_getpwnam ("admin_daemon_user1", config) == NULL);
    fail_unless (gum_file_getgrnam ("admin_daemon_user1", config) == NULL);
    g_free (str);

    /* case 17: user does exist and update fails */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_ADMIN,
            "username", "no_daemon_user1", NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 18: user exist but no change and update fails */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);

    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user1", "uid", norm_uid, NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_NO_CHANGES);
    g_error_free (error); error = NULL;

    /* case 19: user exist and passwd changes */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user_up1", "secret", plain_secret, "realname",
            "Abcdeddfads", "officephone", "+358-1234567", NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);

    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user_up1", "secret", "pass456", "uid", uid,
            NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == TRUE);
    fail_unless (error == NULL);

    /* case 20: user exist and realname changes */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user_up1", "realname", "newrealname", "uid",
            uid, NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == TRUE);
    fail_unless (error == NULL);

    /* case 21: user exist and officephone changes */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user_up1", "officephone", "+358-7654321", "uid",
            uid, NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == TRUE);
    fail_unless (error == NULL);

    /* case 22: user exist and shell changes */
    g_object_unref (user);
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user_up1", "shell", "/bin/sh", "uid", uid,
            NULL);
    fail_unless (gumd_daemon_user_update (user, &error) == TRUE);
    fail_unless (error == NULL);

    /* case 23: new by uid */
    g_object_unref (user);
    user = gumd_daemon_user_new_by_uid ((uid_t)G_MAXUINT, config);
    fail_unless (user == NULL);

    user = gumd_daemon_user_new_by_uid (uid, config);
    fail_unless (user != NULL);

    /* case 24: new by name */
    g_object_unref (user);
    user = gumd_daemon_user_new_by_name ("no_daemon_user", config);
    fail_unless (user == NULL);

    user = gumd_daemon_user_new_by_name ("normal_daemon_user_up1", config);
    fail_unless (user != NULL);

    fail_unless (gumd_daemon_group_get_gid_by_name ("normal_daemon_user_up1",
            config) != GUM_USER_INVALID_UID);

    g_free (encr_secret);
    g_object_unref (user);
    g_object_unref (config);
}
END_TEST

START_TEST (test_create_new_user)
{
    DBG ("\n");
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_add_user)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;
    uid_t user_id = GUM_USER_INVALID_UID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (user_proxy), "username", "test_adduser1",
            "secret", "123456", "nickname", "nick1", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_if (res == FALSE, "Failed to add new user : %s",
            error ? error->message : "");
    fail_unless (user_id != GUM_USER_INVALID_UID);

    /*try to add again*/
    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_unless (res == FALSE);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

START_TEST(test_get_user_by_uid)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GumDbusUserService *user_service = 0;
    uid_t user_id = GUM_USER_INVALID_UID;
    GumDbusUser *user_proxy = NULL, *user_proxy2 = NULL;
    gchar *str1 = NULL, *str2 = NULL;

    GDBusConnection *connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    user_proxy = _get_user_proxy_by_uid (user_service, user_id, &error);
    fail_unless (user_proxy == NULL);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (user_proxy), "username", "test_getuserbyuid1",
            "secret", "123456", "nickname", "nick2", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_if (res == FALSE, "Failed to add new user : %s",
            error ? error->message : "");
    fail_if (user_id == GUM_USER_INVALID_UID);
    g_object_unref (user_proxy);
    user_proxy = NULL;

    user_proxy = _get_user_proxy_by_uid (user_service, user_id, &error);
    fail_if (user_proxy == NULL, "Failed to get user for uid '%u' : %s",
            user_id, error ? error->message : "");

    /* check properties */
    g_object_get (G_OBJECT (user_proxy), "username", &str1, "nickname", &str2,
            NULL);
    fail_if (str1 == NULL);
    fail_if (str2 == NULL);
    fail_if (g_strcmp0 (str1, "test_getuserbyuid1") != 0);
    fail_if (g_strcmp0 (str2, "nick2") != 0);
    g_free (str1); g_free (str2);

    /* check for object paths for users with same uid */
    user_proxy2 = _get_user_proxy_by_uid (user_service, user_id, &error);
    fail_if (user_proxy == NULL, "Failed to get user for uid '%u' : %s",
            user_id, error ? error->message : "");

    g_object_get (G_OBJECT (user_proxy), "g-object-path", &str1, NULL);
    g_object_get (G_OBJECT (user_proxy2), "g-object-path", &str2, NULL);
    fail_unless (g_strcmp0 (str1, str2) == 0);
    g_free (str1); g_free (str2);

    g_object_unref (user_proxy2);

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

START_TEST(test_get_user_by_name)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GumDbusUserService *user_service = 0;
    uid_t user_id = GUM_USER_INVALID_UID;
    GumDbusUser *user_proxy = NULL, *user_proxy2 = NULL;
    const gchar *name = "test_getuserbyname1";
    gchar *str1 = NULL, *str2 = NULL;

    GDBusConnection *connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    user_proxy = _get_user_proxy_by_name (user_service, name, &error);
    fail_unless (user_proxy == NULL);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (user_proxy), "username", name,
            "secret", "123456", "nickname", "nick3", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_if (res == FALSE, "Failed to add new user : %s",
            error ? error->message : "");
    fail_if (user_id == GUM_USER_INVALID_UID);
    g_object_unref (user_proxy);
    user_proxy = NULL;

    user_proxy = _get_user_proxy_by_name (user_service, name, &error);
    fail_if (user_proxy == NULL, "Failed to get user for uid '%u' : %s",
            name, error ? error->message : "");

    /* check properties */
    g_object_get (G_OBJECT (user_proxy), "username", &str1, "nickname",
            &str2, NULL);
    fail_if (str1 == NULL);
    fail_if (str2 == NULL);
    fail_if (g_strcmp0 (name, str1) != 0);
    fail_if (g_strcmp0 (str2, "nick3") != 0);
    g_free (str1); g_free (str2);

    /* check for object paths for users with same uid */
    user_proxy2 = _get_user_proxy_by_name (user_service, name, &error);
    fail_if (user_proxy == NULL, "Failed to get user for uid '%u' : %s",
            user_id, error ? error->message : "");

    g_object_get (G_OBJECT (user_proxy), "g-object-path", &str1, NULL);
    g_object_get (G_OBJECT (user_proxy2), "g-object-path", &str2, NULL);
    fail_unless (g_strcmp0 (str1, str2) == 0);
    g_free (str1); g_free (str2);

    g_object_unref (user_proxy2);

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_delete_user)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;
    uid_t user_id = GUM_USER_INVALID_UID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (user_proxy), "username", "test_deluser1",
            "secret", "123456", "nickname", "nick2", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_if (res == FALSE, "Failed to add new user : %s",
            error ? error->message : "");
    g_object_unref (user_proxy);

    /* get user added-by-uid */
    user_proxy = _get_user_proxy_by_name (user_service, "test_deluser1",
            &error);
    fail_if (user_proxy == NULL, "Failed to get user for name '%s' : %s",
            "test_adduser2", error ? error->message : "");

    res = gum_dbus_user_call_delete_user_sync (user_proxy, TRUE, NULL, &error);
    fail_if (res == FALSE, "Failed to delete new user : %s",
            error ? error->message : "");

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_update_user)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;
    uid_t user_id = GUM_USER_INVALID_UID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (user_proxy), "username", "test_upuser1",
            "secret", "123456", "nickname", "nick2", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_if (res == FALSE, "Failed to add new user : %s",
            error ? error->message : "");
    fail_if (user_id == GUM_USER_INVALID_UID);

    g_object_set (G_OBJECT (user_proxy), "secret", "23456", NULL);

    res = gum_dbus_user_call_update_user_sync (user_proxy, NULL, &error);
    fail_if (res == FALSE, "Failed to update user : %s",
            error ? error->message : "");

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (connection);
}
END_TEST

GumDbusGroupService *
_get_group_service (
        GDBusConnection *connection,
        GError **error)
{
    return gum_dbus_group_service_proxy_new_sync (
            connection, G_DBUS_PROXY_FLAGS_NONE, GUM_SERVICE,
            GUM_GROUP_SERVICE_OBJECTPATH, NULL, error);
}

GumDbusGroup *
_get_group_proxy_for_path (
        GDBusConnection *connection,
        const gchar *proxy_path,
        const gchar *bus_name,
        GError **error)
{
    return gum_dbus_group_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE,
            bus_name, proxy_path, NULL, error);
}

GumDbusGroup *
_create_new_group_proxy (
        GumDbusGroupService *group_service,
        GError **error)
{
    GDBusConnection *connection = NULL;
    GumDbusGroup *group = NULL;
    gchar *proxy_path = NULL;

    if (!gum_dbus_group_service_call_create_new_group_sync (
            group_service, &proxy_path, NULL, error)) {
        DBG (" ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }
    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (group_service));
    group = _get_group_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (group_service)), error);
    g_free (proxy_path);

    return group;
}

GumDbusGroup *
_get_group_proxy_by_gid (
        GumDbusGroupService *group_service,
        guint32 gid,
        GError **error)
{
    gboolean res = FALSE;
    gchar *proxy_path = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroup *group = NULL;

    res = gum_dbus_group_service_call_get_group_sync(
            group_service, gid, &proxy_path, NULL, error);

    if (res == FALSE || !proxy_path) {
        DBG ("ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (group_service));
    group = _get_group_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (group_service)), error);
    g_free (proxy_path);

    return group;
}

GumDbusGroup *
_get_group_proxy_by_name (
        GumDbusGroupService *group_service,
        const gchar *groupname,
        GError **error)
{
    gboolean res = FALSE;
    gchar *proxy_path = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroup *group = NULL;

    res = gum_dbus_group_service_call_get_group_by_name_sync(
            group_service, groupname, &proxy_path, NULL, error);

    if (res == FALSE || !proxy_path) {
        DBG ("ERROR :: %s", error ? (*error)->message : "");
        g_free (proxy_path);
        return NULL;
    }

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (group_service));
    group = _get_group_proxy_for_path (connection, proxy_path,
            g_dbus_proxy_get_name (G_DBUS_PROXY (group_service)), error);
    g_free (proxy_path);

    return group;
}

/*
 * Group test cases
 */

START_TEST (test_daemon_group)
{
    DBG("");
    GError *error = NULL;
    gid_t gid = 0, pref_gid = 33333, sys_gid = 0;
    gchar plain_secret[] = "grouppass123";
    GumdDaemonUser *user = NULL;
    uid_t uid = 0;
    gchar *str = NULL;

    gchar *encr_secret = gum_crypt_encrypt_secret ("grouppass123",
            GUM_CRYPT_SHA512);

    GumConfig* config = gum_config_new ();
    fail_if(config == NULL);

    GumdDaemonGroup* group = gumd_daemon_group_new (config);
    fail_if(group == NULL);

    /* case 1: no property set and calling add */
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_INVALID_GROUP_TYPE);
    g_error_free (error); error = NULL;

    /* case 2: grouptype set and calling add */
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM, NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 3: grouptype, invalid username set and calling add */
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM,
            "groupname", "1cantstartwithdigit", NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 4: grouptype, invalid groupname set and calling add */
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM,
            "groupname", "1111111111111111111111111111111111111111333333", NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 5: grouptype, invalid groupname set and calling add */
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM,
            "groupname", "", NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /* case 6: system group add */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM,
            "groupname", "system_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == TRUE);
    fail_unless (error == NULL);
    fail_unless (gid == 33333);
    sys_gid = gid;

    /* case 7: group already exists */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "root", NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_ALREADY_EXISTS);
    g_error_free (error); error = NULL;

    /* case 8: add user group with valid secret*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group1", "secret", plain_secret, NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == TRUE);
    fail_unless (gid > 0);
    fail_unless (error == NULL);

    /* case 9: group does not exist and delete */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);

    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "no_daemon_group1", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_group_delete (group, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 10: group exist and delete */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);

    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group1", "gid", gid, NULL);
    fail_unless (gumd_daemon_group_delete (group, &error) == TRUE);
    fail_unless (error == NULL);
    fail_unless (gum_file_getgrnam ("user_daemon_group1", config) == NULL);

    /* case 11: group exist and delete*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);

    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_SYSTEM,
            "groupname", "system_daemon_group1", "gid", sys_gid, NULL);
    fail_unless (gum_file_getgrnam ("system_daemon_group1", config) != NULL);
    fail_unless (gumd_daemon_group_delete (group, &error) == TRUE);
    fail_unless (error == NULL);
    fail_unless (gum_file_getgrnam ("system_daemon_group1", config) == NULL);

    /* case 12: add user to group and user does not exist*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "system_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_add_member (group, 38653, TRUE, &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 13: add user to group and user does exist but group does not exist*/
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_GUEST,
            "username", "guest_daemon_user12", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_unref (user);

    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "no_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_add_member (group, uid, FALSE, &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 14: add user to group */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
                "groupname", "user_daemon_group1", "secret", plain_secret, NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == TRUE);
    fail_unless (gid > 0);
    fail_unless (error == NULL);
    fail_unless (gumd_daemon_group_add_member (group, uid, FALSE, &error)
            == TRUE);
    fail_unless (error == NULL);

    /* case 15: add user to group where its already a member */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_add_member (group, uid, TRUE, &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER);
    g_error_free (error); error = NULL;

    /* case 16: adding 'users' group should fail as it is added by default*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "users", NULL);
    fail_unless (gumd_daemon_group_add_member (group, uid, TRUE, &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER);
    g_error_free (error); error = NULL;

    /* case 17: delete user from group and user does not exist*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "system_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_delete_member (group, 38653, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_USER_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 18: delete user from group; user does exist but group does not
     * exist */
    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "normal_daemon_user13", "secret", plain_secret,  NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_unref (user);

    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "no_daemon_group1", NULL);
    fail_unless (gumd_daemon_group_delete_member (group, uid, &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 19: delete user from group */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
                "groupname", "user_daemon_group2", "secret", plain_secret, NULL);
    fail_unless (gumd_daemon_group_add (group, pref_gid, &gid, &error) == TRUE);
    fail_unless (gid > 0);
    fail_unless (error == NULL);
    fail_unless (gumd_daemon_group_add_member (group, uid, TRUE, &error)
            == TRUE);
    fail_unless (error == NULL);
    fail_unless (gumd_daemon_group_delete_member (group, uid, &error)
            == TRUE);
    fail_unless (error == NULL);

    /* case 20: update group but without any data */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    fail_unless (gumd_daemon_group_update (group, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 21: update group name but gid does not exist */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group2_new", "secret", "test22", "gid",
            12345678, NULL);
    fail_unless (gumd_daemon_group_update (group, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 22: update group name should fail */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group2_new", "gid", gid, NULL);
    fail_unless (gumd_daemon_group_update (group, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_GROUP_NOT_FOUND);
    g_error_free (error); error = NULL;

    /* case 23: update group secret */
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "secret", "new_secret", "gid", gid, NULL);
    fail_unless (gumd_daemon_group_update (group, &error) == TRUE);
    fail_unless (error == NULL);
    g_object_get (G_OBJECT (group), "secret", &str,  NULL);
    fail_unless (str != NULL);
    //should be encrypted already
    fail_unless (g_strcmp0 (str, "new_secret") != 0);
    g_free (str);

    /* case 24: update members (add)*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group2", "gid", gid, NULL);

    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "nor_daemon_user_grp_add1", "secret", plain_secret,
            NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_unref (user);
    fail_unless (gumd_daemon_group_add_member (group, uid, TRUE, &error)
                == TRUE);

    user = gumd_daemon_user_new (config);
    g_object_set (G_OBJECT (user), "usertype", GUM_USERTYPE_NORMAL,
            "username", "nor_daemon_user_grp_add2", "secret", plain_secret,
            NULL);
    fail_unless (gumd_daemon_user_add (user, &uid, &error) == TRUE);
    fail_unless (uid > 0);
    fail_unless (error == NULL);
    g_object_unref (user);
    fail_unless (gumd_daemon_group_add_member (group, uid, TRUE, &error)
                == TRUE);
    g_object_get (G_OBJECT (group), "userlist", &str,  NULL);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str,
            "nor_daemon_user_grp_add1,nor_daemon_user_grp_add2") == 0);
    g_free (str);

    /* case 25: update members (delete)*/
    g_object_unref (group);
    group = gumd_daemon_group_new (config);
    g_object_set (G_OBJECT (group), "grouptype", GUM_GROUPTYPE_USER,
            "groupname", "user_daemon_group2", "gid", gid, NULL);
    fail_unless (gumd_daemon_group_delete_member (group, uid, &error)
                == TRUE);
    g_object_get (G_OBJECT (group), "userlist", &str,  NULL);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "nor_daemon_user_grp_add1") == 0);
    g_free (str);

    /* case 26: delete user membership */
    fail_unless (gumd_daemon_group_delete_user_membership (config,
            "nor_daemon_user_grp_add1", &error) == TRUE);
    fail_unless (error == NULL);

    g_object_unref (group);
    group = gumd_daemon_group_new_by_gid (gid, config);
    g_object_get (G_OBJECT (group), "userlist", &str,  NULL);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "") == 0);
    g_free (str);

    /* case 27: new by gid */
    g_object_unref (group);
    group = gumd_daemon_group_new_by_gid ((gid_t)G_MAXUINT, config);
    fail_unless (group == NULL);

    group = gumd_daemon_group_new_by_gid (gid, config);
    fail_unless (group != NULL);

    /* case 28: new by name */
    g_object_unref (group);
    group = gumd_daemon_group_new_by_name ("no_daemon_group", config);
    fail_unless (group == NULL);

    group = gumd_daemon_group_new_by_name ("user_daemon_group2", config);
    fail_unless (group != NULL);

    fail_unless (gumd_daemon_group_get_gid_by_name ("user_daemon_group2",
            config) !=  GUM_GROUP_INVALID_GID);

    g_free (encr_secret);
    g_object_unref (group);
    g_object_unref (config);
}
END_TEST

START_TEST (test_create_new_group)
{
    DBG ("\n");
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_add_group)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;
    gid_t group_id = GUM_GROUP_INVALID_GID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_addgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_unless (group_id != GUM_GROUP_INVALID_GID);

    /*try to add again*/
    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_unless (res == FALSE);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST(test_get_group_by_uid)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GumDbusGroupService *group_service = 0;
    gid_t group_id = GUM_GROUP_INVALID_GID;
    GumDbusGroup *group_proxy = NULL, *group_proxy2 = NULL;
    gchar *str1 = NULL, *str2 = NULL;

    GDBusConnection *connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _get_group_proxy_by_gid (group_service, group_id, &error);
    fail_unless (group_proxy == NULL);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    /* create and add group */
    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_getgrpbyuid1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL,  &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_if (group_id == GUM_GROUP_INVALID_GID);
    g_object_unref (group_proxy);
    group_proxy = NULL;

    group_proxy = _get_group_proxy_by_gid (group_service, group_id, &error);
    fail_if (group_proxy == NULL, "Failed to get group for uid '%u' : %s",
            group_id, error ? error->message : "");

    /* check properties */
    g_object_get (G_OBJECT (group_proxy), "groupname", &str1, NULL);
    fail_if (str1 == NULL);
    fail_if (g_strcmp0 (str1, "test_getgrpbyuid1") != 0);
    g_free (str1);

    /* check for object paths for groups with same uid */
    group_proxy2 = _get_group_proxy_by_gid (group_service, group_id, &error);
    fail_if (group_proxy == NULL, "Failed to get group for uid '%u' : %s",
            group_id, error ? error->message : "");

    g_object_get (G_OBJECT (group_proxy), "g-object-path", &str1, NULL);
    g_object_get (G_OBJECT (group_proxy2), "g-object-path", &str2, NULL);
    fail_unless (g_strcmp0 (str1, str2) == 0);
    g_free (str1); g_free (str2);

    g_object_unref (group_proxy2);

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST(test_get_group_by_name)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GumDbusGroupService *group_service = 0;
    gid_t group_id = GUM_GROUP_INVALID_GID;
    GumDbusGroup *group_proxy = NULL, *group_proxy2 = NULL;
    const gchar *name = "test_getgrpbyname1";
    gchar *str1 = NULL, *str2 = NULL;

    GDBusConnection *connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _get_group_proxy_by_name (group_service, name, &error);
    fail_unless (group_proxy == NULL);
    fail_unless (error != NULL);
    g_error_free (error); error = NULL;

    /* create and add group */
    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", name,
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_if (group_id == GUM_GROUP_INVALID_GID);
    g_object_unref (group_proxy);
    group_proxy = NULL;

    group_proxy = _get_group_proxy_by_name (group_service, name, &error);
    fail_if (group_proxy == NULL, "Failed to get group for uid '%u' : %s",
            name, error ? error->message : "");

    /* check properties */
    g_object_get (G_OBJECT (group_proxy), "groupname", &str1, NULL);
    fail_if (str1 == NULL);
    fail_if (g_strcmp0 (name, str1) != 0);
    g_free (str1);

    /* check for object paths for groups with same uid */
    group_proxy2 = _get_group_proxy_by_name (group_service, name, &error);
    fail_if (group_proxy == NULL, "Failed to get group for uid '%u' : %s",
            group_id, error ? error->message : "");

    g_object_get (G_OBJECT (group_proxy), "g-object-path", &str1, NULL);
    g_object_get (G_OBJECT (group_proxy2), "g-object-path", &str2, NULL);
    fail_unless (g_strcmp0 (str1, str2) == 0);
    g_free (str1); g_free (str2);

    g_object_unref (group_proxy2);

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_delete_group)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;
    gid_t group_id = GUM_GROUP_INVALID_GID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    /* create and add group */
    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_delgroup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    g_object_unref (group_proxy);

    /* get group added-by-uid */
    group_proxy = _get_group_proxy_by_name (group_service, "test_delgroup1",
            &error);
    fail_if (group_proxy == NULL, "Failed to get group for name '%s' : %s",
            "test_addgroup2", error ? error->message : "");

    res = gum_dbus_group_call_delete_group_sync (group_proxy, NULL, &error);
    fail_if (res == FALSE, "Failed to delete new group : %s",
            error ? error->message : "");

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_update_group)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;
    gid_t group_id = GUM_GROUP_INVALID_GID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    /* create and add group */
    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_groupup1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_if (group_id == GUM_GROUP_INVALID_GID);

    g_object_set (G_OBJECT (group_proxy), "secret", "23456", NULL);

    res = gum_dbus_group_call_update_group_sync (group_proxy, NULL, &error);
    fail_if (res == FALSE, "Failed to update group : %s",
            error ? error->message : "");

    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_add_group_member)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;
    uid_t group_id = GUM_GROUP_INVALID_GID;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;
    uid_t user_id = GUM_USER_INVALID_UID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_addgroup_mem1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_unless (group_id != GUM_GROUP_INVALID_GID);

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");
    g_object_set (G_OBJECT (user_proxy), "username", "test_adgrpuser1",
            "secret", "123456", "nickname", "nick", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);

    /* add user to the group */
    res = gum_dbus_group_call_add_member_sync (group_proxy, user_id, TRUE,
            NULL, &error);
    fail_unless (res == TRUE);
    fail_unless (error == NULL);

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

START_TEST (test_delete_group_member)
{
    DBG ("\n");
    gboolean res = FALSE;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GumDbusGroupService *group_service = NULL;
    GumDbusGroup *group_proxy = NULL;
    uid_t group_id = GUM_GROUP_INVALID_GID;
    GumDbusUserService *user_service = NULL;
    GumDbusUser *user_proxy = NULL;
    uid_t user_id = GUM_USER_INVALID_UID;

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    user_service = _get_user_service (connection, &error);
    fail_if (user_service == NULL, "failed to get user_service : %s",
            error ? error->message : "");

    group_service = _get_group_service (connection, &error);
    fail_if (group_service == NULL, "failed to get group_service : %s",
            error ? error->message : "");

    group_proxy = _create_new_group_proxy (group_service, &error);
    fail_if (group_proxy == NULL, "Failed to create new group : %s",
            error ? error->message : "");

    g_object_set (G_OBJECT (group_proxy), "groupname", "test_delgroup_mem1",
            "secret", "123456", "grouptype", GUM_GROUPTYPE_USER, NULL);

    res = gum_dbus_group_call_add_group_sync (group_proxy,
            GUM_GROUP_INVALID_GID, &group_id, NULL, &error);
    fail_if (res == FALSE, "Failed to add new group : %s",
            error ? error->message : "");
    fail_unless (group_id != GUM_GROUP_INVALID_GID);

    /* create and add user */
    user_proxy = _create_new_user_proxy (user_service, &error);
    fail_if (user_proxy == NULL, "Failed to create new user : %s",
            error ? error->message : "");
    g_object_set (G_OBJECT (user_proxy), "username", "test_delgrpuser1",
            "secret", "123456", "nickname", "nick", "usertype",
            GUM_USERTYPE_NORMAL, NULL);

    res = gum_dbus_user_call_add_user_sync (user_proxy, &user_id, NULL,
            &error);
    fail_unless (res == TRUE);

    res = gum_dbus_group_call_add_member_sync (group_proxy, user_id, FALSE,
                NULL, &error);
    fail_unless (res == TRUE);
    fail_unless (error == NULL);

    /* delete user from the group */
    res = gum_dbus_group_call_delete_member_sync (group_proxy, user_id,
            NULL, &error);
    fail_unless (res == TRUE);
    fail_unless (error == NULL);

    g_object_unref (user_proxy);
    g_object_unref (user_service);
    g_object_unref (group_proxy);
    g_object_unref (group_service);
    g_object_unref (connection);
}
END_TEST

Suite* daemon_suite (void)
{
    TCase *tc = NULL;

    Suite *s = suite_create ("Gum daemon");
    
    tc = tcase_create ("Daemon tests");
    tcase_set_timeout(tc, 10);
    tcase_add_unchecked_fixture (tc, _setup_daemon, _teardown_daemon);
    tcase_add_checked_fixture (tc, _create_mainloop, _stop_mainloop);

    tcase_add_test (tc, test_daemon_user);
    tcase_add_test (tc, test_create_new_user);
    tcase_add_test (tc, test_add_user);
    tcase_add_test (tc, test_get_user_by_uid);
    tcase_add_test (tc, test_get_user_by_name);
    tcase_add_test (tc, test_delete_user);
    tcase_add_test (tc, test_update_user);

    tcase_add_test (tc, test_daemon_group);
    tcase_add_test (tc, test_create_new_group);
    tcase_add_test (tc, test_add_group);
    tcase_add_test (tc, test_get_group_by_uid);
    tcase_add_test (tc, test_get_group_by_name);
    tcase_add_test (tc, test_delete_group);
    tcase_add_test (tc, test_update_group);

    tcase_add_test (tc, test_add_group_member);
    tcase_add_test (tc, test_delete_group_member);
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
