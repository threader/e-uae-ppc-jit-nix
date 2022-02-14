INCLUDES =  -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)

noinst_LIBRARIES = libjoydep.a libsnddep.a libgfxdep.a libguidep.a libosdep.a

libgfxdep_a_SOURCES = ami-win.c ami-appw.c
libguidep_a_SOURCES = ami-rexx.c ami-gui.c ami-disk.c
libjoydep_a_SOURCES = joystick.c
libsnddep_a_SOURCES = sound.c
libosdep_a_SOURCES = support.c

noinst_HEADERS = amiga-kludges.h exectasks.h memory.h rpt.h sound.h thread.h 
dist_bin_SCRIPTS    = UAEWB UAEWB.aux UAEWB.rexx UAEGUI.rexx
dist_noinst_SCRIPTS = scc
