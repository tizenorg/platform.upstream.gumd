include $(top_srcdir)/test/test_common.mk

TESTS = clienttest
TESTS_ENVIRONMENT += \
    UM_BIN_DIR=$(top_builddir)/src/daemon/.libs

VALGRIND_TESTS_DISABLE=

check_PROGRAMS = clienttest
include $(top_srcdir)/test/valgrind_common.mk

clienttest_SOURCES = client-test.c

clienttest_CFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(LIBGUM_INCLUDES) \
    $(TEST_CFLAGS) \
    $(LIBGUM_CFLAGS) \
    $(CHECK_CFLAGS) \
    -U G_LOG_DOMAIN \
    -DG_LOG_DOMAIN=\"gum-test-client\"

clienttest_LDADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(top_builddir)/src/lib/libgum.la \
    $(GUMD_LIBS) \
    $(CHECK_LIBS)
