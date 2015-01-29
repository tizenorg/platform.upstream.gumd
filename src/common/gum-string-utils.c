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

#include <string.h>

#include "common/gum-string-utils.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

/**
 * SECTION:gum-string-utils
 * @short_description: Utility functions for strings handling
 * @title: Gum String Utils
 * @include: gum/common/gum-string-utils.h
 *
 * String utility functions
 *
 */

/**
 * GUM_STR_FREE:
 * @s: the string to free
 *
 * A helper macro that frees the string after checking if it is not NULL.
 */

/**
 * GUM_STR_DUP:
 * @s: the source string
 * @d: the destination string
 *
 * A helper macro that duplicates source string to destination string after
 * freeing the destination string first if it is not NULL.
 */

/**
 * GUM_STR_FREEV:
 * @s: the string vector to free
 *
 * A helper macro that frees string vector after checking if it is not NULL.
 */

/**
 * GUM_STR_DUPV:
 * @s: the source string vector
 * @d: the destination string vector
 *
 * A helper macro that duplicates source string vector to destination string
 * vector after freeing the destination string vector first if it is not NULL.
 */

/**
 * gum_string_utils_search_string:
 * @strings: (transfer none): concatenated strings in a string placeholder
 * separated by a separator
 * @separator: (transfer none): separator between strings
 * @search_str: (transfer none): string to search
 *
 * Finds the 'search string' in the strings.
 *
 * Returns: TRUE if found, FALSE otherwise.
 */
gboolean
gum_string_utils_search_string (
        const gchar *strings,
        const gchar *separator,
        const gchar *search_str)
{
    gchar **strv = NULL;

    if (!strings || !separator || !search_str) {
        return FALSE;
    }

    strv = g_strsplit (strings, separator, -1);
    if (strv) {
        gboolean retval = gum_string_utils_search_stringv (strv, search_str);
        g_strfreev (strv);
        return retval;
    }

    return FALSE;
}

/**
 * gum_string_utils_search_stringv:
 * @stringv: (transfer none): vector of strings
 * @search_str: (transfer none): string to search
 *
 * Finds the 'search string' in the string vector.
 *
 * Returns: TRUE if found, FALSE otherwise.
 */
gboolean
gum_string_utils_search_stringv (
        gchar **stringv,
        const gchar *search_str)
{
    gint ind = 0;

    if (!stringv || !search_str) {
        return FALSE;
    }

    while (stringv[ind]) {
        if (g_strcmp0 (search_str, stringv[ind]) == 0) {
            return TRUE;
        }
        ind++;
    }

    return FALSE;
}

/**
 * gum_string_utils_get_string:
 * @strings: (transfer none): vector of strings
 * @separator: (transfer none): separator between strings
 * @str_ind: starting from 0, position for the string to fetch
 *
 * Gets the str_ind'th string from the strings.
 *
 * Returns: (transfer full): string if successful, NULL otherwise.
 */
gchar *
gum_string_utils_get_string (
        const gchar *strings,
        const gchar *separator,
        guint str_ind)
{
    gchar **strv = NULL;
    gchar *str = NULL;

    if (!strings || !separator) {
        return NULL;
    }

    strv = g_strsplit (strings, separator, -1);
    if (str_ind < g_strv_length (strv)) {
        str = g_strdup (strv[str_ind]);
    }
    g_strfreev (strv);

    return str;
}

/**
 * gum_string_utils_insert_string:
 * @strings: (transfer none): vector of strings
 * @separator: (transfer none): separator between strings
 * @str_to_insert: (transfer none): string to insert
 * @str_ind: starting from 0, position for the string to insert
 * @total_strings: total number of strings in the strings vector
 *
 * Inserts the string at the desired position and returns the concatenated
 * string
 *
 * Returns: (transfer full): concatenated string vector if successful, NULL
 * otherwise.
 */
gchar *
gum_string_utils_insert_string (
        const gchar *strings,
        const gchar *separator,
        const gchar *str_to_insert,
        guint str_ind,
        guint total_strings)
{
    gchar **src_strv = NULL, **dest_strv = NULL;
    gint dest_ind = 0;
    gint src_len = 0;
    gchar *result_str = NULL;

    if (!separator || !str_to_insert || (str_ind >= total_strings)) {
        return NULL;
    }

    if (strings) {
        src_strv = g_strsplit (strings, separator, -1);
        if (src_strv) src_len = g_strv_length (src_strv);
    }

    dest_strv = g_malloc0 (sizeof (gchar *) * (total_strings+1));
    if (dest_strv) {
        for (dest_ind = 0; dest_ind < total_strings; dest_ind++) {
            if (dest_ind == str_ind) {
                dest_strv[dest_ind] = g_strdup (str_to_insert);
            } else if (dest_ind < src_len) {
                dest_strv[dest_ind] = g_strdup (src_strv[dest_ind]);
            } else {
                dest_strv[dest_ind] = g_strdup ("");
            }
        }
        dest_strv[dest_ind] = NULL;
        result_str = g_strjoinv (separator, dest_strv);
        g_strfreev (dest_strv);
    }
    GUM_STR_FREEV (src_strv);

    return result_str;
}

/**
 * gum_string_utils_delete_string:
 * @src_strv: (transfer none): vector of strings
 * @string: (transfer none): string to delete
 *
 * Deletes the string from the strings' vector.
 *
 * Returns: (transfer full): modified string vector if string found, NULL
 * otherwise.
 */
gchar **
gum_string_utils_delete_string (
        gchar **src_strv,
        const gchar *string)
{
    gchar **dest_strv = NULL;
    gint ind = 0, dind = 0;

    if (!gum_string_utils_search_stringv (src_strv, string)) {
        return dest_strv;
    }

    gint len = g_strv_length (src_strv);

    dest_strv = g_malloc0 (sizeof (gchar *) * (len));
    while (src_strv[ind]) {
        if (g_strcmp0 (src_strv[ind], string) != 0) {
            dest_strv[dind++] = g_strdup (src_strv[ind]);
        }
        ind++;
    }
    dest_strv[dind] = NULL;

    return dest_strv;
}

/**
 * gum_string_utils_append_string:
 * @src_strv: (transfer none): vector of strings
 * @string: (transfer none): string to append
 *
 * Appends the string to the end of the strings' vector.
 *
 * Returns: (transfer full): concatenated string vector if successful, NULL
 * otherwise.
 */
gchar **
gum_string_utils_append_string (
        gchar **src_strv,
        const gchar *string)
{
    gchar **dest_strv = NULL;
    gint ind = 0;
    gint len = 0;

    if (src_strv)
        len = g_strv_length (src_strv);

    dest_strv = g_malloc0 (sizeof (gchar *) * (len + 2));
    if (src_strv) {
        while (src_strv[ind]) {
            dest_strv[ind] = g_strdup (src_strv[ind]);
            ind++;
        }
    }
    dest_strv[ind++] = g_strdup (string);
    dest_strv[ind] = NULL;

    return dest_strv;
}
