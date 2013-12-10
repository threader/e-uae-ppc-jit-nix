# P-UAE
#
# 2006-2013 Mustafa TUFAN (aka GnoStiC/BRONX)
#
#
base=" --with-sdl --with-sdl-gl --with-sdl-gfx --with-sdl-sound --enable-drvsnd "
cd32=" --enable-cd32 "
a600=" --enable-gayle "
scsi=" --enable-scsi-device --enable-ncr --enable-a2091 "
other=" --with-caps --enable-amax --enable-gccopt --enable-serial-port "
libscg=" --with-libscg-prefix=/usr/local/Cellar/cdrtools/3.00/ "
#debug="--enable-profiling"
#
#
./bootstrap.sh
./configure $debug $base $cd32 $a600 $scsi $other $libscg CFLAGS="-m32" LDFLAGS="-m32" CPPFLAGS="-m32"
make clean
make
