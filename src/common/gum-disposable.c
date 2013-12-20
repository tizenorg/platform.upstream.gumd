/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Imran Zaman <imran.zaman@intel.com>
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

#include "common/gum-disposable.h"
#include "common/gum-log.h"

/**
 * SECTION:gum-disposable
 * @short_description: timer-based auto disposable object
 * @include: gum/common/gum-disposable.h
 *
 * #GumDisposable is used to dispose the object when the timer expires with the
 * specified timeout value provided auto-dispose is enabled. A #GObject which
 * needs to be auto-disposable should derive itself from #GumDisposable and
 * can set the timeout value (at the time of object construction e.g.). One such
 * example, which can make use of #GumDisposable, is a DBus object as it may
 * be required to destory after some period of inactivity.
 *
 * <refsect1><title>Usage</title></refsect1>
 * Following code snippet demonstrates how to derive and use #GumDisposable:
 * |[
 *
 * //gum-test-object.h
 * struct _GumTestObject
 * {
 *     GumDisposable parent;
 * };
 *
 * struct _GumTestObjectClass
 * {
 *     GumDisposableClass parent_class;
 * };
 *
 * //gum-test-object.c
 * GumTestObject *
 * gum_test_object_new ()
 * {
 *    GumTestObject *obj = g_object_new (GUM_TYPE_TEST_OBJECT, NULL);
 *
 *    // ... other code
 *
 *    // GumTestobject will be disposed after 5 seconds unless auto-dispose
 *    // is disabled by calling gum_disposable_set_auto_dispose (obj, FALSE)
 *    gint timeout = 5;
 *    gum_disposable_set_timeout (GUM_DISPOSABLE (obj), timeout);
 *
 *    // ... other code
 * }
 *
 * ]|
 *
 */

/**
 * GumDisposable:
 *
 * Opaque structure for the object.
 */

/**
 * GumDisposableClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

struct _GumDisposablePrivate
{
    guint timeout;       /* timeout in seconds */
    volatile gint  keep_obj_counter; /* keep object request counter */
    guint timer_id;      /* timer source id */
    gboolean delete_later;
};

enum {
    PROP_0,
    PROP_TIMEOUT,
    PROP_AUTO_DISPOSE,
    PROP_DELETE_LATER,
    PROP_MAX,
};

/* signals */
enum {
    SIG_DISPOSING,
    SIG_MAX
};

static GParamSpec *properties[PROP_MAX];
static guint       signals[SIG_MAX];

#define GUM_DISPOSABLE_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_DISPOSABLE, GumDisposablePrivate)

G_DEFINE_ABSTRACT_TYPE (GumDisposable, gum_disposable, G_TYPE_OBJECT);

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    g_return_if_fail (object && GUM_IS_DISPOSABLE (object));

    GumDisposable *self = GUM_DISPOSABLE (object);

    switch (property_id) {
    case PROP_TIMEOUT:
        gum_disposable_set_timeout (self, g_value_get_uint (value));
        break;
    case PROP_AUTO_DISPOSE:
        gum_disposable_set_auto_dispose (self,
                g_value_get_boolean (value));
        break;
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
    g_return_if_fail (object && GUM_IS_DISPOSABLE (object));

    GumDisposable *self = GUM_DISPOSABLE (object);

    switch (property_id) {
    case PROP_TIMEOUT:
        g_value_set_int (value, self->priv->timeout);
        break;
    case PROP_AUTO_DISPOSE:
        g_value_set_boolean (value, gum_disposable_get_auto_dispose(self));
        break;
    case PROP_DELETE_LATER:
        g_value_set_boolean (value, self->priv->delete_later);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (
        GObject *object)
{
    g_return_if_fail (object && GUM_IS_DISPOSABLE (object));

    GumDisposable *self = GUM_DISPOSABLE (object);

    DBG ("%s DISPOSE", G_OBJECT_TYPE_NAME (self));
    if (self->priv->timer_id) {
        DBG (" - TIMER CLEAR");
        g_source_remove (self->priv->timer_id);
        self->priv->timer_id = 0;
    }

    G_OBJECT_CLASS (gum_disposable_parent_class)->dispose (object);
}

static void
_finalize (
        GObject *object)
{
    g_return_if_fail (object && GUM_IS_DISPOSABLE (object));

    DBG ("FINALIZE");
    G_OBJECT_CLASS (gum_disposable_parent_class)->finalize (object);
}

static void
gum_disposable_class_init (
        GumDisposableClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumDisposablePrivate));

    object_class->set_property = _set_property;
    object_class->get_property = _get_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    /**
     * GumDisposable:timeout:
     *
     * This property holds the value of timeout in seconds. Default value is 0.
     * If timeout value is 0, the object never auto-dispose.
     */
    properties[PROP_TIMEOUT] = 
            g_param_spec_uint ("timeout",
                    "Object timeout",
                    "object timeout",
                    0,
                    G_MAXUINT,
                    0,
                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * GumDisposable:auto-dispose:
     *
     * This property holds the value of auto-dispose to be TRUE or FALSE.
     * Default value is FALSE. If auto-dispose is TRUE, then object is
     * auto-disposed otherwise not.
     */
    properties[PROP_AUTO_DISPOSE] =
            g_param_spec_boolean ("auto-dispose",
                    "Auto dispose",
                    "auto dispose",
                    TRUE,
                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * GumDisposable:delete-later:
     *
     * This property is used when the object is asked to be deleted later.
     * The object should not be used once #gum_disposable_delete_later is
     * requested for the object.
     */
    properties[PROP_DELETE_LATER] =
            g_param_spec_boolean ("delete-later",
                    "Delete Later",
                    "delete later",
                    FALSE,
                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, PROP_MAX, properties);

    /**
     * GumDisposable::disposing:
     * @object: the object which emits the signal
     *
     * This signal is emitted when the object is about to be disposed.
     */
    signals[SIG_DISPOSING] = g_signal_new ("disposing",
            GUM_TYPE_DISPOSABLE,
            G_SIGNAL_RUN_FIRST| G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            0,
            NULL, NULL,
            NULL,
            G_TYPE_NONE,
            0,
            G_TYPE_NONE);
}

static void
gum_disposable_init (
        GumDisposable *self)
{
    self->priv = GUM_DISPOSABLE_PRIV (self);

    self->priv->timer_id = 0;
    self->priv->timeout = 0;
    self->priv->delete_later = FALSE;
    g_atomic_int_set(&self->priv->keep_obj_counter, 0);

    DBG ("INIT");
}

static gboolean
_auto_dispose (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_DISPOSABLE (user_data), FALSE);

    GumDisposable *self = GUM_DISPOSABLE (user_data);
    g_signal_emit (self, signals[SIG_DISPOSING], 0);
    /* destroy object */
    DBG ("%s AUTO DISPOSE %d", G_OBJECT_TYPE_NAME (self),
            G_OBJECT (self)->ref_count);
    g_object_unref (G_OBJECT (self));
    return FALSE;
}

static gboolean
_timer_dispose (
        gpointer user_data)
{
    g_return_val_if_fail (user_data && GUM_IS_DISPOSABLE (user_data), FALSE);
    GumDisposable *self = GUM_DISPOSABLE (user_data);

    DBG ("%s TIMER DISPOSE", G_OBJECT_TYPE_NAME (self));
    /* clear out timer since we are already inside timer cb */
    self->priv->timer_id = 0;

    return _auto_dispose (user_data);
}

static void
_update_timer (
        GumDisposable *self)
{
    DBG("%s (%p): keep_obj_counter : %d, timeout : %d  delete_later %d",
            G_OBJECT_TYPE_NAME(self),
            self,
            self->priv->keep_obj_counter,
            self->priv->timeout,
            self->priv->delete_later);

    if (self->priv->delete_later) return;

    if (g_atomic_int_get(&self->priv->keep_obj_counter) == 0) {
        if (self->priv->timeout) {
            DBG("Setting object timeout to %d", self->priv->timeout);
            self->priv->timer_id = g_timeout_add_seconds (self->priv->timeout,
                    _timer_dispose,
                    self);
        }
    } else if (self->priv->timer_id) {
        g_source_remove (self->priv->timer_id);
        self->priv->timer_id = 0;
    }
}

/**
 * gum_disposable_set_auto_dispose:
 * @self: (transfer none): an instance of #GumDisposable
 * @dispose: dispose flag
 *
 * Sets the auto-dispose flag, and sets up the timer if needed.
 *
 */
void
gum_disposable_set_auto_dispose (
        GumDisposable *self,
        gboolean dispose)
{
    g_return_if_fail (self && GUM_IS_DISPOSABLE (self));

    if (g_atomic_int_get(&self->priv->keep_obj_counter) == 0 && dispose) return;

    g_atomic_int_add (&self->priv->keep_obj_counter, !dispose ? +1 : -1);

    _update_timer (self);
}

/**
 * gum_disposable_set_timeout:
 * @self: (transfer none): an instance of #GumDisposable
 * @timeout: timeout value in seconds
 *
 * Sets the timeout value and sets up the timer if needed. If timeout
 * value is set to 0, the object is never disposed.
 */
void
gum_disposable_set_timeout (
        GumDisposable *self,
        guint timeout)
{
    g_return_if_fail (self && GUM_IS_DISPOSABLE (self));

    if (self->priv->timeout == timeout) return;

    self->priv->timeout = timeout;

    _update_timer (self);
}

/**
 * gum_disposable_delete_later:
 * @self: (transfer none): an instance of #GumDisposable
 *
 * Sets the object to be deleted later. Once delete later is requested, the
 * object should not be used.
 */
void
gum_disposable_delete_later (
        GumDisposable *self)
{
    if (self->priv->timer_id)
        g_source_remove (self->priv->timer_id);

    INFO ("Object '%s' (%p) about to dispose...",
            G_OBJECT_TYPE_NAME (self), self);
    self->priv->timer_id = g_idle_add (_auto_dispose, self);
    self->priv->delete_later = TRUE;
}

/**
 * gum_disposable_get_auto_dispose:
 * @self: (transfer none): an instance of #GumDisposable
 *
 * Gets the auto-dispose value.
 *
 * Returns: the auto-dispose value of the object.
 */
gboolean
gum_disposable_get_auto_dispose (
        GumDisposable *self)
{
    g_return_val_if_fail (self && GUM_IS_DISPOSABLE(self), FALSE);

    return g_atomic_int_get(&self->priv->keep_obj_counter) == 0;
}
