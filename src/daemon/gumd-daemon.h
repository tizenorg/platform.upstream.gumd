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

#ifndef __GUMD_DAEMON_H_
#define __GUMD_DAEMON_H_

#include <glib.h>
#include <glib-object.h>
#include <common/gum-config.h>
#include "gumd-daemon-user.h"
#include "gumd-daemon-group.h"
#include "gumd-types.h"

G_BEGIN_DECLS

#define GUMD_TYPE_DAEMON            (gumd_daemon_get_type())
#define GUMD_DAEMON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUMD_TYPE_DAEMON, GumdDaemon))
#define GUMD_DAEMON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUMD_TYPE_DAEMON, GumdDaemonClass))
#define GUMD_IS_DAEMON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUMD_TYPE_DAEMON))
#define GUMD_IS_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUMD_TYPE_DAEMON))
#define GUMD_DAEMON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUMD_TYPE_DAEMON, GumdDaemonClass))

typedef struct _GumdDaemonPrivate GumdDaemonPrivate;

struct _GumdDaemon
{
    GObject parent;

    /* priv */
    GumdDaemonPrivate *priv;
};

struct _GumdDaemonClass
{
    GObjectClass parent_class;
};

GType
gumd_daemon_get_type (void) G_GNUC_CONST;

GumdDaemon *
gumd_daemon_new ();

gboolean
gumd_daemon_clear_user_cache (
        GumdDaemon *self,
        GError **error);

guint
gumd_daemon_get_timeout (
        GumdDaemon *self) G_GNUC_CONST;

GumConfig *
gumd_daemon_get_config (
        GumdDaemon *self) G_GNUC_CONST;

GumdDaemonUser *
gumd_daemon_get_user (
        GumdDaemon *self,
        uid_t uid,
        GError **error);

GumdDaemonUser *
gumd_daemon_get_user_by_name (
        GumdDaemon *self,
        const gchar *username,
        GError **error);

gboolean
gumd_daemon_add_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        GError **error);

gboolean
gumd_daemon_delete_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        gboolean rem_home_dir,
        GError **error);

gboolean
gumd_daemon_update_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        GError **error);

guint
gumd_daemon_get_user_timeout (
        GumdDaemon *self) G_GNUC_CONST;

GumdDaemonGroup *
gumd_daemon_get_group (
        GumdDaemon *self,
        gid_t gid,
        GError **error);

GumdDaemonGroup *
gumd_daemon_get_group_by_name (
        GumdDaemon *self,
        const gchar *groupname,
        GError **error);

gboolean
gumd_daemon_add_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error);

gboolean
gumd_daemon_delete_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error);

gboolean
gumd_daemon_update_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error);

gboolean
gumd_daemon_add_group_member (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        uid_t uid,
        gboolean add_as_admin,
        GError **error);

gboolean
gumd_daemon_delete_group_member (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        uid_t uid,
        GError **error);

guint
gumd_daemon_get_group_timeout (
        GumdDaemon *self) G_GNUC_CONST;

G_END_DECLS

#endif /* __GUMD_DAEMON_H_ */
