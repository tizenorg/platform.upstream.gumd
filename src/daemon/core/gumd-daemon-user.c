/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gumd
 *
 * Copyright (C) 2013 - 2015 Intel Corporation.
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

#include <string.h>
#include <shadow.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include "gumd-daemon-user.h"
#include "gumd-daemon-group.h"
#include "common/gum-lock.h"
#include "common/gum-crypt.h"
#include "common/gum-validate.h"
#include "common/gum-file.h"
#include "common/gum-string-utils.h"
#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-utils.h"

struct _userinfo {
    char *icon;
};

struct _GumdDaemonUserPrivate
{
    GumConfig *config;
    gchar *nick_name;

    /* passwd file entries format:
	 * name:passwd:uid:gid:gecos:dir:shell
	 * whereas gecos contain the following comma(',') separated values
	 * realname,officelocation,officephone,homephone,usertype
	 */
    struct passwd *pw;

    /* shadow file entries format:
     * username:encrypted_password:last_change_date:min_changes_days:
     * max_changes_days:expire_warn_days:account_disable_days:
     * account_expiry_date:reserved_field
     */
    struct spwd *shadow;

    /* user info file entries format:
     * icon:
     */
    struct _userinfo *info;
};

G_DEFINE_TYPE (GumdDaemonUser, gumd_daemon_user, G_TYPE_OBJECT)

#define GUMD_DAEMON_USER_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUMD_TYPE_DAEMON_USER, GumdDaemonUserPrivate)

#define GUMD_DAY (24L*3600L)
#define GUMD_STR_LEN 4096

enum
{
    PROP_0,

    PROP_CONFIG,
    PROP_USERTYPE,
    PROP_NICKNAME,
    PROP_UID,
    PROP_GID,
    PROP_USERNAME,
    PROP_SECRET,
    PROP_REALNAME,
    PROP_OFFICE,
    PROP_OFFICEPHONE,
    PROP_HOMEPHONE,
    PROP_HOMEDIR,
    PROP_SHELL,
    PROP_ICON,

    N_PROPERTIES
};

typedef enum
{
    GECOS_FIELD_REALNAME = 0,
    GECOS_FIELD_OFFICE,
    GECOS_FIELD_OFFICEPHONE,
    GECOS_FIELD_HOMEPHONE,
    GECOS_FIELD_USERTYPE,

    GECOS_FIELD_COUNT
}GecosField;

static GParamSpec *properties[N_PROPERTIES];

static void
_free_passwd_entry (
        struct passwd *pwe)
{
    if (pwe) {
        g_free (pwe->pw_name);
        g_free (pwe->pw_passwd);
        g_free (pwe->pw_gecos);
        g_free (pwe->pw_dir);
        g_free (pwe->pw_shell);
        g_free (pwe);
    }
}

static void
_free_shadow_entry (
        struct spwd *shadow)
{
    if (shadow) {
        g_free (shadow->sp_namp);
        g_free (shadow->sp_pwdp);
        g_free (shadow);
    }
}

static void
_free_info_entry (
        struct _userinfo *info)
{
    if (info) {
        g_free (info->icon);
    }
}

static GumUserType
_get_usertype_from_gecos (
        struct passwd *pw)
{
    if (pw && pw->pw_gecos) {
        gchar *str = gum_string_utils_get_string (pw->pw_gecos, ",",
                GECOS_FIELD_USERTYPE);
        GumUserType ut = gum_user_type_from_string (str);
        g_free (str);
        return ut;
    }
    return GUM_USERTYPE_NONE;
}

static void
_set_config_property (
        GumdDaemonUser *self,
        GObject *value)
{
    if (value != G_OBJECT (self->priv->config)) {
        GUM_OBJECT_UNREF (self->priv->config);
        self->priv->config = GUM_CONFIG (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONFIG]);
    } else {
        g_object_unref (value);
    }
}

static void
_set_uid_property (
        GumdDaemonUser *self,
        uid_t value)
{
    if (self->priv->pw->pw_uid != value) {
        self->priv->pw->pw_uid = value;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UID]);
    }
}

static void
_set_gid_property (
        GumdDaemonUser *self,
        gid_t value)
{
    if (self->priv->pw->pw_gid != value) {
        self->priv->pw->pw_gid = value;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GID]);
    }
}

static void
_set_usertype_property (
        GumdDaemonUser *self,
        GumUserType value)
{
    if (value > GUM_USERTYPE_NONE && value < GUM_USERTYPE_MAX_VALUE) {
        GumUserType ut = _get_usertype_from_gecos (self->priv->pw);
        if (ut != value) {
            gchar *gecos = gum_string_utils_insert_string (
                    self->priv->pw->pw_gecos, ",",
                    gum_user_type_to_string (value), GECOS_FIELD_USERTYPE,
                    GECOS_FIELD_COUNT);
            GUM_STR_FREE (self->priv->pw->pw_gecos);
            self->priv->pw->pw_gecos = gecos;
            g_object_notify_by_pspec (G_OBJECT (self),
                    properties[PROP_USERTYPE]);
        }
    }
}

static void
_set_username_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->pw->pw_name) != 0 &&
        gum_validate_name (value, NULL)) {
        GUM_STR_FREE (self->priv->pw->pw_name);
        self->priv->pw->pw_name = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self),
                properties[PROP_USERNAME]);
    }
}

static void
_set_secret_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->pw->pw_passwd) != 0 &&
        gum_validate_db_secret_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->pw->pw_passwd);
        self->priv->pw->pw_passwd = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECRET]);
    }
}

static void
_set_nickname_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->nick_name) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->nick_name);
        self->priv->nick_name = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NICKNAME]);
    }
}

static void
_set_realname_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    gchar *str = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
            GECOS_FIELD_REALNAME);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, GECOS_FIELD_REALNAME,
                GECOS_FIELD_COUNT);
        GUM_STR_FREE (self->priv->pw->pw_gecos);
        self->priv->pw->pw_gecos = gecos;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REALNAME]);
    }
    g_free (str);
}

static void
_set_office_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    gchar *str = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
            GECOS_FIELD_OFFICE);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, GECOS_FIELD_OFFICE,
                GECOS_FIELD_COUNT);
        GUM_STR_FREE (self->priv->pw->pw_gecos);
        self->priv->pw->pw_gecos = gecos;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OFFICE]);
    }
    g_free (str);
}

static void
_set_officephone_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    gchar *str = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
            GECOS_FIELD_OFFICEPHONE);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, GECOS_FIELD_OFFICEPHONE,
                GECOS_FIELD_COUNT);
        GUM_STR_FREE (self->priv->pw->pw_gecos);
        self->priv->pw->pw_gecos = gecos;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OFFICEPHONE]);
    }
    g_free (str);
}

static void
_set_homephone_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    gchar *str = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
            GECOS_FIELD_HOMEPHONE);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, GECOS_FIELD_HOMEPHONE,
                GECOS_FIELD_COUNT);
        GUM_STR_FREE (self->priv->pw->pw_gecos);
        self->priv->pw->pw_gecos = gecos;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOMEPHONE]);
    }
    g_free (str);
}

static void
_set_homedir_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->pw->pw_dir) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->pw->pw_dir);
        self->priv->pw->pw_dir = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOMEDIR]);
    }
}

static void
_set_shell_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->pw->pw_shell) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->pw->pw_shell);
        self->priv->pw->pw_shell = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHELL]);
    }
}

static void
_set_icon_property (
        GumdDaemonUser *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->info->icon) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->info->icon);
        self->priv->info->icon = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
    }
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDaemonUser *self = GUMD_DAEMON_USER (object);
    switch (property_id) {
        case PROP_CONFIG: {
            _set_config_property (self, g_value_dup_object (value));
            break;
        }
        case PROP_UID: {
            _set_uid_property (self, g_value_get_uint (value));
            break;
        }
        case PROP_GID: {
            _set_gid_property (self, g_value_get_uint (value));
            break;
        }
        case PROP_USERTYPE: {
            _set_usertype_property (self,
                    (GumUserType)g_value_get_uint (value));
            break;
        }
        case PROP_USERNAME: {
            _set_username_property (self, g_value_get_string (value));
            break;
        }
        case PROP_SECRET: {
            _set_secret_property (self, g_value_get_string (value));
            break;
        }
        case PROP_NICKNAME: {
            _set_nickname_property (self, g_value_get_string (value));
            break;
        }
        case PROP_REALNAME: {
            _set_realname_property (self, g_value_get_string (value));
            break;
        }
        case PROP_OFFICE: {
            _set_office_property (self, g_value_get_string (value));
            break;
        }
        case PROP_OFFICEPHONE: {
            _set_officephone_property (self, g_value_get_string (value));
            break;
        }
        case PROP_HOMEPHONE: {
            _set_homephone_property (self, g_value_get_string (value));
            break;
        }
        case PROP_HOMEDIR: {
            _set_homedir_property (self, g_value_get_string (value));
            break;
        }
        case PROP_SHELL: {
            _set_shell_property (self, g_value_get_string (value));
            break;
        }
        case PROP_ICON: {
            _set_icon_property (self, g_value_get_string (value));
            break;
        }
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
    GumdDaemonUser *self = GUMD_DAEMON_USER (object);

    switch (property_id) {
        case PROP_CONFIG: {
            g_value_set_object (value, self->priv->config);
            break;
        }
        case PROP_UID: {
            g_value_set_uint (value, self->priv->pw->pw_uid);
            break;
        }
        case PROP_GID: {
            g_value_set_uint (value, self->priv->pw->pw_gid);
            break;
        }
        case PROP_USERTYPE: {
            g_value_set_uint (value,
                    (guint)_get_usertype_from_gecos(self->priv->pw));
            break;
        }
        case PROP_NICKNAME: {
            g_value_set_string (value, self->priv->nick_name);
            break;
        }
        case PROP_USERNAME: {
            g_value_set_string (value, self->priv->pw->pw_name);
            break;
        }
        case PROP_SECRET: {
            g_value_set_string (value, self->priv->pw->pw_passwd);
            break;
        }
        case PROP_REALNAME: {
            if (self->priv->pw->pw_gecos) {
                gchar *str = gum_string_utils_get_string (
                        self->priv->pw->pw_gecos, ",", GECOS_FIELD_REALNAME);
                if (str) {
                    g_value_set_string (value, str);
                    g_free (str);
                }
            }
            break;
        }
        case PROP_OFFICE: {
            if (self->priv->pw->pw_gecos) {
                gchar *str = gum_string_utils_get_string (
                        self->priv->pw->pw_gecos, ",", GECOS_FIELD_OFFICE);
                if (str) {
                    g_value_set_string (value, str);
                    g_free (str);
                }
            }
            break;
        }
        case PROP_OFFICEPHONE: {
            if (self->priv->pw->pw_gecos) {
                gchar *str = gum_string_utils_get_string (
                        self->priv->pw->pw_gecos, ",", GECOS_FIELD_OFFICEPHONE);
                if (str) {
                    g_value_set_string (value, str);
                    g_free (str);
                }
            }
            break;
        }
        case PROP_HOMEPHONE: {
            if (self->priv->pw->pw_gecos) {
                gchar *str = gum_string_utils_get_string (
                        self->priv->pw->pw_gecos, ",", GECOS_FIELD_HOMEPHONE);
                if (str) {
                    g_value_set_string (value, str);
                    g_free (str);
                }
            }
            break;
        }
        case PROP_HOMEDIR: {
            if (self->priv->pw->pw_dir)
                g_value_set_string (value, self->priv->pw->pw_dir);
            break;
        }
        case PROP_SHELL: {
            if (self->priv->pw->pw_shell)
                g_value_set_string (value, self->priv->pw->pw_shell);
            break;
        }
        case PROP_ICON: {
            g_value_set_string (value, self->priv->info->icon);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (GObject *object)
{
    GumdDaemonUser *self = GUMD_DAEMON_USER (object);

    DBG ("");

    GUM_OBJECT_UNREF (self->priv->config);

    G_OBJECT_CLASS (gumd_daemon_user_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    GumdDaemonUser *self = GUMD_DAEMON_USER (object);

    GUM_STR_FREE (self->priv->nick_name);

    if (self->priv->shadow) {
        _free_shadow_entry (self->priv->shadow);
        self->priv->shadow = NULL;
    }

    if (self->priv->pw) {
        _free_passwd_entry (self->priv->pw);
        self->priv->pw = NULL;
    }

    if (self->priv->info) {
        _free_info_entry (self->priv->info);
        self->priv->info = NULL;
    }

    G_OBJECT_CLASS (gumd_daemon_user_parent_class)->finalize (object);
}

static void
gumd_daemon_user_init (
        GumdDaemonUser *self)
{
    self->priv = GUMD_DAEMON_USER_PRIV (self);
    self->priv->config = NULL;
    self->priv->nick_name = NULL;
    self->priv->pw = g_malloc0 (sizeof (struct passwd));
    self->priv->pw->pw_uid = GUM_USER_INVALID_UID;
    self->priv->pw->pw_gid = GUMD_DAEMON_GROUP_INVALID_GID;
    self->priv->shadow = g_malloc0 (sizeof (struct spwd));
    self->priv->info = g_malloc0 (sizeof (struct _userinfo));
}

static void
gumd_daemon_user_class_init (
        GumdDaemonUserClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumdDaemonUserPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_CONFIG] = g_param_spec_object ("config",
            "config",
            "Configuration object",
            GUM_TYPE_CONFIG,
            G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_UID] =  g_param_spec_uint ("uid",
            "Uid",
            "Unique identifier of the user",
            0,
            G_MAXUINT,
            GUM_USER_INVALID_UID /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the user",
            0,
            G_MAXUINT,
            GUM_GROUP_INVALID_GID /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_USERTYPE] =  g_param_spec_uint ("usertype",
            "UserType",
            "Type of the user",
            0,
            G_MAXUINT16,
            GUM_USERTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_USERNAME] = g_param_spec_string ("username",
            "Username",
            "System name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "User secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_NICKNAME] = g_param_spec_string ("nickname",
            "Nickname",
            "Nick name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_REALNAME] = g_param_spec_string ("realname",
            "Realname",
            "Real name of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_OFFICE] = g_param_spec_string ("office",
            "Office",
            "Office location of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_OFFICEPHONE] = g_param_spec_string ("officephone",
            "OfficePhone",
            "Office phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_HOMEPHONE] = g_param_spec_string ("homephone",
            "HomePhone",
            "Home phone of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_HOMEDIR] = g_param_spec_string ("homedir",
            "HomeDir",
            "Home directory of the user",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SHELL] = g_param_spec_string ("shell",
            "Shell",
            "Shell",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_ICON] = g_param_spec_string ("icon",
            "Icon",
            "Icon path of User",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES,
            properties);

}

static gboolean
_set_daemon_user_name (
        GumdDaemonUser *self,
        GError **error)
{
    /* Atleast username or nickname should be defined.
     * */
    gchar *tname = NULL;
    if (!self->priv->pw->pw_name && !self->priv->nick_name) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "User name not specified", error, FALSE);
    }

    if (self->priv->pw->pw_name) {
        if (!gum_validate_name (self->priv->pw->pw_name, error))
            return FALSE;

        tname = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",", 0);
        if (!tname) {
            _set_realname_property (self, self->priv->pw->pw_name);
        }
        g_free (tname);
        return TRUE;
    } else if (_get_usertype_from_gecos (self->priv->pw) ==
            GUM_USERTYPE_SYSTEM) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_INVALID_NAME,
                "System user name must exist with pattern "
                GUM_NAME_PATTERN, error, FALSE);
    }

    /* Create the hash of the nick name and store it as a user_name
     * to handle non-ascii characters in nickname.
     * */
    tname = gum_validate_generate_username (self->priv->nick_name, error);
    if (!tname) return FALSE;

    _set_username_property (self, tname);
    g_free (tname);
    if (!self->priv->pw->pw_name) {
        return FALSE;
    }

    tname = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",", 0);
    if (!tname) {
        _set_realname_property (self, self->priv->pw->pw_name);
    }
    g_free (tname);
    return TRUE;
}

static gboolean
_get_default_uid_range (
        GumUserType ut,
        GumConfig *config,
        uid_t *min,
        uid_t *max)
{
    if (ut == GUM_USERTYPE_SYSTEM)
        *min = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_SYS_UID_MIN, GUM_USER_INVALID_UID);
    else if (ut == GUM_USERTYPE_SECURITY)
        *min = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_SEC_UID_MIN, GUM_USER_INVALID_UID);
    else
        *min = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_UID_MIN, GUM_USER_INVALID_UID);

    if (ut == GUM_USERTYPE_SYSTEM)
        *max = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_SYS_UID_MAX, GUM_USER_INVALID_UID);
    else if (ut == GUM_USERTYPE_SECURITY)
        *max = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_SEC_UID_MAX, GUM_USER_INVALID_UID);
    else
        *max = (uid_t) gum_config_get_uint (config,
                GUM_CONFIG_GENERAL_UID_MAX, GUM_USER_INVALID_UID);

    return (*min < *max);
}

static gboolean
_find_free_uid (
        GumdDaemonUser *self,
        uid_t *uid)
{
    uid_t tmp_uid, uid_min, uid_max;

    if (!_get_default_uid_range (_get_usertype_from_gecos (self->priv->pw),
            self->priv->config, &uid_min, &uid_max))
        return FALSE;

    /* Select the first available uid */
    tmp_uid = uid_min;
    while (tmp_uid <= uid_max) {
        if (gum_file_getpwuid (tmp_uid, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE)) == NULL) {
            *uid = tmp_uid;
            return TRUE;
        }
        tmp_uid = tmp_uid + 1;
    }
    return FALSE;
}

static gboolean
_set_uid (
        GumdDaemonUser *self,
        GError **error)
{
    uid_t uid = GUM_USER_INVALID_UID;
    if (!_set_daemon_user_name (self, error)) {
        return FALSE;
    }

    if (gum_file_getpwnam (self->priv->pw->pw_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE))
            != NULL) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_ALREADY_EXISTS,
                "User already exists", error, FALSE);
    }

    if (!_find_free_uid (self, &uid)){
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_UID_NOT_AVAILABLE,
                "UID not available", error, FALSE);
    }
    _set_uid_property (self, uid);

    if (_get_usertype_from_gecos (self->priv->pw) != GUM_USERTYPE_SYSTEM)  {
        gchar *dir = g_strdup_printf ("%s/%s",
                gum_config_get_string (self->priv->config,
                        GUM_CONFIG_GENERAL_HOME_DIR_PREF),
                        self->priv->pw->pw_name);
        _set_homedir_property (self, dir);
        g_free (dir);
    }
    return TRUE;
}

static gboolean
_set_secret (
        GumdDaemonUser *self,
        GError **error)
{
    GUM_STR_FREE (self->priv->shadow->sp_pwdp);

    if (!self->priv->pw->pw_passwd) {
        GumUserType ut = _get_usertype_from_gecos (self->priv->pw);
        if (ut == GUM_USERTYPE_SYSTEM)
            self->priv->shadow->sp_pwdp = g_strdup ("*");
        else if (ut == GUM_USERTYPE_GUEST)
            self->priv->shadow->sp_pwdp = g_strdup ("");
        else
            self->priv->shadow->sp_pwdp = g_strdup ("!");
        _set_secret_property (self, "x");
        return TRUE;
    }

    self->priv->shadow->sp_pwdp = gum_crypt_encrypt_secret (
            self->priv->pw->pw_passwd, gum_config_get_string (
                    self->priv->config, GUM_CONFIG_GENERAL_ENCRYPT_METHOD));
    if (!self->priv->shadow->sp_pwdp) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_SECRET_ENCRYPT_FAILURE,
                "Secret encryption failed.", error, FALSE);
    }

    memset (self->priv->pw->pw_passwd, 0, strlen(self->priv->pw->pw_passwd));
    _set_secret_property (self, "x");

    return TRUE;
}

static gboolean
_set_shadow_data (
        GumdDaemonUser *self,
        GError **error)
{
	GUM_STR_DUP (self->priv->pw->pw_name, self->priv->shadow->sp_namp);

	if (!_set_secret (self, error)) {
		return FALSE;
	}

	self->priv->shadow->sp_lstchg = (long) time ((time_t *) 0) / GUMD_DAY;
	if (self->priv->shadow->sp_lstchg == 0) {
		/* Better disable aging than requiring a password change */
		self->priv->shadow->sp_lstchg = -1;
	}
	self->priv->shadow->sp_min = gum_config_get_int (self->priv->config,
            GUM_CONFIG_GENERAL_PASS_MIN_DAYS, -1);
	self->priv->shadow->sp_max = gum_config_get_int (self->priv->config,
            GUM_CONFIG_GENERAL_PASS_MAX_DAYS, -1);
	self->priv->shadow->sp_warn = gum_config_get_int (self->priv->config,
            GUM_CONFIG_GENERAL_PASS_WARN_AGE, -1);
	self->priv->shadow->sp_inact = -1;
	self->priv->shadow->sp_expire = -1;
	self->priv->shadow->sp_flag = ((unsigned long int)-1);
	return TRUE;
}

static gboolean
_update_passwd_entry (
		GumdDaemonUser *self,
		GumOpType op,
		FILE *source_file,
		FILE *dup_file,
        gpointer user_data,
		GError **error)
{
	/* Loop all entries */
	gboolean done = FALSE;
	struct passwd *entry;

	while ((entry = fgetpwent (source_file)) != NULL) {
	    if (!done) {
	        switch (op) {
	        case GUM_OPTYPE_ADD:
	            if (self->priv->pw->pw_uid < entry->pw_uid) {
	                if (putpwent (self->priv->pw, dup_file) < 0) {
	                    GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
	                            "File write failure", error, FALSE);
	                }
	                done = TRUE;
	            }
	            break;
	        case GUM_OPTYPE_DELETE:
	            if (self->priv->pw->pw_uid == entry->pw_uid &&
	                    self->priv->pw->pw_gid == entry->pw_gid &&
	                    g_strcmp0 (self->priv->pw->pw_name, entry->pw_name) == 0) {
	                done = TRUE;
	                continue;
	            }
	            break;
	        case GUM_OPTYPE_MODIFY: {
	            gchar *old_name = user_data ? (gchar *)user_data :
	                    self->priv->pw->pw_name;
	            if (self->priv->pw->pw_uid == entry->pw_uid &&
	                    self->priv->pw->pw_gid == entry->pw_gid &&
	                    g_strcmp0 (old_name, entry->pw_name) == 0) {
	                if (putpwent (self->priv->pw, dup_file) < 0) {
	                    GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
	                            "File write failure", error, FALSE);
	                }
	                done = TRUE;
	                continue;
	            }
	            break;
	        }
	        default:
                break;
            }
	    }
	    if (putpwent (entry, dup_file) < 0) {
	        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
	                error, FALSE);
	    }
	}

	/* Write entry to file in case it is first entry in the file */
	if (!done && op == GUM_OPTYPE_ADD) {
	    if (putpwent (self->priv->pw, dup_file) < 0) {
	        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Add entry failure",
	                error, FALSE);
	    }
	    done = TRUE;
	}

	if (!done) {
	    GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Operation did not complete",
	            error, FALSE);
	}

	return TRUE;
}

static gboolean
_copy_shadow_struct (
        struct spwd *src,
        struct spwd *dest,
        gboolean copy_only_if_invalid)
{
    if (!src || !dest) return FALSE;

    if (!copy_only_if_invalid || !dest->sp_namp)
        GUM_STR_DUP (src->sp_namp, dest->sp_namp);
    if (!copy_only_if_invalid || !dest->sp_pwdp)
        GUM_STR_DUP (src->sp_pwdp, dest->sp_pwdp);
    dest->sp_lstchg = src->sp_lstchg;
    dest->sp_min = src->sp_min;
    dest->sp_max = src->sp_max;
    dest->sp_warn = src->sp_warn;
    dest->sp_inact = src->sp_inact;
    dest->sp_expire = src->sp_expire;
    dest->sp_flag = src->sp_flag;

    return TRUE;
}

static gboolean
_lock_shadow_entry (
        GumdDaemonUser *self,
        GumOpType op,
        FILE *source_file,
        FILE *dup_file,
        gpointer user_data,
        GError **error)
{
    /* Loop all entries */
    gboolean done = FALSE;
    struct spwd *entry = NULL;

    if (!user_data) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
                "File write failure", error, FALSE);
    }

    while ((entry = fgetspent (source_file)) != NULL) {
        if (!done) {
            switch (op) {
            case GUM_OPTYPE_MODIFY: {
                if (g_strcmp0 (self->priv->pw->pw_name, entry->sp_namp) == 0) {
                    gint ret = -1;
                    gboolean lock = *((gboolean *)user_data);
                    struct spwd *spent = g_malloc0 (sizeof (struct spwd));
                    _copy_shadow_struct (entry, spent, FALSE);
                    GUM_STR_FREE (spent->sp_pwdp);
                    if (lock && entry->sp_pwdp[0] != '!') {
                        /* entry is unlocked, lock it */
                        spent->sp_pwdp = g_strdup_printf ("!%s",entry->sp_pwdp);
                    } else if (!lock && entry->sp_pwdp[0] == '!') {
                        /* entry is locked, unlock it */
                        spent->sp_pwdp = g_strdup (entry->sp_pwdp+1);
                    }
                    ret = putspent (spent, dup_file);
                    _free_shadow_entry (spent);
                    if (ret < 0) {
                        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
                                "File write failure", error, FALSE);
                    }
                    done = TRUE;
                    continue;
                }
                break;
            }
            default:
                break;
            }
        }
        if (putspent (entry, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
    }

    if (!done) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Operation did not complete",
                error, FALSE);
    }

    return TRUE;
}

static gboolean
_update_shadow_entry (
		GumdDaemonUser *self,
		GumOpType op,
		FILE *source_file,
		FILE *dup_file,
		gpointer user_data,
		GError **error)
{
    /* Loop all entries */
    gboolean done = FALSE;
    struct spwd *entry = NULL;

    while ((entry = fgetspent (source_file)) != NULL) {
        if (!done) {
            switch (op) {
            case GUM_OPTYPE_ADD:
                if (g_strcmp0 (self->priv->shadow->sp_namp,
                        entry->sp_namp) == 0) {
                    GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_ALREADY_EXISTS,
                            "File write failure", error, FALSE);
                }
                break;
            case GUM_OPTYPE_DELETE:
                if (g_strcmp0 (self->priv->shadow->sp_namp,
                        entry->sp_namp) == 0) {
                    done = TRUE;
                    continue;
                }
                break;
            case GUM_OPTYPE_MODIFY: {
                gchar *old_name = user_data ? (gchar *)user_data :
                        self->priv->shadow->sp_namp;
                if (g_strcmp0 (old_name, entry->sp_namp) == 0) {
                    if (putspent (self->priv->shadow, dup_file) < 0) {
                        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
                                "File write failure", error, FALSE);
                    }
                    done = TRUE;
                    continue;
                }
                break;
            }
            default:
                break;
            }
        }
        if (putspent (entry, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
    }

    /* Write entry to file in case it is first entry in the file */
    if (!done && op == GUM_OPTYPE_ADD) {
        if (putspent (self->priv->shadow, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Add entry failure",
                    error, FALSE);
        }
        done = TRUE;
    }

    if (!done) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "Operation did not complete",
                error, FALSE);
    }

    return TRUE;
}

gboolean
_set_group (
        GumdDaemonUser *self,
        GError **error)
{
    gboolean group_exists = FALSE;
    GumdDaemonGroup *group = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;
    const gchar *primary_gname = NULL;
    struct group *grp = NULL;

    group = gumd_daemon_group_new (self->priv->config);
    if (!group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_GROUP_ADD_FAILURE,
                        "Group add failure", error, FALSE);
    }
    GumUserType ut = _get_usertype_from_gecos (self->priv->pw);
    GumGroupType grp_type = (ut == GUM_USERTYPE_SYSTEM) ?
            GUM_GROUPTYPE_SYSTEM : GUM_GROUPTYPE_USER;

    primary_gname = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_USR_PRIMARY_GRPNAME);
    if (primary_gname) {
        grp = gum_file_getgrnam (primary_gname,
                gum_config_get_string (self->priv->config,
                        GUM_CONFIG_GENERAL_GROUP_FILE));
    }

    if (!grp) {
        g_object_set (G_OBJECT(group), "groupname",
                primary_gname ? primary_gname : self->priv->pw->pw_name,
                        "grouptype", grp_type, NULL);
        if (!(group_exists = gumd_daemon_group_add (group,
                (gid_t)self->priv->pw->pw_uid, &gid, error))) {
            goto _finished;
        }
    } else {
        gid = grp->gr_gid;
        group_exists = TRUE;
    }

    _set_gid_property (self, gid);

_finished:
    g_object_unref (group);
    return group_exists;
}

gboolean
_set_default_groups (
        GumdDaemonUser *self,
        GError **error)
{
    gboolean added = TRUE;
    gchar **def_groupsv = NULL;

    GumUserType ut = _get_usertype_from_gecos (self->priv->pw);
    if (ut == GUM_USERTYPE_SYSTEM)
        return TRUE;

    if (ut == GUM_USERTYPE_ADMIN)
        def_groupsv = g_strsplit (gum_config_get_string (self->priv->config,
                GUM_CONFIG_GENERAL_DEF_ADMIN_GROUPS), ",", -1);
    else
        def_groupsv = g_strsplit (gum_config_get_string (self->priv->config,
                GUM_CONFIG_GENERAL_DEF_USR_GROUPS), ",", -1);

    if (def_groupsv) {
        gint ind = 0;
        while (def_groupsv[ind]) {
            GumdDaemonGroup *agroup = gumd_daemon_group_new (self->priv->config);
            if (agroup) {
                g_object_set (G_OBJECT(agroup), "groupname", def_groupsv[ind],
                        NULL);
                added = gumd_daemon_group_add_member (agroup,
                        self->priv->pw->pw_uid, FALSE, error);
                g_object_unref (agroup);
                if (!added) {
                    WARN ("Failed to set group : %s", def_groupsv[ind]);
                }
            } else {
                GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_GROUP_ADD_FAILURE,
                                "Unable to add default groups", error, FALSE);
            }
            ind++;
        }
        g_strfreev (def_groupsv);
    }

    return added;
}

gboolean
_create_home_dir (
        GumdDaemonUser *self,
        GError **error)
{
    if (_get_usertype_from_gecos (self->priv->pw) == GUM_USERTYPE_SYSTEM) {
        return TRUE;
    }

    return gum_file_create_home_dir (self->priv->pw->pw_dir,
	        self->priv->pw->pw_uid, self->priv->pw->pw_gid,
	        gum_config_get_uint (self->priv->config,
		            GUM_CONFIG_GENERAL_UMASK, GUM_UMASK), error);
}

gboolean
_delete_home_dir (
        GumdDaemonUser *self,
        GError **error)
{
    if (_get_usertype_from_gecos (self->priv->pw) == GUM_USERTYPE_SYSTEM) {
        return TRUE;
    }

    return gum_file_delete_home_dir (self->priv->pw->pw_dir, error);
}

gboolean
_delete_group (
        GumdDaemonUser *self,
        GError **error)
{
    gboolean deleted = FALSE;
    GumdDaemonGroup *group = NULL;

    group = gumd_daemon_group_new (self->priv->config);
    if (!group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_GROUP_DELETE_FAILURE,
                        "Group delete failure - unable to create group obj",
                        error, FALSE);
    }

    g_object_set (G_OBJECT(group), "gid", self->priv->pw->pw_gid, NULL);
    if (!(deleted = gumd_daemon_group_delete (group, error))) {
        goto _finished;
    }

_finished:
    g_object_unref (group);
    return deleted;
}

static gboolean
_get_userinfo_path(GumdDaemonUser *self, gchar *path, gulong n)
{
    const gchar *str = NULL;

    str = gum_config_get_string (self->priv->config, GUM_CONFIG_GENERAL_USERINFO_DIR);
    if (!str) {
        WARN("Failed to get userinfo file path");
        return FALSE;
    }

    g_snprintf(path, n, "%s%d", str, self->priv->pw->pw_uid);

    return TRUE;
}

static void
_delete_userinfo(GumdDaemonUser *self)
{
    gchar path[GUMD_STR_LEN];

    if (!_get_userinfo_path(self, path, sizeof(path)))
        return;

    g_remove(path);
}

static gboolean
_add_userinfo(GumdDaemonUser *self)
{
    GKeyFile *key = NULL;
    GError *error = NULL;
    gchar path[GUMD_STR_LEN];

    key = g_key_file_new();
    if (!key)
        return FALSE;

    if (self->priv->info->icon)
        g_key_file_set_string(key, "User", "Icon", self->priv->info->icon);

    if (!_get_userinfo_path(self, path, sizeof(path)))
        return FALSE;

    if (!g_key_file_save_to_file(key, path, &error)) {
        WARN("Key file save failure error %d:%s", error ? error->code : 0,
                error ? error->message : "");
        g_key_file_free(key);
        g_clear_error(&error);
        return FALSE;
    }

    g_key_file_free(key);

    return TRUE;
}

static gboolean
_update_userinfo(GumdDaemonUser *self, struct _userinfo *info)
{
    GKeyFile *key = NULL;
    GError *error = NULL;
    gchar path[GUMD_STR_LEN];

    key = g_key_file_new();
    if (!key)
        return FALSE;

    if (!_get_userinfo_path(self, path, sizeof(path)))
        return FALSE;

    g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL);

    if (info->icon)
        g_key_file_set_string(key, "User", "Icon", info->icon);

    if (!g_key_file_save_to_file(key, path, &error)) {
        WARN("Key file save failure error %d:%s", error ? error->code : 0,
                error ? error->message : "");
        g_key_file_free(key);
        g_clear_error(&error);
        return FALSE;
    }

    g_key_file_free(key);

    return TRUE;
}

static gboolean
_get_userinfo(GumdDaemonUser *self, struct _userinfo *info)
{
    GKeyFile *key = NULL;
    GError *error = NULL;
    gchar path[GUMD_STR_LEN];

    key = g_key_file_new();
    if (!key)
        return FALSE;

    if (!_get_userinfo_path(self, path, sizeof(path)))
        return FALSE;

    if (!g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, &error)) {
        g_key_file_free(key);
        g_clear_error(&error);
        return FALSE;
    }

    info->icon = g_key_file_get_string(key, "User", "Icon", NULL);
    if (!info->icon)
        info->icon = "";

    g_key_file_free(key);

    return TRUE;
}

static gboolean
_copy_userinfo_struct (
        struct _userinfo *src,
        GumdDaemonUser *dest)
{
    if (!src || !dest)
        return FALSE;

    _set_icon_property (dest, src->icon);

    return TRUE;
}

static struct passwd *
_get_passwd (
        GumdDaemonUser *self,
        GError **error)
{
    struct passwd *pwd = NULL;
    gid_t s_uid = GUM_USER_INVALID_UID;
    gchar *s_name = NULL;

    /* If uid or name is set, get the passwd accordingly along with basic
     * checks */

    if (self->priv->pw->pw_uid != GUM_USER_INVALID_UID) {
        s_uid = self->priv->pw->pw_uid;
        pwd = gum_file_getpwuid (s_uid, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE));
    }

    if (self->priv->pw->pw_name) {
        s_name = self->priv->pw->pw_name;
        if (!pwd) {
            pwd = gum_file_getpwnam (s_name, gum_config_get_string (
                    self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE));
        }
    }

    if (!pwd ||
        (s_uid != GUM_USER_INVALID_UID && s_uid != pwd->pw_uid) ||
        (s_name && g_strcmp0 (s_name, pwd->pw_name) != 0)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found",
                error, FALSE);
    }

    return pwd;
}

static gboolean
_copy_passwd_struct (
        struct passwd *src,
        GumdDaemonUser *dest)
{
    if (!src || !dest) return FALSE;
    gchar *str = NULL;

    _set_username_property (dest, src->pw_name);
    _set_secret_property (dest, src->pw_passwd);
    _set_uid_property (dest, src->pw_uid);
    _set_gid_property (dest, src->pw_gid);

    str = gum_string_utils_get_string (src->pw_gecos, ",",
            GECOS_FIELD_REALNAME);
    _set_realname_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",", GECOS_FIELD_OFFICE);
    _set_office_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",",
            GECOS_FIELD_OFFICEPHONE);
    _set_officephone_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",",
            GECOS_FIELD_HOMEPHONE);
    _set_homephone_property (dest, str);
    g_free (str);

    _set_usertype_property (dest, _get_usertype_from_gecos (src));

    _set_homedir_property (dest, src->pw_dir);
    _set_shell_property (dest, src->pw_shell);

    return TRUE;
}

static gboolean
_copy_passwd_data (
        GumdDaemonUser *self,
        GError **error)
{
    struct passwd *pent = NULL;
    struct spwd *spent = NULL;
    struct _userinfo info = {0,};
    DBG("");

    if ((pent = _get_passwd (self, error)) == NULL) {
        return FALSE;
    }
    spent = gum_file_getspnam (pent->pw_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_SHADOW_FILE));
    if (!spent) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found",
                error, FALSE);
    }
    _get_userinfo (self, &info);

    /*passwd entry*/
    _copy_passwd_struct (pent, self);

    /*shadow entry*/
    _copy_shadow_struct (spent, self->priv->shadow, FALSE);

    /*userinfo entry*/
    _copy_userinfo_struct (&info, self);

    return TRUE;
}

gboolean
_is_user_logged_in (
        GDBusProxy *proxy,
        uid_t uid)
{
    GError *error = NULL;
    gboolean loggedin = FALSE;
    GVariant *res = NULL;
    res = g_dbus_proxy_call_sync (proxy, "ListSessions",
                    g_variant_new ("()"), G_DBUS_CALL_FLAGS_NONE, -1,
                    NULL, &error);
    if (res) {
        uid_t c_uid;
        GVariantIter *iter = NULL;

        g_variant_get (res, "(a(susso))", &iter);
        g_variant_unref (res);

        while (g_variant_iter_next (iter, "(susso)", NULL, &c_uid, NULL, NULL,
                NULL)) {
            if (c_uid == uid) {
                DBG ("user %d is logged in", uid);
                loggedin = TRUE;
                break;
            }
        }
        g_variant_iter_free (iter);
    }

    if (error) g_error_free (error);

    return loggedin;
}

gboolean
_terminate_user (
        uid_t uid)
{
    gboolean retval = TRUE;

    /* when run in test mode, separate dbus-daemon is started. consequently no
     * system dbus services are available */
    /* TODO: In case where systemd does not exist (e.g. ubuntu), termination of
     * user-to-be-deleted sessions is required */

#if USE_SYSTEMD && !defined(ENABLE_TESTS)
    GError *error = NULL;
    GDBusProxy *proxy = NULL;
    GVariant *res = NULL;

    GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL,
            &error);
    if (error) goto _finished;

    proxy = g_dbus_proxy_new_sync (connection,
        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
        "org.freedesktop.login1", //destination
        "/org/freedesktop/login1", //path
        "org.freedesktop.login1.Manager", //interface
        NULL, &error);
    if (error) goto _finished;

    if (_is_user_logged_in (proxy, uid)) {

        res = g_dbus_proxy_call_sync (proxy, "TerminateUser",
                g_variant_new ("(u)", uid), G_DBUS_CALL_FLAGS_NONE, -1,
                NULL, &error);
        if (res) g_variant_unref (res);
        /* seems some bug in systemd as it terminates all the sessions,
         * and sends userremoved signal but still spits out the error;
         * so it need to be verified by checking again whether the user is
         * still logged in or not */
        if (!_is_user_logged_in (proxy, uid) && error) {
            g_error_free (error);
            error = NULL;
        }
    }

_finished:
    if (error) {
        DBG ("failed to terminate user: %s", error->message);
        g_error_free (error);
        retval = FALSE;
    }
    if (proxy) g_object_unref (proxy);
    if (connection) g_object_unref (connection);
#endif

    return retval;
}

GumdDaemonUser *
gumd_daemon_user_new (
        GumConfig *config)
{
    return GUMD_DAEMON_USER (g_object_new (GUMD_TYPE_DAEMON_USER, "config",
            config, NULL));
}

GumdDaemonUser *
gumd_daemon_user_new_by_uid (
        uid_t uid,
        GumConfig *config)
{
    if (!gum_lock_pwdf_lock ()) {
        WARN ("Database already locked");
        return NULL;
    }

    GumdDaemonUser *usr = GUMD_DAEMON_USER (g_object_new (GUMD_TYPE_DAEMON_USER,
            "config", config, "uid", uid, NULL));

    if (usr && !_copy_passwd_data (usr, NULL)) {
        g_object_unref (usr); usr = NULL;
    }

    gum_lock_pwdf_unlock ();
    return usr;
}

GumdDaemonUser *
gumd_daemon_user_new_by_name (
        const gchar *username,
        GumConfig *config)
{
    GumdDaemonUser *usr = NULL;
    if (username) {
        if (!gum_lock_pwdf_lock ()) {
            WARN ("Database already locked");
            return NULL;
        }

        usr = GUMD_DAEMON_USER (g_object_new (GUMD_TYPE_DAEMON_USER, "config",
                config, "username", username, NULL));
        if (usr && !_copy_passwd_data (usr, NULL)) {
            g_object_unref (usr); usr = NULL;
        }

        gum_lock_pwdf_unlock ();
    }
    return usr;
}

gboolean
gumd_daemon_user_add (
        GumdDaemonUser *self,
        uid_t *uid,
        GError **error)
{
    GumUserType usertype = GUM_USERTYPE_NONE;
    DBG ("");

    /* reset uid if set
     * set user type
     * lock db
     ** set user id
     ***  set user_name
     ***  check if user already exist
     ***  allocate user id
     ** set group
     ** set shadow data
     *** set secret, name, etc
     ** update passwd file
     ** update shadow file
     ** set home dir
     *** copy skel files and set permissions
     * unlock db
     */
    usertype = _get_usertype_from_gecos (self->priv->pw);
    if (usertype == GUM_USERTYPE_NONE) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_INVALID_USER_TYPE,
                            "Invalid user type", error, FALSE);
    }

    if (!self->priv->pw->pw_shell) {
        if (usertype == GUM_USERTYPE_SECURITY)  {
            _set_shell_property (self, gum_config_get_string (self->priv->config,
                GUM_CONFIG_GENERAL_SEC_SHELL));
        }
        else    {
            _set_shell_property (self, gum_config_get_string (self->priv->config,
                GUM_CONFIG_GENERAL_SHELL));
        }
    }

    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if (!_set_uid (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (!_set_group (self, error) ||
        !_set_shadow_data (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_ADD,
            (GumFileUpdateCB)_update_passwd_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), NULL, error) ||
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_ADD,
            (GumFileUpdateCB)_update_shadow_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_SHADOW_FILE), NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    _add_userinfo(self);

    _set_default_groups (self, error);

    if (!_create_home_dir (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (uid) {
        *uid = self->priv->pw->pw_uid;
    }

    const gchar *scrip_dir = USERADD_SCRIPT_DIR;
#   ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("UM_USERADD_DIR");
    if (env_val)
        scrip_dir = env_val;
#   endif

    gchar *ut = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
                    GECOS_FIELD_USERTYPE);
    gum_utils_run_user_scripts (scrip_dir, self->priv->pw->pw_name,
            self->priv->pw->pw_uid, self->priv->pw->pw_gid,
            self->priv->pw->pw_dir, ut);

    g_free (ut);
    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_user_delete (
        GumdDaemonUser *self,
        gboolean rem_home_dir,
        GError **error)
{
	DBG ("");

    /* lock db
     ** update passwd and shadow data structures
     ** update passwd file
     ** update shadow file
     ** delete user from groups
     ** delete home dir (if asked)
     * unlock db
     */
    gboolean lock = TRUE;

    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if (self->priv->pw->pw_uid == GUM_USER_INVALID_UID) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User uid invalid", error,
                FALSE);
    }

    if (!_copy_passwd_data (self, error)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found", error,
                FALSE);
    }

	/* deny if user is self-destructing */
    if (self->priv->pw->pw_uid == geteuid ()) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_SELF_DESTRUCTION,
                "Self-destruction not possible", error, FALSE);
    }

    /* lock the user */
    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_lock_shadow_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, NULL)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_LOCK_FAILURE,
                "unable to lock user to login", error, FALSE);
    }

    if (!_terminate_user (self->priv->pw->pw_uid)) {
        /* unlock the user */
        lock = FALSE;
        if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                    (GumFileUpdateCB)_lock_shadow_entry,
                    gum_config_get_string (self->priv->config,
                    GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, NULL)) {
            WARN("Failed to unlock shadow entry");
        }
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_SESSION_TERM_FAILURE,
                "unable to terminate user active sessions", error, FALSE);
    }

    const gchar *scrip_dir = USERDEL_SCRIPT_DIR;
#   ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("UM_USERDEL_DIR");
    if (env_val)
        scrip_dir = env_val;
#   endif

    gum_utils_run_user_scripts (scrip_dir, self->priv->pw->pw_name,
            self->priv->pw->pw_uid, self->priv->pw->pw_gid,
            self->priv->pw->pw_dir, NULL);

    _delete_userinfo(self);

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_DELETE,
            (GumFileUpdateCB)_update_passwd_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), NULL, error) ||
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_DELETE,
            (GumFileUpdateCB)_update_shadow_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_SHADOW_FILE), NULL, error)) {

        /* unlock the user */
        lock = FALSE;
        if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                    (GumFileUpdateCB)_lock_shadow_entry,
                    gum_config_get_string (self->priv->config,
                    GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, NULL)) {
            WARN("Failed to unlock shadow entry");
        }
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (!_delete_group (self, error) ||
        !gumd_daemon_group_delete_user_membership (self->priv->config,
                        self->priv->pw->pw_name, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (rem_home_dir && !_delete_home_dir (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    gum_lock_pwdf_unlock ();
    return TRUE;
}

static void
_update_gecos_field (
        struct passwd *src,
        GumdDaemonUser *dest,
        GecosField field)
{
    gchar *str = NULL;
    str = gum_string_utils_get_string (dest->priv->pw->pw_gecos, ",", field);
    if (str && strlen(str) > 0) {
        g_free(str);
        return;
    }
    g_free(str);

    str = gum_string_utils_get_string (src->pw_gecos, ",", field);
    switch (field) {
    case GECOS_FIELD_REALNAME:
        _set_realname_property (dest, str);
        break;
    case GECOS_FIELD_OFFICE:
        _set_office_property (dest, str);
        break;
    case GECOS_FIELD_OFFICEPHONE:
        _set_officephone_property (dest, str);
        break;
    case GECOS_FIELD_HOMEPHONE:
        _set_homephone_property (dest, str);
        break;
    case GECOS_FIELD_USERTYPE:
        _set_usertype_property (dest, gum_user_type_from_string (str));
        break;
    default:
        break;
    }
    g_free(str);
}

gboolean
gumd_daemon_user_update (
        GumdDaemonUser *self,
        GError **error)
{
    struct passwd *pw = NULL;
    struct spwd *shadow = NULL;
    struct _userinfo info = {0,};
    gchar *old_name = NULL;
    gint change = 0;

    DBG ("");

    /* Only secret, realname, office, officephone, homephone and
     * shell can be updated */
    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if (self->priv->pw->pw_uid == GUM_USER_INVALID_UID) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User uid invalid", error,
                FALSE);
    }

    if ((pw = _get_passwd (self, error)) == NULL) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if ((shadow = gum_file_getspnam (pw->pw_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_SHADOW_FILE))) == NULL) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND,
                "User not found in Shadow", error, FALSE);
    }

    /* userinfo entry */
    _get_userinfo (self, &info);

    if (self->priv->info->icon && g_strcmp0 (info.icon, self->priv->info->icon)) {
        change++;
        info.icon = self->priv->info->icon;
    }
    if (change)
        _update_userinfo(self, &info);

    /* shadow entry */
    _copy_shadow_struct (shadow, self->priv->shadow, TRUE);

    /* passwd entry */
    _set_username_property (self, pw->pw_name);
    _set_uid_property (self, pw->pw_uid);
    _set_gid_property (self, pw->pw_gid);
    _set_homedir_property (self, pw->pw_dir);

    _update_gecos_field (pw, self, GECOS_FIELD_REALNAME);
    _update_gecos_field (pw, self, GECOS_FIELD_OFFICE);
    _update_gecos_field (pw, self, GECOS_FIELD_OFFICEPHONE);
    _update_gecos_field (pw, self, GECOS_FIELD_HOMEPHONE);
    _update_gecos_field (pw, self, GECOS_FIELD_USERTYPE);

    if (self->priv->pw->pw_passwd &&
        g_strcmp0 (self->priv->pw->pw_passwd, "x") != 0 &&
        gum_crypt_cmp_secret (self->priv->pw->pw_passwd,
                self->priv->shadow->sp_pwdp) != 0) {
        change++;
        if (!_set_secret (self, error)) {
            gum_lock_pwdf_unlock ();
            return FALSE;
        }

    } else {
        _set_secret_property (self, pw->pw_passwd);
    }

    if (self->priv->pw->pw_gecos &&
        g_strcmp0 (pw->pw_gecos, self->priv->pw->pw_gecos) != 0) {
        gchar *str1 = NULL, *str2 = NULL;
        DBG ("old gecos %s :: new gecos %s", pw->pw_gecos,
                self->priv->pw->pw_gecos);
        //usertype cannot change
        str1 = gum_string_utils_get_string (self->priv->pw->pw_gecos, ",",
                GECOS_FIELD_USERTYPE);
        str2 = gum_string_utils_get_string (pw->pw_gecos, ",",
                GECOS_FIELD_USERTYPE);
        if (g_strcmp0 (str1, str2) != 0) {
            gum_lock_pwdf_unlock ();
            g_free (str1); g_free (str2);
            GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_INVALID_USER_TYPE,
                            "User type cannot be updated", error, FALSE);
        }
        g_free (str1); g_free (str2);
        //realname, office, officephone, homephone can change
        change++;
    }

    if (self->priv->pw->pw_shell &&
        g_strcmp0 (pw->pw_shell, self->priv->pw->pw_shell) != 0){
        DBG ("old shell %s :: new shell %s", pw->pw_shell,
                self->priv->pw->pw_shell);
        change++;
    }

    if (change == 0) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NO_CHANGES,
                "No changes registered", error, FALSE);
    }

    GUM_STR_DUP (pw->pw_name, old_name);
    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_update_passwd_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_PASSWD_FILE), old_name, error) ||
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_update_shadow_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_SHADOW_FILE), old_name, error)) {
        g_free (old_name);
        gum_lock_pwdf_unlock ();
        return FALSE;
    }
    g_free (old_name);

    gum_lock_pwdf_unlock ();
    return TRUE;
}

uid_t
gumd_daemon_user_get_uid_by_name (
        const gchar *username,
        GumConfig *config)
{
    uid_t uid = GUM_USER_INVALID_UID;
    if (username) {
        if (!gum_lock_pwdf_lock ()) {
            return uid;
        }
        struct passwd *pwd = gum_file_getpwnam (username,
                gum_config_get_string (config, GUM_CONFIG_GENERAL_PASSWD_FILE));
        gum_lock_pwdf_unlock ();
        if (pwd) {
            return pwd->pw_uid;
        }
    }
    return uid;
}

GVariant *
gumd_daemon_user_get_user_list (
        const gchar *const *types,
        GumConfig *config,
        GError **error)
{
    GVariantBuilder builder;
    GVariant *users = NULL;
    struct passwd *pent = NULL;
    FILE *fp = NULL;
    guint16 in_types = GUM_USERTYPE_NONE;
    GumUserType ut;
    uid_t sys_uid_min, sys_uid_max;
    const gchar *fn = NULL;
    DBG ("");

    /* If user type is NULL or empty string, then return all users */
    if (!types || g_strv_length ((gchar **)types) <= 0)
        in_types = GUM_USERTYPE_SYSTEM | GUM_USERTYPE_ADMIN |
        GUM_USERTYPE_GUEST | GUM_USERTYPE_NORMAL;
    else
        in_types = gum_user_type_from_strv (types);

    if (in_types == GUM_USERTYPE_NONE) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_INVALID_USER_TYPE,
                "Invalid user type specified", error, NULL);
    }

    DBG ("get user list in types %d", in_types);
    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, NULL);
    }

    fn = gum_config_get_string (config, GUM_CONFIG_GENERAL_PASSWD_FILE);
    if (!fn || !(fp = fopen (fn, "r"))) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_OPEN,
                "Opening passwd file failed", error, NULL);
    }

    sys_uid_min = (uid_t) gum_config_get_uint (config,
            GUM_CONFIG_GENERAL_SYS_UID_MIN, GUM_USER_INVALID_UID);

    sys_uid_max = (uid_t) gum_config_get_uint (config,
            GUM_CONFIG_GENERAL_SYS_UID_MAX, GUM_USER_INVALID_UID);

    g_variant_builder_init (&builder, (const GVariantType *)"au");
    while ((pent = fgetpwent (fp)) != NULL) {
        /* If type is an empty string, all users are fetched. User type is
         * first compared with usertype in gecos field. If gecos field for
         * usertype does not exist, then all the users are considered as
         * normal users other than system users which are filtered out based
         * on system min and max uids
         * */
        ut = _get_usertype_from_gecos (pent);
        if (ut == GUM_USERTYPE_NONE) {
            if (pent->pw_uid >=sys_uid_min && pent->pw_uid <= sys_uid_max)
                ut = GUM_USERTYPE_SYSTEM;
        }
        if (ut & in_types) {
            g_variant_builder_add (&builder, "u", pent->pw_uid);
        }
        pent = NULL;
    }
    users = g_variant_builder_end (&builder);
    fclose (fp);

    gum_lock_pwdf_unlock ();
    return users;
}
