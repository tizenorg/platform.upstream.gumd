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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common/gum-utils.h"
#include "common/gum-log.h"

/**
 * SECTION:gum-utils
 * @short_description: Utility functions
 * @title: Gum Utils
 * @include: gum/common/gum-utils.h
 *
 */

typedef struct __nonce_ctx_t
{
    gboolean initialized;
    guint32 serial;
    guchar key[32];
    guchar entropy[16];
} _nonce_ctx_t;

static _nonce_ctx_t _nonce_ctx = { 0, };
G_LOCK_DEFINE_STATIC (_nonce_lock);

static gboolean
_init_nonce_gen ()
{
    if (G_LIKELY(_nonce_ctx.initialized))
        return TRUE;

    int fd;

    fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0)
        goto init_exit;
    if (read (fd, _nonce_ctx.key, sizeof (_nonce_ctx.key)) !=
        sizeof (_nonce_ctx.key))
        goto init_close;
    if (read (fd, _nonce_ctx.entropy, sizeof(_nonce_ctx.entropy)) !=
        sizeof (_nonce_ctx.entropy))
        goto init_close;

    _nonce_ctx.serial = 0;

    _nonce_ctx.initialized = TRUE;

init_close:
    close (fd);

init_exit:
    return _nonce_ctx.initialized;
}

/**
 * gum_generate_nonce:
 * @algorithm: the #GChecksumType algorithm
 *
 * Generates nonce based on hashing algorithm as specified in @algorithm
 *
 * Returns: (transfer full): generate nonce if successful, NULL otherwise.
 */
gchar *
gum_generate_nonce (
        GChecksumType algorithm)
{
    GHmac *hmac;
    gchar *nonce = NULL;
    struct timespec ts;

    G_LOCK (_nonce_lock);

    if (G_UNLIKELY (!_init_nonce_gen()))
        goto nonce_exit;

    hmac = g_hmac_new (algorithm, _nonce_ctx.key, sizeof (_nonce_ctx.key));

    g_hmac_update (hmac, _nonce_ctx.entropy, sizeof (_nonce_ctx.entropy));

    _nonce_ctx.serial++;
    g_hmac_update (hmac, (const guchar *) &_nonce_ctx.serial,
            sizeof (_nonce_ctx.serial));

    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
        g_hmac_update (hmac, (const guchar *) &ts, sizeof (ts));

    memset (&ts, 0x00, sizeof(ts));
    nonce = g_strdup (g_hmac_get_string (hmac));
    g_hmac_unref (hmac);

nonce_exit:
    G_UNLOCK (_nonce_lock);

    return nonce;
}

