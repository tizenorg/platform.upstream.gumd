#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([gum], [0.0.1],[],[],[https://github.com/otcshare/gum])
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

# Enable Distcheck
AC_ARG_ENABLE(distcheck, [--enable-distcheck build for distcheck target],
	      [enable_distcheck=yes], [enable_distcheck=no])
echo "--------------------------------"
echo "Distcheck : '$enable_distcheck'"
AM_CONDITIONAL(SET_PERMISSIONS, [test x$enable_distcheck != xyes])

# Enable Dbus type
AC_ARG_ENABLE(dbus-type,
          [--enable-dbus-type=dbus-type specify daemon dbus type:
           p2p(default): uses peer to peer dbus
           session: uses session message bus
           system: uses system message bus],
          [enable_dbus_type=$enableval], [enable_dbus_type=p2p])
          
echo "--------------------------------"
echo "Dbus Type : '$enable_dbus_type'"
echo "--------------------------------"
if test "x$enable_dbus_type" = "xp2p" ; then
    AC_DEFINE(GUM_BUS_TYPE_P2P, [1], [Use peer to peer dbus])
elif test "x$enable_dbus_type" = "xsession" ; then
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
fi
AC_SUBST(DBUS_SERVICES_DIR)
AC_SUBST(DBUS_INTERFACES_DIR)
AC_SUBST(DBUS_CONF_DIR)

AC_SUBST(MESSAGE_BUS_TYPE, [$enable_dbus_type])
AM_CONDITIONAL(HAVE_SYSTEM_BUS, [test x$enable_dbus_type = xsystem])

# Check for systemd
VAR_SYSTEMD="`pkg-config --variable systemdsystemunitdir systemd`"
if test "x$VAR_SYSTEMD" != "x" ; then
    AC_DEFINE(USE_SYSTEMD, [1], [Use systemd-login dbus APIs])
elif test "x$VAR_SYSTEMD" = "x" ; then
    AC_DEFINE(USE_SYSTEMD, [0], [Use systemd-login dbus APIs])
fi

# Enable tests
AC_ARG_ENABLE(tests, [--enable-tests  enable tests features],
          [enable_tests=yes], [enable_tests=no])
echo "--------------------------------"
echo "Enable tests features: '$enable_tests'"
if test "x$enable_tests" = "xyes" ; then
    AC_DEFINE(ENABLE_TESTS, [1], [Enable tests features])
fi
AM_CONDITIONAL(HAVE_TESTS, [test x$enable_tests = xyes])

# Enable Debug
AC_ARG_ENABLE(debug, [--enable-debug  enable debug features],
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

# Gtk-doc
GTK_DOC_CHECK([1.18],[--flavour no-tmpl])

# Checks for typedefs, structures, and compiler characteristics.
GUM_CFLAGS='$(GLIB_CFLAGS) -D_POSIX_C_SOURCE=\"200809L\" -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE'
GUM_CFLAGS="$GUM_CFLAGS -Wall -Werror"
AC_SUBST(GUM_CFLAGS)
GUM_INCLUDES='-I$(top_builddir) -I$(top_srcdir)'
AC_SUBST(GUM_INCLUDES)

# Gum common library cflags, libs, includes
GUM_COMMON_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gum-common\"'
AC_SUBST(GUM_COMMON_CFLAGS)
GUM_COMMON_LIBS='$(GLIB_LIBS) $(CRYPT_LIB)'
AC_SUBST(GUM_COMMON_LIBS)
GUM_COMMON_INCLUDES='$(GUM_INCLUDES) -I$(top_srcdir)/include/gum'
AC_SUBST(GUM_COMMON_INCLUDES)

# Gum daemon cflags, libs, includes
GUMD_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gumd\"'
AC_SUBST(GUMD_CFLAGS)
GUMD_LIBS='$(GLIB_LIBS)'
AC_SUBST(GUMD_LIBS)
GUMD_INCLUDES='$(GUM_COMMON_INCLUDES)'
AC_SUBST(GUMD_INCLUDES)

# Gum client library cflags, libs, includes
LIBGUM_CFLAGS='$(GUM_CFLAGS) -DG_LOG_DOMAIN=\"gum\"'
AC_SUBST(LIBGUM_CFLAGS)
LIBGUM_LIBS='$(GLIB_LIBS)'
AC_SUBST(LIBGUM_LIBS)
LIBGUM_INCLUDES='$(GUM_COMMON_INCLUDES)'
AC_SUBST(LIBGUM_INCLUDES)

AC_CONFIG_FILES([
Makefile
src/Makefile
src/common/Makefile
src/common/gum.conf
src/common/libgum-common.pc
src/common/libgum-common-uninstalled.pc
src/common/dbus/Makefile
src/daemon/gumd.pc
src/daemon/gumd-uninstalled.pc
src/daemon/Makefile
src/daemon/dbus/Makefile
src/daemon/dbus/services/org.tizen.SecurityAccounts.gUserManagement.service
src/daemon/dbus/gumd-dbus.conf
src/lib/Makefile
src/lib/libgum.pc
src/lib/libgum-uninstalled.pc
docs/Makefile
docs/version.xml
test/Makefile
test/common/Makefile
test/daemon/Makefile
test/lib/Makefile
examples/Makefile
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
