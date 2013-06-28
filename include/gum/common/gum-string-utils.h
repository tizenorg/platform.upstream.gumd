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

#ifndef __GUM_STRING_UTILS_H_
#define __GUM_STRING_UTILS_H_

#include <glib.h>

#define GUM_STR_FREE(s) { \
    if (s) { \
        g_free (s); \
        s = NULL; \
    } \
    }

#define GUM_STR_DUP(s, d) { \
    GUM_STR_FREE (d); \
    d = g_strdup (s); \
    }

#define GUM_STR_FREEV(s) { \
    if (s) { \
        g_strfreev (s); \
        s = NULL; \
    } \
    }

#define GUM_STR_DUPV(s, d) { \
    GUM_STR_FREEV (d); \
    d = g_strdupv (s); \
    }

G_BEGIN_DECLS

gboolean
gum_string_utils_search_string (
        const gchar *strings,
        const gchar *separator,
        const gchar *search_str);

gboolean
gum_string_utils_search_stringv (
        gchar **stringv,
        const gchar *search_str);

gchar *
gum_string_utils_get_string (
        const gchar *strings,
        const gchar *separator,
        guint str_ind /*starts from 0*/);

gchar *
gum_string_utils_insert_string (
        const gchar *strings,
        const gchar *separator,
        const gchar *str_to_insert,
        guint str_ind, /*starts from 0*/
        guint total_strings);

gchar **
gum_string_utils_delete_string (
        gchar **src_strv,
        const gchar *string);

gchar **
gum_string_utils_append_string (
        gchar **src_strv,
        const gchar *string);

G_END_DECLS

#endif /* __GUM_STRING_UTILS_H_ */
