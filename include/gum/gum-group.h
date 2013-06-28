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

#ifndef __GUM_GROUP_H_
#define __GUM_GROUP_H_

#include <glib.h>
#include <glib-object.h>
#include <grp.h>

G_BEGIN_DECLS

#define GUM_TYPE_GROUP            (gum_group_get_type())
#define GUM_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUM_TYPE_GROUP, GumGroup))
#define GUM_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUM_TYPE_GROUP, GumGroupClass))
#define GUM_IS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUM_TYPE_GROUP))
#define GUM_IS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUM_TYPE_GROUP))
#define GUM_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUM_TYPE_GROUP, GumGroupClass))

typedef struct _GumGroup GumGroup;
typedef struct _GumGroupClass GumGroupClass;
typedef struct _GumGroupPrivate GumGroupPrivate;

struct _GumGroup
{
    GObject parent;

    /* priv */
    GumGroupPrivate *priv;
};

struct _GumGroupClass
{
    GObjectClass parent_class;
};

typedef void (*GumGroupCb) (
        GumGroup *group,
        const GError *error,
        gpointer user_data);

GType
gum_group_get_type (void) G_GNUC_CONST;

GumGroup *
gum_group_create (
        GumGroupCb callback,
        gpointer user_data);

GumGroup *
gum_group_get (
        gid_t gid,
        GumGroupCb callback,
        gpointer user_data);

GumGroup *
gum_group_get_by_name (
        const gchar *groupname,
        GumGroupCb callback,
        gpointer user_data);

gboolean
gum_group_add (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data);

gboolean
gum_group_delete (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data);

gboolean
gum_group_update (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data);

gboolean
gum_group_add_member (
        GumGroup *self,
        uid_t uid,
        gboolean add_as_admin,
        GumGroupCb callback,
        gpointer user_data);

gboolean
gum_group_delete_member (
        GumGroup *self,
        uid_t uid,
        GumGroupCb callback,
        gpointer user_data);

G_END_DECLS

#endif /* __GUM_GROUP_H_ */
