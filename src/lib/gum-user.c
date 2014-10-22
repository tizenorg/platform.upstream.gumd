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
#include "daemon/core/gumd-daemon.h"
#include "daemon/core/gumd-daemon-user.h"

#include "gum-user.h"
#include "gum-user-service.h"
#include "gum-internals.h"

/**
 * SECTION:gum-user
 * @short_description: provides interface for managing user's account
 * @include: gum/gum-user.h
 *
 * #GumUser provides interface for adding, removing and updating user accounts.
 * User's information can also be retrieved using this interface. Only
 * privileged user can access the interface when system-bus is used for
 * communication with the user management daemon.
 *
 * Following code snippet demonstrates how to create a new remote user object:
 *
 * |[
 *  GumUser *user = NULL;
 *
 *  user = gum_user_create_sync (FALSE);
 *
 *  // use the object
 *
 *  // destroy the object
 *  g_object_unref (user);
 * ]|
 *
 * Similarly, new user can be added as:
 * |[
 *  GumUser *user = NULL;
 *  gboolean rval = FALSE;
 *
 *  user = gum_user_create_sync (FALSE);
 *
 *  // set user properties
 *  g_object_set (G_OBJECT (user), "username", "user1", "secret", "123456",
 *   "usertype", GUM_USERTYPE_NORMAL, NULL);
 *
 *  // add user
 *  rval = gum_user_add_sync (user);
 *
 *  // destroy the object
 *  g_object_unref (user);
 * ]|
 *
 * For more details, see example implementation here:
 *<ulink url="https://github.com/01org/gumd/blob/master/src/utils/gum-utils.c">
 *          gum-utils</ulink>
 */

/**
 * GumUserCb:
 * @user: (transfer none): #GumUser object which is used in the request
 * @error: (transfer none): #GError object. In case of error, error will be
 * non-NULL
 * @user_data: user data passed onto the request
 *
 * #GumUserCb defines the callback which is used when user object is created,
 * added, deleted or updated.
 */

/**
 * GumUser:
 *
 * Opaque structure for the object.
 */

/**
 * GumUserClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

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
    GumdDaemon *offline_service;
    GumdDaemonUser *offline_user;
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
    PROP_OFFLINE,

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
    user->priv->op->cb_id = 0;
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
_on_user_remote_object_destroyed(
        GDBusProxy *proxy,
        gpointer user_data)
{
    g_return_if_fail (GUM_IS_USER (user_data));
    GumUser *self = GUM_USER (user_data);

    DBG("");

    GUM_OBJECT_UNREF (self->priv->dbus_user);
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumUser *self = GUM_USER (object);
    switch (property_id) {
        case PROP_OFFLINE: {
            if (g_value_get_boolean (value))
                self->priv->offline_service = gumd_daemon_new ();
            break;
        }
        default: {
            if (self->priv->offline_user) {
                g_object_set_property (G_OBJECT(self->priv->offline_user),
                        pspec->name, value);
            } else if (self->priv->dbus_user) {
                g_object_set_property (G_OBJECT(self->priv->dbus_user),
                        pspec->name, value);
            }
        }
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
    switch (property_id) {
        case PROP_OFFLINE: {
            g_value_set_boolean (value, self->priv->offline_service != NULL);
            break;
        }
        default: {
            if (self->priv->offline_user) {
                g_object_get_property (G_OBJECT(self->priv->offline_user),
                        pspec->name, value);
            } else if (self->priv->dbus_user) {
                g_object_get_property (G_OBJECT(self->priv->dbus_user),
                        pspec->name, value);
            }
        }
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

    if (self->priv->dbus_user) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->dbus_user),
                _on_user_remote_object_destroyed, self);
    }

    GUM_OBJECT_UNREF (self->priv->dbus_user);

    GUM_OBJECT_UNREF (self->priv->dbus_service);

    GUM_OBJECT_UNREF (self->priv->offline_user);

    GUM_OBJECT_UNREF (self->priv->offline_service);

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
    self->priv->offline_user = NULL;
    if (!self->priv->offline_service) {
        self->priv->cancellable = g_cancellable_new ();
        self->priv->dbus_service = gum_user_service_get_instance ();
    } else {
        self->priv->cancellable = NULL;
        self->priv->dbus_service = NULL;
    }
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

    /**
     * GumUser:usertype:
     *
     * This property holds a user type that the object corresponds to. Valid
     * values of user types are as specified in #GumUserType.
     * #GumUser:usertype must be specified when adding a new user.
     */
    properties[PROP_USERTYPE] =  g_param_spec_uint ("usertype",
            "UserType",
            "Type of the user",
            0,
            G_MAXUINT16,
            GUM_USERTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:uid:
     *
     * This property holds a unique user identity for the user as assigned by
     * ther underlying framework, which is always be in range [0, MAXUINT].
     */
    properties[PROP_UID] =  g_param_spec_uint ("uid",
            "Uid",
            "Unique identifier of the user",
            0,
            G_MAXUINT,
            GUM_USER_INVALID_UID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:gid:
     *
     * This property holds a unique group identity for the user as assigned by
     * the underlying framework, which is always be in range [0, MAXUINT].
     */
    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the user",
            0,
            G_MAXUINT,
            GUM_GROUP_INVALID_GID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:username:
     *
     * This property holds the name of given to the user when the user is
     * added. Allowed pattern for username is:
     * "^[A-Za-z_][A-Za-z0-9_.-]*[A-Za-z0-9_.$-]\\?$". Either #GumUser:username
     * or #GumUser:nickname must be specified when adding a new user.
     */
    properties[PROP_USERNAME] = g_param_spec_string ("username",
            "Username",
            "System name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:secret:
     *
     * This property holds the secret as chosen by the user. Secret should not
     * contain any control chars (0x00-0x1F,0x7F) or colon (':' 0x3A).
     */
    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "User secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:nickname:
     *
     * This property holds the nickname of given to the user. Nickname should
     * not contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c) or
     * colon (':' 0x3A). If no #GumUser:username is specified, nickname is
     * hashed to handle non-ascii characters and then set as #GumUser:username.
     */
    properties[PROP_NICKNAME] = g_param_spec_string ("nickname",
            "Nickname",
            "Nick name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:realname:
     *
     * This property holds the realname of given to the user. Realname should
     * not contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c) or
     * colon (':' 0x3A).
     */
    properties[PROP_REALNAME] = g_param_spec_string ("realname",
            "Realname",
            "Real name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:office:
     *
     * This property holds the location of the office of the user. Office
     * should not contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c)
     * or colon (':' 0x3A).
     */
    properties[PROP_OFFICE] = g_param_spec_string ("office",
            "Office",
            "Office location of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:officephone:
     *
     * This property holds the office phone of the user. Office phone
     * should not contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c)
     * or colon (':' 0x3A).
     */
    properties[PROP_OFFICEPHONE] = g_param_spec_string ("officephone",
            "OfficePhone",
            "Office phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:homephone:
     *
     * This property holds the home phone of the user. Home phone should not
     * contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c) or
     * colon (':' 0x3A).
     */
    properties[PROP_HOMEPHONE] = g_param_spec_string ("homephone",
            "HomePhone",
            "Home phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:homedir:
     *
     * This property holds the location of the home directory of the user. Home
     * directory should not contain any control chars (0x00-0x1F,0x7F),
     * comma (',' 0x2c) or colon (':' 0x3A). For new user, it is recommended to
     * let the underlying framework to set the home directory of the user based
     * on the configuration.
     */
    properties[PROP_HOMEDIR] = g_param_spec_string ("homedir",
            "HomeDir",
            "Home directory of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:shell:
     *
     * This property holds the shell path of the user. Shell path should not
     * contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c) or
     * colon (':' 0x3A). For new user, it is recommended to let the underlying
     * framework to set the home directory of the user based on the
     * configuration.
     */
    properties[PROP_SHELL] = g_param_spec_string ("shell",
            "Shell",
            "Shell",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:offline:
     *
     * This property is used to define the behaviour of synchronous operation
     * performed on the object. If it is set to TRUE, then the object performs
     * the operation without daemon (dbus/gumd) otherwise 'gumd' daemon will be
     * used for the required synchronous functionality.
     */
    properties[PROP_OFFLINE] =  g_param_spec_boolean ("offline",
            "Offline",
            "Operational mode for the User object",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);
}

static void
_create_dbus_user (
        GumUser *user,
        gchar *object_path,
        GError *error)
{
    user->priv->dbus_user = gum_dbus_user_proxy_new_sync (
            g_dbus_proxy_get_connection (G_DBUS_PROXY (
                    user->priv->dbus_service)),
            G_DBUS_PROXY_FLAGS_NONE, g_dbus_proxy_get_name (
            G_DBUS_PROXY (user->priv->dbus_service)), object_path,
            user->priv->cancellable, &error);
    if (!error) {
        g_signal_connect (G_OBJECT (user->priv->dbus_user), "unregistered",
            G_CALLBACK (_on_user_remote_object_destroyed),  user);
    }
}

static GVariant *
_get_prop_value (
        GVariant *props,
        const gchar *prop)
{
    GVariantIter *iter = NULL;
    GVariant *item = NULL;
    g_variant_get (props, "(a{sv})",  &iter);
    while ((item = g_variant_iter_next_value (iter)))  {
        gchar *key;
        GVariant *value;
        g_variant_get (item, "{sv}", &key, &value);
        if (g_strcmp0 (key, prop) == 0) {
            g_free (key);
            return g_variant_ref (value);
        }
        g_free (key); key = NULL;
        g_variant_unref (value); value = NULL;
    }
    return NULL;
}

static gboolean
_sync_properties (
        GumUser *user)
{
    GError *error = NULL;
    GVariant *result = NULL;

    /* load all properties synchronously */
    result = g_dbus_connection_call_sync (
            g_dbus_proxy_get_connection (
                    G_DBUS_PROXY (user->priv->dbus_service)),
            g_dbus_proxy_get_name (G_DBUS_PROXY (user->priv->dbus_service)),
            g_dbus_proxy_get_object_path (G_DBUS_PROXY (user->priv->dbus_user)),
            "org.freedesktop.DBus.Properties",
            "GetAll",
            g_variant_new ("(s)",
                    g_dbus_proxy_get_interface_name (
                            G_DBUS_PROXY (user->priv->dbus_user))),
            G_VARIANT_TYPE ("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            user->priv->cancellable,
            &error);

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        return FALSE;
    }

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(a{sv})"))) {
        guint n_properties = 0, ind = 0;
        GParamSpec **properties = g_object_class_list_properties (
                G_OBJECT_GET_CLASS(user), &n_properties);
        for (ind=0; ind < n_properties; ind++) {
            GParamSpec *pspec = properties[ind];
            GVariant *prop = _get_prop_value (result,  pspec->name);
            if (prop != NULL) {
                g_dbus_proxy_set_cached_property (
                        G_DBUS_PROXY (user->priv->dbus_user), pspec->name,
                        prop);
            }
        }
        g_free (properties);
    }
    g_variant_unref (result);
    return TRUE;
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
            _create_dbus_user (user, object_path, error);
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
            _create_dbus_user (user, object_path, error);
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
            _create_dbus_user (user, object_path, error);
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

/**
 * gum_user_create:
 * @callback: #GumUserCb to be invoked when new user object is created
 * @user_data: user data
 *
 * This method creates a new remote user object over the DBus asynchronously.
 * Callback is used to notify when the remote object is fully created and
 * accessible.
 *
 * Returns: (transfer full): #GumUser newly created object
 */
GumUser *
gum_user_create (
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER, "offline", FALSE,
            NULL));

    g_return_val_if_fail (user && user->priv->dbus_service != NULL, NULL);
    _create_op (user, callback, user_data);
    gum_dbus_user_service_call_create_new_user (user->priv->dbus_service,
            user->priv->cancellable, _on_new_user_cb, user);
    return user;
}

/**
 * gum_user_create_sync:
 * @offline: enables or disables the offline mode
 *
 * This method creates a new remote user object. In case offline mode is
 * enabled, then the object is created directly without using dbus otherwise
 * the objects gets created over the DBus synchronously.
 *
 * Returns: (transfer full): #GumUser newly created object
 */
GumUser *
gum_user_create_sync (
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;

    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER,  "offline", offline,
            NULL));

    if (!user) return NULL;

    if (user->priv->offline_service) {
        user->priv->offline_user = gumd_daemon_user_new (
                gumd_daemon_get_config (user->priv->offline_service));
        return user;
    }

    g_return_val_if_fail (user->priv->dbus_service != NULL, NULL);

    if (gum_dbus_user_service_call_create_new_user_sync (
                user->priv->dbus_service, &object_path, user->priv->cancellable,
                &error)) {
        _create_dbus_user (user, object_path, error);
    }

    g_free (object_path);

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (user);
        user = NULL;
    }

    return user;
}

/**
 * gum_user_get:
 * @uid: user id for the user
 * @callback: #GumUserCb to be invoked when user object is fetched
 * @user_data: user data
 *
 * This method gets the user object attached to uid over the DBus
 * asynchronously. Callback is used to notify when the remote object is fully
 * created and accessible.
 *
 * Returns: (transfer full): #GumUser object
 */
GumUser *
gum_user_get (
        uid_t uid,
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER, "offline", FALSE,
            NULL));

    g_return_val_if_fail (user && user->priv->dbus_service != NULL, NULL);

    _create_op (user, callback, user_data);
     gum_dbus_user_service_call_get_user (user->priv->dbus_service, uid,
            user->priv->cancellable, _on_get_user_cb, user);

    return user;
}

/**
 * gum_user_get_sync:
 * @uid: user id for the user
 * @offline: enables or disables the offline mode
 *
 * This method gets the user object attached to uid. In case offline mode is
 * enabled, then the object is retrieved directly without using dbus otherwise
 * the objects gets retrieved over the DBus synchronously.
 *
 * Returns: (transfer full): #GumUser object
 */
GumUser *
gum_user_get_sync (
        uid_t uid,
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;

    GumUser *user = GUM_USER (g_object_new (GUM_TYPE_USER,  "offline", offline,
            NULL));

    if (!user) return NULL;

    if (user->priv->offline_service) {

        user->priv->offline_user = gumd_daemon_get_user (
                user->priv->offline_service, uid, &error);
    } else if (user->priv->dbus_service) {

        if (gum_dbus_user_service_call_get_user_sync (user->priv->dbus_service,
                uid, &object_path, user->priv->cancellable, &error)) {
            _create_dbus_user (user, object_path, error);
        }
        g_free (object_path);
    }

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (user);
        user = NULL;
    }
    return user;
}

/**
 * gum_user_get_by_name:
 * @username: name of the user
 * @callback: #GumUserCb to be invoked when user object is fetched
 * @user_data: user data
 *
 * This method gets the user object attached to username over the DBus
 * asynchronously. Callback is used to notify when the remote object is fully
 * created and accessible.
 *
 * Returns: (transfer full): #GumUser object
 */
GumUser *
gum_user_get_by_name (
        const gchar *username,
        GumUserCb callback,
        gpointer user_data)
{
    GumUser *user = NULL;

    if (!username) {
        WARN ("username not specified");
        return NULL;
    }

    user = GUM_USER (g_object_new (GUM_TYPE_USER, "offline", FALSE, NULL));

    g_return_val_if_fail (user && user->priv->dbus_service != NULL, NULL);

    _create_op (user, callback, user_data);
    gum_dbus_user_service_call_get_user_by_name (user->priv->dbus_service,
            username, user->priv->cancellable, _on_get_user_by_name_cb, user);
    return user;
}

/**
 * gum_user_get_by_name_sync:
 * @username: name of the user
 * @offline: enables or disables the offline mode
 *
 * This method gets the user object attached to username. In case offline mode
 * is enabled, then the object is retrieved directly without using dbus
 * otherwise the objects gets retrieved over the DBus synchronously.
 *
 * Returns: (transfer full): #GumUser object
 */
GumUser *
gum_user_get_by_name_sync (
        const gchar *username,
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;
    GumUser *user = NULL;

    if (!username) {
        WARN ("username not specified");
        return NULL;
    }

    user = GUM_USER (g_object_new (GUM_TYPE_USER,  "offline", offline, NULL));

    if (!user) return NULL;

    if (user->priv->offline_service) {
        user->priv->offline_user = gumd_daemon_get_user_by_name (
                user->priv->offline_service, username, &error);

    } else if (user->priv->dbus_service) {
        if (gum_dbus_user_service_call_get_user_by_name_sync (
                user->priv->dbus_service, username, &object_path,
                user->priv->cancellable,  &error)) {
            _create_dbus_user (user, object_path, error);
        }
        g_free (object_path);
    }

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (user);
        user = NULL;
    }
    return user;
}

/**
 * gum_user_add:
 * @self: #GumUser object to be added; object should have either valid
 * #GumUser:username or #GumUser:nickname in addition to valid
 * #GumUser:usertype.
 * @callback: #GumUserCb to be invoked when user is added
 * @user_data: user data
 *
 * This method adds the user over the DBus asynchronously. Callback is used to
 * notify when the user is added.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_user_add (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (!self->priv->dbus_user) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_add_user (self->priv->dbus_user, self->priv->cancellable,
            _on_user_add_cb, self);
    return TRUE;
}

/**
 * gum_user_add_sync:
 * @self: #GumUser object to be added; object should have either valid
 * #GumUser:username or #GumUser:nickname in addition to valid
 * #GumUser:usertype.
 *
 * This method adds the user. In case offline mode is enabled, then the user is
 * added directly without using dbus otherwise the user is added over the
 * DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_user_add_sync (
        GumUser *self)
{
    GError *error = NULL;
    uid_t uid = GUM_USER_INVALID_UID;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (self->priv->offline_user) {
        rval = gumd_daemon_add_user (self->priv->offline_service,
                self->priv->offline_user, &error);
    } else if (self->priv->dbus_user) {
        rval = gum_dbus_user_call_add_user_sync (self->priv->dbus_user,
                &uid, self->priv->cancellable,  &error);
        if (rval) _sync_properties (self);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_user_delete:
 * @self: #GumUser object to be deleted; object should have valid #GumUser:uid
 * property.
 * @rem_home_dir: deletes home directory of the user if set to TRUE
 * @callback: #GumUserCb to be invoked when user is deleted
 * @user_data: user data
 *
 * This method deletes the user over the DBus asynchronously. Callback is used
 * to notify when the user is deleted.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
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
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_delete_user (self->priv->dbus_user, rem_home_dir,
            self->priv->cancellable, _on_user_delete_cb, self);
    return TRUE;
}

/**
 * gum_user_delete_sync:
 * @self: #GumUser object to be deleted; object should have valid #GumUser:uid
 * property.
 * @rem_home_dir: deletes home directory of the user if set to TRUE
 *
 * This method deletes the user. In case offline mode is enabled, then the user
 * is deleted directly without using dbus otherwise the user is deleted over the
 * DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_user_delete_sync (
        GumUser *self,
        gboolean rem_home_dir)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (self->priv->offline_user) {
        rval = gumd_daemon_delete_user (self->priv->offline_service,
                self->priv->offline_user, rem_home_dir, &error);
    } else if (self->priv->dbus_user) {
        rval = gum_dbus_user_call_delete_user_sync (self->priv->dbus_user,
                rem_home_dir, self->priv->cancellable,  &error);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_user_update:
 * @self: #GumUser object to be updated; object should have valid #GumUser:uid
 * property.
 * @callback: #GumUserCb to be invoked when user is updated
 * @user_data: user data
 *
 * This method updates the user over the DBus asynchronously. Callback is used
 * to notify when the user is updated. The properties which can be updated
 * are: secret, realname, office, officephone, homephone and shell.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_user_update (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (!self->priv->dbus_user) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_user_call_update_user (self->priv->dbus_user,
            self->priv->cancellable, _on_user_update_cb, self);
    return TRUE;
}

/**
 * gum_user_update_sync:
 * @self: #GumUser object to be updated; object should have valid #GumUser:uid
 * property.
 *
 * This method updates the user. In case offline mode is enabled, then the user
 * is updated directly without using dbus otherwise the user is updated over the
 * DBus synchronously.
 * The properties which can be updated are: secret, realname, office,
 * officephone, homephone and shell.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_user_update_sync (
        GumUser *self)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_USER (self), FALSE);

    if (self->priv->offline_user) {
        rval = gumd_daemon_update_user (self->priv->offline_service,
                self->priv->offline_user, &error);
    } else if (self->priv->dbus_user) {
        rval = gum_dbus_user_call_update_user_sync (self->priv->dbus_user,
                self->priv->cancellable,  &error);
        if (rval) _sync_properties (self);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}
