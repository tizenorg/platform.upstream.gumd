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

#ifndef __GUMD_DBUS_GROUP_SERVICE_ADAPTER_H_
#define __GUMD_DBUS_GROUP_SERVICE_ADAPTER_H_

#include <config.h>
#include <glib.h>
#include <common/gum-disposable.h>
#include "common/dbus/gum-dbus-group-service-gen.h"
#include "daemon/gumd-daemon.h"
#include "gumd-dbus-server-interface.h"

G_BEGIN_DECLS

#define GUMD_TYPE_GROUP_SERVICE_ADAPTER                \
    (gumd_dbus_group_service_adapter_get_type())
#define GUMD_DBUS_GROUP_SERVICE_ADAPTER(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GUMD_TYPE_GROUP_SERVICE_ADAPTER, \
            GumdDbusGroupServiceAdapter))
#define GUMD_DBUS_GROUP_SERVICE_ADAPTER_CLASS(klass)    \
    (G_TYPE_CHECK_CLASS_CAST((klass), GUMD_TYPE_GROUP_SERVICE_ADAPTER, \
            GumdDbusGroupServiceAdapterClass))
#define GUMD_IS_DBUS_GROUP_SERVICE_ADAPTER(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GUMD_TYPE_GROUP_SERVICE_ADAPTER))
#define GUMD_IS_DBUS_GROUP_SERVICE_ADAPTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GUMD_TYPE_GROUP_SERVICE_ADAPTER))
#define GUMD_DBUS_GROUP_SERVICE_ADAPTER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GUMD_TYPE_GROUP_SERVICE_ADAPTER, \
            GumdDbusGroupServiceAdapterClass))

typedef struct _GumdDbusGroupServiceAdapter GumdDbusGroupServiceAdapter;
typedef struct _GumdDbusGroupServiceAdapterClass GumdDbusGroupServiceAdapterClass;
typedef struct _GumdDbusGroupServiceAdapterPrivate GumdDbusGroupServiceAdapterPrivate;

struct _GumdDbusGroupServiceAdapter
{
    GumDisposable parent;

    /* priv */
    GumdDbusGroupServiceAdapterPrivate *priv;
};

struct _GumdDbusGroupServiceAdapterClass
{
    GumDisposableClass parent_class;
};

GType gumd_dbus_group_service_adapter_get_type (void) G_GNUC_CONST;

GumdDbusGroupServiceAdapter *
gumd_dbus_group_service_adapter_new_with_connection (
        GDBusConnection *connection,
        GumdDaemon *daemon,
        GumdDbusServerBusType bus_type);

G_END_DECLS

#endif /* __GUMD_DBUS_GROUP_SERVICE_ADAPTER_H_ */
