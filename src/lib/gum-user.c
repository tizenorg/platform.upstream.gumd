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

#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-user-types.h"
#include "common/dbus/gum-dbus-user-service-gen.h"
#include "common/dbus/gum-dbus-user-gen.h"

#include "gum-user.h"
#include "gum-user-service.h"
#include "gum-internals.h"

typedef struct {
    GumUserCb callback;
    gpointer user_data;
    GError *error;
    guint cb_id;
} GumUserOp;

struct _GumUserPrivate
{
    GumDbusUserService *dbus_service;
    GumDbusUser *dbus_user;
    GCancellable *cancellable;
    GumUserOp *op;
};

G_DEFINE_TYPE (GumUser, gum_user, G_TYPE_OBJECT)

#define GUM_USER_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_USER, GumUserPrivate)
enum
{
    PROP_0,

    PROP_USERTYPE,
    PROP_NICKNAME,
    PROP_UID,
    PROP_GID,
    PROP_USERNAME,
    PROP_SECRET,
    PROP_REALNAME,
    PROP_OFFICE,
    PROP_OFFICEPHONE,
    PROP_HOMEPHONE,
    PROP_HOMEDIR,
    PROP_SHELL,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];


static void
_free_op (
        GumUser *self)
{
    if (self &&
        self->priv->op) {
        if (self->priv->op->error) g_error_free (self->priv->op->error);
        g_free (self->priv->op);
        self->priv->op = NULL;
    }
}

static void
_create_op (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data)
{
    GumUserOp *op = g_malloc0 (sizeof (GumUserOp));
    op->callback = callback;
    op->user_data = user_data;
    _free_op (self);
    self->priv->op = op;
}

static gboolean
_trigger_callback (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_USER (user_data), FALSE);

    GumUser *user = GUM_USER (user_data);
    if (user->priv->op->callback) {
        (user->priv->op->callback)(user, user->priv->op->error,
                user->priv->op->user_data);
    }
    return FALSE;
}

static void
_setup_idle_callback (
        GumUser *self,
        const GError *error)
{
    if (!self->priv->op->callback) return;
    if (error) {
        if (self->priv->op->error) g_clear_error (&self->priv->op->error);
        self->priv->op->error = g_error_copy (error);
    }
    self->priv->op->cb_id = g_idle_add (_trigger_callback, self);
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumUser *self = GUM_USER (object);
    if (self->priv->dbus_user) {
        g_object_set_property (G_OBJECT(self->priv->dbus_user), pspec->name,
                value);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    GumUser *self = GUM_USER (object);
    if (self->priv->dbus_user) {
        g_object_get_property (G_OBJECT(self->priv->dbus_user), pspec->name,
                value);
    }
}

static void
_dispose (GObject *object)
{
    GumUser *self = GUM_USER (object);

    if (self->priv->op &&
        self->priv->op->cb_id > 0) {
        g_source_remove (self->priv->op->cb_id);
        self->priv->op->cb_id = 0;
    }

    if (self->priv->cancellable) {
        g_cancellable_cancel (self->priv->cancellable);
        g_object_unref (self->priv->cancellable);
        self->priv->cancellable = NULL;
    }

    GUM_OBJECT_UNREF (self->priv->dbus_user);

    GUM_OBJECT_UNREF (self->priv->dbus_service);

    G_OBJECT_CLASS (gum_user_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    GumUser *self = GUM_USER (object);

    _free_op (self);

    G_OBJECT_CLASS (gum_user_parent_class)->finalize (object);
}

static void
gum_user_init (
        GumUser *self)
{
    self->priv = GUM_USER_PRIV (self);
    self->priv->dbus_user = NULL;
    self->priv->cancellable = g_cancellable_new ();
    self->priv->dbus_service = gum_user_service_get_instance ();
    self->priv->op = NULL;
}

static void
gum_user_class_init (
        GumUserClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumUserPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_USERTYPE] =  g_param_spec_uint ("usertype",
            "UserType",
            "Type of the user",
            0,
            G_MAXUINT16,
            GUM_USERTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_UID] =  g_param_spec_uint ("uid",
            "Uid",
            "Unique identifier of the user",
            0,
            G_MAXUINT,
            GUM_USER_INVALID_UID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the user",
            0,
            G_MAXUINT,
            GUM_GROUP_INVALID_GID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_USERNAME] = g_param_spec_string ("username",
            "Username",
            "System name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "User secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_NICKNAME] = g_param_spec_string ("nickname",
            "Nickname",
            "Nick name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_REALNAME] = g_param_spec_string ("realname",
            "Realname",
            "Real name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_OFFICE] = g_param_spec_string ("office",
            "Office",
            "Office location of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_OFFICEPHONE] = g_param_spec_string ("officephone",
            "OfficePhone",
            "Office phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_HOMEPHONE] = g_param_spec_string ("homephone",
            "HomePhone",
            "Home phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_HOMEDIR] = g_param_spec_string ("homedir",
            "HomeDir",
            "Home directory of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SHELL] = g_param_spec_string ("shell",
            "Shell",
            "Shell",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);
}

static void
_on_new_user_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUserService *proxy = GUM_DBUS_USER_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_service_call_create_new_user_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            user->priv->dbus_user = gum_dbus_user_proxy_new_sync (
                g_dbus_proxy_get_connection (G_DBUS_PROXY (
                        user->priv->dbus_service)),
                G_DBUS_PROXY_FLAGS_NONE,
                g_dbus_proxy_get_name (G_DBUS_PROXY (user->priv->dbus_service)),
                object_path, user->priv->cancellable, &error);
        }
        _setup_idle_callback (user, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_get_user_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUserService *proxy = GUM_DBUS_USER_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_service_call_get_user_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            user->priv->dbus_user = gum_dbus_user_proxy_new_sync (
                g_dbus_proxy_get_connection (G_DBUS_PROXY (
                        user->priv->dbus_service)),
                G_DBUS_PROXY_FLAGS_NONE,
                g_dbus_proxy_get_name (G_DBUS_PROXY (user->priv->dbus_service)),
                object_path, user->priv->cancellable, &error);
        }
        _setup_idle_callback (user, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_get_user_by_name_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUserService *proxy = GUM_DBUS_USER_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_service_call_get_user_by_name_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            user->priv->dbus_user = gum_dbus_user_proxy_new_sync (
                g_dbus_proxy_get_connection (G_DBUS_PROXY (
                        user->priv->dbus_service)),
                G_DBUS_PROXY_FLAGS_NONE,
                g_dbus_proxy_get_name (G_DBUS_PROXY (user->priv->dbus_service)),
                object_path, user->priv->cancellable, &error);
        }
        _setup_idle_callback (user, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_user_add_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUser *proxy = GUM_DBUS_USER (object);
    GError *error = NULL;
    uid_t uid = GUM_USER_INVALID_UID;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_call_add_user_finish (proxy, &uid, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (user, error);
    }
    g_clear_error (&error);
}

static void
_on_user_delete_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUser *proxy = GUM_DBUS_USER (object);
    GError *error = NULL;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_call_delete_user_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (user, error);
    }
    g_clear_error (&error);
}

static void
_on_user_update_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUser *user = (GumUser*)user_data;
    GumDbusUser *proxy = GUM_DBUS_USER (object);
    GError *error = NULL;

    g_return_if_fail (user != NULL);

    DBG ("");

    gum_dbus_user_call_update_user_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (user, error);
    }
    g_clear_error (&error);
}

GumUser *
gum_user_create (
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER, NULL));
    g_return_val_if_fail (user->priv->dbus_service != NULL, NULL);

    _create_op (user, callback, user_data);
    gum_dbus_user_service_call_create_new_user (user->priv->dbus_service,
            user->priv->cancellable, _on_new_user_cb, user);
    return user;
}

GumUser *
gum_user_get (
        uid_t uid,
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER, NULL));
    g_return_val_if_fail (user->priv->dbus_service != NULL, NULL);

    _create_op (user, callback, user_data);
    gum_dbus_user_service_call_get_user (user->priv->dbus_service, uid,
            user->priv->cancellable, _on_get_user_cb, user);
    return user;
}

GumUser *
gum_user_get_by_name (
        const gchar *username,
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER, NULL));
    g_return_val_if_fail (user->priv->dbus_service != NULL, NULL);
    if (!username) {
        WARN ("username not specified");
        return NULL;
    }

    _create_op (user, callback, user_data);
    gum_dbus_user_service_call_get_user_by_name (user->priv->dbus_service,
            username, user->priv->cancellable, _on_get_user_by_name_cb, user);
    return user;
}

gboolean
gum_user_add (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (!self->priv->dbus_user) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_add_user (self->priv->dbus_user, self->priv->cancellable,
            _on_user_add_cb, self);
    return TRUE;
}

gboolean
gum_user_delete (
        GumUser *self,
        gboolean rem_home_dir,
        GumUserCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (!self->priv->dbus_user) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_delete_user (self->priv->dbus_user, rem_home_dir,
            self->priv->cancellable, _on_user_delete_cb, self);
    return TRUE;
}

gboolean
gum_user_update (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (!self->priv->dbus_user) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_update_user (self->priv->dbus_user,
            self->priv->cancellable, _on_user_update_cb, self);
    return TRUE;
}
