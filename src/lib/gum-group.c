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

#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-group-types.h"
#include "common/dbus/gum-dbus-group-service-gen.h"
#include "common/dbus/gum-dbus-group-gen.h"

#include "gum-group.h"
#include "gum-group-service.h"
#include "gum-internals.h"

typedef struct {
    GumGroupCb callback;
    gpointer user_data;
    GError *error;
    guint cb_id;
} GumGroupOp;

struct _GumGroupPrivate
{
    GumDbusGroupService *dbus_service;
    GumDbusGroup *dbus_group;
    GCancellable *cancellable;
    GumGroupOp *op;
};

G_DEFINE_TYPE (GumGroup, gum_group, G_TYPE_OBJECT)

#define GUM_GROUP_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_GROUP, GumGroupPrivate)
enum
{
    PROP_0,

    PROP_GROUPTYPE,
    PROP_GID,
    PROP_GROUPNAME,
    PROP_SECRET,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];


static void
_free_op (
        GumGroup *self)
{
    if (self &&
        self->priv->op) {
        if (self->priv->op->error) g_error_free (self->priv->op->error);
        g_free (self->priv->op);
        self->priv->op = NULL;
    }
}

static void
_create_op (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    GumGroupOp *op = g_malloc0 (sizeof (GumGroupOp));
    op->callback = callback;
    op->user_data = user_data;
    _free_op (self);
    self->priv->op = op;
}

static gboolean
_trigger_callback (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_GROUP (user_data), FALSE);

    GumGroup *group = GUM_GROUP (user_data);
    if (group->priv->op->callback) {
        (group->priv->op->callback)(group, group->priv->op->error,
                group->priv->op->user_data);
    }
    return FALSE;
}

static void
_setup_idle_callback (
        GumGroup *self,
        const GError *error)
{
    if (!self->priv->op->callback) return;
    if (error) {
        if (self->priv->op->error) g_clear_error (&self->priv->op->error);
        self->priv->op->error = g_error_copy (error);
    }
    self->priv->op->cb_id = g_idle_add (_trigger_callback, self);
}

static void
_on_group_remote_object_destroyed (
        GDBusProxy *proxy,
        gpointer user_data)
{
    g_return_if_fail (GUM_IS_GROUP (user_data));
    GumGroup *self = GUM_GROUP (user_data);

    DBG("");

    GUM_OBJECT_UNREF (self->priv->dbus_group);
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumGroup *self = GUM_GROUP (object);
    if (self->priv->dbus_group) {
        g_object_set_property (G_OBJECT(self->priv->dbus_group), pspec->name,
                value);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    GumGroup *self = GUM_GROUP (object);
    if (self->priv->dbus_group) {
        g_object_get_property (G_OBJECT(self->priv->dbus_group), pspec->name,
                value);
    }
}

static void
_dispose (GObject *object)
{
    GumGroup *self = GUM_GROUP (object);

    if (self->priv->op &&
        self->priv->op->cb_id > 0) {
        g_source_remove (self->priv->op->cb_id);
        self->priv->op->cb_id = 0;
    }

    if (self->priv->cancellable) {
        g_cancellable_cancel (self->priv->cancellable);
        g_object_unref (self->priv->cancellable);
        self->priv->cancellable = NULL;
    }

    if (self->priv->dbus_group) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->dbus_group),
                _on_group_remote_object_destroyed, self);
    }

    GUM_OBJECT_UNREF (self->priv->dbus_group);

    GUM_OBJECT_UNREF (self->priv->dbus_service);

    G_OBJECT_CLASS (gum_group_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    GumGroup *self = GUM_GROUP (object);

    _free_op (self);

    G_OBJECT_CLASS (gum_group_parent_class)->finalize (object);
}

static void
gum_group_init (
        GumGroup *self)
{
    self->priv = GUM_GROUP_PRIV (self);
    self->priv->dbus_group = NULL;
    self->priv->cancellable = g_cancellable_new ();
    self->priv->dbus_service = gum_group_service_get_instance ();
    self->priv->op = NULL;
}

static void
gum_group_class_init (
        GumGroupClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumGroupPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_GROUPTYPE] =  g_param_spec_uint ("grouptype",
            "GroupType",
            "Type of the group",
            0,
            G_MAXUINT16,
            GUM_GROUPTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the group",
            0,
            G_MAXUINT,
            GUM_GROUP_INVALID_GID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GROUPNAME] = g_param_spec_string ("groupname",
            "Groupname",
            "System name of the group",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "Group secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);
}


static void
_create_dbus_group (
        GumGroup *group,
        gchar *object_path,
        GError *error)
{
    group->priv->dbus_group = gum_dbus_group_proxy_new_sync (
            g_dbus_proxy_get_connection (G_DBUS_PROXY (
                    group->priv->dbus_service)),
            G_DBUS_PROXY_FLAGS_NONE, g_dbus_proxy_get_name (
            G_DBUS_PROXY (group->priv->dbus_service)), object_path,
            group->priv->cancellable, &error);
    if (!error) {
        g_signal_connect (G_OBJECT (group->priv->dbus_group), "unregistered",
            G_CALLBACK (_on_group_remote_object_destroyed),  group);
    }
}

static void
_on_new_group_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroupService *proxy = GUM_DBUS_GROUP_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_service_call_create_new_group_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            _create_dbus_group (group, object_path, error);
        }
        _setup_idle_callback (group, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_get_group_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroupService *proxy = GUM_DBUS_GROUP_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_service_call_get_group_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            _create_dbus_group (group, object_path, error);
        }
        _setup_idle_callback (group, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_get_group_by_name_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroupService *proxy = GUM_DBUS_GROUP_SERVICE (object);
    gchar *object_path = NULL;
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_service_call_get_group_by_name_finish (proxy, &object_path,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            _create_dbus_group (group, object_path, error);
        }
        _setup_idle_callback (group, error);
    }
    g_free (object_path);
    g_clear_error (&error);
}

static void
_on_group_add_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroup *proxy = GUM_DBUS_GROUP (object);
    GError *error = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_call_add_group_finish (proxy, &gid, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (group, error);
    }
    g_clear_error (&error);
}

static void
_on_group_delete_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroup *proxy = GUM_DBUS_GROUP (object);
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_call_delete_group_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (group, error);
    }
    g_clear_error (&error);
}

static void
_on_group_update_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroup *proxy = GUM_DBUS_GROUP (object);
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_call_update_group_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (group, error);
    }
    g_clear_error (&error);
}

static void
_on_group_add_member_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroup *proxy = GUM_DBUS_GROUP (object);
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_call_add_member_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (group, error);
    }
    g_clear_error (&error);
}

static void
_on_group_delete_member_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumGroup *group = (GumGroup*)user_data;
    GumDbusGroup *proxy = GUM_DBUS_GROUP (object);
    GError *error = NULL;

    g_return_if_fail (group != NULL);

    DBG ("");

    gum_dbus_group_call_delete_member_finish (proxy, res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        _setup_idle_callback (group, error);
    }
    g_clear_error (&error);
}

GumGroup *
gum_group_create (
        GumGroupCb callback,
        gpointer user_data)
{
    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, NULL));
    g_return_val_if_fail (group->priv->dbus_service != NULL, NULL);

    _create_op (group, callback, user_data);
    gum_dbus_group_service_call_create_new_group (group->priv->dbus_service,
            group->priv->cancellable, _on_new_group_cb, group);
    return group;
}

GumGroup *
gum_group_get (
        gid_t gid,
        GumGroupCb callback,
        gpointer user_data)
{
    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, NULL));
    g_return_val_if_fail (group->priv->dbus_service != NULL, NULL);

    _create_op (group, callback, user_data);
    gum_dbus_group_service_call_get_group (group->priv->dbus_service, gid,
            group->priv->cancellable, _on_get_group_cb, group);
    return group;
}

GumGroup *
gum_group_get_by_name (
        const gchar *groupname,
        GumGroupCb callback,
        gpointer user_data)
{
    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, NULL));
    g_return_val_if_fail (group->priv->dbus_service != NULL, NULL);

    if (!groupname) {
        WARN ("groupname not specified");
        return NULL;
    }
    _create_op (group, callback, user_data);
    gum_dbus_group_service_call_get_group_by_name (group->priv->dbus_service,
            groupname, group->priv->cancellable, _on_get_group_by_name_cb,
            group);
    return group;
}

gboolean
gum_group_add (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_add_group (self->priv->dbus_group,
            GUM_GROUP_INVALID_GID, self->priv->cancellable, _on_group_add_cb,
            self);
    return TRUE;
}

gboolean
gum_group_delete (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_delete_group (self->priv->dbus_group,
            self->priv->cancellable, _on_group_delete_cb, self);
    return TRUE;
}

gboolean
gum_group_update (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("Update Group");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_update_group (self->priv->dbus_group,
            self->priv->cancellable, _on_group_update_cb, self);
    return TRUE;
}

gboolean
gum_group_add_member (
        GumGroup *self,
        uid_t uid,
        gboolean add_as_admin,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_add_member (self->priv->dbus_group, uid, add_as_admin,
            self->priv->cancellable, _on_group_add_member_cb,
            self);
    return TRUE;
}

gboolean
gum_group_delete_member (
        GumGroup *self,
        uid_t uid,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        DBG ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_delete_member (self->priv->dbus_group, uid,
            self->priv->cancellable, _on_group_delete_member_cb, self);
    return TRUE;
}
