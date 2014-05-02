#!/bin/sh
aclocal $ACLOCAL_FLAGS || exit;
libtoolize --force || exit;
autoheader || exit;
automake --add-missing --copy;
autoconf || exit;
automake || exit
