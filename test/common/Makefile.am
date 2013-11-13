include $(top_srcdir)/test/test_common.mk

TESTS = commontest

VALGRIND_TESTS_DISABLE=

check_PROGRAMS = commontest
include $(top_srcdir)/test/valgrind_common.mk
commontest_SOURCES = commontest.c
commontest_CFLAGS = \
    $(GUM_COMMON_INCLUDES) \
    $(TEST_CFLAGS) \
    $(GUMD_CFLAGS) \
    $(CHECK_CFLAGS) \
    -U G_LOG_DOMAIN \
    -I$(top_srcdir)/src \
    -DG_LOG_DOMAIN=\"gum-test-common\"

commontest_LDADD = \
    $(top_builddir)/src/common/libgum-common.la \
    $(GUMD_LIBS) \
    $(CHECK_LIBS)