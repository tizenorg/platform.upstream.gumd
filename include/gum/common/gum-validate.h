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

#ifndef __GUM_NAME_H_
#define __GUM_NAME_H_

#include <glib.h>

G_BEGIN_DECLS

#define GUM_NAME_PATTERN "^[A-Za-z_][A-Za-z0-9_.-]*[A-Za-z0-9_.$-]\\?$"
/*no control, comma, and colon allowed*/
#define GUM_DB_ENTRY_PATTERN "^[^\x2C\x3A[:cntrl:]]*$"

gboolean
gum_validate_name (
        const gchar *name,
        GError **error);

gchar *
gum_validate_generate_username (
        const gchar *str,
        GError **error);

gboolean
gum_validate_db_string_entry_regx (
        const gchar *str,
        GError **error);

gboolean
gum_validate_db_string_entry (
        const gchar *str,
        GError **error);

gboolean
gum_validate_db_secret_entry (
        const gchar *str,
        GError **error);

G_END_DECLS

#endif /* __GUM_NAME_H_ */
