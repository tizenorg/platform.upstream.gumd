/*
 * Generated by gdbus-codegen 2.32.3. DO NOT EDIT.
 *
 * The license of this code is the same as for the source it was derived from.
 */

#ifndef __GUM_DBUS_USER_GEN_H__
#define __GUM_DBUS_USER_GEN_H__

#include <gio/gio.h>

G_BEGIN_DECLS


/* ------------------------------------------------------------------------ */
/* Declarations for org.tizen.SecurityAccounts.gUserManagement.User */

#define GUM_DBUS_TYPE_USER (gum_dbus_user_get_type ())
#define GUM_DBUS_USER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GUM_DBUS_TYPE_USER, GumDbusUser))
#define GUM_DBUS_IS_USER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GUM_DBUS_TYPE_USER))
#define GUM_DBUS_USER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), GUM_DBUS_TYPE_USER, GumDbusUserIface))

struct _GumDbusUser;
typedef struct _GumDbusUser GumDbusUser;
typedef struct _GumDbusUserIface GumDbusUserIface;

struct _GumDbusUserIface
{
  GTypeInterface parent_iface;



  gboolean (*handle_add_user) (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation);

  gboolean (*handle_delete_user) (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation,
    gboolean arg_rem_home_dir);

  gboolean (*handle_update_user) (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation);

  guint  (*get_gid) (GumDbusUser *object);

  const gchar * (*get_homedir) (GumDbusUser *object);

  const gchar * (*get_homephone) (GumDbusUser *object);

  const gchar * (*get_nickname) (GumDbusUser *object);

  const gchar * (*get_office) (GumDbusUser *object);

  const gchar * (*get_officephone) (GumDbusUser *object);

  const gchar * (*get_realname) (GumDbusUser *object);

  const gchar * (*get_secret) (GumDbusUser *object);

  const gchar * (*get_shell) (GumDbusUser *object);

  guint  (*get_uid) (GumDbusUser *object);

  const gchar * (*get_username) (GumDbusUser *object);

  guint16  (*get_usertype) (GumDbusUser *object);

  void (*unregistered) (
    GumDbusUser *object);

};

GType gum_dbus_user_get_type (void) G_GNUC_CONST;

GDBusInterfaceInfo *gum_dbus_user_interface_info (void);
guint gum_dbus_user_override_properties (GObjectClass *klass, guint property_id_begin);


/* D-Bus method call completion functions: */
void gum_dbus_user_complete_add_user (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation,
    guint uid);

void gum_dbus_user_complete_delete_user (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation);

void gum_dbus_user_complete_update_user (
    GumDbusUser *object,
    GDBusMethodInvocation *invocation);



/* D-Bus signal emissions functions: */
void gum_dbus_user_emit_unregistered (
    GumDbusUser *object);



/* D-Bus method calls: */
void gum_dbus_user_call_add_user (
    GumDbusUser *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gum_dbus_user_call_add_user_finish (
    GumDbusUser *proxy,
    guint *out_uid,
    GAsyncResult *res,
    GError **error);

gboolean gum_dbus_user_call_add_user_sync (
    GumDbusUser *proxy,
    guint *out_uid,
    GCancellable *cancellable,
    GError **error);

void gum_dbus_user_call_delete_user (
    GumDbusUser *proxy,
    gboolean arg_rem_home_dir,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gum_dbus_user_call_delete_user_finish (
    GumDbusUser *proxy,
    GAsyncResult *res,
    GError **error);

gboolean gum_dbus_user_call_delete_user_sync (
    GumDbusUser *proxy,
    gboolean arg_rem_home_dir,
    GCancellable *cancellable,
    GError **error);

void gum_dbus_user_call_update_user (
    GumDbusUser *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gum_dbus_user_call_update_user_finish (
    GumDbusUser *proxy,
    GAsyncResult *res,
    GError **error);

gboolean gum_dbus_user_call_update_user_sync (
    GumDbusUser *proxy,
    GCancellable *cancellable,
    GError **error);



/* D-Bus property accessors: */
guint gum_dbus_user_get_uid (GumDbusUser *object);
void gum_dbus_user_set_uid (GumDbusUser *object, guint value);

guint gum_dbus_user_get_gid (GumDbusUser *object);
void gum_dbus_user_set_gid (GumDbusUser *object, guint value);

guint16 gum_dbus_user_get_usertype (GumDbusUser *object);
void gum_dbus_user_set_usertype (GumDbusUser *object, guint16 value);

const gchar *gum_dbus_user_get_nickname (GumDbusUser *object);
gchar *gum_dbus_user_dup_nickname (GumDbusUser *object);
void gum_dbus_user_set_nickname (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_username (GumDbusUser *object);
gchar *gum_dbus_user_dup_username (GumDbusUser *object);
void gum_dbus_user_set_username (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_secret (GumDbusUser *object);
gchar *gum_dbus_user_dup_secret (GumDbusUser *object);
void gum_dbus_user_set_secret (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_realname (GumDbusUser *object);
gchar *gum_dbus_user_dup_realname (GumDbusUser *object);
void gum_dbus_user_set_realname (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_office (GumDbusUser *object);
gchar *gum_dbus_user_dup_office (GumDbusUser *object);
void gum_dbus_user_set_office (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_officephone (GumDbusUser *object);
gchar *gum_dbus_user_dup_officephone (GumDbusUser *object);
void gum_dbus_user_set_officephone (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_homephone (GumDbusUser *object);
gchar *gum_dbus_user_dup_homephone (GumDbusUser *object);
void gum_dbus_user_set_homephone (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_homedir (GumDbusUser *object);
gchar *gum_dbus_user_dup_homedir (GumDbusUser *object);
void gum_dbus_user_set_homedir (GumDbusUser *object, const gchar *value);

const gchar *gum_dbus_user_get_shell (GumDbusUser *object);
gchar *gum_dbus_user_dup_shell (GumDbusUser *object);
void gum_dbus_user_set_shell (GumDbusUser *object, const gchar *value);


/* ---- */

#define GUM_DBUS_TYPE_USER_PROXY (gum_dbus_user_proxy_get_type ())
#define GUM_DBUS_USER_PROXY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GUM_DBUS_TYPE_USER_PROXY, GumDbusUserProxy))
#define GUM_DBUS_USER_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GUM_DBUS_TYPE_USER_PROXY, GumDbusUserProxyClass))
#define GUM_DBUS_USER_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GUM_DBUS_TYPE_USER_PROXY, GumDbusUserProxyClass))
#define GUM_DBUS_IS_USER_PROXY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GUM_DBUS_TYPE_USER_PROXY))
#define GUM_DBUS_IS_USER_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GUM_DBUS_TYPE_USER_PROXY))

typedef struct _GumDbusUserProxy GumDbusUserProxy;
typedef struct _GumDbusUserProxyClass GumDbusUserProxyClass;
typedef struct _GumDbusUserProxyPrivate GumDbusUserProxyPrivate;

struct _GumDbusUserProxy
{
  /*< private >*/
  GDBusProxy parent_instance;
  GumDbusUserProxyPrivate *priv;
};

struct _GumDbusUserProxyClass
{
  GDBusProxyClass parent_class;
};

GType gum_dbus_user_proxy_get_type (void) G_GNUC_CONST;

void gum_dbus_user_proxy_new (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GumDbusUser *gum_dbus_user_proxy_new_finish (
    GAsyncResult        *res,
    GError             **error);
GumDbusUser *gum_dbus_user_proxy_new_sync (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);

void gum_dbus_user_proxy_new_for_bus (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GumDbusUser *gum_dbus_user_proxy_new_for_bus_finish (
    GAsyncResult        *res,
    GError             **error);
GumDbusUser *gum_dbus_user_proxy_new_for_bus_sync (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);


/* ---- */

#define GUM_DBUS_TYPE_USER_SKELETON (gum_dbus_user_skeleton_get_type ())
#define GUM_DBUS_USER_SKELETON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GUM_DBUS_TYPE_USER_SKELETON, GumDbusUserSkeleton))
#define GUM_DBUS_USER_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GUM_DBUS_TYPE_USER_SKELETON, GumDbusUserSkeletonClass))
#define GUM_DBUS_USER_SKELETON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GUM_DBUS_TYPE_USER_SKELETON, GumDbusUserSkeletonClass))
#define GUM_DBUS_IS_USER_SKELETON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GUM_DBUS_TYPE_USER_SKELETON))
#define GUM_DBUS_IS_USER_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GUM_DBUS_TYPE_USER_SKELETON))

typedef struct _GumDbusUserSkeleton GumDbusUserSkeleton;
typedef struct _GumDbusUserSkeletonClass GumDbusUserSkeletonClass;
typedef struct _GumDbusUserSkeletonPrivate GumDbusUserSkeletonPrivate;

struct _GumDbusUserSkeleton
{
  /*< private >*/
  GDBusInterfaceSkeleton parent_instance;
  GumDbusUserSkeletonPrivate *priv;
};

struct _GumDbusUserSkeletonClass
{
  GDBusInterfaceSkeletonClass parent_class;
};

GType gum_dbus_user_skeleton_get_type (void) G_GNUC_CONST;

GumDbusUser *gum_dbus_user_skeleton_new (void);


G_END_DECLS

#endif /* __GUM_DBUS_USER_GEN_H__ */
