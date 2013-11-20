/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gumd
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

#include <string.h>
#include <shadow.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <errno.h>

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

struct _GumdDaemonUserPrivate
{
    GumConfig *config;
    guint16 user_type;
    gchar *nick_name;

    /* passwd file entries format:
	 * name:passwd:uid:gid:gecos:dir:shell
	 * whereas gecos contain the following comma(',') separated values
	 * realname,officelocation,officephone,homephone
	 */
    struct passwd *pw;

    /* shadow file entries format:
     * username:encrypted_password:last_change_date:min_changes_days:
     * max_changes_days:expire_warn_days:account_disable_days:
     * account_expiry_date:reserved_field
     */
    struct spwd *shadow;
};

G_DEFINE_TYPE (GumdDaemonUser, gumd_daemon_user, G_TYPE_OBJECT)

#define GUMD_DAEMON_USER_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUMD_TYPE_DAEMON_USER, GumdDaemonUserPrivate)

#define GUMD_DAY (24L*3600L)

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

    N_PROPERTIES
};

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
    if (self->priv->user_type != value) {
        self->priv->user_type = value;
        g_object_notify_by_pspec (G_OBJECT (self),
                properties[PROP_USERTYPE]);
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
            0);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, 0, 4);
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
            1);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, 1, 4);
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
            2);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, 2, 4);
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
            3);
    if (g_strcmp0 (value, str) != 0 &&
        gum_validate_db_string_entry (value, NULL)) {
        gchar *gecos = gum_string_utils_insert_string (
                self->priv->pw->pw_gecos, ",", value, 3, 4);
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
            g_value_set_uint (value, (guint)self->priv->user_type);
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
        		        self->priv->pw->pw_gecos, ",", 0);
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
                        self->priv->pw->pw_gecos, ",", 1);
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
                        self->priv->pw->pw_gecos, ",", 2);
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
                        self->priv->pw->pw_gecos, ",", 3);
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
    } else if (self->priv->user_type == GUM_USERTYPE_SYSTEM) {
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
        GumdDaemonUser *self,
        uid_t *min,
        uid_t *max)
{
    if (self->priv->user_type == GUM_USERTYPE_SYSTEM)
        *min = (uid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_SYS_UID_MIN, GUM_USER_INVALID_UID);
    else
        *min = (uid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_UID_MIN, GUM_USER_INVALID_UID);

    if (self->priv->user_type == GUM_USERTYPE_SYSTEM)
        *max = (uid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_SYS_UID_MAX, GUM_USER_INVALID_UID);
    else
        *max = (uid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_UID_MAX, GUM_USER_INVALID_UID);

    return (*min < *max);
}

static gboolean
_find_free_uid (
        GumdDaemonUser *self,
        uid_t *uid)
{
    uid_t tmp_uid, uid_min, uid_max;

    if (!_get_default_uid_range (self, &uid_min, &uid_max))
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

    if (self->priv->user_type == GUM_USERTYPE_NORMAL ||
        self->priv->user_type == GUM_USERTYPE_ADMIN) {
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
_check_daemon_user_type (
        GumdDaemonUser *self,
        GError **error)
{
	switch (self->priv->user_type) {
		case GUM_USERTYPE_ADMIN:
		case GUM_USERTYPE_NORMAL:
		case GUM_USERTYPE_GUEST:
		case GUM_USERTYPE_SYSTEM:
			break;
		case GUM_USERTYPE_NONE:
		default:
			GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_INVALID_USER_TYPE,
					"Invalid user type", error, FALSE);
			break;
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
        if (self->priv->user_type == GUM_USERTYPE_SYSTEM)
            self->priv->shadow->sp_pwdp = g_strdup ("*");
        else
            self->priv->shadow->sp_pwdp = g_strdup ("!");
        _set_secret_property (self, "x");
        return TRUE;
    }

    /* TODO: The encrypted password field may be blank, in which
     * case no password is required to authenticate as the specified
     * login name.
     */
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
                    g_free (spent->sp_pwdp);
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
    gboolean added = FALSE;
    GumdDaemonGroup *group = NULL;
    gid_t gid = GUM_GROUP_INVALID_GID;

    group = gumd_daemon_group_new (self->priv->config);
    if (!group) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_GROUP_ADD_FAILURE,
                        "Group add failure", error, FALSE);
    }

    GumGroupType grp_type = self->priv->user_type == GUM_USERTYPE_SYSTEM ?
            GUM_GROUPTYPE_SYSTEM : GUM_GROUPTYPE_USER;
    g_object_set (G_OBJECT(group), "groupname", self->priv->pw->pw_name,
            "grouptype", grp_type, NULL);
    if (!(added = gumd_daemon_group_add (group, (gid_t)self->priv->pw->pw_uid,
            &gid, error))) {
        GUM_SET_ERROR (GUM_ERROR_USER_GROUP_ADD_FAILURE,
                        "Group add failure", error, added, FALSE);
        goto _finished;
    }
    _set_gid_property (self, gid);

_finished:
    g_object_unref (group);
    return added;
}

gboolean
_set_default_groups (
        GumdDaemonUser *self,
        GError **error)
{
    gboolean added = TRUE;
    gchar **def_groupsv = NULL;

    if (self->priv->user_type == GUM_USERTYPE_SYSTEM)
        return TRUE;

    if (self->priv->user_type == GUM_USERTYPE_ADMIN)
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
                if (!added) break;
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
	if (self->priv->user_type == GUM_USERTYPE_SYSTEM ||
		self->priv->user_type == GUM_USERTYPE_GUEST) {
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
	if (self->priv->user_type == GUM_USERTYPE_SYSTEM ||
		self->priv->user_type == GUM_USERTYPE_GUEST) {
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

    g_object_set (G_OBJECT(group), "groupname", self->priv->pw->pw_name,
            "gid", self->priv->pw->pw_gid, NULL);
    if (!(deleted = gumd_daemon_group_delete (group, error))) {
        GUM_SET_ERROR (GUM_ERROR_USER_GROUP_DELETE_FAILURE,
                        "Group delete failure", error, deleted, FALSE);
        goto _finished;
    }

_finished:
    g_object_unref (group);
    return deleted;
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

    str = gum_string_utils_get_string (src->pw_gecos, ",", 0);
    _set_realname_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",", 1);
    _set_office_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",", 2);
    _set_officephone_property (dest, str);
    g_free (str);

    str = gum_string_utils_get_string (src->pw_gecos, ",", 3);
    _set_homephone_property (dest, str);
    g_free (str);

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

    /*passwd entry*/
    _copy_passwd_struct (pent, self);

    /*shadow entry*/
    _copy_shadow_struct (spent, self->priv->shadow, FALSE);

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
        "org.freedesktop.login1", //destintation
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
    if (!_check_daemon_user_type (self, error)) {
        return FALSE;
    }

    if (!self->priv->pw->pw_shell) {
        _set_shell_property (self, gum_config_get_string (self->priv->config,
                GUM_CONFIG_GENERAL_SHELL));
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

    if (!_set_default_groups (self, error) ||
        !_create_home_dir (self, error)) {
    	gum_lock_pwdf_unlock ();
    	return FALSE;
    }

    if (uid) {
        *uid = self->priv->pw->pw_uid;
    }

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
            GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, error)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_LOCK_FAILURE,
                "unable to lock user to login", error, FALSE);
    }

    if (!_terminate_user (self->priv->pw->pw_uid)) {
        /* unlock the user */
        lock = FALSE;
        gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                    (GumFileUpdateCB)_lock_shadow_entry,
                    gum_config_get_string (self->priv->config,
                    GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, NULL);
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_SESSION_TERM_FAILURE,
                "unable to terminate user active sessions", error, FALSE);
    }

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
        gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                    (GumFileUpdateCB)_lock_shadow_entry,
                    gum_config_get_string (self->priv->config,
                    GUM_CONFIG_GENERAL_SHADOW_FILE), &lock, NULL);
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

gboolean
gumd_daemon_user_update (
        GumdDaemonUser *self,
        GError **error)
{
    struct passwd *pw = NULL;
    struct spwd *shadow = NULL;
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

    /* shadow entry */
    _copy_shadow_struct (shadow, self->priv->shadow, TRUE);

    /* passwd entry */
    _set_username_property (self, pw->pw_name);
    _set_uid_property (self, pw->pw_uid);
    _set_gid_property (self, pw->pw_gid);
    _set_homedir_property (self, pw->pw_dir);

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
        DBG ("old gecos %s :: new gecos %s", pw->pw_gecos,
                self->priv->pw->pw_gecos);
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
