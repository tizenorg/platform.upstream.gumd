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

#ifndef __GUM_CONFIG_GENERAL_H_
#define __GUM_CONFIG_GENERAL_H_

/**
 * SECTION:gum-config-general
 * @title: General configuration
 * @short_description: gum general configuration keys
 * @include: gum/common/gum-config.h
 *
 * General configuration keys are defined below. See #GumConfig for how to use
 * them.
 */

/**
 * GUM_CONFIG_GENERAL:
 *
 * A prefix for general keys. Should be used only when defining new keys.
 */
#define GUM_CONFIG_GENERAL                  "General"

/**
 * GUM_CONFIG_GENERAL_DEF_USR_GROUPS:
 *
 * Comma separate listed of groups, which every user (other than system user)
 * will be added to at the time of user account creation. Default value is:
 * 'users'
 */
#define GUM_CONFIG_GENERAL_DEF_USR_GROUPS   GUM_CONFIG_GENERAL \
                                              "/DEFAULT_USR_GROUPS"

/**
 * GUM_CONFIG_GENERAL_PASSWD_FILE:
 *
 * Path to passwd file which represents user information. Default is
 * /etc/passwd. More information about the file format can be read at the
 * manpages for 'passwd'.
 *
 * Can be overriden in debug builds by setting UM_PASSWD_FILE
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_PASSWD_FILE      GUM_CONFIG_GENERAL \
                                              "/PASSWD_FILE"

/**
 * GUM_CONFIG_GENERAL_SHADOW_FILE:
 *
 * Path to shadow file which represents user shadowed password information.
 * Default is /etc/shadow. More information about the file format can be read
 * at the manpages for 'shadow'.
 *
 * Can be overriden in debug builds by setting UM_SHADOW_FILE
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_SHADOW_FILE      GUM_CONFIG_GENERAL \
                                              "/SHADOW_FILE"

/**
 * GUM_CONFIG_GENERAL_GROUP_FILE:
 *
 * Path to group file which represents group information. Default is /etc/group.
 * More information about the file format can be read at the manpages for
 * 'group'.
 *
 * Can be overriden in debug builds by setting UM_GROUP_FILE
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_GROUP_FILE       GUM_CONFIG_GENERAL \
                                              "/GROUP_FILE"

/**
 * GUM_CONFIG_GENERAL_GSHADOW_FILE:
 *
 * Path to group file which represents shadowed group information.
 * Default is /etc/gshadow. More information about the file format can be read
 * at the manpages for 'gshadow'.
 *
 * Can be overriden in debug builds by setting UM_GSHADOW_FILE
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_GSHADOW_FILE     GUM_CONFIG_GENERAL \
                                              "/GSHADOW_FILE"

/**
 * GUM_CONFIG_GENERAL_HOME_DIR_PREF:
 *
 * Prefix to be used when creating home directory for the user. For example,
 * with prefix '/home', user 'newu' home directory will be created as
 * '/home/newu'. Default value is '/home'
 *
 * Can be overriden in debug builds by setting UM_HOMEDIR_PREFIX
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_HOME_DIR_PREF    GUM_CONFIG_GENERAL \
                                              "/HOME_DIR"

/**
 * GUM_CONFIG_GENERAL_SHELL:
 *
 * Path to user shell executable. Default value is '/bin/bash'
 */
#define GUM_CONFIG_GENERAL_SHELL            GUM_CONFIG_GENERAL \
                                              "/SHELL"

/**
 * GUM_CONFIG_GENERAL_SKEL_DIR:
 *
 * Path to skeleton folder. When new users are created, contents of the skel
 * folder is copied to user home directory. Default value is '/etc/skel'
 *
 * Can be overriden in debug builds by setting UM_SKEL_DIR
 * environment variable.
 */
#define GUM_CONFIG_GENERAL_SKEL_DIR         GUM_CONFIG_GENERAL \
                                              "/SKEL_DIR"

/**
 * GUM_CONFIG_GENERAL_UID_MIN:
 *
 * Minimum value for the automatic uid selection. Default value is: 2000
 */
#define GUM_CONFIG_GENERAL_UID_MIN          GUM_CONFIG_GENERAL \
                                              "/UID_MIN"

/**
 * GUM_CONFIG_GENERAL_UID_MAX:
 *
 * Maximum value for the automatic uid selection. Default value is: 60000
 */
#define GUM_CONFIG_GENERAL_UID_MAX          GUM_CONFIG_GENERAL \
                                              "/UID_MAX"

/**
 * GUM_CONFIG_GENERAL_SYS_UID_MIN:
 *
 * Minimum value for the automatic uid selection for system user. Default
 * value is: 200
 */
#define GUM_CONFIG_GENERAL_SYS_UID_MIN      GUM_CONFIG_GENERAL \
                                              "/SYS_UID_MIN"

/**
 * GUM_CONFIG_GENERAL_SYS_UID_MAX:
 *
 * Maximum value for the automatic uid selection for system user. Default value
 * is: 999
 */
#define GUM_CONFIG_GENERAL_SYS_UID_MAX      GUM_CONFIG_GENERAL \
                                              "/SYS_UID_MAX"

/**
 * GUM_CONFIG_GENERAL_GID_MIN:
 *
 * Minimum value for the automatic gid selection. Default value is: 2000
 */
#define GUM_CONFIG_GENERAL_GID_MIN          GUM_CONFIG_GENERAL \
                                              "/GID_MIN"

/**
 * GUM_CONFIG_GENERAL_GID_MAX:
 *
 * Maximum value for the automatic gid selection. Default value is: 60000
 */
#define GUM_CONFIG_GENERAL_GID_MAX          GUM_CONFIG_GENERAL \
                                              "/GID_MAX"

/**
 * GUM_CONFIG_GENERAL_SYS_GID_MIN:
 *
 * Minimum value for the automatic gid selection for system user. Default value
 * is: 200
 */
#define GUM_CONFIG_GENERAL_SYS_GID_MIN      GUM_CONFIG_GENERAL \
                                              "/SYS_GID_MIN"

/**
 * GUM_CONFIG_GENERAL_SYS_GID_MAX:
 *
 * Maximum value for the automatic gid selection for system user. Default value
 * is: 999
 */
#define GUM_CONFIG_GENERAL_SYS_GID_MAX      GUM_CONFIG_GENERAL \
                                              "/SYS_GID_MAX"

/**
 * GUM_CONFIG_GENERAL_PASS_MIN_DAYS:
 *
 * Minimum number of days a password may be used. Default value is: 0
 */
#define GUM_CONFIG_GENERAL_PASS_MIN_DAYS    GUM_CONFIG_GENERAL \
                                              "/PASS_MIN_DAYS"

/**
 * GUM_CONFIG_GENERAL_PASS_MAX_DAYS:
 *
 * Maximum number of days allowed between password changes. Default value is:
 * 99999
 */
#define GUM_CONFIG_GENERAL_PASS_MAX_DAYS    GUM_CONFIG_GENERAL \
                                              "/PASS_MAX_DAYS"

/**
 * GUM_CONFIG_GENERAL_PASS_WARN_AGE:
 *
 * Number of days warning given before a password expires. Default value is:
 * 7
 */
#define GUM_CONFIG_GENERAL_PASS_WARN_AGE    GUM_CONFIG_GENERAL \
                                              "/PASS_WARN_AGE"

/**
 * GUM_CONFIG_GENERAL_UMASK:
 *
 * Value used to set the mode of home directories created for new users.
 * Default value is: 022
 */
#define GUM_CONFIG_GENERAL_UMASK            GUM_CONFIG_GENERAL \
	                                          "/UMASK"

#endif /* __GUM_GENERAL_CONFIG_H_ */
