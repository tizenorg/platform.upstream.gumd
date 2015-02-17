/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __GUM_DBUS_H_
#define __GUM_DBUS_H_

#include "config.h"

/*
 * Common DBUS definitions
 */
#define GUM_DBUS_ADDRESS         "unix:path=%s/gumd/bus-sock"
#define GUM_SERVICE_PREFIX       "org.O1.SecurityAccounts.gUserManagement"
#define GUM_SERVICE              GUM_SERVICE_PREFIX
#define GUM_USER_SERVICE_OBJECTPATH      \
    "/org/O1/SecurityAccounts/gUserManagement/User"
#define GUM_GROUP_SERVICE_OBJECTPATH     \
    "/org/O1/SecurityAccounts/gUserManagement/Group"

#define GUM_DBUS_FREEDESKTOP_SERVICE    "org.freedesktop.DBus"
#define GUM_DBUS_FREEDESKTOP_PATH       "/org/freedesktop/DBus"
#define GUM_DBUS_FREEDESKTOP_INTERFACE  "org.freedesktop.DBus"

#ifndef GUM_BUS_TYPE
    #define GUM_BUS_TYPE G_BUS_TYPE_NONE
#endif

#endif /* __GUM_DBUS_H_ */
