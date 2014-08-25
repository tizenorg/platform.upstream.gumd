/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *          Imran Zaman <imran.zaman@intel.com>
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common/gum-utils.h"
#include "common/gum-log.h"

/**
 * SECTION:gum-utils
 * @short_description: Utility functions
 * @title: Gum Utils
 * @include: gum/common/gum-utils.h
 *
 * Utility functions
 */

typedef struct __nonce_ctx_t
{
    gboolean initialized;
    guint32 serial;
    guchar key[32];
    guchar entropy[16];
} _nonce_ctx_t;

static _nonce_ctx_t _nonce_ctx = { 0, };
G_LOCK_DEFINE_STATIC (_nonce_lock);

static gboolean
_init_nonce_gen ()
{
    if (G_LIKELY(_nonce_ctx.initialized))
        return TRUE;

    int fd;

    fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0)
        goto init_exit;
    if (read (fd, _nonce_ctx.key, sizeof (_nonce_ctx.key)) !=
        sizeof (_nonce_ctx.key))
        goto init_close;
    if (read (fd, _nonce_ctx.entropy, sizeof(_nonce_ctx.entropy)) !=
        sizeof (_nonce_ctx.entropy))
        goto init_close;

    _nonce_ctx.serial = 0;

    _nonce_ctx.initialized = TRUE;

init_close:
    close (fd);

init_exit:
    return _nonce_ctx.initialized;
}

/**
 * gum_utils_generate_nonce:
 * @algorithm: the #GChecksumType algorithm
 *
 * Generates nonce based on hashing algorithm as specified in @algorithm
 *
 * Returns: (transfer full): generate nonce if successful, NULL otherwise.
 */
gchar *
gum_utils_generate_nonce (
        GChecksumType algorithm)
{
    GHmac *hmac;
    gchar *nonce = NULL;
    struct timespec ts;

    G_LOCK (_nonce_lock);

    if (G_UNLIKELY (!_init_nonce_gen()))
        goto nonce_exit;

    hmac = g_hmac_new (algorithm, _nonce_ctx.key, sizeof (_nonce_ctx.key));

    g_hmac_update (hmac, _nonce_ctx.entropy, sizeof (_nonce_ctx.entropy));

    _nonce_ctx.serial++;
    g_hmac_update (hmac, (const guchar *) &_nonce_ctx.serial,
            sizeof (_nonce_ctx.serial));

    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
        g_hmac_update (hmac, (const guchar *) &ts, sizeof (ts));

    memset (&ts, 0x00, sizeof(ts));
    nonce = g_strdup (g_hmac_get_string (hmac));
    g_hmac_unref (hmac);

nonce_exit:
    G_UNLOCK (_nonce_lock);

    return nonce;
}

/**
 * gum_utils_drop_privileges:
 *
 * Drops the privileges for the calling process. Effective uid is to real uid.
 *
 */
void
gum_utils_drop_privileges ()
{
    DBG ("Before set: r-uid %d e-uid %d", getuid (), geteuid ());
    if (seteuid (getuid()))
        WARN ("seteuid() failed");
    DBG ("After set: r-uid %d e-uid %d", getuid (), geteuid ());
}

/**
 * gum_utils_gain_privileges:
 *
 * Gains the privileges for the calling process. Effective uid is to 0.
 *
 */
void
gum_utils_gain_privileges ()
{
    DBG ("Before set: r-uid %d e-uid %d", getuid (), geteuid ());
    if (seteuid (0))
        WARN ("seteuid() failed");
    DBG ("After set: r-uid %d e-uid %d", getuid (), geteuid ());
}

static gint
_script_sort (
        gconstpointer a,
        gconstpointer b,
        gpointer data)
{
    (void)data;
    return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

void
_run_script (const gchar *script_dir,
        gchar **args)
{
    gint status;
    gboolean ret = FALSE;
    const gchar *script = args[0];
    GError *error = NULL;

    if (!script ||
        !g_file_test(script, G_FILE_TEST_EXISTS)) {
        DBG ("script file does not exist: %s", script);
        return;
    }

    ret = g_spawn_sync (NULL, args, NULL,
            G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL,
            NULL, NULL, NULL, &status, &error);
    if (!ret) {
        WARN ("g_spawn failed as retval %d and status %d", ret, status);
    }
    if (error) {
        WARN ("script failed with error %s", error->message);
        g_error_free (error);
    }
}

gboolean
_run_scripts (
        const gchar *script_dir,
        gchar **args)
{
    GDir *dir = NULL;
    const gchar *script_fname = NULL;
    gchar *script_filepath = NULL;
    struct stat stat_entry;
    GSequence *scripts = NULL;
    GSequenceIter *scripts_iter = NULL;
    gboolean stop = FALSE;

    DBG ("");

    if (!script_dir ||
        !g_file_test(script_dir, G_FILE_TEST_EXISTS) ||
        !g_file_test (script_dir, G_FILE_TEST_IS_DIR)) {
        DBG ("script dir check failed %s", script_dir);
        g_strfreev (args);
        return FALSE;
    }
    dir = g_dir_open (script_dir, 0, NULL);
    if (!dir) {
        DBG ("unable to open script dir %s", script_dir);
        g_strfreev (args);
        return FALSE;
    }

    scripts = g_sequence_new ((GDestroyNotify) g_free);
    while ((script_fname = g_dir_read_name (dir))) {
        if (g_strcmp0 (script_fname, ".") == 0 ||
            g_strcmp0 (script_fname, "..") == 0) {
            continue;
        }
        script_filepath = g_build_filename (script_dir, script_fname, NULL);
        stop = (lstat(script_filepath, &stat_entry) != 0);
        if (stop) goto _free_data;

        if (!S_ISDIR (stat_entry.st_mode)) {
            DBG ("insert script file %s", script_filepath);
            g_sequence_insert_sorted (scripts,
                                    (gpointer) script_filepath,
                                    _script_sort,
                                    NULL);
            script_filepath = NULL;
        }

_free_data:
        g_free (script_filepath);
        if (stop) {
            WARN ("failure in reading script dir %s", script_dir);
            g_sequence_free (scripts);
            scripts = NULL;
            break;
        }
    }
    g_dir_close (dir);

    if (!scripts) {
        g_strfreev (args);
        return FALSE;
    }

    gchar* tmp = args[0];
    scripts_iter = g_sequence_get_begin_iter (scripts);
    while (!g_sequence_iter_is_end (scripts_iter)) {
        args[0] = (gchar *)g_sequence_get (scripts_iter);
        _run_script (script_dir, args);
        scripts_iter = g_sequence_iter_next (scripts_iter);
    }
    args[0] = tmp;
    g_sequence_free (scripts);
    g_strfreev (args);
    return TRUE;
}

/**
 * gum_utils_run_user_scripts:
 * @script_dir: path to the scripts directory
 * @username: name of the user
 * @uid: uid of the user
 * @gid: gid of the user
 * @homedir: home directory of the user
 *
 * Runs the user scripts in sorted order.
 *
 * Returns: TRUE if successful, FALSE otherwise
 */
gboolean
gum_utils_run_user_scripts (
        const gchar *script_dir,
        const gchar *username,
        uid_t uid,
        gid_t gid,
        const gchar *homedir)
{
    gchar **args = NULL;
    DBG ("");

    if (!username || !homedir) {
        DBG ("script invalid username/homedir for script dir");
        return FALSE;
    }
    args = g_new0 (gchar *, 6);
    args[0] = g_strdup (""); /* script path to be added later on */
    args[1] = g_strdup (username);
    args[2] = g_strdup_printf ("%u", uid);
    args[3] = g_strdup_printf ("%u", gid);
    args[4] = g_strdup (homedir);
    /* ownership of 'args' is transferred to _run_scripts */
    return _run_scripts (script_dir, args);;
}

/**
 * gum_utils_run_group_scripts:
 * @script_dir: path to the scripts directory
 * @groupname: name of the group
 * @gid: gid of the group
 *
 * Runs the group scripts in sorted order.
 *
 * Returns: TRUE if successful, FALSE otherwise
 */
gboolean
gum_utils_run_group_scripts (
        const gchar *script_dir,
        const gchar *groupname,
        gid_t gid)
{
    gchar **args = NULL;
    DBG ("");

    if (!groupname) {
        DBG ("script invalid groupname for script dir");
        return FALSE;
    }
    args = g_new0 (gchar *, 4);
    args[0] = g_strdup (""); /* script path to be added later on */
    args[1] = g_strdup (groupname);
    args[2] = g_strdup_printf ("%u", gid);
    /* ownership of 'args' is transferred to _run_scripts */
    return _run_scripts (script_dir, args);;
}
