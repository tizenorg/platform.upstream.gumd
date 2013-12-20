TESTS_ENVIRONMENT = \
	G_MESSAGES_DEBUG="all" \
	LD_LIBRARY_PATH="$(top_builddir)/src/common/.libs:$(top_builddir)/src/daemon/.libs:$(top_builddir)/src/daemon/dbus/.libs"

TEST_CFLAGS = -DGUM_TEST_DATA_DIR=\"$(abs_top_srcdir)/test/data/\"