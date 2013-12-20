/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gumd
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
#include "common/gum-utils.h"
#include "common/gum-dbus.h"
#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

#include "gumd-dbus-user-adapter.h"
#include "daemon/gumd-daemon.h"

enum
{
    PROP_0,

    PROP_USER,
    PROP_CONNECTION,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GumdDbusUserAdapterPrivate
{
    GumdDaemonUser *user;
    GDBusConnection *connection;
    GumDbusUser *dbus_user;
    const gchar *prop_name_in_change;
    GumdDaemon *daemon;
};

G_DEFINE_TYPE (GumdDbusUserAdapter, gumd_dbus_user_adapter, \
        GUM_TYPE_DISPOSABLE)

#define GUMD_DBUS_USER_ADAPTER_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), GUMD_TYPE_USER_ADAPTER,\
            GumdDbusUserAdapterPrivate)

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (object);

    switch (property_id) {
        case PROP_USER: {
            GObject *user = g_value_get_object (value);
            if (user) {
                if (self->priv->user) g_object_unref (self->priv->user);
                self->priv->user = GUMD_DAEMON_USER (user);
            }
            break;
        }
        case PROP_CONNECTION: {
            self->priv->connection = g_value_get_object(value);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (object);

    switch (property_id) {
        case PROP_USER: {
            g_value_set_object (value, self->priv->user);
            break;
        }
        case PROP_CONNECTION: {
            g_value_set_object (value, self->priv->connection);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (
        GObject *object)
{
    GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (object);

    GUM_OBJECT_UNREF (self->priv->daemon);
    GUM_OBJECT_UNREF (self->priv->user);

    if (self->priv->dbus_user) {
        GDBusInterfaceSkeleton *iface = G_DBUS_INTERFACE_SKELETON(
                self->priv->dbus_user);
        gum_dbus_user_emit_unregistered (self->priv->dbus_user);
        DBG("(-)'%s' object unexported",
                g_dbus_interface_skeleton_get_object_path (iface));
        g_dbus_interface_skeleton_unexport (iface);
        g_object_unref (self->priv->dbus_user);
        self->priv->dbus_user = NULL;
    }

    GUM_OBJECT_UNREF (self->priv->connection);

    G_OBJECT_CLASS (gumd_dbus_user_adapter_parent_class)->dispose (object);
}

static void
_finalize (
        GObject *object)
{
    //GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (object);

    G_OBJECT_CLASS (gumd_dbus_user_adapter_parent_class)->finalize (object);
}

static void
gumd_dbus_user_adapter_class_init (
        GumdDbusUserAdapterClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumdDbusUserAdapterPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_USER] = g_param_spec_object ("user",
            "User",
            "User object",
            GUMD_TYPE_DAEMON_USER,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_CONNECTION] = g_param_spec_object ("connection",
            "Bus connection",
            "DBus connection used",
            G_TYPE_DBUS_CONNECTION,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}


static void
_on_dbus_property_changed(
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (user_data);
    if (g_strcmp0 (self->priv->prop_name_in_change, pspec->name) == 0)
        return;

    self->priv->prop_name_in_change = pspec->name;
    if (g_object_class_find_property (G_OBJECT_GET_CLASS(self->priv->user),
            pspec->name) != NULL) {
        DBG("Dbus property %s changed for %p", pspec->name, self);
        GValue value = G_VALUE_INIT;
        g_value_init (&value, pspec->value_type);
        g_object_get_property (G_OBJECT(self->priv->dbus_user),
                pspec->name, &value);
        g_object_set_property (G_OBJECT(self->priv->user), pspec->name,
                &value);
        g_value_unset (&value);
    }
    self->priv->prop_name_in_change = NULL;
}

static void
_on_user_property_changed(
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    GumdDbusUserAdapter *self = GUMD_DBUS_USER_ADAPTER (user_data);
    if (g_strcmp0 (self->priv->prop_name_in_change, pspec->name) == 0)
        return;
    self->priv->prop_name_in_change = pspec->name;
    if (g_object_class_find_property (G_OBJECT_GET_CLASS(self->priv->dbus_user),
            pspec->name) != NULL) {
        DBG("User property %s changed for %p", pspec->name, self);
        GValue value = G_VALUE_INIT;
        g_value_init (&value, pspec->value_type);
        g_object_get_property (G_OBJECT(self->priv->user),
                pspec->name, &value);
        g_object_set_property (G_OBJECT(self->priv->dbus_user), pspec->name,
                &value);
        g_value_unset (&value);
    }
    self->priv->prop_name_in_change = NULL;
}

static void
_sync_user_properties(
        GObject *src_object,
        GObject *dest_object)
{
    guint n_properties = 0, ind = 0;
    GParamSpec **properties = g_object_class_list_properties (
            G_OBJECT_GET_CLASS(src_object), &n_properties);
    for (ind=0; ind < n_properties; ind++) {
        GParamSpec *pspec = properties[ind];
        if (g_object_class_find_property (G_OBJECT_GET_CLASS(dest_object),
                pspec->name) != NULL) {
            GValue value = G_VALUE_INIT;
            g_value_init (&value, pspec->value_type);
            g_object_get_property (G_OBJECT(src_object),
                    pspec->name, &value);
            g_object_set_property (G_OBJECT(dest_object), pspec->name,
                    &value);
            g_value_unset (&value);
        }
    }
    g_free (properties);
}

static gboolean
_handle_add_user (
        GumdDbusUserAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer user_data)
{
    GError *error = NULL;
    gboolean rval = FALSE;
    uid_t uid = GUM_USER_INVALID_UID;

    g_return_val_if_fail (self && GUMD_IS_DBUS_USER_ADAPTER(self), FALSE);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    g_object_freeze_notify (G_OBJECT(self->priv->dbus_user));
    rval = gumd_daemon_add_user (self->priv->daemon, self->priv->user, &error);
    g_object_thaw_notify (G_OBJECT(self->priv->dbus_user));

    if (rval) {
        g_dbus_interface_skeleton_flush (
                G_DBUS_INTERFACE_SKELETON(self->priv->dbus_user));
        g_object_get (G_OBJECT (self->priv->user), "uid", &uid, NULL);
        gum_dbus_user_complete_add_user (self->priv->dbus_user, invocation,
                uid);
    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

static gboolean
_handle_delete_user (
        GumdDbusUserAdapter *self,
        GDBusMethodInvocation *invocation,
        gboolean rem_home_dir,
        gpointer user_data)
{
    GError *error = NULL;

    g_return_val_if_fail (self && GUMD_IS_DBUS_USER_ADAPTER(self), FALSE);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);
    if (gumd_daemon_delete_user (self->priv->daemon, self->priv->user,
            rem_home_dir, &error)) {
        gum_dbus_user_complete_delete_user (self->priv->dbus_user, invocation);
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
        /* delete successful so not needed anymore */
        gum_disposable_delete_later (GUM_DISPOSABLE (self));
    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
    }

    return TRUE;
}

static gboolean
_handle_update_user (
        GumdDbusUserAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer user_data)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    g_return_val_if_fail (self && GUMD_IS_DBUS_USER_ADAPTER(self), FALSE);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    g_object_freeze_notify (G_OBJECT(self->priv->dbus_user));
    rval = gumd_daemon_update_user (self->priv->daemon, self->priv->user,
            &error);
    g_object_thaw_notify (G_OBJECT(self->priv->dbus_user));

    if (rval) {
        g_dbus_interface_skeleton_flush (
                G_DBUS_INTERFACE_SKELETON(self->priv->dbus_user));
        gum_dbus_user_complete_update_user (self->priv->dbus_user, invocation);
    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

static void
gumd_dbus_user_adapter_init (
        GumdDbusUserAdapter *self)
{
    self->priv = GUMD_DBUS_USER_ADAPTER_GET_PRIV(self);

    self->priv->connection = NULL;
    self->priv->user = NULL;
    self->priv->dbus_user = gum_dbus_user_skeleton_new ();
    self->priv->prop_name_in_change = NULL;
    self->priv->daemon = gumd_daemon_new ();
}

GumdDbusUserAdapter *
gumd_dbus_user_adapter_new (
        GumdDaemonUser *user,
        guint timeout)
{
    GError *error = NULL;
    GDBusConnection *connection = g_bus_get_sync (GUM_BUS_TYPE, NULL,
            &error);
    if (error) {
        WARN("failed to connect to session bus : %s", error->message);
        g_error_free (error);
        return NULL;
    }

    return gumd_dbus_user_adapter_new_with_connection (connection, user,
            timeout);
}

GumdDbusUserAdapter *
gumd_dbus_user_adapter_new_with_connection (
        GDBusConnection *bus_connection,
        GumdDaemonUser *user,
        guint timeout)
{
    GError *err = NULL;
    static guint32 object_counter = 0;
    gchar *nonce = NULL;
    gchar *object_path = NULL;

    GumdDbusUserAdapter *adapter = GUMD_DBUS_USER_ADAPTER (
        g_object_new (GUMD_TYPE_USER_ADAPTER, "user", user,
            "connection", bus_connection, NULL));
    _sync_user_properties (G_OBJECT (adapter->priv->user),
            G_OBJECT (adapter->priv->dbus_user));

    nonce = gum_utils_generate_nonce (G_CHECKSUM_SHA1);
    object_path = g_strdup_printf ("%s/User_%s_%d",
            GUM_USER_SERVICE_OBJECTPATH, nonce, object_counter++);
    g_free (nonce);

    if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON(adapter->priv->dbus_user),
            adapter->priv->connection, object_path, &err)) {
        WARN ("failed to register object: %s", err->message);
        g_error_free (err);
        g_free (object_path);
        g_object_unref (adapter);
        return NULL;
    }

    DBG("(+) started user interface '%p' at path '%s' on connection '%p'",
            adapter, object_path, bus_connection);
    g_free (object_path);

    if (timeout) {
        gum_disposable_set_timeout (GUM_DISPOSABLE (adapter), timeout);
    }

    g_signal_connect_swapped (adapter->priv->dbus_user, "handle-add-user",
            G_CALLBACK (_handle_add_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_user, "handle-delete-user",
            G_CALLBACK (_handle_delete_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_user, "handle-update-user",
            G_CALLBACK (_handle_update_user), adapter);

    g_signal_connect (G_OBJECT (adapter->priv->dbus_user), "notify",
            G_CALLBACK (_on_dbus_property_changed), adapter);
    g_signal_connect (G_OBJECT (adapter->priv->user), "notify",
            G_CALLBACK (_on_user_property_changed), adapter);

    return adapter;
}

const gchar *
gumd_dbus_user_adapter_get_object_path(
        GumdDbusUserAdapter *self)
{
    g_return_val_if_fail (self && GUMD_IS_DBUS_USER_ADAPTER (self), NULL);

    return g_dbus_interface_skeleton_get_object_path (
                G_DBUS_INTERFACE_SKELETON (self->priv->dbus_user));
}

uid_t
gumd_dbus_user_adapter_get_uid (
        GumdDbusUserAdapter *self)
{
    uid_t uid = GUM_USER_INVALID_UID;
    g_object_get (G_OBJECT (self->priv->user), "uid", &uid, NULL);
    return uid;
}
