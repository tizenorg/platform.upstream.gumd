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

#ifndef __GUM_GROUP_TYPES_H_
#define __GUM_GROUP_TYPES_H_

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:gum-group-types
 * @short_description: group types definition
 * @title: Group types
 *
 * This file provides various types of group that can be created by the user
 * management framework.
 *
 */

/**
 * GumGroupType:
 * @GUM_GROUPTYPE_NONE: group type not defined/set.
 * @GUM_GROUPTYPE_SYSTEM: system group.
 * @GUM_GROUPTYPE_USER: normal group.
 *
 * This enumeration lists group types.
 */

typedef enum {

    GUM_GROUPTYPE_NONE = 0,
    GUM_GROUPTYPE_SYSTEM = 1,
    GUM_GROUPTYPE_USER = 2

} GumGroupType;

G_END_DECLS

#endif /* __GUM_GROUP_TYPES_H_ */
