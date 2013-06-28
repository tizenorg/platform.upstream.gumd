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

#ifndef __GUMD_DAEMON_GROUP_H_
#define __GUMD_DAEMON_GROUP_H_

#include <glib.h>
#include <glib-object.h>
#include <grp.h>
#include <common/gum-config.h>
#include <common/gum-group-types.h>

G_BEGIN_DECLS

#define GUMD_TYPE_DAEMON_GROUP            (gumd_daemon_group_get_type())
#define GUMD_DAEMON_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUMD_TYPE_DAEMON_GROUP, GumdDaemonGroup))
#define GUMD_DAEMON_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUMD_TYPE_DAEMON_GROUP, GumdDaemonGroupClass))
#define GUMD_IS_DAEMON_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUMD_TYPE_DAEMON_GROUP))
#define GUMD_IS_DAEMON_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUMD_TYPE_DAEMON_GROUP))
#define GUMD_DAEMON_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUMD_TYPE_DAEMON_GROUP, GumdDaemonGroupClass))

typedef struct _GumdDaemonGroup GumdDaemonGroup;
typedef struct _GumdDaemonGroupClass GumdDaemonGroupClass;
typedef struct _GumdDaemonGroupPrivate GumdDaemonGroupPrivate;

#define GUMD_DAEMON_GROUP_INVALID_GID G_MAXUINT

struct _GumdDaemonGroup
{
    GObject parent;

    /* priv */
    GumdDaemonGroupPrivate *priv;
};

struct _GumdDaemonGroupClass
{
    GObjectClass parent_class;
};

GType
gumd_daemon_group_get_type (void) G_GNUC_CONST;

GumdDaemonGroup *
gumd_daemon_group_new (
        GumConfig *config);

GumdDaemonGroup *
gumd_daemon_group_new_by_gid (
        gid_t gid,
        GumConfig *config);

GumdDaemonGroup *
gumd_daemon_group_new_by_name (
        const gchar *groupname,
        GumConfig *config);

gboolean
gumd_daemon_group_add (
        GumdDaemonGroup *self,
        gid_t preferred_gid,
        uid_t *gid,
        GError **error);

gboolean
gumd_daemon_group_delete (
        GumdDaemonGroup *self,
        GError **error);

gboolean
gumd_daemon_group_update (
        GumdDaemonGroup *self,
        GError **error);

gboolean
gumd_daemon_group_add_member (
        GumdDaemonGroup *self,
        uid_t uid,
        gboolean add_as_admin,
        GError **error);

gboolean
gumd_daemon_group_delete_member (
        GumdDaemonGroup *self,
        uid_t uid,
        GError **error);

gboolean
gumd_daemon_group_delete_user_membership (
        GumConfig *config,
        const gchar *user_name,
        GError **error);

gid_t
gumd_daemon_group_get_gid_by_name (
        const gchar *groupname,
        GumConfig *config);

G_END_DECLS

#endif /* __GUMD_DAEMON_GROUP_H_ */
