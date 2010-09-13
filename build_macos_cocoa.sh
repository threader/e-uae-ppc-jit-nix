# P-UAE
#
# 2010 Mustafa TUFAN
#

./bootstrap.sh
./configure --with-sdl --with-sdl-gl --with-sdl-gfx --with-sdl-sound --with-caps --enable-drvsnd --enable-amax --enable-cd32 --enable-scsi-device --enable-a2091 --enable-gayle --enable-ncr CFLAGS="-m32" LDFLAGS="-m32" CPPFLAGS="-m32"
make clean
make
