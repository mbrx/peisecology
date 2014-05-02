#!/bin/sh

rm -f ./ltmain.sh ./libtool
libtoolize --force 
aclocal $ACLOCAL_FLAGS || exit;
autoheader || exit;
automake --add-missing --copy;
autoconf || exit;
automake || exit
