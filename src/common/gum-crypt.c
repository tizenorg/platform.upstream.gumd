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
#include <string.h>
#include <ctype.h>

#include "common/gum-crypt.h"
#include "common/gum-log.h"

/**
 * SECTION:gum-crypt
 * @short_description: Utility functions for encryption
 * @title: Gum Crypt
 * @include: gum/common/gum-crypt.h
 *
 * Following code snippets shows how a string can be encrypted with any of the
 * supported encryption algorithm. Moreover, plain and encrypted secrets can be
 * compared if needed.
 *
 * |[
 *   gchar *pass = gum_crypt_encrypt_secret("pas.-s123", "SHA512");
 *   g_free (pass);
 *
 *   pass = gum_crypt_encrypt_secret("pass ?()123", "SHA512");
 *   gum_crypt_cmp_secret("pass ?()123", pass); //should return true.
 *   g_free (pass);
 *
 * ]|
 */

guchar _salt_chars[64 + 1] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define METHODID_LEN 3
#define SALT_LEN 16
#define SALT_ARRAY_LEN (METHODID_LEN + SALT_LEN + 1)

gchar *
_generate_salt (
		const gchar *encryp_algo)
{
    ssize_t bytes_read = 0;
    gchar salt[SALT_ARRAY_LEN];
    int fd = 0;
    gint id_len = METHODID_LEN, i = 0;

    fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return NULL;

    /* crypt(3) */
    if (g_strcmp0 (encryp_algo, "MD5") == 0) {
            salt[0] = salt[2] = '$';
            salt[1] = '1';
    } else if (g_strcmp0 (encryp_algo, "SHA256") == 0) {
            salt[0] = salt[2] = '$';
            salt[1] = '5';
    } else if (g_strcmp0 (encryp_algo, "SHA512") == 0) {
            salt[0] = salt[2] = '$';
            salt[1] = '6';
    } else { //if (g_strcmp0 (encryp_algo, "DES") == 0)
            id_len = 0;
    }

    bytes_read = read (fd, &salt[id_len], SALT_LEN);
    close (fd);
    if (bytes_read != SALT_LEN)
        return NULL;

    for (i=id_len; i < (SALT_LEN+id_len); i++) {
        salt[i] = _salt_chars[ salt[i] & 0x3F ];
    }
    salt[i] = '\0';
    return g_strdup (salt);
}

/**
 * gum_crypt_encrypt_secret:
 * @secret: (transfer none): string to encrypt
 * @encryp_algo: algorithm to be used for encryption. 'MD5', 'SHA256', 'SHA512',
 * and 'DES' are supported algorithms.
 *
 * Encrypts the secret with the specified algorithm @encryp_algo.
 *
 * Returns: (transfer full): encrypted secret if successful, NULL otherwise.
 */
gchar *
gum_crypt_encrypt_secret (
        const gchar *secret,
        const gchar *encryp_algo)
{
    gchar *enc_sec = NULL;
    gchar *salt = _generate_salt (encryp_algo);
    if (!salt) return NULL;

    enc_sec = g_strdup (crypt (secret, salt));
    g_free (salt);
    return enc_sec;
}

gchar *
_extract_salt (
        const gchar *enc_secret)
{
    gchar *salt = NULL;
    gint id_len = 0, enc_len = 0, i = 0;

    if (!enc_secret) return NULL;

    if (enc_secret[0] == '$' &&
        isdigit (enc_secret[1]) &&
        enc_secret[2] == '$') {
        id_len = METHODID_LEN;
    }

    enc_len = strlen (enc_secret);
    i = id_len;
    while (i < enc_len) {
        if (enc_secret[i] == '$') {
            salt = g_strndup (enc_secret, i);
            break;
        }
        i++;
    }

    return salt;
}

/**
 * gum_crypt_cmp_secret:
 * @plain_str1: (transfer none): plain string
 * @enc_str2: (transfer none): encrypted string
 *
 * Encrypts plain string with the same parameters as that of encrypted string
 * and then compares both encrypted strings.
 *
 * Returns: 0 if enc_str1 == enc_str2, less than 0 if the enc_str1 < enc_str2,
 * greater than 0 if enc_str1 > enc_str2.
 */
gint
gum_crypt_cmp_secret (
        const gchar *plain_str1,
        const gchar *enc_str2)
{
    gint cmp = -1;
    gchar *plain_enc = NULL;

    if (!enc_str2 || !plain_str1) return cmp;

    gchar *salt = _extract_salt (enc_str2);
    if (!salt) return cmp;

    plain_enc = g_strdup (crypt (plain_str1, salt));
    g_free (salt);

    cmp = g_strcmp0 (plain_enc, enc_str2);
    g_free (plain_enc);

    return cmp;
}
