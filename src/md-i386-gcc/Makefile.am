INCLUDES =  -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)

AM_CCASFLAGS = $(INCLUDES)

noinst_LIBRARIES = libmachdep.a

libmachdep_a_SOURCES = support.c

noinst_HEADERS = exectasks.h m68k.h maccess.h rpt.h
EXTRA_libmachdep_a_SOURCES = X86.S
