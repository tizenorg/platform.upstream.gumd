## Process this file with automake to produce Makefile.in

bin_PROGRAMS = gum-example

gum_example_SOURCES = gum-example.c
gum_example_CPPFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(LIBGUM_INCLUDES) \
    $(LIBGUM_CFLAGS) \
    $(DEPS_CFLAGS)
	
gum_example_LDADD = \
	$(DEPS_LIBS) \
	$(top_builddir)/src/lib/libgum.la \
	$(LIBGUM_LIBS)

if HAVE_LIBTLM_NFC
gum_example_CPPFLAGS += $(LIBTLM_NFC_CFLAGS)
gum_example_LDADD += $(LIBTLM_NFC_LIBS)
endif