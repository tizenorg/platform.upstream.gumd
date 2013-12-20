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

#ifndef __GUMD_DBUS_SERVER_INTERFACE_H_
#define __GUMD_DBUS_SERVER_INTERFACE_H_

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GUMD_TYPE_DBUS_SERVER    (gumd_dbus_server_get_type ())
#define GUMD_DBUS_SERVER(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        GUMD_TYPE_DBUS_SERVER, GumdDbusServer))
#define GUMD_IS_DBUS_SERVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        GUMD_TYPE_DBUS_SERVER))
#define GUMD_DBUS_SERVER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ( \
        (inst), GUMD_TYPE_DBUS_SERVER, GumdDbusServerInterface))

typedef struct _GumdDbusServer GumdDbusServer; /* dummy object */
typedef struct _GumdDbusServerInterface GumdDbusServerInterface;

typedef enum {
    GUMD_DBUS_SERVER_BUSTYPE_NONE = 0,
    GUMD_DBUS_SERVER_BUSTYPE_MSG_BUS = 1,
    GUMD_DBUS_SERVER_BUSTYPE_P2P = 2
} GumdDbusServerBusType;

struct _GumdDbusServerInterface {

    GTypeInterface parent;

    gboolean
    (*start) (GumdDbusServer *self);

    gboolean
    (*stop)  (GumdDbusServer *self);

    pid_t
    (*get_remote_pid)  (
            GumdDbusServer *self,
            GDBusMethodInvocation *invocation);
};

GType
gumd_dbus_server_get_type();

gboolean
gumd_dbus_server_start (
        GumdDbusServer *self);

gboolean
gumd_dbus_server_stop (
        GumdDbusServer *self);

pid_t
gumd_dbus_server_get_remote_pid (
        GumdDbusServer *self,
        GDBusMethodInvocation *invocation);

GumdDbusServerBusType
gumd_dbus_server_get_bus_type (
        GumdDbusServer *self);

G_END_DECLS

#endif /* __GUMD_DBUS_SERVER_INTERFACE_H_ */
