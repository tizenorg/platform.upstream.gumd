/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#if defined HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "common/gum-file.h"
#include "common/gum-string-utils.h"
#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

#define GUM_PERM 0777

static FILE *
_open_file (
        const gchar *fn,
        const gchar *mode)
{
    FILE *fp = NULL;
    if (!fn || !(fp = fopen (fn, mode))) {
        if (!fp)
            WARN ("Could not open file '%s', error: %s", fn, strerror(errno));
        return NULL;
    }
    return fp;
}

static gboolean
_copy_file_attributes (
        const gchar *from_path,
        const gchar *to_path)
{
    gboolean ret = TRUE;
    gsize attrs_size = 0;
    struct stat from_stat;

    if (stat (from_path, &from_stat) < 0 ||
        chmod (to_path, from_stat.st_mode) < 0 ||
        chown (to_path, from_stat.st_uid, from_stat.st_gid) < 0) {
        return FALSE;
    }

    /* copy extended attributes */
#if defined(HAVE_LLISTXATTR) && \
    defined(HAVE_LGETXATTR) && \
    defined(HAVE_LSETXATTR)
    attrs_size = llistxattr (from_path, NULL, 0);
    if (attrs_size > 0) {

        gchar *names = g_new0 (gchar, attrs_size + 1);
        if (names && llistxattr (from_path, names, attrs_size) > 0) {

            gchar *name = NULL, *value = NULL;
            gchar *end_names = names + attrs_size;
            names[attrs_size] = '\0';
            gsize size = 0;

            for (name = names; name != end_names; name =strchr (name,'\0')+1) {

                if (name && (size = lgetxattr (from_path, name, NULL, 0)) > 0 &&
                        (value = g_realloc (value, size)) &&
                        lgetxattr (from_path, name, value, size) > 0) {

                    if (lsetxattr (to_path, name, value, size, 0) != 0) {
                        ret = FALSE;
                        break;
                    }
                }
            }
            g_free (value);
        }
        g_free (names);
    }
#endif

    return ret;
}

gboolean
gum_file_open_db_files (
        const gchar *origfn,
        const gchar *newfn,
        FILE **origf,
        FILE **newf,
        GError **error)
{
    if (!origfn || !origf || !(*origf = _open_file (origfn, "r"))) {
        DBG("origfn %s --- orig %p", origfn ? origfn : "NULL", origf);
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_OPEN, "Unable to open orig file",
                error, FALSE);
    }

    if (!newfn || !newf || !(*newf = _open_file (newfn, "w+"))) {
        if (*origf) fclose (*origf);
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_OPEN, "Unable to open new file",
                error, FALSE);
    }

    if (!_copy_file_attributes (origfn, newfn)) {
        if (*origf) fclose (*origf);
        if (*newf) fclose (*newf);
        g_unlink (newfn);
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_ATTRIBUTE,
                "Unable to get/set file attributes", error, FALSE);
    }

    return TRUE;
}

gboolean
gum_file_close_db_files (
        const gchar *origfn,
        const gchar *newfn,
        FILE *origf,
        FILE *newf,
        GError **error)
{
    gboolean retval = TRUE;
    gchar *old_file = NULL;

    if (!newf ||
        !newfn ||
        fflush (newf) != 0 ||
        fsync (fileno (newf)) != 0 ||
        fclose (newf) != 0) {
        GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write failure", error, retval,
                FALSE);
        goto _close_new;
    }
    newf = NULL;

    /* Move original file to old file and new file as updated file */
    old_file = g_strdup_printf ("%s.old", origfn);
    if (!old_file) {
        GUM_SET_ERROR (GUM_ERROR_FILE_MOVE, "Unable to create old file", error,
                retval, FALSE);
        goto _close_new;
    }

    g_unlink (old_file);
    if (link (origfn, old_file) != 0 ||
        g_rename (newfn, origfn) != 0) {
        GUM_SET_ERROR (GUM_ERROR_FILE_MOVE, "Unable to move file", error, retval,
                FALSE);
    }

_close_new:
    if (newf) fclose (newf);
    g_unlink (newfn);

    if (origf) fclose (origf);

    g_free (old_file);

    return retval;
}

gboolean
gum_file_update (
        GObject *object,
        GumOpType op,
        GumFileUpdateFunc update_func,
        const gchar *origfn,
        gpointer user_data,
        GError **error)
{
    /* open files
     * read (current) file
     ** write 1-by-1 line to new file along with the
     ** modification (add/remove/modify)
     * rename files and move original file to old file
     * close files
     * */
    gboolean retval = TRUE;
    FILE *origf = NULL, *newf = NULL;
    gchar *newfn = NULL;

    newfn = g_strdup_printf ("%s-tmp.%lu", origfn, (unsigned long)getpid ());
    retval = gum_file_open_db_files (origfn, newfn, &origf, &newf, error);
    if (!retval) goto _finished;

    /* Update, sync and close file */
    if (!update_func) {
        GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write function not specified",
                error, retval, FALSE);
        goto _close;
    }

    retval = (*update_func) (object, op, origf, newf, user_data, error);
    if (!retval) {
        goto _close;
    }

    retval = gum_file_close_db_files (origfn, newfn, origf, newf, error);

_close:
    if (!retval) {
        if (newf) fclose (newf);
        g_unlink (newfn);
        if (origf) fclose (origf);
    }
_finished:
    g_free (newfn);

    return retval;
}

struct passwd *
gum_file_getpwnam (
        const gchar *usrname,
        GumConfig *config)
{
    struct passwd *pent = NULL;
    FILE *fp = NULL;

    if (!usrname || !config) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), "r"))) {
        return NULL;
    }
    while ((pent = fgetpwent (fp)) != NULL) {
        if(g_strcmp0 (usrname, pent->pw_name) == 0)
            break;
        pent = NULL;
    }
    fclose (fp);

    return pent;
}

struct passwd *
gum_file_getpwuid (
        uid_t uid,
        GumConfig *config)
{
    struct passwd *pent = NULL;
    FILE *fp = NULL;

    if (!config) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), "r"))) {
        return NULL;
    }

    while ((pent = fgetpwent (fp)) != NULL) {
        if(uid == pent->pw_uid)
            break;
        pent = NULL;
    }
    fclose (fp);

    return pent;
}

struct passwd *
gum_file_find_user_by_gid (
        gid_t gid,
        GumConfig *config)
{
    struct passwd *pent = NULL;
    FILE *fp = NULL;

    if (!config || gid == GUM_GROUP_INVALID_GID) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), "r"))) {
        return NULL;
    }

    while ((pent = fgetpwent (fp)) != NULL) {
        if(gid == pent->pw_gid)
            break;
        pent = NULL;
    }
    fclose (fp);

    return pent;
}

struct spwd *
gum_file_getspnam (
        const gchar *usrname,
        GumConfig *config)
{
    struct spwd *spent = NULL;
    FILE *fp = NULL;

    if (!usrname || !config) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_SHADOW_FILE), "r"))) {
        return NULL;
    }

    while ((spent = fgetspent (fp)) != NULL) {
        if(g_strcmp0 (usrname, spent->sp_namp) == 0)
            break;
        spent = NULL;
    }
    fclose (fp);

    return spent;
}

struct group *
gum_file_getgrnam (
        const gchar *grname,
        GumConfig *config)
{
    struct group *gent = NULL;
    FILE *fp = NULL;

    if (!grname || !config) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE), "r"))) {
        return NULL;
    }
    while ((gent = fgetgrent (fp)) != NULL) {
        if(g_strcmp0 (grname, gent->gr_name) == 0)
            break;
        gent = NULL;
    }
    fclose (fp);

    return gent;
}

struct group *
gum_file_getgrgid (
        gid_t gid,
        GumConfig *config)
{
    struct group *gent = NULL;
    FILE *fp = NULL;

    if (!config) {
        return NULL;
    }

    if (!(fp = _open_file (gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE), "r"))) {
        return NULL;
    }
    while ((gent = fgetgrent (fp)) != NULL) {
        if(gid == gent->gr_gid)
            break;
        gent = NULL;
    }
    fclose (fp);

    return gent;
}

struct sgrp *
gum_file_getsgnam (
        const gchar *grpname,
        GumConfig *config)
{
    struct sgrp *sgent = NULL;
    FILE *fp = NULL;
    const gchar *shadow_file = NULL;

    if (!grpname || !config) {
        return NULL;
    }

    shadow_file = gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (!g_file_test (shadow_file, G_FILE_TEST_EXISTS) ||
        !(fp = _open_file (shadow_file, "r"))) {
        return NULL;
    }

    while ((sgent = fgetsgent (fp)) != NULL) {
        if(g_strcmp0 (grpname, sgent->sg_namp) == 0)
            break;
        sgent = NULL;
    }
    fclose (fp);

    return sgent;
}

GFile *
gum_file_new_path (
		const gchar *dir,
		const gchar *fname)
{
	GFile *file = NULL;
	gchar *fn = NULL;

	if (!dir || !fname) {
		return NULL;
	}

	fn = g_build_filename (dir, fname, NULL);
	if (fn) {
		file = g_file_new_for_path (fn);
		g_free (fn);
	}

	return file;
}

static gboolean
_copy_dir_recursively (
        GumConfig *config,
        const gchar *src,
        const gchar *dest,
        uid_t uid,
        gid_t gid,
        GError **error)
{
    gboolean retval = TRUE;
    gboolean stop = FALSE;
    const gchar *src_fname = NULL;
    gchar *src_filepath = NULL, *dest_filepath = NULL;
    GDir *src_dir = NULL;
    struct stat stat_entry;

    if (!src || !dest) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_COPY_FAILURE,
                "Invalid directory path(s)", error, FALSE);
    }

    DBG ("copy directory %s -> %s", src, dest);
    src_dir = g_dir_open (src, 0, NULL);
    if (!src_dir) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_COPY_FAILURE,
                "Invalid source directory path", error, FALSE);
    }

    while ((src_fname = g_dir_read_name (src_dir))) {
        GError *err = NULL;
        GFile *src_file = NULL, *dest_file = NULL;

        src_filepath = g_build_filename (src, src_fname, NULL);
        stop = (lstat(src_filepath, &stat_entry) != 0);
        if (stop) goto _free_data;

        dest_filepath = g_build_filename (dest, src_fname, NULL);
        src_file = g_file_new_for_path (src_filepath);
        dest_file = g_file_new_for_path (dest_filepath);

        if (S_ISDIR (stat_entry.st_mode)) {
            DBG ("copy directory %s", src_filepath);
            gint mode = GUM_PERM & ~gum_config_get_uint (config,
                        GUM_CONFIG_GENERAL_UMASK, GUM_UMASK);
            g_mkdir_with_parents (dest_filepath, mode);
            stop = !_copy_dir_recursively (config, src_filepath, dest_filepath,
                    uid, gid, NULL);
        } else {
            DBG ("copy file %s", src_filepath);
            if (!g_file_copy (src_file, dest_file,
                    G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL,
                    NULL, NULL, &err)) {
                WARN("File copy failure error %d:%s", err ? err->code : 0,
                        err ? err->message : "");
                if (err) g_error_free (err);
                stop = TRUE;
                goto _free_data;
            }
        }
#ifndef ENABLE_TESTS
        if (!stop) stop = (chown (dest_filepath, uid, gid) < 0);
#endif

_free_data:
        g_free (src_filepath);
        g_free (dest_filepath);
        GUM_OBJECT_UNREF (src_file);
        GUM_OBJECT_UNREF (dest_file);
        if (stop) {
            GUM_SET_ERROR (GUM_ERROR_HOME_DIR_COPY_FAILURE,
                    "Home directory copy failure", error, retval, FALSE);
            break;
        }
    }

    g_dir_close (src_dir);
    return retval;
}

gboolean
gum_file_create_home_dir (
        GumConfig *config,
        const gchar *usr_home_dir,
        uid_t uid,
        gid_t gid,
        GError **error)
{
	gboolean retval = TRUE;
	gint mode = GUM_PERM & ~gum_config_get_uint (config,
			GUM_CONFIG_GENERAL_UMASK, GUM_UMASK);

    if (!usr_home_dir) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_CREATE_FAILURE,
                "Invalid home directory path", error, FALSE);
    }

    if (g_access (usr_home_dir, F_OK) != 0) {

        if (!g_file_test (usr_home_dir, G_FILE_TEST_EXISTS)) {
            g_mkdir_with_parents (usr_home_dir, mode);
        }

        if (!g_file_test (usr_home_dir, G_FILE_TEST_IS_DIR)) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_CREATE_FAILURE,
                    "Home directory creation failure", error, FALSE);
        }

        /* when run in test mode, user may not exist */
#ifndef ENABLE_TESTS
		if (chown (usr_home_dir, uid, gid) < 0) {
			GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_CREATE_FAILURE,
					"Home directory chown failure", error, FALSE);
		}
#endif

		retval = _copy_dir_recursively (config, gum_config_get_string (config,
                GUM_CONFIG_GENERAL_SKEL_DIR), usr_home_dir, uid, gid,
		        error);
	}

	return retval;
}

gboolean
gum_file_delete_home_dir (
        const gchar *dir,
        GError **error)
{
    GDir* gdir = NULL;
    struct stat sent;

    if (!dir || !(gdir = g_dir_open(dir, 0, NULL))) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_DELETE_FAILURE,
                "Invalid home directory path", error, FALSE);
    }

    const gchar *fname = NULL;
    gint retval = 0;
    gchar *filepath = NULL;
    while ((fname = g_dir_read_name (gdir)) != NULL) {
        if (g_strcmp0 (fname, ".") == 0 ||
            g_strcmp0 (fname, "..") == 0) {
            continue;
        }
        retval = -1;
        filepath = g_build_filename (dir, fname, NULL);
        if (filepath) {
            retval = lstat(filepath, &sent);
            if (retval == 0) {
                /* recurse the directory */
                if (S_ISDIR (sent.st_mode)) {
                    retval = (gint)!gum_file_delete_home_dir (filepath, error);
                } else {
                    retval = g_remove (filepath);
                }
            }
            g_free (filepath);
        }
        if (retval != 0) {
            g_dir_close (gdir);
            GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_DELETE_FAILURE,
                "Unable to delete files in the directory", error, FALSE);
        }
    }
    g_dir_close (gdir);

    if (g_remove (dir) != 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_HOME_DIR_DELETE_FAILURE,
                "Unable to delete home directory", error, FALSE);
    }

	return TRUE;
}
