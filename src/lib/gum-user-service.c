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

#include "config.h"

#include "common/dbus/gum-dbus-user-service-gen.h"
#include "common/gum-dbus.h"
#include "common/gum-error.h"
#include "common/gum-log.h"
#include "common/gum-defines.h"

#include "daemon/core/gumd-daemon.h"
#include "gum-user.h"
#include "gum-user-service.h"
#include "gum-internals.h"

/**
 * SECTION:gum-user-service
 * @short_description: provides interface for managing user's service
 * @include: gum/gum-user-service.h
 *
 * #GumUserService provides interface for retrieving user list based on the
 * types of the users. Only privileged user can access the interface when
 * system-bus is used for communication with the user management daemon.
 * NOTE: #GumUserService object is a singleton object.
 *
 * Following code snippet demonstrates how to create a new remote user
 * service object:
 *
 * |[
 *  GumUserService *user_service = NULL;
 *
 *  user_service = gum_user_service_create_sync (FALSE);
 *
 *  // use the object
 *
 *  // destroy the object
 *  g_object_unref (user_service);
 * ]|
 *
 * Similarly, user list can be retrieved as:
 * |[
 *  GumUserService *user_service = NULL;
 *  gchar *users = NULL;
 *
 *  user_service = gum_user_service_create_sync (FALSE);
 *
 *  // fetch user list
 *  users = gum_user_service_get_user_list_sync (user_service, "normal,guest",
 *  NULL, FALSE);
 *
 *  // destroy the object
 *  g_object_unref (user_service);
 * ]|
 *
 * For more details, see example implementation here:
 *<ulink url="https://github.com/01org/gumd/blob/master/src/utils/gum-utils.c">
 *          gum-utils</ulink>
 */

/**
 * GumUserList:
 *
 * Data structure for the list of users (tyepdef'd #GList).
 */

/**
 * GumUserServiceCb:
 * @service: (transfer none): #GumUserService object which is used in the
 * request
 * @error: (transfer none): #GError object. In case of error, error will be
 * non-NULL
 * @user_data: user data passed onto the request
 *
 * #GumUserServiceCb defines the callback which is used when the service object
 * is created e.g..
 */

/**
 * GumUserServiceListCb:
 * @service: (transfer none): #GumUserService object which is used in the
 * request
 * @users: (transfer full): #GumUserList list of #GumUser objects. Use
 * gum_user_list_free to free the list of the users
 * @error: (transfer none): #GError object. In case of error, error will be
 * non-NULL
 * @user_data: user data passed onto the request
 *
 * #GumUserServiceListCb defines the callback which is used when list of users
 * are to be retrieved based on the user types.
 */

/**
 * GumUserService:
 *
 * Opaque structure for the object.
 */

/**
 * GumUserServiceClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

G_DEFINE_TYPE (GumUserService, gum_user_service, G_TYPE_OBJECT)

#define GUM_USER_SERVICE_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_USER_SERVICE, GumUserServicePrivate)

typedef struct {
    gpointer callback;
    gpointer user_data;
    GError *error;
    GumUserList *users;
    guint cb_id;
} GumUserServiceOp;

struct _GumUserServicePrivate
{
    GumDbusUserService *dbus_service;
    GumdDaemon *offline_service;
    GCancellable *cancellable;
    GumUserServiceOp *op;
    guint dbus_proxy_cb_id;
};

enum
{
    PROP_0,

    PROP_OFFLINE,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static const gchar *_bus_type =
#ifdef GUM_BUS_TYPE_P2P
    "p2p";
#else
    "system";
#endif
static GObject *self = 0;
static GHashTable *dbus_service_objects = NULL;
static GMutex mutex;

static void
_thread_dbus_service_free (
        GWeakRef *data)
{
    g_weak_ref_clear (data);
    g_slice_free (GWeakRef, data);
}

static GumDbusUserService *
_get_dbus_service ()
{
    GumDbusUserService *object = NULL;
    g_mutex_lock (&mutex);

    if (dbus_service_objects != NULL) {
        GWeakRef *ref;
        ref = g_hash_table_lookup (dbus_service_objects, g_thread_self ());
        if (ref != NULL) {
            object = g_weak_ref_get (ref);
        }
    }

    g_mutex_unlock (&mutex);
    return object;
}

static void
_insert_dbus_service (
        GumDbusUserService *object)
{
    g_return_if_fail (GUM_DBUS_IS_USER_SERVICE (object));

    g_mutex_lock (&mutex);

    if (!dbus_service_objects) {
        dbus_service_objects = g_hash_table_new_full (g_direct_hash,
                g_direct_equal, NULL,
                (GDestroyNotify)_thread_dbus_service_free);
    }

    if (object != NULL) {
        GWeakRef *ref = g_slice_new (GWeakRef);
        g_weak_ref_init (ref, object);
        g_hash_table_insert (dbus_service_objects, g_thread_self (), ref);
    }

    g_mutex_unlock (&mutex);
}

static void
_on_dbus_service_destroyed (
        gpointer data,
        GObject *obj)
{
    g_mutex_lock (&mutex);
    if (dbus_service_objects) {
        g_hash_table_remove (dbus_service_objects, g_thread_self ());
        if (g_hash_table_size (dbus_service_objects) <= 0) {
            g_hash_table_unref (dbus_service_objects);
            dbus_service_objects = NULL;
        }
    }
    g_mutex_unlock (&mutex);
}

static void
_free_op (
        GumUserService *self)
{
    if (self &&
        self->priv->op) {
        if (self->priv->op->cb_id > 0) {
            g_source_remove (self->priv->op->cb_id);
            self->priv->op->cb_id = 0;
        }
        if (self->priv->op->error) g_error_free (self->priv->op->error);
        gum_user_service_list_free (self->priv->op->users);
        g_free (self->priv->op);
        self->priv->op = NULL;
    }
}

static void
_create_op (
        GumUserService *self,
        gpointer callback,
        gpointer user_data)
{
    GumUserServiceOp *op = g_malloc0 (sizeof (GumUserServiceOp));
    op->callback = callback;
    op->user_data = user_data;
    _free_op (self);
    self->priv->op = op;
}

static gboolean
_trigger_callback (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_USER_SERVICE (user_data), FALSE);

    GumUserService *user_service = GUM_USER_SERVICE (user_data);
    if (user_service->priv->op) {
        if (user_service->priv->op->callback) {
            ((GumUserServiceCb)user_service->priv->op->callback) (user_service,
                user_service->priv->op->error,
                user_service->priv->op->user_data);
        }
        user_service->priv->op->cb_id = 0;
    }
    return FALSE;
}

static void
_setup_idle_callback (
        GumUserService *self,
        const GError *error)
{
    if (!self->priv->op || !self->priv->op->callback) return;
    if (error) {
        if (self->priv->op->error) g_clear_error (&self->priv->op->error);
        self->priv->op->error = g_error_copy (error);
    }
    self->priv->op->cb_id = g_idle_add (_trigger_callback, self);
}

static gboolean
_service_dbus_proxy_callback (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_USER_SERVICE (user_data), FALSE);
    const gchar *env = NULL;
    GumDbusUserService *dbus_service = NULL;
    GDBusConnection *connection = NULL;
    GError *error = NULL;
    const gchar *bus_name = NULL;

    GumUserService *self = GUM_USER_SERVICE (user_data);
    self->priv->dbus_proxy_cb_id = 0;

    if (!self->priv->dbus_service)
        self->priv->dbus_service = _get_dbus_service ();

    if (self->priv->dbus_service) {
        _setup_idle_callback (self, NULL);
        return FALSE;
    }

    env = getenv ("GUM_BUS_TYPE");
    if (env)
        _bus_type = env;
    if (g_strcmp0 (_bus_type, "p2p") == 0) {
        gchar *bus_address = g_strdup_printf (GUM_DBUS_ADDRESS,
            g_get_user_runtime_dir());
        connection = g_dbus_connection_new_for_address_sync (bus_address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, &error);
        g_free (bus_address);
    } else {
        connection = g_bus_get_sync (GUM_BUS_TYPE, NULL, &error);
        bus_name = GUM_SERVICE;
    }

    dbus_service = gum_dbus_user_service_proxy_new_sync (connection,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, bus_name,
            GUM_USER_SERVICE_OBJECTPATH, NULL, &error);
    if (G_LIKELY (error == NULL)) {
        g_object_weak_ref (G_OBJECT (dbus_service), _on_dbus_service_destroyed,
                dbus_service);
        _insert_dbus_service (dbus_service);
        gum_error_quark ();
        self->priv->dbus_service = dbus_service;
        _setup_idle_callback (self, NULL);
    } else {
        WARN ("Unable to start user service: %s", error->message);
        _setup_idle_callback (self, error);
        g_clear_error (&error);
    }
    return FALSE;
}

static gboolean
_trigger_user_list_callback (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_USER_SERVICE (user_data), FALSE);

    GumUserService *self = GUM_USER_SERVICE (user_data);
    if (self->priv->op) {
        if (self->priv->op->callback) {
            ((GumUserServiceListCb)self->priv->op->callback) (self,
                    self->priv->op->users, self->priv->op->error,
                    self->priv->op->user_data);
            self->priv->op->users = NULL;
        }
        self->priv->op->cb_id = 0;
    }
    return FALSE;
}

static void
_setup_idle_user_list_callback (
        GumUserService *self,
        GumUserList *users,
        const GError *error)
{
    if (!self->priv->op || !self->priv->op->callback) return;
    if (error) {
        if (self->priv->op->error) g_clear_error (&self->priv->op->error);
        self->priv->op->error = g_error_copy (error);
    }
    self->priv->op->users = users;
    self->priv->op->cb_id = g_idle_add (_trigger_user_list_callback, self);
}

static void
_setup_idle_dbus_proxy_callback (
        GumUserService *self)
{
    if (self->priv->dbus_proxy_cb_id > 0)
        return;
    self->priv->dbus_proxy_cb_id = g_idle_add (
            _service_dbus_proxy_callback, self);
}

static void
_gum_user_service_list_free (
        gpointer ptr)
{
    GumUserService *self = (GumUserService *) ptr;
    GUM_OBJECT_UNREF (self);
}

static GumUserList *
_uids_variant_to_user_list(
        GVariant *uids,
        gboolean offline)
{
    GumUserList *users = NULL;
    if (uids) {
        GumUser *user = NULL;
        GVariantIter iter;
        GVariant *value;
        uid_t uid;

        g_variant_iter_init (&iter, uids);
        while ((value = g_variant_iter_next_value (&iter))) {
            uid = g_variant_get_uint32 (value);
            if (uid != GUM_USER_INVALID_UID) {
                user = gum_user_get_sync (uid, offline);
                if (user)
                    users = g_list_append (users, user);
                else
                    WARN ("unable to get user for uid %d", uid);
            }
            g_variant_unref (value);
        }
    }
    return users;
}

static void
_on_get_user_list_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GumUserService *self = (GumUserService*)user_data;
    GumDbusUserService *proxy = GUM_DBUS_USER_SERVICE (object);
    GVariant *uids = NULL;
    GError *error = NULL;
    GumUserList *users = NULL;

    g_return_if_fail (self != NULL);

    DBG ("");

    gum_dbus_user_service_call_get_user_list_finish (proxy, &uids,
            res, &error);

    if (GUM_OPERATION_IS_NOT_CANCELLED (error)) {
        if (!error) {
            users = _uids_variant_to_user_list (uids, FALSE);
        }
        _setup_idle_user_list_callback (self, users, error);
    }
    g_variant_unref (uids);
    g_clear_error (&error);
}

static GObject*
_constructor (GType type,
              guint n_construct_params,
              GObjectConstructParam *construct_params)
{
    /*
     * Singleton object
     */
    if (!self) {
        self = G_OBJECT_CLASS (gum_user_service_parent_class)->constructor (
                  type, n_construct_params, construct_params);
    }
    else {
        g_object_ref (self);
    }

    return self;
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumUserService *self = GUM_USER_SERVICE (object);
    switch (property_id) {
        case PROP_OFFLINE: {
            if (g_value_get_boolean (value)) {
                self->priv->offline_service = gumd_daemon_new ();
            } else {
                self->priv->cancellable = g_cancellable_new ();
            }
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    GumUserService *self = GUM_USER_SERVICE (object);
    switch (property_id) {
        case PROP_OFFLINE: {
            g_value_set_boolean (value, self->priv->offline_service != NULL);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (GObject *object)
{
    GumUserService *self = GUM_USER_SERVICE(object);
    if (self->priv->dbus_proxy_cb_id > 0) {
        g_source_remove (self->priv->dbus_proxy_cb_id);
        self->priv->dbus_proxy_cb_id = 0;
    }

    _free_op (self);

    if (self->priv->cancellable) {
        g_cancellable_cancel (self->priv->cancellable);
        g_object_unref (self->priv->cancellable);
        self->priv->cancellable = NULL;
    }

    GUM_OBJECT_UNREF (self->priv->dbus_service);

    GUM_OBJECT_UNREF (self->priv->offline_service);

    G_OBJECT_CLASS (gum_user_service_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    G_OBJECT_CLASS (gum_user_service_parent_class)->finalize (object);
    self = NULL;
}

static void
gum_user_service_init (
        GumUserService *self)
{
    self->priv = GUM_USER_SERVICE_PRIV (self);
    self->priv->cancellable = NULL;
    self->priv->dbus_service = NULL;
    self->priv->offline_service = NULL;
    self->priv->op = NULL;
    self->priv->dbus_proxy_cb_id = 0;
}

static void
gum_user_service_class_init (
        GumUserServiceClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumUserServicePrivate));

    object_class->constructor = _constructor;
    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    /**
     * GumUserService:offline:
     *
     * This property is used to define the behaviour of synchronous operation
     * performed on the object. If it is set to TRUE, then the object performs
     * the operation without daemon (dbus/gumd) otherwise 'gumd' daemon will be
     * used for the required synchronous functionality.
     */
    properties[PROP_OFFLINE] =  g_param_spec_boolean ("offline",
            "Offline",
            "Operational mode for the User object",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);
}

/**
 * gum_user_service_create:
 * @callback: #GumUserServiceCb to be invoked when new object is created
 * @user_data: user data
 *
 * This method creates a new user object over the DBus asynchronously.
 * Callback is used to notify when the remote object is fully created and
 * accessible.
 *
 * NOTE: #GumUserService is a singleton object. If the object is already
 * created earlier, same object will be returned in the callback.
 *
 * Returns: (transfer full): #GumUserService newly created object if
 * successful, NULL otherwise.
 */
GumUserService *
gum_user_service_create (
        GumUserServiceCb callback,
        gpointer user_data)
{
    GumUserService *self = GUM_USER_SERVICE (
            g_object_new (GUM_TYPE_USER_SERVICE, "offline", FALSE, NULL));

    g_return_val_if_fail (self != NULL, NULL);
    _create_op (self, callback, user_data);
    _setup_idle_dbus_proxy_callback (self);
    return self;
}

/**
 * gum_user_service_create_sync:
 * @offline: enables or disables the offline mode
 *
 * This method creates a new user service object. In case offline mode is
 * enabled, then the object is created directly without using dbus otherwise
 * the objects gets created over the DBus synchronously.
 * NOTE: #GumUserService is a singleton object.
 *
 * Returns: (transfer full): #GumUserService newly created object
 */
GumUserService *
gum_user_service_create_sync (
        gboolean offline)
{
    GumUserService *self = GUM_USER_SERVICE (g_object_new (
            GUM_TYPE_USER_SERVICE, "offline", offline, NULL));
    _service_dbus_proxy_callback (self);
    return self;
}

/**
 * gum_user_service_get_user_list:
 * @self: #GumUserService object
 * @types: (transfer none): a string array of user types (e.g admin, normal etc)
 * @callback: #GumUserServiceListCb to be invoked when user list is retrieved
 * @user_data: user data
 *
 * This method gets the user over the DBus asynchronously. Callback is used
 * to notify when the user list is retrieved.
 *
 * Returns: returns TRUE if the request has been pushed and is waiting for
 * the response, FALSE otherwise. No callback is triggered, in case the
 * function returns FALSE.
 */
gboolean
gum_user_service_get_user_list (
        GumUserService *self,
        const gchar *const *types,
        GumUserServiceListCb callback,
        gpointer user_data)
{
    DBG ("");
    g_return_val_if_fail (GUM_IS_USER_SERVICE (self), FALSE);

    if (!self->priv->dbus_service) {
        WARN ("Remote dbus object not valid");
        return FALSE;
    }
    _create_op (self, (gpointer)callback, user_data);
    gum_dbus_user_service_call_get_user_list (self->priv->dbus_service,
            types, self->priv->cancellable, _on_get_user_list_cb, self);

    return TRUE;
}

/**
 * gum_user_service_get_user_list_sync:
 * @self: #GumUserService object
 * @types: (transfer none): a string array of user types (e.g admin, normal etc)
 *
 * This method gets the list of users based on the type. In case offline mode is
 * enabled, then the users is retrieved directly without using dbus otherwise
 * the users gets retrieved over the DBus synchronously.
 *
 * Returns: (transfer full): #GumUserList of #GumUser. use
 * gum_user_list_free to free the list of the users.
 */
GumUserList *
gum_user_service_get_user_list_sync (
        GumUserService *self,
        const gchar *const *types)
{
    GError *error = NULL;
    gboolean rval = FALSE;
    GumUserList *users = NULL;
    GVariant *uids = NULL;

    DBG ("");
    g_return_val_if_fail (GUM_IS_USER_SERVICE (self), NULL);

    if (self->priv->offline_service) {
        uids = gumd_daemon_get_user_list (self->priv->offline_service,
                types, &error);
        if (uids) {
            users = _uids_variant_to_user_list (uids, TRUE);
            g_variant_unref (uids);
        }
    } else if (self->priv->dbus_service) {
        rval = gum_dbus_user_service_call_get_user_list_sync (
                self->priv->dbus_service, types, &uids, NULL, &error);
        if (!rval && error) {
            WARN ("Failed with error %d:%s", error->code, error->message);
            g_error_free (error);
            error = NULL;
        } else {
            users = _uids_variant_to_user_list (uids, FALSE);
            g_variant_unref (uids);
        }
    }
    return users;
}

/**
 * gum_user_service_list_free:
 * @users: #GList of #GumUser
 *
 * Frees the list and its elements completely.
 *
 */
void
gum_user_service_list_free (
        GumUserList *users)
{
    if (users)
        g_list_free_full (users, _gum_user_service_list_free);
}

/**
 * gum_user_service_get_dbus_proxy:
 * @self: #GumUserService object
 *
 * Gets the dbus proxy of the singleton #GumUserService object.
 *
 * Returns: (transfer none): #GDBusProxy object.
 */
GDBusProxy *
gum_user_service_get_dbus_proxy (
        GumUserService *self)
{
    g_return_val_if_fail (GUM_IS_USER_SERVICE (self) &&
            self->priv->dbus_service, NULL);
    return G_DBUS_PROXY (self->priv->dbus_service);

}

