#!/bin/sh

aclocal -I m4 \
&& automake --foreign --add-missing \
&& autoconf
