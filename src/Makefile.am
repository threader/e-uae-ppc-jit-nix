INCLUDES = \
	-I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)

SUBDIRS = \
	tools @MACHDEP@ @THREADDEP@ @GFX_DEP@ @SND_DEP@ @JOY_DEP@ @GUI_DEP@ \
	@OSDEP@ keymap dms caps

DIST_SUBDIRS = \
	md-generic md-68k md-i386-gcc md-i386-watcom md-ppc md-ppc-gcc \
	gfx-amigaos gfx-beos gfx-x11 gfx-sdl gfx-curses gfx-svga \
	sd-none sd-af sd-alsa sd-amigaos sd-beos sd-file sd-mme sd-sdl sd-sgi sd-solaris sd-uss \
	jd-none jd-amigaos jd-beos jd-linuxold jd-sdl \
	gui-none gui-beos gui-cocoa gui-gtk gui-muirexx \
	od-generic od-amiga od-beos od-linux od-macos od-macosx \
	td-none td-amigaos td-beos td-posix td-sdl \
	keymap \
	caps dms

BUILT_SOURCES = \
	blit.h blitfunc.h blitfunc.c blittable.c \
	cpudefs.c \
	cpuemu_0.c cpuemu_5.c cpuemu_6.c \
	cpustbl.c cputbl.h \
	compemu.c \
	compstbl.c comptbl.h

CLEANFILES = \
	blit.h blitfunc.h blitfunc.c blittable.c \
	cpudefs.c \
	cpuemu_0.c cpuemu_5.c cpuemu_6.c \
	cpustbl.c cputbl.h \
	compemu.c \
	compstbl.c comptbl.h

DISTCLEANFILES = \
	machdep osdep threaddep gfxdep sounddep joydep guidep \
	target.h md-X86.S md-fpp.h

bin_PROGRAMS  = \
	uae readdisk

uae$(EXEEXT):	$(uae_OBJECTS) $(uae_DEPENDENCIES)
		@rm -f uae$(EXEEXT)
		$(LINK) $(uae_LDFLAGS) $(uae_OBJECTS) $(uae_LDADD) $(LIBS)
if TARGET_BEOS
		xres -o $@ @UAE_RSRCFILE@
		mimeset -f $@
endif


noinst_LIBRARIES = \
	libcpuemu.a

dist_noinst_DATA = \
	table68k inputevents.def filesys.asm

noinst_HEADERS = \
	include/akiko.h		include/ar.h		\
	include/audio.h		include/autoconf.h	\
	include/blitter.h	include/blkdev.h	\
	include/bsdsocket.h 	include/caps.h		\
	include/catweasel.h	include/cia.h		\
	include/commpipe.h	include/compemu.h	\
	include/cpu_prefetch.h  include/custom.h	\
	include/debug.h		include/disk.h 		\
	include/drawing.h 	include/driveclick.h	\
	include/enforcer.h	include/ersatz.h	\
	include/events.h 	include/execlib.h 	\
	include/fdi2raw.h	include/filesys.h	\
	include/fpp-ieee-be.h	include/fpp-unknown.h	\
	include/fsdb.h		include/fsusage.h	\
	include/genblitter.h	include/gensound.h	\
	include/gfxfilter.h	include/gui.h		\
	include/hotkeys.h	include/identify.h	\
	include/inputdevice.h	include/joystick.h	\
	include/keyboard.h	include/keybuf.h	\
	include/mackbd.h	include/memory.h	\
	include/native2amiga.h	include/newcpu.h	\
	include/noflags.h	include/options.h	\
	include/osemu.h		include/picasso96.h	\
	include/readcpu.h	include/savestate.h	\
	include/scsidev.h	include/serial.h	\
	include/sleep.h		include/sysdeps.h	\
	include/tui.h		include/uae.h		\
	include/uaeexe.h	include/unzip.h		\
	include/xwin.h		include/zfile.h		\
	targets/t-amiga.h	targets/t-beos.h	\
	targets/t-macos.h	targets/t-unix.h

dist_noinst_SCRIPTS = \
	filesys.sh install_libscg

EXTRA_DIST = \
	tools/configure.in tools/configure tools/sysconfig.h.in \
	tools/aclocal.m4 tools/target.h tools/Makefile.in

uae_SOURCES = \
	main.c newcpu.c fpp.c memory.c events.c custom.c serial.c cia.c \
	blitter.c autoconf.c ersatz.c keybuf.c expansion.c \
	zfile.c cfgfile.c picasso96.c inputdevice.c \
	gfxutil.c gfxlib.c audio.c drawing.c \
	native2amiga.c identify.c disk.c  savestate.c unzip.c \
	uaeexe.c uaelib.c fdi2raw.c sleep.c hotkeys.c \
	ar.c driveclick.c enforcer.c misc.c \
	missing.c readcpu.c writelog.c

EXTRA_uae_SOURCES = \
	bsdsocket.c bsdsocket-posix.c bsdsocket-posix-new.c build68k.c catweasel.c cdrom.c \
	compemu_fpp.c compemu_optimizer.c compemu_optimizer_x86.c \
	compemu_raw_x86.c compemu_support.c cpuopti.c \
	debug.c filesys.c fsdb.c fsdb_unix.c fsusage.c genblitter.c \
	gencpu.c gengenblitter.c genp2c.c gencomp.c hardfile.c \
	hardfile_unix.c linetoscr.c scsi-none.c \
	scsiemul.c svgancui.c tui.c \
	blkdev-libscg.c filesys_unix.c \
	akiko.c akiko2.c ar.c blkdev.c blitops.c

uae_DEPENDENCIES = \
	@MACHDEP@/libmachdep.a		\
	@JOY_DEP@/libjoydep.a		\
	@SND_DEP@/libsnddep.a		\
	@GFX_DEP@/libgfxdep.a		\
	@GUI_DEP@/libguidep.a		\
	keymap/libkeymap.a		\
	dms/libdms.a			\
	caps/libcaps.a			\
	blitfunc.o blittable.o		\
	cpustbl.o cpudefs.o		\
	libcpuemu.a			\
	@FILESYSOBJS@ 			\
	@BSDSOCKOBJS@			\
	@SCSIOBJS@			\
	@CDOBJS@			\
	@DEBUGOBJS@			\
	@ASMOBJS@			\
	@THREADDEP@/libthreaddep.a	\
	@OSDEP@/libosdep.a

uae_LDADD = \
	@MACHDEP@/libmachdep.a		\
	@JOY_DEP@/libjoydep.a		\
	@SND_DEP@/libsnddep.a		\
	@GFX_DEP@/libgfxdep.a		\
	@GUI_DEP@/libguidep.a		\
	keymap/libkeymap.a		\
	dms/libdms.a			\
	caps/libcaps.a			\
	blitfunc.o blittable.o		\
	cpustbl.o cpudefs.o		\
	libcpuemu.a			\
	@FILESYSOBJS@			\
	@BSDSOCKOBJS@			\
	@SCSIOBJS@			\
	@CDOBJS@			\
	@DEBUGOBJS@			\
	@ASMOBJS@			\
	@THREADDEP@/libthreaddep.a	\
	@OSDEP@/libosdep.a


readdisk_SOURCES = \
	readdisk.c missing.c

libcpuemu_a_SOURCES =
libcpuemu_a_LIBADD =		@CPUOBJS@ @JITOBJS@
libcpuemu_a_DEPENDENCIES =	@CPUOBJS@ @JITOBJS@

cpuemu_0.c:	tools/gencpu 
		./tools/gencpu @GENCPUOPTS@

cpustbl.c:	cpuemu_0.c

cputbl.h:	cpuemu_0.c

cpuemu_5.c:	cpuemu_0.c

cpuemu_6.c:	cpuemu_0.c

compemu.c:	tools/gencomp
		./tools/gencomp

compstbl.c:	compemu.c

comptbl.h:	compemu.c


cpufast_nf_%.s: cpuemu_%.c tools/cpuopti
	$(CC) $(INCLUDES) -S $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) -DNOFLAGS $< -o $(@:cpufast_%=cputmp_%)
	./tools/cpuopti $(@:cpufast_%=cputmp_%) >$@
	rm $(@:cpufast_%=cputmp_%)

cpufast_%.s: cpuemu_%.c tools/cpuopti
	$(CC) $(INCLUDES) -S $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) $< -o $(@:cpufast_%=cputmp_%)
	./tools/cpuopti <$(@:cpufast_%=cputmp_%) >$@
	rm $(@:cpufast_%=cputmp_%)

cpuemu_nf_%.o: cpuemu_%.c
	$(CC) $(INCLUDES) -c $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) -DNOFLAGS $< -o $@

cpuemu_%.o: cpuemu_%.c
	$(CC) $(INCLUDES) -c $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) $< -o $@

cpufast_%.o: cpufast_%.s
	$(CC) $(INCLUDES) -c $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) $< -o $@

cpustbl_nf.o: cpustbl.c
	$(CC) $(INCLUDES) -c $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) $(NO_SCHED_CFLAGS) -DNOFLAGS cpustbl.c -o $@


blit.h:		tools/genblitter
		./tools/genblitter i >blit.h

blitfunc.c: 	tools/genblitter blitfunc.h
		./tools/genblitter f >blitfunc.c

blitfunc.h: 	tools/genblitter
		./tools/genblitter h >blitfunc.h

blittable.c: 	tools/genblitter blitfunc.h
		./tools/genblitter t >blittable.c

tools/genblitter:
	$(MAKE) -C tools genblitter
tools/build68k:
	$(MAKE) -C tools build68k
tools/cpuopti:
	$(MAKE) -C tools cpuopti
tools/gencpu:
	$(MAKE) -C tools gencpu
tools/gencomp:
	$(MAKE) -C tools gencomp

cpudefs.c:	tools/build68k @top_srcdir@/src/table68k
		./tools/build68k <@top_srcdir@/src/table68k >cpudefs.c

machdep/libmachdep.a:
	$(MAKE) -C @MACHDEP@ libmachdep.a

gfxdep/libgfxdep.a:
	$(MAKE) -C @GFX_DEP@ libgfxdep.a

sounddep/libsnddep.a:
	$(MAKE) -C @SND_DEP@ libsnddep.a

joydep/libjoydep.a:
	$(MAKE)	-C @JOY_DEP@ libjoydep.a

guidep/libguidep.a:
	$(MAKE) -C @GUI_DEP@ libguidep.a

osdep/libosdep.a:
	$(MAKE) -C @OSDEP@ libosdep.a

osdep/libthreaddep.a:
	$(MAKE) -C @THREADDEP@ libthreaddep.a

keymap/libkeymap.a:
	$(MAKE) -C keymap libkeymap.a

md-X86.o: md-X86.S
	$(CC) -I$(INCLUDES) $(CFLAGS) md-X86.S -c

clean-local:
	$(MAKE) -C tools clean

distclean-local:
	$(MAKE) -C tools distclean
