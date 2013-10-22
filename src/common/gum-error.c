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

#include <string.h>
#include <gio/gio.h>

#include "common/gum-error.h"

#define _ERROR_PREFIX "org.tizen.SecurityAccounts.gUserManagement.Error"

GDBusErrorEntry _gum_errors[] =
{
    {GUM_ERROR_UNKNOWN, _ERROR_PREFIX".Unknown"},
    {GUM_ERROR_INTERNAL_SERVER, _ERROR_PREFIX".InternalServerError"},
    {GUM_ERROR_INTERNAL_COMMUNICATION,
            _ERROR_PREFIX".InternalCommunicationError"},
    {GUM_ERROR_PERMISSION_DENIED, _ERROR_PREFIX".PermissionDenied"},

    {GUM_ERROR_USER_ALREADY_EXISTS, _ERROR_PREFIX".UserAlreadyExists"},
    {GUM_ERROR_USER_GROUP_ADD_FAILURE, _ERROR_PREFIX".UserGroupAddFailure"},
    {GUM_ERROR_USER_UID_NOT_AVAILABLE, _ERROR_PREFIX".UidNotAvailable"},
    {GUM_ERROR_USER_INVALID_USER_TYPE, _ERROR_PREFIX".InvalidUserType"},
    {GUM_ERROR_USER_SECRET_ENCRYPT_FAILURE,
            _ERROR_PREFIX".UserEncryptFailure"},
    {GUM_ERROR_USER_NOT_FOUND, _ERROR_PREFIX".UserNotFound"},
    {GUM_ERROR_USER_INVALID_DATA, _ERROR_PREFIX".UserInvalidData"},
    {GUM_ERROR_USER_GROUP_DELETE_FAILURE,
            _ERROR_PREFIX".UserGroupDeleteFailure"},
    {GUM_ERROR_USER_SELF_DESTRUCTION, _ERROR_PREFIX".UserSelfDestruction"},
    {GUM_ERROR_USER_SESSION_TERM_FAILURE,
            _ERROR_PREFIX".UserSessionTermFailure"},

    {GUM_ERROR_USER_NO_CHANGES, _ERROR_PREFIX".UserNoChanges"},

    {GUM_ERROR_GROUP_ALREADY_EXISTS, _ERROR_PREFIX".GroupAlreadyExists"},
    {GUM_ERROR_GROUP_GID_NOT_AVAILABLE, _ERROR_PREFIX".GidNotAvailable"},
    {GUM_ERROR_GROUP_INVALID_GROUP_TYPE, _ERROR_PREFIX".InvalidGroupType"},
    {GUM_ERROR_GROUP_SECRET_ENCRYPT_FAILURE,
            _ERROR_PREFIX".GroupEncryptFailure"},
    {GUM_ERROR_GROUP_NOT_FOUND, _ERROR_PREFIX".GroupNotFound"},
    {GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER,
            _ERROR_PREFIX".GroupUserAlreadyMember"},
    {GUM_ERROR_GROUP_INVALID_DATA, _ERROR_PREFIX".GroupInvalidData"},
    {GUM_ERROR_GROUP_SELF_DESTRUCTION, _ERROR_PREFIX".GroupSelfDestruction"},
    {GUM_ERROR_GROUP_HAS_USER, _ERROR_PREFIX".GroupHasUser"},
    {GUM_ERROR_GROUP_NO_CHANGES, _ERROR_PREFIX".GroupNoChanges"},

    {GUM_ERROR_DB_ALREADY_LOCKED, _ERROR_PREFIX".DbAlreadyLocked"},
    {GUM_ERROR_FILE_OPEN, _ERROR_PREFIX".FileOpenError"},
    {GUM_ERROR_FILE_ATTRIBUTE, _ERROR_PREFIX".FileAttributeError"},
    {GUM_ERROR_FILE_MOVE, _ERROR_PREFIX".FileMoveError"},
    {GUM_ERROR_FILE_WRITE, _ERROR_PREFIX".FileWriteError"},
    {GUM_ERROR_HOME_DIR_CREATE_FAILURE, _ERROR_PREFIX".HomeDirCreateFailure"},
    {GUM_ERROR_HOME_DIR_DELETE_FAILURE, _ERROR_PREFIX".HomeDirDeleteFailure"},
    {GUM_ERROR_HOME_DIR_COPY_FAILURE, _ERROR_PREFIX".HomeDirCopyFailure"},

    {GUM_ERROR_INVALID_NAME, _ERROR_PREFIX".InvalidName"},
    {GUM_ERROR_INVALID_NICKNAME, _ERROR_PREFIX".InvalidNickName"},

    {GUM_ERROR_INVALID_SECRET, _ERROR_PREFIX".InvalidSecret"},

    {GUM_ERROR_INVALID_STR, _ERROR_PREFIX".InvalidString"},
    {GUM_ERROR_INVALID_STR_LEN, _ERROR_PREFIX".InvalidStringLength"},

    {GUM_ERROR_INVALID_INPUT, _ERROR_PREFIX".InvalidInput"},
} ;

GQuark
gum_error_quark (void)
{
    static volatile gsize quark_volatile = 0;

    g_dbus_error_register_error_domain (GUM_ERROR_DOMAIN,
                                        &quark_volatile,
                                        _gum_errors,
                                        G_N_ELEMENTS (_gum_errors));

    return (GQuark) quark_volatile;
}

GString*
gum_concat_domain_and_error (
        const gchar *str1,
        const gchar *str2)
{
    GString *str = NULL;
    g_return_val_if_fail (str1 != NULL && str2 != NULL, NULL);
    str = g_string_sized_new (strlen(str1)+strlen(str2)-1);
    g_string_printf (str,"[%s].%s\n",str1,str2);
    return str;
}

GString*
gum_prepend_domain_to_error_msg (const GError *err)
{
    GString *msg = NULL;
    const gchar *domain = NULL;
    g_return_val_if_fail (err != NULL, NULL);
    if (err->message != NULL) {
        domain = g_quark_to_string(err->domain);
        msg = gum_concat_domain_and_error(domain, err->message);
    }
    return msg;
}

/**
 * gum_error_new_from_variant:
 * @var: instance of #GVariant
 *
 * Converts the GVariant to GError.
 *
 * Returns: (transfer full) #GError object if successful, NULL otherwise.
 */
GError *
gum_error_new_from_variant (
        GVariant *var)
{
    GError *error = NULL;
    gchar *message;
    GQuark domain;
    gint code;

    if (!var) {
        return NULL;
    }

    g_variant_get (var, "(uis)", &domain, &code, &message);
    error = g_error_new_literal (domain, code, message);
    g_free (message);
    return error;
}

/**
 * gum_error_to_variant:
 * @error: instance of #GError
 *
 * Converts the GError to GVariant.
 *
 * Returns: (transfer full) #GVariant object if successful, NULL otherwise.
 */
GVariant *
gum_error_to_variant (
        GError *error)
{
    if (!error) {
        return NULL;
    }

    return g_variant_new ("(uis)", error->domain, error->code, error->message);
}

