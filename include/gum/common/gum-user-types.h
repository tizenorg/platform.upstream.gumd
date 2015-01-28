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

#ifndef __GUM_USER_TYPES_H_
#define __GUM_USER_TYPES_H_

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:gum-user-types
 * @short_description: user types definition
 * @title: User types
 *
 * This file provides various types of users that can be created by the user
 * management framework.
 *
 */

/**
 * GumUserType:
 * @GUM_USERTYPE_NONE: user type not defined/set/invalid
 * @GUM_USERTYPE_SYSTEM: no home directory will be created for system user.
 * System user is not able to use login/logout functionality as
 * its primary usage is limited to system daemons. Uid will be chosen between
 * #GUM_CONFIG_GENERAL_SYS_UID_MIN and #GUM_CONFIG_GENERAL_SYS_UID_MAX
 * @GUM_USERTYPE_ADMIN: admin user is similar to normal user with the addition
 * that it will be assigned to admin user groups at the time of account
 * creation. Uid will be chosen between #GUM_CONFIG_GENERAL_UID_MIN and
 * #GUM_CONFIG_GENERAL_UID_MAX
 * @GUM_USERTYPE_GUEST: guest user does not need secret/password to login.
 * Guest user home directory is created with login and cleaned up/destroyed
 * when user logs out. Uid will be chosen between #GUM_CONFIG_GENERAL_UID_MIN
 * and #GUM_CONFIG_GENERAL_UID_MAX
 * @GUM_USERTYPE_NORMAL: normal user with home directory created based on prefix
 * #GUM_CONFIG_GENERAL_HOME_DIR_PREF. Contents of #GUM_CONFIG_GENERAL_SKEL_DIR
 * are copied to the home directory. Uid will be chosen between
 * #GUM_CONFIG_GENERAL_UID_MIN and #GUM_CONFIG_GENERAL_UID_MAX
 *
 * This enumeration lists users types.
 */

/**
 * GUM_USERTYPE_COUNT:
 *
 * Defines total number of types of the users.
 */
#define GUM_USERTYPE_COUNT   5

/**
 * GUM_USERTYPE_MAX_VALUE:
 *
 * Defines the maximum value of the user type in #GumUserType.
 */
#define GUM_USERTYPE_MAX_VALUE   (0x1 << (GUM_USERTYPE_COUNT-1))

typedef enum {
    GUM_USERTYPE_NONE = 0x00,
    GUM_USERTYPE_SYSTEM = 0x01,
    GUM_USERTYPE_ADMIN = 0x02,
    GUM_USERTYPE_GUEST = 0x04,
    GUM_USERTYPE_NORMAL = 0x08
} GumUserType;

/**
 * GumUserTypeString:
 * @type: user type #GumUserType
 * @str: string name for the type #GumUserType.
 *
 * This structure combines user type and its string name.
 */
typedef struct  {
    GumUserType type;
    const char *str;
} GumUserTypeString;

const gchar *
gum_user_type_to_string (
        GumUserType type);

GumUserType
gum_user_type_from_string (
        const gchar *type);

gchar *
gum_user_type_users_to_string (
        guint16 types);

guint16
gum_user_type_users_to_integer (
        const gchar *types);

G_END_DECLS

#endif /* __GUM_USER_TYPES_H_ */
