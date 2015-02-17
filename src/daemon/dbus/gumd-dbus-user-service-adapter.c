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

#include "config.h"
#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-dbus.h"
#include "common/gum-defines.h"
#include "common/gum-string-utils.h"

#include "gumd-dbus-user-service-adapter.h"
#include "gumd-dbus-user-adapter.h"

enum
{
    PROP_0,

    PROP_CONNECTION,
    PROP_SERVER_BUSTYPE,
    PROP_DAEMON,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

typedef struct
{
    gchar *peer_name;
    GumdDbusUserAdapter *user_adapter;
    GumdDbusUserServiceAdapter *user_service;
}PeerUserService;

struct _GumdDbusUserServiceAdapterPrivate
{
    GDBusConnection *connection;
    GumDbusUserService *dbus_user_service;
    GumdDaemon  *daemon;
    GumdDbusServerBusType  dbus_server_type;
    GList *peer_users;
    GHashTable *caller_watchers; //(dbus_caller:watcher_id)
};

G_DEFINE_TYPE (GumdDbusUserServiceAdapter, gumd_dbus_user_service_adapter, \
        GUM_TYPE_DISPOSABLE)

#define GUMD_DBUS_USER_SERVICE_ADAPTER_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), GUMD_TYPE_USER_SERVICE_ADAPTER, \
            GumdDbusUserServiceAdapterPrivate)

static gboolean
_handle_create_new_user (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer user_data);

static gboolean
_handle_get_user (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        guint32 uid,
        gpointer user_data);

static gboolean
_handle_get_user_by_name (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *username,
        gpointer user_data);

static gboolean
_handle_get_user_list (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *const *types,
        gpointer user_data);

static void
_on_dbus_user_adapter_disposed (
        gpointer data,
        GObject *object);

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (object);

    switch (property_id) {
        case PROP_DAEMON: {
            self->priv->daemon = g_value_dup_object(value);
            break;
        }
        case PROP_CONNECTION:
            self->priv->connection = g_value_dup_object(value);
            break;
        case PROP_SERVER_BUSTYPE:
            self->priv->dbus_server_type =
                    (GumdDbusServerBusType)g_value_get_uint(value);
            break;
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
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (object);

    switch (property_id) {
        case PROP_DAEMON: {
            g_value_set_object (value, self->priv->daemon);
            break;
        }
        case PROP_CONNECTION:
            g_value_set_object (value, self->priv->connection);
            break;
        case PROP_SERVER_BUSTYPE:
            g_value_set_uint (value, (guint)self->priv->dbus_server_type);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_on_user_added (
        GObject *object,
        guint uid,
        gpointer user_data)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (
            user_data);
    gum_dbus_user_service_emit_user_added (self->priv->dbus_user_service, uid);
}

static void
_on_user_deleted (
        GObject *object,
        guint uid,
        gpointer user_data)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (
            user_data);
    gum_dbus_user_service_emit_user_deleted (self->priv->dbus_user_service,
            uid);
}

static void
_on_user_updated (
        GObject *object,
        guint uid,
        gpointer user_data)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (
            user_data);
    gum_dbus_user_service_emit_user_updated (self->priv->dbus_user_service,
            uid);
}

static PeerUserService *
_dbus_peer_user_new (
        GumdDbusUserServiceAdapter *self,
        gchar *peer_name,
        GumdDbusUserAdapter *user_adapter)
{
    PeerUserService *peer_user = g_malloc0 (sizeof (PeerUserService));
    peer_user->peer_name = peer_name;
    peer_user->user_adapter = user_adapter;
    peer_user->user_service = self;
    return peer_user;
}

static void
_dbus_peer_user_free (
        PeerUserService *peer_user,
        gpointer user_data)
{
    if (peer_user) {
        GUM_STR_FREE (peer_user->peer_name);
        GUM_OBJECT_UNREF (peer_user->user_adapter);
        peer_user->user_service = NULL;
        g_free (peer_user);
    }
}

static void
_dbus_peer_user_remove (
        PeerUserService *peer_user,
        gpointer user_data)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (
            user_data);
    if (peer_user && GUMD_IS_DBUS_USER_ADAPTER(peer_user->user_adapter)) {
        g_object_weak_unref (G_OBJECT (peer_user->user_adapter),
                _on_dbus_user_adapter_disposed, user_data);
        _dbus_peer_user_free (peer_user, NULL);
        self->priv->peer_users = g_list_remove (self->priv->peer_users,
                (gpointer)peer_user);
    }
}

static void
_dispose (
        GObject *object)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (object);

    DBG("- unregistering dbus user service adapter (%p). %d", object,
            G_OBJECT (self->priv->daemon)->ref_count);

    if (self->priv->peer_users) {
        g_list_foreach (self->priv->peer_users, (GFunc)_dbus_peer_user_remove,
                self);
    }

    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_user_added, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_user_deleted, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_user_updated, self);

    GUM_OBJECT_UNREF (self->priv->daemon);

    if (self->priv->dbus_user_service) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (
                self->priv->dbus_user_service));
        g_object_unref (self->priv->dbus_user_service);
        self->priv->dbus_user_service = NULL;
    }

    if (self->priv->connection) {
        /* NOTE: There seems to be some bug in glib's dbus connection's
         * https://bugzilla.gnome.org/show_bug.cgi?id=734281
         * worker thread such that it does not closes the stream. The problem
         * is hard to be tracked exactly as it is more of timing issue.
         * Following code snippet at least closes the stream to avoid any
         * descriptors leak.
         * */
        GIOStream *stream = g_dbus_connection_get_stream (
                self->priv->connection);
        if (stream) g_io_stream_close (stream, NULL, NULL);
        g_object_unref (self->priv->connection);
        self->priv->connection = NULL;
    }

    GUM_HASHTABLE_UNREF (self->priv->caller_watchers);

    G_OBJECT_CLASS (gumd_dbus_user_service_adapter_parent_class)->dispose (
            object);
}

static void
_finalize (
        GObject *object)
{
    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (object);

    if (self->priv->peer_users) {
        g_list_free (self->priv->peer_users);
        self->priv->peer_users = NULL;
    }

    G_OBJECT_CLASS (gumd_dbus_user_service_adapter_parent_class)->finalize (
            object);
}

static void
gumd_dbus_user_service_adapter_class_init (
        GumdDbusUserServiceAdapterClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (GumdDbusUserServiceAdapterPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_DAEMON] = g_param_spec_object (
            "daemon",
            "Daemon",
            "Daemon object",
            GUMD_TYPE_DAEMON,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_CONNECTION] = g_param_spec_object (
            "connection",
            "Bus connection",
            "DBus connection used",
            G_TYPE_DBUS_CONNECTION,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_SERVER_BUSTYPE] =  g_param_spec_uint ("bustype",
            "BusType",
            "DBus server bus type",
            0,
            G_MAXUINT,
            GUMD_DBUS_SERVER_BUSTYPE_NONE /* default value */,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
            G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gumd_dbus_user_service_adapter_init (GumdDbusUserServiceAdapter *self)
{
    self->priv = GUMD_DBUS_USER_SERVICE_ADAPTER_GET_PRIV(self);

    self->priv->connection = 0;
    self->priv->daemon = NULL;
    self->priv->peer_users = NULL;
    self->priv->dbus_user_service = gum_dbus_user_service_skeleton_new ();
    self->priv->caller_watchers = g_hash_table_new_full (g_str_hash,
            g_str_equal, g_free, (GDestroyNotify)g_bus_unwatch_name);
}

static void
_clear_cache_for_peer_name (
        PeerUserService *peer_user,
        PeerUserService *user_data)
{
    g_return_if_fail (peer_user && user_data);
    g_return_if_fail (GUMD_IS_DBUS_USER_ADAPTER (peer_user->user_adapter));

    if (g_strcmp0 (peer_user->peer_name, user_data->peer_name) == 0) {
        DBG ("removing dbus user '%p' for peer name %s from cache",
                peer_user->user_adapter, peer_user->peer_name);
        g_object_weak_unref (G_OBJECT (peer_user->user_adapter),
                _on_dbus_user_adapter_disposed, peer_user->user_service);
        _dbus_peer_user_free (peer_user, NULL);
        user_data->user_service->priv->peer_users = g_list_remove (
                user_data->user_service->priv->peer_users,
                (gpointer)peer_user);
    }
}

static void
_on_bus_name_lost (
        GDBusConnection *conn,
        const char *peer_name,
        gpointer user_data)
{
    PeerUserService peer_user;

    g_return_if_fail (peer_name && user_data &&
            GUMD_IS_DBUS_USER_SERVICE_ADAPTER (user_data));

    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (
            user_data);
    DBG ("(-)peer disappeared : %s", peer_name);

    peer_user.peer_name = (gchar *)peer_name;
    peer_user.user_adapter = NULL;
    peer_user.user_service = self;
    g_list_foreach (self->priv->peer_users, (GFunc)_clear_cache_for_peer_name,
            (gpointer)&peer_user);

    g_hash_table_remove (self->priv->caller_watchers, (gpointer)peer_name);
}

static void
_add_bus_name_watcher (
        GumdDbusUserServiceAdapter *self,
        GumdDbusUserAdapter *user_adapter,
        GDBusMethodInvocation *invocation)
{
    if (self->priv->dbus_server_type == GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS) {
        const gchar *sender = g_dbus_method_invocation_get_sender (invocation);
        GDBusConnection *connection = g_dbus_method_invocation_get_connection (
                    invocation);
        if (!g_hash_table_contains (self->priv->caller_watchers, sender)) {
            guint watcher_id = g_bus_watch_name_on_connection (connection,
                    sender, G_BUS_NAME_WATCHER_FLAGS_NONE, NULL,
                    _on_bus_name_lost, self, NULL);
            g_hash_table_insert (self->priv->caller_watchers,
                    (gpointer)g_strdup (sender),
                    GUINT_TO_POINTER(watcher_id));
        }
    }
}

static void
_clear_cache_for_user (
        PeerUserService *peer_user,
        PeerUserService *user_data)
{
    g_return_if_fail (peer_user && user_data);
    g_return_if_fail (GUMD_IS_DBUS_USER_ADAPTER (peer_user->user_adapter));

    if (peer_user->user_adapter == user_data->user_adapter) {
        DBG ("removing dbus user adapter '%p' from cache",
                peer_user->user_adapter);
        peer_user->user_adapter = NULL;
        _dbus_peer_user_free (peer_user, NULL);
        user_data->user_service->priv->peer_users = g_list_remove (
                user_data->user_service->priv->peer_users,
                (gpointer)peer_user);
    }
}

static void
_on_dbus_user_adapter_disposed (
        gpointer data,
        GObject *object)
{
    PeerUserService peer_user;

    GumdDbusUserServiceAdapter *self = GUMD_DBUS_USER_SERVICE_ADAPTER (data);

    DBG ("Dbus user adapter object %p disposed", object);

    peer_user.user_adapter = GUMD_DBUS_USER_ADAPTER (object);
    peer_user.user_service = self;
    g_list_foreach (self->priv->peer_users, (GFunc)_clear_cache_for_user,
            (gpointer)&peer_user);

    if (g_list_length (self->priv->peer_users) == 0) {
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
    }
}

static gchar *
_get_sender (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation)
{
    gchar *sender = NULL;
    if (self->priv->dbus_server_type == GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS) {
        sender = g_strdup (g_dbus_method_invocation_get_sender (invocation));
    } else {
        GDBusConnection *connection =  g_dbus_method_invocation_get_connection (
                invocation);
        sender = g_strdup_printf ("%d", g_socket_get_fd (
                g_socket_connection_get_socket (G_SOCKET_CONNECTION (
                        g_dbus_connection_get_stream(connection)))));
    }
    return sender;
}

static GumdDbusUserAdapter *
_create_and_cache_user_adapter (
        GumdDbusUserServiceAdapter *self,
        GumdDaemonUser *user,
        GDBusMethodInvocation *invocation)
{
    GDBusConnection *connection = g_dbus_method_invocation_get_connection (
            invocation);

    GumdDbusUserAdapter *user_adapter =
            gumd_dbus_user_adapter_new_with_connection (
                    connection, user,
                    gumd_daemon_get_user_timeout (self->priv->daemon));

    /* keep alive till this user object gets disposed */
    if (g_list_length (self->priv->peer_users) == 0)
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    self->priv->peer_users = g_list_append (self->priv->peer_users,
            _dbus_peer_user_new (self, _get_sender (self, invocation),
                    user_adapter));
    g_object_weak_ref (G_OBJECT (user_adapter), _on_dbus_user_adapter_disposed,
            self);

    /* watchers used for msg-bus only */
    _add_bus_name_watcher (self, user_adapter, invocation);

    DBG ("created user adapter %p for user %p", user_adapter, user);
    return user_adapter;
}

static GumdDbusUserAdapter *
_get_user_adapter_from_cache (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        uid_t uid)
{
    GumdDbusUserAdapter *user_adapter = NULL;
    PeerUserService *peer_user = NULL;
    GList *list = self->priv->peer_users;
    gchar *peer_name = NULL;
    gboolean delete_later = FALSE;

    if (uid == GUM_USER_INVALID_UID) {
        return NULL;
    }

    peer_name = _get_sender (self, invocation);
    DBG ("peername:%s uid %u", peer_name, uid);
    for ( ; list != NULL; list = g_list_next (list)) {
        peer_user = (PeerUserService *) list->data;
        if (g_strcmp0 (peer_name, peer_user->peer_name) == 0 &&
            gumd_dbus_user_adapter_get_uid (peer_user->user_adapter) == uid) {

            g_object_get (G_OBJECT (peer_user->user_adapter), "delete-later",
                    &delete_later, NULL);
            if (!delete_later) {
                user_adapter = peer_user->user_adapter;
                break;
            }
        }
    }
    g_free (peer_name);

    DBG ("user adapter %p", user_adapter);
    return user_adapter;
}

static gboolean
_handle_create_new_user (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer user_data)
{
    GumdDaemonUser *user = NULL;
    GError *error = NULL;

    g_return_val_if_fail (self && GUMD_IS_DBUS_USER_SERVICE_ADAPTER(self),
            FALSE);
    DBG ("");

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    user = gumd_daemon_user_new (gumd_daemon_get_config (self->priv->daemon));
    if (user) {
        GumdDbusUserAdapter *user_adapter = _create_and_cache_user_adapter (
                self, user, invocation);
        gum_dbus_user_service_complete_create_new_user (
                self->priv->dbus_user_service, invocation,
                gumd_dbus_user_adapter_get_object_path (user_adapter));
    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }
    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
    
    return TRUE;
}

static gboolean
_handle_get_user (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        guint32 uid,
        gpointer user_data)
{
    GumdDaemonUser *user = NULL;
    GError *error = NULL;
    GumdDbusUserAdapter *user_adapter = NULL;
    DBG ("uid %d", uid);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    user_adapter = _get_user_adapter_from_cache (self, invocation, uid);
    if (!user_adapter) {
    	user = gumd_daemon_get_user (self->priv->daemon, (uid_t)uid, &error);
    	if (user) {
            user_adapter = _create_and_cache_user_adapter (self, user,
                invocation);
    	}
    }

    if (user_adapter) {
        gum_dbus_user_service_complete_get_user (self->priv->dbus_user_service,
                invocation, gumd_dbus_user_adapter_get_object_path (
                        user_adapter));
    } else {
        if (!error) {
            error = GUM_GET_ERROR_FOR_ID (GUM_ERROR_USER_NOT_FOUND,
                    "User Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

static gboolean
_handle_get_user_by_name (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *username,
        gpointer user_data)
{
    GumdDaemonUser *user = NULL;
    GError *error = NULL;
    GumdDbusUserAdapter *user_adapter = NULL;
    uid_t uid = GUM_USER_INVALID_UID;

    DBG ("username %s", username);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    uid = gumd_daemon_user_get_uid_by_name (username, gumd_daemon_get_config (
    		self->priv->daemon));

    user_adapter = _get_user_adapter_from_cache (self, invocation, uid);
    if (!user_adapter) {
        user = gumd_daemon_get_user (self->priv->daemon, (uid_t)uid, &error);
        if (user) {
            user_adapter = _create_and_cache_user_adapter (self, user,
                    invocation);
        }
    }

    if (user_adapter) {
        gum_dbus_user_service_complete_get_user_by_name (
        		self->priv->dbus_user_service, invocation,
                gumd_dbus_user_adapter_get_object_path (user_adapter));
    } else {
        if (!error) {
            error = GUM_GET_ERROR_FOR_ID (GUM_ERROR_USER_NOT_FOUND,
                    "User Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

static gboolean
_handle_get_user_list (
        GumdDbusUserServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *const *types,
        gpointer user_data)
{
    GError *error = NULL;
    GVariant *users = NULL;

    DBG ("");

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    users = gumd_daemon_get_user_list (self->priv->daemon, types, &error);

    if (users) {
        gum_dbus_user_service_complete_get_user_list (
                self->priv->dbus_user_service, invocation, users);
    } else {
        if (!error) {
            error = GUM_GET_ERROR_FOR_ID (GUM_ERROR_USER_NOT_FOUND,
                    "Users Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

GumdDbusUserServiceAdapter *
gumd_dbus_user_service_adapter_new_with_connection (
        GDBusConnection *bus_connection,
        GumdDaemon *daemon,
        GumdDbusServerBusType bus_type)
{
    GError *err = NULL;
    guint timeout = 0;
    GumdDbusUserServiceAdapter *adapter = GUMD_DBUS_USER_SERVICE_ADAPTER (
        g_object_new (GUMD_TYPE_USER_SERVICE_ADAPTER,
            "daemon", daemon,
            "bustype", bus_type,
            "connection", bus_connection,
            NULL));

    if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON(adapter->priv->dbus_user_service),
            adapter->priv->connection, GUM_USER_SERVICE_OBJECTPATH, &err)) {
        WARN ("failed to register object: %s", err->message);
        g_error_free (err);
        g_object_unref (adapter);
        return NULL;
    }
    DBG("(+) started user service interface '%p' at path '%s' on connection"
            " '%p'", adapter, GUM_USER_SERVICE_OBJECTPATH, bus_connection);

    timeout = gumd_daemon_get_timeout (adapter->priv->daemon);
    if (timeout && bus_type != GUMD_DBUS_SERVER_BUSTYPE_P2P) {
        gum_disposable_set_timeout (GUM_DISPOSABLE (adapter), timeout);
    }

    g_signal_connect_swapped (adapter->priv->dbus_user_service,
        "handle-create-new-user", G_CALLBACK (_handle_create_new_user),
        adapter);
    g_signal_connect_swapped (adapter->priv->dbus_user_service,
        "handle-get-user", G_CALLBACK(_handle_get_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_user_service,
        "handle-get-user-by-name", G_CALLBACK(_handle_get_user_by_name),
        adapter);
    g_signal_connect_swapped (adapter->priv->dbus_user_service,
        "handle-get-user-list", G_CALLBACK(_handle_get_user_list),
        adapter);

    g_signal_connect (G_OBJECT (adapter->priv->daemon), "user-added",
            G_CALLBACK (_on_user_added), adapter);
    g_signal_connect (G_OBJECT (adapter->priv->daemon), "user-deleted",
            G_CALLBACK (_on_user_deleted), adapter);
    g_signal_connect (G_OBJECT (adapter->priv->daemon), "user-updated",
            G_CALLBACK (_on_user_updated), adapter);

    return adapter;
}
