# P-UAE
#
# 2010 Mustafa TUFAN
#
# this script cleans up various files so that we don't push unnecessary files to git..
#

make clean

rm -rf ./df0.adz
rm -rf ./kick.rom

rm -rf ./aclocal.m4

rm -rf ./src/gfxdep
rm -rf ./src/guidep
rm -rf ./src/joydep
rm -rf ./src/machdep
rm -rf ./src/osdep
rm -rf ./src/sounddep
rm -rf ./src/threaddep
rm -rf ./src/PUAE.app

rm -rf `find . -type d -name autom4te.cache`
rm -rf `find . -type d -name .deps`
rm -rf `find . -type f -name Makefile`
rm -rf `find . -type f -name *~`
rm -rf `find . -type f -name *.o`
rm -rf `find . -type f -name *.a`
rm -rf `find . -type f -name configure`
rm -rf `find . -type f -name config.log`
rm -rf `find . -type f -name config.status`
rm -rf `find . -type f -name sysconfig.h`

rm -rf Makefile.in
rm -rf src/Makefile.in
rm -rf src/archivers/dms/Makefile.in
rm -rf src/archivers/zip/Makefile.in
rm -rf src/caps/Makefile.in
rm -rf src/gfx-amigaos/Makefile.in
rm -rf src/gfx-beos/Makefile.in
rm -rf src/gfx-curses/Makefile.in
rm -rf src/gfx-sdl/Makefile.in
rm -rf src/gfx-svga/Makefile.in
rm -rf src/gfx-x11/Makefile.in
rm -rf src/gui-beos/Makefile.in
rm -rf src/gui-cocoa/Makefile.in
rm -rf src/gui-gtk/Makefile.in
rm -rf src/gui-muirexx/Makefile.in
rm -rf src/gui-none/Makefile.in
rm -rf src/gui-qt/Makefile.in
rm -rf src/jd-amigainput/Makefile.in
rm -rf src/jd-amigaos/Makefile.in
rm -rf src/jd-beos/Makefile.in
rm -rf src/jd-linuxold/Makefile.in
rm -rf src/jd-none/Makefile.in
rm -rf src/jd-sdl/Makefile.in
rm -rf src/keymap/Makefile.in
rm -rf src/md-68k/Makefile.in
rm -rf src/md-amd64-gcc/Makefile.in
rm -rf src/md-generic/Makefile.in
rm -rf src/md-i386-gcc/Makefile.in
rm -rf src/md-ppc-gcc/Makefile.in
rm -rf src/md-ppc/Makefile.in
rm -rf src/od-amiga/Makefile.in
rm -rf src/od-beos/Makefile.in
rm -rf src/od-generic/Makefile.in
rm -rf src/od-linux/Makefile.in
rm -rf src/od-macosx/Makefile.in
rm -rf src/od-win32/Makefile.in
rm -rf src/sd-alsa/Makefile.in
rm -rf src/sd-amigaos/Makefile.in
rm -rf src/sd-beos/Makefile.in
rm -rf src/sd-none/Makefile.in
rm -rf src/sd-sdl/Makefile.in
rm -rf src/sd-solaris/Makefile.in
rm -rf src/sd-uss/Makefile.in
rm -rf src/td-amigaos/Makefile.in
rm -rf src/td-beos/Makefile.in
rm -rf src/td-none/Makefile.in
rm -rf src/td-posix/Makefile.in
rm -rf src/td-sdl/Makefile.in
rm -rf src/td-win32/Makefile.in
rm -rf src/test/Makefile.in

echo "=================================================="
echo "Current Commit: "
tail -1 .git/packed-refs | awk '{print $1}'
echo "=================================================="
