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

