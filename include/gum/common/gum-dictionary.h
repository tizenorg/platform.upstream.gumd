/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2012-2013 Intel Corporation.
 *
 * Contact: Alexander Kanavin <alex.kanavin@gmail.com>
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

#ifndef __GUM_DICTIONARY_H__
#define __GUM_DICTIONARY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GUM_TYPE_DICTIONARY (G_TYPE_HASH_TABLE)

#define GUM_DICTIONARY(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GUM_TYPE_DICTIONARY, \
                                           GumDictionary))
#define GUM_IS_DICTIONARY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                           GUM_TYPE_DICTIONARY))

typedef GHashTable GumDictionary;

GumDictionary *
gum_dictionary_new (void);

void
gum_dictionary_ref (GumDictionary *dict);

void
gum_dictionary_unref (GumDictionary *dict);

GumDictionary *
gum_dictionary_copy (GumDictionary *other);

GumDictionary *
gum_dictionary_new_from_variant (GVariant *variant);

GVariant *
gum_dictionary_to_variant (GumDictionary *self);

GVariant *
gum_dictionary_get (GumDictionary *dict, const gchar *key);

gboolean
gum_dictionary_set (GumDictionary *dict,
    const gchar *key, GVariant *value);

gboolean
gum_dictionary_get_boolean (GumDictionary *dict, const gchar *key,
                                 gboolean *value);

gboolean
gum_dictionary_set_boolean (GumDictionary *dict, const gchar *key,
                                 gboolean value);

gboolean
gum_dictionary_get_int32 (GumDictionary *dict, const gchar *key,
                               gint *value);

gboolean
gum_dictionary_set_int32 (GumDictionary *dict, const gchar *key,
                               gint value);

gboolean
gum_dictionary_get_uint32 (GumDictionary *dict, const gchar *key,
                                guint *value);

gboolean
gum_dictionary_set_uint32 (GumDictionary *dict, const gchar *key,
                                guint32 value);

gboolean
gum_dictionary_get_int64 (GumDictionary *dict, const gchar *key,
                               gint64 *value);

gboolean
gum_dictionary_set_int64 (GumDictionary *dict, const gchar *key,
                               gint64 value);

gboolean
gum_dictionary_get_uint64 (GumDictionary *dict, const gchar *key,
                                guint64 *value);

gboolean
gum_dictionary_set_uint64 (GumDictionary *dict, const gchar *key,
                                guint64 value);

const gchar *
gum_dictionary_get_string (GumDictionary *dict, const gchar *key);

gboolean
gum_dictionary_set_string (GumDictionary *dict, const gchar *key,
                                const gchar *value);

gboolean
gum_dictionary_remove (GumDictionary *dict, const gchar *key);

G_END_DECLS

#endif /* __GUM_DICTIONARY_H__ */
