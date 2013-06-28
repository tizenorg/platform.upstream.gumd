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

#ifndef __GUM_CONFIG_DBUS_H_
#define __GUM_CONFIG_DBUS_H_

#define GUM_CONFIG_DBUS_TIMEOUTS  "ObjectTimeouts"

#define GUM_CONFIG_DBUS_DAEMON_TIMEOUT     GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/DAEMON_TIMEOUT"
#define GUM_CONFIG_DBUS_USER_TIMEOUT      GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/USER_TIMEOUT"
#define GUM_CONFIG_DBUS_GROUP_TIMEOUT     GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/GROUP_TIMEOUT"
#endif /* __GUM_CONFIG_DBUS_H_ */
