SUBDIRS =
NULL=

lib_LTLIBRARIES = libgum.la

libgum_la_CFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(LIBGUM_INCLUDES) \
    $(LIBGUM_CFLAGS) \
    $(NULL)

libgum_la_DEPENDENCIES = \
    $(NULL)

libgum_la_LIBADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(top_builddir)/src/daemon/core/libgumd-core.la \
    $(LIBGUM_LIBS) \
    $(NULL)

gum_pubhdr = $(top_srcdir)/include/gum
libgum_includedir = $(includedir)/gum
libgum_include_HEADERS = \
    $(gum_pubhdr)/gum-user.h \
    $(gum_pubhdr)/gum-user-service.h \
    $(gum_pubhdr)/gum-group.h \
    $(NULL)
    
libgum_la_SOURCES = \
    gum-internals.h \
    gum-user-service.c \
    gum-user.c \
    gum-group-service.h \
    gum-group-service.c \
    gum-group.c \
    $(NULL)

dist_libgum_la_SOURCES = \
    $(NULL)

EXTRA_DIST = \
      libgum.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgum.pc

CLEANFILES = *.gcno *.gcda

