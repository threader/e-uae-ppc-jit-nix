INCLUDES =  -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)

AM_CXXFLAGS = $(CFLAGS) -Wno-multichar

noinst_LIBRARIES = libjoydep.a libsnddep.a libgfxdep.a libguidep.a

libjoydep_a_SOURCES = joystick.cpp
libsnddep_a_SOURCES = sound.cpp
libgfxdep_a_SOURCES = gfx.c
libguidep_a_SOURCES = beos.cpp

noinst_HEADERS = exectasks.h memory.h sound.h
