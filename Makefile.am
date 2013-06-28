ACLOCAL_AMFLAGS = -I m4

# --enable-distcheck is here to make it possible to use
# also --enable-dbus-type=session here
AM_DISTCHECK_CONFIGURE_FLAGS = \
    --enable-debug \
    --enable-distcheck \
    --enable-tests

SUBDIRS = src test examples

EXTRA_DIST = dists tools

valgrind:
	cd test; make valgrind
