#Environment variables for the tests
export G_MESSAGES_DEBUG=all

make distclean;
autoreconf -fi;
./configure --enable-dbus-type=session --enable-debug && \
make -j4 && make check && make distclean && \
./configure --enable-dbus-type=system --enable-debug && \
make -j4 && make check && make distclean && \
./configure --enable-dbus-type=p2p --enable-debug && \
make -j4 && make check && make distclean;

