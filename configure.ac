#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([gumd], [1.0.7],[],[],[https://github.com/01org/gumd])
AC_CONFIG_SRCDIR([src/daemon/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 nostdinc silent-rules subdir-objects tar-pax -Wno-portability])

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AX_CHECK_GNU_MAKE
if test "x$ifGNUmake" = "x#" ; then
	AC_MSG_ERROR("GNUmake is required")
fi

#libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], 
                  [glib-2.0 >= 2.30
                   gio-2.0
                   gio-unix-2.0
                   gmodule-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

# Checks for libtlm-nfc
PKG_CHECK_MODULES([LIBTLM_NFC], [libtlm-nfc], [have_libtlm_nfc=yes], [have_libtlm_nfc=no])
AM_CONDITIONAL(HAVE_LIBTLM_NFC, [test x$have_libtlm_nfc = xyes])
if test "x$have_libtlm_nfc" = xyes; then
    AC_DEFINE(HAVE_LIBTLM_NFC, [1], [Define if libtlm-nfc exists.])
fi

# Check library.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [have_check=yes], [have_check=no])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/xattr.h attr/xattr.h],[break])
AC_CHECK_FUNCS(llistxattr lgetxattr lsetxattr)

# Crypt library.
AC_SEARCH_LIBS([crypt],[crypt], CRYPT_LIB="-l$ac_lib", CRYPT_LIB="")
AC_SUBST(CRYPT_LIB)
if test "x$CRYPT_LIB" = "x" ; then
    AC_MSG_ERROR("CRYPT Library is required")
fi

# Enable cov
AC_ARG_ENABLE(cov, [  --enable-cov build to be used for coverage analysis ],
          [enable_cov=yes], [enable_cov=no])
if test "x$enable_cov" = "xyes" ; then
    GCOV_CFLAGS="-ftest-coverage -fprofile-arcs"
    GCOV_LIBS="-lgcov"
fi
AC_SUBST(GCOV_CFLAGS)
AC_SUBST(GCOV_LIBS)

# Enable Distcheck
AC_ARG_ENABLE(distcheck, [  --enable-distcheck build for distcheck target],
	      [enable_distcheck=yes], [enable_distcheck=no])
echo "--------------------------------"
echo "Distcheck : '$enable_distcheck'"
AM_CONDITIONAL(SET_PERMISSIONS, [test x$enable_distcheck != xyes])

# Enable Dbus type
AC_ARG_ENABLE(dbus-type,
          [  --enable-dbus-type=dbus-type specify daemon dbus type:
           p2p: uses peer to peer dbus
           session: uses session message bus
           system(default): uses system message bus],
          [enable_dbus_type=$enableval], [enable_dbus_type=system])
          
echo "--------------------------------"
echo "Dbus Type : '$enable_dbus_type'"
echo "--------------------------------"
if test "x$enable_dbus_type" = "xp2p" ; then
    AC_DEFINE(GUM_BUS_TYPE_P2P, [1], [Use peer to peer dbus])
elif test "x$enable_dbus_type" = "xsession" ; then
	PKG_CHECK_MODULES([DBus], [dbus-1])
    AC_DEFINE(GUM_BUS_TYPE, [G_BUS_TYPE_SESSION], [Use session bus])
    if test "x$enable_distcheck" = "xyes" ; then
        DBUS_SERVICES_DIR="${datadir}/dbus-1/services"
        DBUS_INTERFACES_DIR="${datadir}/dbus-1/interfaces"
        DBUS_CONF_DIR="${datadir}/dbus-1/session.d"
    else
        DBUS_SERVICES_DIR="`pkg-config --variable session_bus_services_dir dbus-1`"
        DBUS_INTERFACES_DIR="`pkg-config --variable interfaces_dir dbus-1`"
        DBUS_CONF_DIR="`pkg-config --variable sysconfdir dbus-1`/dbus-1/session.d"
    fi
elif test "x$enable_dbus_type" = "xsystem" ; then
    PKG_CHECK_MODULES([DBus], [dbus-1])
    AC_DEFINE(GUM_BUS_TYPE, [G_BUS_TYPE_SYSTEM], [Use system bus])
    if test "x$enable_distcheck" = "xyes" ; then
        DBUS_SERVICES_DIR="${datadir}/dbus-1/system-services"
        DBUS_INTERFACES_DIR="${datadir}/dbus-1/interfaces"
        DBUS_CONF_DIR="${datadir}/dbus-1/system.d"
    else
        DBUS_SERVICES_DIR="`pkg-config --variable system_bus_services_dir dbus-1`"
        DBUS_INTERFACES_DIR="`pkg-config --variable interfaces_dir dbus-1`"
        DBUS_CONF_DIR="`pkg-config --variable sysconfdir dbus-1`/dbus-1/system.d"
    fi
    AC_SUBST(MESSAGE_BUS_USER, ["User=root"])
fi
AC_SUBST(DBUS_SERVICES_DIR)
AC_SUBST(DBUS_INTERFACES_DIR)
AC_SUBST(DBUS_CONF_DIR)

AC_SUBST(MESSAGE_BUS_TYPE, [$enable_dbus_type])
AM_CONDITIONAL(HAVE_SYSTEM_BUS, [test x$enable_dbus_type = xsystem])
AM_CONDITIONAL(USE_DBUS_SERVICE, [test x$enable_dbus_type != xp2p])

# Check for systemd
VAR_SYSTEMD="`pkg-config --variable systemdsystemunitdir systemd`"
if test "x$VAR_SYSTEMD" != "x" ; then
    AC_DEFINE(USE_SYSTEMD, [1], [Use systemd-login dbus APIs])
elif test "x$VAR_SYSTEMD" = "x" ; then
    AC_DEFINE(USE_SYSTEMD, [0], [Use systemd-login dbus APIs])
fi

# Enable tests
AC_ARG_ENABLE(tests, [  --enable-tests  enable tests features],
          [enable_tests=yes], [enable_tests=no])
if test "x$enable_cov" = "xyes" ; then
    # Enable tests automatically when code coverage is enabled
    enable_tests="yes"
fi
echo "--------------------------------"
echo "Enable tests features: '$enable_tests'"
if test "x$enable_tests" = "xyes" ; then
    AC_DEFINE(ENABLE_TESTS, [1], [Enable tests features])
fi
AM_CONDITIONAL(HAVE_TESTS, [test x$enable_tests = xyes])

# Enable Debug
AC_ARG_ENABLE(debug, [  --enable-debug  enable debug features],
	      [enable_debug=yes], [enable_debug=no])
if test "x$enable_tests" = "xyes" ; then
    # Enable debug automatically when tests are enabled
    enable_debug="yes"
fi
echo "--------------------------------"
echo "Enable Debug Feature: '$enable_debug'"
echo "--------------------------------"
if test "x$enable_debug" = "xyes" ; then
    AC_DEFINE(ENABLE_DEBUG, [1], [Enable debug features])
fi
AM_CONDITIONAL(HAVE_DEBUG, [test x$enable_debug = xyes])

# passwd file
AC_ARG_ENABLE(passwdfile,
	      [  --enable-passwdfile=path  enable passwd file at location "path"
	       instead of default "/etc/passwd"], 
	      [enable_passwdfile=$enableval],
	      [enable_passwdfile="/etc/passwd"])
AC_DEFINE_UNQUOTED(GUM_PASSWD_FILE, ["$enable_passwdfile"],
		 [Path for passwd file])

# shadow file
AC_ARG_ENABLE(shadowfile,
	      [  --enable-shadowfile=path  enable shadow file at location "path"
	       instead of default "/etc/shadow"], 
	      [enable_shadowfile=$enableval],
	      [enable_shadowfile="/etc/shadow"])
AC_DEFINE_UNQUOTED(GUM_SHADOW_FILE, ["$enable_shadowfile"],
		 [Path for shadow file])

# group file
AC_ARG_ENABLE(groupfile,
	      [  --enable-groupfile=path  enable group file at location "path"
	       instead of default "/etc/group"], 
	      [enable_groupfile=$enableval],
	      [enable_groupfile="/etc/group"])
AC_DEFINE_UNQUOTED(GUM_GROUP_FILE, ["$enable_groupfile"],
		 [Path for group file])

# gshadow file
AC_ARG_ENABLE(gshadowfile,
	      [  --enable-gshadowfile=path  enable gshadow file at location "path"
	       instead of default "/etc/gshadow"], 
	      [enable_gshadowfile=$enableval],
	      [enable_gshadowfile="/etc/gshadow"])
AC_DEFINE_UNQUOTED(GUM_GSHADOW_FILE, ["$enable_gshadowfile"],
		 [Path for gshadow file])

# home dir prefix
AC_ARG_ENABLE(homedirpref,
	      [  --enable-homedirpref=path  enable home directory prefix at "path"
	       instead of default "/home"], 
	      [enable_homedirpref=$enableval],
	      [enable_homedirpref="/home"])
AC_DEFINE_UNQUOTED(GUM_HOME_DIR_PREFIX, ["$enable_homedirpref"],
		 [Path for home dir prefix])

# shell path
AC_ARG_ENABLE(shell,
	      [  --enable-shell=path  enable shell at location "path"
	       instead of default "/bin/bash"], 
	      [enable_shell=$enableval],
	      [enable_shell="/bin/bash"])
AC_DEFINE_UNQUOTED(GUM_SHELL, ["$enable_shell"],
		 [Path for shell])

# defult user groups
AC_ARG_ENABLE(defusergroups,
	      [  --enable-defusergroups=groups  enable default user groups by
	       "groups" instead of default ""],
	      [enable_defusergroups=$enableval],
	      [enable_defusergroups=""])
AC_DEFINE_UNQUOTED(GUM_DEF_GROUPS, ["$enable_defusergroups"],
		 [Default user groups to be assigned to user on creation])

# defult admin groups
AC_ARG_ENABLE(defadmingroups,
	      [  --enable-defadmingroups=groups  enable default groups for admin
	       user by "groups" instead of default ""],
	      [enable_defadmingroups=$enableval],
	      [enable_defadmingroups=""])
AC_DEFINE_UNQUOTED(GUM_DEF_ADMIN_GROUPS, ["$enable_defadmingroups"],
		 [Default groups to be assigned to the admin user on creation])
		 
# skeldir
AC_ARG_ENABLE(skeldir,
	      [  --enable-skeldir=path  enable skel dir at location "path" instead
	       of default "/etc/skel"], 
	      [enable_skeldir=$enableval],
	      [enable_skeldir="/etc/skel"])
AC_DEFINE_UNQUOTED(GUM_SKEL_DIR, ["$enable_skeldir"], [Path for skel directory])

# encryption algorithm
AC_ARG_ENABLE(encryptalgo,
	      [  --enable-encryptalgo=algo  enable encrypt algorithm as specified
	      by "algo" instead of default "SHA512". Other supported algorithms are
	      'MD5', 'SHA256', 'DES'], 
	      [enable_encryptalgo=$enableval],
	      [enable_encryptalgo="SHA512"])
AC_DEFINE_UNQUOTED(GUM_ENCRYPT_METHOD, ["$enable_encryptalgo"], [Encrypt
		 algorithm])

# Gtk-doc
GTK_DOC_CHECK([1.18],[--flavour no-tmpl])

# Checks for typedefs, structures, and compiler characteristics.
GUM_CFLAGS='$(GLIB_CFLAGS) -D_POSIX_C_SOURCE=\"200809L\" -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE'
GUM_CFLAGS="$GUM_CFLAGS -Wall $GCOV_CFLAGS"
if test "x$enable_debug" = "xno" ; then
    GUM_CFLAGS="$GUM_CFLAGS -Werror"
else
    GUM_CFLAGS="$GUM_CFLAGS -DENABLE_DEBUG"
fi


AC_SUBST(GUM_CFLAGS)
GUM_INCLUDES='-I$(top_builddir) -I$(top_srcdir)'
AC_SUBST(GUM_INCLUDES)

# Gum common library cflags, libs, includes
GUM_COMMON_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gum-common\"'
AC_SUBST(GUM_COMMON_CFLAGS)
GUM_COMMON_LIBS='$(GLIB_LIBS) $(CRYPT_LIB) $(GCOV_LIBS)'
AC_SUBST(GUM_COMMON_LIBS)
GUM_COMMON_INCLUDES='$(GUM_INCLUDES) -I$(top_srcdir)/include/gum'
AC_SUBST(GUM_COMMON_INCLUDES)

# Gumd core lib cflags, libs, includes
GUMD_CORE_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gumd-core\"'
AC_SUBST(GUMD_CORE_CFLAGS)
GUMD_CORE_LIBS='$(GLIB_LIBS) $(GCOV_LIBS)'
AC_SUBST(GUMD_CORE_LIBS)
GUMD_CORE_INCLUDES='$(GUM_COMMON_INCLUDES)'
AC_SUBST(GUMD_CORE_INCLUDES)

# Gum daemon cflags, libs, includes
GUMD_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gumd\"'
AC_SUBST(GUMD_CFLAGS)
GUMD_LIBS='$(GLIB_LIBS) $(GCOV_LIBS)'
AC_SUBST(GUMD_LIBS)
GUMD_INCLUDES='$(GUM_COMMON_INCLUDES)'
AC_SUBST(GUMD_INCLUDES)

# Gum client library cflags, libs, includes
LIBGUM_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gum\"'
AC_SUBST(LIBGUM_CFLAGS)
LIBGUM_LIBS='$(GLIB_LIBS) $(GCOV_LIBS)'
AC_SUBST(LIBGUM_LIBS)
LIBGUM_INCLUDES='$(GUM_COMMON_INCLUDES)'
AC_SUBST(LIBGUM_INCLUDES)

AC_CONFIG_FILES([
Makefile
src/Makefile
src/common/Makefile
src/common/dbus/Makefile
src/daemon/Makefile
src/daemon/core/Makefile
src/daemon/dbus/Makefile
src/daemon/dbus/services/org.tizen.SecurityAccounts.gUserManagement.service
src/daemon/dbus/gumd-dbus.conf
src/lib/Makefile
src/lib/libgum.pc
src/lib/libgum-uninstalled.pc
src/utils/Makefile
data/Makefile
data/gumd.conf
docs/Makefile
docs/version.xml
test/Makefile
test/common/Makefile
test/daemon/Makefile
test/lib/Makefile
])

if test "x$enable_tests" = "xyes" ; then
    AC_CONFIG_FILES([
    test/data/test-gumd-dbus.conf
    ])
    
    if test x$enable_dbus_type != xp2p; then
        AC_CONFIG_FILES([
        test/data/services/org.tizen.SecurityAccounts.gUserManagement.service
        ])
    fi
fi

AC_OUTPUT
