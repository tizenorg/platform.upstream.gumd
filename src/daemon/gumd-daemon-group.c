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

#include <stdio.h>
#include <sys/types.h>
#include <gshadow.h>
#include <string.h>
#include <glib/gstdio.h>

#include "gumd-daemon-group.h"
#include "common/gum-file.h"
#include "common/gum-validate.h"
#include "common/gum-crypt.h"
#include "common/gum-lock.h"
#include "common/gum-string-utils.h"
#include "common/gum-defines.h"
#include "common/gum-log.h"
#include "common/gum-error.h"
#include "common/gum-utils.h"

struct _GumdDaemonGroupPrivate
{
    GumConfig *config;
    GumGroupType group_type;

    /* group file entries format:
	 * group_name:password:GID:user_list
	 */
    struct group *group;

    /* gshadow file entries format:
     * group_name:encrypted_password:administrators:members
     */
    struct sgrp *gshadow;
};

G_DEFINE_TYPE (GumdDaemonGroup, gumd_daemon_group, G_TYPE_OBJECT)

#define GUMD_DAEMON_GROUP_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        GUMD_TYPE_DAEMON_GROUP, GumdDaemonGroupPrivate)

enum
{
    PROP_0,

    PROP_CONFIG,
    PROP_GROUPTYPE,
    PROP_GID,
    PROP_GROUPNAME,
    PROP_SECRET,
    PROP_MEMBERS,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static void
_free_daemon_group_entry (
        struct group *grp)
{
    if (grp) {
        g_free (grp->gr_name);
        g_free (grp->gr_passwd);

        GUM_STR_FREEV (grp->gr_mem);
        g_free (grp);
    }
}

static void
_free_gshadow_entry (
        struct sgrp *gshadow)
{
    if (gshadow) {
        g_free (gshadow->sg_namp);
        g_free (gshadow->sg_passwd);
        GUM_STR_FREEV (gshadow->sg_adm);
        GUM_STR_FREEV (gshadow->sg_mem);
        g_free (gshadow);
    }
}

static void
_set_config_property (
        GumdDaemonGroup *self,
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
_set_gid_property (
        GumdDaemonGroup *self,
        gid_t value)
{
    if (self->priv->group->gr_gid != value) {
        self->priv->group->gr_gid = value;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GID]);
    }
}

static void
_set_grouptype_property (
        GumdDaemonGroup *self,
        GumGroupType value)
{
    if (self->priv->group_type != value) {
        self->priv->group_type = value;
        g_object_notify_by_pspec (G_OBJECT (self),
                properties[PROP_GROUPTYPE]);
    }
}

static void
_set_groupname_property (
        GumdDaemonGroup *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->group->gr_name) != 0 &&
        gum_validate_name (value, NULL)) {
        GUM_STR_FREE (self->priv->group->gr_name);
        self->priv->group->gr_name = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self),
                properties[PROP_GROUPNAME]);
    }
}

static void
_set_secret_property (
        GumdDaemonGroup *self,
        const gchar *value)
{
    if (g_strcmp0 (value, self->priv->group->gr_passwd) != 0 &&
        gum_validate_db_secret_entry (value, NULL)) {
        GUM_STR_FREE (self->priv->group->gr_passwd);
        self->priv->group->gr_passwd = g_strdup (value);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECRET]);
    }
}

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    GumdDaemonGroup *self = GUMD_DAEMON_GROUP (object);
    switch (property_id) {
        case PROP_CONFIG: {
            _set_config_property (self, g_value_dup_object (value));
            break;
        }
        case PROP_GID: {
            _set_gid_property (self, g_value_get_uint (value));
            break;
        }
        case PROP_GROUPTYPE: {
            _set_grouptype_property (self,
                    (GumGroupType)g_value_get_uint (value));
            break;
        }
        case PROP_GROUPNAME: {
            _set_groupname_property (self, g_value_get_string (value));
            break;
        }
        case PROP_SECRET: {
            _set_secret_property (self, g_value_get_string (value));
            break;
        }
        /* No need to writable userlist property as there are APIs for adding
         * and removing members */
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
    GumdDaemonGroup *self = GUMD_DAEMON_GROUP (object);

    switch (property_id) {
        case PROP_CONFIG: {
            g_value_set_object (value, self->priv->config);
            break;
        }
        case PROP_GROUPTYPE: {
            g_value_set_uint (value, (guint)self->priv->group_type);
            break;
        }
        case PROP_GID: {
            g_value_set_uint (value, self->priv->group->gr_gid);
            break;
        }
        case PROP_GROUPNAME: {
            g_value_set_string (value, self->priv->group->gr_name);
            break;
        }
        case PROP_SECRET: {
            g_value_set_string (value, self->priv->group->gr_passwd);
            break;
        }
        case PROP_MEMBERS: {
            if (self->priv->group->gr_mem) {
                gchar *members = g_strjoinv (",", self->priv->group->gr_mem);
                if (members) {
                    g_value_set_string (value, members);
                    g_free (members);
                }
            }
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (GObject *object)
{
    GumdDaemonGroup *self = GUMD_DAEMON_GROUP (object);

    GUM_OBJECT_UNREF (self->priv->config);

    G_OBJECT_CLASS (gumd_daemon_group_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    GumdDaemonGroup *self = GUMD_DAEMON_GROUP (object);

    if (self->priv->gshadow) {
    	_free_gshadow_entry (self->priv->gshadow);
    	self->priv->gshadow = NULL;
    }

    if (self->priv->group) {
    	_free_daemon_group_entry (self->priv->group);
        self->priv->group = NULL;
    }

    G_OBJECT_CLASS (gumd_daemon_group_parent_class)->finalize (object);
}

static void
gumd_daemon_group_init (
        GumdDaemonGroup *self)
{
    self->priv = GUMD_DAEMON_GROUP_PRIV(self);
    self->priv->config = NULL;
    self->priv->group = g_malloc0 (sizeof (struct group));
    self->priv->group->gr_gid = G_MAXUINT;
    self->priv->gshadow = g_malloc0 (sizeof (struct sgrp));
}

static void
gumd_daemon_group_class_init (
        GumdDaemonGroupClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (GumdDaemonGroupPrivate));

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

    properties[PROP_GROUPTYPE] =  g_param_spec_uint ("grouptype",
            "GroupType",
            "Type of the group",
            0,
            G_MAXUINT16,
            GUM_GROUPTYPE_NONE /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GID] =  g_param_spec_uint ("gid",
            "Gid",
            "Unique identifier of the group of the user",
            0,
            G_MAXUINT,
            G_MAXUINT /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_GROUPNAME] = g_param_spec_string ("groupname",
            "GroupName",
            "Group name",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_SECRET] = g_param_spec_string ("secret",
            "Secret",
            "Group secret",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);

    properties[PROP_MEMBERS] = g_param_spec_string ("userlist",
            "UserList",
            "Users of the group",
            "" /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES,
    		properties);
}

static gboolean
_get_default_gid_range (
        GumdDaemonGroup *self,
        gid_t *min,
        gid_t *max)
{
    if (self->priv->group_type == GUM_GROUPTYPE_SYSTEM)
        *min = (gid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_SYS_GID_MIN, G_MAXUINT);
    else
        *min = (gid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_GID_MIN, G_MAXUINT);

    if (self->priv->group_type == GUM_GROUPTYPE_SYSTEM)
        *max = (gid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_SYS_GID_MAX, G_MAXUINT);
    else
        *max = (gid_t) gum_config_get_uint (self->priv->config,
                GUM_CONFIG_GENERAL_GID_MAX, G_MAXUINT);

    return (*min < *max);
}

static gboolean
_find_free_gid (
        GumdDaemonGroup *self,
        gid_t preferred_gid,
        gid_t *gid)
{
    gid_t tmp_gid, gid_min, gid_max;

    if (preferred_gid != GUM_GROUP_INVALID_GID &&
        gum_file_getgrgid (preferred_gid, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_GROUP_FILE)) == NULL) {
        *gid = preferred_gid;
        return TRUE;
    }

    if (!_get_default_gid_range (self, &gid_min, &gid_max))
        return FALSE;

    /* Select the first available gid */
    tmp_gid = gid_min;
    while (tmp_gid <= gid_max) {
        if (gum_file_getgrgid (tmp_gid, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_GROUP_FILE)) == NULL) {
            *gid = tmp_gid;
            return TRUE;
        }
        tmp_gid = tmp_gid + 1;
    }
    return FALSE;
}

static gboolean
_set_gid (
        GumdDaemonGroup *self,
        gid_t preferred_gid,
        GError **error)
{
    gid_t gid = GUM_GROUP_INVALID_GID;
    if (!gum_validate_name (self->priv->group->gr_name, error)) {
        return FALSE;
    }

    if (gum_file_getgrnam (self->priv->group->gr_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_GROUP_FILE))
            != NULL) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_ALREADY_EXISTS,
                "Group already exists", error, FALSE);
    }

    if (!_find_free_gid (self, preferred_gid, &gid)){
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_GID_NOT_AVAILABLE,
                "GID not available", error, FALSE);
    }
    _set_gid_property (self, gid);

    return TRUE;
}

static gboolean
_update_daemon_group_entry (
        GumdDaemonGroup *self,
        GumOpType op,
        FILE *source_file,
        FILE *dup_file,
        gpointer user_data,
        GError **error)
{
    gboolean done = FALSE;
    struct group *entry;

    while ((entry = fgetgrent (source_file)) != NULL) {
        if (!done) {
            switch (op) {
            case GUM_OPTYPE_ADD:
                if (self->priv->group->gr_gid < entry->gr_gid) {

                    if (putgrent (self->priv->group, dup_file) < 0) {
                        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE,
                                "File write failure", error, FALSE);
                    }
                    done = TRUE;
                }
                break;
            case GUM_OPTYPE_DELETE:
                if (self->priv->group->gr_gid == entry->gr_gid &&
                    g_strcmp0 (self->priv->group->gr_name,
                            entry->gr_name) == 0) {
                    done = TRUE;
                    continue;
                }
                break;
            case GUM_OPTYPE_MODIFY: {
                gchar *old_name = user_data ? (gchar *)user_data :
                        self->priv->group->gr_name;
                if (self->priv->group->gr_gid == entry->gr_gid &&
                    g_strcmp0 (old_name, entry->gr_name) == 0) {
                    if (putgrent (self->priv->group, dup_file) < 0) {
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
        if (putgrent (entry, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
    }

    /* Write entry to file in case it is first entry in the file */
    if (!done && op == GUM_OPTYPE_ADD) {
        if (putgrent (self->priv->group, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
        done = TRUE;
    }

    if (!done) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure", error,
                FALSE);
    }

    return TRUE;
}

static gboolean
_update_gshadow_entry (
        GumdDaemonGroup *self,
        GumOpType op,
        FILE *source_file,
        FILE *dup_file,
        gpointer user_data,
        GError **error)
{
    gboolean done = FALSE;
    struct sgrp *entry = NULL;

    while ((entry = fgetsgent (source_file)) != NULL) {
        if (!done) {
            switch (op) {
            case GUM_OPTYPE_ADD:
                if (g_strcmp0 (self->priv->gshadow->sg_namp,
                        entry->sg_namp) == 0) {
                    GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_ALREADY_EXISTS,
                            "Group already exists", error, FALSE);
                }
                break;
            case GUM_OPTYPE_DELETE:
                if (g_strcmp0 (self->priv->gshadow->sg_namp,
                        entry->sg_namp) == 0) {
                    done = TRUE;
                    continue;
                }
                break;
            case GUM_OPTYPE_MODIFY: {
                gchar *old_name = user_data ? (gchar *)user_data :
                        self->priv->gshadow->sg_namp;
                if (g_strcmp0 (old_name, entry->sg_namp) == 0) {
                    if (putsgent (self->priv->gshadow, dup_file) < 0) {
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
        if (putsgent (entry, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
    }

    /* Write entry to file in case it is first entry in the file */
    if (!done && op == GUM_OPTYPE_ADD) {
        if (putsgent (self->priv->gshadow, dup_file) < 0) {
            GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, FALSE);
        }
        done = TRUE;
    }

    if (!done) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_FILE_WRITE, "File write failure", error,
                FALSE);
    }

    return TRUE;
}

static gboolean
_set_secret (
        GumdDaemonGroup *self,
        GError **error)
{
    gsize pwd_len = 0;
    GUM_STR_FREE (self->priv->gshadow->sg_passwd);

    if (!self->priv->group->gr_passwd) {
        if (self->priv->group_type == GUM_GROUPTYPE_SYSTEM)
            self->priv->gshadow->sg_passwd = g_strdup ("*");
        else
            self->priv->gshadow->sg_passwd = g_strdup ("!");
        self->priv->group->gr_passwd = g_strdup("x");
        return TRUE;
    }

    pwd_len = strlen(self->priv->group->gr_passwd);

    self->priv->gshadow->sg_passwd = gum_crypt_encrypt_secret (
            self->priv->group->gr_passwd, gum_config_get_string (
                    self->priv->config, GUM_CONFIG_GENERAL_ENCRYPT_METHOD));
    if (!self->priv->gshadow->sg_passwd) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_SECRET_ENCRYPT_FAILURE,
                "Secret encryption failed.", error, FALSE);
    }

    memset (self->priv->group->gr_passwd, 0, pwd_len);
    _set_secret_property (self, "x");

    return TRUE;
}

static gboolean
_set_gshadow_data (
        GumdDaemonGroup *self,
        GError **error)
{
    GUM_STR_DUP (self->priv->group->gr_name, self->priv->gshadow->sg_namp);

    if (!_set_secret (self, error)) {
        return FALSE;
    }

    GUM_STR_DUPV (self->priv->group->gr_mem, self->priv->gshadow->sg_mem);

    return TRUE;
}

static gboolean
_check_group_type (
        GumdDaemonGroup *self,
        GError **error)
{
    switch (self->priv->group_type) {
        case GUM_GROUPTYPE_SYSTEM:
        case GUM_GROUPTYPE_USER:
            break;
        case GUM_GROUPTYPE_NONE:
        default:
            GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_INVALID_GROUP_TYPE,
                    "Invalid group type", error, FALSE);
            break;
    }

    return TRUE;
}

static struct group *
_get_group (
        GumdDaemonGroup *self,
        GError **error)
{
    struct group *grp = NULL;
    gid_t s_gid = G_MAXUINT;
    gchar *s_name = NULL;

    /* If gid or name is set, get the group accordingly along with basic
     * checks */

    if (self->priv->group->gr_gid != G_MAXUINT) {
        s_gid = self->priv->group->gr_gid;
        grp = gum_file_getgrgid (s_gid, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_GROUP_FILE));
    }

    if (self->priv->group->gr_name) {
        s_name = self->priv->group->gr_name;
        if (!grp) {
            grp = gum_file_getgrnam (s_name, gum_config_get_string (
                    self->priv->config, GUM_CONFIG_GENERAL_GROUP_FILE));
        }
    }

    if (!grp ||
        (s_gid != G_MAXUINT && s_gid != grp->gr_gid) ||
        (s_name && g_strcmp0 (s_name, grp->gr_name) != 0)) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NOT_FOUND, "Group not found",
                error, FALSE);
    }

    return grp;
}

static gboolean
_copy_group_prop (
        struct group *src,
        GumdDaemonGroup *dest)
{
    if (!src || !dest) return FALSE;

    _set_groupname_property (dest, src->gr_name);
    _set_gid_property (dest, src->gr_gid);
    _set_secret_property (dest, src->gr_passwd);
    GUM_STR_DUPV (src->gr_mem, dest->priv->group->gr_mem);

    return TRUE;
}

static gboolean
_copy_group_struct (
        struct group *src,
        struct group *dest)
{
    if (!src || !dest) return FALSE;

    GUM_STR_DUP (src->gr_name, dest->gr_name);
    dest->gr_gid = src->gr_gid;
    GUM_STR_DUP (src->gr_passwd, dest->gr_passwd);
    GUM_STR_DUPV (src->gr_mem, dest->gr_mem);

    return TRUE;
}

static gboolean
_copy_gshadow_struct (
        struct sgrp *src,
        struct sgrp *dest,
        gboolean copy_only_if_invalid)
{
    if (!src || !dest) return FALSE;

    if (!copy_only_if_invalid || !dest->sg_namp)
        GUM_STR_DUP (src->sg_namp, dest->sg_namp);
    if (!copy_only_if_invalid || !dest->sg_passwd)
        GUM_STR_DUP (src->sg_passwd, dest->sg_passwd);
    if (!copy_only_if_invalid || !dest->sg_adm)
        GUM_STR_DUPV (src->sg_adm, dest->sg_adm);
    if (!copy_only_if_invalid || !dest->sg_mem)
        GUM_STR_DUPV (src->sg_mem, dest->sg_mem);

    return TRUE;
}

static gboolean
_copy_group_data (
        GumdDaemonGroup *self,
        struct group *gent,
        struct sgrp *sgent,
        GError **error)
{

    if (!gent) {
        gent = _get_group (self, error);
        if (!gent) return FALSE;
    }

    if (!sgent) {
        sgent = gum_file_getsgnam (gent->gr_name, gum_config_get_string (
                self->priv->config, GUM_CONFIG_GENERAL_GSHADOW_FILE));
    }

    /*group entry*/
    _copy_group_prop (gent, self);

    /*gshadow entry*/
    _copy_gshadow_struct (sgent, self->priv->gshadow, FALSE);

    return TRUE;
}

GumdDaemonGroup *
gumd_daemon_group_new (
        GumConfig *config)
{
    return GUMD_DAEMON_GROUP (g_object_new (GUMD_TYPE_DAEMON_GROUP, "config",
            config, NULL));
}

GumdDaemonGroup *
gumd_daemon_group_new_by_gid (
        gid_t gid,
        GumConfig *config)
{
    if (!gum_lock_pwdf_lock ()) {
        WARN ("Database already locked");
        return NULL;
    }

    GumdDaemonGroup *grp = GUMD_DAEMON_GROUP (g_object_new (
            GUMD_TYPE_DAEMON_GROUP, "config", config, "gid", gid, NULL));
    if (grp && !_copy_group_data (grp, NULL, NULL, NULL)) {
        g_object_unref (grp); grp = NULL;
    }

    gum_lock_pwdf_unlock ();
    return grp;
}

GumdDaemonGroup *
gumd_daemon_group_new_by_name (
        const gchar *groupname,
        GumConfig *config)
{
    GumdDaemonGroup *grp = NULL;
    if (groupname) {
        if (!gum_lock_pwdf_lock ()) {
            WARN ("Database already locked");
            return NULL;
        }

        grp = GUMD_DAEMON_GROUP (g_object_new (GUMD_TYPE_DAEMON_GROUP,
                "config", config, "groupname", groupname, NULL));
        if (grp && !_copy_group_data (grp, NULL, NULL, NULL)) {
            g_object_unref (grp); grp = NULL;
        }

        gum_lock_pwdf_unlock ();
    }
    return grp;
}

gboolean
gumd_daemon_group_add (
        GumdDaemonGroup *self,
        gid_t preferred_gid,
        gid_t *gid,
        GError **error)
{
    DBG ("");

    /* reset gid if set
     * check group type
     * set secret
     * lock db
     ** set group id
     *** set group name
     *** check if group already exist
     *** allocate group id
     ** update group file
     ** update gshadow file
     * unlock db
     */
    const gchar *shadow_file = NULL;

    if (!_check_group_type (self, error)) {
        return FALSE;
    }

    if (!gum_lock_pwdf_lock ()) {
    	GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
    	        "Database already locked", error, FALSE);
    }

    if (!_set_gid (self, preferred_gid, error) ||
        !_set_gshadow_data (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_ADD,
            (GumFileUpdateCB)_update_daemon_group_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GROUP_FILE), NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    shadow_file = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (g_file_test (shadow_file, G_FILE_TEST_EXISTS) &&
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_ADD,
                (GumFileUpdateCB)_update_gshadow_entry, shadow_file, NULL,
                error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (gid) {
        *gid = self->priv->group->gr_gid;
    }

    const gchar *scrip_dir = GROUPADD_SCRIPT_DIR;
#   ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("UM_GROUPADD_DIR");
    if (env_val)
        scrip_dir = env_val;
#   endif

    gum_utils_run_group_scripts (scrip_dir, self->priv->group->gr_name,
            self->priv->group->gr_gid);

    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_group_delete (
        GumdDaemonGroup *self,
        GError **error)
{
    DBG ("");
    const gchar *shadow_file = NULL;

    if (!gum_lock_pwdf_lock ()) {
    	GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
    	        "Database already locked", error, FALSE);
    }

    if (self->priv->group->gr_gid == GUM_GROUP_INVALID_GID) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NOT_FOUND, "Group gid invalid",
                error, FALSE);
    }

    if (!_copy_group_data (self, NULL, NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (self->priv->group->gr_gid == getegid ()) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_SELF_DESTRUCTION,
        		"Self-destruction not possible", error, FALSE);
    }

    /* We can remove this group, if it is not the primary group of any
     * remaining user. i.e. scan through pwent and see if it is still
     * being used as primary group by any user.
     */
    if (gum_file_find_user_by_gid (self->priv->group->gr_gid,
            gum_config_get_string (self->priv->config,
                    GUM_CONFIG_GENERAL_PASSWD_FILE)) != NULL) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_HAS_USER,
                "Group is a primary group of an existing user", error, FALSE);
    }

    const gchar *scrip_dir = GROUPDEL_SCRIPT_DIR;
#   ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("UM_GROUPDEL_DIR");
    if (env_val)
        scrip_dir = env_val;
#   endif

    gum_utils_run_group_scripts (scrip_dir, self->priv->group->gr_name,
            self->priv->group->gr_gid);

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_DELETE,
            (GumFileUpdateCB)_update_daemon_group_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GROUP_FILE), NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    shadow_file = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (g_file_test (shadow_file, G_FILE_TEST_EXISTS) &&
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_DELETE,
                (GumFileUpdateCB)_update_gshadow_entry, shadow_file, NULL,
                error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_group_update (
        GumdDaemonGroup *self,
        GError **error)
{
    struct group *grp = NULL;
    struct sgrp *gshadow = NULL;
    gchar *old_name = NULL;
    const gchar *shadow_file = NULL;

    DBG ("");

    /* Only secret can be updated */
    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if (self->priv->group->gr_gid == GUM_GROUP_INVALID_GID) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NOT_FOUND, "Group gid invalid",
                error, FALSE);
    }

    if ((grp = _get_group (self, error)) == NULL) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    gshadow = gum_file_getsgnam (grp->gr_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_GSHADOW_FILE));

    if (!self->priv->group->gr_passwd ||
        g_strcmp0 (self->priv->group->gr_passwd, "x") == 0 ||
        (gshadow && gum_crypt_cmp_secret (self->priv->group->gr_passwd,
                gshadow->sg_passwd) == 0)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_NO_CHANGES,
                "No changes registered", error, FALSE);
    }

    /* gshadow entry */
    _copy_gshadow_struct (gshadow, self->priv->gshadow, TRUE);

    /* group entry */
    if (!self->priv->group->gr_name)
        _set_groupname_property (self, grp->gr_name);
    if (!self->priv->group->gr_mem)
        GUM_STR_DUPV (grp->gr_mem, self->priv->group->gr_mem);

    if (!_set_secret (self, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    GUM_STR_DUP (grp->gr_name, old_name);
    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_update_daemon_group_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GROUP_FILE), old_name, error)) {
        g_free (old_name);
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    shadow_file = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (g_file_test (shadow_file, G_FILE_TEST_EXISTS) &&
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                (GumFileUpdateCB)_update_gshadow_entry,
                shadow_file, old_name, error)) {
        g_free (old_name);
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    g_free (old_name);

    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_group_add_member (
        GumdDaemonGroup *self,
        uid_t uid,
        gboolean add_as_admin,
        GError **error)
{
    struct passwd *pent = NULL;
    struct group *grp = NULL;
    struct sgrp *gshadow = NULL;
    const gchar *shadow_file = NULL;

    DBG ("");

    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if ((pent = gum_file_getpwuid (uid, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE))) == NULL) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found", error,
                FALSE);
    }

    if ((grp = _get_group (self, error)) == NULL) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (gum_string_utils_search_stringv (grp->gr_mem, pent->pw_name)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_USER_ALREADY_A_MEMBER,
                "User already a member of the group", error, FALSE);
    }
    shadow_file = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);

    gshadow = gum_file_getsgnam (grp->gr_name, shadow_file);

    if (!_copy_group_data (self, grp, gshadow, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    /* add user to group and gshadow */
    GUM_STR_FREEV (self->priv->group->gr_mem);
    self->priv->group->gr_mem = gum_string_utils_append_string (grp->gr_mem,
            pent->pw_name);

    GUM_STR_DUPV (self->priv->group->gr_mem, self->priv->gshadow->sg_mem);

    if (add_as_admin &&
        gshadow &&
        !gum_string_utils_search_stringv (gshadow->sg_adm, pent->pw_name)) {
        GUM_STR_FREEV (self->priv->gshadow->sg_adm);
        self->priv->gshadow->sg_adm = gum_string_utils_append_string (
                gshadow->sg_adm, pent->pw_name);
    }

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_update_daemon_group_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GROUP_FILE), NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (gshadow &&
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                (GumFileUpdateCB)_update_gshadow_entry, shadow_file,
                NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_group_delete_member (
        GumdDaemonGroup *self,
        uid_t uid,
        GError **error)
{
    struct passwd *pent = NULL;
    struct group *grp = NULL;
    struct sgrp *gshadow = NULL;
    const gchar *shadow_file = NULL;

    DBG ("");

    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    if ((pent = gum_file_getpwuid (uid, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_PASSWD_FILE))) == NULL) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND, "User not found", error,
                FALSE);
    }

    if ((grp = _get_group (self, error)) == NULL) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    if (!gum_string_utils_search_stringv (grp->gr_mem, pent->pw_name)) {
        gum_lock_pwdf_unlock ();
        GUM_RETURN_WITH_ERROR (GUM_ERROR_USER_NOT_FOUND,
                "User not a member of the group", error, FALSE);
    }

    gshadow = gum_file_getsgnam (grp->gr_name, gum_config_get_string (
            self->priv->config, GUM_CONFIG_GENERAL_GSHADOW_FILE));

    if (!_copy_group_data (self, grp, gshadow, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    /* delete user from group and gshadow */
    GUM_STR_FREEV (self->priv->group->gr_mem);
    self->priv->group->gr_mem = gum_string_utils_delete_string (grp->gr_mem,
            pent->pw_name);

    GUM_STR_DUPV (self->priv->group->gr_mem, self->priv->gshadow->sg_mem);

    /* delete user from adm group in gshadow */
    if (gshadow &&
        gum_string_utils_search_stringv (gshadow->sg_adm, pent->pw_name)) {
        GUM_STR_FREEV (self->priv->gshadow->sg_adm);
        self->priv->gshadow->sg_adm = gum_string_utils_delete_string (
                gshadow->sg_adm, pent->pw_name);
    }

    if (!gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
            (GumFileUpdateCB)_update_daemon_group_entry,
            gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GROUP_FILE), NULL, error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    shadow_file = gum_config_get_string (self->priv->config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (g_file_test (shadow_file, G_FILE_TEST_EXISTS) &&
        !gum_file_update (G_OBJECT (self), GUM_OPTYPE_MODIFY,
                (GumFileUpdateCB)_update_gshadow_entry, shadow_file, NULL,
                error)) {
        gum_lock_pwdf_unlock ();
        return FALSE;
    }

    gum_lock_pwdf_unlock ();
    return TRUE;
}

gboolean
gumd_daemon_group_delete_user_membership (
        GumConfig *config,
        const gchar *user_name,
        GError **error)
{
    gboolean retval = TRUE;
    FILE *source_file = NULL, *dup_file = NULL;
    gchar *dup_file_path = NULL;
    const gchar *source_file_path = NULL;

    if (!config || !user_name) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_GROUP_INVALID_DATA,
                "Invalid input data", error, FALSE);
    }

    if (!gum_validate_name (user_name, error)) {
        return FALSE;
    }

    if (!gum_lock_pwdf_lock ()) {
        GUM_RETURN_WITH_ERROR (GUM_ERROR_DB_ALREADY_LOCKED,
                "Database already locked", error, FALSE);
    }

    /* update group entries */
    source_file_path = gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GROUP_FILE);
    dup_file_path = g_strdup_printf ("%s-tmp.%lu", source_file_path,
            (unsigned long)getpid ());

    retval = gum_file_open_db_files (source_file_path, dup_file_path,
            &source_file, &dup_file, error);
    if (!retval) goto _finished;

    struct group *gent = NULL;
    while ((gent = fgetgrent (source_file)) != NULL) {

        if (gum_string_utils_search_stringv (gent->gr_mem, user_name)) {
            gint status = 0;
            struct group *gdest = g_malloc0 (sizeof (struct group));
            _copy_group_struct (gent, gdest);
            GUM_STR_FREEV (gdest->gr_mem);
            gdest->gr_mem = gum_string_utils_delete_string (gent->gr_mem,
                    user_name);
            status = putgrent (gdest, dup_file);
            _free_daemon_group_entry (gdest);
            if (status < 0) {
                GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                        error, retval, FALSE);
                break;
            }

        } else if (putgrent (gent, dup_file) < 0) {
            GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, retval, FALSE);
            break;
        }
    }
    if (!retval) {
        fclose (dup_file);
        g_unlink (dup_file_path);
        fclose (source_file);
    } else {
        retval = gum_file_close_db_files (source_file_path, dup_file_path,
                source_file, dup_file, error);
    }
    dup_file = NULL; source_file = NULL;
    g_free (dup_file_path); dup_file_path = NULL;

    /* update gshadow entries */
    source_file_path = gum_config_get_string (config,
            GUM_CONFIG_GENERAL_GSHADOW_FILE);
    if (!g_file_test (source_file_path, G_FILE_TEST_EXISTS))  goto _finished;

    dup_file_path = g_strdup_printf ("%s-tmp.%lu", source_file_path,
            (unsigned long)getpid ());

    retval = gum_file_open_db_files (source_file_path, dup_file_path,
            &source_file, &dup_file, error);
    if (!retval) goto _finished;

    struct sgrp *gsent = NULL;
    while ((gsent = fgetsgent (source_file)) != NULL) {

        gboolean is_mem = gum_string_utils_search_stringv (gsent->sg_mem,
                user_name);
        gboolean is_adm = gum_string_utils_search_stringv (gsent->sg_adm,
                user_name);
        if (is_mem || is_adm) {

            gint status = 0;
            struct sgrp *gsdest = g_malloc0 (sizeof (struct sgrp));
            _copy_gshadow_struct (gsent, gsdest, FALSE);
            if (is_mem) {
                GUM_STR_FREEV (gsdest->sg_mem);
                gsdest->sg_mem = gum_string_utils_delete_string (gsent->sg_mem,
                        user_name);
            }
            if (is_adm) {
                GUM_STR_FREEV (gsdest->sg_adm);
                gsdest->sg_adm = gum_string_utils_delete_string (gsent->sg_adm,
                        user_name);
            }

            status = putsgent (gsdest, dup_file);
            _free_gshadow_entry (gsdest);
            if (status < 0) {
                GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                        error, retval, FALSE);
                break;
            }

        } else if (putsgent (gsent, dup_file) < 0) {
            GUM_SET_ERROR (GUM_ERROR_FILE_WRITE, "File write failure",
                    error, retval, FALSE);
            break;
        }
    }

    if (!retval) {
        fclose (dup_file);
        g_unlink (dup_file_path);
        fclose (source_file);
    } else {
        retval = gum_file_close_db_files (source_file_path, dup_file_path,
                source_file, dup_file, error);
    }

_finished:
    g_free (dup_file_path);

    gum_lock_pwdf_unlock ();
    return retval;
}

gid_t
gumd_daemon_group_get_gid_by_name (
        const gchar *groupname,
        GumConfig *config)
{
    gid_t gid = GUM_GROUP_INVALID_GID;
    if (groupname) {
        if (!gum_lock_pwdf_lock ()) {
            return gid;
        }
        struct group *grp = gum_file_getgrnam (groupname,
                gum_config_get_string (config, GUM_CONFIG_GENERAL_GROUP_FILE));
        gum_lock_pwdf_unlock ();
        if (grp) {
            return grp->gr_gid;
        }
    }
    return gid;
}
