/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#define GUM_CONFIG_GENERAL                 "General"

#define GUM_CONFIG_GENERAL_DEF_USR_GROUPS   GUM_CONFIG_GENERAL \
                                              "/DEFAULT_USR_GROUPS"
#define GUM_CONFIG_GENERAL_PASSWD_FILE      GUM_CONFIG_GENERAL \
                                              "/PASSWD_FILE"
#define GUM_CONFIG_GENERAL_SHADOW_FILE      GUM_CONFIG_GENERAL \
                                              "/SHADOW_FILE"
#define GUM_CONFIG_GENERAL_GROUP_FILE       GUM_CONFIG_GENERAL \
                                              "/GROUP_FILE"
#define GUM_CONFIG_GENERAL_GSHADOW_FILE     GUM_CONFIG_GENERAL \
                                              "/GSHADOW_FILE"
#define GUM_CONFIG_GENERAL_HOME_DIR_PREF    GUM_CONFIG_GENERAL \
                                              "/HOME_DIR"
#define GUM_CONFIG_GENERAL_SHELL            GUM_CONFIG_GENERAL \
                                              "/SHELL"
#define GUM_CONFIG_GENERAL_SKEL_DIR         GUM_CONFIG_GENERAL \
                                              "/SKEL_DIR"
#define GUM_CONFIG_GENERAL_UID_MIN          GUM_CONFIG_GENERAL \
                                              "/UID_MIN"
#define GUM_CONFIG_GENERAL_UID_MAX          GUM_CONFIG_GENERAL \
                                              "/UID_MAX"
#define GUM_CONFIG_GENERAL_SYS_UID_MIN      GUM_CONFIG_GENERAL \
                                              "/SYS_UID_MIN"
#define GUM_CONFIG_GENERAL_SYS_UID_MAX      GUM_CONFIG_GENERAL \
                                              "/SYS_UID_MAX"
#define GUM_CONFIG_GENERAL_GID_MIN          GUM_CONFIG_GENERAL \
                                              "/GID_MIN"
#define GUM_CONFIG_GENERAL_GID_MAX          GUM_CONFIG_GENERAL \
                                              "/GID_MAX"
#define GUM_CONFIG_GENERAL_SYS_GID_MIN      GUM_CONFIG_GENERAL \
                                              "/SYS_GID_MIN"
#define GUM_CONFIG_GENERAL_SYS_GID_MAX      GUM_CONFIG_GENERAL \
                                              "/SYS_GID_MAX"
#define GUM_CONFIG_GENERAL_PASS_MAX_DAYS    GUM_CONFIG_GENERAL \
                                              "/PASS_MAX_DAYS"
#define GUM_CONFIG_GENERAL_PASS_MIN_DAYS    GUM_CONFIG_GENERAL \
                                              "/PASS_MIN_DAYS"
#define GUM_CONFIG_GENERAL_PASS_WARN_AGE    GUM_CONFIG_GENERAL \
                                              "/PASS_WARN_AGE"
#define GUM_CONFIG_GENERAL_UMASK            GUM_CONFIG_GENERAL \
	                                          "/UMASK"
#endif /* __GUM_GENERAL_CONFIG_H_ */
