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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common/gum-crypt.h"
#include "common/gum-log.h"

guchar _salt_chars[64 + 1] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define METHODID_LEN 3
#define SALT_LEN 16
#define SALT_ARRAY_LEN (METHODID_LEN + SALT_LEN + 1)

gchar *
gum_crypt_encrypt_secret (
        const gchar *secret,
        GumCryptMethodID methodid)
{
	ssize_t bytes_read = 0;
	gchar salt[SALT_ARRAY_LEN];
    int fd = 0;
    gint idlen = METHODID_LEN, i = 0;

    if (!secret) return NULL;

    fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return NULL;

    switch (methodid) {
        case GUM_CRYPT_MD5:/* crypt(3) */
            salt[0] = salt[2] = '$';
            salt[1] = '1';
            break;
        case GUM_CRYPT_SHA256:
            salt[0] = salt[2] = '$';
            salt[1] = '5';
            break;
        case GUM_CRYPT_SHA512:
            salt[0] = salt[2] = '$';
            salt[1] = '6';
            break;
        case GUM_CRYPT_DES:
        default:
            idlen = 0;
            break;
    }

    bytes_read = read (fd, &salt[idlen], SALT_LEN);
    close (fd);
    if (bytes_read != SALT_LEN)
        return NULL;

    for (i=idlen; i < (SALT_LEN+idlen); i++) {
        salt[i] = _salt_chars[ salt[i] & 0x3F ];
    }
    salt[i] = '\0';
    return g_strdup (crypt (secret, salt));
}
