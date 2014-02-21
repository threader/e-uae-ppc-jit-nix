#!/bin/sh
# P-UAE
#
# 2006-2011 Mustafa TUFAN (aka GnoStiC/BRONX)
#
#
#
base=" --with-sdl --with-sdl-gfx --with-alsa --enable-drvsnd --with-sdl-gui"
cd32=" --enable-cd32 "
a600=" --enable-gayle "
scsi=" --enable-scsi-device --enable-ncr --enable-a2091 "
other=" --with-caps --enable-amax --enable-gccopt --enable-serial-port "
debug=""
#
#
./bootstrap.sh
./configure $debug $base $cd32 $a600 $scsi $other
make -j9
