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

#include "config.h"
#include "common/gum-dbus.h"
#include "common/gum-log.h"
#include "common/gum-defines.h"

#include "gumd-dbus-server-msg-bus.h"
#include "gumd-dbus-server-interface.h"
#include "gumd-dbus-user-service-adapter.h"
#include "gumd-dbus-group-service-adapter.h"
#include "daemon/gumd-daemon.h"

enum
{
    PROP_0,

    IFACE_PROP_TYPE,
    N_PROPERTIES
};

static void
_gumd_dbus_server_msg_bus_interface_init (
        GumdDbusServerInterface *iface);

static gboolean
_gumd_dbus_server_msg_bus_start (
        GumdDbusServer *self);

static gboolean
_gumd_dbus_server_msg_bus_stop (
        GumdDbusServer *self);

G_DEFINE_TYPE_WITH_CODE (GumdDbusServerMsgBus,
        gumd_dbus_server_msg_bus, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (GUMD_TYPE_DBUS_SERVER,
                _gumd_dbus_server_msg_bus_interface_init));

struct _GumdDbusServerMsgBusPrivate
{
    GumdDaemon *daemon;
    GumdDbusUserServiceAdapter *user_service;
    GumdDbusGroupServiceAdapter *group_service;
    guint name_owner_id;
};

#define GUMD_DBUS_SERVER_MSG_BUS_GET_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ( \
        (obj), GUMD_TYPE_DBUS_SERVER_MSG_BUS, GumdDbusServerMsgBusPrivate)

static gboolean
_close_server (
        gpointer data)
{
	g_object_unref (data);
	return FALSE;
}

static void
_on_bus_acquired (
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data)
{
    INFO ("Bus aquired on connection '%p'", connection);
}

static void
_on_name_lost (
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data)
{
    INFO ("Lost (or failed to acquire) the name '%s' on the on bus "
            "connection '%p'", name, connection);
    if (user_data && GUMD_IS_DBUS_SERVER_MSG_BUS(user_data))
        g_object_unref (G_OBJECT (user_data));
}

static void
_on_user_interface_dispose (
        gpointer data,
        GObject *dead)
{
    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (data);

    g_return_if_fail (server);

    server->priv->user_service = NULL;

    /* close server if all the interfaces are down */
    if (!server->priv->group_service)
        g_idle_add (_close_server, data);
}

static void
_on_group_interface_dispose (
        gpointer data,
        GObject *dead)
{
    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (data);

    g_return_if_fail (server);

    server->priv->group_service = NULL;

    /* close server if all the interfaces are down */
    if (!server->priv->user_service)
        g_idle_add (_close_server, data);
}

static void
_on_name_acquired (
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data)
{
    INFO ("Acquired the name %s on connection '%p'", name, connection);
    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (user_data);

    DBG("Export user service interface");
    server->priv->user_service =
    		gumd_dbus_user_service_adapter_new_with_connection (
    				g_object_ref (connection),
    				g_object_ref (server->priv->daemon),
    				GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS);
    g_object_weak_ref (G_OBJECT (server->priv->user_service),
            _on_user_interface_dispose, server);

    server->priv->group_service =
            gumd_dbus_group_service_adapter_new_with_connection (
                    g_object_ref (connection),
                    g_object_ref (server->priv->daemon),
                    GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS);
    g_object_weak_ref (G_OBJECT (server->priv->group_service),
            _on_group_interface_dispose, server);

    DBG ("Before: real uid %d effective uid %d", getuid (), geteuid ());
    if (seteuid (getuid()))
        WARN ("seteuid() failed");
    if (setegid (getgid()))
        WARN ("setegid() failed");
    DBG ("After: real gid %d effective gid %d", getgid (), getegid ());
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    switch (property_id)
    {
    case IFACE_PROP_TYPE: {
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
_get_property (
        GObject *object,
        guint prop_id,
        GValue *value,
        GParamSpec *pspec)
{
    switch (prop_id)
    {
        case IFACE_PROP_TYPE:
            g_value_set_uint (value, (guint)GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
_dispose (
        GObject *object)
{
    GumdDbusServerMsgBus *self = GUMD_DBUS_SERVER_MSG_BUS (object);

    _gumd_dbus_server_msg_bus_stop (GUMD_DBUS_SERVER (self));

    GUM_OBJECT_UNREF (self->priv->daemon);
 
    G_OBJECT_CLASS (gumd_dbus_server_msg_bus_parent_class)->dispose (object);
}

static void
_finalize (
        GObject *object)
{
    G_OBJECT_CLASS (gumd_dbus_server_msg_bus_parent_class)->finalize (object);
}

static void
gumd_dbus_server_msg_bus_class_init (
        GumdDbusServerMsgBusClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (GumdDbusServerMsgBusPrivate));

    object_class->set_property = _set_property;
    object_class->get_property = _get_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    g_object_class_override_property (object_class, IFACE_PROP_TYPE, "bustype");
}

static void
gumd_dbus_server_msg_bus_init (
        GumdDbusServerMsgBus *self)
{
    self->priv = GUMD_DBUS_SERVER_MSG_BUS_GET_PRIV(self);
    self->priv->daemon = gumd_daemon_new ();
    self->priv->user_service = NULL;
    self->priv->group_service = NULL;
    self->priv->name_owner_id = 0;
}

static gboolean
_gumd_dbus_server_msg_bus_start (
        GumdDbusServer *self)
{
    g_return_val_if_fail (GUMD_IS_DBUS_SERVER_MSG_BUS (self), FALSE);

    DBG("Start MSG-BUS Dbus server for bus type %s",
            GUM_BUS_TYPE == G_BUS_TYPE_SYSTEM? "SYSTEM":"SESSION");

    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (self);
    server->priv->name_owner_id = g_bus_own_name (GUM_BUS_TYPE,
            GUM_SERVICE,
            G_BUS_NAME_OWNER_FLAGS_REPLACE,
            _on_bus_acquired,
            _on_name_acquired,
            _on_name_lost,
            server, NULL);
    return TRUE;
}

static gboolean
_gumd_dbus_server_msg_bus_stop (
        GumdDbusServer *self)
{
    g_return_val_if_fail (GUMD_IS_DBUS_SERVER_MSG_BUS (self), FALSE);

    DBG("Stop MSG-BUS Dbus server");

    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (self);

    if (server->priv->group_service) {
        g_object_weak_unref (G_OBJECT (server->priv->group_service),
                _on_group_interface_dispose, server);
        g_object_unref (server->priv->group_service);
        server->priv->group_service = NULL;
    }

    if (server->priv->user_service) {
        g_object_weak_unref (G_OBJECT (server->priv->user_service),
                _on_user_interface_dispose, server);
        g_object_unref (server->priv->user_service);
        server->priv->user_service = NULL;
    }

    if (server->priv->name_owner_id) {
        g_bus_unown_name (server->priv->name_owner_id);
        server->priv->name_owner_id = 0;
    }

    return TRUE;
}

static pid_t
_gumd_dbus_server_msg_bus_get_remote_pid (
        GumdDbusServer *self,
        GDBusMethodInvocation *invocation)
{
    pid_t remote_pid = 0;
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    GVariant *response = NULL;
    guint32 upid;
    const gchar *sender = NULL;

    g_return_val_if_fail (invocation && GUMD_IS_DBUS_SERVER_MSG_BUS (self),
            remote_pid);

    sender = g_dbus_method_invocation_get_sender (invocation);
    if (!sender) {
        WARN ("Failed to get sender");
        return remote_pid;
    }

    connection = g_bus_get_sync (GUM_BUS_TYPE, NULL, &error);
    if (!connection) {
        WARN ("Failed to open connection to msg bus: %s", error->message);
        g_error_free (error);
        return remote_pid;
    }

    error = NULL;
    response = g_dbus_connection_call_sync (connection,
            GUM_DBUS_FREEDESKTOP_SERVICE, GUM_DBUS_FREEDESKTOP_PATH,
            GUM_DBUS_FREEDESKTOP_INTERFACE, "GetConnectionUnixProcessID",
            g_variant_new ("(s)", sender), ((const GVariantType *) "(u)"),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    g_object_unref (connection);

    if (!response) {
        WARN ("Request for msg-bus peer pid failed: %s", error->message);
        g_error_free (error);
        return remote_pid;
    }

    g_variant_get (response, "(u)", &upid);
    DBG ("Remote msg-bus peer service=%s pid=%u", sender, upid);
    remote_pid = (pid_t) upid;

    g_variant_unref (response);

    return remote_pid;
}

static void
_gumd_dbus_server_msg_bus_interface_init (
        GumdDbusServerInterface *iface)
{
    iface->start = _gumd_dbus_server_msg_bus_start;
    iface->stop = _gumd_dbus_server_msg_bus_stop;
    iface->get_remote_pid = _gumd_dbus_server_msg_bus_get_remote_pid;
}

GumdDbusServerMsgBus *
gumd_dbus_server_msg_bus_new ()
{
    INFO ("Create MSG-BUS Dbus server");
    GumdDbusServerMsgBus *server = GUMD_DBUS_SERVER_MSG_BUS (
            g_object_new (GUMD_TYPE_DBUS_SERVER_MSG_BUS, NULL));
    return server;
}
