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

#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

#include "gumd-daemon.h"

struct _GumdDaemonPrivate
{
    GumConfig *config;
    GHashTable *users;
    GHashTable *groups;
};

G_DEFINE_TYPE (GumdDaemon, gumd_daemon, G_TYPE_OBJECT)

#define GUMD_DAEMON_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUMD_TYPE_DAEMON, GumdDaemonPrivate)

static GObject *self = 0;

enum {
    SIG_USER_DELETED,
    SIG_USER_ADDED,
    SIG_USER_UPDATED,
    SIG_GROUP_DELETED,
    SIG_GROUP_ADDED,
    SIG_GROUP_UPDATED,

    SIG_MAX
};

static guint signals[SIG_MAX];

static gboolean
_compare_by_pointer (
        gpointer key,
        gpointer value,
        gpointer dead_object)
{
    return value == dead_object;
}

static void
_on_user_disposed (
        gpointer data,
        GObject *object)
{
    GumdDaemon *daemon = GUMD_DAEMON (data);

    DBG ("daemon %p user %p disposed", daemon, object);
    g_hash_table_foreach_remove (daemon->priv->users, _compare_by_pointer,
            object);
}

static void
_on_group_disposed (
        gpointer data,
        GObject *object)
{
    GumdDaemon *daemon = GUMD_DAEMON (data);

    DBG ("daemon %p group %p disposed", daemon, object);
    g_hash_table_foreach_remove (daemon->priv->groups, _compare_by_pointer,
            object);
}

static void
_clear_user_weak_ref (
        gpointer uid,
        gpointer user,
        gpointer user_data)
{
    g_object_weak_unref (G_OBJECT(user), _on_user_disposed, user_data);
}

static void
_clear_group_weak_ref (
        gpointer gid,
        gpointer group,
        gpointer user_data)
{
    g_object_weak_unref (G_OBJECT(group), _on_group_disposed, user_data);
}

static GObject*
_constructor (GType type,
              guint n_construct_params,
              GObjectConstructParam *construct_params)
{
    /*
     * Singleton daemon
     */
    if (!self) {
        self = G_OBJECT_CLASS (gumd_daemon_parent_class)->constructor (
                  type, n_construct_params, construct_params);
        g_object_add_weak_pointer (self, (gpointer) &self);
    }
    else {
        g_object_ref (self);
    }

    return self;
}

static void
_dispose (GObject *object)
{
    GumdDaemon *self = GUMD_DAEMON(object);

    if (self->priv->users) {
        g_hash_table_foreach (self->priv->users, _clear_user_weak_ref, self);
        g_hash_table_unref (self->priv->users);
        self->priv->users = NULL;
    }

    if (self->priv->groups) {
        g_hash_table_foreach (self->priv->groups, _clear_group_weak_ref, self);
        g_hash_table_unref (self->priv->groups);
        self->priv->groups = NULL;
    }

    GUM_OBJECT_UNREF (self->priv->config);

    G_OBJECT_CLASS (gumd_daemon_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    G_OBJECT_CLASS (gumd_daemon_parent_class)->finalize (object);
}

static void
gumd_daemon_init (
        GumdDaemon *self)
{
    self->priv = GUMD_DAEMON_PRIV (self);
    self->priv->config = gum_config_new ();
    self->priv->users = g_hash_table_new_full (g_direct_hash, g_direct_equal,
            NULL, NULL);
    self->priv->groups = g_hash_table_new_full (g_direct_hash, g_direct_equal,
            NULL, NULL);
}

static void
gumd_daemon_class_init (
        GumdDaemonClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumdDaemonPrivate));

    object_class->constructor = _constructor;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    signals[SIG_USER_ADDED] = g_signal_new ("user-added",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);

    signals[SIG_USER_DELETED] = g_signal_new ("user-deleted",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);
    signals[SIG_USER_UPDATED] = g_signal_new ("user-updated",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);

    signals[SIG_GROUP_ADDED] = g_signal_new ("group-added",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);

    signals[SIG_GROUP_DELETED] = g_signal_new ("group-deleted",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);
    signals[SIG_GROUP_UPDATED] = g_signal_new ("group-updated",
            GUMD_TYPE_DAEMON,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);
}

GumdDaemon *
gumd_daemon_new ()
{
    return GUMD_DAEMON (g_object_new (GUMD_TYPE_DAEMON, NULL));
}

guint
gumd_daemon_get_timeout (
        GumdDaemon *self)
{
    return gum_config_get_int (self->priv->config,
        GUM_CONFIG_DBUS_DAEMON_TIMEOUT, 0);
}

GumConfig *
gumd_daemon_get_config (
        GumdDaemon *self)
{
    g_return_val_if_fail (self != NULL && self->priv != NULL, NULL);

    return self->priv->config;
}

GumdDaemonUser *
gumd_daemon_get_user (
        GumdDaemon *self,
        uid_t uid,
        GError **error)
{
    GumdDaemonUser *user = NULL;
    if (!self || !GUMD_IS_DAEMON (self)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon object is not valid", error, NULL);
    }

    user = GUMD_DAEMON_USER (g_hash_table_lookup (self->priv->users,
            GUINT_TO_POINTER(uid)));
    if (user) {
        return GUMD_DAEMON_USER (g_object_ref (user));
    }

    user = gumd_daemon_user_new_by_uid (uid, self->priv->config);
    if (!user) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found", error,
                NULL);
    }

    g_hash_table_insert (self->priv->users, GUINT_TO_POINTER(uid), user);
    g_object_weak_ref (G_OBJECT (user), _on_user_disposed, self);

    return user;
}

GumdDaemonUser *
gumd_daemon_get_user_by_name (
        GumdDaemon *self,
        const gchar *username,
        GError **error)
{
    uid_t uid = GUM_USER_INVALID_UID;
    if (!self || !GUMD_IS_DAEMON (self)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon object is not valid", error, NULL);
    }

    uid = gumd_daemon_user_get_uid_by_name (username, self->priv->config);
    if (uid == GUM_USER_INVALID_UID) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found", error,
                NULL);
    }

    return gumd_daemon_get_user (self, uid, error);
}

gboolean
gumd_daemon_add_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        GError **error)
{
    uid_t uid = GUM_USER_INVALID_UID;

    if (!self || !GUMD_IS_DAEMON (self) || !user) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/usr object not valid", error, FALSE);
    }

    if (!gumd_daemon_user_add (user, &uid, error)) {
        return FALSE;
    }

    if (!g_hash_table_lookup (self->priv->users, GUINT_TO_POINTER(uid))) {
        g_hash_table_insert (self->priv->users, GUINT_TO_POINTER(uid), user);
        g_object_weak_ref (G_OBJECT (user), _on_user_disposed, self);
        g_signal_emit (self, signals[SIG_USER_ADDED], 0, uid);
    }

    return TRUE;
}

gboolean
gumd_daemon_delete_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        gboolean rem_home_dir,
        GError **error)
{
    uid_t uid = GUM_USER_INVALID_UID;

    if (!self || !GUMD_IS_DAEMON (self) || !user) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/user object not valid", error, FALSE);
    }

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);
    if (!gumd_daemon_user_delete (user, rem_home_dir, error)) {
        return FALSE;
    }
    /* user will be removed from cache when it is disposed off */
    if (uid != GUM_USER_INVALID_UID) {
        g_signal_emit (self, signals[SIG_USER_DELETED], 0, uid);
    }
    return TRUE;
}

gboolean
gumd_daemon_update_user (
        GumdDaemon *self,
        GumdDaemonUser *user,
        GError **error)
{
    uid_t uid = GUM_USER_INVALID_UID;

    if (!self || !GUMD_IS_DAEMON (self) || !user) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/user object not valid", error, FALSE);
    }

    if (!gumd_daemon_user_update (user, error)) {
        return FALSE;
    }

    g_object_get (G_OBJECT (user), "uid", &uid, NULL);
    if (uid != GUM_USER_INVALID_UID) {
        g_signal_emit (self, signals[SIG_USER_UPDATED], 0, uid);
    }
    return TRUE;
}

guint
gumd_daemon_get_user_timeout (
        GumdDaemon *self)
{
    return gum_config_get_int (self->priv->config,
        GUM_CONFIG_DBUS_USER_TIMEOUT, 0);
}

GumdDaemonGroup *
gumd_daemon_get_group (
        GumdDaemon *self,
        gid_t gid,
        GError **error)
{
    GumdDaemonGroup *group = NULL;
    if (!self || !GUMD_IS_DAEMON (self)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon object is not valid", error, NULL);
    }

    group = GUMD_DAEMON_GROUP (g_hash_table_lookup (self->priv->groups,
            GUINT_TO_POINTER(gid)));
    if (group) {
        return GUMD_DAEMON_GROUP (g_object_ref (group));
    }

    group = gumd_daemon_group_new_by_gid (gid, self->priv->config);
    if (!group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NOT_FOUND, "Group not found", error,
                FALSE);
    }

    g_hash_table_insert (self->priv->groups, GUINT_TO_POINTER(gid), group);
    g_object_weak_ref (G_OBJECT (group), _on_group_disposed, self);

    return group;
}

GumdDaemonGroup *
gumd_daemon_get_group_by_name (
        GumdDaemon *self,
        const gchar *groupname,
        GError **error)
{
    gid_t gid = GUM_GROUP_INVALID_GID;
    if (!self || !GUMD_IS_DAEMON (self)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon object is not valid", error, NULL);
    }

    gid = gumd_daemon_group_get_gid_by_name (groupname, self->priv->config);
    if (gid == GUM_GROUP_INVALID_GID) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NOT_FOUND, "User not found", error,
                NULL);
    }

    return gumd_daemon_get_group (self, gid, error);
}

gboolean
gumd_daemon_add_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error)
{
    gid_t gid = GUM_GROUP_INVALID_GID;

    if (!self || !GUMD_IS_DAEMON (self) || !group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/usr object not valid", error, FALSE);
    }

    if (!gumd_daemon_group_add (group, GUM_GROUP_INVALID_GID, &gid, error)) {
        return FALSE;
    }

    if (!g_hash_table_lookup (self->priv->groups, GUINT_TO_POINTER(gid))) {
        g_hash_table_insert (self->priv->groups, GUINT_TO_POINTER(gid), group);
        g_object_weak_ref (G_OBJECT (group), _on_group_disposed, self);
    }
    return TRUE;
}

gboolean
gumd_daemon_delete_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error)
{
    gid_t gid = GUM_GROUP_INVALID_GID;

    if (!self || !GUMD_IS_DAEMON (self) || !group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/group object not valid", error, FALSE);
    }

    g_object_get (G_OBJECT (group), "gid", &gid, NULL);
    if (!gumd_daemon_group_delete (group, error)) {
        return FALSE;
    }
    /* group will be removed from cache when it is disposed off */
    if (gid != GUM_GROUP_INVALID_GID) {
        g_signal_emit (self, signals[SIG_GROUP_DELETED], 0, gid);
    }
    return TRUE;
}

gboolean
gumd_daemon_update_group (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        GError **error)
{
    gid_t gid = GUM_GROUP_INVALID_GID;

    if (!self || !GUMD_IS_DAEMON (self) || !group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/group object not valid", error, FALSE);
    }

    if (!gumd_daemon_group_update (group, error)) {
        return FALSE;
    }

    g_object_get (G_OBJECT (group), "gid", &gid, NULL);
    if (gid != GUM_GROUP_INVALID_GID) {
        g_signal_emit (self, signals[SIG_GROUP_UPDATED], 0, gid);
    }
    return TRUE;
}

gboolean
gumd_daemon_add_group_member (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        uid_t uid,
        gboolean add_as_admin,
        GError **error)
{
    if (!self || !GUMD_IS_DAEMON (self) || !group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/group object not valid", error, FALSE);
    }

    if (!gumd_daemon_group_add_member (group, uid, add_as_admin, error)) {
        return FALSE;
    }

    return TRUE;
}

gboolean
gumd_daemon_delete_group_member (
        GumdDaemon *self,
        GumdDaemonGroup *group,
        uid_t uid,
        GError **error)
{
    if (!self || !GUMD_IS_DAEMON (self) || !group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_INPUT,
                "Daemon/group object not valid", error, FALSE);
    }

    if (!gumd_daemon_group_delete_member (group, uid, error)) {
        return FALSE;
    }

    return TRUE;
}

guint
gumd_daemon_get_group_timeout (
        GumdDaemon *self)
{
    return gum_config_get_int (self->priv->config,
        GUM_CONFIG_DBUS_GROUP_TIMEOUT, 0);
}

