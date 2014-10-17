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
    gumd-daemon-offline.c \
    gumd-daemon-offline.h \
    gumd-types.h \
    $(NULL)

gumd_CFLAGS = \
    $(GUMD_INCLUDES) \
    $(GUMD_CFLAGS) \
    -I$(top_srcdir)/src \
    -DUSERADD_SCRIPT_DIR='"${sysconfdir}/gumd/useradd.d"' \
    -DUSERDEL_SCRIPT_DIR='"${sysconfdir}/gumd/userdel.d"' \
    -DGROUPADD_SCRIPT_DIR='"${sysconfdir}/gumd/groupadd.d"' \
    -DGROUPDEL_SCRIPT_DIR='"${sysconfdir}/gumd/groupdel.d"' \
    $(NULL)

gumd_LDADD = \
    $(top_builddir)/src/common/libgum-common.la \
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