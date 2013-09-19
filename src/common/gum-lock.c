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

static gint lock_count = 0;

gboolean
gum_lock_pwdf_lock ()
{
    if (lock_count == 0) {
        /* when run in debug mode, normal user may not have privileges to get
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
