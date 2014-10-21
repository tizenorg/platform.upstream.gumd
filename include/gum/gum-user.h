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

#ifndef __GUM_USER_H_
#define __GUM_USER_H_

#include <glib.h>
#include <glib-object.h>
#include <pwd.h>

G_BEGIN_DECLS

#define GUM_TYPE_USER            (gum_user_get_type())
#define GUM_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUM_TYPE_USER, GumUser))
#define GUM_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUM_TYPE_USER, GumUserClass))
#define GUM_IS_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUM_TYPE_USER))
#define GUM_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUM_TYPE_USER))
#define GUM_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUM_TYPE_USER, GumUserClass))

typedef struct _GumUser GumUser;
typedef struct _GumUserClass GumUserClass;
typedef struct _GumUserPrivate GumUserPrivate;

struct _GumUser
{
    GObject parent;

    /* priv */
    GumUserPrivate *priv;
};

struct _GumUserClass
{
    GObjectClass parent_class;
};

typedef void (*GumUserCb) (
        GumUser *user,
        const GError *error,
        gpointer user_data);

GType
gum_user_get_type (void) G_GNUC_CONST;

GumUser *
gum_user_create (
        GumUserCb callback,
        gpointer user_data);

GumUser *
gum_user_create_sync (
        gboolean offline);

GumUser *
gum_user_get (
        uid_t uid,
        GumUserCb callback,
        gpointer user_data);

GumUser *
gum_user_get_sync (
        uid_t uid,
        gboolean offline);

GumUser *
gum_user_get_by_name (
        const gchar *username,
        GumUserCb callback,
        gpointer user_data);

GumUser *
gum_user_get_by_name_sync (
        const gchar *username,
        gboolean offline);

gboolean
gum_user_add (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data);

gboolean
gum_user_add_sync (
        GumUser *self);

gboolean
gum_user_delete (
        GumUser *self,
        gboolean rem_home_dir,
        GumUserCb callback,
        gpointer user_data);

gboolean
gum_user_delete_sync (
        GumUser *self,
        gboolean rem_home_dir);

gboolean
gum_user_update (
        GumUser *self,
        GumUserCb callback,
        gpointer user_data);

gboolean
gum_user_update_sync (
        GumUser *self);

G_END_DECLS

#endif /* __GUM_USER_H_ */
