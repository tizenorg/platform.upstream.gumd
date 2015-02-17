#!/bin/sh -e

# This file is part of gum
#
# Copyright (C) 2013 Intel Corporation.
#
# Contact: Imran Zaman <imran.zaman@intel.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA


srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

if which gtkdocize >/dev/null 2>/dev/null; then
        gtkdocize --docdir docs/
else
        rm -f docs/gtk-doc.make
        echo 'EXTRA_DIST =' > docs/gtk-doc.make
fi

aclocal #-I m4 
autoheader
libtoolize --copy --force
autoconf
automake --add-missing --copy

autoreconf --force --install --symlink

./configure "$@"

