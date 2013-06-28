include $(top_srcdir)/test/test_common.mk

TESTS = daemontest
TESTS_ENVIRONMENT += \
    UM_BIN_DIR=$(top_builddir)/src/daemon/.libs

VALGRIND_TESTS_DISABLE=

check_PROGRAMS = daemontest
include $(top_srcdir)/test/valgrind_common.mk

daemontest_SOURCES = daemon-test.c

daemontest_CFLAGS = \
    -I$(top_builddir)/src \
    -I$(top_srcdir)/src \
    $(GUMD_INCLUDES) \
    $(TEST_CFLAGS) \
    $(GUMD_CFLAGS) \
    $(CHECK_CFLAGS) \
    -U G_LOG_DOMAIN \
    -DG_LOG_DOMAIN=\"gum-test-daemon\"

if USE_GTESTDBUS
daemontest_CFLAGS += \
    -DGUMD_TEST_DBUS_SERVICE_DIR=\""$(abs_top_builddir)/test/data/services"\"
endif

daemontest_LDADD = \
    $(top_builddir)/src/daemon/gumd-gumd-daemon-user.o \
    $(top_builddir)/src/daemon/gumd-gumd-daemon-group.o \
    $(top_builddir)/src/common/libgum-common.la \
    $(GUMD_LIBS) \
    $(CHECK_LIBS)

