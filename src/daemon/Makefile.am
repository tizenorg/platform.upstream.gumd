SUBDIRS = dbus
NULL=

bin_PROGRAMS = gumd

gumd_SOURCES = \
    main.c \
    gumd-daemon.c \
    gumd-daemon.h \
    gumd-daemon-user.c \
    gumd-daemon-user.h \
    gumd-daemon-group.c \
    gumd-daemon-group.h \
    gumd-types.h \
    $(NULL)

gumd_CFLAGS = \
    $(GUMD_INCLUDES) \
    $(GUMD_CFLAGS) \
    -I$(top_srcdir)/src \
    $(NULL)

gumd_LDADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(top_builddir)/src/daemon/dbus/libgumd-dbus.la \
    $(GUMD_LIBS) \
    $(NULL)

EXTRA_DIST = \
      gumd.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gumd.pc

if SET_PERMISSIONS
install-exec-hook:
	chown root $(DESTDIR)$(bindir)/gumd || true
	chmod u+s $(DESTDIR)$(bindir)/gumd || true
endif

CLEANFILES = *.gcno *.gcda