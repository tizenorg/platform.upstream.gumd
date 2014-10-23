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

/**
 * SECTION:gum-config
 * @short_description: gum configuration information (singleton object)
 * @include: gum/common/gum-config.h
 *
 * #GumConfig holds configuration information as a set of keys and values
 * (integer or strings). The key names are defined in
 * <link linkend="gumd-General-configuration">General config keys</link>,
 * and <link linkend="gumd-DBus-configuration">DBus config keys</link>.
 *
 * The configuration is retrieved from the gum configuration file. See below
 * for where the file is searched for.
 *
 * <refsect1><title>Usage</title></refsect1>
 * Following code snippet demonstrates how to create and use config object:
 * |[
 *
 * GumConfig* config = gum_config_new (sysroot_path);
 * const gchar *str = gum_config_get_string (config,
 *  GUM_CONFIG_GENERAL_SKEL_DIR, 0);
 * g_object_unref(config);
 *
 * ]|
 *
 * <refsect1><title>Where the configuration file is searched for</title>
 * </refsect1>
 *
 * If gum has been compiled with --enable-debug, then these locations are used,
 * in decreasing order of priority:
 * - UM_CONF_FILE environment variable
 * - g_get_user_config_dir() + "gum.conf"
 * - each of g_get_system_config_dirs() + "gum.conf"
 *
 * Otherwise, the config file location is determined at compilation time as
 * $(sysconfdir) + "gum.conf"
 *
 * <refsect1><title>Example configuration file</title></refsect1>
 *
 * See example configuration file here:
 * <ulink url="https://github.com/01org/gumd/blob/master/src/common/gum.conf.in">
 * gum configuration file</ulink>
 *
 */

/**
 * GumConfig:
 *
 * Opaque structure for the object.
 */

/**
 * GumConfigClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

struct _GumConfigPrivate
{
    gchar *config_file_path;
    GumDictionary *config_table;
    gchar *sysroot;
};

#define GUM_CONFIG_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUM_TYPE_CONFIG, GumConfigPrivate)

G_DEFINE_TYPE (GumConfig, gum_config, G_TYPE_OBJECT);

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

static GumConfig *glob_config = NULL;

enum {
    PROP_0,
    PROP_SYSROOT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    g_return_if_fail (object && GUM_IS_CONFIG (object));

    GumConfig *self = GUM_CONFIG (object);

    switch (property_id) {
    case PROP_SYSROOT:
        self->priv->sysroot = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    g_return_if_fail (object && GUM_IS_CONFIG (object));

    GumConfig *self = GUM_CONFIG (object);

    switch (property_id) {
    case PROP_SYSROOT:
        g_value_set_string (value, self->priv->sysroot);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gchar *
_check_config_file (
        const gchar *path,
        const gchar *sysroot)
{
    gchar *filename = NULL;

    if (sysroot && sysroot[0] != '\0')
        filename = g_build_filename (sysroot, path, "gumd.conf", NULL);
    else
        filename = g_build_filename (path, "gumd.conf", NULL);
    DBG ("check config file at %s", filename);
    if (g_access (filename, R_OK) == 0)
        return filename;
    g_free (filename);
    return NULL;
}

static gboolean
_load_config (
        GumConfig *self)
{
    GError *err = NULL;
    gchar **groups = NULL;
    gsize n_groups = 0;
    int i,j;
    GKeyFile *settings = g_key_file_new ();

#   ifdef ENABLE_DEBUG
    const gchar * const *sysconfdirs;

    if (!self->priv->config_file_path) {
        const gchar *def_config = g_getenv ("UM_CONF_FILE");
        if (def_config)
            self->priv->config_file_path = _check_config_file (def_config,
                    NULL);
    }

    if (!self->priv->config_file_path) {
        gchar *filename = g_strdup_printf ("%s/gumd", GUM_SYSCONF_DIR);
        self->priv->config_file_path = _check_config_file (filename,
                self->priv->sysroot);
        g_free (filename);
    }

    if (!self->priv->config_file_path) {
        self->priv->config_file_path = _check_config_file (GUM_SYSCONF_DIR,
                self->priv->sysroot);
    }

    if (!self->priv->config_file_path) {
        sysconfdirs = g_get_system_config_dirs ();
        while (*sysconfdirs != NULL) {
            self->priv->config_file_path = _check_config_file (*sysconfdirs,
                    self->priv->sysroot);
            if (self->priv->config_file_path) {
                break;
            }
            sysconfdirs++;
        }
    }
#   else  /* ENABLE_DEBUG */
#   ifndef GUM_SYSCONF_DIR
#   error "System configuration directory not defined!"
#   endif
    gchar *filename = g_strdup_printf ("%s/gumd", GUM_SYSCONF_DIR);
    self->priv->config_file_path = _check_config_file (filename,
            self->priv->sysroot);
    g_free (filename);
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

            if (g_strcmp0 (GUM_CONFIG_DBUS_DAEMON_TIMEOUT, key) == 0 ||
                g_strcmp0 (GUM_CONFIG_DBUS_USER_TIMEOUT, key) == 0 ||
                g_strcmp0 (GUM_CONFIG_DBUS_GROUP_TIMEOUT, key) == 0) {
                gum_config_set_int (self, key, atoi (value));
            } else {
                gum_config_set_string (self, key, value);
            }

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
    gint timeout = 0;
    
    e_val = g_getenv ("UM_DAEMON_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_int (self, GUM_CONFIG_DBUS_DAEMON_TIMEOUT, timeout);

    e_val = g_getenv ("UM_USER_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_int (self, GUM_CONFIG_DBUS_USER_TIMEOUT, timeout);

    e_val = g_getenv ("UM_GROUP_TIMEOUT");
    if (e_val && (timeout = atoi(e_val)))
        gum_config_set_int (self, GUM_CONFIG_DBUS_GROUP_TIMEOUT, timeout);

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

/**
 * gum_config_get_int:
 * @self: (transfer none): an instance of #GumConfig
 * @key: the key name
 * @retval: value to be returned in case key is not found
 *
 * Get an integer configuration value.
 *
 * Returns: the value corresponding to the key as an integer. If the key does
 * not exist or cannot be converted to the integer, retval is returned.
 */
gint
gum_config_get_int (
        GumConfig *self,
        const gchar *key,
        gint retval)
{
    gint32 value = retval;
    gum_dictionary_get_int32 (self->priv->config_table, key, &value);
    return value;
}

/**
 * gum_config_set_int:
 * @self: (transfer none): an instance of #GumConfig
 * @key: the key name
 * @value: the value
 *
 * Sets the configuration value to the provided integer.
 */
void
gum_config_set_int (
        GumConfig *self,
        const gchar *key,
        gint value)
{
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    gum_dictionary_set_int32 (self->priv->config_table, key, value);
}

/**
 * gum_config_get_uint:
 * @self: (transfer none): an instance of #GumConfig
 * @key: the key name
 * @retval: value to be returned in case key is not found
 *
 * Get an unsigned integer configuration value.
 *
 * Returns: the value corresponding to the key as an unsigned integer. If the
 * key does not exist or cannot be converted to the integer, retval is returned.
 */
guint
gum_config_get_uint (
        GumConfig *self,
        const gchar *key,
        guint retval)
{
    guint32 value = retval;
    gum_dictionary_get_uint32 (self->priv->config_table, key, &value);
    return value;
}

/**
 * gum_config_set_uint:
 * @self: (transfer none): an instance of #GumConfig
 * @key: the key name
 * @value: the value
 *
 * Sets the configuration value to the provided unsigned integer.
 */
void
gum_config_set_uint (
        GumConfig *self,
        const gchar *key,
        guint value)
{
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    gum_dictionary_set_uint32 (self->priv->config_table, key, value);
}

/**
 * gum_config_get_string:
 * @self: (transfer none): an instance of #GumConfig
 * @key: (transfer none): the key name
 *
 * Get a string configuration value.
 *
 * Returns: the value corresponding to the key as an string. If the
 * key does not exist or cannot be converted to the integer, NULL is returned.
 */
const gchar *
gum_config_get_string (
        GumConfig *self,
        const gchar *key)
{
    g_return_val_if_fail (self && GUM_IS_CONFIG (self), NULL);

    return gum_dictionary_get_string (self->priv->config_table, key);
}

/**
 * gum_config_set_string:
 * @self: (transfer none): an instance of #GumConfig
 * @key: (transfer none): the key name
 * @value: (transfer none): the value
 *
 * Sets the configuration value to the provided string.
 */
void
gum_config_set_string (
        GumConfig *self,
        const gchar *key,
        const gchar *value)
{
    g_return_if_fail (self && GUM_IS_CONFIG (self));

    if ((self->priv->sysroot && self->priv->sysroot[0] != '\0') &&
        (g_strcmp0 (key, GUM_CONFIG_GENERAL_PASSWD_FILE) == 0 ||
         g_strcmp0 (key, GUM_CONFIG_GENERAL_SHADOW_FILE) == 0 ||
         g_strcmp0 (key, GUM_CONFIG_GENERAL_GROUP_FILE) == 0 ||
         g_strcmp0 (key, GUM_CONFIG_GENERAL_GSHADOW_FILE) == 0 ||
         g_strcmp0 (key, GUM_CONFIG_GENERAL_SKEL_DIR) == 0 ||
         g_strcmp0 (key, GUM_CONFIG_GENERAL_HOME_DIR_PREF) == 0)) {
         gchar *sysval = g_build_filename (self->priv->sysroot, value, NULL);
         gum_dictionary_set_string (self->priv->config_table, key, sysval);
         g_free (sysval);
         return;
    }
    gum_dictionary_set_string (self->priv->config_table, key, value);
}

static void
gum_config_dispose (
        GObject *object)
{
    GumConfig *self = 0;
    g_return_if_fail (object && GUM_IS_CONFIG (object));
    glob_config = NULL;

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

    if (self->priv->sysroot) {
        g_free (self->priv->sysroot);
        self->priv->sysroot = NULL;
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

    gum_config_set_string (self, GUM_CONFIG_GENERAL_DEF_ADMIN_GROUPS,
    		GUM_DEF_ADMIN_GROUPS);

    gum_config_set_string (self, GUM_CONFIG_GENERAL_ENCRYPT_METHOD,
    		GUM_ENCRYPT_METHOD);

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

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = gum_config_dispose;
    object_class->finalize = gum_config_finalize;

    /**
     * GumConfig:sysroot:
     *
     * This property holds the shell path of the user. Shell path should not
     * contain any control chars (0x00-0x1F,0x7F), comma (',' 0x2c) or
     * colon (':' 0x3A). For new user, it is recommended to let the underlying
     * framework to set the home directory of the user based on the
     * configuration.
     */
    properties[PROP_SYSROOT] = g_param_spec_string ("sysroot",
            "Sysroot",
            "sysroot path",
            NULL, /* default value */
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);
}

/**
 * gum_config_new:
 * @sysroot: (transfer none): sysroot property value
 *
 * Create a singleton #GumConfig object, which means 'sysroot' has to be
 * passed on in the first call to this function.
 *
 * Returns: a singleton instance of #GumConfig.
 */
GumConfig *
gum_config_new (
        const gchar *sysroot)
{
    if (!glob_config) {
        glob_config = GUM_CONFIG (g_object_new (GUM_TYPE_CONFIG, "sysroot",
                sysroot, NULL));
    } else {
        g_object_ref (glob_config);
    }

    return glob_config;
}

/**
 * gum_config_prepend_sysroot:
 * @self: (transfer none): an instance of #GumConfig
 * @string: (transfer none): string to append
 *
 * Prepends the sysroot path to the end of the 'string'.
 * If sysroot is NULL or empty, then 'string' is duplicated and returned.
 *
 * Returns: (transfer full): concatenated string if successful, NULL
 * otherwise.
 */
gchar *
gum_config_prepend_sysroot (
        GumConfig *self,
        const gchar *string)
{
    gchar *dest = NULL;
    g_return_val_if_fail (self && GUM_IS_CONFIG (self) && string, NULL);

    if (self->priv->sysroot && self->priv->sysroot[0] != '\0')
        dest = g_build_filename(self->priv->sysroot, string, NULL);
    else
        dest = g_strdup (string);

    return dest;
}
