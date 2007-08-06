#!/bin/sh
#dnl echo "no" | gettextize -c -f
aclocal
autoheader
automake -a
autoconf
./configure
