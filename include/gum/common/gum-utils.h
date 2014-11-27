/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *          Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _GUM_UTILS_H_
#define _GUM_UTILS_H_

#include <glib.h>
#include <pwd.h>

G_BEGIN_DECLS

gchar *
gum_utils_generate_nonce (
        GChecksumType algorithm);

void
gum_utils_drop_privileges ();

void
gum_utils_gain_privileges ();

gboolean
gum_utils_run_user_scripts (
        const gchar *script_dir,
        const gchar *username,
        uid_t uid,
        gid_t gid,
        const gchar *homedir,
        const gchar *usertype);

gboolean
gum_utils_run_group_scripts (
        const gchar *script_dir,
        const gchar *groupname,
        gid_t gid,
        uid_t uid);

G_END_DECLS

#endif  /* _GUM_UTILS_H_ */

