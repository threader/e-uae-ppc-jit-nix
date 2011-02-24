AM_CPPFLAGS  = @UAE_CPPFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS    = @UAE_CFLAGS@

noinst_LIBRARIES = libosdep.a

libosdep_a_SOURCES = main.m memory.c parser.c

noinst_HEADERS = main.h memory.h hrtimer.h
noinst_DATA = Info.plist.in puae.icns

EXTRA_DIST = puae.icns
