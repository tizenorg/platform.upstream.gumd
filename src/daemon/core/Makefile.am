lib_LTLIBRARIES = libgumd-core.la
NULL=

libgumd_core_la_CFLAGS = \
    $(GUMD_CORE_INCLUDES) \
    $(GUMD_CORE_CFLAGS) \
    -I$(top_srcdir)/src \
    -DUSERADD_SCRIPT_DIR='"${sysconfdir}/gumd/useradd.d"' \
    -DUSERDEL_SCRIPT_DIR='"${sysconfdir}/gumd/userdel.d"' \
    -DGROUPADD_SCRIPT_DIR='"${sysconfdir}/gumd/groupadd.d"' \
    -DGROUPDEL_SCRIPT_DIR='"${sysconfdir}/gumd/groupdel.d"' \
    $(NULL)

libgumd_core_la_LIBADD = \
   $(GUMD_CORE_LIBS) \
   $(top_builddir)/src/common/libgum-common.la \
   $(NULL)

libgumd_core_la_SOURCES = \
   gumd-daemon.c \
   gumd-daemon.h \
   gumd-daemon-user.c \
   gumd-daemon-user.h \
   gumd-daemon-group.c \
   gumd-daemon-group.h \
   gumd-types.h \
	$(NULL)

dist_libgumd_core_la_SOURCES = \
    $(NULL)

CLEANFILES = *.gcno *.gcda
