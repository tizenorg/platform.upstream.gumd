SUBDIRS = dbus
NULL=

lib_LTLIBRARIES = libgum-common.la

libgum_common_la_CPPFLAGS = \
    -DGUM_SYSCONF_DIR='"$(sysconfdir)"' \
    $(GUM_COMMON_INCLUDES) \
    $(GUM_COMMON_CFLAGS) \
    -I$(top_srcdir)/src \
    $(NULL)

libgum_common_la_DEPENDENCIES = \
    $(NULL)

libgum_common_la_LIBADD = \
    $(top_builddir)/src/common/dbus/libgum-dbus-glue.la \
    $(GUM_COMMON_LIBS) \
    $(NULL)

gum_common_pubhdr = $(top_srcdir)/include/gum/common
libgum_common_includedir = $(includedir)/gum/common
libgum_common_include_HEADERS = \
    $(gum_common_pubhdr)/gum-config-dbus.h \
    $(gum_common_pubhdr)/gum-config-general.h \
    $(gum_common_pubhdr)/gum-config.h \
    $(gum_common_pubhdr)/gum-dictionary.h \
    $(gum_common_pubhdr)/gum-error.h \
    $(gum_common_pubhdr)/gum-log.h \
    $(gum_common_pubhdr)/gum-disposable.h \
    $(gum_common_pubhdr)/gum-crypt.h \
    $(gum_common_pubhdr)/gum-lock.h \
    $(gum_common_pubhdr)/gum-file.h \
    $(gum_common_pubhdr)/gum-string-utils.h \
    $(gum_common_pubhdr)/gum-utils.h \
    $(gum_common_pubhdr)/gum-validate.h \
    $(gum_common_pubhdr)/gum-user-types.h \
    $(gum_common_pubhdr)/gum-group-types.h \
    $(NULL)
    
libgum_common_la_SOURCES = \
    gum-config.c \
    gum-error.c \
    gum-dictionary.c \
    gum-disposable.c \
    gum-crypt.c \
    gum-lock.c \
    gum-file.c \
    gum-string-utils.c \
    gum-utils.c \
    gum-validate.c \
    $(NULL)

dist_libgum_common_la_SOURCES = \
    $(NULL)

EXTRA_DIST =     \
      gum-dbus.h \
      gum-defines.h

CLEANFILES = *.gcno *.gcda

