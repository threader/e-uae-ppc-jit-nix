dnl Process this file with autoconf to produce a configure script.
dnl Written 1996, 1997, 1998 Bernd Schmidt
dnl
dnl Updated, re-written and generally mauled 2003-2008 by Richard Drummond
dnl Modified for JIT compiling implementation for PowerPC by Almos Rajnai

AC_PREREQ(2.55)

m4_define(uae_version_major, 1)
m4_define(uae_version_minor, 0)
m4_define(uae_version_revision, 0)

AC_INIT([E-UAE], uae_version_major.uae_version_minor.uae_version_revision, ,[e-uae])
AC_CONFIG_SRCDIR([bootstrap.sh])
AC_CONFIG_HEADER([src/sysconfig.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(1.7 dist-bzip2 foreign)

AC_ARG_WITH(version-tag,
  AS_HELP_STRING([--with-version-tag], [Optional tag to append to version number]),
  [UAE_VERSION_TAG=$withval],[])

UAE_VERSION_MAJOR=uae_version_major()
UAE_VERSION_MINOR=uae_version_minor()
UAE_VERSION_REVISION=uae_version_revision()

if test -n "$UAE_VERSION_TAG"; then
  VERSION=$VERSION-$UAE_VERSION_TAG
fi

AC_DEFINE_UNQUOTED([UAE_VERSION_MAJOR],    [$UAE_VERSION_MAJOR],    [Define to the UAE major version number])
AC_DEFINE_UNQUOTED([UAE_VERSION_MINOR],    [$UAE_VERSION_MINOR],    [Define to the UAE minor version number])
AC_DEFINE_UNQUOTED([UAE_VERSION_REVISION], [$UAE_VERSION_REVISION], [Define to the UAE revision number])
AC_DEFINE_UNQUOTED([UAE_VERSION_TAG],      ["$UAE_VERSION_TAG"],    [Define to the optional UAE version tag])
AC_DEFINE_UNQUOTED([UAE_VERSION],          ["$VERSION"],            [Define to the UAE version])

dnl
dnl  Checks for programs.
dnl

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_OBJC
AS_PROG_OBJC
AM_PROG_AS
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_CHECK_TOOL([AR], [ar], [])
AC_PROG_INSTALL
AC_PATH_PROG(MAKEDEPPRG, makedepend, not-found)
AC_PATH_PROG(FILEPRG, file, not-found)
AC_PATH_PROG(WRCPRG, wrc, not-found)
AC_PATH_PROG(RCLPRG, rcl, not-found)

AC_AIX
AC_ISC_POSIX

NR_WARNINGS=0
NR_ERRORS=0

ADDITIONAL_CFLAGS=
OPTIMIZE_CFLAGS=
WARNING_CFLAGS=
WARNING_CXXFLAGS=
DEBUG_CFLAGS=
NO_SCHED_CFLAGS=

dnl
dnl  Checks for libraries.
dnl
dnl Replace main' with a function in -lMedia_s: (Ian!)
dnl AC_CHECK_LIB(Media_s, main, HAVE_MEDIA_LIB=yes, HAVE_MEDIA_LIB=no)
dnl Replace main' with a function in -lNeXT_s:
dnl AC_CHECK_LIB(NeXT_s, main, HAVE_NEXT_LIB=yes, HAVE_NEXT_LIB=no)
dnl AC_CHECK_LIB(moto, cos, HAVE_MOTO_LIB=yes, HAVE_MOTO_LIB=no)
dnl AC_CHECK_LIB(amiga, OpenLibrary, HAVE_AMIGA_LIB=yes, HAVE_AMIGA_LIB=n)
dnl AC_CHECK_LIB(vga, vga_setmode, HAVE_SVGA_LIB=yes, HAVE_SVGA_LIB=no)

AC_CHECK_LIB(ossaudio,_oss_ioctl, HAVE_LIBOSSAUDIO=yes, HAVE_LIBOSSAUDIO=no)

dnl  Prefer Gtk2.x over Gtk1.x if both are available
AM_PATH_GTK_2_0(2.0.0,HAVE_GTK=yes,HAVE_GTK=no)
if [[ "x$HAVE_GTK" = "xno" ]]; then
  AM_PATH_GTK(1.0.0,HAVE_GTK=yes,HAVE_GTK=no)
fi

dnl
dnl  Checks for header files.
dnl
AC_HEADER_DIRENT
AC_HEADER_STDC

AC_CHECK_HEADERS([fcntl.h sys/ioctl.h sys/time.h utime.h])
AC_CHECK_HEADERS([values.h ncurses.h curses.h sys/termios.h])
AC_CHECK_HEADERS([sys/stat.h sys/ipc.h sys/shm.h sys/mman.h])
AC_CHECK_HEADERS([sys/filio.h])

AC_CHECK_HEADERS([libraries/cybergraphics.h cybergraphx/cybergraphics.h])

AC_CHECK_HEADERS([devices/ahi.h])
AC_CHECK_HEADERS([sys/soundcard.h machine/soundcard.h sun/audioio.h sys/audioio.h])

AC_CHECK_HEADERS([machine/joystick.h])
AC_CHECK_HEADER([amigainput/amigainput.h], HAVE_AMIGAINPUT=yes, HAVE_AMIGAINPUT=no)

AC_CHECK_HEADERS([byteswap.h])

dnl
dnl  Checks for typedefs, structures, and compiler characteristics.
dnl
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLOCKS
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE
AC_C_BIGENDIAN

AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(__int64, 8)
AC_CHECK_SIZEOF(void *)

if test "${ac_cv_c_have_long_double}" = "yes" ; then
	AC_CHECK_SIZEOF(long double, 12)
fi

TYPE_SOCKLEN_T

dnl
dnl  Checks for library functions.
dnl
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS(gettimeofday sigaction)
AC_CHECK_FUNCS(select strerror isnan isinf setitimer alarm sync)
AC_CHECK_FUNCS(readdir_r)
AC_CHECK_FUNCS(strdup strstr strcasecmp stricmp strcmpi)
AC_CHECK_FUNCS(nanosleep usleep sleep)
AC_CHECK_FUNCS(vprintf vsprintf vfprintf)

dnl AC_CHECK_FUNCS(statvfs statfs)
dnl AC_VAR_TIMEZONE_EXTERNALS - broken on *BSD
AC_CHECK_FUNCS(localtime_r timegm gmtime_r)
AC_SYS_LARGEFILE

AC_MSG_CHECKING(for bswap_16)
AC_TRY_LINK([
  #if HAVE_BYTESWAP_H
  # include <byteswap.h>
  #endif
],[
  bswap_16 (0x12);
],[
  AC_DEFINE(HAVE_BSWAP_16, 1, [Define to 1 if you have the 'bswap_16' function.])
  AC_MSG_RESULT(yes)
],[
  AC_MSG_RESULT(no)
])
AC_MSG_CHECKING(for bswap_32)
AC_TRY_LINK([
  #if HAVE_BYTESWAP_H
  # include <byteswap.h>
  #endif
],[
  bswap_32 (0x1234);
],[
  AC_DEFINE(HAVE_BSWAP_32, 1, [Define to 1 if you have the 'bswap_32' function.])
  AC_MSG_RESULT(yes)
],[
  AC_MSG_RESULT(no)
])

dnl
dnl  Checks for fsusage
dnl
AC_CHECK_HEADERS(sys/param.h sys/vfs.h sys/fs_types.h)
AC_CHECK_HEADERS(sys/mount.h, [], [],
[#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])
jm_FILE_SYSTEM_USAGE([gl_cv_fs_space=yes], [gl_cv_fs_space=no])
if test $gl_cv_fs_space = yes; then
  AC_LIBOBJ(fsusage)
  gl_PREREQ_FSUSAGE_EXTRA
fi

AC_CACHE_SAVE

dnl
dnl  Check for libz
dnl
AC_CHECK_LIB(z, inflateEnd, [zlib_cv_libz=yes], [zlib_cv_libz=no])
AC_CHECK_HEADER(zlib.h, [zlib_cv_zlib_h=yes], [zlib_cv_zlib_h=no])
if test "$zlib_cv_libz" = "yes" -a "$zlib_cv_zlib_h" = "yes"
then
    UAE_LIBS="-lz $UAE_LIBS"
else
    AC_MSG_ERROR(Check for libz failed)
fi

dnl
dnl  Check for pthreads
dnl
ACX_PTHREAD(HAVE_PTHREAD=yes,HAVE_PTHREAD=no)

dnl Solaris, for example, needs additional libs to use POSIX semaphores.
if [[ "x$HAVE_PTHREAD" = "xyes" ]]; then
 SAVE_CFLAGS="$CFLAGS"
 SAVE_LIBS="$LIBS"
 CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
 LIBS="$PTHREAD_LIBS"
 AC_SEARCH_LIBS(sem_init, rt posix4,,,"$SAVE_LIBS")
 PTHREAD_LIBS="$LIBS"
 CFLAGS="$SAVE_CFLAGS"
 LIBS="$SAVE_LIBS"
fi

dnl
dnl  Check for dlopen
dnl
AC_CHECK_LIB(dl, dlopen, [
    UAE_LIBS="-ldl $UAE_LIBS"
    AC_DEFINE(HAVE_DLOPEN, 1, "Define to 1 if you have 'dlopen' function)
  ],
)

dnl
dnl  Check availability of OSS
dnl
HAVE_USS_SOUND=no
if [[ "x$ac_cv_header_sys_soundcard_h" = "xyes" -o "x$ac_cv_header_machine_soundcard_h" = "xyes" ]]; then
  if [[ "x$HAVE_LIBOSSAUDIO" = "xyes" ]]; then
    SAVE_LIBS="$LIBS"
    LIBS="$LIBS -lossaudio"
  fi
  dnl Avoid surprises
  AC_MSG_CHECKING(whether sys/soundcard.h or machine/soundcard.h works)
  AC_TRY_LINK([
#include "confdefs.h"
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#include <machine/soundcard.h>
#endif
#include <sys/ioctl.h>
],
[int soundfd; ioctl (soundfd, SNDCTL_DSP_GETFMTS, 0);],
AC_MSG_RESULT(yes)
HAVE_USS_SOUND=yes,
AC_MSG_RESULT(no))
  if [[ "x$HAVE_LIBOSSAUDIO" = "xyes" ]]; then
    LIBS="$SAVE_LIBS"
  fi
fi


dnl
dnl  Find out which platform to build for
dnl
AC_MSG_CHECKING([host OS support])

dnl default is generic Unix-like OS
HOST_OS=generic
OSDEP=od-generic

case $host_os in
    linux*)
      HOST_OS=linux
      OSDEP=od-linux
      if [[ "$HOST_CPU = 'ppc'" ]]; then 

      if [[ "`cat /proc/cpuinfo | grep -o -m1 440EP`" = '440EP' ]]; then
      AC_DEFINE(AMCC_CPU, 440ep, [AMCC PPC CPU])
      elif [[ "`cat /proc/cpuinfo | grep -o -m1 460EX`" = '460EX' ]]; then
      AC_DEFINE(AMCC_CPU, 460ex, [AMCC PPC CPU]) 
      fi

      cp confdefs.h src/include/config.h
      fi
      ;;
    darwin[0-7]*)
      HOST_OS=darwin
      OSDEP=od-macosx
      AC_DEFINE_UNQUOTED(darwin0, $host_os, [Darwin 0-7])
      cp confdefs.h src/include/config.h
      ;;
    darwin8*)
      HOST_OS=darwin
      OSDEP=od-macosx
      AC_DEFINE_UNQUOTED(darwin8, $host_os, [Darwin 8])   
      cp confdefs.h src/include/config.h
      ;;
    darwin9*)
      HOST_OS=darwin
      OSDEP=od-macosx
      AC_DEFINE_UNQUOTED(darwin9, $host_os, [Darwin 9])
      cp confdefs.h src/include/config.h
      ;;
    darwin*)
      HOST_OS=darwin
      OSDEP=od-macosx
      ;;
    beos)
      HOST_OS=beos
      OSDEP=od-beos
      ;;
    amigaos)
      HOST_OS=amiga
      OSDEP=od-amiga
      ;;
    morphos)
      HOST_OS=morphos
      OSDEP=od-amiga
      ;;
    aros)
      HOST_OS=aros
      OSDEP=od-amiga
      ;;
    mingw32*)
      HOST_OS=win32
      OSDEP=od-win32
esac

AC_MSG_RESULT([$HOST_OS])

dnl
dnl  Find out what CPU arch to build for
dnl
AC_MSG_CHECKING([host cpu support])

dnl default is to use no CPU-specific features
HOST_CPU=generic
MDFPP_H=fpp-unknown.h

dnl quick hack for arm
have_armcpu=no

case $host_cpu in
     i[[3-7]]86)
       HOST_CPU=i386
       MDFPP_H=fpp-ieee.h
       ;;
     amd64 | x86_64)
       HOST_CPU=amd64
       MDFPP_H=fpp-ieee.h
       ;;
     powerpc*)
       HOST_CPU=ppc
       MDFPP_H=fpp-ieee.h
       ;;
     powerpc64*)
       HOST_CPU=ppc64
       MDFPP_H=fpp-ieee.h
       ;;
     m68k | m680[246]0)
       HOST_CPU=68k
       MDFPP_H=fpp-ieee.h
       ;;
     arm*)
       HOST_CPU=generic
       MDFPP_H=fpp-unknown.h
       have_armcpu=yes
       ;;
esac

AC_MSG_RESULT([$HOST_CPU])
MACHDEP="md-$HOST_CPU"


dnl
dnl  MULTIPLICATION_PROFITABLE?
dnl
dnl  We always set this for now. It's only used in the audio emulation,
dnl  where the option is to do multiplication or table look-up. On just
dnl  about any CPU it's worthwhile running E-UAE on, multiplication will be
dnl  better.

AC_DEFINE(MULTIPLICATION_PROFITABLE, 1, [Define to 1 if your CPU profitably supports multiplication.])


dnl
dnl  Find out what compiler we have
dnl
HAVE_GCC27=no
HAVE_GCC30=no
HAVE_GCC40=no
HAVE_GCC46=no
HAVE_GCC47=no
HAVE_GCC48=no
HAVE_GLIBC2=no

AC_MSG_CHECKING([for GCC 2.7 or higher])
AC_EGREP_CPP(yes,
[#if (__GNUC__ - 1 > 0 && __GNUC_MINOR__ - 1 > 5) || __GNUC__ - 1 > 1
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC27=yes], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for GCC 3.0 or higher])
AC_EGREP_CPP(yes,
[#if __GNUC__ - 1 > 1
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC30=yes], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for GCC 4.0 or higher])
AC_EGREP_CPP(yes,
[#if __GNUC__ - 1 > 2
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC40=yes], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for GCC 4.6 or higher])
AC_EGREP_CPP(yes,
[#if (__GNUC__ > 3 && __GNUC_MINOR__ > 5)
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC46=yes], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for GCC 4.7 or higher])
AC_EGREP_CPP(yes,
[#if (__GNUC__ > 3 && __GNUC_MINOR__ > 6)
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC47=yes], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for GCC 4.8 or higher])
AC_EGREP_CPP(yes,
[#if (__GNUC__ > 3 && __GNUC_MINOR__ > 7)
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC48=yes], AC_MSG_RESULT(no))

if test $ac_cv_prog_gcc = yes; then
  if test $HAVE_GCC27 != yes; then
    AC_MSG_WARN([Version 2.7 or better of GCC is recommended])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
  fi
else
  if test $uae_cv_prog_cc_watcom != yes; then
    AC_MSG_WARN([UAE may not compile and run correctly with your compiler])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
  fi
fi

if test $ac_cv_prog_gcc = yes; then
  if [[ "$HOST_CPU" = i386 -o "$HOST_CPU" = ppc -o "$HOST_CPU" = amd64 ]]; then
    MACHDEP="$MACHDEP-gcc"
  fi
fi

dnl
dnl  Now we know the compiler, check some compiler options
dnl
if [[ "x$HAVEGCC27" = "xyes" -a "$HOST_CPU" = "i386" ]]; then
  SAVECFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -mno-schedule-prologue"
  AC_TRY_COMPILE([],[int main(){return 0;}], [OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -mno-schedule-prologue"])
  CFLAGS=$SAVECFLAGS
fi
if [[ "x$HAVEGCC27" = "xyes" -a "$HOST_CPU" = "i386" ]]; then
  SAVECFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -mpreferred-stack-boundary=2"
  AC_TRY_COMPILE([],[int main(){return 0;}], [OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -mpreferred-stack-boundary=2"])
  CFLAGS=$SAVECFLAGS
fi

dnl
dnl If GCC supports exceptions, we don't want them.
dnl
if [[ "x$HAVEGCC27" = "xyes" -a "$HOST_CPU" = "i386" ]]; then
  SAVECFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -fno-exceptions"
  AC_TRY_COMPILE(,int main(){return 0;}, OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -fno-exceptions")
  CFLAGS=$SAVECFLAGS
fi

dnl on OS X, we don't want position-independent code, so
dnl include the option -mdynamic-no-pic.
dnl
if [[ "$HOST_OS" = "darwin" ]]; then
  OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -mdynamic-no-pic"
fi

if [[ "x$ac_cv_header_features_h" = "xyes" ]]; then
  AC_MSG_CHECKING(for glibc-2.0 or higher)
  AC_EGREP_CPP(yes,
[#include <features.h>
#if __GLIBC__ - 1 >= 1
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_GLIBC2=yes], AC_MSG_RESULT(no))
fi


dnl
dnl  Generic default targets
dnl
TARGET=x11
TARGETDEP=t-unix.h
if [[ "$HAVE_PTHREAD" = "yes" ]]; then
  THREADDEP=td-posix
  THREADNAME=posix
else
  THREADDEP=td-none
  THREADNAME=none
fi
if [[ "x$no_x" != "xyes" ]]; then
  GFX_DEP=gfx-x11
  GFX_NAME=x11
else
  GFX_DEP=
  GXF_NAME=
fi
GUI_DEP=gui-none
GUI_NAME=none
GUI_LIBS=
SND_DEP=sd-none
SND_NAME=none
JOY_DEP=jd-none
JOY_NAME=none

DO_PROFILING=no
if [[ "$HOST_CPU" = "ppc" ]]; then
  WANT_JIT=yes
else
  WANT_JIT=no
fi
WANT_DGA=no
WANT_VIDMODE=no
WANT_THREADS=dunno
NEED_THREAD_SUPPORT=no
WANT_AUTOCONFIG=dunno
WANT_SCSIEMU=no
WANT_AGA=yes
WANT_CD32=dunno
WANT_CDTV=dunno
WANT_BSDSOCK=dunno
WANT_UI=dunno
WANT_AUDIO=dunno
WANT_FPU=yes
WANT_COMPATIBLE=yes
WANT_CYCLEEXACT=yes
WANT_CAPS=no
WANT_FDI=yes
WANT_ENFORCER=dunno

MATHLIB=-lm

dnl
dnl  Override defaults for specific targets
dnl
if [[ "$HOST_OS" = "linux" ]]; then
  JOY_DEP=jd-linuxold
  JOY_NAME="linux"
else if [[ "$OSDEP" = "od-beos" ]]; then
	TARGET=beos
	TARGETDEP=t-beos.h
	THREADDEP=td-beos
	THREADNAME="native BeOS"

	GFX_DEP=gfx-beos
	GFX_NAME="native BeOS"
	GFX_LIBS="-lgame"

	GUI_DEP="gui-beos"
	GUI_NAME="native BeOS"
	GUI_LIBS="-lbe -ltracker"

	JOY_DEP=jd-beos
	JOY_NAME="native BeOS"
	JOY_LIBS="-ldevice"

	SND_DEP=sd-beos
	SND_NAME="native BeOS"
	SND_LIBS="-lmedia"

	WANT_BSDSOCK=no
	MATHLIB=""
	UAE_RSRCFILE="$OSDEP/uae.rsrc"
else if [[ "$OSDEP" = "od-amiga" ]]; then
	TARGET=amiga
	TARGETDEP=t-amiga.h
	THREADDEP=td-amigaos
	THREADNAME="native AmigaOS"

	GFX_DEP=gfx-amigaos
	GFX_NAME="native AmigaOS"

	GUI_DEP=gui-muirexx
	GUI_NAME="MUIRexx"

	SND_DEP=sd-amigaos
	SND_NAME="native AmigaOS/AHI"

	if [[ "x$HAVE_AMIGAINPUT" = "xyes" ]]; then
		JOY_DEP=jd-amigainput
		JOY_NAME="AmigaInput"
	else
		JOY_DEP=jd-amigaos
		JOY_NAME="Amiga lowlevel.library"
	fi
	WANT_BSDSOCK=no
else if [[ "$OSDEP" = "od-win32" ]]; then
  TARGET=win32
  TARGETDEP=t-win32.h
  WANT_BSDSOCK=no
fi
fi
fi
fi

dnl
dnl Win32 build has its own writelog.c
dnl
if [[ "$OSDEP" != "od-win32" ]]; then
   EXTRAOBJS='writelog.$(OBJEXT)'
fi



dnl
dnl  Options
dnl

AC_ARG_ENABLE(profiling,       AS_HELP_STRING([--enable-profiling],       [Build a profiling (SLOW!) version]),                       [DO_PROFILING=$enableval],[])
AC_ARG_ENABLE(aga,             AS_HELP_STRING([--enable-aga],             [Enable AGA chipset emulation (default yes)]),              [WANT_AGA=$enableval],[])
AC_ARG_ENABLE(cdtv,            AS_HELP_STRING([--enable-cdtv],            [Enable CDTV emulation (default no)]),                      [WANT_CDTV=$enableval],[])
AC_ARG_ENABLE(cd32,            AS_HELP_STRING([--enable-cd32],            [Enable CD32 emulation (default no)]),                      [WANT_CD32=$enableval],[])
AC_ARG_ENABLE(cycle-exact-cpu, AS_HELP_STRING([--enable-cycle-exact-cpu], [Enable cycle-exact CPU emulation (default yes)]),          [WANT_CYCLEEXACT=$enableval],[])
AC_ARG_ENABLE(compatible-cpu,  AS_HELP_STRING([--enable-compatible-cpu],  [Enable compatible CPU emulation (default yes)]),           [WANT_COMPATIBLE=$enableval],[])
AC_ARG_ENABLE(optimization,    AS_HELP_STRING([--disable-optimization],   [Disable local CPU specific optimazions (default no)]),     [WANT_CPUOPTI=$enableval],[])
AC_ARG_ENABLE(fpu,             AS_HELP_STRING([--enable-fpu],             [Enable FPU emulation (default yes)]),                      [WANT_FPU=$enableval],[])
AC_ARG_ENABLE(jit,             AS_HELP_STRING([--enable-jit],             [Enable JIT compiler (currently PPC only)]),                [WANT_JIT=$enableval],[])
AC_ARG_ENABLE(threads,         AS_HELP_STRING([--enable-threads],         [Enable thread support (default auto)]),                    [WANT_THREADS=$enableval],[])
AC_ARG_ENABLE(autoconfig,      AS_HELP_STRING([--enable-autoconfig],      [Enable emulaton of autoconfig devices (default auto)]),    [WANT_AUTOCONFIG=$enableval],[])
AC_ARG_ENABLE(scsi-device,     AS_HELP_STRING([--enable-scsi-device],     [Enable emulaton of SCSI devices (default no)]),            [WANT_SCSIEMU=$enableval],[])
AC_ARG_ENABLE(bsdsock,         AS_HELP_STRING([--enable-bsdsock],         [Enable bsdsocket.library emulation]),                      [WANT_BSDSOCK=$enableval],[])
AC_ARG_ENABLE(debugger,        AS_HELP_STRING([--disable-debugger],       [Disable internal debugger/monitor (default no)]),          [WANT_DEBUGGER=$enableval],[])
AC_ARG_ENABLE(state-saving,    AS_HELP_STRING([--disable-state-saving],   [Disable support for saving state snapshots (default no)]),   [WANT_STATESAVING=$enableval],[])
AC_ARG_ENABLE(enforcer,	       AS_HELP_STRING([--enable-enforcer],        [Enable ersatz Enforcer support (default auto)]),           [WANT_ENFORCER=$enableval],[])
AC_ARG_ENABLE(action-replay,   AS_HELP_STRING([--enable-action-replay],   [Enable Action Replay cartridge emulation (default yes)]),  [WANT_ACTION_REPLAY=$enableval],[])
AC_ARG_ENABLE(xarcade,         AS_HELP_STRING([--disable-xarcade],        [Disable keymaps for X-Aracde joysticks (default: enabled)]),  [WANT_XARCADE=$enableval],[WANT_XARCADE=yes])
AC_ARG_ENABLE(dga,             AS_HELP_STRING([--enable-dga],             [X11 version: Use the DGA extension]),                      [WANT_DGA=$enableval],[])
AC_ARG_ENABLE(vidmode,         AS_HELP_STRING([--enable-vidmode],         [X11 version: Use the XF86VidMode extension]),              [WANT_VIDMODE=$enableval],[])
AC_ARG_ENABLE(ui,              AS_HELP_STRING([--enable-ui],              [Use a user interface if possible (default on)]),           [WANT_UI=$enableval],[])
AC_ARG_ENABLE(audio,	       AS_HELP_STRING([--enable-audio],           [Enable audio output (default auto)]),                      [WANT_AUDIO=$enableval],[])
AC_ARG_ENABLE(fdi,	       AS_HELP_STRING([--enable-fdi],             [Enable FDI support (default yes)]),                        [WANT_FDI=$enableval],[])

AC_ARG_WITH(sdl,
  AS_HELP_STRING([--with-sdl], [Use SDL library for low-level functions]),
  [WANT_SDL=$withval], [])
AC_ARG_WITH(sdl-sound,
  AS_HELP_STRING([--with-sdl-sound], [Use SDL library for sound]),
  [WANT_SDLSND=$withval], [])
AC_ARG_WITH(sdl-gfx,
  AS_HELP_STRING([--with-sdl-gfx], [Use SDL library for graphics]),
  [WANT_SDLGFX=$withval], [])
AC_ARG_WITH(sdl-gl,
  AS_HELP_STRING([--with-sdl-gl], [Allow GL for 2D acceleration with SDL graphics]),
  [WANT_SDLGL=$withval], [])

AC_ARG_WITH(curses,
  AS_HELP_STRING([--with-curses], [Use ncurses library for graphics]),
  [WANT_NCURSES=$withval], [])

AC_ARG_WITH(cocoa-gui,
  AS_HELP_STRING([--with-cocoa-gui], [Use Cocoa for GUI on OS X]),
  [WANT_COCOA_UI=$withval], [])

AC_ARG_WITH(libscg-prefix,
  AS_HELP_STRING([--with-libscg-prefix], [Absolute path to where libscg is installed (optional)]),
  [LIBSCG_PREFIX=$withval], [])
AC_ARG_WITH(libscg-includedir,
  AS_HELP_STRING([--with-libscg-includedir], [Absolute path to libscg headers are installed (default LIBSCG_PREFIX/include)]),
  [LIBSCG_INCLUDEDIR=$withval], [])
AC_ARG_WITH(libscg-libdir,
  AS_HELP_STRING([--with-libscg-libdir], [Absolute path to libscg libs are installed (default LIBSCG_PREFIX/lib)]),
  [LIBSCG_LIBDIR=$withval], [])

AC_ARG_WITH(alsa,
  AS_HELP_STRING([--with-alsa], [Use ALSA library for sound]),
  [WANT_ALSA=$withval],[])

dnl
dnl Some simple plausibility tests...
dnl
AC_MSG_CHECKING([configuration options])

if [[ "x$WANT_SDLGL" = "xyes" ]]; then
  WANT_SDLGFX=yes
fi

if [[ "x$WANT_SDLSND" = "xyes" -o "x$WANT_SDLGFX" = "xyes" ]]; then
  WANT_SDL=yes
fi
if [[ "x$WANT_SDL" = "xno" ]]; then
  WANT_SDLGFX=no
  WANT_SDLSND=no
  WANT_SDLGL=no
fi

if [[ "x$WANT_DGA" = "xyes" -a "x$WANT_SDLGFX" = "xyes" ]]; then
    AC_MSG_WARN([DGA support cannot be enabled for non-X11 targets!])
    NR_ERRORS=`expr $NR_ERRORS + 1`
    WANT_DGA=no
fi

if [[ "x$WANT_DGA" = "xyes" -a "x$no_x" = "xyes" ]]; then
  AC_MSG_WARN([Ignoring --enable-dga, since X was disabled or not found])
  NR_ERRORS=`expr $NR_ERRORS + 1`
  WANT_DGA=no
fi

if [[ "x$WANT_DGA" = "xno" -a "x$WANT_VIDMODE" = "xyes" ]]; then
  AC_MSG_WARN([The XF86VidMode extension can only be used in DGA mode. Disabling it])
  NR_ERRORS=`expr $NR_ERRORS + 1`
  WANT_VIDMODE=no
fi

AC_MSG_RESULT(ok)


dnl Check for X
AC_PATH_XTRA

dnl Check for ncurses
AC_CHECK_LIB(ncurses, waddch, HAVE_NCURSES_LIB=yes, HAVE_NCURSES_LIB=no)

dnl
dnl Check for SDL
dnl
if [[ "x$WANT_SDL" != "xno" ]]; then

  if [[ "$HOST_OS" = "darwin" ]]; then

if test -d "/Library/Frameworks/SDL.framework/Headers" ; then
    dnl Assume this is MacOS X and try to use SDL framework

USE_SDL_FRAMEWORK=TRUE
    SDL_CFLAGS="-I/Library/Frameworks/SDL.framework/Headers -D_REENTRANT"
    SDL_LIBS="-framework Cocoa -framework SDL -lobjc"

elif test -e "/usr/local/lib/libSDL.dylib" ; then
    dnl Assume this is MacOS X and try to use /usr/local/lib/libSDL

USE_SDL_FRAMEWORK=FALSE
    SDL_CFLAGS="-I/usr/local/include/SDL -I/usr/include/SDL -D_GNU_SOURCE=1 -D_THREAD_SAFE -DHAVE_OPENGL"
    SDL_LIBS="-L/usr/local/lib -L/usr/lib -lSDLmain -lSDL -Wl,-framework,Cocoa"
fi

    AC_MSG_CHECKING(for SDL framework)

    SAVE_CPPFLAGS=$CPPFLAGS
    SAVE_LIBS=$LIBS
    CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
    LIBS="$SDL_LIBS $LIBS"

    AC_TRY_LINK(
    [
      #include "confdefs.h"
      #include <SDL.h>
      #undef main
    ],
    [
      SDL_Init(SDL_INIT_VIDEO);
    ],
    AC_MSG_RESULT(yes)
    HAVE_SDL=yes
    WANT_COCOA_UI=yes,
    AC_MSG_RESULT(no)
    HAVE_SDL=no)

    CPPFLAGS="$SAVE_CPPFLAGS"
    LIBS="$SAVE_LIBS"
  else
    AM_PATH_SDL(1.2.0,HAVE_SDL=yes,HAVE_SDL=no)
  fi

  if [[ "x$WANT_SDL" = "xyes" -a "x$HAVE_SDL" = "xno" ]]; then
    AC_MSG_WARN([SDL support wanted, but libSDL could not be found])
    WANT_SDL=no
    NR_ERRORS=`expr $NR_ERRORS + 1`
  fi
fi


if test $ac_cv_prog_gcc = yes; then
  WARNING_CFLAGS="$WARNING_CFLAGS -Wall -Wno-unused -Wno-format -W -Wmissing-prototypes -Wstrict-prototypes"
  OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -fomit-frame-pointer"
fi

if [[ "x$DO_PROFILING" = "xyes" ]]; then
  if [[ "x$HAVE_GCC27" = "xyes" ]]; then
    OPTIMIZE_CFLAGS=""
    DEBUG_CFLAGS="-g -fno-inline -fno-omit-frame-pointer -pg -DUSE_PROFILING"
    LDFLAGS="-pg"
  else
    DO_PROFILING=no
    AC_MSG_WARN([Don't know how to set up profiling for your compiler])
    NR_ERRORS=`expr $NR_ERRORS + 1`
  fi
fi

UAE_CFLAGS="$OPTIMIZE_CFLAGS $DEBUG_CFLAGS $WARNING_CFLAGS"
UAE_CXXFLAGS="$OPTIMIZE_CFLAGS $DEBUG_CFLAGS $WARNING_CXXFLAGS"

dnl -----------------------------------------------
dnl  Check CPU emulation options
dnl

dnl  Now configure the CPU emulation proper
ASMOBJS=
UAE_DEFINES="$UAE_DEFINES -DCPUEMU_0"
CPUOBJS='cpuemu_0.$(OBJEXT)'

if [[ "x$WANT_COMPATIBLE" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DCPUEMU_5"
  CPUOBJS="$CPUOBJS cpuemu_5.\$(OBJEXT)"
fi

if [[ "x$WANT_CYCLEEXACT" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DCPUEMU_6"
  CPUOBJS="$CPUOBJS cpuemu_6.\$(OBJEXT)"
fi

if [[ "x$WANT_FPU" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DFPUEMU"
  CPUOBJS="$CPUOBJS fpp.\$(OBJEXT)"
fi

if [[ "x$HAVE_GCC27" = "xyes" -a "$HOST_CPU" = "i386" -a "x$DO_PROFILING" = "xno" ]]; then
  dnl strength-reduce is turned off not because of paranoia, but because it
  dnl actually makes the code worse in some cases on the i386 (generates too
  dnl many registers, which all end up on the stack).
  UAE_CFLAGS="$UAE_CFLAGS -fno-strength-reduce -DREGPARAM=\"__attribute__((regparm(3)))\""
  UAE_DEFINES="$UAE_DEFINES -DUNALIGNED_PROFITABLE"
else if [[ "x$HAVE_GCC27" = "xyes" -a "$HOST_CPU" = "68k" ]]; then
  UAE_CFLAGS="$UAE_CFLAGS -DREGPARAM=\"__attribute__((regparm(4)))\""
  UAE_DEFINES="$UAE_DEFINES -DUNALIGNED_PROFITABLE -DM68K_FLAG_OPT=1 -DSTATIC_INLINE=\"static inline\""
else
  UAE_CFLAGS="$UAE_CFLAGS -DREGPARAM="
fi
fi

dnl
dnl Define __USE_INLINE__ for AmigaOS4
dnl
if [[ "$HOST_OS" = "amiga" ]]; then
  UAE_DEFINES="$UAE_DEFINES -D__USE_INLINE__"
fi

dnl
dnl  Check whether to build with optimized CCR flags handling
dnl
if [[ "x$HAVE_GCC27" = "xyes" ]]; then
  if [[ "$HOST_CPU" = "i386" -o "$HOST_CPU" = "ppc" -o "$HOST_CPU" = "68k" ]]; then
    GENCPUOPTS="--optimized-flags"
  fi
fi


dnl
dnl  Check whether to build JIT
dnl
AC_MSG_CHECKING([whether to build PPC JIT compiler])
JITOBJS=""
if [[ "x$WANT_JIT" != "xno" -a "$HOST_CPU" = "ppc" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DJIT"
  JITOBJS='compstbl.$(OBJEXT) compemu_macroblocks_ppc.$(OBJEXT) compemu_compiler_ppc.$(OBJEXT) compemu_support.$(OBJEXT)'
  AC_MSG_RESULT(yes)

  AC_MSG_CHECKING([whether assembler supports --execstack option])
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="-Wa,--execstack $CFLAGS"
  AC_TRY_COMPILE([], [int main (void) {return 0;}],
    [AC_MSG_RESULT(yes)
    UAE_CFLAGS="-Wa,--execstack $UAE_CFLAGS"],
    AC_MSG_RESULT(no)
  )
  CFLAGS="$SAVE_CFLAGS"
else
  AC_MSG_RESULT(no)
fi

AC_CACHE_SAVE

dnl
dnl  Check which graphics target to build
dnl
AC_MSG_CHECKING([for graphics target to build])

dnl Check whether we wanted to use ncurses
dnl
dnl First check if we need to uses ncurses because no
dnl other gfx support is available
if [[ "x$WANT_NCURSES" != "xno" -a "x$WANT_SDL" = "xno" -a "x$GFX_DEP" = "x" ]]; then
 WANT_NCURSES=yes
fi

if [[ "x$WANT_NCURSES" = "xyes" ]]; then
 if [[ "x$HAVE_NCURSES_LIB" = "xyes" ]]; then
  GFX_LIBS="-lncurses"
  GFX_CFLAGS=
  GFX_CPPFLAGS=
  GFX_DEP=gfx-curses
  GFX_NAME=ncurses
 else
  AC_MSG_WARN([ncurses support wanted, but libncurses could not be found])
 fi
fi


dnl  Check whether we actually wanted to use SDL or whether
dnl  we need to use SDL because we couldn't find any other
dnl  graphics support
dnl
if [[ "x$WANT_SDL" = "xyes" -o "x$GFX_DEP" = "x" ]]; then
  if [[ "x$HAVE_SDL" = "xyes" -a "x$WANT_SDLGFX" != "xno" ]]; then
    WANT_SDL=yes
    if [[ "x$WANT_SDLGFX" = "xyes" -o "x$GFX_DEP" = "x" ]]; then
      GFX_DEP=gfx-sdl
      GFX_NAME=SDL
      GFX_LIBS=""
      GFX_CFLAGS=""
      GFX_CPPFLAGS=""
      if [[ "x$WANT_SDLGL" = "xyes" ]]; then
        GFX_CFLAGS="-DUSE_GL"
        if [[ "x$OSDEP" = "xod-macosx" ]]; then
          GFX_LIBS="-framework OpenGL"
        else
          GFX_LIBS="-lGL -lGLU"
        fi
      fi
    fi
  fi
fi


dnl  If we got here and we still haven't found a graphics target
dnl  then bail out.
dnl
if [[ "x$GFX_DEP" = "x" ]]; then
  AC_MSG_RESULT(none)
  AC_MSG_ERROR([Could not find a useable graphics target])
fi

AC_MSG_RESULT("$GFX_NAME")

dnl
dnl  If we're using X, find out what extensions are wanted/available
dnl
if [[ "$GFX_DEP" = "gfx-x11" ]]; then
  dnl If the user wants DGA, see if we have it.
  dnl This must come after we checked for X11.

  if [[ "x$WANT_DGA" = "xyes" ]]; then
    TMP_SAVE_LIBS=$LIBS
    LIBS="$X_LIBS $LIBS"
    AC_CHECK_LIB(Xxf86dga, XF86DGAQueryExtension, HAVE_DGA=yes, HAVE_DGA=no, [ $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS ])
    LIBS=$TMP_SAVE_LIBS
    if [[ "x$HAVE_DGA" = "xno" ]]; then
      AC_MSG_WARN([Could not find DGA extension, ignoring --enable-dga])
      NR_ERRORS=`expr $NR_ERRORS + 1`
      WANT_DGA=no
    else
      X_CFLAGS="$X_CFLAGS -DUSE_DGA_EXTENSION"
      X_EXTRA_LIBS="$X_EXTRA_LIBS -lXxf86dga"
    fi
  fi

  if [[ "x$WANT_VIDMODE" = "xyes" ]]; then
    TMP_SAVE_LIBS=$LIBS
    LIBS="$X_LIBS $LIBS"
    AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension, HAVE_VIDMODE=yes, HAVE_VIDMODE=no, [ $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS ])
    LIBS=$TMP_SAVE_LIBS
    if [[ "x$HAVE_VIDMODE" = "xno" ]]; then
      AC_MSG_WARN([Could not find VidMode extension, ignoring --enable-vidmode])
      NR_ERRORS=`expr $NR_ERRORS + 1`
      WANT_VIDMODE=no
    else
      X_CFLAGS="$X_CFLAGS -DUSE_VIDMODE_EXTENSION"
      X_EXTRA_LIBS="$X_EXTRA_LIBS -lXxf86vm"
    fi
  fi

  dnl Check if we can compile with XKB/XKBfile support

  AC_MSG_CHECKING(whether to compile XKB extension support)
  TMP_SAVE_LIBS=$LIBS
  TMP_SAVE_CFLAGS=$CFLAGS
  LIBS="$X_LIBS $LIBS -lxkbfile"
  CFLAGS="$CFLAGS $X_CFLAGS"
  AC_TRY_LINK([
    #include "confdefs.h"
    #include <stdio.h>
    #ifdef HAVE_SYS_TYPES_H
    # include <sys/types.h>
    #endif
    #include <X11/Xlib.h>
    #include <X11/XKBlib.h>
    #include <X11/extensions/XKBrules.h>],[
    int major, minor;
    XkbRF_RulesPtr rules;
    XkbLibraryVersion (&major, &minor);
    rules = XkbRF_Load ("dummy", "", True, True);
  ],
  AC_MSG_RESULT(yes)
  X_CFLAGS="$X_CFLAGS -DUSE_XKB"
  X_EXTRA_LIBS="$X_EXTRA_LIBS -lxkbfile"
  xkb_rules_dir_found="no"
  for xkb_rules_dir in /usr/share/X11/xkb/rules /usr/lib/X11/xkb/rules /usr/X11R6/lib/X11/xkb/rules
  do
	if test -d $xkb_rules_dir ; then
		X_CFLAGS="$X_CFLAGS -DXKB_PATH=\\\"$xkb_rules_dir/\\\""
		xkb_rules_dir_found="yes"
		break
	fi
  done
  if test "x$xkb_rules_dir_found" = "xno" ; then
  	AC_MSG_ERROR([[Could not find xkb path.]])
  fi,
  AC_MSG_RESULT(no)
  )
  LIBS=$TMP_SAVE_LIBS
  CFLAGS=$TMP_SAVE_CFLAGS

dnl See if we can at least compile SHM support

  if [[ "x$no_x" != "xyes" -a "x$ac_cv_header_sys_ipc_h" = "xyes" -a "x$ac_cv_header_sys_shm_h" = "xyes" ]]; then
    AC_MSG_CHECKING(whether the X11 MIT-SHM extension can be compiled in)
    TMP_SAVE_CFLAGS=$CFLAGS
    TMP_SAVE_LIBS=$LIBS
    CFLAGS="$CFLAGS $X_CFLAGS"
    LIBS="$X_LIBS $LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS"
    AC_TRY_LINK([
  #include "confdefs.h"
  #ifdef HAVE_SYS_TYPES_H
  #include <sys/types.h>
  #endif
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/keysym.h>
  #include <X11/cursorfont.h>
  #include <sys/ipc.h>
  #include <sys/shm.h>
  #include <X11/extensions/XShm.h>],[
  static XShmSegmentInfo shminfo;
  static Display *display;
  XShmAttach(display, &shminfo);
  XSync(display,0);
  shmctl(shminfo.shmid, IPC_RMID, 0);
  ],
  SHM_SUPPORT_LINKS=1
  AC_MSG_RESULT(yes),
  SHM_SUPPORT_LINKS=0
  AC_MSG_RESULT(no))
  fi
  CFLAGS=$TMP_SAVE_CFLAGS
  LIBS=$TMP_SAVE_LIBS

  GFX_CFLAGS="$X_CFLAGS"
  GFX_CPPFLAGS="-DSHM_SUPPORT_LINKS=$SHM_SUPPORT_LINKS"
  GFX_LIBS="$X_LIBS $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS"
fi

dnl
dnl
dnl
AC_MSG_NOTICE([================================================================])

dnl
dnl  Check which sound target to use
dnl
AC_MSG_CHECKING([for sound target to build])

if [[ "x$WANT_AUDIO" != "xno" ]]; then
  if [[ "x$SND_NAME" = "xnone" ]]; then
    if [[ "x$ac_cv_header_sys_audioio_h" = "xyes" -o "x$ac_cv_header_sun_audioio_h" = "xyes" ]]; then
      SND_DEP=sd-solaris
      SND_NAME=Solaris/NetBSD
      USE_SOUND=yes
    else if [[ "x$WANT_ALSA" = "xyes" ]]; then
      SND_DEP=sd-alsa
      SND_LIBS="-lasound"
      SND_NAME=ALSA
      USE_SOUND=yes
    else if [[ "x$HAVE_USS_SOUND" = "xyes" ]]; then
      SND_DEP=sd-uss
      if [[ "x$HAVE_LIBOSSAUDIO" = "xyes" ]]; then
        SND_LIBS="-lossaudio"
      fi
      SND_NAME=OSS
      USE_SOUND=yes
    fi
    fi
    fi
  fi

  dnl
  dnl  Did we actually want to use SDL for sound, or should we
  dnl  use it 'cos there's nothing else?
  dnl
  if [[ "x$WANT_SDLSND" = "xyes" -o "x$SND_DEP" = "x" ]]; then
    if [[ "x$HAVE_SDL" = "xyes" -a "x$WANT_SDLSND" != "xno" ]]; then
      SND_DEP=sd-sdl
      SND_LIBS=
      SND_NAME=SDL
      NEED_THREAD_SUPPORT=yes
    fi
  fi
  
  AC_MSG_RESULT("$SND_NAME")

  dnl
  dnl  Enable floppy sound emulation if audio emulation is available
  dnl
  UAE_DEFINES="$UAE_DEFINES -DDRIVESOUND"

else
  AC_MSG_RESULT("sound support disabled")
  SND_DEP=sd-none
  SND_LIBS=
  SND_NAME=none
fi

dnl
dnl  Did we want a GUI?
dnl
AC_MSG_CHECKING([which UI target to build])
if [[ "x$WANT_UI" = "xno" ]]; then
  GUI_DEP=gui-none
  GUI_LIBS=
  GUI_NAME=none
else
  dnl Hack for building Cocoa gui on OS X
  dnl We really should reall check for the presence of objc and Cocoa first
  if [[ "$HOST_OS" = "darwin" ]]; then
    if [[ "x$WANT_COCOA_UI" != "xno" -o "x$HAVE_GTK" = "xno" ]]; then
      GUI_DEP=gui-cocoa
      GUI_CFLAGS="$CFLAGS -DNO_MAIN_IN_MAIN_C"
      GUI_LIBS="-framework Cocoa"
      GUI_NAME="Cocoa"
    fi
  fi
  if [[ "$GUI_DEP" = "gui-none" ]]; then
    if [[ "x$HAVE_GTK" = "xno" ]]; then
      AC_MSG_WARN([UI wanted, but no UI useable target found])
      NR_ERRORS=`expr $NR_ERRORS + 1`
      WANT_UI=no
    else
      GUI_DEP=gui-gtk
      GUI_LIBS="$GTK_LIBS"
      GUI_NAME="GTK+"
      GUI_CFLAGS="$CFLAGS"
      NEED_THREAD_SUPPORT=yes
      if [[ "x$WANT_THREADS" = "xno" ]]; then
        AC_MSG_ERROR([Thread support disabled, but GTK+ UI requires threads])
      fi
    fi
  fi
fi
AC_MSG_RESULT("$GUI_NAME")


dnl
dnl  Do we want AGA emulation
dnl
AC_MSG_CHECKING([whether to build AGA emulation])
if [[ "x$WANT_AGA" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DAGA"
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Do we want Autoconfig?
dnl
AC_MSG_CHECKING([whether to build support for Autoconfig devices])
if [[ "x$WANT_AUTOCONFIG" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DAUTOCONFIG"

  dnl  Configure hard disk emulation
  UAE_DEFINES="$UAE_DEFINES -DFILESYS"
  NEED_THREAD_SUPPORT=yes
  FILESYSOBJS='filesys.$(OBJEXT) fsdb.$(OBJEXT) fsusage.$(OBJEXT) hardfile.$(OBJEXT)'
  if [[ "x$HOST_OS" != "xwin32" ]]; then
    FILESYSOBJS="$FILESYSOBJS filesys_unix.\$(OBJEXT) fsdb_unix.\$(OBJEXT) hardfile_unix.\$(OBJEXT)"
  fi
  dnl  Filesys support currently requires threads, because hardfile
  dnl  support requires threads and we can't currently separate the
  dnl  two.
  dnl
  dnl  However, we don't currently build the threaded filesys implementation
  dnl  because it performs poorly.
  dnl
  dnl  UAE_DEFINES="$UAE_DEFINES -DUAE_FILESYS_THREADS"

  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Do we want SCSI emulation?
dnl  This needs tidying up. ;-)
dnl
AC_MSG_CHECKING([whether to build SCSI emulation])
if [[ "x$WANT_SCSIEMU" = "xyes" ]]; then
  if [[ "x$WANT_AUTOCONFIG" = "xno" ]]; then
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Autoconfig support not enabled, so SCSI emulation cannot be enabled])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
    WANT_SCSIEMU=no
  else
    if [[ "$OSDEP" = "od-amiga" ]]; then
      AC_MSG_RESULT(yes)
      SCSIOBJS='scsiemul.$(OBJEXT) blkdev.$(OBJEXT)'
      UAE_DEFINES="$UAE_DEFINES -DSCSIEMU"
      NEED_THREAD_SUPPORT=yes
    else
    SCGLIBS="-lscg -lschily"
    if [[ "$HOST_OS" = "darwin" ]]; then
      SCGLIBS="$SCGLIBS -framework IOKit -framework CoreFoundation"
    fi
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING([for libscg and libschily])
    OLDLIBS="$LIBS"
    OLDCPPFLAGS="$CPPFLAGS"
    if [[ "x$LIBSCG_PREFIX" != "x" ]]; then
       LIBSCG_CPPFLAGS="-I$LIBSCG_PREFIX/include"
       LIBSCG_LIBS="-L$LIBSCG_PREFIX/lib"
    fi
    if [[ "x$LIBSCG_INCLUDEDIR" != "x" ]]; then
       LIBSCG_CPPFLAGS="-I$LIBSCG_INCLUDEDIR"
    fi
    if [[ "x$LIBSCG_LIBDIR" != "x" ]]; then
       LIBSCG_LIBS="-L$LIBSCG_LIBDIR $SCGLIBS"
    else
       LIBSCG_LIBS="$SCGLIBS"
    fi
    LIBS="$LIBS $LIBSCG_LIBS"
    CPPFLAGS="$CPPFLAGS $LIBSCG_CPPFLAGS"
    AC_TRY_LINK([
typedef int BOOL;
#include "scg/scgcmd.h"
#include "scg/scsitransp.h"
#include "scg/scsireg.h"],
[{SCSI *scgp = scg_open (0, 0, 0, 0, 0);}],
    [
      AC_MSG_RESULT(yes)
      SCSIOBJS='scsiemul.$(OBJEXT) blkdev.$(OBJEXT) blkdev-libscg.$(OBJEXT)'
      UAE_DEFINES="$UAE_DEFINES -DSCSIEMU"
      UAE_CPPFLAGS="$UAE_CPPFLAGS $LIBSCG_CPPFLAGS"
      UAE_LIBS="$LIBSCG_LIBS $UAE_LIBS"
      NEED_THREAD_SUPPORT=yes
    ],
    [
      AC_MSG_RESULT(no)
      AC_MSG_CHECKING([for libscg and libschily in UAE source tree])
      LIBSCG_CPPFLAGS="$OLDCPPFLAGS -I$srcdir/src/include"
      LIBSCG_LIBS="-L$srcdir/src $OLDLIBS $SCGLIBS"
      CPPFLAGS="$LIBSCG_CPPFLAGS"
      LIBS="$LIBSCG_LIBS"
      AC_TRY_LINK([
typedef int BOOL;
#include "scg/scgcmd.h"
#include "scg/scsitransp.h"
#include "scg/scsireg.h"],
[{SCSI *scgp = scg_open (0, 0, 0, 0, 0);}],
      [
        AC_MSG_RESULT(yes)
        SCSIOBJS='scsiemul.$(OBJEXT) blkdev.$(OBJEXT) blkdev-libscg.$(OBJEXT)'
        UAE_DEFINES="$UAE_DEFINES -DSCSIEMU"
        NEED_THREAD_SUPPORT=yes
        if [[ "$srcdir" != "." ]]; then
dnl          AC_CONFIG_LINKS(src/libscg.a:src/libscg.a)
dnl          AC_CONFIG_LINKS(src/libschily.a:src/libschily.a)
	  UAE_LIBS="$LIBSCG_LIBS $UAE_LIBS"
        else
          UAE_LIBS="-L. $SCGLIBS $UAE_LIBS"
        fi
      ],
      [
        AC_MSG_RESULT(no)

	AC_MSG_CHECKING([for Linux cdrom support via ioctl])
	LIBS="$OLDLIBS"
	CPPFLAGS="$OLDCPPFLAGS"
	AC_TRY_LINK([
#include <linux/cdrom.h>],
[{struct cdrom_generic_command cmd; ioctl (0, CDROM_SEND_PACKET, &cmd);}],
	[
	   AC_MSG_RESULT(yes)
	   SCSIOBJS='scsiemul.$(OBJEXT) blkdev.$(OBJEXT)'
	   UAE_DEFINES="$UAE_DEFINES -DSCSIEMU -DSCSIEMU_LINUX_IOCTL"
	   NEED_THREAD_SUPPORT=yes
	],
	[

	AC_MSG_RESULT(no)
        NR_WARNINGS=`expr $NR_WARNINGS + 1`
        echo "Please refer to the UAE SCSI device section in docs/README for installation instructions."
        WANT_SCSIEMU=no
	])
      ])
    LIBS="$OLDLIBS"
    CPPFLAGS="$OLDCPPFLAGS"
    ])
    fi
  fi
else
  AC_MSG_RESULT(no)
fi
if [[ "x$WANT_SCSIEMU" != "xyes" ]]; then
  SCSIOBJS='scsi-none.$(OBJEXT)'
  AC_MSG_RESULT(no)
fi


dnl
dnl  Do we want CDTV emulation ?
dnl
if [[ "x$WANT_CDTV" != "xyes" -a "x$WANT_SCSIEMU" != "xyes" ]]; then
  WANT_CDTV=no
fi
AC_MSG_CHECKING([whether to build CDTV emulation])
if [[ "x$WANT_CDTV" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  if [[ "x$WANT_SCSIEMU" != "xyes" ]]; then
    AC_MSG_WARN([SCSI support is not enabled or cannot be built. Disabling CDTV emulation])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
    WANT_CDTV=no
  else
    UAE_DEFINES="$UAE_DEFINES -DCDTV"
    CDOBJS='cdrom.$(OBJEXT)'
  fi
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Do we want CD32 emulation ?
dnl
if [[ "x$WANT_CD32" != "xyes" ]]; then
  if [[ "x$WANT_SCSIEMU" != "xyes" -o "x$WANT_AGA" = "xno" ]]; then
    WANT_CD32=no
  fi
fi
AC_MSG_CHECKING([whether to build CD32 emulation])
if [[ "x$WANT_CD32" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  if [[ "x$WANT_SCSIEMU" != "xyes" ]]; then
    AC_MSG_WARN([SCSI support is not enabled or cannot be built. Disabling CD32 emulation])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
    WANT_CD32=no
  else
    if [[ "x$WANT_AGA" != "xyes" ]]; then
      AC_MSG_WARN([AGA emulation is not enabled. Disabling CD32 emulation])
      NR_WARNINGS=`expr $NR_WARNINGS + 1`
      WANT_CD32=no
    else
      UAE_DEFINES="$UAE_DEFINES -DCD32"
      if [[ "x$WANT_CDTV" = "xno" ]]; then
        CDOBJS='cdrom.$(OBJEXT)'
      fi
      CDOBJS="$CDOBJS akiko.\$(OBJEXT)"
    fi
  fi
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Do we want bsdsocket.library emulation?
dnl
AC_MSG_CHECKING([whether to build bsdsocket.library emulation])
if [[ "x$WANT_BSDSOCK" != "xno" ]]; then
  if [[ "x$WANT_AUTOCONFIG" = "xno" ]]; then
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Autoconfig support not enabled, so bsdsocket emulation cannot be enabled])
    NR_WARNINGS=`expr $NR_WARNINGS + 1`
  else
    NEED_THREAD_SUPPORT=yes
    UAE_DEFINES="$UAE_DEFINES -DBSDSOCKET"
    BSDSOCKOBJS='bsdsocket-posix-new.$(OBJEXT) bsdsocket.$(OBJEXT)'
    AC_MSG_RESULT(yes)
  fi
else
  AC_MSG_RESULT(no)
  BSDSOCKOBJS='bsdsocket.$(OBJEXT)'
fi


dnl
dnl  Check whether we want or need threads
dnl
AC_MSG_CHECKING([whether to build with threads])
if [[ "x$WANT_THREADS" = "xyes" -o "x$NEED_THREAD_SUPPORT" = "xyes" ]]; then
  AC_MSG_RESULT(yes)
  AC_MSG_CHECKING([which thread target to build])

  if [[ "x$WANT_SDL" = "xyes" -o "$THREADDEP" = "td-none" ]]; then
    if [[ "x$HAVE_SDL" = "xyes" -a "x$WANT_SDL" != "xno" ]]; then
      WANT_SDL=yes
      THREADDEP=td-sdl
      THREADNAME=SDL
    fi
  fi

  if [[ "$THREADDEP" = "td-none" ]]; then
    AC_MSG_ERROR([Thread support wanted or required, but no thread target found])
  fi

  if [[ "$THREADDEP" = "td-posix" ]]; then
    CC="$PTHREAD_CC"
    UAE_CFLAGS="$UAE_CFLAGS $PTHREAD_CFLAGS"
    UAE_LIBS="$PTHREAD_LIBS $UAE_LIBS"

    dnl Hack to use named semaphores on OS X/darwin
    dnl This needs to be properly checked when detecting pthreads!
    if [[ "$HOST_OS" = "darwin" ]]; then
      UAE_DEFINES="$UAE_DEFINES -DUSE_NAMED_SEMAPHORES"
    fi
  fi

  UAE_DEFINES="$UAE_DEFINES -DSUPPORT_THREADS"
  AC_MSG_RESULT($THREADNAME)
else
  THREADDEP=td-none
  AC_MSG_RESULT(no)
fi


dnl
dnl  So, are we using SDL?
dnl
if [[ "x$WANT_SDL" = "xyes" ]]; then
   UAE_DEFINES="$UAE_DEFINES -DUSE_SDL"
   JOY_DEP=jd-sdl
   UAE_LIBS="$SDL_LIBS $UAE_LIBS"
   UAE_CPPFLAGS="$UAE_CPPFLAGS $SDL_CFLAGS"
fi


dnl
dnl  Check for CAPS API
dnl
WANT_CAPS=dunno
AC_ARG_WITH(caps,        AS_HELP_STRING([--with-caps],        [Build with support for CAPS/IPF disk images) (default auto)]), [WANT_CAPS=$withval],[])
AC_ARG_WITH(caps-prefix, AS_HELP_STRING([--with-caps-prefix], [Prefix where CAPS API is installed (optional)]),                 [CAPS_PREFIX=$withval],[])
if [[ "x$WANT_CAPS" != "xno" ]]; then
  SAVE_CPPFLAGS="$CPPFLAGS"
  CAPS_CPPFLAGS="-I$CAPS_PREFIX/include"
  if [[ "x$CAPS_PREFIX" != "x" ]]; then
    CPPFLAGS="$CPPFLAGS $CAPS_CPPFLAGS"
  fi
  AC_CHECK_HEADERS(caps/capsimage.h, [HAVE_CAPS=yes], [HAVE_CAPS=no])
  if [[ "$HAVE_CAPS" = "yes" ]]; then
    UAE_DEFINES="$UAE_DEFINES -DCAPS"
    UAE_CPPFLAGS="$UAE_CPPFLAGS $CAPS_CPPFLAGS"
  else
    if [[ "$WANT_CAPS" = "yes" ]]; then
      AC_MSG_WARN([CAPS support wanted, but CAPS API not found])
      WANT_CAPS=no
    fi
  fi
  CPPFLAGS="$SAVE_CPPFLAGS"

  if [[ "$HAVE_CAPS" = "yes" ]]; then
    dnl Check for CAPSImage framework on OS X.
    SAVE_LIBS="$LIBS"
    AC_CHECK_FRAMEWORK(CAPSImage, CAPSInit, [
      UAE_LIBS="$UAE_LIBS -weak_framework CAPSImage"
      AC_DEFINE(HAVE_FRAMEWORK_CAPSIMAGE, 1, [Define to 1 if you have the CAPS framework.])
    ])
    LIBS="$SAVE_LIBS"
  fi
fi


dnl
dnl  Check whether FDI (FDI 1.0 and 2.x image) support wanted
dnl
if [[ "x$WANT_FDI" != "xno" ]]; then
  UAE_DEFINES="$UAE_DEFINES -DFDI2RAW"
fi



dnl
dnl  Build debugger?
dnl
AC_MSG_CHECKING([whether to build internal debugger/monitor])
if [[ "x$WANT_DEBUGGER" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  UAE_DEFINES="$UAE_DEFINES -DDEBUGGER"
  DEBUGOBJS='debug.$(OBJEXT) ppc_disasm.$(OBJEXT) identify.$(OBJEXT)'
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Support state saving/loading?
dnl
AC_MSG_CHECKING([whether to build support for saving/loading emulator state])
if [[ "x$WANT_STATESAVING" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  UAE_DEFINES="$UAE_DEFINES -DSAVESTATE"
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Build fake enforcer?
dnl
AC_MSG_CHECKING([whether to build with enforcer])
if [[ "x$WANT_ENFORCER" != "xno" ]]; then
  if [[ "x$WANT_AUTOCONFIG" != "xno" ]]; then
    AC_MSG_RESULT(yes)
    UAE_DEFINES="$UAE_DEFINES -DENFORCER"
  else
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Autoconfig support not enabled, so enforcer emulation cannot be enabled])
  fi
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Build Action Replay emulation
dnl
AC_MSG_CHECKING([whether to build Action Replay emulation])
if [[ "x$WANT_ACTION_REPLAY" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  UAE_DEFINES="$UAE_DEFINES -DACTION_REPLAY"
else
  AC_MSG_RESULT(no)
fi


dnl
dnl  Include keymapping for X-Arcade joysticks
dnl
AC_MSG_CHECKING([whether to include keymaps for X-Arcade joysticks])
if [[ "x$WANT_XARCADE" != "xno" ]]; then
  AC_MSG_RESULT(yes)
  UAE_DEFINES="$UAE_DEFINES -DXARCADE"
else
  AC_MSG_RESULT(no)
fi

dnl -----------------------------------------------
dnl CPU Specifics
dnl -----------------------------------------------

dnl ppc
if [[ "$HOST_CPU" = 'ppc' -o "$HOST_CPU" = 'ppc64' ]]; then 
AC_DEFINE_UNQUOTED(PPC_CPU, $host_cpu, [PPC CPU])

dnl
dnl  Do we want local cpu optimization
dnl
AC_MSG_CHECKING([whether to build with cpu specific optimization])
if [[ "x$WANT_CPUOPTI" != "xno" ]]; then
     AC_MSG_RESULT(yes)

dnl
dnl 32 or 64 bit
dnl 
is_32bit=no
AC_MSG_CHECKING([whether CPU is 32bit])
if [[ "$HOST_CPU" = "ppc" ]]; then
is_32bit=yes
AC_DEFINE(HAVE_PPC32,1,[CPU is PPC32])
AC_DEFINE(ICACHE_SIZE,32,[CPU ICACHE_SIZE])
AC_DEFINE(DCACHE_SIZE,32,[CPU DCACHE_SIZE])
fi
AC_MSG_RESULT([$is_32bit])

is_64bit=no
AC_MSG_CHECKING([whether CPU is 64bit])
if [[ "$HOST_CPU" = "ppc64" ]]; then
is_64bit=yes
dnl Generally the case 
AC_DEFINE(HAVE_PPC64,1,[CPU is PPC64])
AC_DEFINE(ICACHE_SIZE,64,[CPU ICACHE_SIZE])
AC_DEFINE(DCACHE_SIZE,64,[CPU DCACHE_SIZE])
fi
AC_MSG_RESULT([$is_64bit])

have_g4=no
have_g5=no
have_altivec=no
have_440ep=no
have_460ex=no

dnl -- macos
if [[ "x$OSDEP" = "xod-macosx" ]]; then

AC_MSG_CHECKING([host cpu Altivec(tm) support])
GETFLAGS=`sysctl -a hw | grep hw.optional.altivec: | awk '{print $2}'`
if test "$GETFLAGS" = "1"; then
have_altivec=yes
CPUCFLAGS="-maltivec -mabi=altivec -faltivec"
AC_DEFINE(HAVE_ALTIVEC,1,[CPU supports Altivec])
else
have_altivec=no
fi
AC_MSG_RESULT([$have_altivec])

dnl detecting cpu for osx ppc, since march=native does not work, get cpusubtype, check mach/machine.h for list.
dnl 9 = g3 (750) 10 = g4 (7400) 11 = g4 (7450) 100 = g5 (970)
AC_MSG_CHECKING([host cpu type])
GETFLAGS=`sysctl -a hw | grep hw.cpusubtype:  | awk '{print $2}'`
if test "$GETFLAGS" =  "9" ; then
CPUCFLAGS="-mcpu=750 -mtune=750" 
elif test  "$GETFLAGS" =  "10" ; then
CPUCFLAGS="$CPUCFLAGS -mcpu=7400 -mtune=7400"
elif test  "$GETFLAGS" =  "11" ; then
CPUCFLAGS="$CPUCFLAGS -mcpu=7450 -mtune=7450"
elif test  "$GETFLAGS" =  "100" ; then
CPUCFLAGS="$CPUCFLAGS -mcpu=970 -mtune=970"
fi
AC_MSG_RESULT([$GETFLAGS using $CPUCFLAGS])

AC_MSG_CHECKING([host cpu L1 icache size support])
GETFLAGS=`sysctl -a hw | grep hw.l1icachesize: | awk '{print $2}'`
if test "$GETFLAGS" = "32768" ; then
AC_DEFINE(ICACHE_SIZE,32,[CPU ICACHE_SIZE])
elif  test "$GETFLAGS" = "65526" ; then
AC_DEFINE(ICACHE_SIZE,64,[CPU ICACHE_SIZE])
elif  test "$GETFLAGS" = "131072" ; then
AC_DEFINE(ICACHE_SIZE,128,[CPU ICACHE_SIZE])
fi
AC_MSG_RESULT([$GETFLAGS])

AC_MSG_CHECKING([host cpu L1 dcache size support])
GETFLAGS=`sysctl -a hw | grep hw.l1dcachesize: | awk '{print $2}'`
if test "$GETFLAGS" = "32768" ; then
AC_DEFINE(DCACHE_SIZE,32,[CPU DCACHE_SIZE])
elif  test "$GETFLAGS" = "65526" ; then
AC_DEFINE(DCACHE_SIZE,64,[CPU DCACHE_SIZE])
elif  test "$GETFLAGS" = "131072" ; then
AC_DEFINE(DCACHE_SIZE,128,[CPU DCACHE_SIZE])
fi
AC_MSG_RESULT([$GETFLAGS])

fi 
dnl -- macos

dnl -- linux
if [[ "x$OSDEP" = "xod-linux" ]]; then
_cpuinfo="cat /proc/cpuinfo"
proc=$($_cpuinfo | grep 'cpu' | cut -d ':' -f 2 | cut -d ',' -f 1 | cut -b 2- | head -n 1)
AC_MSG_CHECKING([host cpu Altivec(tm) support])
    if test -n "$($_cpuinfo | grep altivec)"; then
      _altivec=yes
    fi
    if test "$_altivec" = "yes"; then
AC_MSG_RESULT([$proc altivec])
CPUFLAGS="-maltivec -mabi=altivec"
    else
        _altivec=no
AC_MSG_RESULT([$proc"])
    fi


AC_MSG_CHECKING([host cpu is ])

    if test -n "$proc"; then
        case "$proc" in
            601) _march='-mcpu=601' _mcpu='-mtune=601' ;;
            603) _march='-mcpu=603' _mcpu='-mtune=603' ;;
            603e|603ev) _march='-mcpu=603e' _mcpu='-mtune=603e' ;;
            604|604e|604r|604ev) _march='-mcpu=604' _mcpu='-mtune=604' ;;
            740|740/750|745/755) _march='-mcpu=740' _mcpu='-mtune=740' ;;
            750|750CX) _march='-mcpu=750' _mcpu='-mtune=750' ;;
            POWER)  _march='-mcpu=power'  _mcpu='-mtune=power'  ;;
            POWER2) _march='-mcpu=power2' _mcpu='-mtune=power2' ;;
            POWER3) _march='-mcpu=power3' _mcpu='-mtune=power3' ;;
            7400*|7410*) _march='-mcpu=7400' _mcpu='-mtune=7400' have_g4='yes' ;;
            7447*|7450*|7455*) _march='-mcpu=7450' _mcpu='-mtune=7450' have_g4='yes' ;;
            970*|PPC970*) _march='-mcpu=970' _mcpu='-mtune=970' have_g5='yes'
				AC_DEFINE(DCACHE_SIZE,128,[CPU DCACHE_SIZE])
				AC_DEFINE(ICACHE_SIZE,128,[CPU ICACHE_SIZE])
			;;
            POWER4) _march='-mcpu=power4' _mcpu='-mtune=power4' have_g4='yes' ;;
            POWER5*) _march='-mcpu=power5' _mcpu='-mtune=power5' have_g5='yes'
				AC_DEFINE(DCACHE_SIZE,128,[CPU DCACHE_SIZE])
				AC_DEFINE(ICACHE_SIZE,128,[CPU ICACHE_SIZE])
			;;
			POWER6*) _march='-mcpu=power6' _mcpu='-mtune=power6' ;;
			POWER7*) _march='-mcpu=power7' _mcpu='-mtune=power7' ;;
			POWER8*) _march='-mcpu=power8' _mcpu='-mtune=power8' ;;
            440EP*) _march='-mcpu=440fp' _mcpu='-mtune=440fp' have_440ep='yes'
 				AC_DEFINE_UNQUOTED(AMCC_CPU, "$host_cpu"-amcc-440ep, [AMCC PPC CPU])
				;;
            440G* ) _march='-mcpu=440'   _mcpu='-mtune=440' have_440ep='yes'
  				AC_DEFINE_UNQUOTED(AMCC_CPU, "$host_cpu"-amcc-440g, [AMCC PPC CPU])
				;;
            460EX*) _march='-mcpu=440fp' _mcpu='-mtune=440fp'
				AC_DEFINE_UNQUOTED(AMCC_CPU, "$host_cpu"-amcc-460ex, [AMCC PPC CPU])
				;;
            CELL*) _march='-mcpu=cell' _mcpu='-mtune=cell' ;;
			PA6T*) _march='-mcpu=pa6t' _mcpu='-mtune=pa6t' ;;
            *) ;;
        esac
	fi


    if test -n "$_mcpu"; then
        _optimizing=$(echo $_mcpu | cut -c 8-)
AC_MSG_RESULT(["$_optimizing"])
    else
AC_MSG_RESULT(["none"])
    fi

CPUFLAGS="$_march $_mcpu $CPUFLAGS"

fi
dnl -- linux

cp confdefs.h src/include/config.h

CFLAGS="$CFLAGS $CPUCFLAGS"
CXXFLAGS="$CXXFLAGS $CPUCFLAGS"
CPPFLAGS="$CPPFLAGS $CPUCFLAGS"


else
  AC_MSG_RESULT(no)
fi
dnl -- Do we want local cpu optimazions

fi
dnl -- ppc

LIBS="$LIBS $GUI_LIBS $GFX_LIBS $SND_LIBS $JOY_LIBS $UAE_LIBS"
CPPFLAGS="$CPPFLAGS $UAE_CPPFLAGS $UAE_DEFINES $GFX_CPPFLAGS"
CFLAGS="$CFLAGS $UAE_CFLAGS $GUI_CFLAGS $GFX_CFLAGS $SND_CFLAGS $JOY_CFLAGS"

AC_CONFIG_LINKS(src/target.h:src/targets/$TARGETDEP)
AC_CONFIG_LINKS(src/machdep:src/$MACHDEP)
AC_CONFIG_LINKS(src/md-fpp.h:src/include/$MDFPP_H)
AC_CONFIG_LINKS(src/osdep:src/$OSDEP)
AC_CONFIG_LINKS(src/gfxdep:src/$GFX_DEP)
AC_CONFIG_LINKS(src/sounddep:src/$SND_DEP)
AC_CONFIG_LINKS(src/threaddep:src/$THREADDEP)
AC_CONFIG_LINKS(src/joydep:src/$JOY_DEP)
AC_CONFIG_LINKS(src/guidep:src/$GUI_DEP)

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/dms/Makefile
		 src/caps/Makefile
		 src/md-generic/Makefile
		 src/md-i386-gcc/Makefile
		 src/md-ppc-gcc/Makefile
		 src/md-ppc/Makefile
		 src/md-68k/Makefile
		 src/md-amd64-gcc/Makefile
		 src/od-generic/Makefile
		 src/od-linux/Makefile
		 src/od-amiga/Makefile
		 src/od-beos/Makefile
		 src/od-macosx/Makefile
		 src/od-macosx/Info.plist
		 src/od-win32/Makefile
		 src/td-none/Makefile
		 src/td-amigaos/Makefile
		 src/td-beos/Makefile
		 src/td-posix/Makefile
		 src/td-sdl/Makefile
		 src/td-win32/Makefile
		 src/gfx-amigaos/Makefile
		 src/gfx-beos/Makefile
		 src/gfx-x11/Makefile
		 src/gfx-sdl/Makefile
		 src/gfx-curses/Makefile
		 src/gfx-svga/Makefile
		 src/sd-alsa/Makefile
		 src/sd-amigaos/Makefile
		 src/sd-beos/Makefile
		 src/sd-none/Makefile
		 src/sd-sdl/Makefile
		 src/sd-solaris/Makefile
		 src/sd-uss/Makefile
		 src/jd-none/Makefile
		 src/jd-amigainput/Makefile
		 src/jd-amigaos/Makefile
		 src/jd-beos/Makefile
		 src/jd-linuxold/Makefile
		 src/jd-sdl/Makefile
		 src/gui-none/Makefile
		 src/gui-beos/Makefile
		 src/gui-cocoa/Makefile
		 src/gui-gtk/Makefile
		 src/gui-muirexx/Makefile
		 src/keymap/Makefile
		 src/test/Makefile
])

AM_CONDITIONAL(TARGET_BIGENDIAN, test x$ac_cv_c_bigendian = xyes)
AM_CONDITIONAL(TARGET_BEOS,  test x$OSDEP = xod-beos)
AM_CONDITIONAL(TARGET_LINUX, test x$OSDEP = xod-linux)
AM_CONDITIONAL(TARGET_WIN32, test x$OSDEP = xod-win32)
AM_CONDITIONAL(BUILD_MACOSX_BUNDLE, test x$OSDEP = xod-macosx)

AC_SUBST(UAE_CFLAGS)
AC_SUBST(UAE_CPPFLAGS)
AC_SUBST(UAE_CXXFLAGS)
AC_SUBST(UAE_LIBS)

AC_SUBST(ac_cv_c_inline)
AC_SUBST(WRCPRG)
AC_SUBST(NO_SCHED_CFLAGS)
AC_SUBST(GENCPUOPTS)
AC_SUBST(TARGET)
AC_SUBST(MACHDEP)
AC_SUBST(OSDEP)
AC_SUBST(THREADDEP)
AC_SUBST(GFX_DEP)
AC_SUBST(SND_DEP)
AC_SUBST(JOY_DEP)
AC_SUBST(GUI_DEP)
AC_SUBST(EXTRAOBJS)
AC_SUBST(CPUOBJS)
AC_SUBST(JITOBJS)
AC_SUBST(RESOBJS)
AC_SUBST(ASMOBJS)
AC_SUBST(SCSIOBJS)
AC_SUBST(CDOBJS)
AC_SUBST(DEBUGOBJS)
AC_SUBST(FILESYSOBJS)
AC_SUBST(BSDSOCKOBJS)
AC_SUBST(UAE_RSRCFILE)
AC_SUBST(SET_MAKE)
AC_SUBST(MATHLIB)
AC_SUBST(top_srcdir)

dnl  If we're not cross-compiling then, the config script in src/tools
dnl  can be called by config.status as normal
dnl
if [[ "x$cross_compiling" != "xyes" ]]; then
  AC_CONFIG_SUBDIRS(src/tools)
fi

AC_OUTPUT

dnl  However, if we are cross-compiling, then we need to call
dnl  configure in src/tools manually ourselves. This is because
dnl  autoconf currently has no support for building host tools
dnl  when cross-compiling. Okay. This is a hack. But it works. Can
dnl  anybody think of a better way of doing this?
dnl
if [[ "x$cross_compiling" = "xyes" ]]; then
  export CC=$CC_FOR_BUILD
  export CFLAGS=$CFLAGS_FOR_BUILD
  export LDFLAGS=$LDFLAGS_FOR_BUILD
  export LIBS=$LIBS_FOR_BUILD
  mkdir -p src/tools
  cd  src/tools && ../../"$srcdir"/src/tools/configure --cache-file=/dev/null --host=$build --target=$host
fi
