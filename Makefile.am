ACLOCAL_AMFLAGS = -I m4

# configure flags used in by distcheck
AM_DISTCHECK_CONFIGURE_FLAGS = \
    --enable-distcheck \
    --enable-dbus-type=p2p\
    --enable-tests\
    --enable-gtk-doc

if ENABLE_GTK_DOC
AM_DISTCHECK_CONFIGURE_FLAGS += \
    --enable-gtk-doc
endif

SUBDIRS = src test examples docs

EXTRA_DIST = dists tools

valgrind:
	cd test; make valgrind

lcov: check
	@rm -rf lcov-report
	@lcov -c --directory src/ --output-file lcov.output
	@genhtml lcov.output --output-directory lcov-report
	@rm lcov.output
	@echo "Coverage report is in file://$(abs_srcdir)/lcov-report/index.html"
