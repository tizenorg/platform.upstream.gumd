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
#include "daemon/core/gumd-daemon.h"
#include "daemon/core/gumd-daemon-group.h"

#include "gum-group.h"
#include "gum-group-service.h"
#include "gum-internals.h"

/**
 * SECTION:gum-group
 * @short_description: provides interface for managing group's data
 * @include: gum/gum-group.h
 *
 * #GumGroup provides interface for adding, removing and updating group.
 * Group's information can also be retrieved using this interface. Only
 * privileged user can access the interface when system-bus is used for
 * communication with the user management daemon.
 *
 * Following code snippet demonstrates how to create a new remote group object:
 *
 * |[
 *  GumGroup *group = NULL;
 *
 *  group = gum_group_create_sync (FALSE);
 *
 *  // use the object
 *
 *  // destroy the object
 *  g_object_unref (group);
 * ]|
 *
 * Similarly, new group can be added as:
 * |[
 *  GumGroup *group = NULL;
 *  gboolean rval = FALSE;
 *
 *  group = gum_group_create_sync (FALSE);
 *
 *  // set group properties
 *  g_object_set (G_OBJECT (group), "groupname", "group1", "secret", "123456",
 *   "grouptype", GUM_GROUPTYPE_USER, NULL);
 *
 *  // add group
 *  rval = gum_group_add_sync (user);
 *
 *  // destroy the object
 *  g_object_unref (group);
 * ]|
 *
 * For more details, see command-line utility implementation here:
 *<ulink url="https://github.com/01org/gumd/blob/master/src/utils/gum-utils.c">
 *          gum-utils</ulink>
 */

/**
 * GumGroupCb:
 * @group: (transfer none): #GumGroup object which is used in the request
 * @error: (transfer none): #GError object. In case of error, error will be
 * non-NULL
 * @user_data: user data passed onto the request
 *
 * #GumGroupCb defines the callback which is used when group object is created,
 * added, deleted or updated or new members are added to the group.
 */

/**
 * GumGroup:
 *
 * Opaque structure for the object.
 */

/**
 * GumGroupClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

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
    GumdDaemon *offline_service;
    GumdDaemonGroup *offline_group;
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
    PROP_OFFLINE,

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
    group->priv->op->cb_id = 0;
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
    switch (property_id) {
        case PROP_OFFLINE: {
            if (g_value_get_boolean (value)) {
                self->priv->offline_service = gumd_daemon_new ();
            } else {
                self->priv->cancellable = g_cancellable_new ();
                self->priv->dbus_service = gum_group_service_get_instance ();
            }
            break;
        }
        default: {
            if (self->priv->offline_group) {
                g_object_set_property (G_OBJECT(self->priv->offline_group),
                        pspec->name, value);
            } else if (self->priv->dbus_group) {
                g_object_set_property (G_OBJECT(self->priv->dbus_group),
                        pspec->name, value);
            }
        }
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
    switch (property_id) {
        case PROP_OFFLINE: {
            g_value_set_boolean (value, self->priv->offline_service != NULL);
            break;
        }
        default: {
            if (self->priv->offline_group) {
                g_object_get_property (G_OBJECT(self->priv->offline_group),
                        pspec->name, value);
            } else if (self->priv->dbus_group) {
                g_object_get_property (G_OBJECT(self->priv->dbus_group),
                        pspec->name, value);
            }
        }
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

    GUM_OBJECT_UNREF (self->priv->offline_group);

    GUM_OBJECT_UNREF (self->priv->offline_service);

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
    self->priv->offline_group = NULL;
    self->priv->cancellable = NULL;
    self->priv->dbus_service = NULL;
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

    /**
     * GumGroup:grouptype:
     *
     * This property holds a group type that the object corresponds to. Valid
     * values of group types are as specified in #GumGroupType.
     * #GumGroup:grouptype must be specified when adding a new group.
     */
    properties[PROP_GROUPTYPE] =  g_param_spec_uint ("grouptype",
            "GroupType",
            "Type of the group",
            0,
            G_MAXUINT16,
            GUM_GROUPTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumGroup:gid:
     *
     * This property holds a unique group identity for the group as assigned by
     * the underlying framework, which is always be in range [0, MAXUINT].
     */
    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the group",
            0,
            G_MAXUINT,
            GUM_GROUP_INVALID_GID /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumGroup:groupname:
     *
     * This property holds the name of given to the group when the group is
     * added. Allowed pattern for groupname is:
     * "^[A-Za-z_][A-Za-z0-9_.-]*[A-Za-z0-9_.$-]\\?$".
     */
    properties[PROP_GROUPNAME] = g_param_spec_string ("groupname",
            "Groupname",
            "System name of the group",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumGroup:secret:
     *
     * This property holds the secret as chosen. Secret should not
     * contain any control chars (0x00-0x1F,0x7F) or colon (':' 0x3A).
     */
    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "Group secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    /**
     * GumUser:offline:
     *
     * This property is used to define the behaviour of synchronous operation
     * performed on the object. If it is set to TRUE, then the object performs
     * the operation without daemon (dbus/gumd) otherwise 'gumd' daemon will be
     * used for the required synchronous functionality.
     */
    properties[PROP_OFFLINE] =  g_param_spec_boolean ("offline",
            "Offline",
            "Operational mode for the Group object",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

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

static GVariant *
_get_prop_value (
        GVariant *props,
        const gchar *prop)
{
    GVariantIter *iter = NULL;
    GVariant *item = NULL;
    g_variant_get (props, "(a{sv})",  &iter);
    while ((item = g_variant_iter_next_value (iter)))  {
        gchar *key;
        GVariant *value;
        g_variant_get (item, "{sv}", &key, &value);
        if (g_strcmp0 (key, prop) == 0) {
            g_free (key);
            return g_variant_ref (value);
        }
        g_free (key); key = NULL;
        g_variant_unref (value); value = NULL;
    }
    return NULL;
}

static gboolean
_sync_properties (
        GumGroup *group)
{
    GError *error = NULL;
    GVariant *result = NULL;

    /* load all properties synchronously */
    result = g_dbus_connection_call_sync (
            g_dbus_proxy_get_connection (
                    G_DBUS_PROXY (group->priv->dbus_service)),
            g_dbus_proxy_get_name (G_DBUS_PROXY (group->priv->dbus_service)),
            g_dbus_proxy_get_object_path (G_DBUS_PROXY (group->priv->dbus_group)),
            "org.freedesktop.DBus.Properties",
            "GetAll",
            g_variant_new ("(s)",
                    g_dbus_proxy_get_interface_name (
                            G_DBUS_PROXY (group->priv->dbus_group))),
            G_VARIANT_TYPE ("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            group->priv->cancellable,
            &error);

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        return FALSE;
    }

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(a{sv})"))) {
        guint n_properties = 0, ind = 0;
        GParamSpec **properties = g_object_class_list_properties (
                G_OBJECT_GET_CLASS(group), &n_properties);
        for (ind=0; ind < n_properties; ind++) {
            GParamSpec *pspec = properties[ind];
            GVariant *prop = _get_prop_value (result,  pspec->name);
            if (prop != NULL) {
                g_dbus_proxy_set_cached_property (
                        G_DBUS_PROXY (group->priv->dbus_group), pspec->name,
                        prop);
            }
        }
        g_free (properties);
    }
    g_variant_unref (result);
    return TRUE;
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

/**
 * gum_group_create:
 * @callback: #GumGroupCb to be invoked when new group object is created
 * @user_data: user data
 *
 * This method creates a new remote group object over the DBus asynchronously.
 * Callback is used to notify when the remote object is fully created and
 * accessible.
 *
 * Returns: (transfer full): #GumGroup newly created object
 */
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

/**
 * gum_group_create_sync:
 * @offline: enables or disables the offline mode
 *
 * This method creates a new remote group object. In case offline mode is
 * enabled, then the object is created directly without using dbus otherwise
 * the objects gets created over the DBus synchronously.
 *
 * Returns: (transfer full): #GumGroup newly created object
 */
GumGroup *
gum_group_create_sync (
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;

    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, "offline",
            offline, NULL));
    if (!group) return NULL;

    if (group->priv->offline_service) {
        group->priv->offline_group = gumd_daemon_group_new (
                gumd_daemon_get_config (group->priv->offline_service));
        return group;
    }

    g_return_val_if_fail (group->priv->dbus_service != NULL, NULL);

    if (gum_dbus_group_service_call_create_new_group_sync (
            group->priv->dbus_service, &object_path, group->priv->cancellable,
                &error)) {
        _create_dbus_group (group, object_path, error);
    }

    g_free (object_path);

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (group);
        group = NULL;
    }

    return group;
}

/**
 * gum_group_get:
 * @gid: group id for the group
 * @callback: #GumGroupCb to be invoked when group object is fetched
 * @user_data: user data
 *
 * This method gets the group object attached to gid over the DBus
 * asynchronously. Callback is used to notify when the remote object is fully
 * created and accessible.
 *
 * Returns: (transfer full): #GumGroup object
 */
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


/**
 * gum_group_get_sync:
 * @gid: group id for the group
 * @offline: enables or disables the offline mode
 *
 * This method gets the group object attached to gid. In case offline mode is
 * enabled, then the object is retrieved directly without using dbus otherwise
 * the objects gets retrieved over the DBus synchronously.
 *
 * Returns: (transfer full): #GumGroup object
 */
GumGroup *
gum_group_get_sync (
        gid_t gid,
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;

    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, "offline",
            offline, NULL));
    if (!group) return NULL;

    if (group->priv->offline_service) {
        group->priv->offline_group = gumd_daemon_get_group (
                group->priv->offline_service, gid, &error);
    } else if (group->priv->dbus_service) {

        if (gum_dbus_group_service_call_get_group_sync (
                group->priv->dbus_service, gid, &object_path,
                group->priv->cancellable, &error)) {
            _create_dbus_group (group, object_path, error);
        }
        g_free (object_path);
    }

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (group);
        group = NULL;
    }
    return group;
}

/**
 * gum_group_get_by_name:
 * @groupname: name of the group
 * @callback: #GumGroupCb to be invoked when group object is fetched
 * @user_data: user data
 *
 * This method gets the group object attached to groupname over the DBus
 * asynchronously. Callback is used to notify when the remote object is fully
 * created and accessible.
 *
 * Returns: (transfer full): #GumGroup object
 */
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

/**
 * gum_group_get_by_name_sync:
 * @groupname: name of the group
 * @offline: enables or disables the offline mode
 *
 * This method gets the group object attached to groupname. In case offline mode
 * is enabled, then the object is retrieved directly without using dbus
 * otherwise the objects gets retrieved over the DBus synchronously.
 *
 * Returns: (transfer full): #GumGroup object
 */
GumGroup *
gum_group_get_by_name_sync (
        const gchar *groupname,
        gboolean offline)
{
    GError *error = NULL;
    gchar *object_path = NULL;

    if (!groupname) {
        WARN ("groupname not specified");
        return NULL;
    }

    GumGroup *group = GUM_GROUP (g_object_new (GUM_TYPE_GROUP, "offline",
            offline, NULL));
    if (!group) return NULL;

    if (group->priv->offline_service) {

        group->priv->offline_group = gumd_daemon_get_group_by_name (
                group->priv->offline_service, groupname, &error);
    } else if (group->priv->dbus_service) {

        if (gum_dbus_group_service_call_get_group_by_name_sync (
                group->priv->dbus_service, groupname, &object_path,
                group->priv->cancellable, &error)) {
            _create_dbus_group (group, object_path, error);
        }
        g_free (object_path);
    }

    if (error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        g_object_unref (group);
        group = NULL;
    }
    return group;
}

/**
 * gum_group_add:
 * @self: #GumGroup object to be added; object should have valid
 * #GumGroup:groupname and #GumGroup:grouptype properties.
 * @callback: #GumGroupCb to be invoked when group is added
 * @user_data: user data
 *
 * This method adds the group over the DBus asynchronously. Callback is used to
 * notify when the group is added.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_group_add (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_add_group (self->priv->dbus_group,
            GUM_GROUP_INVALID_GID, self->priv->cancellable, _on_group_add_cb,
            self);
    return TRUE;
}

/**
 * gum_group_add_sync:
 * @self: #GumGroup object to be added; object should have valid
 * #GumGroup:groupname and #GumGroup:grouptype properties.
 *
 * This method adds the group. In case offline mode is enabled, then the group
 * is added directly without using dbus otherwise the group is added over the
 * DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_group_add_sync (
        GumGroup *self)
{
    GError *error = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (self->priv->offline_group) {
        rval = gumd_daemon_add_group (self->priv->offline_service,
                self->priv->offline_group, &error);
    } else if (self->priv->dbus_group) {
        rval = gum_dbus_group_call_add_group_sync (self->priv->dbus_group,
                GUM_GROUP_INVALID_GID, &gid, self->priv->cancellable, &error);
        if (rval) _sync_properties (self);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_group_delete:
 * @self: #GumGroup object to be deleted; object should have valid
 * #GumGroup:gid property.
 * @callback: #GumGroupCb to be invoked when group is deleted
 * @user_data: user data
 *
 * This method deletes the group over the DBus asynchronously. Callback is used
 * to notify when the group is deleted.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_group_delete (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_delete_group (self->priv->dbus_group,
            self->priv->cancellable, _on_group_delete_cb, self);
    return TRUE;
}

/**
 * gum_group_delete_sync:
 * @self: #GumGroup object to be deleted; object should have valid
 * #GumGroup:gid property.
 *
 * This method deletes the group. In case offline mode is enabled, then group
 * is deleted directly without using dbus otherwise the group is deleted over
 * DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_group_delete_sync (
        GumGroup *self)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (self->priv->offline_group) {
        rval = gumd_daemon_delete_group (self->priv->offline_service,
                self->priv->offline_group, &error);
    } else if (self->priv->dbus_group) {
        rval = gum_dbus_group_call_delete_group_sync (self->priv->dbus_group,
                self->priv->cancellable, &error);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_group_update:
 * @self: #GumGroup object to be updated; object should have valid
 * #GumGroup:gid property.
 * @callback: #GumGroupCb to be invoked when group is updated
 * @user_data: user data
 *
 * This method updates the group over the DBus asynchronously. Callback is used
 * to notify when the group is updated. The properties which can be updated
 * are: secret.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_group_update (
        GumGroup *self,
        GumGroupCb callback,
        gpointer user_data)
{
    DBG ("Update Group");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (!self->priv->dbus_group) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_update_group (self->priv->dbus_group,
            self->priv->cancellable, _on_group_update_cb, self);
    return TRUE;
}

/**
 * gum_group_update_sync:
 * @self: #GumGroup object to be updated; object should have valid
 * #GumGroup:gid property.
 *
 * This method updates the group. In case offline mode is enabled, then group
 * is updated directly without using dbus otherwise the group is updated over
 * DBus synchronously.
 * The properties which can be updated are: secret.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_group_update_sync (
        GumGroup *self)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (self->priv->offline_group) {
        rval = gumd_daemon_update_group (self->priv->offline_service,
                self->priv->offline_group, &error);
    } else if (self->priv->dbus_group) {
        rval = gum_dbus_group_call_update_group_sync (self->priv->dbus_group,
                self->priv->cancellable, &error);
        if (rval) _sync_properties (self);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_group_add_member:
 * @self: #GumGroup object where new member is to be added; object should have
 * valid #GumGroup:gid property.
 * @uid: user id of the member to be added to the group
 * @add_as_admin: user will be added with admin privileges for the group if set
 * to TRUE
 * @callback: #GumGroupCb to be invoked when member is added
 * @user_data: user data
 *
 * This method adds new member to the group over the DBus asynchronously.
 * Callback is used to notify when the member is added.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
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
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_add_member (self->priv->dbus_group, uid, add_as_admin,
            self->priv->cancellable, _on_group_add_member_cb,
            self);
    return TRUE;
}

/**
 * gum_group_add_member_sync:
 * @self: #GumGroup object where new member is to be added; object should have
 * valid #GumGroup:gid property.
 * @uid: user id of the member to be added to the group
 * @add_as_admin: user will be added with admin privileges for the group if set
 * to TRUE
 *
 * This method adds new member to the group. In case offline mode is enabled,
 * then group member is added directly without using dbus otherwise member is
 * added over DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_group_add_member_sync (
        GumGroup *self,
        uid_t uid,
        gboolean add_as_admin)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (self->priv->offline_group) {
        rval = gumd_daemon_add_group_member (self->priv->offline_service,
                self->priv->offline_group, uid, add_as_admin, &error);
    } else if (self->priv->dbus_group) {
        rval = gum_dbus_group_call_add_member_sync (self->priv->dbus_group, uid,
                add_as_admin, self->priv->cancellable, &error);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}

/**
 * gum_group_delete_member:
 * @self: #GumGroup object where member is to be deleted from; object should
 * have valid #GumGroup:gid property.
 * @uid: user id of the member to be deleted from the group
 * @callback: #GumGroupCb to be invoked when member is deleted
 * @user_data: user data
 *
 * This method deletes new member from the group over the DBus asynchronously.
 * Callback is used to notify when the member is deleted.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
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
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, callback, user_data);
    gum_dbus_group_call_delete_member (self->priv->dbus_group, uid,
            self->priv->cancellable, _on_group_delete_member_cb, self);
    return TRUE;
}

/**
 * gum_group_delete_member_sync:
 * @self: #GumGroup object where member is to be deleted from; object should
 * have valid #GumGroup:gid property.
 * @uid: user id of the member to be deleted from the group
 *
 * This method deletes new member from the group. In case offline mode is
 * enabled, then group member is deleted directly without using dbus otherwise
 * member is deleted over DBus synchronously.
 *
 * Returns: returns TRUE if successful, FALSE otherwise.
 */
gboolean
gum_group_delete_member_sync (
        GumGroup *self,
        uid_t uid)
{
    GError *error = NULL;
    gboolean rval = FALSE;

    DBG ("");
    g_return_val_if_fail (GUM_IS_GROUP (self), FALSE);

    if (self->priv->offline_group) {
        rval = gumd_daemon_delete_group_member (self->priv->offline_service,
                self->priv->offline_group, uid, &error);
    } else if (self->priv->dbus_group) {
        rval = gum_dbus_group_call_delete_member_sync (self->priv->dbus_group,
                uid, self->priv->cancellable, &error);
    }

    if (!rval && error) {
        WARN ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    return rval;
}
