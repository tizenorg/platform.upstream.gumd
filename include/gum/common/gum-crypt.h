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

#ifndef __GUM_CRYPT_H_
#define __GUM_CRYPT_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {

    GUM_CRYPT_MD5 = 1,
    GUM_CRYPT_SHA256 = 2,
    GUM_CRYPT_SHA512 = 3,
    GUM_CRYPT_DES = 4

} GumCryptMethodID;

gchar *
gum_crypt_encrypt_secret (
        const gchar *secret,
        GumCryptMethodID methodid);

gint
gum_crypt_cmp_secret (
        const gchar *plain_secret,
        const gchar *enc_secret);

G_END_DECLS

#endif /* __GUM_CRYPT_H_ */
