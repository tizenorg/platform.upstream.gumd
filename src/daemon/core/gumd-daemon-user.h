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

#ifndef __GUMD_DAEMON_USER_H_
#define __GUMD_DAEMON_USER_H_

#include <glib.h>
#include <glib-object.h>
#include <common/gum-config.h>
#include <common/gum-user-types.h>

G_BEGIN_DECLS

#define GUMD_TYPE_DAEMON_USER            (gumd_daemon_user_get_type())
#define GUMD_DAEMON_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUMD_TYPE_DAEMON_USER, GumdDaemonUser))
#define GUMD_DAEMON_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUMD_TYPE_DAEMON_USER, GumdDaemonUserClass))
#define GUMD_IS_DAEMON_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUMD_TYPE_DAEMON_USER))
#define GUMD_IS_DAEMON_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUMD_TYPE_DAEMON_USER))
#define GUMD_DAEMON_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUMD_TYPE_DAEMON_USER, GumdDaemonUserClass))

typedef struct _GumdDaemonUser GumdDaemonUser;
typedef struct _GumdDaemonUserClass GumdDaemonUserClass;
typedef struct _GumdDaemonUserPrivate GumdDaemonUserPrivate;

struct _GumdDaemonUser
{
    GObject parent;

    /* priv */
    GumdDaemonUserPrivate *priv;
};

struct _GumdDaemonUserClass
{
    GObjectClass parent_class;
};

GType
gumd_daemon_user_get_type (void) G_GNUC_CONST;

GumdDaemonUser *
gumd_daemon_user_new (
        GumConfig *config);

GumdDaemonUser *
gumd_daemon_user_new_by_uid (
        uid_t uid,
        GumConfig *config);

GumdDaemonUser *
gumd_daemon_user_new_by_name (
        const gchar *username,
        GumConfig *config);

gboolean
gumd_daemon_user_add (
        GumdDaemonUser *self,
        uid_t *uid,
        GError **error);

gboolean
gumd_daemon_user_delete (
        GumdDaemonUser *self,
        gboolean rem_home_dir,
        GError **error);

gboolean
gumd_daemon_user_update (
        GumdDaemonUser *self,
        GError **error);

uid_t
gumd_daemon_user_get_uid_by_name (
        const gchar *username,
        GumConfig *config);

GVariant *
gumd_daemon_user_get_user_list (
        const gchar *types,
        GumConfig *config,
        GError **error);

G_END_DECLS

#endif /* __GUMD_DAEMON_USER_H_ */
