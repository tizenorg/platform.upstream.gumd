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
#include "common/gum-error.h"
#include "common/gum-user-types.h"
#include "gum-group.h"
#include "common/gum-group-types.h"
#include "common/gum-defines.h"

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
} TestUser;

typedef struct {
    gid_t gid;
    GumGroupType grouptype;
    gchar *group_name;
    gchar *grp_secret;

    uid_t mem_uid; /*used in adding/deleting a member from the group*/
} TestGroup;

static GMainLoop *main_loop = NULL;
static GError *op_error = NULL;

static TestUser *
_create_test_user ()
{
    TestUser *user = g_malloc0 (sizeof (TestUser));
    user->uid = GUM_USER_INVALID_UID;
    user->gid = GUM_GROUP_INVALID_GID;
    return user;
}

static TestGroup *
_create_test_group ()
{
    TestGroup *group = g_malloc0 (sizeof (TestGroup));
    group->gid = GUM_GROUP_INVALID_GID;
    group->mem_uid = GUM_USER_INVALID_UID;
    return group;
}

static void
_free_test_user (
		TestUser *user)
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
		TestGroup *group)
{
    if (group) {
        g_free (group->group_name); g_free (group->grp_secret);
        g_free (group);
    }
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
    if (main_loop) {
        g_main_loop_run (main_loop);
    }
}

static void
_on_user_op (
        GumUser *user,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    if (error) {
        op_error = g_error_copy (error);
    }
}

static void
_set_user_prop (
		GumUser *guser,
		TestUser *user)
{
	g_object_set (G_OBJECT (guser), "usertype", user->usertype, NULL);
	if (user->user_name) {
		g_object_set (G_OBJECT (guser), "username", user->user_name, NULL);
	}
	if (user->nick_name) {
		g_object_set (G_OBJECT (guser), "nickname", user->nick_name, NULL);
	}
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
	if (user->home_dir) {
		g_object_set (G_OBJECT (guser), "homedir", user->home_dir, NULL);
	}
	if (user->shell) {
		g_object_set (G_OBJECT (guser), "shell", user->shell, NULL);
	}
	if (user->secret) {
		g_object_set (G_OBJECT (guser), "secret", user->secret, NULL);
	}

}

static void
_print_user_prop (
		GumUser *guser)
{
	TestUser *user = _create_test_user ();
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
    g_object_get (G_OBJECT (guser), "secret", &user->secret, NULL);

    DBG ("uid : %u", user->uid);
    DBG ("gid : %u", user->gid);
    DBG ("username : %s", user->user_name ? user->user_name : "UNKNOWN");
    DBG ("nickname : %s", user->nick_name ? user->nick_name : "UNKNOWN");
    DBG ("realname : %s", user->real_name ? user->real_name : "UNKNOWN");
    DBG ("office : %s", user->office ? user->office : "UNKNOWN");
    DBG ("officephone : %s",
    		user->office_phone ? user->office_phone : "UNKNOWN");
    DBG ("homephone : %s", user->home_phone ? user->home_phone : "UNKNOWN");
    DBG ("homedir : %s", user->home_dir ? user->home_dir : "UNKNOWN");
    DBG ("shell : %s", user->shell ? user->shell : "UNKNOWN");
    DBG ("secret : %s", user->secret ? user->secret : "UNKNOWN");

    _free_test_user (user);
}

static void
_handle_user_add (
        TestUser *user)
{
    GumUser *guser = NULL;
    guser = gum_user_create_sync ();
    if (!guser) return;

    _set_user_prop (guser, user);

    if (!gum_user_add_sync (guser)) {
        WARN ("Failed user add setup");
        g_object_unref (guser);
        return;
    }

    _print_user_prop (guser);

    DBG ("User added successfully");

    g_object_unref (guser);
}

static void
_handle_user_del (
		TestUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_sync (user->uid);
    if (!guser) return;

    if (!gum_user_delete_sync (guser, TRUE)) {
        WARN ("Failed user update setup");
        g_object_unref (guser);
        return;
    }

    DBG ("User deleted successfully");

    g_object_unref (guser);
}

static void
_handle_user_up (
		TestUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get (user->uid, _on_user_op, NULL);
    if (!guser) return;

    _run_mainloop ();
    if (op_error) {
        WARN ("Failed get user for uid %d -- %d:%s", user->uid, op_error->code,
                op_error->message);
        g_object_unref (guser);
        g_error_free (op_error);
        return;
    }

    _set_user_prop (guser, user);

    if (!gum_user_update (guser, _on_user_op, NULL)) {
        WARN ("Failed user update setup");
        g_object_unref (guser);
        return;
    }
    _run_mainloop ();
    if (op_error) {
        WARN ("Failed user update -- %d:%s", op_error->code, op_error->message);
        g_error_free (op_error);
    } else {
    	_print_user_prop (guser);
        DBG ("User updated successfully");
    }

    g_object_unref (guser);
}

static void
_handle_user_get (
		TestUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_sync (user->uid);
    if (!guser) return;

    DBG ("User retrieved successfully");
    _print_user_prop (guser);

    if (guser) {
    	g_object_unref (guser);
    }
}

static void
_handle_user_get_by_name (
		TestUser *user)
{
    GumUser *guser = NULL;

    guser = gum_user_get_by_name (user->user_name, _on_user_op, NULL);
    if (!guser) return;

    _run_mainloop ();

    if (op_error) {
        WARN ("Failed user get by name -- %d:%s", op_error->code,
        		op_error->message);
        g_error_free (op_error);
    } else {
        DBG ("User retrieved by name successfully");
        _print_user_prop (guser);
    }

    if (guser) {
    	g_object_unref (guser);
    }
}

static void
_on_group_op (
        GumGroup *group,
        const GError *error,
        gpointer user_data)
{
    _stop_mainloop ();

    if (error) {
        op_error = g_error_copy (error);
    }
}

static void
_set_group_prop (
		GumGroup *grp,
		TestGroup *group)
{
	g_object_set (G_OBJECT (grp), "grouptype", group->grouptype, NULL);
	if (group->group_name) {
		g_object_set (G_OBJECT (grp), "groupname", group->group_name, NULL);
	}
	if (group->grp_secret) {
		g_object_set (G_OBJECT (grp), "secret", group->grp_secret, NULL);
	}
}

static void
_print_group_prop (
		GumGroup *grp)
{
	TestGroup *group = _create_test_group ();

	g_object_get (G_OBJECT (grp), "gid", &group->gid, NULL);
    g_object_get (G_OBJECT (grp), "groupname", &group->group_name, NULL);
    g_object_get (G_OBJECT (grp), "secret", &group->grp_secret, NULL);

    DBG ("gid : %u", group->gid);
    DBG ("groupname : %s", group->group_name ? group->group_name : "UNKNOWN");
    DBG ("secret : %s", group->grp_secret ? group->grp_secret : "UNKNOWN");

    _free_test_group (group);
}

static void
_handle_group_add (
        TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_create_sync (_on_group_op);
    if (!grp) return;

    _set_group_prop (grp, group);

    if (!gum_group_add_sync (grp)) {
        WARN ("Failed group add setup");
        g_object_unref (grp);
        return;
    }

    _print_group_prop (grp);
    DBG ("Group added successfully");

    g_object_unref (grp);
}

static void
_handle_group_del (
        TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid);
    if (!grp) return;

    if (!gum_group_delete_sync (grp)) {
        WARN ("Failed group delete setup");
        g_object_unref (grp);
        return;
    }

    DBG ("Group deleted successfully");
    g_object_unref (grp);
}

static void
_handle_group_up (
        TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get (group->gid, _on_group_op, NULL);
    if (!grp) return;

    _run_mainloop ();
    if (op_error) {
        WARN ("Failed group get for add -- %d:%s", op_error->code,
                op_error->message);
        g_object_unref (grp);
        g_error_free (op_error);
        return;
    }

    _set_group_prop (grp, group);

    if (!gum_group_update (grp, _on_group_op, NULL)) {
        WARN ("Failed group update setup");
        g_object_unref (grp);
        return;
    }
    _run_mainloop ();
    if (op_error) {
        WARN ("Failed group update -- %d:%s", op_error->code,
        		op_error->message);
        g_error_free (op_error);
    } else {
        _print_group_prop (grp);
        DBG ("Group updated successfully");
    }

    g_object_unref (grp);
}

static void
_handle_group_get (
		TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get (group->gid, _on_group_op, NULL);
    if (!grp) return;

    _run_mainloop ();

    if (op_error) {
        WARN ("Failed group get -- %d:%s", op_error->code, op_error->message);
        g_error_free (op_error);
    } else {
        DBG ("Group retrieved successfully");
        _print_group_prop (grp);
    }

    if (grp) {
    	g_object_unref (grp);
    }
}

static void
_handle_group_get_by_name (
		TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_by_name_sync (group->group_name);
    if (!grp) return;

    DBG ("Group retrieved by name successfully");
    _print_group_prop (grp);

    if (grp) {
    	g_object_unref (grp);
    }
}

static void
_handle_group_add_mem (
        TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get_sync (group->gid);
    if (!grp) return;

    if (!gum_group_add_member_sync (grp, group->mem_uid, TRUE)) {
        WARN ("Failed group addmem setup");
        g_object_unref (grp);
        return;
    }

    DBG ("Group mem added successfully");
    g_object_unref (grp);
}

static void
_handle_group_del_mem (
        TestGroup *group)
{
    GumGroup *grp = NULL;

    grp = gum_group_get (group->gid, _on_group_op, NULL);
    if (!grp) return;

    _run_mainloop ();
    if (op_error) {
        WARN ("Failed group get for delmem -- %d:%s", op_error->code,
                op_error->message);
        g_object_unref (grp);
        g_error_free (op_error);
        return;
    }

    if (!gum_group_delete_member (grp, group->mem_uid, _on_group_op, NULL)) {
        WARN ("Failed group delmem setup");
        g_object_unref (grp);
        return;
    }
    _run_mainloop ();
    if (op_error) {
        WARN ("Failed group delmem -- %d:%s", op_error->code,
        		op_error->message);
        g_error_free (op_error);
    } else {
        DBG ("Group mem deleted successfully");
    }

    g_object_unref (grp);
}

int
main (int argc, char *argv[])
{
   
    GError *error = NULL;
    GOptionContext *context;
    GMainLoop* main_loop = NULL;
    gboolean rval = FALSE;
    
    gboolean is_user_add_op = FALSE, is_user_del_op = FALSE;
    gboolean is_user_up_op = FALSE, is_user_get_op = FALSE;
    gboolean is_user_get_by_name_op = FALSE;
    GOptionGroup* user_option = NULL;
    TestUser *user = NULL;
    
    gboolean is_group_add_op = FALSE, is_group_del_op = FALSE;
    gboolean is_group_up_op = FALSE, is_group_get_op = FALSE;
    gboolean is_group_get_by_name_op = FALSE, is_group_add_mem_op = FALSE;
    gboolean is_group_del_mem_op = FALSE;
    GOptionGroup* group_option = NULL;
    TestGroup *group = NULL;

    user = _create_test_user();
    group = _create_test_group();

    GOptionEntry main_entries[] =
    {
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
    
    context = g_option_context_new (" [Application Option]\n"
            "  e.g. To delete a user, ./gum-example --uid=2001 -v");
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
        g_print ("option parsing failed: %s\n", error->message);
        g_free (user);
        g_free (group);
        exit (1);
    }
    
    _create_mainloop ();
    
    if (is_user_add_op) {
        _handle_user_add (user);
    } else if (is_user_del_op) {
    	_handle_user_del (user);
    } else if (is_user_up_op) {
    	_handle_user_up (user);
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
        WARN ("No option specified");
    }

    if (main_loop) {
        g_main_loop_unref (main_loop);
    }

    _free_test_user (user);
    _free_test_group (group);

    return 0;
}
