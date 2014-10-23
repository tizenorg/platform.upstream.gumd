SUBDIRS = core dbus
NULL=

bin_PROGRAMS = gumd

gumd_SOURCES = \
    main.c \
    $(NULL)

gumd_CFLAGS = \
    $(GUMD_INCLUDES) \
    $(GUMD_CFLAGS) \
    -I$(top_srcdir)/src \
    $(NULL)

gumd_LDADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(top_builddir)/src/daemon/core/libgumd-core.la \
    $(top_builddir)/src/daemon/dbus/libgumd-dbus.la \
    $(GUMD_LIBS) \
    $(NULL)

EXTRA_DIST = \
      gumd.conf.in

gumdconfdir = ${sysconfdir}/gumd
gumdconf_DATA = gumd.conf

if SET_PERMISSIONS
install-exec-hook:
	chown root $(DESTDIR)$(bindir)/gumd || true
endif

CLEANFILES = *.gcno *.gcda