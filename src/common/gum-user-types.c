/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2015 Intel Corporation.
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

#include "common/gum-user-types.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

/**
 * SECTION:gum-user-types
 * @short_description: Utility functions for user types
 * @title: Gum User Types
 * @include: gum/common/gum-user-types.h
 *
 * User type conversion/validation functions
 */

typedef struct  {
    GumUserType type;
    const char *str;
} GumUserTypeString;

GumUserTypeString user_type_strings[GUM_USERTYPE_COUNT] = {
        {GUM_USERTYPE_NONE, "none"},
        {GUM_USERTYPE_SYSTEM, "system"},
        {GUM_USERTYPE_ADMIN, "admin"},
        {GUM_USERTYPE_GUEST, "guest"},
        {GUM_USERTYPE_NORMAL, "normal"}
};

gint16
_get_user_type_strings_index (
        GumUserType type)
{
    if (type >= GUM_USERTYPE_MAX_VALUE || type < 0)
        return -1;
    gint index = 0;
    while (type) {
        type >>= 1;
        index++;
    }
    return index;
}

/**
 * gum_user_type_to_string:
 * @type: the user type enum to convert
 *
 * Converts the user type enum to string
 *
 * Returns: (transfer none): usertype if conversion succeeds, NULL otherwise.
 */
const gchar *
gum_user_type_to_string (
        GumUserType type)
{
    gint16 ind = _get_user_type_strings_index (type);
    if (ind <= 0 || ind >= GUM_USERTYPE_COUNT)
        return NULL;
    return user_type_strings[ind].str;
}

/**
 * gum_user_type_from_string:
 * @type: (transfer none): the user type string to convert
 *
 * Validates and then converts it to the correct user type
 *
 * Returns: #GumUserType if conversion succeeds, GUM_USERTYPE_NONE otherwise.
 */
GumUserType
gum_user_type_from_string (
        const gchar *type)
{
    gint i = 0;
    if (!type || strlen(type) <= 0) {
        return GUM_USERTYPE_NONE;
    }

    while (i < GUM_USERTYPE_COUNT) {
        if (g_strcmp0 (type,  user_type_strings[i].str) == 0)
            return user_type_strings[i].type;
        i++;
    }
    WARN ("user type %s not found", type);
    return GUM_USERTYPE_NONE;
}

/**
 * gum_user_type_to_strv:
 * @types: the type(s) of the user OR'd as per defined as #GumUserType
 * e.g. GUM_USERTYPE_SYSTEM | GUM_USERTYPE_NORMAL
 *
 * Converts enumerated #GumUserType types into an array of string of user type.
 *
 * Returns: (transfer full) if successful returns a string, NULL otherwise.
 */
gchar **
gum_user_type_to_strv (
        guint16 types)
{
    gint i = 0, ind = 0;
    gchar **strv = g_malloc0 (sizeof (gchar *) * (GUM_USERTYPE_COUNT + 1));
    while (i < GUM_USERTYPE_COUNT) {
        if (types & user_type_strings[i].type) {
            strv[ind++] = g_strdup ((gchar *) user_type_strings[i].str);
        }
        i++;
    }
    strv[ind] = NULL;
    return strv;
}

/**
 * gum_user_type_from_strv:
 * @types: (transfer none): an array of string of user types
 *
 * Converts the string consisting of types to a single uint16 by ORing each
 * type as #GumUserType e.g. if string array contains guest and normal, then
 * result would be GUM_USERTYPE_GUEST | GUM_USERTYPE_NORMAL
 *
 * Returns: if successful returns the converted types, GUM_USERTYPE_NONE
 * otherwise.
 */
guint16
gum_user_type_from_strv (
        const gchar *const *types)
{
    guint16 res = GUM_USERTYPE_NONE;
    gint i = 0, len = 0;
    gchar **strv = (gchar **)types;

    if (!strv || (len = g_strv_length (strv)) <= 0) {
        return GUM_USERTYPE_NONE;
    }

    while (i < len) {
        res |= gum_user_type_from_string (strv[i++]);
    }
    return res;
}
