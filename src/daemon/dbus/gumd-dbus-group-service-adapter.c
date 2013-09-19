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

#include "gumd-dbus-group-service-adapter.h"
#include "gumd-dbus-group-adapter.h"

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
    GumdDbusGroupAdapter *dbus_group;
    GumdDbusGroupServiceAdapter *group_service;
} PeerGroupService;

struct _GumdDbusGroupServiceAdapterPrivate
{
    GDBusConnection *connection;
    GumDbusGroupService *dbus_group_service;
    GumdDaemon *daemon;
    GumdDbusServerBusType  dbus_server_type;
    GList *peer_groups;
    GHashTable *caller_watchers; //(dbus_caller:watcher_id)
};

G_DEFINE_TYPE (GumdDbusGroupServiceAdapter, gumd_dbus_group_service_adapter, \
        GUM_TYPE_DISPOSABLE)

#define GUMD_DBUS_GROUP_SERVICE_ADAPTER_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), GUMD_TYPE_GROUP_SERVICE_ADAPTER, \
            GumdDbusGroupServiceAdapterPrivate)

static gboolean
_handle_create_new_group (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer group_data);

static gboolean
_handle_get_group (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        guint32 gid,
        gpointer group_data);

static gboolean
_handle_get_group_by_name (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *groupname,
        gpointer group_data);

static void
_on_dbus_group_adapter_disposed (
        gpointer data,
        GObject *object);

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (object);

    switch (property_id) {
        case PROP_DAEMON: {
            GObject *daemon = g_value_get_object (value);
            if (daemon) {
                GUM_OBJECT_UNREF (self->priv->daemon);
                self->priv->daemon = GUMD_DAEMON (daemon);
            }
            break;
        }
        case PROP_CONNECTION:
            self->priv->connection = g_value_get_object(value);
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
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (object);

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
_on_group_added (
        GObject *object,
        guint gid,
        gpointer group_data)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
            group_data);
    gum_dbus_group_service_emit_group_added (self->priv->dbus_group_service,
            gid);
}

static void
_on_group_deleted (
        GObject *object,
        guint gid,
        gpointer group_data)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
            group_data);
    gum_dbus_group_service_emit_group_deleted (self->priv->dbus_group_service,
            gid);
}

static void
_on_group_updated (
        GObject *object,
        guint gid,
        gpointer group_data)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
            group_data);
    gum_dbus_group_service_emit_group_updated (self->priv->dbus_group_service,
            gid);
}

static PeerGroupService *
_dbus_peer_group_new (
        GumdDbusGroupServiceAdapter *self,
        const gchar *peer_name,
        GumdDbusGroupAdapter *dbus_group)
{
    PeerGroupService *peer_group = g_malloc0 (sizeof (PeerGroupService));
    peer_group->peer_name = g_strdup (peer_name);
    peer_group->dbus_group = dbus_group;
    peer_group->group_service = self;
    return peer_group;
}

static void
_dbus_peer_group_free (
        PeerGroupService *peer_group,
        gpointer group_data)
{
    if (peer_group) {
        GUM_STR_FREE (peer_group->peer_name);
        GUM_OBJECT_UNREF (peer_group->dbus_group);
        peer_group->group_service = NULL;
        g_free (peer_group);
    }
}

static void
_dbus_peer_group_remove (
        PeerGroupService *peer_group,
        gpointer group_data)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
            group_data);
    if (peer_group && GUMD_IS_DBUS_GROUP_ADAPTER(peer_group->dbus_group)) {
        g_object_weak_unref (G_OBJECT (peer_group->dbus_group),
                _on_dbus_group_adapter_disposed, group_data);
        _dbus_peer_group_free (peer_group, NULL);
        self->priv->peer_groups = g_list_remove (self->priv->peer_groups,
                (gpointer)peer_group);
    }
}

static void
_dispose (
        GObject *object)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (object);

    DBG("- unregistering dbus group service. %d",
            G_OBJECT (self->priv->daemon)->ref_count);

    if (self->priv->peer_groups) {
        g_list_foreach (self->priv->peer_groups, (GFunc)_dbus_peer_group_remove,
                self);
    }

    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_group_added, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_group_deleted, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->daemon),
            _on_group_updated, self);

    GUM_OBJECT_UNREF (self->priv->daemon);

    if (self->priv->dbus_group_service) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (
                self->priv->dbus_group_service));
        g_object_unref (self->priv->dbus_group_service);
        self->priv->dbus_group_service = NULL;
    }

    GUM_HASHTABLE_UNREF (self->priv->caller_watchers);

    G_OBJECT_CLASS (gumd_dbus_group_service_adapter_parent_class)->dispose (
            object);
}

static void
_finalize (
        GObject *object)
{
    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (object);

    if (self->priv->peer_groups) {
        g_list_free (self->priv->peer_groups);
        self->priv->peer_groups = NULL;
    }

    G_OBJECT_CLASS (gumd_dbus_group_service_adapter_parent_class)->finalize (
            object);
}

static void
gumd_dbus_group_service_adapter_class_init (
        GumdDbusGroupServiceAdapterClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (GumdDbusGroupServiceAdapterPrivate));

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
gumd_dbus_group_service_adapter_init (GumdDbusGroupServiceAdapter *self)
{
    self->priv = GUMD_DBUS_GROUP_SERVICE_ADAPTER_GET_PRIV(self);

    self->priv->connection = 0;
    self->priv->daemon = NULL;
    self->priv->peer_groups = NULL;
    self->priv->dbus_group_service = gum_dbus_group_service_skeleton_new ();
    self->priv->caller_watchers = g_hash_table_new_full (g_str_hash,
            g_str_equal, g_free, (GDestroyNotify)g_bus_unwatch_name);
}

static void
_clear_cache_for_peer_name (
        PeerGroupService *peer_group,
        PeerGroupService *group_data)
{
    g_return_if_fail (peer_group && group_data);
    g_return_if_fail (GUMD_IS_DBUS_GROUP_ADAPTER (peer_group->dbus_group));

    if (g_strcmp0 (peer_group->peer_name, group_data->peer_name) == 0) {
        DBG ("removing dbus group '%p' from cache", peer_group->dbus_group);
        g_object_weak_unref (G_OBJECT (peer_group->dbus_group),
                _on_dbus_group_adapter_disposed, peer_group->group_service);
        _dbus_peer_group_free (peer_group, NULL);
        group_data->group_service->priv->peer_groups = g_list_remove (
                group_data->group_service->priv->peer_groups,
                (gpointer)peer_group);
    }
}

static void
_on_bus_name_lost (
        GDBusConnection *conn,
        const char *peer_name,
        gpointer group_data)
{
    PeerGroupService peer_group;

    g_return_if_fail (peer_name && group_data &&
            GUMD_IS_DBUS_GROUP_SERVICE_ADAPTER (group_data));

    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
            group_data);
    DBG ("(-)peer disappeared : %s", peer_name);

    peer_group.peer_name = (gchar *)peer_name;
    peer_group.dbus_group = NULL;
    peer_group.group_service = self;
    g_list_foreach (self->priv->peer_groups, (GFunc)_clear_cache_for_peer_name,
            (gpointer)&peer_group);

    g_hash_table_remove (self->priv->caller_watchers, (gpointer)peer_name);
}

static void
_add_bus_name_watcher (
        GumdDbusGroupServiceAdapter *self,
        GumdDbusGroupAdapter *dbus_group,
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
                    (gpointer)g_strdup (sender), GUINT_TO_POINTER(watcher_id));
        }
    }
}

static void
_clear_cache_for_group (
        PeerGroupService *peer_group,
        PeerGroupService *group_data)
{
    g_return_if_fail (peer_group && group_data);
    g_return_if_fail (GUMD_IS_DBUS_GROUP_ADAPTER (peer_group->dbus_group));

    if (peer_group->dbus_group == group_data->dbus_group) {
        DBG ("removing dbus group '%p' from cache", peer_group->dbus_group);
        peer_group->dbus_group = NULL;
        _dbus_peer_group_free (peer_group, NULL);
        group_data->group_service->priv->peer_groups = g_list_remove (
                group_data->group_service->priv->peer_groups,
                (gpointer)peer_group);
    }
}

static void
_on_dbus_group_adapter_disposed (
        gpointer data,
        GObject *object)
{
    PeerGroupService peer_group;

    GumdDbusGroupServiceAdapter *self = GUMD_DBUS_GROUP_SERVICE_ADAPTER (data);

    DBG ("Dbus group adapter object %p disposed", object);

    peer_group.peer_name = NULL;
    peer_group.dbus_group = GUMD_DBUS_GROUP_ADAPTER (object);
    peer_group.group_service = self;
    g_list_foreach (self->priv->peer_groups, (GFunc)_clear_cache_for_group,
            (gpointer)&peer_group);

    if (g_list_length (self->priv->peer_groups) == 0) {
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
    }
}

static gchar *
_get_sender (
        GumdDbusGroupServiceAdapter *self,
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

static GumdDbusGroupAdapter *
_create_and_cache_dbus_group (
        GumdDbusGroupServiceAdapter *self,
        GumdDaemonGroup *group,
        GDBusMethodInvocation *invocation)
{
    GDBusConnection *connection = g_dbus_method_invocation_get_connection (
            invocation);

    GumdDbusGroupAdapter *dbus_group =
            gumd_dbus_group_adapter_new_with_connection (
                    g_object_ref (connection), group,
                    gumd_daemon_get_group_timeout (self->priv->daemon));

    /* keep alive till this group object gets disposed */
    if (g_list_length (self->priv->peer_groups) == 0)
        gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    self->priv->peer_groups = g_list_append (self->priv->peer_groups,
            _dbus_peer_group_new (self, _get_sender (self, invocation),
            dbus_group));
    g_object_weak_ref (G_OBJECT (dbus_group), _on_dbus_group_adapter_disposed,
            self);

    /* watchers used for msg-bus only */
    _add_bus_name_watcher (self, dbus_group, invocation);

    return dbus_group;
}

static GumdDbusGroupAdapter *
_get_dbus_group_from_cache (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        gid_t gid)
{
    GumdDbusGroupAdapter *dbus_group = NULL;
    PeerGroupService *peer_group = NULL;
    GList *list = self->priv->peer_groups;
    gchar *peer_name = NULL;

    if (gid == GUM_GROUP_INVALID_GID) {
        return NULL;
    }

    peer_name = _get_sender (self, invocation);
    DBG ("peername:%s uid %u", peer_name, gid);
    for ( ; list != NULL; list = g_list_next (list)) {
        peer_group = (PeerGroupService *) list->data;
        if (g_strcmp0 (peer_name, peer_group->peer_name) == 0 &&
            gumd_dbus_group_adapter_get_gid (peer_group->dbus_group) == gid) {
            dbus_group = peer_group->dbus_group;
            break;
        }
    }
    g_free (peer_name);

    return dbus_group;
}

static gboolean
_handle_create_new_group (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer group_data)
{
    GumdDaemonGroup *group = NULL;
    GError *error = NULL;

    g_return_val_if_fail (self && GUMD_IS_DBUS_GROUP_SERVICE_ADAPTER(self),
            FALSE);

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    group = gumd_daemon_group_new (gumd_daemon_get_config (self->priv->daemon));
    if (group) {
        GumdDbusGroupAdapter *dbus_group = _create_and_cache_dbus_group (self,
                group, invocation);
        gum_dbus_group_service_complete_create_new_group (
                self->priv->dbus_group_service, invocation,
                gumd_dbus_group_adapter_get_object_path (dbus_group));
    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }
    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);
    
    return TRUE;
}

static gboolean
_handle_get_group (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        guint32 gid,
        gpointer group_data)
{
    GumdDaemonGroup *group = NULL;
    GError *error = NULL;
    GumdDbusGroupAdapter *dbus_group = NULL;

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    dbus_group = _get_dbus_group_from_cache (self, invocation, gid);
    if (!dbus_group) {
    	group = gumd_daemon_get_group (self->priv->daemon, (gid_t)gid, &error);
    	if (group) {
    		dbus_group = _create_and_cache_dbus_group (self, group, invocation);
    	}
    }

    if (dbus_group) {
        gum_dbus_group_service_complete_get_group (self->priv->dbus_group_service,
                invocation, gumd_dbus_group_adapter_get_object_path (dbus_group));
    } else {
        if (!error) {
            error = gum_get_gerror_for_id (GUM_ERROR_GROUP_NOT_FOUND,
                    "Group Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

static gboolean
_handle_get_group_by_name (
        GumdDbusGroupServiceAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *groupname,
        gpointer group_data)
{
    GumdDaemonGroup *group = NULL;
    GError *error = NULL;
    GumdDbusGroupAdapter *dbus_group = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), FALSE);

    gid = gumd_daemon_group_get_gid_by_name (groupname, gumd_daemon_get_config (
    		self->priv->daemon));

    dbus_group = _get_dbus_group_from_cache (self, invocation, gid);
    if (!dbus_group) {
        group = gumd_daemon_get_group (self->priv->daemon, (gid_t)gid, &error);
        if (group) {
            dbus_group = _create_and_cache_dbus_group (self, group, invocation);
        }
    }

    if (dbus_group) {
        gum_dbus_group_service_complete_get_group_by_name (
        		self->priv->dbus_group_service, invocation,
        		gumd_dbus_group_adapter_get_object_path (dbus_group));
    } else {
        if (!error) {
            error = gum_get_gerror_for_id (GUM_ERROR_GROUP_NOT_FOUND,
                    "Group Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    gum_disposable_set_auto_dispose (GUM_DISPOSABLE (self), TRUE);

    return TRUE;
}

GumdDbusGroupServiceAdapter *
gumd_dbus_group_service_adapter_new_with_connection (
        GDBusConnection *bus_connection,
        GumdDaemon *daemon,
        GumdDbusServerBusType bus_type)
{
    GError *err = NULL;
    guint timeout = 0;
    GumdDbusGroupServiceAdapter *adapter = GUMD_DBUS_GROUP_SERVICE_ADAPTER (
        g_object_new (GUMD_TYPE_GROUP_SERVICE_ADAPTER,
            "daemon", daemon,
            "bustype", bus_type,
            "connection", bus_connection,
            NULL));

    if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON(adapter->priv->dbus_group_service),
            adapter->priv->connection, GUM_GROUP_SERVICE_OBJECTPATH, &err)) {
        WARN ("failed to register object: %s", err->message);
        g_error_free (err);
        g_object_unref (adapter);
        return NULL;
    }
    DBG("(+) started group service interface '%p' at path '%s' on connection"
            " '%p'", adapter, GUM_GROUP_SERVICE_OBJECTPATH, bus_connection);

    timeout = gumd_daemon_get_group_timeout (adapter->priv->daemon);
    if (timeout) {
        gum_disposable_set_timeout (GUM_DISPOSABLE (adapter), timeout);
    }

    g_signal_connect_swapped (adapter->priv->dbus_group_service,
        "handle-create-new-group", G_CALLBACK (_handle_create_new_group),
        adapter);
    g_signal_connect_swapped (adapter->priv->dbus_group_service,
        "handle-get-group", G_CALLBACK(_handle_get_group), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_group_service,
        "handle-get-group-by-name", G_CALLBACK(_handle_get_group_by_name),
        adapter);

    g_signal_connect (G_OBJECT (adapter->priv->daemon), "group-added",
            G_CALLBACK (_on_group_added), adapter);
    g_signal_connect (G_OBJECT (adapter->priv->daemon), "group-deleted",
            G_CALLBACK (_on_group_deleted), adapter);
    g_signal_connect (G_OBJECT (adapter->priv->daemon), "group-updated",
            G_CALLBACK (_on_group_updated), adapter);

    return adapter;
}
