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
#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common/gum-dbus.h"
#include "common/gum-log.h"
#include "common/gum-defines.h"

#include "gumd-dbus-server-p2p.h"
#include "gumd-dbus-server-interface.h"
#include "gumd-dbus-user-adapter.h"
#include "gumd-dbus-user-service-adapter.h"
#include "gumd-dbus-group-adapter.h"
#include "gumd-dbus-group-service-adapter.h"
#include "daemon/gumd-daemon.h"

enum
{
    PROP_0,

    IFACE_PROP_TYPE,
    PROP_ADDRESS,

    N_PROPERTIES
};

struct _GumdDbusServerP2PPrivate
{
    GumdDaemon *daemon;
    GHashTable *user_service_adapters;
    GHashTable *group_service_adapters;
    GDBusServer *bus_server;
    gchar *address;
};

static void
_gumd_dbus_server_p2p_interface_init (
        GumdDbusServerInterface *iface);

gboolean
_gumd_dbus_server_p2p_stop (
        GumdDbusServer *self);

static void
_on_connection_closed (
        GDBusConnection *connection,
        gboolean remote_peer_vanished,
        GError *error,
        gpointer user_data);

G_DEFINE_TYPE_WITH_CODE (GumdDbusServerP2P,
        gumd_dbus_server_p2p, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (GUMD_TYPE_DBUS_SERVER,
                _gumd_dbus_server_p2p_interface_init));

#define GUMD_DBUS_SERVER_P2P_GET_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj),\
        GUMD_TYPE_DBUS_SERVER_P2P, GumdDbusServerP2PPrivate)

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDbusServerP2P *self = GUMD_DBUS_SERVER_P2P (object);
    switch (property_id) {
        case IFACE_PROP_TYPE: {
            break;
        }
        case PROP_ADDRESS: {
            self->priv->address = g_value_dup_string (value);
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
    GumdDbusServerP2P *self = GUMD_DBUS_SERVER_P2P (object);

    switch (property_id) {
        case IFACE_PROP_TYPE: {
            g_value_set_uint (value, (guint)GUMD_DBUS_SERVER_BUSTYPE_P2P);
            break;
        }
        case PROP_ADDRESS: {
            g_value_set_string (value, g_dbus_server_get_client_address (
                    self->priv->bus_server));
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gboolean
_compare_by_pointer (
        gpointer key,
        gpointer value,
        gpointer dead_object)
{
    return value == dead_object;
}

static void
_on_user_service_dispose (
        gpointer data,
        GObject *dead)
{
    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (data);
    g_return_if_fail (server);
    g_hash_table_foreach_steal (server->priv->user_service_adapters,
                _compare_by_pointer, dead);
}

static void
_on_group_service_dispose (
        gpointer data,
        GObject *dead)
{
    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (data);
    g_return_if_fail (server);
    g_hash_table_foreach_steal (server->priv->group_service_adapters,
                _compare_by_pointer, dead);
}

static void
_clear_user_watchers (
        gpointer connection,
        gpointer user_service,
        gpointer user_data)
{
    g_signal_handlers_disconnect_by_func (connection, _on_connection_closed,
            user_data);
    g_object_weak_unref (G_OBJECT(user_service), _on_user_service_dispose,
            user_data);
}

static void
_clear_group_watchers (
        gpointer connection,
        gpointer group_service,
        gpointer user_data)
{
    g_signal_handlers_disconnect_by_func (connection, _on_connection_closed,
            user_data);
    g_object_weak_unref (G_OBJECT(group_service), _on_group_service_dispose,
            user_data);
}

static void
_add_user_watchers (
        gpointer connection,
        gpointer user_service,
        GumdDbusServerP2P *server)
{
    g_signal_connect (connection, "closed", G_CALLBACK(_on_connection_closed),
            server);
    g_object_weak_ref (G_OBJECT (user_service), _on_user_service_dispose,
            server);
    g_hash_table_insert (server->priv->user_service_adapters, connection,
            user_service);
}

static void
_add_group_watchers (
        gpointer connection,
        gpointer group_service,
        GumdDbusServerP2P *server)
{
    g_signal_connect (connection, "closed", G_CALLBACK(_on_connection_closed),
            server);
    g_object_weak_ref (G_OBJECT (group_service), _on_group_service_dispose,
            server);
    g_hash_table_insert (server->priv->group_service_adapters, connection,
            group_service);
}

static void
_dispose (
        GObject *object)
{
    GumdDbusServerP2P *self = GUMD_DBUS_SERVER_P2P (object);

    if (self->priv->group_service_adapters) {
        g_hash_table_foreach (self->priv->group_service_adapters,
                _clear_group_watchers, self);
        g_hash_table_unref (self->priv->group_service_adapters);
        self->priv->group_service_adapters = NULL;
    }

    if (self->priv->user_service_adapters) {
        g_hash_table_foreach (self->priv->user_service_adapters,
                _clear_user_watchers, self);
        g_hash_table_unref (self->priv->user_service_adapters);
        self->priv->user_service_adapters = NULL;
    }

    _gumd_dbus_server_p2p_stop (GUMD_DBUS_SERVER (self));

    GUM_OBJECT_UNREF (self->priv->daemon);
 
    G_OBJECT_CLASS (gumd_dbus_server_p2p_parent_class)->dispose (object);
}

static void
_finalize (
        GObject *object)
{
    GumdDbusServerP2P *self = GUMD_DBUS_SERVER_P2P (object);
    if (self->priv->address) {
        if (g_str_has_prefix (self->priv->address, "unix:path=")) {
            const gchar *path = g_strstr_len(self->priv->address, -1,
                    "unix:path=") + 10;
            if (path) {
                g_unlink (path);
            }
        }
        g_free (self->priv->address);
        self->priv->address = NULL;
    }
    G_OBJECT_CLASS (gumd_dbus_server_p2p_parent_class)->finalize (object);
}

static void
gumd_dbus_server_p2p_class_init (
        GumdDbusServerP2PClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GParamSpec *address_spec = NULL;

    g_type_class_add_private (object_class, sizeof (GumdDbusServerP2PPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    g_object_class_override_property (object_class, IFACE_PROP_TYPE, "bustype");

    address_spec = g_param_spec_string ("address", "server address",
            "Server socket address",  NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_ADDRESS,
            address_spec);
}

static void
gumd_dbus_server_p2p_init (
        GumdDbusServerP2P *self)
{
    self->priv = GUMD_DBUS_SERVER_P2P_GET_PRIV(self);
    self->priv->bus_server = NULL;
    self->priv->address = NULL;
    self->priv->daemon = gumd_daemon_new ();
    self->priv->user_service_adapters = g_hash_table_new_full (g_direct_hash,
            g_direct_equal, NULL, g_object_unref);
    self->priv->group_service_adapters = g_hash_table_new_full (g_direct_hash,
            g_direct_equal, NULL, g_object_unref);
}

static void
_on_connection_closed (
        GDBusConnection *connection,
        gboolean remote_peer_vanished,
        GError *error,
        gpointer user_data)
{
    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (user_data);
    gpointer service = g_hash_table_lookup (server->priv->user_service_adapters,
            connection);
    if  (service) {
        _clear_user_watchers (connection, service, user_data);
        DBG("P2P dbus connection(%p) user service closed (peer vanished : %d)"
                " with error: %s", connection, remote_peer_vanished,
                error ? error->message : "NONE");
        g_hash_table_remove (server->priv->user_service_adapters, connection);
    }

    service = g_hash_table_lookup (server->priv->group_service_adapters,
            connection);
    if  (service) {
        _clear_group_watchers (connection, service, user_data);
        DBG("P2P dbus connection(%p) group_service closed (peer vanished : %d)"
                " with error: %s", connection, remote_peer_vanished,
                error ? error->message : "NONE");
        g_hash_table_remove (server->priv->group_service_adapters, connection);
    }
}

static void
_gumd_dbus_server_p2p_start_user_service (
        GumdDbusServerP2P *server,
        GDBusConnection *connection)
{
    GumdDbusUserServiceAdapter *user_service = NULL;
    GumdDbusGroupServiceAdapter *group_service = NULL;

    DBG("Export interfaces on connection %p", connection);

    user_service = gumd_dbus_user_service_adapter_new_with_connection (
            g_object_ref (connection), g_object_ref (server->priv->daemon),
            GUMD_DBUS_SERVER_BUSTYPE_P2P);
    _add_user_watchers (connection, user_service, server);

    group_service = gumd_dbus_group_service_adapter_new_with_connection (
            g_object_ref (connection), g_object_ref (server->priv->daemon),
            GUMD_DBUS_SERVER_BUSTYPE_P2P);
    _add_group_watchers (connection, group_service, server);
}

static gboolean
_on_client_request (
        GDBusServer *dbus_server,
        GDBusConnection *connection,
        gpointer user_data)
{
    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (user_data);
    if (!server) {
        WARN ("memory corruption");
        return TRUE;
    }
    _gumd_dbus_server_p2p_start_user_service (server, connection);
    return TRUE;
}

gboolean
_gumd_dbus_server_p2p_start (
        GumdDbusServer *self)
{
    g_return_val_if_fail (GUMD_IS_DBUS_SERVER_P2P (self), FALSE);

    DBG("Start P2P DBus Server");

    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (self);
    if (!server->priv->bus_server) {
        GError *err = NULL;
        gchar *guid = g_dbus_generate_guid ();
        server->priv->bus_server = g_dbus_server_new_sync (
                server->priv->address, G_DBUS_SERVER_FLAGS_NONE, guid, NULL,
                NULL, &err);
        g_free (guid);

        if (!server->priv->bus_server) {
            WARN ("Failed to start server at address '%s':%s",
                    server->priv->address, err ? err->message : "NULL");
            g_error_free (err);
            return FALSE;
        }

        g_signal_connect (server->priv->bus_server, "new-connection",
                G_CALLBACK(_on_client_request), server);
    }

    if (!g_dbus_server_is_active (server->priv->bus_server)) {
        const gchar *path = NULL;
        g_dbus_server_start (server->priv->bus_server);
        path = g_strstr_len(server->priv->address, -1, "unix:path=") + 10;
        if (path) {
            g_chmod (path, S_IRUSR | S_IWUSR);
        }
    }
    DBG("Dbus server started at : %s", server->priv->address);

    return TRUE;
}

gboolean
_gumd_dbus_server_p2p_stop (
        GumdDbusServer *self)
{
    g_return_val_if_fail (GUMD_IS_DBUS_SERVER_P2P (self), FALSE);

    DBG("Stop P2P DBus Server");

    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (self);

    if (server->priv->bus_server) {
        if (g_dbus_server_is_active (server->priv->bus_server))
            g_dbus_server_stop (server->priv->bus_server);
        g_object_unref (server->priv->bus_server);
        server->priv->bus_server = NULL;
    }

    return TRUE;
}

static pid_t
_gumd_dbus_server_p2p_get_remote_pid (
        GumdDbusServer *self,
        GDBusMethodInvocation *invocation)
{
    pid_t remote_pid = 0;
    GDBusConnection *connection = NULL;
    gint peer_fd = -1;
    struct ucred peer_cred;
    socklen_t cred_size = sizeof(peer_cred);

    g_return_val_if_fail (invocation && GUMD_IS_DBUS_SERVER_P2P (self),
            remote_pid);

    connection = g_dbus_method_invocation_get_connection (invocation);
    peer_fd = g_socket_get_fd (g_socket_connection_get_socket (
            G_SOCKET_CONNECTION (g_dbus_connection_get_stream(connection))));
    if (peer_fd < 0 || getsockopt (peer_fd, SOL_SOCKET, SO_PEERCRED,
            &peer_cred, &cred_size) != 0) {
        WARN ("getsockopt() for SO_PEERCRED failed");
        return remote_pid;
    }
    DBG ("Remote p2p peer pid=%d uid=%d gid=%d", peer_cred.pid, peer_cred.uid,
            peer_cred.gid);

    return peer_cred.pid;
}

static void
_gumd_dbus_server_p2p_interface_init (
        GumdDbusServerInterface *iface)
{
    iface->start = _gumd_dbus_server_p2p_start;
    iface->stop = _gumd_dbus_server_p2p_stop;
    iface->get_remote_pid = _gumd_dbus_server_p2p_get_remote_pid;
}

const gchar *
gumd_dbus_server_p2p_get_address (
        GumdDbusServerP2P *server)
{
    g_return_val_if_fail (server || GUMD_IS_DBUS_SERVER_P2P (server), NULL);
    return g_dbus_server_get_client_address (server->priv->bus_server);
}

GumdDbusServerP2P *
gumd_dbus_server_p2p_new_with_address (
        const gchar *address)
{
    DBG("Create P2P DBus Server");

    GumdDbusServerP2P *server = GUMD_DBUS_SERVER_P2P (
        g_object_new (GUMD_TYPE_DBUS_SERVER_P2P, "address", address, NULL));

    if (!server) {
        return NULL;
    }

    if (g_str_has_prefix(address, "unix:path=")) {
        const gchar *file_path = g_strstr_len (address, -1, "unix:path=")
        		+ 10;

        if (g_file_test(file_path, G_FILE_TEST_EXISTS)) {
            g_unlink (file_path);
        }
        else {
            gchar *base_path = g_path_get_dirname (file_path);
            if (g_mkdir_with_parents (base_path, S_IRUSR | S_IWUSR | S_IXUSR)
                    == -1) {
                WARN ("Could not create '%s', error: %s", base_path,
                        strerror(errno));
            }
            g_free (base_path);
        }
    }

    return server;
}

GumdDbusServerP2P *
gumd_dbus_server_p2p_new ()
{
    gchar *address = g_strdup_printf (GUM_DBUS_ADDRESS,
            g_get_user_runtime_dir());
    GumdDbusServerP2P *server = gumd_dbus_server_p2p_new_with_address (
            address);
    g_free (address);
    return server;
}


