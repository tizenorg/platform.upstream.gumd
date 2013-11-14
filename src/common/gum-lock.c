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
#include "config.h"

#include <shadow.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "common/gum-lock.h"
#include "common/gum-log.h"

/**
 * SECTION:gum-lock
 * @short_description: Utility functions for (un)locking user/group database
 * @title: Gum Lock
 * @include: gum/common/gum-lock.h
 *
 * Locking the user/group database is required when modifications are needed to
 * the database e.g. adding and deleting users. Lock can be called multiple
 * times as it merely increases the lock counter after acquiring the lock for
 * the first time. Similarly unlock can be called multiple times as it decreases
 * lock counter until it the count reaches to 0 when the lock is released.
 * Locking and unlocking the database is disabled for when testing is enabled
 * as tests are run on dummy databases.
 *
 * |[
 *   //return value must be checked if the lock succeed or not.
 *   gboolean ret = gum_lock_pwdf_lock ();
 *
 *   //return value must be checked if the unlock succeed or not.
 *   ret = gum_lock_pwdf_unlock ();
 * ]|
 */

static gint lock_count = 0;

/**
 * gum_lock_pwdf_lock:
 *
 * Gets the lock to the user/group database file(s). Lock can be called multiple
 * times as it merely increases the lock counter after acquiring the lock for
 * the first time. In order to release the lock, unlock needs to be called
 * the same number of times as that of lock. Locking and unlocking the database
 * is disabled for when testing is enabled as tests are run on dummy databases.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gum_lock_pwdf_lock ()
{
    if (lock_count == 0) {
        /* when run in test mode, normal user may not have privileges to get
         * the lock */
#ifndef ENABLE_TESTS
        DBG ("Before: ruid:%d euid:%d rgid:%d egid:%d ", getuid (), geteuid (),
                getgid (), getegid ());
        if (seteuid (0))
            WARN ("seteuid(0) failed");
        if (setegid (0))
            WARN ("setegid(0) failed");
        DBG ("After: ruid:%d euid:%d rgid:%d egid:%d ", getuid (), geteuid (),
                getgid (), getegid ());
        if (lckpwdf () < 0) {
            DBG ("pwd lock failed %s", strerror (errno));
            return FALSE;
        }
#endif
    }
    lock_count++;
    return TRUE;
}

/**
 * gum_lock_pwdf_unlock:
 *
 * Releases the lock to the user/group database file(s). Unlock can be called
 * multiple times as it decreases lock counter until it the count reaches to 0
 * when the lock is released. In order to release the lock, unlock needs to be
 * called the same number of times as that of lock. Locking and unlocking the
 * database is disabled for when testing is enabled as tests are run on dummy
 * databases.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gum_lock_pwdf_unlock ()
{
    if (lock_count > 0) {
    	lock_count--;
    	if (lock_count == 0) {
#ifndef ENABLE_TESTS
    	    if (ulckpwdf () < 0) {
    	        DBG ("pwd unlock failed %s", strerror (errno));
    	        return FALSE;
    	    }
            DBG ("Before: ruid:%d euid:%d rgid:%d egid:%d ", getuid (),
                    geteuid (), getgid (), getegid ());
    	    if (seteuid (getuid()))
    	        WARN ("seteuid() failed");
    	    if (setegid (getgid()))
    	        WARN ("setegid() failed");
            DBG ("After: ruid:%d euid:%d rgid:%d egid:%d ", getuid (),
                    geteuid (), getgid (), getegid ());
#endif
    	}
    } else if (lock_count <= 0) {
    	return FALSE;
    }

    return TRUE;
}
