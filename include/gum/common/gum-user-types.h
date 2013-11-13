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
 * @GUM_USERTYPE_NONE: user type not defined/set
 * @GUM_USERTYPE_SYSTEM: system user. No home directory will be created for
 * system user. System user is not able to use login/logout functionality as
 * its primary usage is limited to system daemons.
 * @GUM_USERTYPE_ADMIN: admin user is similar to normal user with the addition
 * of super user privileges.
 * @GUM_USERTYPE_GUEST: guest user does not need secret/password to login.
 * Guest user home directory is created with login and cleaned up/destroyed
 * when user logs out.
 * @GUM_USERTYPE_NORMAL: normal user with home directory created based on prefix
 * #GUM_CONFIG_GENERAL_HOME_DIR_PREF. Contents of #GUM_CONFIG_GENERAL_SKEL_DIR
 * are copied to the home directory.
 *
 * This enumeration lists users types.
 */

typedef enum {

    GUM_USERTYPE_NONE = 0,
    GUM_USERTYPE_SYSTEM = 1,
    GUM_USERTYPE_ADMIN = 2,
    GUM_USERTYPE_GUEST = 3,
    GUM_USERTYPE_NORMAL = 4

} GumUserType;

G_END_DECLS

#endif /* __GUM_USER_TYPES_H_ */
