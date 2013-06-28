/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#ifndef __GUM_DISPOSABLE_H_
#define __GUM_DISPOSABLE_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GUM_TYPE_DISPOSABLE            (gum_disposable_get_type())
#define GUM_DISPOSABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GUM_TYPE_DISPOSABLE, GumDisposable))
#define GUM_DISPOSABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        GUM_TYPE_DISPOSABLE, GumDisposableClass))
#define GUM_IS_DISPOSABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        GUM_TYPE_DISPOSABLE))
#define GUM_IS_DISPOSABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        GUM_TYPE_DISPOSABLE))
#define GUM_DISPOSABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        GUM_TYPE_DISPOSABLE, GumDisposableClass))

typedef struct _GumDisposable GumDisposable;
typedef struct _GumDisposableClass GumDisposableClass;
typedef struct _GumDisposablePrivate GumDisposablePrivate;

struct _GumDisposable
{
    GObject parent;

    /* priv */
    GumDisposablePrivate *priv;
};

struct _GumDisposableClass
{
    GObjectClass parent_class;
};

GType gum_disposable_get_type (void) G_GNUC_CONST;

void
gum_disposable_set_auto_dispose (GumDisposable *disposable,
                                      gboolean dispose);

gboolean
gum_disposable_get_auto_dispose (GumDisposable *disposable);

void
gum_disposable_set_timeout (GumDisposable *self,
                                 guint timeout);

void
gum_disposable_delete_later (GumDisposable *self);

G_END_DECLS

#endif /* __GUM_DISPOSABLE_H_ */
