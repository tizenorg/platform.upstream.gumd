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

#ifndef __GUM_CONFIG_H_
#define __GUM_CONFIG_H_

#include <glib.h>
#include <glib-object.h>

#include "gum-config-general.h"
#include "gum-config-dbus.h"

/**
 * GUM_UMASK:
 *
 * Value used to set the mode of home directories created for new users.
 */
#define GUM_UMASK  077

G_BEGIN_DECLS

#define GUM_TYPE_CONFIG            (gum_config_get_type())
#define GUM_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUM_TYPE_CONFIG, GumConfig))
#define GUM_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUM_TYPE_CONFIG, GumConfigClass))
#define GUM_IS_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUM_TYPE_CONFIG))
#define GUM_IS_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUM_TYPE_CONFIG))
#define GUM_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUM_TYPE_CONFIG, GumConfigClass))

typedef struct _GumConfig GumConfig;
typedef struct _GumConfigClass GumConfigClass;
typedef struct _GumConfigPrivate GumConfigPrivate;

struct _GumConfig
{
    GObject parent;

    /* priv */
    GumConfigPrivate *priv;
};

struct _GumConfigClass
{
    GObjectClass parent_class;
};

GType
gum_config_get_type (void) G_GNUC_CONST;

GumConfig *
gum_config_new ();

gint
gum_config_get_int (
        GumConfig *self,
        const gchar *key,
        gint retval);

void
gum_config_set_int (
        GumConfig *self,
        const gchar *key,
        gint value);

guint
gum_config_get_uint (
        GumConfig *self,
        const gchar *key,
        guint retval);

void
gum_config_set_uint (
        GumConfig *self,
        const gchar *key,
        guint value);

const gchar*
gum_config_get_string (
        GumConfig *self,
        const gchar *key);

void
gum_config_set_string (
        GumConfig *self,
        const gchar *key,
        const gchar *value);

G_END_DECLS

#endif /* __GUM_CONFIG_H_ */
