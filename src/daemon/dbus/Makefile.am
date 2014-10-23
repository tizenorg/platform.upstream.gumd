SERVICES_DIR = $(top_builddir)/src/daemon/dbus/services
NULL=

noinst_LTLIBRARIES = libgumd-dbus.la

libgumd_dbus_la_CPPFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(GUMD_INCLUDES) \
    $(GUMD_CFLAGS) \
    $(NULL)

libgumd_dbus_la_LIBADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(top_builddir)/src/daemon/core/libgumd-core.la \
    $(GUMD_LIBS) \
    $(NULL)

libgumd_dbus_la_SOURCES = \
   gumd-dbus-server-interface.c \
   gumd-dbus-server-interface.h \
   gumd-dbus-server-msg-bus.c \
   gumd-dbus-server-msg-bus.h \
   gumd-dbus-server-p2p.c \
   gumd-dbus-server-p2p.h \
   gumd-dbus-user-service-adapter.c \
   gumd-dbus-user-service-adapter.h \
   gumd-dbus-user-adapter.c \
   gumd-dbus-user-adapter.h \
   gumd-dbus-group-service-adapter.c \
   gumd-dbus-group-service-adapter.h \
   gumd-dbus-group-adapter.c \
   gumd-dbus-group-adapter.h \
   $(NULL)

if USE_DBUS_SERVICE
dbusservicedir = $(DBUS_SERVICES_DIR)
dbusservice_DATA = \
    $(SERVICES_DIR)/org.tizen.SecurityAccounts.gUserManagement.service
nodist_dbusservice_DATA = \
    $(SERVICES_DIR)/org.tizen.SecurityAccounts.gUserManagement.service
endif

EXTRA_DIST = \
      gumd-dbus.conf.in

if HAVE_SYSTEM_BUS
dbusconfdir = $(DBUS_CONF_DIR)
dbusconf_DATA = \
    gumd-dbus.conf
endif

clean-local :
	rm -f *~

