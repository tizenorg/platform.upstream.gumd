/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gum
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
 *          Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#include <stdlib.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "config.h"
#include "common/gum-config.h"
#include "common/gum-log.h"
#include "common/gum-dictionary.h"

struct _GumConfigPrivate
{
    gchar *config_file_path;
    GumDictionary *config_table;
};

#define GUM_CONFIG_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_CONFIG, GumConfigPrivate)

G_DEFINE_TYPE (GumConfig, gum_config, G_TYPE_OBJECT);

#define GUM_PASSWD_FILE "/etc/passwd"
#define GUM_SHADOW_FILE "/etc/shadow"
#define GUM_GROUP_FILE "/etc/group"
#define GUM_GSHADOW_FILE "/etc/gshadow"
#define GUM_HOME_DIR_PREFIX "/home"
#define GUM_SKEL_DIR "/etc/skel"
#define GUM_SHELL "/bin/bash"
#define GUM_DEF_GROUPS "users"
#define UID_MIN      2000
#define UID_MAX      60000
#define SYS_UID_MIN  200
#define SYS_UID_MAX  999
#define GID_MIN      2000
#define GID_MAX      60000
#define SYS_GID_MIN  200
#define SYS_GID_MAX  999

/* shadow */
#define PASS_MAX_DAYS  99999
#define PASS_MIN_DAYS  0
#define PASS_WARN_AGE  7

static gboolean
_load_config (
        GumConfig *self)
{
    gchar *def_config;
    GError *err = NULL;
    gchar **groups = NULL;
    gsize n_groups = 0;
    int i,j;
    GKeyFile *settings = g_key_file_new ();

#   ifdef ENABLE_DEBUG
    const gchar * const *sysconfdirs;

    if (!self->priv->config_file_path) {
        def_config = g_strdup (g_getenv ("UM_CONF_FILE"));
        if (!def_config)
            def_config = g_build_filename (g_get_user_config_dir(),
                                           "gum/gum.conf",
                                           NULL);
        if (g_access (def_config, R_OK) == 0) {
            self->priv->config_file_path = def_config;
        } else {
            g_free (def_config);
            sysconfdirs = g_get_system_config_dirs ();
            while (*sysconfdirs != NULL) {
                def_config = g_build_filename (*sysconfdirs,
                                               "gum/gum.conf",
                                               NULL);
                if (g_access (def_config, R_OK) == 0) {
                    self->priv->config_file_path = def_config;
                    break;
                }
                g_free (def_config);
                sysconfdirs++;
            }
        }
    }
#   else  /* ENABLE_DEBUG */
#   ifndef GUM_SYSCONF_DIR
#   error "System configuration directory not defined!"
#   endif
    def_config = g_build_filename (GUM_SYSCONF_DIR,
                                   "gum/gum.conf",
                                   NULL);
    if (g_access (def_config, R_OK) == 0) {
        self->priv->config_file_path = def_config;
    } else {
        g_free (def_config);
    }
#   endif  /* ENABLE_DEBUG */

    if (self->priv->config_file_path) {
        DBG ("Loading UM config from %s", self->priv->config_file_path);
        if (!g_key_file_load_from_file (settings,
                                        self->priv->config_file_path,
                                        G_KEY_FILE_NONE, &err)) {
            WARN ("error reading config file at '%s': %s",
                 self->priv->config_file_path, err->message);
            g_error_free (err);
            g_key_file_free (settings);
            return FALSE;
        }
    }

    groups = g_key_file_get_groups (settings, &n_groups);

    for (i = 0; i < n_groups; i++) {
        GError *err = NULL;
        gsize n_keys =0;
        gchar **keys = g_key_file_get_keys (settings,
                                            groups[i],
                                            &n_keys,
                                            &err);
        if (err) {
            WARN ("fail to read group '%s': %s", groups[i], err->message);
            g_error_free (err);
            continue;
        }

        for (j = 0; j < n_keys; j++) {
            gchar *key = g_strdup_printf ("%s/%s", groups[i], keys[j]);
            gchar *value = g_key_file_get_value (settings,
                                                 groups[i],
                                                 keys[j],
                                                 &err);
            if (err) {
                WARN ("fail to read key '%s/%s': %s", groups[i], keys[j],
                        err->message);
                g_error_free (err);
                continue;
            }

            DBG ("found config : '%s/%s' - '%s'", groups[i], keys[j], value);

            gum_config_set_string (self, key, value);

            g_free (key);
            g_free (value);
        }

        g_strfreev (keys);
    }

    g_strfreev (groups);

    g_key_file_free (settings);

    return TRUE;
}

#ifdef ENABLE_DEBUG
static void
_load_environment (
        GumConfig *self)
{
    const gchar *e_val = 0;
    guint timeout = 0;
    
    e_val = g_getenv ("UM_DAEMON_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_string (self, GUM_CONFIG_DBUS_DAEMON_TIMEOUT, e_val);

    e_val = g_getenv ("UM_USER_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_string (self, GUM_CONFIG_DBUS_USER_TIMEOUT, e_val);

    e_val = g_getenv ("UM_GROUP_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_string (self, GUM_CONFIG_DBUS_GROUP_TIMEOUT, e_val);

    e_val = g_getenv ("UM_PASSWD_FILE");
    if (e_val)
    	gum_config_set_string (self, GUM_CONFIG_GENERAL_PASSWD_FILE, e_val);

    e_val = g_getenv ("UM_SHADOW_FILE");
    if (e_val)
    	gum_config_set_string (self, GUM_CONFIG_GENERAL_SHADOW_FILE, e_val);

    e_val = g_getenv ("UM_GROUP_FILE");
    if (e_val)
        gum_config_set_string (self, GUM_CONFIG_GENERAL_GROUP_FILE, e_val);

    e_val = g_getenv ("UM_GSHADOW_FILE");
    if (e_val)
        gum_config_set_string (self, GUM_CONFIG_GENERAL_GSHADOW_FILE, e_val);

    e_val = g_getenv ("UM_HOMEDIR_PREFIX");
    if (e_val)
        gum_config_set_string (self, GUM_CONFIG_GENERAL_HOME_DIR_PREF, e_val);

    e_val = g_getenv ("UM_SKEL_DIR");
    if (e_val)
        gum_config_set_string (self, GUM_CONFIG_GENERAL_SKEL_DIR, e_val);
}
#endif  /* ENABLE_DEBUG */

gint
gum_config_get_int (
        GumConfig *self,
        const gchar *key,
        gint retval)
{
    const gchar *str_value = gum_config_get_string (self, key);
    return (gint) (str_value ? atoi (str_value) : retval);
}

void
gum_config_set_int (
        GumConfig *self,
        const gchar *key,
        gint value)
{
    gchar *s_value = 0;
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    s_value = g_strdup_printf ("%d", value);
    if (!s_value) return;

    gum_config_set_string (self, (gpointer) key, s_value);

    g_free (s_value);
}

guint
gum_config_get_uint (
        GumConfig *self,
        const gchar *key,
        guint retval)
{
    guint value;
    const gchar *str_value = gum_config_get_string (self, key);
    if (!str_value || sscanf (str_value,"%u",&value) <= 0) value = retval;
    return value;
}

void
gum_config_set_uint (
        GumConfig *self,
        const gchar *key,
        guint value)
{
    gchar *s_value = 0;
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    s_value = g_strdup_printf ("%u", value);
    if (!s_value) return;

    gum_config_set_string (self, (gpointer) key, s_value);
    g_free (s_value);
}

const gchar *
gum_config_get_string (
        GumConfig *self,
        const gchar *key)
{
    g_return_val_if_fail (self && GUM_IS_CONFIG (self), NULL);

    GVariant* value = gum_dictionary_get (self->priv->config_table,
                                               (gpointer) key);
    if (!value) return NULL;

    return g_variant_get_string (value, NULL);
}

void
gum_config_set_string (
        GumConfig *self,
        const gchar *key,
        const gchar *value)
{
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    gum_dictionary_set (self->priv->config_table,
                             (gpointer) key,
                             g_variant_new_string (value));

}

static void
gum_config_dispose (
        GObject *object)
{
    GumConfig *self = 0;
    g_return_if_fail (object && GUM_IS_CONFIG (object));

    self = GUM_CONFIG (object);

    if (self->priv->config_table) {
        gum_dictionary_unref (self->priv->config_table);
        self->priv->config_table = NULL;
    }

    G_OBJECT_CLASS (gum_config_parent_class)->dispose (object);
}

static void
gum_config_finalize (
        GObject *object)
{
    GumConfig *self = 0;
    g_return_if_fail (object && GUM_IS_CONFIG (object));

    self = GUM_CONFIG (object);

    if (self->priv->config_file_path) {
        g_free (self->priv->config_file_path);
        self->priv->config_file_path = NULL;
    }

    G_OBJECT_CLASS (gum_config_parent_class)->finalize (object);
}

static void
gum_config_init (
        GumConfig *self)
{
    self->priv = GUM_CONFIG_PRIV (self);

    self->priv->config_file_path = NULL;
    self->priv->config_table = gum_dictionary_new ();

    gum_config_set_string (self, GUM_CONFIG_GENERAL_PASSWD_FILE,
    		GUM_PASSWD_FILE);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_SHADOW_FILE,
    		GUM_SHADOW_FILE);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_GROUP_FILE,
            GUM_GROUP_FILE);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_GSHADOW_FILE,
            GUM_GSHADOW_FILE);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_HOME_DIR_PREF,
    		GUM_HOME_DIR_PREFIX);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_SHELL, GUM_SHELL);
    gum_config_set_string (self, GUM_CONFIG_GENERAL_SKEL_DIR, GUM_SKEL_DIR);

    gum_config_set_uint (self, GUM_CONFIG_GENERAL_UID_MIN, UID_MIN);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_UID_MAX, UID_MAX);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_SYS_UID_MIN, SYS_UID_MIN);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_SYS_UID_MAX, SYS_UID_MAX);

    gum_config_set_uint (self, GUM_CONFIG_GENERAL_GID_MIN, GID_MIN);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_GID_MAX, GID_MAX);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_SYS_GID_MIN, SYS_GID_MIN);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_SYS_GID_MAX, SYS_GID_MAX);

    gum_config_set_int (self, GUM_CONFIG_GENERAL_PASS_MAX_DAYS,
    		PASS_MAX_DAYS);
    gum_config_set_int (self, GUM_CONFIG_GENERAL_PASS_MIN_DAYS,
    		PASS_MIN_DAYS);
    gum_config_set_int (self, GUM_CONFIG_GENERAL_PASS_WARN_AGE,
    		PASS_WARN_AGE);
    gum_config_set_uint (self, GUM_CONFIG_GENERAL_UMASK, GUM_UMASK);

    gum_config_set_string (self, GUM_CONFIG_GENERAL_DEF_USR_GROUPS,
            GUM_DEF_GROUPS);

    if (!_load_config (self))
        WARN ("load configuration failed, using default settings");

#ifdef ENABLE_DEBUG
    _load_environment (self);
#endif

}

static void
gum_config_class_init (
        GumConfigClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumConfigPrivate));

    object_class->dispose = gum_config_dispose;
    object_class->finalize = gum_config_finalize;

}

GumConfig *
gum_config_new ()
{
    return GUM_CONFIG (g_object_new (GUM_TYPE_CONFIG, NULL));
}

