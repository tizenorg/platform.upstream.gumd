/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2012-2013 Intel Corporation.
 *
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

#ifndef __GUM_LOG_H_
#define __GUM_LOG_H_

#include <glib.h>

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#define TRACEBACK() \
{ \
    void *array[256];\
    size_t size, i;\
    char **strings;\
\
    fprintf (stderr, "Backtrace for: %s %s\n", __FILE__, __PRETTY_FUNCTION__); \
    size = backtrace (array, 256);\
    strings = backtrace_symbols (array, size);\
    if (strings) { \
        for (i=0; i <size; i++) fprintf (stderr, "\t%s\n", strings[i]);\
        free (strings);\
    }\
}

#define INFO(frmt, args...) g_message("%f %s:%d %s " frmt , \
        g_get_monotonic_time()*1.0e-6, __FILE__, __LINE__, \
        __PRETTY_FUNCTION__, ##args)
#define ERR(frmt, args...)  g_critical("%f %s:%d %s " frmt , \
        g_get_monotonic_time()*1.0e-6, __FILE__, __LINE__, \
        __PRETTY_FUNCTION__, ##args)
#define WARN(frmt, args...) g_warning("%f %s:%d %s " frmt , \
        g_get_monotonic_time()*1.0e-6, __FILE__, __LINE__, \
        __PRETTY_FUNCTION__, ##args)
#define DBG(frmt, args...)  g_debug("%f %s:%d %s " frmt , \
        g_get_monotonic_time()*1.0e-6, __FILE__, __LINE__, \
        __PRETTY_FUNCTION__, ##args)

#endif /* __GUM_LOG_H_ */
