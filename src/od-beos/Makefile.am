INCLUDES =  -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)

AM_CXXFLAGS = $(CFLAGS) -Wno-multichar

noinst_LIBRARIES = libjoydep.a libsnddep.a libgfxdep.a libguidep.a libosdep.a

libjoydep_a_SOURCES = joystick.cpp
libsnddep_a_SOURCES = sound.cpp
libgfxdep_a_SOURCES = beos.cpp
libguidep_a_SOURCES = gui.cpp
libosdep_a_SOURCES  = main.cpp

noinst_HEADERS = exectasks.h gfx.h memory.h sound.h

dist_noinst_DATA = uae.rsrc
