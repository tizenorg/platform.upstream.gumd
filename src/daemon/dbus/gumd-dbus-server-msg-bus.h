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

#ifndef __GUMD_DBUS_SERVER_MSG_BUS_H_
#define __GUMD_DBUS_SERVER_MSG_BUS_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GUMD_TYPE_DBUS_SERVER_MSG_BUS      (gumd_dbus_server_msg_bus_get_type())
#define GUMD_DBUS_SERVER_MSG_BUS(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj),\
        GUMD_TYPE_DBUS_SERVER_MSG_BUS, GumdDbusServerMsgBus))
#define GUMD_DBUS_SERVER_MSG_BUS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),\
        GUMD_TYPE_DBUS_SERVER_MSG_BUS, GumdDbusServerMsgBusClass))
#define GUMD_IS_DBUS_SERVER_MSG_BUS(obj)     (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
        GUMD_TYPE_DBUS_SERVER_MSG_BUS))
#define GUMD_IS_DBUS_SERVER_MSG_BUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(\
        (klass),GUMD_TYPE_DBUS_SERVER_MSG_BUS))
#define GUMD_DBUS_SERVER_MSG_BUS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(\
        (obj), GUMD_TYPE_DBUS_SERVER_MSG_BUS, GumdDbusServerMsgBusClass))

typedef struct _GumdDbusServerMsgBus GumdDbusServerMsgBus;
typedef struct _GumdDbusServerMsgBusClass GumdDbusServerMsgBusClass;
typedef struct _GumdDbusServerMsgBusPrivate GumdDbusServerMsgBusPrivate;

struct _GumdDbusServerMsgBus
{
    GObject parent;

    /* priv */
    GumdDbusServerMsgBusPrivate *priv;
};

struct _GumdDbusServerMsgBusClass
{
    GObjectClass parent_class;
};

GType
gumd_dbus_server_msg_bus_get_type ();

GumdDbusServerMsgBus *
gumd_dbus_server_msg_bus_new ();

#endif /* __GUMD_DBUS_SERVER_MSG_BUS_H_ */
