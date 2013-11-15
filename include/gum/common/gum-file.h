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

#ifndef __GUM_FILE_H_
#define __GUM_FILE_H_

#include <glib.h>
#include <glib-object.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <gshadow.h>
#include <gio/gio.h>

#include <common/gum-config.h>

G_BEGIN_DECLS

typedef enum {
    GUM_OPTYPE_ADD = 1,
    GUM_OPTYPE_DELETE = 2,
    GUM_OPTYPE_MODIFY = 3
} GumOpType;

typedef gboolean (*GumFileUpdateCB) (
        GObject *object,
        GumOpType op,
        FILE *source_file,
        FILE *dup_file,
        gpointer user_data,
        GError **error);

gboolean
gum_file_update (
        GObject *object,
        GumOpType op,
        GumFileUpdateCB callback,
        const gchar *source_file_path,
        gpointer user_data,
        GError **error);

gboolean
gum_file_open_db_files (
        const gchar *source_file_path,
        const gchar *dup_file_path,
        FILE **source_file,
        FILE **dup_file,
        GError **error);

gboolean
gum_file_close_db_files (
        const gchar *source_file_path,
        const gchar *dup_file_path,
        FILE *source_file,
        FILE *dup_file,
        GError **error);

struct passwd *
gum_file_getpwnam (
        const gchar *usrname,
        GumConfig *config);

struct passwd *
gum_file_getpwuid (
        uid_t uid,
        GumConfig *config);

struct passwd *
gum_file_find_user_by_gid (
        gid_t gid,
        GumConfig *config);

struct spwd *
gum_file_getspnam (
        const gchar *usrname,
        GumConfig *config);

struct group *
gum_file_getgrnam (
        const gchar *grname,
        GumConfig *config);

struct group *
gum_file_getgrgid (
        gid_t gid,
        GumConfig *config);

struct sgrp *
gum_file_getsgnam (
        const gchar *grpname,
        GumConfig *config);

GFile *
gum_file_new_path (
		const gchar *dir,
		const gchar *fname);

gboolean
gum_file_create_home_dir (
        GumConfig *config,
        const gchar *usr_home_dir,
        uid_t uid,
        gid_t gid,
        GError **error);

gboolean
gum_file_delete_home_dir (
        const gchar *dir,
        GError **error);

G_END_DECLS

#endif /* __GUM_FILE_H_ */
