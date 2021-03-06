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

#ifndef __GUM_DEFINES_H_
#define __GUM_DEFINES_H_

#include <glib.h>

#define GUM_OBJECT_UNREF(o) { \
    if (o) { \
        g_object_unref (o); \
        o = NULL; \
    } \
    }

#define GUM_HASHTABLE_UNREF(o) { \
    if (o) { \
        g_hash_table_unref (o); \
        o = NULL; \
    } \
    }

#define GUM_USER_INVALID_UID G_MAXUINT

#define GUM_GROUP_INVALID_GID G_MAXUINT

#endif /* __GUM_DEFINES_H_ */
