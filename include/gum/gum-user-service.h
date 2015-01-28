/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 - 2015 Intel Corporation.
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

#ifndef __GUM_USER_SERVICE_H_
#define __GUM_USER_SERVICE_H_

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GUM_TYPE_USER_SERVICE            (gum_user_service_get_type())
#define GUM_USER_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUM_TYPE_USER_SERVICE, GumUserService))
#define GUM_USER_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUM_TYPE_USER_SERVICE, GumUserServiceClass))
#define GUM_IS_USER_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUM_TYPE_USER_SERVICE))
#define GUM_IS_USER_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUM_TYPE_USER_SERVICE))
#define GUM_USER_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUM_TYPE_USER_SERVICE, GumUserServiceClass))

typedef struct _GumUserService GumUserService;
typedef struct _GumUserServiceClass GumUserServiceClass;
typedef struct _GumUserServicePrivate GumUserServicePrivate;

struct _GumUserService
{
    GObject parent;

    /* priv */
    GumUserServicePrivate *priv;
};

struct _GumUserServiceClass
{
    GObjectClass parent_class;
};

typedef GList GumUserList;

typedef void (*GumUserServiceCb) (
        GumUserService *service,
        const GError *error,
        gpointer user_data);

typedef void (*GumUserServiceListCb) (
        GumUserService *service,
        GumUserList *users,
        const GError *error,
        gpointer user_data);

GType
gum_user_service_get_type (void) G_GNUC_CONST;

GumUserService *
gum_user_service_create (
        GumUserServiceCb callback,
        gpointer user_data);

GumUserService *
gum_user_service_create_sync (
        gboolean offline);

gboolean
gum_user_service_get_user_list (
        GumUserService *self,
        const gchar *types,
        GumUserServiceListCb callback,
        gpointer user_data);

GumUserList *
gum_user_service_get_user_list_sync (
        GumUserService *self,
        const gchar *types);

void
gum_user_service_list_free (
        GumUserList *users);

GDBusProxy *
gum_user_service_get_dbus_proxy (
        GumUserService *self);

G_END_DECLS

#endif /* __GUM_USER_SERVICE_H_ */
