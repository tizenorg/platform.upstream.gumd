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

#ifndef __GUM_CONFIG_DBUS_H_
#define __GUM_CONFIG_DBUS_H_

/**
 * SECTION:gum-config-dbus
 * @title: DBus configuration
 * @short_description: gum dbus configuration keys
 * @include: gum/common/gum-config.h
 *
 * DBus configuration keys are defined below. See #GumConfig for how to use
 * them.
 */

/**
 * GUM_CONFIG_DBUS_TIMEOUTS:
 *
 * A prefix for dbus timeout keys. Should be used only when defining new keys.
 */
#define GUM_CONFIG_DBUS_TIMEOUTS           "ObjectTimeouts"

/**
 * GUM_CONFIG_DBUS_DAEMON_TIMEOUT:
 *
 * A timeout in seconds, after which the gum daemon will exit. If not set (or
 * set to 0), the daemon will not exit. Has no effect if P2P DBus is in use.
 *
 * Can be overriden in debug builds by setting UM_DAEMON_TIMEOUT environment
 * variable.
 */
#define GUM_CONFIG_DBUS_DAEMON_TIMEOUT     GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/DAEMON_TIMEOUT"

/**
 * GUM_CONFIG_DBUS_USER_TIMEOUT:
 *
 * A timeout in seconds, after which inactive user dbus objects will be removed.
 * If not set (or set to 0), the dbus objects will persist.
 *
 * Can be overriden in debug builds by setting UM_USER_TIMEOUT
 * environment variable.
 */
#define GUM_CONFIG_DBUS_USER_TIMEOUT       GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/USER_TIMEOUT"

/**
 * GUM_CONFIG_DBUS_GROUP_TIMEOUT:
 *
 * A timeout in seconds, after which inactive group dbus objects will be
 * removed. If not set (or set to 0), the dbus objects will persist.
 *
 * Can be overriden in debug builds by setting UM_GROUP_TIMEOUT
 * environment variable.
 */
#define GUM_CONFIG_DBUS_GROUP_TIMEOUT      GUM_CONFIG_DBUS_TIMEOUTS \
                                                "/GROUP_TIMEOUT"
#endif /* __GUM_CONFIG_DBUS_H_ */
