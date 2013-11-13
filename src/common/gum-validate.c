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

#include <utmp.h>
#include <regex.h>
#include <string.h>
#include <errno.h>

#include "common/gum-validate.h"
#include "common/gum-log.h"
#include "common/gum-error.h"

gboolean
gum_validate_name (
        const gchar *name,
        GError **error)
{
    size_t len = 0;
    int rval;
    regex_t re;
    if (!name) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "Invalid name. len must be > 0 and less than UT_NAMESIZE",
                error, FALSE);
    }

    len = strlen (name);
    if (len == 0 || len > UT_NAMESIZE) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "Invalid name len. len must be > 0 and less than UT_NAMESIZE",
                error, FALSE);
    }

    memset (&re, 0, sizeof (regex_t));
    if (regcomp (&re, (const char*)GUM_NAME_PATTERN, 0) != 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INTERNAL_SERVER, "Internal server error",
                error, FALSE);
    }
    rval = regexec (&re, name, 0, NULL, 0);
    regfree (&re);
    if (rval != 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "Name failed pattern match for "
                GUM_NAME_PATTERN, error, FALSE);
    }
    return TRUE;
}

gchar *
gum_validate_generate_username (
        const gchar *str,
        GError **error)
{
    gchar *gen_name = NULL;
    GChecksum *hid = NULL;
    gsize len = 0;
    if (!str || (len = strlen (str)) == 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "Unable to generate name for 0 len str", error, NULL);
    }

    hid = g_checksum_new (G_CHECKSUM_MD5);
    g_checksum_update (hid, (const guchar*)str, len);

    gen_name = g_strdup (g_checksum_get_string (hid));
    g_checksum_free (hid);
    if (!g_ascii_isalpha ((gchar)gen_name[0])) {
        gen_name[0] = 'U';
        DBG ("First character changed to alpha in %s",gen_name);
    }
    if (!gum_validate_name (gen_name, error)) {
        g_free (gen_name);
        gen_name = NULL;
    }

    return gen_name;
}

gboolean
gum_validate_db_string_entry_regx (
        const gchar *str,
        GError **error)
{
    int rval;
    regex_t re;
    if (!str) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_STR, "Invalid input str.", error,
                FALSE);
    }

    memset (&re, 0, sizeof (regex_t));
    if (regcomp (&re, (const char*)GUM_DB_ENTRY_PATTERN, REG_EXTENDED) != 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INTERNAL_SERVER, "Internal server error",
                error, FALSE);
    }
    rval = regexec (&re, str, 0, NULL, 0);
    regfree (&re);
    if (rval != 0) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_STR,
                "No control characters or ':' or ',' is allowed."
                "String failed pattern match for "GUM_DB_ENTRY_PATTERN, error,
                FALSE);
    }
    return TRUE;
}

gboolean
gum_validate_db_string_entry (
        const gchar *str,
        GError **error)
{
    /* list of control chars (0x00-0x1F,0x7F), comma (',' 0x2c) and
     * colon (':' 0x3A) */
    static const char invalid[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x7f, 0x2c, 0x3a, 0x00
    };

    if (!str) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_STR, "Invalid input str.",
                error, FALSE);
    }

    if (strlen (str) > 0 && strpbrk (str, invalid) != NULL) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_STR,
                "No control characters or ':' or ',' is allowed", error, FALSE);
    }
    return TRUE;
}

gboolean
gum_validate_db_secret_entry (
        const gchar *str,
        GError **error)
{
    /* list of control chars (0x00-0x1F,0x7F) and colon (':' 0x3A) */
    static const char invalid[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x7f, 0x3a, 0x00
    };

    if (!str) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_SECRET, "Invalid input str.",
                error, FALSE);
    }

    if (strpbrk (str, invalid) != NULL) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_SECRET,
                "No control characters or ':' is allowed", error, FALSE);
    }
    return TRUE;
}
