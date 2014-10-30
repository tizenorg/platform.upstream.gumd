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

#include <glib.h>
#include <stdlib.h>

#include "gum-user.h"
#include "common/gum-log.h"
#include "common/gum-config.h"
#include "common/gum-defines.h"
#include "common/gum-error.h"
#include "common/gum-user-types.h"
#include "gum-group.h"
#include "common/gum-group-types.h"
#include "config.h"

#ifdef HAVE_LIBTLM_NFC
#include <gtlm-nfc.h>
#endif

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
} InputUser;

typedef struct {
    gid_t gid;
    GumGroupType grouptype;
    gchar *group_name;
    gchar *grp_secret;

    uid_t mem_uid; /*used in adding/deleting a member from the group*/
} InputGroup;

static gboolean offline_mode = FALSE;

static InputUser *
_create_test_user ()
{
    InputUser *user = g_malloc0 (sizeof (InputUser));
    user->uid = GUM_USER_INVALID_UID;
    user->gid = GUM_GROUP_INVALID_GID;
    return user;
}

static InputGroup *
_create_test_group ()
{
    InputGroup *group = g_malloc0 (sizeof (InputGroup));
    group->gid = GUM_GROUP_INVALID_GID;
    group->mem_uid = GUM_USER_INVALID_UID;
    return group;
}

static void
_free_test_user (
		InputUser *user)
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
_free_test_group (
		InputGroup *group)
{
    if (group) {
        g_free (group->group_name); g_free (group->grp_secret);
        g_free (group);
    }
}

static void
_set_user_update_prop (
        GumUser *guser,
        InputUser *user)
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
        GumUser *guser,
        InputUser *user)
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
		GumUser *guser)
{
	InputUser *user = _create_test_user ();
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

    INFO ("uid : %u", user->uid);
    INFO ("gid : %u", user->gid);
    INFO ("username : %s", user->user_name ? user->user_name : "UNKNOWN");
    INFO ("nickname : %s", user->nick_name ? user->nick_name : "UNKNOWN");
    INFO ("realname : %s", user->real_name ? user->real_name : "UNKNOWN");
    INFO ("office : %s", user->office ? user->office : "UNKNOWN");
    INFO ("officephone : %s",
    		user->office_phone ? user->office_phone : "UNKNOWN");
    INFO ("homephone : %s", user->home_phone ? user->home_phone : "UNKNOWN");
    INFO ("homedir : %s", user->home_dir ? user->home_dir : "UNKNOWN");
    INFO ("shell : %s", user->shell ? user->shell : "UNKNOWN");

    _free_test_user (user);
}

#ifdef HAVE_LIBTLM_NFC
static void _on_tag_found(GTlmNfc* tlm_nfc,
                          const gchar* tag_path,
                          gpointer user_data)
{
    g_print("Tag %s found\n", tag_path);

    g_print("Waiting 5 seconds due to https://01.org/jira/browse/NFC-57\n");
    sleep(5);

    GumUser* user = GUM_USER(user_data);

    gchar* user_name;
    gchar* secret;

    g_object_get (G_OBJECT (user), "username", &user_name, NULL);
    g_object_get (G_OBJECT (user), "secret", &secret, NULL);

    GError* error = NULL;
    g_print("Writing to tag...");
    gtlm_nfc_write_username_password(tlm_nfc,
                                     tag_path,
                                     user_name,
                                     secret,
                                     &error);

    if (!error)
        g_print("success!\n");
    else
        g_print("error: %s\n", error->message);

    g_free(user_name);
    g_free(secret);

    _stop_mainloop();
}

static void
_handle_write_nfc (
        GumUser *user)
{
    g_print("Writing the username and password to NFC tag; please"
            " place a tag next to the NFC adapter\n");

    GTlmNfc* tlm_nfc = g_object_new(G_TYPE_TLM_NFC, NULL);
    G_IS_TLM_NFC(tlm_nfc);
    g_signal_connect(tlm_nfc, "tag-found", G_CALLBACK(_on_tag_found), user);

    _run_mainloop();

    g_object_unref(tlm_nfc);

}
#else
static void
_handle_write_nfc (
        GumUser *user)
{
    g_print("gum-utils was compiled without libtlm-nfc support\n");
}
#endif

static void
_handle_user_add (
        InputUser *user,
        gboolean write_nfc)
{
    GumUser *guser = NULL;
    guser = gum_user_create_sync (offline_mode);
    if (!guser) return;

    _set_user_prop (guser, user);

    if (!gum_user_add_sync (guser)) {
        INFO ("Failed user add setup");
        g_object_unref (guser);
        return;
    }

    INFO ("User added successfully");
    _print_user_prop (guser);

    if (write_nfc) {
        _handle_write_nfc(guser);
    }
    g_object_unref (guser);
}

static void
_handle_user_del (
		InputUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_sync (user->uid, offline_mode);
    if (!guser) return;

    if (!gum_user_delete_sync (guser, TRUE)) {
        INFO ("Failed user update setup");
        g_object_unref (guser);
        return;
    }

    INFO ("User deleted successfully");
    g_object_unref (guser);
}

static void
_handle_user_up (
		InputUser *user,
                gboolean write_nfc
                )
{
    GumUser *guser = NULL;

    guser = gum_user_get_sync (user->uid, offline_mode);
    if (!guser) return;

    _set_user_update_prop (guser, user);

    if (!gum_user_update_sync (guser)) {
        INFO ("Failed user update setup");
        g_object_unref (guser);
        return;
    }

    INFO ("User updated successfully");
    _print_user_prop (guser);

    if (write_nfc) {
        _handle_write_nfc(guser);
    }
    g_object_unref (guser);
}

static void
_handle_user_get (
		InputUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_sync (user->uid, offline_mode);
    if (!guser) return;

    INFO ("User retrieved successfully");
    _print_user_prop (guser);

    g_object_unref (guser);
}

static void
_handle_user_get_by_name (
		InputUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_by_name_sync (user->user_name, offline_mode);
    if (!guser) return;

    INFO ("User retrieved by name successfully");
    _print_user_prop (guser);

    g_object_unref (guser);
}

static void
_set_group_update_prop (
        GumGroup *grp,
        InputGroup *group)
{
    if (group->grp_secret) {
        g_object_set (G_OBJECT (grp), "secret", group->grp_secret, NULL);
    }
}

static void
_set_group_prop (
        GumGroup *grp,
        InputGroup *group)
{
    g_object_set (G_OBJECT (grp), "grouptype", group->grouptype, NULL);
    if (group->group_name) {
        g_object_set (G_OBJECT (grp), "groupname", group->group_name, NULL);
    }
    _set_group_update_prop (grp, group);
}


static void
_print_group_prop (
		GumGroup *grp)
{
	InputGroup *group = _create_test_group ();

	g_object_get (G_OBJECT (grp), "gid", &group->gid, NULL);
    g_object_get (G_OBJECT (grp), "groupname", &group->group_name, NULL);

    INFO ("gid : %u", group->gid);
    INFO ("groupname : %s", group->group_name ? group->group_name : "UNKNOWN");

    _free_test_group (group);
}

static void
_handle_group_add (
        InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_create_sync (offline_mode);
    if (!grp) return;

    _set_group_prop (grp, group);

    if (!gum_group_add_sync (grp)) {
        INFO ("Failed group add setup");
        g_object_unref (grp);
        return;
    }

    INFO ("Group added successfully");
    _print_group_prop (grp);

    g_object_unref (grp);
}

static void
_handle_group_del (
        InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid, offline_mode);
    if (!grp) return;

    if (!gum_group_delete_sync (grp)) {
        INFO ("Failed group delete setup");
        g_object_unref (grp);
        return;
    }

    INFO ("Group deleted successfully");
    g_object_unref (grp);
}

static void
_handle_group_up (
        InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid, offline_mode);
    if (!grp) return;

    _set_group_update_prop (grp, group);

    if (!gum_group_update_sync (grp)) {
        INFO ("Failed group update setup");
        g_object_unref (grp);
        return;
    }

    INFO ("Group updated successfully");
    _print_group_prop (grp);

    g_object_unref (grp);
}

static void
_handle_group_get (
		InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid, offline_mode);
    if (!grp) return;

    INFO ("Group retrieved successfully");
    _print_group_prop (grp);

    g_object_unref (grp);
}

static void
_handle_group_get_by_name (
		InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_by_name_sync (group->group_name, offline_mode);
    if (!grp) return;

    INFO ("Group retrieved by name successfully");
    _print_group_prop (grp);

    g_object_unref (grp);
}

static void
_handle_group_add_mem (
        InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid, offline_mode);
    if (!grp) return;

    if (!gum_group_add_member_sync (grp, group->mem_uid, TRUE)) {
        INFO ("Failed group addmem setup");
        g_object_unref (grp);
        return;
    }

    INFO ("Group mem added successfully");
    g_object_unref (grp);
}

static void
_handle_group_del_mem (
        InputGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid, offline_mode);
    if (!grp) return;

    if (!gum_group_delete_member_sync (grp, group->mem_uid)) {
        INFO ("Failed group delmem setup");
        g_object_unref (grp);
        return;
    }
    INFO ("Group mem deleted successfully");

    g_object_unref (grp);
}

int
main (int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context = NULL;
    gboolean rval = FALSE;
    GumConfig *config = NULL;
    gchar *sysroot = NULL;

    gboolean is_user_add_op = FALSE, is_user_del_op = FALSE;
    gboolean is_user_up_op = FALSE, is_user_get_op = FALSE;
    gboolean is_user_get_by_name_op = FALSE;
    GOptionGroup* user_option = NULL;
    InputUser *user = NULL;
    
    gboolean is_group_add_op = FALSE, is_group_del_op = FALSE;
    gboolean is_group_up_op = FALSE, is_group_get_op = FALSE;
    gboolean is_group_get_by_name_op = FALSE, is_group_add_mem_op = FALSE;
    gboolean is_group_del_mem_op = FALSE;
    gboolean is_write_nfc = FALSE;
    GOptionGroup* group_option = NULL;
    InputGroup *group = NULL;

    user = _create_test_user();
    group = _create_test_group();

    GOptionEntry main_entries[] =
    {
        { "offline", 'o', 0, G_OPTION_ARG_NONE, &offline_mode,
                "when gum-utils is invoked in offline mode, it triggers "
                "synchronous APIs in offline mode. Effectively libgum will "
                "perform the sync op add/delete/update/get without (dbus) gumd",
                NULL},
        { "sysroot", 0, 0, G_OPTION_ARG_STRING, &sysroot, "sysroot path "
                "[Offline mode ONLY]", "sysroot"},
        { "add-user", 'a', 0, G_OPTION_ARG_NONE, &is_user_add_op, "add user"
                " -- username (or nickname) and usertype is mandatory", NULL},
        { "delete-user", 'd', 0, G_OPTION_ARG_NONE, &is_user_del_op,
                "delete user -- uid is mandatory", NULL},
        { "update-user", 'u', 0, G_OPTION_ARG_NONE, &is_user_up_op,
                "update user -- uid is mandatory; possible props that can be "
                "updated are secret, realname, office, officephone, homephone "
                "and shell", NULL},
        { "get-user", 'b', 0, G_OPTION_ARG_NONE, &is_user_get_op, "get user"
                " -- uid is mandatory", NULL},
        { "get-user-by-name", 'c', 0, G_OPTION_ARG_NONE,
                &is_user_get_by_name_op, "get user by name -- username is"
                        " mandatory", NULL},

        { "add-group", 'g', 0, G_OPTION_ARG_NONE, &is_group_add_op, "add group"
                " -- groupname and grouptype are mandatory", NULL},
        { "delete-group", 'h', 0, G_OPTION_ARG_NONE, &is_group_del_op,
                "delete group -- gid is mandatory", NULL},
        { "update-group", 'i', 0, G_OPTION_ARG_NONE, &is_group_up_op,
                "update group -- gid is mandatory; possible props that can be"
                "updated are secret", NULL},
        { "get-group", 'j', 0, G_OPTION_ARG_NONE, &is_group_get_op, "get group"
                " -- gid is mandatory",
                NULL},
        { "get-group-by-name", 'k', 0, G_OPTION_ARG_NONE,
                &is_group_get_by_name_op, "get group by name -- groupname"
                        " is mandatory", NULL},
        { "add-member", 'm', 0, G_OPTION_ARG_NONE, &is_group_add_mem_op,
                "group add member -- gid and mem_uid are mandatory", NULL},
        { "delete-member", 'n', 0, G_OPTION_ARG_NONE, &is_group_del_mem_op,
                "group delete member -- gid and mem_uid are mandatory", NULL},
        { "write-nfc", 0, 0, G_OPTION_ARG_NONE, &is_write_nfc,
                "write username and secret to an NFC tag when creating or"
                " updating a user", NULL},
        { NULL }
    };
    
    GOptionEntry user_entries[] =
    {
        { "username", 0, 0, G_OPTION_ARG_STRING, &user->user_name,
                "user name", "name"},
        { "usertype", 0, 0, G_OPTION_ARG_INT, &user->usertype,
                "usertype can be system(1), admin(2), guest(3), normal(4). ",
                "type"},
        { "uid", 0, 0, G_OPTION_ARG_INT, &user->uid, "user id", "uid"},
        { "ugid", 0, 0, G_OPTION_ARG_INT, &user->gid, "group id", "gid"},
        { "usecret", 0, 0, G_OPTION_ARG_STRING, &user->secret,
                "secret in plain text", "secret"},
        { "nickname", 0, 0, G_OPTION_ARG_STRING, &user->nick_name, "nick name",
                "nick name"},
        { "realname", 0, 0, G_OPTION_ARG_STRING, &user->real_name, "real name",
                "real name"},
        { "office", 0, 0, G_OPTION_ARG_STRING, &user->office, "Office location",
                "office"},
        { "officephone", 0, 0, G_OPTION_ARG_STRING, &user->office_phone,
                "office phone", "phone"},
        { "homephone", 0, 0, G_OPTION_ARG_STRING, &user->home_phone,
                "home phone", "phone"},
        { "homedir", 0, 0, G_OPTION_ARG_STRING, &user->home_dir, "home dir",
                "dir"},
        { "shell", 0, 0, G_OPTION_ARG_STRING, &user->shell, "shell path",
                "shell"},
        { NULL }
    };

    GOptionEntry group_entries[] =
    {
        { "groupname", 0, 0, G_OPTION_ARG_STRING, &group->group_name,
                "group name", "name"},
        { "grouptype", 0, 0, G_OPTION_ARG_INT, &group->grouptype,
                "group type can be system(1), user(2).", "type"},
        { "gid", 0, 0, G_OPTION_ARG_INT, &group->gid, "group id", "gid"},
        { "gsecret", 0, 0, G_OPTION_ARG_STRING, &group->grp_secret,
                "group secret in plain text", "secret"},
        { "mem_uid", 0, 0, G_OPTION_ARG_INT, &group->mem_uid, "mem uid",
        		"mem_uid"},
        { NULL }
    };

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif   
    
    context = g_option_context_new ("\n"
            "  To add user in non-offline mode, gum-utils -a --username=user1 "
            "  --usertype=4\n"
            "  To delete user in offline mode, gum-utils -o -d --uid=2001\n");
    g_option_context_add_main_entries (context, main_entries, NULL);

    user_option = g_option_group_new("user-options", "User specific options",
            "User specific options", NULL, NULL);
    g_option_group_add_entries(user_option, user_entries);
    g_option_context_add_group (context, user_option);

    group_option = g_option_group_new("group-options", "Group specific options",
            "Group specific options", NULL, NULL);
    g_option_group_add_entries(group_option, group_entries);
    g_option_context_add_group (context, group_option);

    rval = g_option_context_parse (context, &argc, &argv, &error);
    g_option_context_free(context);
    if (!rval) {
        INFO ("option parsing failed: %s\n", error->message);
        g_free (user);
        g_free (group);
        exit (1);
    }

    if (!offline_mode && sysroot) {
        INFO ("sysroot is ONLY supported in offline mode\n");
        g_free (sysroot); sysroot = NULL;
    }

    config = gum_config_new (sysroot);
    
    if (is_user_add_op) {
        _handle_user_add (user, is_write_nfc);
    } else if (is_user_del_op) {
    	_handle_user_del (user);
    } else if (is_user_up_op) {
    	_handle_user_up (user, is_write_nfc);
    } else if (is_user_get_op) {
    	_handle_user_get (user);
    } else if (is_user_get_by_name_op) {
    	_handle_user_get_by_name (user);
    }

    /* group */
    else if (is_group_add_op) {
    	_handle_group_add (group);
    } else if (is_group_del_op) {
    	_handle_group_del (group);
    } else if (is_group_up_op) {
    	_handle_group_up (group);
    } else if (is_group_get_op) {
    	_handle_group_get (group);
    } else if (is_group_get_by_name_op) {
    	_handle_group_get_by_name (group);
    } else if (is_group_add_mem_op) {
    	_handle_group_add_mem (group);
    } else if (is_group_del_mem_op) {
    	_handle_group_del_mem (group);
    } else {
        INFO ("No option specified");
    }

    if (config) g_object_unref (config);
    _free_test_user (user);
    _free_test_group (group);

    return 0;
}