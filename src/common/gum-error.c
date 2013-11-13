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

/**
 * SECTION:gum-error
 * @short_description: error definitions and utilities
 * @title: Errors
 * @include: gum/common/gum-error.h
 *
 * This file provides Gum error definitions and utilities.
 * When creating an error, use #GUM_ERROR for the error domain and errors
 * from #GumError for the error code.
 *
 * |[
 *     GError* err = g_error_new(GUM_ERROR, GUM_ERROR_USER_ALREADY_EXISTS,
 *     "User already exists");
 * ]|
 */

/**
 * GUM_ERROR:
 *
 * This macro should be used when creating a #GError (for example with
 * g_error_new()).
 */

/**
 * GumError:
 * @GUM_ERROR_NONE: No error
 * @GUM_ERROR_UNKNOWN: Catch-all for errors not distinguished by another error
 * code
 * @GUM_ERROR_INTERNAL_SERVER: Server internal error
 * @GUM_ERROR_PERMISSION_DENIED: The operation cannot be performed due to
 * insufficient client permissions
 * @GUM_ERROR_USER_ALREADY_EXISTS: User already exists
 * @GUM_ERROR_USER_GROUP_ADD_FAILURE: Adding/creating groups for the user
 * failure
 * @GUM_ERROR_USER_UID_NOT_AVAILABLE: UID not available in the defined range
 * @GUM_ERROR_USER_INVALID_USER_TYPE: Usertype is not set
 * @GUM_ERROR_USER_SECRET_ENCRYPT_FAILURE: Encryption of secret failure
 * @GUM_ERROR_USER_NOT_FOUND: User not found
 * @GUM_ERROR_USER_INVALID_DATA: Invalid data provided in the request
 * @GUM_ERROR_USER_GROUP_DELETE_FAILURE: Deleting group for the user failure
 * @GUM_ERROR_USER_SELF_DESTRUCTION: Self-destruction is not allowed
 * @GUM_ERROR_USER_SESSION_TERM_FAILURE: Session termination of a user failure
 * @GUM_ERROR_USER_NO_CHANGES: No changes specified in the user update request
 * @GUM_ERROR_USER_LOCK_FAILURE: Unable to lock the user account before doing
 * any changes
 * @GUM_ERROR_GROUP_ALREADY_EXISTS: Group already exists
 * @GUM_ERROR_GROUP_GID_NOT_AVAILABLE: GID mot available in the defined range
 * @GUM_ERROR_GROUP_INVALID_GROUP_TYPE: Group type not set
 * @GUM_ERROR_GROUP_SECRET_ENCRYPT_FAILURE: Encryption of group secret failure
 * @GUM_ERROR_GROUP_NOT_FOUND: Group not found
 * @GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER: User is already a member of the group
 * @GUM_ERROR_GROUP_INVALID_DATA: Invalid data is specified in the request
 * @GUM_ERROR_GROUP_SELF_DESTRUCTION: Self-destruction is not allowed
 * @GUM_ERROR_GROUP_HAS_USER: Group has (other) user(s)
 * @GUM_ERROR_GROUP_NO_CHANGES: No changes specified in the group update request
 * @GUM_ERROR_DB_ALREADY_LOCKED: Database is already locked
 * @GUM_ERROR_FILE_OPEN: File open error
 * @GUM_ERROR_FILE_ATTRIBUTE: File attribute error
 * @GUM_ERROR_FILE_MOVE: File move error
 * @GUM_ERROR_FILE_WRITE: File write error
 * @GUM_ERROR_HOME_DIR_CREATE_FAILURE: Directory create failure
 * @GUM_ERROR_HOME_DIR_DELETE_FAILURE: Directory delete failure
 * @GUM_ERROR_HOME_DIR_COPY_FAILURE: Directory copy failure
 * @GUM_ERROR_INVALID_NAME: Invalid name specified
 * @GUM_ERROR_INVALID_NICKNAME: Invalid nickname specified
 * @GUM_ERROR_INVALID_SECRET: Invalid secret specified
 * @GUM_ERROR_INVALID_STR: Invalid string specified
 * @GUM_ERROR_INVALID_STR_LEN: Invalid string length
 * @GUM_ERROR_INVALID_INPUT: Invalid input specified
 * @GUM_ERROR_USER_ERR: Placeholder to rearrange enumeration - User space
 * specific
 *
 * This enum provides a list of errors
 */

/**
 * GUM_ERROR_DOMAIN:
 *
 * This macro defines the error domain for gum.
 */
#define GUM_ERROR_DOMAIN "gum"

/**
 * GUM_GET_ERROR_FOR_ID:
 * @code: A #GumError specifying the error
 * @message: Format string for the error message
 * @...: parameters for the error string
 *
 * A helper macro that creates a #GError with the proper gum domain
 */

/**
 * GUM_SET_ERROR:
 * @code: the error code as listed in #GumError
 * @err_str: the error message to be set for the #GError
 * @err: a #GError
 * @retvar: return variable to hold the return value
 * @retval: return value to be set to the return variable
 *
 * A helper macro that creates a #GError with the proper gum domain, output
 * the error message to logs, and sets the specified retval to retvar.
 */

/**
 * GUM_RETURN_WITH_ERROR:
 * @code: the error code as listed in #GumError
 * @err_str: the error message to be set for the #GError
 * @err: a #GError
 * @retval: value to be used on return
 *
 * A helper macro that creates a #GError with the proper gum domain, output
 * the error message to logs, and returns with specified retval.
 */

#define _ERROR_PREFIX "org.tizen.SecurityAccounts.gUserManagement.Error"

GDBusErrorEntry _gum_errors[] =
{
    {GUM_ERROR_UNKNOWN, _ERROR_PREFIX".Unknown"},
    {GUM_ERROR_INTERNAL_SERVER, _ERROR_PREFIX".InternalServerError"},
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
    {GUM_ERROR_USER_LOCK_FAILURE, _ERROR_PREFIX".UserLockFailure"},

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

 /**
  * gum_error_quark:
  *
  * Creates and returns a domain for Gum errors.
  *
  * Returns: #GQuark for Gum errors
  */
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

/**
 * gum_error_new_from_variant:
 * @var: (transfer none): instance of #GVariant
 *
 * Converts the GVariant to GError.
 *
 * Returns: (transfer full): #GError object if successful, NULL otherwise.
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
 * @error: (transfer none): instance of #GError
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

