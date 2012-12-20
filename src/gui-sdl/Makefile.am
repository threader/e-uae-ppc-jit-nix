AM_CPPFLAGS  = @UAE_CPPFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS    = @UAE_CFLAGS@
AM_CFLAGS   += @SDL_CFLAGS@

noinst_LIBRARIES = libguidep.a

libguidep_a_SOURCES = inputmode.c menu.c menu_load.c menu_prefs.c \
		menu_tweak.c vkbd.cpp volumecontrol.c

noinst_HEADERS = inputmode.h menu.h vkbd.h volumecontrol.h
