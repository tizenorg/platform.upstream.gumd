/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2014 Intel Corporation.
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

#include <glib.h>
#include <stdlib.h>

#include "config.h"

#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-defines.h"
#include "common/gum-user-types.h"
#include "common/gum-group-types.h"
#include "gumd-daemon.h"
#include "gumd-daemon-user.h"
#include "gumd-daemon-group.h"

typedef struct {
    uid_t uid;
    gid_t gid;
    GumUserType usertype;
    gchar *user_name;
    gchar *nick_name;
    gchar *real_name;
    gchar *office;
    gchar *office_phone;
    gchar *home_phone;
    gchar *home_dir;
    gchar *shell;
    gchar *secret;
} OfflineUser;

typedef struct {
    gid_t gid;
    GumGroupType grouptype;
    gchar *group_name;
    gchar *grp_secret;

    uid_t mem_uid; /*used in adding/deleting a member from the group*/
} OfflineGroup;

static OfflineUser *
_create_offline_user ()
{
    OfflineUser *user = g_malloc0 (sizeof (OfflineUser));
    user->uid = GUM_USER_INVALID_UID;
    user->gid = GUM_GROUP_INVALID_GID;
    return user;
}

static OfflineGroup *
_create_offline_group ()
{
    OfflineGroup *group = g_malloc0 (sizeof (OfflineGroup));
    group->gid = GUM_GROUP_INVALID_GID;
    group->mem_uid = GUM_USER_INVALID_UID;
    return group;
}

static void
_free_offline_user (
        OfflineUser *user)
{
    if (user) {
        g_free (user->user_name); g_free (user->secret);
        g_free (user->nick_name); g_free (user->real_name);
        g_free (user->office); g_free (user->office_phone);
        g_free (user->home_phone); g_free (user->home_dir);
        g_free (user->shell);
        g_free (user);
    }
}

static void
_free_offline_group (
        OfflineGroup *group)
{
    if (group) {
        g_free (group->group_name); g_free (group->grp_secret);
        g_free (group);
    }
}

static void
_set_user_update_prop (
        GumdDaemonUser *guser,
        OfflineUser *user)
{
    if (user->real_name) {
        g_object_set (G_OBJECT (guser), "realname", user->real_name, NULL);
    }
    if (user->office) {
        g_object_set (G_OBJECT (guser), "office", user->office, NULL);
    }
    if (user->office_phone) {
        g_object_set (G_OBJECT (guser), "officephone", user->office_phone,
                NULL);
    }
    if (user->home_phone) {
        g_object_set (G_OBJECT (guser), "homephone", user->home_phone, NULL);
    }
    if (user->shell) {
        g_object_set (G_OBJECT (guser), "shell", user->shell, NULL);
    }
    if (user->secret) {
        g_object_set (G_OBJECT (guser), "secret", user->secret, NULL);
    }
}

static void
_set_user_prop (
        GumdDaemonUser *guser,
        OfflineUser *user)
{
    g_object_set (G_OBJECT (guser), "usertype", user->usertype, NULL);
    if (user->user_name) {
        g_object_set (G_OBJECT (guser), "username", user->user_name, NULL);
    }
    if (user->nick_name) {
        g_object_set (G_OBJECT (guser), "nickname", user->nick_name, NULL);
    }
    if (user->home_dir) {
        g_object_set (G_OBJECT (guser), "homedir", user->home_dir, NULL);
    }
    _set_user_update_prop (guser, user);
}

static void
_print_user_prop (
        GumdDaemonUser *guser)
{
    OfflineUser *user = _create_offline_user ();
    g_object_get (G_OBJECT (guser), "uid", &user->uid, NULL);
    g_object_get (G_OBJECT (guser), "gid", &user->gid, NULL);
    g_object_get (G_OBJECT (guser), "username", &user->user_name, NULL);
    g_object_get (G_OBJECT (guser), "nickname", &user->nick_name, NULL);
    g_object_get (G_OBJECT (guser), "realname", &user->real_name, NULL);
    g_object_get (G_OBJECT (guser), "office", &user->office, NULL);
    g_object_get (G_OBJECT (guser), "officephone", &user->office_phone,
            NULL);
    g_object_get (G_OBJECT (guser), "homephone", &user->home_phone, NULL);
    g_object_get (G_OBJECT (guser), "homedir", &user->home_dir, NULL);
    g_object_get (G_OBJECT (guser), "shell", &user->shell, NULL);

    WARN ("uid : %u", user->uid);
    WARN ("gid : %u", user->gid);
    WARN ("username : %s", user->user_name ? user->user_name : "UNKNOWN");
    WARN ("nickname : %s", user->nick_name ? user->nick_name : "UNKNOWN");
    WARN ("realname : %s", user->real_name ? user->real_name : "UNKNOWN");
    WARN ("office : %s", user->office ? user->office : "UNKNOWN");
    WARN ("officephone : %s",
            user->office_phone ? user->office_phone : "UNKNOWN");
    WARN ("homephone : %s", user->home_phone ? user->home_phone : "UNKNOWN");
    WARN ("homedir : %s", user->home_dir ? user->home_dir : "UNKNOWN");
    WARN ("shell : %s", user->shell ? user->shell : "UNKNOWN");

    _free_offline_user (user);
}

static gboolean
_handle_user_add (
        OfflineUser *user,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonUser *guser = NULL;
    guser = gumd_daemon_user_new (gumd_daemon_get_config (daemon));
    if (!guser) {
        WARN ("Failed user add op -- unable to setup user");
        return FALSE;
    }

    _set_user_prop (guser, user);

    if (!gumd_daemon_add_user (daemon, guser, &error)) {
        WARN ("Failed user add setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (guser);
        return FALSE;
    }

    _print_user_prop (guser);

    WARN ("User added successfully");
    g_object_unref (guser);
    return TRUE;
}

static gboolean
_handle_user_del (
        OfflineUser *user,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonUser *guser = NULL;
    guser = gumd_daemon_get_user (daemon, user->uid, &error);
    if (!guser) {
        WARN ("Failed user del with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!gumd_daemon_delete_user (daemon, guser, TRUE, &error)) {
        WARN ("Failed user del with error %s", error->message);
        g_error_free (error);
        g_object_unref (guser);
        return FALSE;
    }

    WARN ("User deleted successfully");
    g_object_unref (guser);
    return TRUE;
}

static gboolean
_handle_user_up (
        OfflineUser *user,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonUser *guser = NULL;
    guser = gumd_daemon_get_user (daemon, user->uid, &error);
    if (!guser) {
        WARN ("Failed user update with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _set_user_update_prop (guser, user);

    if (!gumd_daemon_update_user (daemon, guser, &error)) {
        WARN ("Failed user update with error %s", error->message);
        g_error_free (error);
        g_object_unref (guser);
        return FALSE;
    }

    _print_user_prop (guser);

    WARN ("User updated successfully");
    g_object_unref (guser);
    return TRUE;
}

static gboolean
_handle_user_get (
        OfflineUser *user,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonUser *guser = NULL;
    guser = gumd_daemon_get_user (daemon, user->uid, &error);
    if (!guser) {
        WARN ("Failed user get with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _print_user_prop (guser);

    WARN ("User retrieved by uid successfully");
    g_object_unref (guser);
    return TRUE;
}

static gboolean
_handle_user_get_by_name (
        OfflineUser *user,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonUser *guser = NULL;
    guser = gumd_daemon_get_user_by_name (daemon, user->user_name, &error);
    if (!guser) {
        WARN ("Failed user get by name with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _print_user_prop (guser);

    WARN ("User retrieved by name successfully");
    g_object_unref (guser);
    return TRUE;
}

static void
_set_group_update_prop (
        GumdDaemonGroup *grp,
        OfflineGroup *group)
{
    if (group->grp_secret) {
        g_object_set (G_OBJECT (grp), "secret", group->grp_secret, NULL);
    }
}

static void
_set_group_prop (
        GumdDaemonGroup *grp,
        OfflineGroup *group)
{
    g_object_set (G_OBJECT (grp), "grouptype", group->grouptype, NULL);
    if (group->group_name) {
        g_object_set (G_OBJECT (grp), "groupname", group->group_name, NULL);
    }
    _set_group_update_prop (grp, group);
}

static void
_print_group_prop (
        GumdDaemonGroup *grp)
{
    OfflineGroup *group = _create_offline_group ();

    g_object_get (G_OBJECT (grp), "gid", &group->gid, NULL);
    g_object_get (G_OBJECT (grp), "groupname", &group->group_name, NULL);

    WARN ("gid : %u", group->gid);
    WARN ("groupname : %s", group->group_name ? group->group_name : "UNKNOWN");

    _free_offline_group (group);
}

static gboolean
_handle_group_add (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_group_new (gumd_daemon_get_config (daemon));
    if (!grp) {
        WARN ("Failed group add op -- unable to setup user");
        return FALSE;
    }

    _set_group_prop (grp, group);

    if (!gumd_daemon_add_group (daemon, grp, &error)) {
        WARN ("Failed group add setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (grp);
        return FALSE;
    }

    _print_group_prop (grp);

    WARN ("Group added successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_del (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group (daemon, group->gid, &error);
    if (!grp) {
        WARN ("Failed group del setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!gumd_daemon_delete_group (daemon, grp, &error)) {
        WARN ("Failed group del setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (grp);
        return FALSE;
    }

    WARN ("Group deleted successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_up (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group (daemon, group->gid, &error);
    if (!grp) {
        WARN ("Failed group update setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _set_group_update_prop (grp, group);

    if (!gumd_daemon_update_group (daemon, grp, &error)) {
        WARN ("Failed group update setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (grp);
        return FALSE;
    }

    _print_group_prop (grp);

    WARN ("Group updated successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_get (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group (daemon, group->gid, &error);
    if (!grp) {
        WARN ("Failed group get setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _print_group_prop (grp);

    WARN ("Group get by uid successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_get_by_name (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group_by_name (daemon, group->group_name, &error);
    if (!grp) {
        WARN ("Failed group get by name setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    _print_group_prop (grp);

    WARN ("Group get by name successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_add_mem (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group (daemon, group->gid, &error);
    if (!grp) {
        WARN ("Failed group add member setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!gumd_daemon_add_group_member (daemon, grp, group->mem_uid, TRUE,
            &error)) {
        WARN ("Failed group add member setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (grp);
        return FALSE;
    }

    WARN ("Group member added successfully");
    g_object_unref (grp);
    return TRUE;
}

static gboolean
_handle_group_del_mem (
        OfflineGroup *group,
        GumdDaemon *daemon)
{
    GError *error = NULL;
    GumdDaemonGroup *grp = NULL;
    grp = gumd_daemon_get_group (daemon, group->gid, &error);
    if (!grp) {
        WARN ("Failed group del member setup with error %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!gumd_daemon_delete_group_member (daemon, grp, group->mem_uid,
            &error)) {
        WARN ("Failed group del member setup with error %s", error->message);
        g_error_free (error);
        g_object_unref (grp);
        return FALSE;
    }

    WARN ("Group member deleted successfully");
    g_object_unref (grp);
    return TRUE;
}

gboolean
gumd_daemon_offline_handle_op (
        int argc,
        char *argv[],
        gboolean *offline_mode)
{
    GError *error = NULL;
    GOptionContext *context;
    gboolean rval = TRUE;

    gboolean is_user_add_op = FALSE, is_user_del_op = FALSE;
    gboolean is_user_up_op = FALSE, is_user_get_op = FALSE;
    gboolean is_user_get_by_name_op = FALSE;
    GOptionGroup* user_option = NULL;
    OfflineUser *user = NULL;

    gboolean is_group_add_op = FALSE, is_group_del_op = FALSE;
    gboolean is_group_up_op = FALSE, is_group_get_op = FALSE;
    gboolean is_group_get_by_name_op = FALSE, is_group_add_mem_op = FALSE;
    gboolean is_group_del_mem_op = FALSE;
    GOptionGroup* group_option = NULL;
    OfflineGroup *group = NULL;

    user = _create_offline_user();
    group = _create_offline_group();

    GOptionEntry main_entries[] =
    {
        { "offline", 'o', 0, G_OPTION_ARG_NONE, offline_mode, "gumd starts "
                "with offline mode, performs the operation and exits. One "
                "of the operations add/delete/update/get user/group is "
                "mandatory",
                NULL},
        { "add-user", 'a', 0, G_OPTION_ARG_NONE, &is_user_add_op, "add user"
                " -- username (or nickname) and usertype is mandatory "
                "[OFFLINE MODE]", NULL},
        { "delete-user", 'd', 0, G_OPTION_ARG_NONE, &is_user_del_op,
                "delete user -- uid is mandatory [OFFLINE MODE]", NULL},
        { "update-user", 'u', 0, G_OPTION_ARG_NONE, &is_user_up_op,
                "update user -- uid is mandatory; possible props updates are "
                "secret, realname, office, officephone, homephone "
                "and shell [OFFLINE MODE]", NULL},
        { "get-user", 'b', 0, G_OPTION_ARG_NONE, &is_user_get_op, "get user"
                " -- uid is mandatory [OFFLINE MODE]", NULL},
        { "get-user-by-name", 'c', 0, G_OPTION_ARG_NONE,
                &is_user_get_by_name_op, "get user by name -- username is"
                        " mandatory [OFFLINE MODE]", NULL},

        { "add-group", 'g', 0, G_OPTION_ARG_NONE, &is_group_add_op, "add group"
                " -- groupname and grouptype are mandatory [OFFLINE MODE]",
                NULL},
        { "delete-group", 'h', 0, G_OPTION_ARG_NONE, &is_group_del_op,
                "delete group -- gid is mandatory [OFFLINE MODE]", NULL},
        { "update-group", 'i', 0, G_OPTION_ARG_NONE, &is_group_up_op,
                "update group -- gid is mandatory; possible props that can be "
                "updated is secret [OFFLINE MODE]", NULL},
        { "get-group", 'j', 0, G_OPTION_ARG_NONE, &is_group_get_op, "get group"
                " -- gid is mandatory [OFFLINE MODE]",
                NULL},
        { "get-group-by-name", 'k', 0, G_OPTION_ARG_NONE,
                &is_group_get_by_name_op, "get group by name -- groupname "
                        "[OFFLINE MODE] is mandatory", NULL},
        { "add-member", 'm', 0, G_OPTION_ARG_NONE, &is_group_add_mem_op,
                "group add member -- gid and mem_uid are mandatory", NULL},
        { "delete-member", 'n', 0, G_OPTION_ARG_NONE, &is_group_del_mem_op,
                "group delete member -- gid and mem_uid are mandatory "
                "[OFFLINE MODE]", NULL},
        { NULL }
    };

    GOptionEntry user_entries[] =
    {
        { "username", 0, 0, G_OPTION_ARG_STRING, &user->user_name,
                "user name [OFFLINE MODE]", "name"},
        { "usertype", 0, 0, G_OPTION_ARG_INT, &user->usertype,
                "usertype can be system(1), admin(2), guest(3), normal(4) "
                "[OFFLINE MODE]", "type"},
        { "uid", 0, 0, G_OPTION_ARG_INT, &user->uid, "user id "
                "[OFFLINE MODE]", "uid"},
        { "ugid", 0, 0, G_OPTION_ARG_INT, &user->gid, "group id "
                "[OFFLINE MODE]", "gid"},
        { "usecret", 0, 0, G_OPTION_ARG_STRING, &user->secret,
                "secret in plain text [OFFLINE MODE]", "secret"},
        { "nickname", 0, 0, G_OPTION_ARG_STRING, &user->nick_name, "nick name "
                "[OFFLINE MODE]", "nick name"},
        { "realname", 0, 0, G_OPTION_ARG_STRING, &user->real_name, "real name "
                "[OFFLINE MODE]", "real name"},
        { "office", 0, 0, G_OPTION_ARG_STRING, &user->office, "Office location "
                "[OFFLINE MODE]", "office"},
        { "officephone", 0, 0, G_OPTION_ARG_STRING, &user->office_phone,
                "office phone [OFFLINE MODE]", "phone"},
        { "homephone", 0, 0, G_OPTION_ARG_STRING, &user->home_phone,
                "home phone [OFFLINE MODE]", "phone"},
        { "homedir", 0, 0, G_OPTION_ARG_STRING, &user->home_dir, "home dir "
                "[OFFLINE MODE]", "dir"},
        { "shell", 0, 0, G_OPTION_ARG_STRING, &user->shell, "shell path "
                "[OFFLINE MODE]", "shell"},
        { NULL }
    };

    GOptionEntry group_entries[] =
    {
        { "groupname", 0, 0, G_OPTION_ARG_STRING, &group->group_name,
                "group name [OFFLINE MODE]", "name"},
        { "grouptype", 0, 0, G_OPTION_ARG_INT, &group->grouptype,
                "group type can be system(1), user(2) [OFFLINE MODE]",
                "type"},
        { "gid", 0, 0, G_OPTION_ARG_INT, &group->gid, "group id "
                "[OFFLINE MODE]", "gid"},
        { "gsecret", 0, 0, G_OPTION_ARG_STRING, &group->grp_secret,
                "group secret in plain text [OFFLINE MODE]", "secret"},
        { "mem_uid", 0, 0, G_OPTION_ARG_INT, &group->mem_uid, "mem uid "
                "[OFFLINE MODE]", "mem_uid"},
        { NULL }
    };

    context = g_option_context_new ("\n"
            "  To run gumd in non-offline mode, ./gumd\n"
            "  To run gumd in offline mode (e.g. to delete a user), ./gumd "
            "--offline -d --uid=2001\n");
    g_option_context_add_main_entries (context, main_entries, NULL);

    user_option = g_option_group_new("user-options", "User specific options "
            "[OFFLINE MODE]", "User specific options", NULL, NULL);
    g_option_group_add_entries(user_option, user_entries);
    g_option_context_add_group (context, user_option);

    group_option = g_option_group_new("group-options", "Group specific options "
            "[OFFLINE MODE]", "Group specific options", NULL, NULL);
    g_option_group_add_entries(group_option, group_entries);
    g_option_context_add_group (context, group_option);

    rval = g_option_context_parse (context, &argc, &argv, &error);
    g_option_context_free(context);
    if (!rval) {
        WARN ("option parsing failed: %s\n", error->message);
        _free_offline_user (user);
        _free_offline_group (group);
        return FALSE;
    }

    if (!*offline_mode) {
        WARN ("No offline mode for gumd\n");
        return FALSE;
    }

    GumdDaemon *daemon = gumd_daemon_new ();
    if (is_user_add_op) {
        rval = _handle_user_add (user, daemon);
    } else if (is_user_del_op) {
        rval = _handle_user_del (user, daemon);
    } else if (is_user_up_op) {
        rval = _handle_user_up (user, daemon);
    } else if (is_user_get_op) {
        rval = _handle_user_get (user, daemon);
    } else if (is_user_get_by_name_op) {
        rval = _handle_user_get_by_name (user, daemon);
    }

    /* group */
    else if (is_group_add_op) {
        rval = _handle_group_add (group, daemon);
    } else if (is_group_del_op) {
        rval = _handle_group_del (group, daemon);
    } else if (is_group_up_op) {
        rval = _handle_group_up (group, daemon);
    } else if (is_group_get_op) {
        rval = _handle_group_get (group, daemon);
    } else if (is_group_get_by_name_op) {
        rval = _handle_group_get_by_name (group, daemon);
    } else if (is_group_add_mem_op) {
        rval = _handle_group_add_mem (group, daemon);
    } else if (is_group_del_mem_op) {
        rval = _handle_group_del_mem (group, daemon);
    } else {
        WARN ("No option specified");
        rval = FALSE;
    }

    _free_offline_user (user);
    _free_offline_group (group);
    g_object_unref (daemon);

    return rval;
}
