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

#include "config.h"

#include "common/gum-dbus.h"
#include "common/gum-error.h"
#include "common/gum-log.h"

#include "gum-user-service.h"

static GHashTable *user_service_objects = NULL;
static GMutex mutex;

static void
_thread_service_free (
        GWeakRef *data)
{
    g_slice_free (GWeakRef, data);
}

static GumDbusUserService *
_get_service_singleton ()
{
    GumDbusUserService *object = NULL;

    g_mutex_lock (&mutex);

    if (user_service_objects != NULL) {
        GWeakRef *ref;
        ref = g_hash_table_lookup (user_service_objects, g_thread_self ());
        if (ref != NULL) {
            object = g_weak_ref_get (ref);
        }
    }

    g_mutex_unlock (&mutex);
    return object;
}

static void
_set_service_singleton (
        GumDbusUserService *object)
{
    g_return_if_fail (GUM_DBUS_IS_USER_SERVICE (object));

    g_mutex_lock (&mutex);

    if (!user_service_objects) {
        user_service_objects = g_hash_table_new_full (g_direct_hash,
                g_direct_equal, NULL, (GDestroyNotify)_thread_service_free);
    }

    if (object != NULL) {
        GWeakRef *ref = g_slice_new (GWeakRef);
        g_weak_ref_init (ref, object);
        g_hash_table_insert (user_service_objects, g_thread_self (), ref);
    }

    g_mutex_unlock (&mutex);
}

static void
_on_user_service_destroyed (
        gpointer data,
        GObject *obj)
{
    g_mutex_lock (&mutex);
    if (user_service_objects)
    {
        g_hash_table_unref (user_service_objects);
        user_service_objects = NULL;
    }
    g_mutex_unlock (&mutex);
}

GumDbusUserService *
gum_user_service_get_instance ()
{
    GumDbusUserService *user_service = NULL;
    GDBusConnection *connection = NULL;
    GError *error = NULL;
    const gchar *bus_name = NULL;

    user_service = _get_service_singleton ();
    if (user_service) {
        return user_service;
    }

#ifdef GUM_BUS_TYPE_P2P
    gchar *bus_address = g_strdup_printf (GUM_DBUS_ADDRESS,
            g_get_user_runtime_dir());
    connection = g_dbus_connection_new_for_address_sync (bus_address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, &error);
    g_free (bus_address);
#else
    connection = g_bus_get_sync (GUM_BUS_TYPE, NULL, &error);
    bus_name = GUM_SERVICE;
#endif

    user_service = gum_dbus_user_service_proxy_new_sync (connection,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, bus_name,
            GUM_USER_SERVICE_OBJECTPATH, NULL, &error);

    if (G_LIKELY (error == NULL)) {
        g_object_weak_ref (G_OBJECT (user_service), _on_user_service_destroyed,
                user_service);
        _set_service_singleton (user_service);
        gum_error_quark ();
    } else {
        WARN ("Unable to start user service: %s", error->message);
        g_clear_error (&error);
    }

    return user_service;
}
