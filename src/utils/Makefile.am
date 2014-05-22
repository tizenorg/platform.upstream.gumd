## Process this file with automake to produce Makefile.in

bin_PROGRAMS = gum-utils

gum_utils_SOURCES = gum-utils.c
gum_utils_CPPFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(LIBGUM_INCLUDES) \
    $(LIBGUM_CFLAGS) \
    $(DEPS_CFLAGS)
	
gum_utils_LDADD = \
	$(DEPS_LIBS) \
	$(top_builddir)/src/lib/libgum.la \
	$(LIBGUM_LIBS)

if HAVE_LIBTLM_NFC
gum_utils_CPPFLAGS += $(LIBTLM_NFC_CFLAGS)
gum_utils_LDADD += $(LIBTLM_NFC_LIBS)
endif

CLEANFILES = *.gcno *.gcda