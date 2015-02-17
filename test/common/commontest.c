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
 * This library is distributed in the hope that it will be useful,
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
#include <utmp.h>
#include <glib.h>
#include <glib-object.h>
#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/gum-config.h"
#include "common/gum-error.h"
#include "common/gum-log.h"
#include "common/gum-file.h"
#include "common/gum-crypt.h"
#include "common/gum-validate.h"
#include "common/gum-user-types.h"
#include "common/gum-lock.h"
#include "common/gum-string-utils.h"
#include "common/gum-defines.h"
#include "common/gum-dictionary.h"

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
_setup (void)
{
    gchar *fpath = NULL;
    gchar *cmd = NULL;

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
    cmd = g_strdup_printf ("cp -r %s/ /tmp/gum/", fpath);
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
_terminate (void)
{
    fail_if (system("rm -rf /tmp/gum") == -1);
	g_unsetenv ("UM_SKEL_DIR");
	g_unsetenv ("UM_HOMEDIR_PREFIX");
    g_unsetenv ("UM_PASSWD_FILE");
    g_unsetenv ("UM_SHADOW_FILE");
    g_unsetenv ("UM_GROUP_FILE");
    g_unsetenv ("UM_GSHADOW_FILE");
    g_unsetenv ("UM_CONF_FILE");
}

START_TEST (test_config)
{
    DBG("");

    GumConfig* config = gum_config_new (NULL);
    fail_if(config == NULL);
    fail_if (gum_config_get_int (config,
            GUM_CONFIG_DBUS_DAEMON_TIMEOUT, 0) != 0);
    g_object_unref(config);

    fail_if (g_setenv ("UM_CONF_FILE", GUM_TEST_DATA_DIR, TRUE) == FALSE);
    fail_if (g_setenv ("UM_DAEMON_TIMEOUT", "6", TRUE) == FALSE);

    config = gum_config_new (NULL);
    fail_if(config == NULL);
    fail_if (gum_config_get_int (config,
            GUM_CONFIG_DBUS_DAEMON_TIMEOUT, -1) != 6);
    g_object_unref(config);
    g_unsetenv ("UM_DAEMON_TIMEOUT");

    config = gum_config_new (NULL);
    fail_if(config == NULL);
    fail_if (gum_config_get_int (config,
            GUM_CONFIG_DBUS_DAEMON_TIMEOUT, -1) != 7);

    fail_if (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_DEF_USR_GROUPS) == NULL);

    fail_if (g_strcmp0 (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_DEF_USR_GROUPS), "") != 0);

    g_unsetenv ("UM_CONF_FILE");

    g_object_unref (config);
}
END_TEST

START_TEST (test_lock)
{
    DBG("");
	fail_unless (gum_lock_pwdf_unlock () == FALSE);
	fail_unless (gum_lock_pwdf_lock () == TRUE);
	fail_unless (gum_lock_pwdf_unlock () == TRUE);

	fail_unless (gum_lock_pwdf_lock () == TRUE);
	fail_unless (gum_lock_pwdf_lock () == TRUE);
	fail_unless (gum_lock_pwdf_lock () == TRUE);
	fail_unless (gum_lock_pwdf_unlock () == TRUE);
	fail_unless (gum_lock_pwdf_unlock () == TRUE);
	fail_unless (gum_lock_pwdf_unlock () == TRUE);
	fail_unless (gum_lock_pwdf_unlock () == FALSE);
}
END_TEST

START_TEST (test_string)
{
    DBG("");
    gchar **strv = NULL, **strv2 = NULL;
    gchar *str = NULL, *str1 = NULL;

    fail_unless (gum_string_utils_search_string (NULL, NULL, "cde") == FALSE);
    fail_unless (gum_string_utils_search_string ("", ",", NULL) == FALSE);
    fail_unless (gum_string_utils_search_string ("", ",", "cde") == FALSE);
    fail_unless (gum_string_utils_search_string ("test", ",", "cde") == FALSE);
    fail_unless (gum_string_utils_search_string ("test", ",", "test1")
            == FALSE);
    fail_unless (gum_string_utils_search_string ("test,123,testtt", ",",
            "test1") == FALSE);
    fail_unless (gum_string_utils_search_string ("test1,123,testtt", ",",
            "test1") == TRUE);
    fail_unless (gum_string_utils_search_string ("test,test1,testtt", ",",
            "test1") == TRUE);
    fail_unless (gum_string_utils_search_string ("test,123test,test1", ",",
            "test1") == TRUE);

    strv = g_malloc0 (sizeof (gchar *) * 3);
    strv[0] = g_strdup ("str1");
    strv[1] = g_strdup ("str2");
    strv[2] = NULL;
    fail_unless (gum_string_utils_search_stringv (NULL, NULL) == FALSE);
    fail_unless (gum_string_utils_search_stringv (NULL, "cde") == FALSE);
    fail_unless (gum_string_utils_search_stringv (strv, NULL) == FALSE);
    fail_unless (gum_string_utils_search_stringv (strv, "") == FALSE);
    fail_unless (gum_string_utils_search_stringv (strv, "12") == FALSE);
    fail_unless (gum_string_utils_search_stringv (strv, "str12") == FALSE);
    fail_unless (gum_string_utils_search_stringv (strv, "str1") == TRUE);
    fail_unless (gum_string_utils_search_stringv (strv, "str2") == TRUE);

    fail_unless (gum_string_utils_delete_string (strv, "str3") == NULL);
    strv2 = gum_string_utils_delete_string (strv, "str2");
    fail_unless (strv2 != NULL);
    g_strfreev (strv2);

    strv2 = gum_string_utils_append_string (strv, "str3");
    fail_unless (strv2 != NULL);
    fail_unless (gum_string_utils_search_stringv (strv2, "str3") == TRUE);
    g_strfreev (strv2);

    g_strfreev (strv);

    fail_unless (gum_string_utils_get_string (NULL, NULL, 0) == NULL);
    fail_unless (gum_string_utils_get_string ("stttt", NULL, 0) == NULL);
    fail_unless (gum_string_utils_get_string ("string", ",", 1) == NULL);

    str = gum_string_utils_get_string ("string", ",", 0);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "string") == 0);
    g_free (str);

    str = gum_string_utils_get_string ("str0,str1,str r2", ",", 1);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "str1") == 0);
    g_free (str);

    str = gum_string_utils_get_string ("str0,str1,str r2", ",", 2);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "str r2") == 0);
    g_free (str);

    fail_unless (gum_string_utils_get_string ("str0,str1,str r2", ":", 1)
            == NULL);

    fail_unless (gum_string_utils_insert_string (NULL, NULL, "istr1", 0, 4)
            == NULL);
    str = gum_string_utils_insert_string (":str0,str1", ":", "istr1",
            0, 4);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "istr1:str0,str1::") == 0);
    g_free (str);

    fail_unless (gum_string_utils_insert_string ("str0", NULL, "istr1", 0, 4)
            == NULL);
    fail_unless (gum_string_utils_insert_string ("str0,str1", NULL, "istr1",
            0, 4) == NULL);
    str = gum_string_utils_insert_string ("str0,str1", ",", "istr1", 0, 4);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "istr1,str1,,") == 0);
    g_free (str);

    str = gum_string_utils_insert_string ("str0", ",", "istr1", 2, 4);
    fail_unless (str != NULL);
    fail_unless (g_strcmp0 (str, "str0,,istr1,") == 0);
    str1 = gum_string_utils_get_string (str, ",", 1);
    fail_unless (str1 != NULL);
    fail_unless (g_strcmp0 (str1, "") == 0);
    g_free (str1);
    str1 = gum_string_utils_get_string (str, ",", 2);
    fail_unless (str1 != NULL);
    fail_unless (g_strcmp0 (str1, "istr1") == 0);
    g_free (str1);

    g_free (str);
}
END_TEST


static gboolean
_update_file_entries (
        GObject *self,
        GumOpType op,
        FILE *origf,
        FILE *newf,
        gpointer user_data,
        GError **error)
{
    #define BLOCKSIZE 1024*64
    gchar buffer[BLOCKSIZE], *p = NULL;

    /* simply copy orig to new file as it is for testing */
    while (1) {
        gint n;
        gint n_read = 0;
        p = buffer;
        do {
            n = fread (p, 1, BLOCKSIZE - n_read, origf);
            p += n;
            n_read += n;
        } while (n_read < BLOCKSIZE && n != 0);
        if (n == 0 && ferror (origf))
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Unable to write file",
                    error, FALSE);

        p = buffer;
        size_t n_written = 0;
        while (n_read > 0) {
            n_written = fwrite (p, 1, n_read, newf);
            if (n_written == -1)
                GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
                        "Unable to write file", error, FALSE);

            p += n_written;
            n_read -= n_written;
        }
        if (n == 0)
            break;
    }
    return TRUE;
}

START_TEST (test_file)
{
    DBG("");
    GError *error = NULL;
    const gchar *origfn = NULL;
    struct stat origst, oldst, st;
    gchar *oldfn = NULL;
    GFile *file = NULL;
    gchar *hdir = NULL;
    struct stat sb;
    uid_t uid;
    gid_t gid;
    GObject* user = NULL;

    GumConfig* config = gum_config_new (NULL);
    fail_if(config == NULL);

    origfn = gum_config_get_string (config, GUM_CONFIG_GENERAL_PASSWD_FILE);
    fail_if(origfn == NULL);
    fail_if (stat (origfn, &origst) < 0);

    fail_unless (gum_file_update (user, GUM_OPTYPE_ADD,
            (GumFileUpdateCB)_update_file_entries, origfn, NULL,
            &error) == TRUE);
    fail_unless (error == NULL);

    oldfn = g_strdup_printf ("%s.old", origfn);
    fail_if (stat (oldfn, &oldst) < 0);
    g_free (oldfn);
    fail_if (oldst.st_size != origst.st_size && oldst.st_mode !=origst.st_mode);
    fail_if (oldst.st_uid != origst.st_uid && oldst.st_gid != origst.st_gid);

    fail_if (stat (origfn, &origst) < 0);
    fail_if (oldst.st_size != origst.st_size && oldst.st_mode !=origst.st_mode);
    fail_if (oldst.st_uid != origst.st_uid && oldst.st_gid != origst.st_gid);

    FILE *o = NULL,*n = NULL;
    fail_if (system("rm -rf /tmp/gum/test*") != 0);
    fail_if (system("touch /tmp/gum/testo") != 0);
    fail_unless (gum_file_open_db_files ("/tmp/gum/testo", "/tmp/gum/testn",
            &o, &n, &error) == TRUE);
    fail_unless (error == NULL);
    fail_if (stat ("/tmp/gum/testo", &st) < 0);
    fail_if (stat ("/tmp/gum/testn", &st) < 0);
    fail_unless (gum_file_close_db_files ("/tmp/gum/testo", "/tmp/gum/testn",
                o, n, &error) == TRUE);
    fail_unless (error == NULL);
    fail_if (system("rm -rf /tmp/gum/test*") != 0);
    fail_unless (stat ("/tmp/gum/testo", &st) < 0);
    fail_unless (stat ("/tmp/gum/testn", &st) < 0);

    fail_unless (gum_file_getpwnam (NULL, NULL) == NULL);
    fail_unless (gum_file_getpwnam ("", NULL) == NULL);
    fail_unless (gum_file_getpwnam ("", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE)) == NULL);
    fail_unless (gum_file_getpwnam ("test121", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE)) == NULL);
    fail_unless (gum_file_getpwnam ("root", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE)) != NULL);

    fail_unless (gum_file_getpwuid (1, NULL) == NULL);
    fail_unless (gum_file_getpwuid (0, gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE)) != NULL);

    fail_unless (gum_file_find_user_by_gid (GUM_GROUP_INVALID_GID, NULL)
            == NULL);
    fail_unless (gum_file_find_user_by_gid (0, gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE)) != NULL);

    fail_unless (gum_file_getspnam (NULL,NULL) == NULL);
    fail_unless (gum_file_getspnam ("", NULL) == NULL);
    fail_unless (gum_file_getspnam ("", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_SHADOW_FILE)) == NULL);
    fail_unless (gum_file_getspnam ("test121", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_SHADOW_FILE)) == NULL);
    fail_unless (gum_file_getspnam ("root", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_SHADOW_FILE)) != NULL);

    fail_unless (gum_file_getgrnam (NULL,NULL) == NULL);
    fail_unless (gum_file_getgrnam ("", NULL) == NULL);
    fail_unless (gum_file_getgrnam ("", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE)) == NULL);
    fail_unless (gum_file_getgrnam ("test121", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE)) == NULL);
    fail_unless (gum_file_getgrnam ("root", gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE)) != NULL);

    fail_unless (gum_file_getgrgid (1, NULL) == NULL);
    fail_unless (gum_file_getgrgid (0, gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE)) != NULL);

    if (g_file_test (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE), G_FILE_TEST_EXISTS)) {
        fail_unless (gum_file_getsgnam (NULL,NULL) == NULL);
        fail_unless (gum_file_getsgnam ("", NULL) == NULL);
        fail_unless (gum_file_getsgnam ("", gum_config_get_string (config,
                GUM_CONFIG_GENERAL_GSHADOW_FILE)) == NULL);
        fail_unless (gum_file_getsgnam ("test121", gum_config_get_string (
                config, GUM_CONFIG_GENERAL_GSHADOW_FILE)) == NULL);
        fail_unless (gum_file_getsgnam ("root", gum_config_get_string (config,
                GUM_CONFIG_GENERAL_GSHADOW_FILE)) != NULL);
    }

    fail_unless (gum_file_new_path (NULL, NULL) == NULL);
    fail_unless (gum_file_new_path (NULL,"test123") == NULL);
    file = gum_file_new_path ("/tmp", "test1232");
    fail_unless (file != NULL);
    g_object_unref (file);

    uid = getuid ();
    gid = getgid ();
    hdir = g_build_filename (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_HOME_DIR_PREF), "test_daemon_user_file", NULL);
    fail_unless (gum_file_create_home_dir (hdir, uid, gid,
            gum_config_get_uint (config, GUM_CONFIG_GENERAL_UMASK, GUM_UMASK),
            &error) == TRUE);
    fail_unless (error == NULL);
    fail_unless (stat (hdir, &sb) == 0);
    fail_unless (uid == sb.st_uid && gid == sb.st_gid);

    fail_unless (gum_file_delete_home_dir (hdir, &error) == TRUE);
    fail_unless (error == NULL);
    fail_unless (stat (hdir, &sb) != 0);
    g_free (hdir);

    fail_unless (gum_file_delete_home_dir (NULL, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_HOME_DIR_DELETE_FAILURE);
    g_error_free (error);

    fail_unless (gum_file_create_home_dir (NULL, uid, gid,
            gum_config_get_uint (config, GUM_CONFIG_GENERAL_UMASK, GUM_UMASK),
            &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_HOME_DIR_CREATE_FAILURE);
    g_error_free (error);

    g_object_unref (config);
}
END_TEST

START_TEST (test_validate)
{
    DBG("");
    GError *error = NULL;
    gchar *name = NULL;

    /*gum_validate_name*/
    fail_unless (gum_validate_name ("abcde1234", &error) == TRUE);
    fail_unless (error == NULL);

    fail_unless (gum_validate_name ("A123abcde", &error) == TRUE);
    fail_unless (error == NULL);

    fail_unless (gum_validate_name ("a.bcde$", &error) == TRUE);
    fail_unless (error == NULL);

    fail_unless (gum_validate_name ("a__bcde$", &error) == TRUE);
    fail_unless (error == NULL);

    fail_unless (gum_validate_name ("A-b-0-2-_.4a", &error) == TRUE);
    fail_unless (error == NULL);

    fail_unless (gum_validate_name (NULL, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("Aa?", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("Ab0$abcde", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("1abcde", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("2ABC-", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    fail_unless (gum_validate_name ("12ABC._", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error);

    /*gum_validate_generate_daemon_username*/
    name = gum_validate_generate_username ("A B", &error);
    fail_unless (name != NULL && strlen (name) == UT_NAMESIZE);
    g_free (name);

    name = gum_validate_generate_username ("123AadfB", &error);
    fail_unless (name != NULL && strlen (name) == UT_NAMESIZE);
    g_free (name);

    name = gum_validate_generate_username (",-&#¤?+AB", &error);
    fail_unless (name != NULL && strlen (name) == UT_NAMESIZE);
    g_free (name);

    fail_unless (gum_validate_generate_username (NULL, &error) == NULL);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_generate_username ("", &error) == NULL);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_NAME);
    g_error_free (error); error = NULL;

    /*gum_validate_db_string_entry_regx*/
    fail_unless (gum_validate_db_string_entry_regx (NULL, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("abc de", &error) == TRUE);
    fail_unless (gum_validate_db_string_entry_regx ("123 adfa 121",
            &error) == TRUE);
    fail_unless (gum_validate_db_string_entry_regx ("123 adfa 121\x7E",
            &error) == TRUE);
    fail_unless (gum_validate_db_string_entry_regx ("ddfd\rasdf\fadsfa",
            &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("aaa\f", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("\nddd", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("a\tdfd", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("ad\x7Fjjj", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("ad:jjj", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx (":adjjj", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("adjjj:", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("adj,jj", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry_regx ("adjjj,", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    /*gum_validate_db_string_entry*/
    fail_unless (gum_validate_db_string_entry (NULL, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("", &error) == TRUE);

    fail_unless (gum_validate_db_string_entry ("abc de", &error) == TRUE);
    fail_unless (gum_validate_db_string_entry ("123 adfa 121", &error)
            == TRUE);
    fail_unless (gum_validate_db_string_entry ("123 adfa 121\x7E", &error)
            == TRUE);
    fail_unless (gum_validate_db_string_entry ("ddfd\rasdf\fadsfa", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("aaa\f", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("\nddd", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("a\tdfd", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("ad\x7Fjjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("ad:jjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry (":adjjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("adjjj:", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("adj,jj", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_string_entry ("adjjj,", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_STR);
    g_error_free (error); error = NULL;

    /*gum_validate_db_secret_entry*/
    fail_unless (gum_validate_db_secret_entry (NULL, &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("abc de", &error) == TRUE);
    fail_unless (gum_validate_db_secret_entry ("123 adfa 121", &error)
            == TRUE);
    fail_unless (gum_validate_db_secret_entry ("123 adfa 121\x7E", &error)
            == TRUE);
    fail_unless (gum_validate_db_secret_entry ("ddfd\rasdf\fadsfa", &error)
            == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("aaa\f", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("\nddd", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("a\tdfd", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("ad\x7Fjjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("ad:jjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry (":adjjj", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("adjjj:", &error) == FALSE);
    fail_unless (error != NULL);
    fail_unless (error->code == GUM_ERROR_INVALID_SECRET);
    g_error_free (error); error = NULL;

    fail_unless (gum_validate_db_secret_entry ("adj,jj", &error) == TRUE);
    fail_unless (gum_validate_db_secret_entry ("adjjj,", &error) == TRUE);
}
END_TEST

START_TEST (test_crypt)
{
    DBG("");
    gchar *pass = NULL;

    pass = gum_crypt_encrypt_secret("", "DES");
    fail_if (pass == NULL);
    fail_if (strlen (pass) != 13); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("pass123", "DES");
    fail_if (pass == NULL);
    fail_if (strlen (pass) != 13); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("pas¤$s123", "MD5");
    fail_if (pass == NULL);
    fail_unless (strlen (pass) >= (3+9+22)); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("pass{123", "SHA256");
    fail_if (pass == NULL);
    fail_unless (strlen (pass) >= (3+9+43)); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("pas.-s123", "SHA512");
    fail_if (pass == NULL);
    fail_unless (strlen (pass) >= (3+9+86)); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("", "SHA512");
    fail_if (pass == NULL);
    fail_unless (strlen (pass) >= (3+9+86)); /*crypt(3)*/
    g_free (pass);

    pass = gum_crypt_encrypt_secret("pass ?()#123", "SHA512");
    fail_if (pass == NULL);
    fail_unless (strlen (pass) >= (3+9+86)); /*crypt(3)*/

    fail_unless(gum_crypt_cmp_secret("pass ?()#123", pass) == 0);
    fail_unless(gum_crypt_cmp_secret("pa1ss ?()#123", pass) != 0);
    g_free (pass);

}
END_TEST

START_TEST (test_error)
{
    DBG("");

    GVariant *var = NULL;
    GError *err = GUM_GET_ERROR_FOR_ID (GUM_ERROR_INVALID_INPUT, "testerror");
    fail_if (err == NULL);

    var = gum_error_to_variant (err);
    fail_if (var == NULL);

    g_error_free (err); err = NULL;
    err = gum_error_new_from_variant (var);

    fail_if (err == NULL);
    fail_if (err->code != GUM_ERROR_INVALID_INPUT);
    fail_if (g_strcmp0 (err->message, "testerror") != 0);

    g_error_free (err);
    g_variant_unref (var);
}
END_TEST

START_TEST (test_dictionary)
{
    DBG("");
    gboolean data1 = FALSE;
    GVariant *var = NULL;
    gint64 data2 = 123;
    guint64 data3 = 789;

    GumDictionary* dict = gum_dictionary_new ();
    fail_if (dict == NULL);
    fail_if (gum_dictionary_get_boolean (dict,
            "key1", &data1) == TRUE);

    fail_if (gum_dictionary_set_boolean (dict,
            "key1", data1) == FALSE);
    data1 = TRUE;
    fail_if (gum_dictionary_get_boolean (dict,
            "key1", &data1) == FALSE);
    fail_if (data1 == TRUE);
    fail_if (gum_dictionary_get_boolean (dict,
            "key1", &data1) == FALSE);

    fail_if (gum_dictionary_set_int64 (dict,
            "key2", data2) == FALSE);
    data2 = 456;
    fail_if (gum_dictionary_set_uint64 (dict,
            "key3", data3) == FALSE);

    var = gum_dictionary_to_variant (dict);
    fail_if (var == NULL);
    g_hash_table_unref (dict);

    dict = gum_dictionary_new_from_variant (var);
    fail_if (dict == NULL);

    fail_if (gum_dictionary_get_int64 (dict,
            "key2", &data2) == FALSE);
    fail_if (data2 != 123);
    fail_if (gum_dictionary_get_uint64 (dict,
            "key3", &data3) == FALSE);

    g_variant_unref (var);
    g_hash_table_unref (dict);
}
END_TEST

START_TEST (test_usertype)
{
    DBG("");
    gchar** strv = NULL;
    gchar* str = NULL;

    fail_if (gum_user_type_from_string (NULL) != GUM_USERTYPE_NONE);
    fail_if (gum_user_type_from_string ("") != GUM_USERTYPE_NONE);

    fail_if (gum_user_type_from_string ("abcde") != GUM_USERTYPE_NONE);
    fail_if (gum_user_type_from_string ("system") != GUM_USERTYPE_SYSTEM);
    fail_if (gum_user_type_from_string ("admin") != GUM_USERTYPE_ADMIN);
    fail_if (gum_user_type_from_string ("guest") != GUM_USERTYPE_GUEST);
    fail_if (gum_user_type_from_string ("normal") != GUM_USERTYPE_NORMAL);

    fail_if (gum_user_type_to_string (45) != NULL);
    fail_if (gum_user_type_to_string (0) != NULL);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_NONE),
            "asdf") == 0);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_NONE),
            "none") == 0);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_SYSTEM),
            "system") != 0);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_ADMIN),
            "admin") != 0);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_GUEST),
            "guest") != 0);
    fail_if (g_strcmp0 (gum_user_type_to_string (GUM_USERTYPE_NORMAL),
            "normal") != 0);

    strv = gum_user_type_to_strv (GUM_USERTYPE_NORMAL);
    fail_if (strv == NULL || g_strv_length (strv) != 1);
    str = g_strjoinv (",",strv);
    fail_if (g_strcmp0 (str, "normal") != 0);
    g_free (str); g_strfreev (strv);

    strv = gum_user_type_to_strv (GUM_USERTYPE_NORMAL|GUM_USERTYPE_SYSTEM);
    fail_if (strv == NULL || g_strv_length (strv) != 2);
    str = g_strjoinv (",",strv);
    fail_if (g_strcmp0 (str, "system,normal") != 0);
    g_free (str); g_strfreev (strv);

    strv = gum_user_type_to_strv (16);
    fail_if (strv == NULL || g_strv_length (strv) != 0);
    g_strfreev (strv);

    strv = gum_user_type_to_strv (GUM_USERTYPE_NONE);
    fail_if (strv == NULL || g_strv_length (strv) != 0);
    g_strfreev (strv);

    fail_if (gum_user_type_from_strv (NULL) != GUM_USERTYPE_NONE);

    strv = gum_string_utils_append_string (NULL,"");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv)
            != GUM_USERTYPE_NONE);
    g_strfreev (strv);

    strv = gum_string_utils_append_string (NULL,"normal");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv) !=
            GUM_USERTYPE_NORMAL);
    g_strfreev (strv);

    strv = gum_string_utils_append_string (NULL,"guest");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv) !=
            GUM_USERTYPE_GUEST);
    g_strfreev (strv);

    strv = gum_string_utils_append_string (NULL,"dasdf");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv) !=
            GUM_USERTYPE_NONE);
    g_strfreev (strv);

    strv = gum_string_utils_append_string (NULL,"guest");
    strv = gum_string_utils_append_string (strv,"normal");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv) !=
            (GUM_USERTYPE_NORMAL|GUM_USERTYPE_GUEST));
    g_strfreev (strv);

    strv = gum_string_utils_append_string (NULL,"guest");
    strv = gum_string_utils_append_string (strv,"normalsdafsa");
    fail_if (gum_user_type_from_strv ((const gchar *const *)strv) !=
            GUM_USERTYPE_GUEST);
    g_strfreev (strv);
}
END_TEST

Suite* common_suite (void)
{
    Suite *s = suite_create ("Common library");
    
    /* Core test case */
    TCase *tc_core = tcase_create ("Tests");
    tcase_add_unchecked_fixture (tc_core, _setup, _terminate);

    tcase_add_test (tc_core, test_config);
    tcase_add_test (tc_core, test_lock);
    tcase_add_test (tc_core, test_string);
    tcase_add_test (tc_core, test_file);
    tcase_add_test (tc_core, test_validate);
    tcase_add_test (tc_core, test_crypt);
    tcase_add_test (tc_core, test_error);
    tcase_add_test (tc_core, test_dictionary);
    tcase_add_test (tc_core, test_usertype);
    suite_add_tcase (s, tc_core);
    return s;
}

int main (void)
{
    int number_failed;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    Suite *s = common_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
