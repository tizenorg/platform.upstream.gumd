SRC_HOME="."
with_duma=0
with_gdb=0

if test $# -ge 1 ; then
    if test "$1" == "--with-duma" ; then
        with_duma=1
        with_gdb=1
    else
        with_duma=0
        if test "$1" == "--with-gdb" ; then
            with_gdb=1
        fi
    fi
fi

killall gumd

export LD_LIBRARY_PATH="$SRC_HOME/src/common/.libs:$SRC_HOME/src/daemon/.libs:$SRC_HOME/src/daemon/dbus/.libs"
export G_MESSAGES_DEBUG="all"

echo "--------------------------"
echo "with_duma:  $with_duma"
echo "with_gdb:  $with_gdb"
echo "--------------------------"
if test $with_duma -eq 1 ; then
    export G_SLICE="always-malloc"
    export DUMA_PROTECT_FREE=1
    export DUMA_PROTECT_BELOW=1

    LD_PRELOAD="libduma.so" $SRC_HOME/src/daemon/.libs/gumd  &

    if test $with_gdb -eq 1 ; then
        sudo gdb --pid=`pidof gumd`
    fi
elif test $with_gdb -eq 1 ; then
    gdb $SRC_HOME/src/daemon/.libs/gumd
else
    $SRC_HOME/src/daemon/.libs/gumd
fi

