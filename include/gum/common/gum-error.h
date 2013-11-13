/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@linux.intel.com>
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

/* inclusion guard */
#ifndef __GUM_ERROR_H__
#define __GUM_ERROR_H__

#include <glib.h>
#include <common/gum-log.h>

G_BEGIN_DECLS

#define GUM_ERROR   (gum_error_quark())

typedef enum {
    GUM_ERROR_NONE,

    GUM_ERROR_UNKNOWN = 1,
    GUM_ERROR_INTERNAL_SERVER,
    GUM_ERROR_PERMISSION_DENIED,

    GUM_ERROR_USER_ALREADY_EXISTS = 32,
    GUM_ERROR_USER_GROUP_ADD_FAILURE,
    GUM_ERROR_USER_UID_NOT_AVAILABLE,
    GUM_ERROR_USER_INVALID_USER_TYPE,
    GUM_ERROR_USER_SECRET_ENCRYPT_FAILURE,
    GUM_ERROR_USER_NOT_FOUND,
    GUM_ERROR_USER_INVALID_DATA,
    GUM_ERROR_USER_GROUP_DELETE_FAILURE,
    GUM_ERROR_USER_SELF_DESTRUCTION,
    GUM_ERROR_USER_SESSION_TERM_FAILURE,
    GUM_ERROR_USER_NO_CHANGES,
    GUM_ERROR_USER_LOCK_FAILURE,

    GUM_ERROR_GROUP_ALREADY_EXISTS = 64,
    GUM_ERROR_GROUP_GID_NOT_AVAILABLE,
    GUM_ERROR_GROUP_INVALID_GROUP_TYPE,
    GUM_ERROR_GROUP_SECRET_ENCRYPT_FAILURE,
    GUM_ERROR_GROUP_NOT_FOUND,
    GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER,
    GUM_ERROR_GROUP_INVALID_DATA,
    GUM_ERROR_GROUP_SELF_DESTRUCTION,
    GUM_ERROR_GROUP_HAS_USER,
    GUM_ERROR_GROUP_NO_CHANGES,

    GUM_ERROR_DB_ALREADY_LOCKED = 90,
    GUM_ERROR_FILE_OPEN,
    GUM_ERROR_FILE_ATTRIBUTE,
    GUM_ERROR_FILE_MOVE,
    GUM_ERROR_FILE_WRITE,
    GUM_ERROR_HOME_DIR_CREATE_FAILURE,
    GUM_ERROR_HOME_DIR_DELETE_FAILURE,
    GUM_ERROR_HOME_DIR_COPY_FAILURE,

    GUM_ERROR_INVALID_NAME = 120,
    GUM_ERROR_INVALID_NICKNAME,

    GUM_ERROR_INVALID_SECRET = 130,

    GUM_ERROR_INVALID_STR = 140,
    GUM_ERROR_INVALID_STR_LEN,

    GUM_ERROR_INVALID_INPUT = 160,

    GUM_ERROR_USER_ERR = 400

} GumError;

#define GUM_GET_ERROR_FOR_ID(code, message, args...) \
    g_error_new (GUM_ERROR, code, message, ##args);

#define GUM_SET_ERROR(code, err_str, err, retvar, retval) \
    { \
        if (err) { \
            *err = GUM_GET_ERROR_FOR_ID (code, err_str); \
            DBG ("Error %d:%s", code, err_str); \
        } \
        retvar = retval; \
    }

#define GUM_RETURN_WITH_ERROR(code, err_str, err, retval) \
    { \
        if (err) { \
            *err = GUM_GET_ERROR_FOR_ID (code, err_str); \
            DBG ("Error %d:%s", code, err_str); \
        } \
        return retval; \
    }

GQuark
gum_error_quark (void);

GError *
gum_error_new_from_variant (
        GVariant *var);

GVariant *
gum_error_to_variant (
        GError *error);

G_END_DECLS

#endif /* __GUM_ERROR_H__ */
