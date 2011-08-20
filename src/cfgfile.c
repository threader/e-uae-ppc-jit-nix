/*
 * UAE - The Un*x Amiga Emulator
 *
 * Config file handling
 * This still needs some thought before it's complete...
 *
 * Copyright 1998 Brian King, Bernd Schmidt
 * Copyright 2006 Richard Drummond
 * Copyright 2008 Mustafa Tufan
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>

#include "options.h"
#include "uae.h"
#include "audio.h"
#include "filesys.h"
#include "events.h"
#include "custom.h"
#include "inputdevice.h"
#include "gfxfilter.h"
#include "gfxdep/gfx.h"
#include "sounddep/sound.h"
#include "savestate.h"
#include "memory.h"
#include "rommgr.h"
#include "gui.h"
#include "newcpu.h"
#include "zfile.h"
#include "fsdb.h"
#include "disk.h"
#include "blkdev.h"
#include "statusline.h"

static int config_newfilesystem;
static struct strlist *temp_lines;
static struct zfile *default_file;
static int uaeconfig;
static int unicode_config = 0;

/* @@@ need to get rid of this... just cut part of the manual and print that
* as a help text.  */
struct cfg_lines
{
	const TCHAR *config_label, *config_help;
};

/* For formatting help output - should be done dynamically */
#define MAX_OPTION_KEY_LEN  23

static const struct cfg_lines opttable[] =
{
	{"help", "Prints this help" },
    {"config_description", "" },
    {"config_info", "" },
    {"use_gui", "Enable the GUI?  If no, then goes straight to emulator" },
#ifdef DEBUGGER
    {"use_debugger", "Enable the debugger?" },
#endif
    {"cpu_speed", "can be max, real, or a number between 1 and 20" },
    {"cpu_model", "Can be 68000, 68010, 68020, 68030, 68040, 68060" },
    {"fpu_model", "Can be 68881, 68882, 68040, 68060" },
    {"cpu_compatible", "yes enables compatibility-mode" },
    {"cpu_24bit_addressing", "must be set to 'no' in order for Z3mem or P96mem to work" },
    {"autoconfig", "yes = add filesystems and extra ram" },
    {"log_illegal_mem", "print illegal memory access by Amiga software?" },
    {"fastmem_size", "Size in megabytes of fast-memory" },
    {"chipmem_size", "Size in megabytes of chip-memory" },
    {"bogomem_size", "Size in megabytes of bogo-memory at 0xC00000" },
    {"a3000mem_size", "Size in megabytes of A3000 memory" },
    {"gfxcard_size", "Size in megabytes of Picasso96 graphics-card memory" },
    {"z3mem_size", "Size in megabytes of Zorro-III expansion memory" },
    {"gfx_test_speed", "Test graphics speed?" },
    {"gfx_framerate", "Print every nth frame" },
    {"gfx_width", "Screen width" },
    {"gfx_height", "Screen height" },
    {"gfx_refreshrate", "Fullscreen refresh rate" },
    {"gfx_vsync", "Sync screen refresh to refresh rate" },
    {"gfx_lores", "Treat display as lo-res?" },
    {"gfx_linemode", "Can be none, double, or scanlines" },
    {"gfx_fullscreen_amiga", "Amiga screens are fullscreen?" },
    {"gfx_fullscreen_picasso", "Picasso screens are fullscreen?" },
    {"gfx_center_horizontal", "Center display horizontally?" },
    {"gfx_center_vertical", "Center display vertically?" },
    {"gfx_colour_mode", "" },
    {"32bit_blits", "Enable 32 bit blitter emulation" },
    {"immediate_blits", "Perform blits immediately" },
    {"hide_cursor", "Whether to hide host window manager's cursor"},
    {"show_leds", "LED display" },
    {"keyboard_leds", "Keyboard LEDs" },
    {"gfxlib_replacement", "Use graphics.library replacement?" },
    {"sound_output", "" },
    {"sound_frequency", "" },
    {"sound_bits", "" },
    {"sound_channels", "" },
    {"sound_max_buff", "" },
#ifdef JIT
    {"comp_trustbyte", "How to access bytes in compiler (direct/indirect/indirectKS/afterPic" },
    {"comp_trustword", "How to access words in compiler (direct/indirect/indirectKS/afterPic" },
    {"comp_trustlong", "How to access longs in compiler (direct/indirect/indirectKS/afterPic" },
    {"comp_nf", "Whether to optimize away flag generation where possible" },
    {"comp_fpu", "Whether to provide JIT FPU emulation" },
    {"compforcesettings", "Whether to force the JIT compiler settings" },
    {"cachesize", "How many MB to use to buffer translated instructions"},
#endif
    {"override_dga_address", "Address from which to map the frame buffer (upper 16 bits) (DANGEROUS!)"},
    {"avoid_cmov", "Set to yes on machines that lack the CMOV instruction" },
    {"avoid_dga", "Set to yes if the use of DGA extension creates problems" },
    {"avoid_vid", "Set to yes if the use of the Vidmode extension creates problems" },
    {"parallel_on_demand", "" },
    {"serial_on_demand", "" },
    {"scsi", "scsi.device emulation" },
    {"joyport0", "" },
    {"joyport1", "" },
    {"pci_devices", "List of PCI devices to make visible to the emulated Amiga" },
    {"kickstart_rom_file", "Kickstart ROM image, (C) Copyright Amiga, Inc." },
    {"kickstart_ext_rom_file", "Extended Kickstart ROM image, (C) Copyright Amiga, Inc." },
    {"kickstart_key_file", "Key-file for encrypted ROM images (from Cloanto's Amiga Forever)" },
    {"flash_ram_file", "Flash/battery backed RAM image file." },
#ifdef ACTION_REPLAY
    {"cart_file", "Freezer cartridge ROM image file." },
#endif
    {"floppy0", "Diskfile for drive 0" },
    {"floppy1", "Diskfile for drive 1" },
    {"floppy2", "Diskfile for drive 2" },
    {"floppy3", "Diskfile for drive 3" },
#ifdef FILESYS
    {"hardfile", "access,sectors, surfaces, reserved, blocksize, path format" },
    {"filesystem", "access,'Amiga volume-name':'host directory path' - where 'access' can be 'read-only' or 'read-write'" },
#endif
#ifdef CATWEASEL
    {"catweasel","Catweasel board io base address" }
#endif
};

static const TCHAR *guimode1[] = { "no", "yes", "nowait", 0 };
static const TCHAR *guimode2[] = { "false", "true", "nowait", 0 };
static const TCHAR *guimode3[] = { "0", "1", "nowait", 0 };
static const TCHAR *csmode[] = { "ocs", "ecs_agnus", "ecs_denise", "ecs", "aga", 0 };
static const TCHAR *linemode[] = { "none", "none", "double", "scanlines", 0 };
static const TCHAR *speedmode[] = { "max", "real", 0 };
static const TCHAR *colormode1[] = { "8bit", "15bit", "16bit", "8bit_dither", "4bit_dither", "32bit", 0 };
static const TCHAR *colormode2[] = { "8", "15", "16", "8d", "4d", "32", 0 };
static const TCHAR *soundmode1[] = { "none", "interrupts", "normal", "exact", 0 };
static const TCHAR *soundmode2[] = { "none", "interrupts", "good", "best", 0 };
static const TCHAR *centermode1[] = { "none", "simple", "smart", 0 };
static const TCHAR *centermode2[] = { "false", "true", "smart", 0 };
static const TCHAR *stereomode[] = { "mono", "stereo", "clonedstereo", "4ch", "clonedstereo6ch", "6ch", "mixed", 0 };
static const TCHAR *interpolmode[] = { "none", "anti", "sinc", "rh", "crux", 0 };
static const TCHAR *collmode[] = { "none", "sprites", "playfields", "full", 0 };
static const TCHAR *compmode[] = { "direct", "indirect", "indirectKS", "afterPic", 0 };
static const TCHAR *flushmode[] = { "soft", "hard", 0 };
static const TCHAR *kbleds[] = { "none", "POWER", "DF0", "DF1", "DF2", "DF3", "HD", "CD", 0 };
static const TCHAR *onscreenleds[] = { "false", "true", "rtg", "both", 0 };
static const TCHAR *soundfiltermode1[] = { "off", "emulated", "on", 0 };
static const TCHAR *soundfiltermode2[] = { "standard", "enhanced", 0 };
static const TCHAR *lorestype1[] = { "lores", "hires", "superhires", 0 };
static const TCHAR *lorestype2[] = { "true", "false", 0 };
static const TCHAR *loresmode[] = { "normal", "filtered", 0 };
static const TCHAR *vertmode[] = { "single", "double", "quadruple", 0 };
#ifdef GFXFILTER
static const TCHAR *filtermode2[] = { "1x", "2x", "3x", "4x", 0 };
#endif
static const TCHAR *cartsmode[] = { "none", "hrtmon", 0 };
static const TCHAR *idemode[] = { "none", "a600/a1200", "a4000", 0 };
static const TCHAR *rtctype[] = { "none", "MSM6242B", "RP5C01A", 0 };
static const TCHAR *ciaatodmode[] = { "vblank", "50hz", "60hz", 0 };
static const TCHAR *ksmirrortype[] = { "none", "e0", "a8+e0", 0 };
static const TCHAR *cscompa[] = {
	"-", "Generic", "CDTV", "CD32", "A500", "A500+", "A600",
	"A1000", "A1200", "A2000", "A3000", "A3000T", "A4000", "A4000T", 0
};
static const TCHAR *qsmodes[] = {
	"A500", "A500+", "A600", "A1000", "A1200", "A3000", "A4000", "", "CD32", "CDTV", "ARCADIA", NULL };
/* 3-state boolean! */
static const TCHAR *fullmodes[] = { "false", "true", /* "FILE_NOT_FOUND", */ "fullwindow", 0 };
/* bleh for compatibility */
static const TCHAR *scsimode[] = { "false", "true", "scsi", 0 };
static const TCHAR *maxhoriz[] = { "lores", "hires", "superhires", 0 };
static const TCHAR *maxvert[] = { "nointerlace", "interlace", 0 };
static const TCHAR *abspointers[] = { "none", "mousehack", "tablet", 0 };
static const TCHAR *magiccursors[] = { "both", "native", "host", 0 };
static const TCHAR *autoscale[] = { "none", "auto", "standard", "max", "scale", "resize", "center", "manual", 0 };
static const TCHAR *joyportmodes[] = { "", "mouse", "djoy", "gamepad", "ajoy", "cdtvjoy", "cd32joy", "lightpen", 0 };
static const TCHAR *joyaf[] = { "none", "normal", "toggle", 0 };
static const TCHAR *epsonprinter[] = { "none", "ascii", "epson_matrix_9pin", "epson_matrix_24pin", "epson_matrix_48pin", 0 };
static const TCHAR *aspects[] = { "none", "vga", "tv", 0 };
static const TCHAR *vsyncmodes[] = { "false", "true", "autoswitch", 0 };
static const TCHAR *vsyncmodes2[] = { "normal", "busywait", 0 };
static const TCHAR *filterapi[] = { "directdraw", "direct3d", 0 };
static const TCHAR *dongles[] =
{
	"none",
	"robocop 3", "leaderboard", "b.a.t. ii", "italy'90 soccer", "dames grand maitre",
	"rugby coach", "cricket captain", "leviathan",
	NULL
};
static const TCHAR *cdmodes[] = { "disabled", "", "image", "ioctl", "spti", "aspi", 0 };
static const TCHAR *cdconmodes[] = { "", "uae", "ide", "scsi", "cdtv", "cd32", 0 };

static const TCHAR *obsolete[] = {
	"accuracy", "gfx_opengl", "gfx_32bit_blits", "32bit_blits",
	"gfx_immediate_blits", "gfx_ntsc", "win32", "gfx_filter_bits",
	"sound_pri_cutoff", "sound_pri_time", "sound_min_buff", "sound_bits",
	"gfx_test_speed", "gfxlib_replacement", "enforcer", "catweasel_io",
	"kickstart_key_file", "fast_copper", "sound_adjust",
	"serial_hardware_dtrdsr", "gfx_filter_upscale",
	"gfx_correct_aspect", "gfx_autoscale", "parallel_sampler", "parallel_ascii_emulation",
	"avoid_vid", "avoid_dga", "z3chipmem_size", "state_replay_buffer", "state_replay",
	NULL
};

#define UNEXPANDED "$(FILE_PATH)"

static void trimwsa (char *s)
{
	/* Delete trailing whitespace.  */
	int len = strlen (s);
	while (len > 0 && strcspn (s + len - 1, "\t \r\n") == 0)
		s[--len] = '\0';
}

static int match_string (const TCHAR *table[], const TCHAR *str)
{
	int i;
	for (i = 0; table[i] != 0; i++)
		if (strcasecmp (table[i], str) == 0)
			return i;
	return -1;
}

static TCHAR *cfgfile_subst_path2 (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
	/* @@@ use strcasecmp for some targets.  */
	if (_tcslen (path) > 0 && _tcsncmp (file, path, _tcslen (path)) == 0) {
		int l;
		TCHAR *p2, *p = xmalloc (TCHAR, _tcslen (file) + _tcslen (subst) + 2);
		_tcscpy (p, subst);
		l = _tcslen (p);
		while (l > 0 && p[l - 1] == '/')
			p[--l] = '\0';
		l = _tcslen (path);
		while (file[l] == '/')
			l++;
		_tcscat (p, "/");
		_tcscat (p, file + l);
		return p;
	}
	return NULL;
}

TCHAR *cfgfile_subst_path (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
	TCHAR *s = cfgfile_subst_path2 (path, subst, file);
	if (s)
		return s;
/*
	s = target_expand_environment (file);
	if (s) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, s);
		xfree (s);
		fullpath (tmp, sizeof tmp / sizeof (TCHAR));
		s = my_strdup (tmp);
	}*/
	return file;
	return s;
}

static TCHAR *cfgfile_get_multipath2 (struct multipath *mp, const TCHAR *path, const TCHAR *file, bool dir)
{
        unsigned int i;

	for (i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], ".\\") != 0 && _tcscmp (mp->path[i], "./") != 0 && (file[0] != '/' && file[0] != '\\' && !_tcschr(file, ':'))) {
			TCHAR *s = NULL;
			if (path)
				s = cfgfile_subst_path2 (path, mp->path[i], file);
			if (!s) {
				TCHAR np[MAX_DPATH];
				_tcscpy (np, mp->path[i]);
				fixtrailing (np);
				_tcscat (np, file);
				fullpath (np, sizeof np / sizeof (TCHAR));
				s = my_strdup (np);
			}
			if (dir) {
				if (my_existsdir (s))
					return s;
			} else {
				if (zfile_exists (s))
					return s;
			}
			xfree (s);
		}
	}
	return NULL;
}

static TCHAR *cfgfile_get_multipath (struct multipath *mp, const TCHAR *path, const TCHAR *file, bool dir)
{
	TCHAR *s = cfgfile_get_multipath2 (mp, path, file, dir);
	if (s)
		return s;
	return my_strdup (file);
}

static TCHAR *cfgfile_put_multipath (struct multipath *mp, const TCHAR *s)
{
        unsigned int i;

	for (i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], ".\\") != 0 && _tcscmp (mp->path[i], "./") != 0) {
			if (_tcsnicmp (mp->path[i], s, _tcslen (mp->path[i])) == 0) {
				return my_strdup (s + _tcslen (mp->path[i]));
			}
		}
	}
	return my_strdup (s);
}

static TCHAR *cfgfile_subst_path_load (const TCHAR *path, struct multipath *mp, const TCHAR *file, bool dir)
{
	TCHAR *s = cfgfile_get_multipath2 (mp, path, file, dir);
	if (s)
		return s;
	return cfgfile_subst_path (path, mp->path[0], file);
}

static int isdefault (const TCHAR *s)
{
	TCHAR tmp[MAX_DPATH];
	if (!default_file || uaeconfig)
		return 0;
	zfile_fseek (default_file, 0, SEEK_SET);
	while (zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), default_file)) {
		if (!_tcscmp (tmp, s))
			return 1;
	}
	return 0;
}

static size_t cfg_write (void *b, struct zfile *z)
{
	size_t v;
	if (unicode_config) {
		TCHAR lf = 10;
		v = zfile_fwrite (b, _tcslen ((TCHAR*)b), sizeof (TCHAR), z);
		zfile_fwrite (&lf, 1, 1, z);
	} else {
		char lf = 10;
		char *s = strdup ((TCHAR*)b);
		v = zfile_fwrite (s, strlen (s), 1, z);
		zfile_fwrite (&lf, 1, 1, z);
		xfree (s);
	}
	return v;
}

#define UTF8NAME ".utf8"

static void cfg_dowrite (struct zfile *f, const TCHAR *option, const TCHAR *value, int d, int target)
{
	char lf = 10;
	TCHAR tmp[CONFIG_BLEN];

	if (target)
		_stprintf (tmp, "%s.%s=%s", TARGET_NAME, option, value);
	else
		_stprintf (tmp, "%s=%s", option, value);
	if (d && isdefault (tmp))
		goto end;
	cfg_write (tmp, f);

end:
	;
}

void cfgfile_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? "true" : "false", 0, 0);
}
void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? "true" : "false", 1, 0);
}
/*void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, int b)
{
	cfgfile_dwrite_bool (f, option, b != 0);
}*/
void cfgfile_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 0, 0);
}
void cfgfile_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 1, 0);
}

void cfgfile_target_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? "true" : "false", 0, 1);
}
void cfgfile_target_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? "true" : "false", 1, 1);
}
void cfgfile_target_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 0, 1);
}
void cfgfile_target_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 1, 1);
}

void cfgfile_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 0, 0);
	va_end (parms);
}
void cfgfile_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 1, 0);
	va_end (parms);
}
void cfgfile_target_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 0, 1);
	va_end (parms);
}
void cfgfile_target_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 1, 1);
	va_end (parms);
}

static void cfgfile_write_rom (struct zfile *f, struct multipath *mp, const TCHAR *romfile, const TCHAR *name)
{
	TCHAR *str = cfgfile_subst_path (mp->path[0], UNEXPANDED, romfile);
	str = cfgfile_put_multipath (mp, str);
	cfgfile_write_str (f, name, str);
	struct zfile *zf = zfile_fopen (str, "rb", ZFD_ALL);
	if (zf) {
		struct romdata *rd = getromdatabyzfile (zf);
		if (rd) {
			TCHAR name2[MAX_DPATH], str2[MAX_DPATH];
			_tcscpy (name2, name);
			_tcscat (name2, "_id");
			_stprintf (str2, "%08X,%s", rd->crc32, rd->name);
			cfgfile_write_str (f, name2, str2);
		}
		zfile_fclose (zf);
	}
	xfree (str);

}

static void cfgfile_write_path (struct zfile *f, struct multipath *mp, const TCHAR *option, const TCHAR *value)
{
	TCHAR *s = cfgfile_put_multipath (mp, value);
	cfgfile_write_str (f, option, s);
	xfree (s);
}
static void cfgfile_dwrite_path (struct zfile *f, struct multipath *mp, const TCHAR *option, const TCHAR *value)
{
	TCHAR *s = cfgfile_put_multipath (mp, value);
	cfgfile_dwrite_str (f, option, s);
	xfree (s);
}

static void write_filesys_config (struct uae_prefs *p, struct zfile *f)
{
	int i;
	TCHAR tmp[MAX_DPATH], tmp2[MAX_DPATH];
	TCHAR *hdcontrollers[] = { "uae",
		"ide0", "ide1", "ide2", "ide3",
		"scsi0", "scsi1", "scsi2", "scsi3", "scsi4", "scsi5", "scsi6",
		"scsram", "scside" }; /* scsram = smart card sram = pcmcia sram card */

	for (i = 0; i < p->mountitems; i++) {
		struct uaedev_config_info *uci = &p->mountconfig[i];
		TCHAR *str;
		int bp = uci->bootpri;

		if (!uci->autoboot)
			bp = -128;
		if (uci->donotmount)
			bp = -129;
		str = cfgfile_put_multipath (&p->path_hardfile, uci->rootdir);
		if (!uci->ishdf) {
			_stprintf (tmp, "%s,%s:%s:%s,%d", uci->readonly ? "ro" : "rw",
				uci->devname ? uci->devname : "", uci->volname, str, bp);
			cfgfile_write_str (f, "filesystem2", tmp);
		} else {
			_stprintf (tmp, "%s,%s:%s,%d,%d,%d,%d,%d,%s,%s",
				uci->readonly ? "ro" : "rw",
				uci->devname ? uci->devname : "", str,
				uci->sectors, uci->surfaces, uci->reserved, uci->blocksize,
				bp, uci->filesys ? uci->filesys : "", hdcontrollers[uci->controller]);
			cfgfile_write_str (f, "hardfile2", tmp);
		}
		_stprintf (tmp2, "uaehf%d", i);
		cfgfile_write (f, tmp2, "%s,%s", uci->ishdf ? "hdf" : "dir", tmp);
		xfree (str);
	}
}

static void subst_home (char *f, int n)
{
    const char *home = getenv ("HOME");

    if (home) {
	char *str = cfgfile_subst_path ("~", home, f);
	strncpy (f, str, n - 1);
	f[n - 1] = '\0';
	//FIXME: free (str);
    }
}

void cfgfile_subst_home (char *path, unsigned int maxlen)
{
    subst_home (path, maxlen);
}

void do_cfgfile_write (FILE *f, const char *format,...)
{
    va_list parms;
    char tmp[CONFIG_BLEN];

    va_start (parms, format);
    vsprintf (tmp, format, parms);
    fprintf (f, tmp);
    va_end (parms);
}

static void cfgfile_write_path_option (FILE *f, const char *key)
{
    const char *home = getenv ("HOME");
    const char *path = "./";
    char *out_path = 0;

    if (path)
		out_path = cfgfile_subst_path (home, "~", path);

	cfgfile_write (f, "%s.%s=%s\n", TARGET_NAME, key, out_path ? out_path : "");

    if (out_path)
	free (out_path);
}

static void write_compatibility_cpu (struct zfile *f, struct uae_prefs *p)
{
	TCHAR tmp[100];
	int model;

	model = p->cpu_model;
	if (model == 68030)
		model = 68020;
	if (model == 68060)
		model = 68040;
	if (p->address_space_24 && model == 68020)
		_tcscpy (tmp, "68ec020");
	else
		_stprintf (tmp, "%d", model);
	if (model == 68020 && (p->fpu_model == 68881 || p->fpu_model == 68882))
		_tcscat (tmp, "/68881");
	cfgfile_write (f, "cpu_type", tmp);
}

void cfgfile_save_options (struct zfile *f, struct uae_prefs *p, int type)
{
	struct strlist *sl;
	TCHAR *str, tmp[MAX_DPATH];
	int i;

	cfgfile_write_str (f, "config_description", p->description);
	cfgfile_write_bool (f, "config_hardware", type & CONFIG_TYPE_HARDWARE);
	cfgfile_write_bool (f, "config_host", !!(type & CONFIG_TYPE_HOST));
	if (p->info[0])
		cfgfile_write (f, "config_info", p->info);
	cfgfile_write (f, "config_version", "%d.%d.%d", UAEMAJOR, UAEMINOR, UAESUBREV);
	cfgfile_write_str (f, "config_hardware_path", p->config_hardware_path);
	cfgfile_write_str (f, "config_host_path", p->config_host_path);
	cfgfile_write_str (f, "config_window_title", p->config_window_title);

	for (sl = p->all_lines; sl; sl = sl->next) {
		if (sl->unknown) {
			if (sl->option)
				cfgfile_write_str (f, sl->option, sl->value);
		}
	}

	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_rom.path[i][0]) {
			_stprintf (tmp, "%s.rom_path", TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_rom.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_floppy.path[i][0]) {
			_stprintf (tmp, "%s.floppy_path", TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_floppy.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_hardfile.path[i][0]) {
			_stprintf (tmp, "%s.hardfile_path", TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_hardfile.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_cd.path[i][0]) {
			_stprintf (tmp, "%s.cd_path", TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_cd.path[i]);
		}
	}

	cfg_write ("; host-specific", f);

	target_save_options (f, p);

	cfg_write ("; common", f);

	cfgfile_write_str (f, "use_gui", guimode1[p->start_gui]);
#ifdef DEBUGGER
	cfgfile_write_bool (f, "use_debugger", p->start_debugger);
#endif
	cfgfile_write_rom (f, &p->path_rom, p->romfile, "kickstart_rom_file");
	cfgfile_write_rom (f, &p->path_rom, p->romextfile, "kickstart_ext_rom_file");
	if (p->romextfile2addr) {
		cfgfile_write (f, "kickstart_ext_rom_file2_address", "%x", p->romextfile2addr);
		cfgfile_write_rom (f, &p->path_rom, p->romextfile2, "kickstart_ext_rom_file2");
	}
	if (p->romident[0])
		cfgfile_dwrite_str (f, "kickstart_rom", p->romident);
	if (p->romextident[0])
		cfgfile_write_str (f, "kickstart_ext_rom=", p->romextident);
	cfgfile_write_path (f, &p->path_rom, "flash_file", p->flashfile);
	cfgfile_write_path (f, &p->path_rom, "cart_file", p->cartfile);
#ifdef ACTION_REPLAY
	if (p->cartident[0])
		cfgfile_write_str (f, "cart", p->cartident);
	if (p->amaxromfile[0])
		cfgfile_write_path (f, &p->path_rom, "amax_rom_file", p->amaxromfile);
#endif

	cfgfile_write_bool (f, "kickshifter", p->kickshifter);

	p->nr_floppies = 4;
	for (i = 0; i < 4; i++) {
		_stprintf (tmp, "floppy%d", i);
		cfgfile_write_path (f, &p->path_floppy, tmp, p->floppyslots[i].df);
		_stprintf (tmp, "floppy%dtype", i);
		cfgfile_dwrite (f, tmp, "%d", p->floppyslots[i].dfxtype);
#ifdef DRIVESOUND
		_stprintf (tmp, "floppy%dsound", i);
		cfgfile_dwrite (f, tmp, "%d", p->floppyslots[i].dfxclick);
		if (p->floppyslots[i].dfxclick < 0 && p->floppyslots[i].dfxclickexternal[0]) {
			_stprintf (tmp, "floppy%dsoundext", i);
			cfgfile_dwrite (f, tmp, p->floppyslots[i].dfxclickexternal);
		}
#endif
		if (p->floppyslots[i].dfxtype < 0 && p->nr_floppies > i)
			p->nr_floppies = i;
	}
	for (i = 0; i < MAX_SPARE_DRIVES; i++) {
		if (p->dfxlist[i][0]) {
			_stprintf (tmp, "diskimage%d", i);
			cfgfile_dwrite_path (f, &p->path_floppy, tmp, p->dfxlist[i]);
		}
	}

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		if (p->cdslots[i].name[0] || p->cdslots[i].inuse) {
			TCHAR tmp2[MAX_DPATH];
			_stprintf (tmp, "cdimage%d", i);
			TCHAR *s = cfgfile_put_multipath (&p->path_cd, p->cdslots[i].name);
			_tcscpy (tmp2, s);
			xfree (s);
			if (p->cdslots[i].type != SCSI_UNIT_DEFAULT || _tcschr (p->cdslots[i].name, ',') || p->cdslots[i].delayed) {
				_tcscat (tmp2, ",");
				if (p->cdslots[i].delayed) {
					_tcscat (tmp2, "delay");
					_tcscat (tmp2, ":");
				}
				if (p->cdslots[i].type != SCSI_UNIT_DEFAULT) {
					_tcscat (tmp2, cdmodes[p->cdslots[i].type + 1]);
				}
			}
			cfgfile_write_str (f, tmp, tmp2);
		}
	}

	if (p->statefile[0])
		cfgfile_write_str (f, "statefile", p->statefile);
	if (p->quitstatefile[0])
		cfgfile_write_str (f, "statefile_quit", p->quitstatefile);

	cfgfile_write (f, "nr_floppies", "%d", p->nr_floppies);
	cfgfile_write (f, "floppy_speed", "%d", p->floppy_speed);
#ifdef DRIVESOUND
	cfgfile_write (f, "floppy_volume", "%d", p->dfxclickvolume);
	cfgfile_dwrite (f, "floppy_channel_mask", "0x%x", p->dfxclickchannelmask);
#endif
	cfgfile_write_bool (f, "parallel_on_demand", p->parallel_demand);
	cfgfile_write_bool (f, "serial_on_demand", p->serial_demand);
	cfgfile_write_bool (f, "serial_hardware_ctsrts", p->serial_hwctsrts);
	cfgfile_write_bool (f, "serial_direct", p->serial_direct);
	cfgfile_dwrite (f, "serial_stopbits", "%d", p->serial_stopbits);
	cfgfile_write_str (f, "scsi", scsimode[p->scsi]);
	cfgfile_write_bool (f, "uaeserial", p->uaeserial);
	cfgfile_write_bool (f, "sana2", p->sana2);

	cfgfile_write_str (f, "sound_output", soundmode1[p->produce_sound]);
	cfgfile_write_str (f, "sound_channels", stereomode[p->sound_stereo]);
	cfgfile_write (f, "sound_stereo_separation", "%d", p->sound_stereo_separation);
	cfgfile_write (f, "sound_stereo_mixing_delay", "%d", p->sound_mixed_stereo_delay >= 0 ? p->sound_mixed_stereo_delay : 0);
	cfgfile_write (f, "sound_max_buff", "%d", p->sound_maxbsiz);
	cfgfile_write (f, "sound_frequency", "%d", p->sound_freq);
	cfgfile_write (f, "sound_latency", "%d", p->sound_latency);
	cfgfile_write_str (f, "sound_interpol", interpolmode[p->sound_interpol]);
	cfgfile_write_str (f, "sound_filter", soundfiltermode1[p->sound_filter]);
	cfgfile_write_str (f, "sound_filter_type", soundfiltermode2[p->sound_filter_type]);
	cfgfile_write (f, "sound_volume", "%d", p->sound_volume);
	if (p->sound_volume_cd >= 0)
		cfgfile_write (f, "sound_volume_cd", "%d", p->sound_volume_cd);
	cfgfile_write_bool (f, "sound_auto", p->sound_auto);
	cfgfile_write_bool (f, "sound_stereo_swap_paula", p->sound_stereo_swap_paula);
	cfgfile_write_bool (f, "sound_stereo_swap_ahi", p->sound_stereo_swap_ahi);

#ifdef JIT
	cfgfile_write_str (f, "comp_trustbyte", compmode[p->comptrustbyte]);
	cfgfile_write_str (f, "comp_trustword", compmode[p->comptrustword]);
	cfgfile_write_str (f, "comp_trustlong", compmode[p->comptrustlong]);
	cfgfile_write_str (f, "comp_trustnaddr", compmode[p->comptrustnaddr]);
	cfgfile_write_bool (f, "comp_nf", p->compnf);
	cfgfile_write_bool (f, "comp_constjump", p->comp_constjump);
	cfgfile_write_bool (f, "comp_oldsegv", p->comp_oldsegv);

	cfgfile_write_str (f, "comp_flushmode", flushmode[p->comp_hardflush]);
	cfgfile_write_bool (f, "compfpu", p->compfpu);
	cfgfile_write_bool (f, "fpu_strict", p->fpu_strict);
	cfgfile_write_bool (f, "comp_midopt", p->comp_midopt);
	cfgfile_write_bool (f, "comp_lowopt", p->comp_lowopt);
	cfgfile_write_bool (f, "avoid_cmov", p->avoid_cmov);
	cfgfile_write (f, "cachesize", "%d", p->cachesize);
#endif

	for (i = 0; i < MAX_JPORTS; i++) {
		struct jport *jp = &p->jports[i];
		int v = jp->id;
		TCHAR tmp1[MAX_DPATH], tmp2[MAX_DPATH];
		if (v == JPORT_CUSTOM) {
			_tcscpy (tmp2, "custom");
		} else if (v == JPORT_NONE) {
			_tcscpy (tmp2, "none");
		} else if (v < JSEM_JOYS) {
			_stprintf (tmp2, "kbd%d", v + 1);
		} else if (v < JSEM_MICE) {
			_stprintf (tmp2, "joy%d", v - JSEM_JOYS);
		} else {
			_tcscpy (tmp2, "mouse");
			if (v - JSEM_MICE > 0)
				_stprintf (tmp2, "mouse%d", v - JSEM_MICE);
		}
		if (i < 2 || jp->id >= 0) {
			_stprintf (tmp1, "joyport%d", i);
			cfgfile_write (f, tmp1, tmp2);
			_stprintf (tmp1, "joyport%dautofire", i);
			cfgfile_write (f, tmp1, joyaf[jp->autofire]);
			if (i < 2 && jp->mode > 0) {
				_stprintf (tmp1, "joyport%dmode", i);
				cfgfile_write (f, tmp1, joyportmodes[jp->mode]);
			}
			if (jp->name[0]) {
				_stprintf (tmp1, "joyportfriendlyname%d", i);
				cfgfile_write (f, tmp1, jp->name);
			}
			if (jp->configname[0]) {
				_stprintf (tmp1, "joyportname%d", i);
				cfgfile_write (f, tmp1, jp->configname);
			}
		}
	}
	if (p->dongle) {
		if (p->dongle + 1 >= sizeof (dongles) / sizeof (TCHAR*))
			cfgfile_write (f, "dongle", "%d", p->dongle);
		else
			cfgfile_write_str (f, "dongle", dongles[p->dongle]);
	}

	cfgfile_write_bool (f, "bsdsocket_emu", p->socket_emu);
	if (p->a2065name[0])
		cfgfile_write_str (f, "a2065", p->a2065name);

	cfgfile_write_bool (f, "synchronize_clock", p->tod_hack);
	cfgfile_write (f, "maprom", "0x%x", p->maprom);
	cfgfile_dwrite_str (f, "parallel_matrix_emulation", epsonprinter[p->parallel_matrix_emulation]);
	cfgfile_write_bool (f, "parallel_postscript_emulation", p->parallel_postscript_emulation);
	cfgfile_write_bool (f, "parallel_postscript_detection", p->parallel_postscript_detection);
	cfgfile_write_str (f, "ghostscript_parameters", p->ghostscript_parameters);
	cfgfile_write (f, "parallel_autoflush", "%d", p->parallel_autoflush_time);
	cfgfile_dwrite (f, "uae_hide", "%d", p->uae_hide);
	cfgfile_dwrite_bool (f, "magic_mouse", p->input_magic_mouse);
	cfgfile_dwrite_str (f, "magic_mousecursor", magiccursors[p->input_magic_mouse_cursor]);
	cfgfile_dwrite_str (f, "absolute_mouse", abspointers[p->input_tablet]);
	cfgfile_dwrite_bool (f, "clipboard_sharing", p->clipboard_sharing);

	cfgfile_write (f, "gfx_display", "%d", p->gfx_display);
	cfgfile_write_str (f, "gfx_display_name", p->gfx_display_name);
	cfgfile_write (f, "gfx_framerate", "%d", p->gfx_framerate);
	cfgfile_write (f, "gfx_width", "%d", p->gfx_size_win.width); /* compatibility with old versions */
	cfgfile_write (f, "gfx_height", "%d", p->gfx_size_win.height); /* compatibility with old versions */
	cfgfile_write (f, "gfx_top_windowed", "%d", p->gfx_size_win.x);
	cfgfile_write (f, "gfx_left_windowed", "%d", p->gfx_size_win.y);
	cfgfile_write (f, "gfx_width_windowed", "%d", p->gfx_size_win.width);
	cfgfile_write (f, "gfx_height_windowed", "%d", p->gfx_size_win.height);
	cfgfile_write (f, "gfx_width_fullscreen", "%d", p->gfx_size_fs.width);
	cfgfile_write (f, "gfx_height_fullscreen", "%d", p->gfx_size_fs.height);
	cfgfile_write (f, "gfx_refreshrate", "%d", p->gfx_refreshrate);
	cfgfile_write_bool (f, "gfx_autoresolution", p->gfx_autoresolution);
	cfgfile_dwrite (f, "gfx_autoresolution_min_vertical", lorestype1[p->gfx_autoresolution_minv]);
	cfgfile_dwrite (f, "gfx_autoresolution_min_horizontal", vertmode[p->gfx_autoresolution_minh]);

	cfgfile_write (f, "gfx_backbuffers", "%d", p->gfx_backbuffers);
	cfgfile_write_str (f, "gfx_vsync", vsyncmodes[p->gfx_avsync]);
	cfgfile_write_str (f, "gfx_vsyncmode", vsyncmodes2[p->gfx_avsyncmode]);
	cfgfile_write_str (f, "gfx_vsync_picasso", vsyncmodes[p->gfx_pvsync]);
	cfgfile_write_str (f, "gfx_vsyncmode_picasso", vsyncmodes2[p->gfx_pvsyncmode]);
	cfgfile_write_bool (f, "gfx_lores", p->gfx_resolution == 0);
	cfgfile_write_str (f, "gfx_resolution", lorestype1[p->gfx_resolution]);
	cfgfile_write_str (f, "gfx_lores_mode", loresmode[p->gfx_lores_mode]);
	cfgfile_write_bool (f, "gfx_flickerfixer", p->gfx_scandoubler);
	cfgfile_write_str (f, "gfx_linemode", linemode[p->gfx_vresolution * 2 + p->gfx_scanlines]);
	cfgfile_write_str (f, "gfx_fullscreen_amiga", fullmodes[p->gfx_afullscreen]);
	cfgfile_write_str (f, "gfx_fullscreen_picasso", fullmodes[p->gfx_pfullscreen]);
	cfgfile_write_str (f, "gfx_center_horizontal", centermode1[p->gfx_xcenter]);
	cfgfile_write_str (f, "gfx_center_vertical", centermode1[p->gfx_ycenter]);
	cfgfile_write_str (f, "gfx_colour_mode", colormode1[p->color_mode]);
	cfgfile_write_bool (f, "gfx_blacker_than_black", p->gfx_blackerthanblack);
	cfgfile_write_str (f, "gfx_api", filterapi[p->gfx_api]);

#ifdef GFXFILTER
	if (p->gfx_filter > 0) {
		int i = 0;
		struct uae_filter *uf;
		while (uaefilters[i].name) {
			uf = &uaefilters[i];
			if (uf->type == p->gfx_filter) {
				cfgfile_dwrite_str (f, "gfx_filter", uf->cfgname);
			}
			i++;
		}
	} else {
		cfgfile_dwrite (f, "gfx_filter", "no");
	}

	cfgfile_dwrite_str (f, "gfx_filter_mode", filtermode2[p->gfx_filter_filtermode]);
	cfgfile_dwrite (f, "gfx_filter_vert_zoom", "%d", p->gfx_filter_vert_zoom);
	cfgfile_dwrite (f, "gfx_filter_horiz_zoom", "%d", p->gfx_filter_horiz_zoom);
	cfgfile_dwrite (f, "gfx_filter_vert_zoom_mult", "%d", p->gfx_filter_vert_zoom_mult);
	cfgfile_dwrite (f, "gfx_filter_horiz_zoom_mult", "%d", p->gfx_filter_horiz_zoom_mult);
	cfgfile_dwrite (f, "gfx_filter_vert_offset", "%d", p->gfx_filter_vert_offset);
	cfgfile_dwrite (f, "gfx_filter_horiz_offset", "%d", p->gfx_filter_horiz_offset);
	cfgfile_dwrite (f, "gfx_filter_scanlines", "%d", p->gfx_filter_scanlines);
	cfgfile_dwrite (f, "gfx_filter_scanlinelevel", "%d", p->gfx_filter_scanlinelevel);
	cfgfile_dwrite (f, "gfx_filter_scanlineratio", "%d", p->gfx_filter_scanlineratio);
	cfgfile_dwrite (f, "gfx_filter_luminance", "%d", p->gfx_filter_luminance);
	cfgfile_dwrite (f, "gfx_filter_contrast", "%d", p->gfx_filter_contrast);
	cfgfile_dwrite (f, "gfx_filter_saturation", "%d", p->gfx_filter_saturation);
	cfgfile_dwrite (f, "gfx_filter_gamma", "%d", p->gfx_filter_gamma);
	cfgfile_dwrite (f, "gfx_filter_blur", "%d", p->gfx_filter_blur);
	cfgfile_dwrite (f, "gfx_filter_noise", "%d", p->gfx_filter_noise);
	cfgfile_dwrite_bool (f, "gfx_filter_bilinear", p->gfx_filter_bilinear != 0);
	cfgfile_dwrite_str (f, "gfx_filter_keep_aspect", aspects[p->gfx_filter_keep_aspect]);
	cfgfile_dwrite_str (f, "gfx_filter_autoscale", autoscale[p->gfx_filter_autoscale]);
	cfgfile_dwrite (f, "gfx_filter_aspect_ratio", "%d:%d",
		p->gfx_filter_aspect >= 0 ? (p->gfx_filter_aspect >> 8) : -1,
		p->gfx_filter_aspect >= 0 ? (p->gfx_filter_aspect & 0xff) : -1);
	cfgfile_dwrite (f, "gfx_luminance", "%d", p->gfx_luminance);
	cfgfile_dwrite (f, "gfx_contrast", "%d", p->gfx_contrast);
	cfgfile_dwrite (f, "gfx_gamma", "%d", p->gfx_gamma);
	cfgfile_dwrite_str (f, "gfx_filter_mask", p->gfx_filtermask);
	if (p->gfx_filteroverlay[0]) {
		cfgfile_dwrite (f, "gfx_filter_overlay", "%s%s",
			p->gfx_filteroverlay, _tcschr (p->gfx_filteroverlay, ',') ? "," : "");
	}

	cfgfile_dwrite (f, "gfx_center_horizontal_position", "%d", p->gfx_xcenter_pos);
	cfgfile_dwrite (f, "gfx_center_vertical_position", "%d", p->gfx_ycenter_pos);
	cfgfile_dwrite (f, "gfx_center_horizontal_size", "%d", p->gfx_xcenter_size);
	cfgfile_dwrite (f, "gfx_center_vertical_size", "%d", p->gfx_ycenter_size);

#endif

	cfgfile_write_bool (f, "immediate_blits", p->immediate_blits);
	cfgfile_write_bool (f, "waiting_blits", p->waiting_blits);
	cfgfile_write_bool (f, "ntsc", p->ntscmode);
	cfgfile_write_bool (f, "genlock", p->genlock);
	cfgfile_dwrite_bool (f, "show_leds", !!(p->leds_on_screen & STATUSLINE_CHIPSET));
	if (p->osd_pos.y || p->osd_pos.x) {
		cfgfile_dwrite (f, "osd_position", "%.1f%s:%.1f%s",
			p->osd_pos.x >= 20000 ? (p->osd_pos.x - 30000) / 10.0 : (float)p->osd_pos.x, p->osd_pos.x >= 20000 ? "%" : "",
			p->osd_pos.y >= 20000 ? (p->osd_pos.y - 30000) / 10.0 : (float)p->osd_pos.y, p->osd_pos.y >= 20000 ? "%" : "");
	}
	cfgfile_dwrite_bool (f, "show_leds_rtg", !!(p->leds_on_screen & STATUSLINE_RTG));
	cfgfile_dwrite (f, "keyboard_leds", "numlock:%s,capslock:%s,scrolllock:%s",
		kbleds[p->keyboard_leds[0]], kbleds[p->keyboard_leds[1]], kbleds[p->keyboard_leds[2]]);
	if (p->chipset_mask & CSMASK_AGA)
		cfgfile_dwrite (f, "chipset","aga");
	else if ((p->chipset_mask & CSMASK_ECS_AGNUS) && (p->chipset_mask & CSMASK_ECS_DENISE))
		cfgfile_dwrite (f, "chipset","ecs");
	else if (p->chipset_mask & CSMASK_ECS_AGNUS)
		cfgfile_dwrite (f, "chipset","ecs_agnus");
	else if (p->chipset_mask & CSMASK_ECS_DENISE)
		cfgfile_dwrite (f, "chipset","ecs_denise");
	else
		cfgfile_dwrite (f, "chipset", "ocs");
	if (p->chipset_refreshrate > 0)
		cfgfile_write (f, "chipset_refreshrate", "%d", p->chipset_refreshrate);
		
	for (i = 0; i < MAX_CHIPSET_REFRESH_TOTAL; i++) {
		if (p->cr[i].rate <= 0)
			continue;
		struct chipset_refresh *cr = &p->cr[i];
		_stprintf (tmp, "%f", cr->rate);
		TCHAR *s = tmp + _tcslen (tmp);
		if (cr->label[0] > 0 && i < MAX_CHIPSET_REFRESH)
			s += _stprintf (s, ",t=%s", cr->label);
		if (cr->horiz > 0)
			s += _stprintf (s, ",h=%d", cr->horiz);
		if (cr->vert > 0)
			s += _stprintf (s, ",v=%d", cr->vert);
		if (cr->locked)
			_tcscat (s, ",locked");
		if (cr->ntsc > 0)
			_tcscat (s, ",ntsc");
		else if (cr->ntsc == 0)
			_tcscat (s, ",pal");
		if (cr->lace > 0)
			_tcscat (s, ",lace");
		else if (cr->lace == 0)
			_tcscat (s, ",nlace");
		if (cr->framelength > 0)
			_tcscat (s, ",lof");
		else if (cr->framelength == 0)
			_tcscat (s, ",shf");
		if (cr->vsync > 0)
			_tcscat (s, ",vsync");
		else if (cr->vsync == 0)
			_tcscat (s, ",nvsync");
		if (cr->rtg)
			_tcscat (s, ",rtg");
		if (cr->commands[0]) {
			_tcscat (s, ",");
			_tcscat (s, cr->commands);
			unsigned int j;
			for (j = 0; j < _tcslen (s); j++) {
				if (s[j] == '\n')
					s[j] = ',';
			}
			s[_tcslen (s) - 1] = 0;
		}
		if (i == CHIPSET_REFRESH_PAL)
			cfgfile_dwrite (f, "displaydata_pal", tmp);
		else if (i == CHIPSET_REFRESH_NTSC)
			cfgfile_dwrite (f, "displaydata_ntsc", tmp);
		else
			cfgfile_dwrite (f, "displaydata", tmp);
	}

	cfgfile_write_str (f, "collision_level", collmode[p->collision_level]);

	cfgfile_write_str(f, "chipset_compatible", cscompa[p->cs_compatible]);
	cfgfile_dwrite_str (f, "ciaatod", ciaatodmode[p->cs_ciaatod]);
	cfgfile_dwrite_str (f, "rtc", rtctype[p->cs_rtc]);
	//cfgfile_dwrite (f, "chipset_rtc_adjust", "%d", p->cs_rtc_adjust);
	cfgfile_dwrite_bool (f, "ksmirror_e0", p->cs_ksmirror_e0);
	cfgfile_dwrite_bool (f, "ksmirror_a8", p->cs_ksmirror_a8);
	cfgfile_dwrite_bool (f, "cd32cd", p->cs_cd32cd);
	cfgfile_dwrite_bool (f, "cd32c2p", p->cs_cd32c2p);
	cfgfile_dwrite_bool (f, "cd32nvram", p->cs_cd32nvram);
	cfgfile_dwrite_bool (f, "cdtvcd", p->cs_cdtvcd);
	cfgfile_dwrite_bool (f, "cdtvram", p->cs_cdtvram);
	cfgfile_dwrite (f, "cdtvramcard", "%d", p->cs_cdtvcard);
	cfgfile_dwrite_str (f, "ide", p->cs_ide == IDE_A600A1200 ? "a600/a1200" : (p->cs_ide == IDE_A4000 ? "a4000" : "none"));
	cfgfile_dwrite_bool (f, "a1000ram", p->cs_a1000ram);
	cfgfile_dwrite (f, "fatgary", "%d", p->cs_fatgaryrev);
	cfgfile_dwrite (f, "ramsey", "%d", p->cs_ramseyrev);
	cfgfile_dwrite_bool (f, "pcmcia", p->cs_pcmcia);
	cfgfile_dwrite_bool (f, "scsi_cdtv", p->cs_cdtvscsi);
	cfgfile_dwrite_bool (f, "scsi_a2091", p->cs_a2091);
	cfgfile_dwrite_bool (f, "scsi_a4091", p->cs_a4091);
	cfgfile_dwrite_bool (f, "scsi_a3000", p->cs_mbdmac == 1);
	cfgfile_dwrite_bool (f, "scsi_a4000t", p->cs_mbdmac == 2);
	cfgfile_dwrite_bool (f, "bogomem_fast", p->cs_slowmemisfast);
	cfgfile_dwrite_bool (f, "resetwarning", p->cs_resetwarning);
	cfgfile_dwrite_bool (f, "denise_noehb", p->cs_denisenoehb);
	cfgfile_dwrite_bool (f, "agnus_bltbusybug", p->cs_agnusbltbusybug);
	cfgfile_dwrite_bool (f, "ics_agnus", p->cs_dipagnus);

	cfgfile_write (f, "fastmem_size", "%d", p->fastmem_size / 0x100000);
	cfgfile_write (f, "a3000mem_size", "%d", p->mbresmem_low_size / 0x100000);
	cfgfile_write (f, "mbresmem_size", "%d", p->mbresmem_high_size / 0x100000);
	cfgfile_write (f, "z3mem_size", "%d", p->z3fastmem_size / 0x100000);
	cfgfile_write (f, "z3mem2_size", "%d", p->z3fastmem2_size / 0x100000);
	cfgfile_write (f, "z3mem_start", "0x%x", p->z3fastmem_start);
	cfgfile_write (f, "bogomem_size", "%d", p->bogomem_size / 0x40000);
	cfgfile_write (f, "gfxcard_size", "%d", p->gfxmem_size / 0x100000);
	cfgfile_write (f, "chipmem_size", "%d", p->chipmem_size == 0x20000 ? -1 : (p->chipmem_size == 0x40000 ? 0 : p->chipmem_size / 0x80000));
	cfgfile_dwrite (f, "megachipmem_size", "%d", p->z3chipmem_size / 0x100000);

	if (p->m68k_speed > 0)
		cfgfile_write (f, "finegrain_cpu_speed", "%d", p->m68k_speed);
	else
		cfgfile_write_str (f, "cpu_speed", p->m68k_speed == -1 ? "max" : "real");

	/* do not reorder start */
	write_compatibility_cpu(f, p);
	cfgfile_write (f, "cpu_model", "%d", p->cpu_model);
	if (p->fpu_model)
		cfgfile_write (f, "fpu_model", "%d", p->fpu_model);
	if (p->mmu_model)
		cfgfile_write (f, "mmu_model", "%d", p->mmu_model);
	cfgfile_write_bool (f, "cpu_compatible", p->cpu_compatible);
	cfgfile_write_bool (f, "cpu_24bit_addressing", p->address_space_24);
	/* do not reorder end */

	if (p->cpu_cycle_exact) {
		if (p->cpu_frequency)
			cfgfile_write (f, "cpu_frequency", "%d", p->cpu_frequency);
		if (p->cpu_clock_multiplier) {
			if (p->cpu_clock_multiplier >= 256)
				cfgfile_write (f, "cpu_multiplier", "%d", p->cpu_clock_multiplier >> 8);
		}
	}

	cfgfile_write_bool (f, "cpu_cycle_exact", p->cpu_cycle_exact);
	cfgfile_write_bool (f, "blitter_cycle_exact", p->blitter_cycle_exact);
	cfgfile_write_bool (f, "cycle_exact", p->cpu_cycle_exact && p->blitter_cycle_exact ? 1 : 0);
	cfgfile_write_bool (f, "rtg_nocustom", p->picasso96_nocustom);
	cfgfile_write (f, "rtg_modes", "0x%x", p->picasso96_modeflags);

	cfgfile_write_bool (f, "log_illegal_mem", p->illegal_mem);
	if (p->catweasel >= 100)
		cfgfile_dwrite (f, "catweasel", "0x%x", p->catweasel);
	else
		cfgfile_dwrite (f, "catweasel", "%d", p->catweasel);

	cfgfile_write_str (f, "kbd_lang", (p->keyboard_lang == KBD_LANG_DE ? "de"
		: p->keyboard_lang == KBD_LANG_DK ? "dk"
		: p->keyboard_lang == KBD_LANG_ES ? "es"
		: p->keyboard_lang == KBD_LANG_US ? "us"
		: p->keyboard_lang == KBD_LANG_SE ? "se"
		: p->keyboard_lang == KBD_LANG_FR ? "fr"
		: p->keyboard_lang == KBD_LANG_IT ? "it"
		: p->keyboard_lang == KBD_LANG_FI ? "fi"
		: p->keyboard_lang == KBD_LANG_TR ? "tr"
		: "FOO"));

#ifdef SAVESTATE
	cfgfile_dwrite (f, "state_replay_rate", "%d", p->statecapturerate);
	cfgfile_dwrite (f, "state_replay_buffers", "%d", p->statecapturebuffersize);
	cfgfile_dwrite_bool (f, "state_replay_autoplay", p->inprec_autoplay);
#endif
	cfgfile_dwrite_bool (f, "warp", p->turbo_emulation);

#ifdef FILESYS
	write_filesys_config (p, f);
	if (p->filesys_no_uaefsdb)
		cfgfile_write_bool (f, "filesys_no_fsdb", p->filesys_no_uaefsdb);
#endif
	write_inputdevice_config (p, f);
}

int cfgfile_yesno2 (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location)
{
	if (_tcscmp (option, name) != 0)
		return 0;
	if (strcasecmp (value, "yes") == 0 || strcasecmp (value, "y") == 0
		|| strcasecmp (value, "true") == 0 || strcasecmp (value, "t") == 0)
		*location = 1;
	else if (strcasecmp (value, "no") == 0 || strcasecmp (value, "n") == 0
		|| strcasecmp (value, "false") == 0 || strcasecmp (value, "f") == 0
		|| strcasecmp (value, "0") == 0)
		*location = 0;
	else {
		write_log ("Option `%s' requires a value of either `yes' or `no' (was '%s').\n", option, value);
		return -1;
	}
	return 1;
}
int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location)
{
	int val;
	int ret = cfgfile_yesno2 (option, value, name, &val);
	if (ret == 0)
		return 0;
	if (ret < 0)
		*location = false;
	else
		*location = val != 0;
	return 1;
}

int cfgfile_doubleval (const TCHAR *option, const TCHAR *value, const TCHAR *name, double *location)
{
	int base = 10;
	TCHAR *endptr;
	if (_tcscmp (option, name) != 0)
		return 0;
	*location = _tcstod (value, &endptr);
	return 1;
}

int cfgfile_intval_unsigned (const TCHAR *option, const TCHAR *value, const TCHAR *name, unsigned int *location, int scale)
{
	int base = 10;
	TCHAR *endptr;
	if (_tcscmp (option, name) != 0)
		return 0;
	/* I guess octal isn't popular enough to worry about here...  */
	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*location = _tcstol (value, &endptr, base) * scale;

	if (*endptr != '\0' || *value == '\0') {
		if (strcasecmp (value, "false") == 0 || strcasecmp (value, "no") == 0) {
			*location = 0;
			return 1;
		}
		if (strcasecmp (value, "true") == 0 || strcasecmp (value, "yes") == 0) {
			*location = 1;
			return 1;
		}
		write_log ("Option '%s' requires a numeric argument but got '%s'\n", option, value);
		return -1;
	}
	return 1;
}

int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, int scale)
{
	unsigned int v = 0;
	int r = cfgfile_intval_unsigned (option, value, name, &v, scale);
	if (!r)
		return 0;
	*location = (int)v;
	return r;
}

int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, const TCHAR *table[], int more)
{
	int val;
	if (_tcscmp (option, name) != 0)
		return 0;
	val = match_string (table, value);
	if (val == -1) {
		if (more)
			return 0;

		write_log ("Unknown value ('%s') for option '%s'.\n", value, option);
		return -1;
	}
	*location = val;
	return 1;
}

int cfgfile_strboolval (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location, const TCHAR *table[], int more)
{
	int locationint;
	if (!cfgfile_strval (option, value, name, &locationint, table, more))
		return 0;
	*location = locationint != 0;
	return 1;
}

int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	if (_tcscmp (option, name) != 0)
		return 0;
	_tcsncpy (location, value, maxsz - 1);
	location[maxsz - 1] = '\0';
	return 1;
}

int cfgfile_path_mp (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz, struct multipath *mp)
{
	if (!cfgfile_string (option, value, name, location, maxsz))
		return 0;

	unsigned int i;
	//TCHAR *s = target_expand_environment (location);
	_tcsncpy (location, location, maxsz - 1);
	location[maxsz - 1] = 0;
	if (mp) {
		for (i = 0; i < MAX_PATHS; i++) {
			if (mp->path[i][0] && _tcscmp (mp->path[i], ".\\") != 0 && _tcscmp (mp->path[i], "./") != 0 && (location[0] != '/' && location[0] != '\\' && !_tcschr(location, ':'))) {
				TCHAR np[MAX_DPATH];
				_tcscpy (np, mp->path[i]);
				fixtrailing (np);
				_tcscat (np, location);
				fullpath (np, sizeof np / sizeof (TCHAR));
				if (zfile_exists (np)) {
					_tcsncpy (location, np, maxsz - 1);
					location[maxsz - 1] = 0;
					break;
				}
			}
		}
	}

	return 1;
}
int cfgfile_path (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	return cfgfile_path_mp (option, value, name, location, maxsz, NULL);
}

int cfgfile_multipath (const TCHAR *option, const TCHAR *value, const TCHAR *name, struct multipath *mp)
{
	unsigned int i;

	TCHAR tmploc[MAX_DPATH];
	if (!cfgfile_string (option, value, name, tmploc, 256))
		return 0;
	for (i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] == 0 || (i == 0 && (!_tcscmp (mp->path[i], ".\\") || !_tcscmp (mp->path[i], "./")))) {
			//TCHAR *s = target_expand_environment (tmploc);
			_tcsncpy (mp->path[i], tmploc, 256 - 1);
			mp->path[i][256 - 1] = 0;
			fixtrailing (mp->path[i]);
			return 1;
		}
	}
	return 1;
}

int cfgfile_rom (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	TCHAR id[MAX_DPATH];
	if (!cfgfile_string (option, value, name, id, sizeof id / sizeof (TCHAR)))
		return 0;
	if (zfile_exists (location))
		return 1;
	TCHAR *p = _tcschr (id, ',');
	if (p) {
		TCHAR *endptr, tmp;
		*p = 0;
		tmp = id[4];
		id[4] = 0;
		uae_u32 crc32 = _tcstol (id, &endptr, 16) << 16;
		id[4] = tmp;
		crc32 |= _tcstol (id + 4, &endptr, 16);
		struct romdata *rd = getromdatabycrc (crc32);
		if (rd) {
			struct romlist *rl = getromlistbyromdata (rd);
			if (rl) {
				write_log ("%s: %s -> %s\n", name, location, rl->path);
				_tcsncpy (location, rl->path, maxsz);
			}
		}
	}
	return 1;
}

static int getintval (TCHAR **p, int *result, int delim)
{
	TCHAR *value = *p;
	int base = 10;
	TCHAR *endptr;
	TCHAR *p2 = _tcschr (*p, delim);

	if (p2 == 0)
		return 0;

	*p2++ = '\0';

	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*result = _tcstol (value, &endptr, base);
	*p = p2;

	if (*endptr != '\0' || *value == '\0')
		return 0;

	return 1;
}

static int getintval2 (TCHAR **p, int *result, int delim)
{
	TCHAR *value = *p;
	int base = 10;
	TCHAR *endptr;
	TCHAR *p2 = _tcschr (*p, delim);

	if (p2 == 0) {
		p2 = _tcschr (*p, 0);
		if (p2 == 0) {
			*p = 0;
			return 0;
		}
	}
	if (*p2 != 0)
		*p2++ = '\0';

	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*result = _tcstol (value, &endptr, base);
	*p = p2;

	if (*endptr != '\0' || *value == '\0') {
		*p = 0;
		return 0;
	}

	return 1;
}

static void set_chipset_mask (struct uae_prefs *p, int val)
{
	p->chipset_mask = (val == 0 ? 0
		: val == 1 ? CSMASK_ECS_AGNUS
		: val == 2 ? CSMASK_ECS_DENISE
		: val == 3 ? CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS
		: CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS);
}

/*
 * Duplicate the path 'src'. If 'src' begins with '~/' substitue
 * the home directory.
 *
 * TODO: Clean this up.
 * TODO: Collect path handling tools in one place and cleanly
 * handle platform-specific differences.
 */
static const TCHAR *strdup_path_expand (const char *src)
{
    char *path = 0;
    unsigned int srclen, destlen;
    int need_separator = 0;
    const char *home = getenv ("HOME");

    srclen = strlen (src);

    if (srclen > 0) {
	if (src[srclen - 1] != '/' && src[srclen - 1] != '\\'
#ifdef TARGET_AMIGAOS
	    && src[srclen - 1] != ':'
#endif
	    ) {
	    need_separator = 1;
	}
    }

    destlen = srclen + need_separator;

    if (src[0] == '~' && src[1] == '/' && home) {
	destlen += srclen + strlen (home);
	src++;
	srclen--;
    } else
	home = 0;

    path = malloc (destlen + 1); path[0]=0;

    if (path) {
	if (home)
	    strcpy (path, home);

	strcat (path, src);

	if (need_separator)
	    strcat (path, "/");
    }

    return path;
}

static int cfgfile_parse_host (struct uae_prefs *p, TCHAR *option, TCHAR *value)
{
	int i;
	bool vb;
	TCHAR *section = 0;
	TCHAR *tmpp;
	TCHAR tmpbuf[CONFIG_BLEN];

	if (_tcsncmp (option, "input.", 6) == 0) {
		read_inputdevice_config (p, option, value);
		return 1;
	}

	for (tmpp = option; *tmpp != '\0'; tmpp++)
		if (_istupper (*tmpp))
			*tmpp = _totlower (*tmpp);
	tmpp = _tcschr (option, '.');
	if (tmpp) {
		section = option;
		option = tmpp + 1;
		*tmpp = '\0';
		if (_tcscmp (section, TARGET_NAME) == 0) {
			/* We special case the various path options here.  */
			if (cfgfile_multipath (option, value, "rom_path", &p->path_rom)
				|| cfgfile_multipath (option, value, "floppy_path", &p->path_floppy)
				|| cfgfile_multipath (option, value, "cd_path", &p->path_cd)
				|| cfgfile_multipath (option, value, "hardfile_path", &p->path_hardfile))
				return 1;
			return target_parse_option (p, option, value);
		}
	if (strcmp (section, MACHDEP_NAME) == 0)
	    return machdep_parse_option (p, option, value);
	if (strcmp (section, GFX_NAME) == 0)
	    return gfx_parse_option (p, option, value);
	if (strcmp (section, AUDIO_NAME) == 0)
	    return audio_parse_option (p, option, value);

		return 0;
	}
	for (i = 0; i < MAX_SPARE_DRIVES; i++) {
		_stprintf (tmpbuf, "diskimage%d", i);
		if (cfgfile_path (option, value, tmpbuf, p->dfxlist[i], sizeof p->dfxlist[i] / sizeof (TCHAR))) {
#if 0
			if (i < 4 && !p->df[i][0])
				_tcscpy (p->df[i], p->dfxlist[i]);
#endif
			return 1;
		}
	}

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		TCHAR tmp[20];
		_stprintf (tmp, "cdimage%d", i);
		if (!_tcsicmp (option, tmp)) {
			p->cdslots[i].delayed = false;
			TCHAR *next = _tcsrchr (value, ',');
			int type = SCSI_UNIT_DEFAULT;
			int mode = 0;
			int unitnum = 0;
			for (;;) {
				if (!next)
					break;
				*next++ = 0;
				TCHAR *next2 = _tcschr (next, ':');
				if (next2)
					*next2++ = 0;
				int tmpval = 0;
				if (!_tcsicmp (next, "delay")) {
					p->cdslots[i].delayed = true;
					next = next2;
					if (!next)
						break;
					next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
				}
				type = match_string (cdmodes, next);
				if (type < 0)
					type = SCSI_UNIT_DEFAULT;
				else
					type--;
				next = next2;
				if (!next)
					break;
				next2 = _tcschr (next, ':');
				if (next2)
					*next2++ = 0;
				mode = match_string (cdconmodes, next);
				if (mode < 0)
					mode = 0;
				next = next2;
				if (!next)
					break;
				next2 = _tcschr (next, ':');
				if (next2)
					*next2++ = 0;
				cfgfile_intval (option, next, tmp, &unitnum, 1);
			}
			if (_tcslen (value) > 0) {
				TCHAR *s = cfgfile_get_multipath (&p->path_cd, NULL, value, false);
				_tcsncpy (p->cdslots[i].name, s, sizeof p->cdslots[i].name / sizeof (TCHAR));
				xfree (s);
			}
			p->cdslots[i].name[sizeof p->cdslots[i].name - 1] = 0;
			p->cdslots[i].inuse = true;
			p->cdslots[i].type = type;
			// disable all following units
			i++;
			while (i < MAX_TOTAL_SCSI_DEVICES) {
				p->cdslots[i].type = SCSI_UNIT_DISABLED;
				i++;
			}
			return 1;
		}
	}

	if (cfgfile_intval (option, value, "sound_frequency", &p->sound_freq, 1)) {
		/* backwards compatibility */
		p->sound_latency = 1000 * (p->sound_maxbsiz >> 1) / p->sound_freq;
		return 1;
	}

	if (cfgfile_intval (option, value, "sound_latency", &p->sound_latency, 1)
		|| cfgfile_intval (option, value, "sound_max_buff", &p->sound_maxbsiz, 1)
		|| cfgfile_intval (option, value, "state_replay_rate", &p->statecapturerate, 1)
		|| cfgfile_intval (option, value, "state_replay_buffers", &p->statecapturebuffersize, 1)
		|| cfgfile_yesno (option, value, "state_replay_autoplay", &p->inprec_autoplay)
		|| cfgfile_intval (option, value, "sound_frequency", &p->sound_freq, 1)
		|| cfgfile_intval (option, value, "sound_volume", &p->sound_volume, 1)
		|| cfgfile_intval (option, value, "sound_volume_cd", &p->sound_volume_cd, 1)
		|| cfgfile_intval (option, value, "sound_stereo_separation", &p->sound_stereo_separation, 1)
		|| cfgfile_intval (option, value, "sound_stereo_mixing_delay", &p->sound_mixed_stereo_delay, 1)

		|| cfgfile_intval (option, value, "gfx_display", &p->gfx_display, 1)
		|| cfgfile_intval (option, value, "gfx_framerate", &p->gfx_framerate, 1)
		|| cfgfile_intval (option, value, "gfx_width_windowed", &p->gfx_size_win.width, 1)
		|| cfgfile_intval (option, value, "gfx_height_windowed", &p->gfx_size_win.height, 1)
		|| cfgfile_intval (option, value, "gfx_top_windowed", &p->gfx_size_win.x, 1)
		|| cfgfile_intval (option, value, "gfx_left_windowed", &p->gfx_size_win.y, 1)
		|| cfgfile_intval (option, value, "gfx_width_fullscreen", &p->gfx_size_fs.width, 1)
		|| cfgfile_intval (option, value, "gfx_height_fullscreen", &p->gfx_size_fs.height, 1)
		|| cfgfile_intval (option, value, "gfx_refreshrate", &p->gfx_refreshrate, 1)
		|| cfgfile_yesno (option, value, "gfx_autoresolution", &p->gfx_autoresolution)
		|| cfgfile_strval (option, value, "gfx_autoresolution_min_vertical", &p->gfx_autoresolution_minv, lorestype1, 0)
		|| cfgfile_strval (option, value, "gfx_autoresolution_min_horizontal", &p->gfx_autoresolution_minh, vertmode, 0)
 		|| cfgfile_intval (option, value, "gfx_backbuffers", &p->gfx_backbuffers, 1)

		|| cfgfile_intval (option, value, "gfx_center_horizontal_position", &p->gfx_xcenter_pos, 1)
		|| cfgfile_intval (option, value, "gfx_center_vertical_position", &p->gfx_ycenter_pos, 1)
		|| cfgfile_intval (option, value, "gfx_center_horizontal_size", &p->gfx_xcenter_size, 1)
		|| cfgfile_intval (option, value, "gfx_center_vertical_size", &p->gfx_ycenter_size, 1)

#ifdef GFXFILTER
		|| cfgfile_intval (option, value, "gfx_filter_vert_zoom", &p->gfx_filter_vert_zoom, 1)
		|| cfgfile_intval (option, value, "gfx_filter_horiz_zoom", &p->gfx_filter_horiz_zoom, 1)
		|| cfgfile_intval (option, value, "gfx_filter_vert_zoom_mult", &p->gfx_filter_vert_zoom_mult, 1)
		|| cfgfile_intval (option, value, "gfx_filter_horiz_zoom_mult", &p->gfx_filter_horiz_zoom_mult, 1)
		|| cfgfile_intval (option, value, "gfx_filter_vert_offset", &p->gfx_filter_vert_offset, 1)
		|| cfgfile_intval (option, value, "gfx_filter_horiz_offset", &p->gfx_filter_horiz_offset, 1)
		|| cfgfile_intval (option, value, "gfx_filter_scanlines", &p->gfx_filter_scanlines, 1)
		|| cfgfile_intval (option, value, "gfx_filter_scanlinelevel", &p->gfx_filter_scanlinelevel, 1)
		|| cfgfile_intval (option, value, "gfx_filter_scanlineratio", &p->gfx_filter_scanlineratio, 1)
		|| cfgfile_intval (option, value, "gfx_filter_luminance", &p->gfx_filter_luminance, 1)
		|| cfgfile_intval (option, value, "gfx_filter_contrast", &p->gfx_filter_contrast, 1)
		|| cfgfile_intval (option, value, "gfx_filter_saturation", &p->gfx_filter_saturation, 1)
		|| cfgfile_intval (option, value, "gfx_filter_gamma", &p->gfx_filter_gamma, 1)
		|| cfgfile_intval (option, value, "gfx_filter_blur", &p->gfx_filter_blur, 1)
		|| cfgfile_intval (option, value, "gfx_filter_noise", &p->gfx_filter_noise, 1)
		|| cfgfile_intval (option, value, "gfx_filter_bilinear", &p->gfx_filter_bilinear, 1)
		|| cfgfile_intval (option, value, "gfx_luminance", &p->gfx_luminance, 1)
		|| cfgfile_intval (option, value, "gfx_contrast", &p->gfx_contrast, 1)
		|| cfgfile_intval (option, value, "gfx_gamma", &p->gfx_gamma, 1)
		|| cfgfile_string (option, value, "gfx_filter_mask", p->gfx_filtermask, sizeof p->gfx_filtermask / sizeof (TCHAR))
#endif
#ifdef DRIVESOUND
		|| cfgfile_intval (option, value, "floppy0sound", &p->floppyslots[0].dfxclick, 1)
		|| cfgfile_intval (option, value, "floppy1sound", &p->floppyslots[1].dfxclick, 1)
		|| cfgfile_intval (option, value, "floppy2sound", &p->floppyslots[2].dfxclick, 1)
		|| cfgfile_intval (option, value, "floppy3sound", &p->floppyslots[3].dfxclick, 1)
		|| cfgfile_intval (option, value, "floppy_channel_mask", &p->dfxclickchannelmask, 1)
		|| cfgfile_intval (option, value, "floppy_volume", &p->dfxclickvolume, 1)
#endif
		)
		return 1;

	if (
#ifdef DRIVESOUND
		cfgfile_path (option, value, "floppy0soundext", p->floppyslots[0].dfxclickexternal, sizeof p->floppyslots[0].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, "floppy1soundext", p->floppyslots[1].dfxclickexternal, sizeof p->floppyslots[1].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, "floppy2soundext", p->floppyslots[2].dfxclickexternal, sizeof p->floppyslots[2].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, "floppy3soundext", p->floppyslots[3].dfxclickexternal, sizeof p->floppyslots[3].dfxclickexternal / sizeof (TCHAR))
		|| 
#endif
		   cfgfile_string (option, value, "gfx_display_name", p->gfx_display_name, sizeof p->gfx_display_name / sizeof (TCHAR))
		|| cfgfile_string (option, value, "config_window_title", p->config_window_title, sizeof p->config_window_title / sizeof (TCHAR))
		|| cfgfile_string (option, value, "config_info", p->info, sizeof p->info / sizeof (TCHAR))
		|| cfgfile_string (option, value, "config_description", p->description, sizeof p->description / sizeof (TCHAR)))
		return 1;

	if (cfgfile_yesno (option, value, "use_debugger", &p->start_debugger)
		|| cfgfile_yesno (option, value, "sound_auto", &p->sound_auto)
		|| cfgfile_yesno (option, value, "sound_stereo_swap_paula", &p->sound_stereo_swap_paula)
		|| cfgfile_yesno (option, value, "sound_stereo_swap_ahi", &p->sound_stereo_swap_ahi)
		|| cfgfile_yesno (option, value, "avoid_cmov", &p->avoid_cmov)
		|| cfgfile_yesno (option, value, "log_illegal_mem", &p->illegal_mem)
		|| cfgfile_yesno (option, value, "filesys_no_fsdb", &p->filesys_no_uaefsdb)
		|| cfgfile_yesno (option, value, "gfx_blacker_than_black", &p->gfx_blackerthanblack)
		|| cfgfile_yesno (option, value, "gfx_flickerfixer", &p->gfx_scandoubler)
		|| cfgfile_yesno (option, value, "synchronize_clock", &p->tod_hack)
		|| cfgfile_yesno (option, value, "magic_mouse", &p->input_magic_mouse)
		|| cfgfile_yesno (option, value, "warp", &p->turbo_emulation)
		|| cfgfile_yesno (option, value, "headless", &p->headless)
		|| cfgfile_yesno (option, value, "clipboard_sharing", &p->clipboard_sharing)
		|| cfgfile_yesno (option, value, "bsdsocket_emu", &p->socket_emu))
		return 1;

	if (cfgfile_strval (option, value, "sound_output", &p->produce_sound, soundmode1, 1)
		|| cfgfile_strval (option, value, "sound_output", &p->produce_sound, soundmode2, 0)
		|| cfgfile_strval (option, value, "sound_interpol", &p->sound_interpol, interpolmode, 0)
		|| cfgfile_strval (option, value, "sound_filter", &p->sound_filter, soundfiltermode1, 0)
		|| cfgfile_strval (option, value, "sound_filter_type", &p->sound_filter_type, soundfiltermode2, 0)
		|| cfgfile_strboolval (option, value, "use_gui", &p->start_gui, guimode1, 1)
		|| cfgfile_strboolval (option, value, "use_gui", &p->start_gui, guimode2, 1)
		|| cfgfile_strboolval (option, value, "use_gui", &p->start_gui, guimode3, 0)
		|| cfgfile_strval (option, value, "gfx_resolution", &p->gfx_resolution, lorestype1, 0)
		|| cfgfile_strval (option, value, "gfx_lores", &p->gfx_resolution, lorestype2, 0)
		|| cfgfile_strval (option, value, "gfx_lores_mode", &p->gfx_lores_mode, loresmode, 0)
		|| cfgfile_strval (option, value, "gfx_fullscreen_amiga", &p->gfx_afullscreen, fullmodes, 0)
		|| cfgfile_strval (option, value, "gfx_fullscreen_picasso", &p->gfx_pfullscreen, fullmodes, 0)
		|| cfgfile_strval (option, value, "gfx_center_horizontal", &p->gfx_xcenter, centermode1, 1)
		|| cfgfile_strval (option, value, "gfx_center_vertical", &p->gfx_ycenter, centermode1, 1)
		|| cfgfile_strval (option, value, "gfx_center_horizontal", &p->gfx_xcenter, centermode2, 0)
		|| cfgfile_strval (option, value, "gfx_center_vertical", &p->gfx_ycenter, centermode2, 0)
		|| cfgfile_strval (option, value, "gfx_colour_mode", &p->color_mode, colormode1, 1)
		|| cfgfile_strval (option, value, "gfx_colour_mode", &p->color_mode, colormode2, 0)
		|| cfgfile_strval (option, value, "gfx_color_mode", &p->color_mode, colormode1, 1)
		|| cfgfile_strval (option, value, "gfx_color_mode", &p->color_mode, colormode2, 0)
		|| cfgfile_strval (option, value, "gfx_max_horizontal", &p->gfx_max_horizontal, maxhoriz, 0)
		|| cfgfile_strval (option, value, "gfx_max_vertical", &p->gfx_max_vertical, maxvert, 0)
		|| cfgfile_strval (option, value, "gfx_filter_autoscale", &p->gfx_filter_autoscale, autoscale, 0)
		|| cfgfile_strval (option, value, "gfx_api", &p->gfx_api, filterapi, 0)
		|| cfgfile_strval (option, value, "magic_mousecursor", &p->input_magic_mouse_cursor, magiccursors, 0)
		|| cfgfile_strval (option, value, "gfx_filter_keep_aspect", &p->gfx_filter_keep_aspect, aspects, 0)
		|| cfgfile_strval (option, value, "absolute_mouse", &p->input_tablet, abspointers, 0))
		return 1;

	if (_tcscmp (option, "gfx_linemode") == 0) {
		int v;
		p->gfx_vresolution = VRES_DOUBLE;
		p->gfx_scanlines = false;
		if (cfgfile_strval (option, value, "gfx_linemode", &v, linemode, 0)) {
			p->gfx_scanlines = v & 1;
			p->gfx_vresolution = v / 2;
		}
		return 1;
	}
	if (_tcscmp (option, "gfx_vsync") == 0) {
		if (cfgfile_strval (option, value, "gfx_vsync", &p->gfx_avsync, vsyncmodes, 0) >= 0)
			return 1;
		return cfgfile_yesno (option, value, "gfx_vsync", &p->gfx_avsync);
	}
	if (_tcscmp (option, "gfx_vsync_picasso") == 0) {
		if (cfgfile_strval (option, value, "gfx_vsync_picasso", &p->gfx_pvsync, vsyncmodes, 0) >= 0)
			return 1;
		return cfgfile_yesno (option, value, "gfx_vsync_picasso", &p->gfx_pvsync);
	}
	if (cfgfile_strval (option, value, "gfx_vsyncmode", &p->gfx_avsyncmode, vsyncmodes2, 0))
		return 1;
	if (cfgfile_strval (option, value, "gfx_vsyncmode_picasso", &p->gfx_pvsyncmode, vsyncmodes2, 0))
		return 1;

	if (cfgfile_yesno (option, value, "show_leds", &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_CHIPSET;
		else
			p->leds_on_screen &= ~STATUSLINE_CHIPSET;
		return 1;
	}
	if (cfgfile_yesno (option, value, "show_leds_rtg", &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_RTG;
		else
			p->leds_on_screen &= ~STATUSLINE_RTG;
		return 1;
	}

	if (!_tcscmp (option, "osd_position")) {
		TCHAR *s = value;
		p->osd_pos.x = 0;
		p->osd_pos.y = 0;
		while (s) {
			if (!_tcschr (s, ':'))
				break;
			p->osd_pos.x = _tstof (s) * 10.0;
			s = _tcschr (s, ':');
			if (!s)
				break;
			if (s[-1] == '%')
				p->osd_pos.x += 30000;
			s++;
			p->osd_pos.y = _tstof (s) * 10.0;
			s += _tcslen (s);
			if (s[-1] == '%')
				p->osd_pos.y += 30000;
			break;
		}
		return 1;
	}

#ifdef GFXFILTER
	if (_tcscmp (option, "gfx_filter_overlay") == 0) {
		return 1;
	}

	if (_tcscmp (option, "gfx_filter") == 0) {
		int i = 0;
		TCHAR *s = _tcschr (value, ':');
		p->gfx_filtershader[0] = 0;
		p->gfx_filter = 0;
		if (s) {
			*s++ = 0;
			if (!_tcscmp (value, "D3D")) {
				p->gfx_api = 1;
				_tcscpy (p->gfx_filtershader, s);
			}
		}
		if (!_tcscmp (value, "direct3d")) {
			p->gfx_api = 1; // forwards compatibiity
		} else {
			while(uaefilters[i].name) {
				if (!_tcscmp (uaefilters[i].cfgname, value)) {
					p->gfx_filter = uaefilters[i].type;
					break;
				}
				i++;
			}
		}
		return 1;
	}
	if (_tcscmp (option, "gfx_filter_mode") == 0) {
		cfgfile_strval (option, value, "gfx_filter_mode", &p->gfx_filter_filtermode, filtermode2, 0);
		return 1;
	}

	if (cfgfile_string (option, value, "gfx_filter_aspect_ratio", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		int v1, v2;
		TCHAR *s;

		p->gfx_filter_aspect = -1;
		v1 = _tstol (tmpbuf);
		s = _tcschr (tmpbuf, ':');
		if (s) {
			v2 = _tstol (s + 1);
			if (v1 < 0 || v2 < 0)
				p->gfx_filter_aspect = -1;
			else if (v1 == 0 || v2 == 0)
				p->gfx_filter_aspect = 0;
			else
				p->gfx_filter_aspect = (v1 << 8) | v2;
		}
		return 1;
	}
#endif

	if (_tcscmp (option, "gfx_width") == 0 || _tcscmp (option, "gfx_height") == 0) {
		cfgfile_intval (option, value, "gfx_width", &p->gfx_size_win.width, 1);
		cfgfile_intval (option, value, "gfx_height", &p->gfx_size_win.height, 1);
		p->gfx_size_fs.width = p->gfx_size_win.width;
		p->gfx_size_fs.height = p->gfx_size_win.height;
		return 1;
	}

	if (_tcscmp (option, "gfx_fullscreen_multi") == 0 || _tcscmp (option, "gfx_windowed_multi") == 0) {
		TCHAR tmp[256], *tmpp, *tmpp2;
		struct wh *wh = p->gfx_size_win_xtra;
		if (_tcscmp (option, "gfx_fullscreen_multi") == 0)
			wh = p->gfx_size_fs_xtra;
		_stprintf (tmp, ",%s,", value);
		tmpp2 = tmp;
		for (i = 0; i < 4; i++) {
			tmpp = _tcschr (tmpp2, ',');
			tmpp++;
			wh[i].width = _tstol (tmpp);
			while (*tmpp != ',' && *tmpp != 'x' && *tmpp != '*')
				tmpp++;
			wh[i].height = _tstol (tmpp + 1);
			tmpp2 = tmpp;
		}
		return 1;
	}

	if (_tcscmp (option, "joyportfriendlyname0") == 0 || _tcscmp (option, "joyportfriendlyname1") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyportfriendlyname0") == 0 ? 0 : 1, -1, 2);
		return 1;
	}
	if (_tcscmp (option, "joyportfriendlyname2") == 0 || _tcscmp (option, "joyportfriendlyname3") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyportfriendlyname2") == 0 ? 2 : 3, -1, 2);
		return 1;
	}
	if (_tcscmp (option, "joyportname0") == 0 || _tcscmp (option, "joyportname1") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyportname0") == 0 ? 0 : 1, -1, 1);
		return 1;
	}
	if (_tcscmp (option, "joyportname2") == 0 || _tcscmp (option, "joyportname3") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyportname2") == 0 ? 2 : 3, -1, 1);
		return 1;
	}
	if (_tcscmp (option, "joyport0") == 0 || _tcscmp (option, "joyport1") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyport0") == 0 ? 0 : 1, -1, 0);
		return 1;
	}
	if (_tcscmp (option, "joyport2") == 0 || _tcscmp (option, "joyport3") == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, "joyport2") == 0 ? 2 : 3, -1, 0);
		return 1;
	}
	if (cfgfile_strval (option, value, "joyport0mode", &p->jports[0].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport1mode", &p->jports[1].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport2mode", &p->jports[2].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport3mode", &p->jports[3].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport0autofire", &p->jports[0].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport1autofire", &p->jports[1].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport2autofire", &p->jports[2].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, "joyport3autofire", &p->jports[3].autofire, joyaf, 0))
		return 1;

	if (cfgfile_path (option, value, "statefile_quit", p->quitstatefile, sizeof p->quitstatefile / sizeof (TCHAR)))
		return 1;

#ifdef SAVESTATE
	if (cfgfile_path (option, value, "statefile", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		_tcscpy (p->statefile, tmpbuf);
		_tcscpy (savestate_fname, tmpbuf);
		if (zfile_exists (savestate_fname)) {
			savestate_state = STATE_DORESTORE;
		} else {
			int ok = 0;
			if (savestate_fname[0]) {
				for (;;) {
					TCHAR *p;
					if (my_existsdir (savestate_fname)) {
						ok = 1;
						break;
					}
					p = _tcsrchr (savestate_fname, '\\');
					if (!p)
						p = _tcsrchr (savestate_fname, '/');
					if (!p)
						break;
					*p = 0;
				}
			}
			if (!ok) {
				TCHAR tmp[MAX_DPATH];
				//fetch_statefilepath (tmp, sizeof tmp / sizeof (TCHAR));
				_tcscat (tmp, savestate_fname);
				if (zfile_exists (tmp)) {
					_tcscpy (savestate_fname, tmp);
					savestate_state = STATE_DORESTORE;
				} else {
					savestate_fname[0] = 0;
				}
			}
		}
		return 1;
	}
#endif

	if (cfgfile_strval (option, value, "sound_channels", &p->sound_stereo, stereomode, 1)) {
		if (p->sound_stereo == SND_NONE) { /* "mixed stereo" compatibility hack */
			p->sound_stereo = SND_STEREO;
			p->sound_mixed_stereo_delay = 5;
			p->sound_stereo_separation = 7;
		}
		return 1;
	}

	if (_tcscmp (option, "kbd_lang") == 0) {
		KbdLang l;
		if ((l = KBD_LANG_DE, strcasecmp (value, "de") == 0)
			|| (l = KBD_LANG_DK, strcasecmp (value, "dk") == 0)
			|| (l = KBD_LANG_SE, strcasecmp (value, "se") == 0)
			|| (l = KBD_LANG_US, strcasecmp (value, "us") == 0)
			|| (l = KBD_LANG_FR, strcasecmp (value, "fr") == 0)
			|| (l = KBD_LANG_IT, strcasecmp (value, "it") == 0)
			|| (l = KBD_LANG_ES, strcasecmp (value, "es") == 0)
			|| (l = KBD_LANG_FI, strcasecmp (value, "fi") == 0)
			|| (l = KBD_LANG_TR, strcasecmp (value, "tr") == 0))
			p->keyboard_lang = l;
		else
			write_log ("Unknown keyboard language\n");
		return 1;
	}

	if (cfgfile_string (option, value, "config_version", tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
		TCHAR *tmpp2;
		tmpp = _tcschr (value, '.');
		if (tmpp) {
			*tmpp++ = 0;
			tmpp2 = tmpp;
			p->config_version = _tstol (tmpbuf) << 16;
			tmpp = _tcschr (tmpp, '.');
			if (tmpp) {
				*tmpp++ = 0;
				p->config_version |= _tstol (tmpp2) << 8;
				p->config_version |= _tstol (tmpp);
			}
		}
		return 1;
	}

	if (cfgfile_string (option, value, "keyboard_leds", tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
		TCHAR *tmpp2 = tmpbuf;
		int i, num;
		p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
		p->keyboard_leds_in_use = 0;
		_tcscat (tmpbuf, ",");
		for (i = 0; i < 3; i++) {
			tmpp = _tcschr (tmpp2, ':');
			if (!tmpp)
				break;
			*tmpp++= 0;
			num = -1;
			if (!strcasecmp (tmpp2, "numlock"))
				num = 0;
			if (!strcasecmp (tmpp2, "capslock"))
				num = 1;
			if (!strcasecmp (tmpp2, "scrolllock"))
				num = 2;
			tmpp2 = tmpp;
			tmpp = _tcschr (tmpp2, ',');
			if (!tmpp)
				break;
			*tmpp++= 0;
			if (num >= 0) {
				p->keyboard_leds[num] = match_string (kbleds, tmpp2);
				if (p->keyboard_leds[num])
					p->keyboard_leds_in_use = 1;
			}
			tmpp2 = tmpp;
		}
		return 1;
	}

	if (_tcscmp (option, "displaydata") == 0 || _tcscmp (option, "displaydata_pal") == 0 || _tcscmp (option, "displaydata_ntsc") == 0) {
	    _tcsncpy (tmpbuf, value, sizeof tmpbuf / sizeof (TCHAR) - 1);
	    tmpbuf[sizeof tmpbuf / sizeof (TCHAR) - 1] = '\0';
	
	    int vert = -1, horiz = -1, lace = -1, ntsc = -1, framelength = -1, vsync = -1;
	    bool locked = false;
	    bool rtg = false;
	    double rate = -1;
	    TCHAR cmd[MAX_DPATH], label[16] = { 0 };
	    TCHAR *tmpp = tmpbuf;
	    TCHAR *end = tmpbuf + _tcslen (tmpbuf);
	    cmd[0] = 0;
	    for (;;) {
	      TCHAR *next = _tcschr (tmpp, ',');
	      TCHAR *equals = _tcschr (tmpp, '=');
	
	      if (!next)
	        next = end;
	      if (equals == NULL || equals > next)
	        equals = NULL;
	      else
	        equals++;
	      *next = 0;
	
	      if (rate < 0)
	        rate = _tstof (tmpp);
	      else if (!_tcsnicmp (tmpp, "v=", 2))
	        vert = _tstol (equals);
	      else if (!_tcsnicmp (tmpp, "h=", 2))
	        horiz = _tstol (equals);
	      else if (!_tcsnicmp (tmpp, "t=", 2))
	        _tcsncpy (label, equals, sizeof label / sizeof (TCHAR) - 1);
	      else if (equals) {
	        if (_tcslen (cmd) + _tcslen (tmpp) + 2 < sizeof (cmd) / sizeof(TCHAR)) {
	          _tcscat (cmd, tmpp);
	          _tcscat (cmd, "\n");
	        }
	      }
	      if (!_tcsnicmp (tmpp, "locked", 4))
	        locked = true;
	      if (!_tcsnicmp (tmpp, "nlace", 5))
	        lace = 0;
	      if (!_tcsnicmp (tmpp, "lace", 4))
	        lace = 1;
	      if (!_tcsnicmp (tmpp, "nvsync", 5))
	        vsync = 0;
	      if (!_tcsnicmp (tmpp, "vsync", 4))
	        vsync = 1;
	      if (!_tcsnicmp (tmpp, "ntsc", 4))
	        ntsc = 1;
	      if (!_tcsnicmp (tmpp, "pal", 3))
	        ntsc = 0;
	      if (!_tcsnicmp (tmpp, "lof", 3))
	        framelength = 1;
	      if (!_tcsnicmp (tmpp, "shf", 3))
	        framelength = 0;
	      if (!_tcsnicmp (tmpp, "rtg", 3))
	        rtg = true;
	      tmpp = next;
	      if (tmpp >= end)
	        break;
	      tmpp++;
	    }
	    if (rate > 0) {
	      for (i = 0; i < MAX_CHIPSET_REFRESH; i++) {
	        if (_tcscmp (option, "displaydata_pal") == 0) {
	          i = CHIPSET_REFRESH_PAL;
	          p->cr[i].rate = -1;
	          _tcscpy (label, "PAL");
	        } else if (_tcscmp (option, "displaydata_ntsc") == 0) {
	          i = CHIPSET_REFRESH_NTSC;
	          p->cr[i].rate = -1;
	          _tcscpy (label, "NTSC");
	        }
	        if (p->cr[i].rate <= 0) {
	          p->cr[i].horiz = horiz;
	          p->cr[i].vert = vert;
	          p->cr[i].lace = lace;
	          p->cr[i].ntsc = ntsc;
	          p->cr[i].vsync = vsync;
	          p->cr[i].locked = locked;
	          p->cr[i].rtg = rtg;
	          p->cr[i].framelength = framelength;
	          p->cr[i].rate = rate;
	          _tcscpy (p->cr[i].commands, cmd);
	          _tcscpy (p->cr[i].label, label);
	          break;
	        }
	      }
	    }
	    return 1;
	  }
	
	return 0;
}

static void decode_rom_ident (TCHAR *romfile, int maxlen, const TCHAR *ident, int romflags)
{
	const TCHAR *p;
	int ver, rev, subver, subrev, round, i;
	TCHAR model[64], *modelp;
	struct romlist **rl;
	TCHAR *romtxt;

	if (!ident[0])
		return;
	romtxt = xmalloc (TCHAR, 10000);
	romtxt[0] = 0;
	for (round = 0; round < 2; round++) {
		ver = rev = subver = subrev = -1;
		modelp = NULL;
		memset (model, 0, sizeof model);
		p = ident;
		while (*p) {
			TCHAR c = *p++;
			int *pp1 = NULL, *pp2 = NULL;
			if (_totupper (c) == 'V' && _istdigit (*p)) {
				pp1 = &ver;
				pp2 = &rev;
			} else if (_totupper (c) == 'R' && _istdigit (*p)) {
				pp1 = &subver;
				pp2 = &subrev;
			} else if (!_istdigit (c) && c != ' ') {
				_tcsncpy (model, p - 1, (sizeof model) / sizeof (TCHAR) - 1);
				p += _tcslen (model);
				modelp = model;
			}
			if (pp1) {
				*pp1 = _tstol (p);
				while (*p != 0 && *p != '.' && *p != ' ')
					p++;
				if (*p == '.') {
					p++;
					if (pp2)
						*pp2 = _tstol (p);
				}
			}
			if (*p == 0 || *p == ';') {
				rl = getromlistbyident (ver, rev, subver, subrev, modelp, romflags, round > 0);
				if (rl) {
					for (i = 0; rl[i]; i++) {
						if (round) {
							TCHAR romname[MAX_DPATH];
							getromname(rl[i]->rd, romname);
							_tcscat (romtxt, romname);
							_tcscat (romtxt, "\n");
						} else {
							_tcsncpy (romfile, rl[i]->path, maxlen);
							goto end;
						}
					}
					xfree (rl);
				}
			}
		}
	}
end:
	if (round && romtxt[0]) {
		gui_message("One of the following system ROMs is required:\n\n%s\n\nCheck the System ROM path in the Paths panel and click Rescan ROMs.\n");
	}
	xfree (romtxt);
}

static struct uaedev_config_info *getuci(struct uae_prefs *p)
{
	if (p->mountitems < MOUNT_CONFIG_SIZE)
		return &p->mountconfig[p->mountitems++];
	return NULL;
}

struct uaedev_config_info *add_filesys_config (struct uae_prefs *p, int index,
	TCHAR *devname, TCHAR *volname, TCHAR *rootdir, bool readonly,
	int secspertrack, int surfaces, int reserved,
	int blocksize, int bootpri,
	TCHAR *filesysdir, int hdc, int flags)
{
	struct uaedev_config_info *uci;
	int i;
	TCHAR *s;

	if (index < 0 && devname && _tcslen (devname) > 0) {
		for (i = 0; i < p->mountitems; i++) {
			if (p->mountconfig[i].devname && !_tcscmp (p->mountconfig[i].devname, devname))
				return 0;
		}
	}

	if (index < 0) {
		uci = getuci(p);
		uci->configoffset = -1;
	} else {
		uci = &p->mountconfig[index];
	}
	if (!uci)
		return 0;

	uci->ishdf = volname == NULL ? 1 : 0;
	_tcscpy (uci->devname, devname ? devname : "");
	_tcscpy (uci->volname, volname ? volname : "");
	_tcscpy (uci->rootdir, rootdir ? rootdir : "");
	validatedevicename (uci->devname);
	validatevolumename (uci->volname);
	uci->readonly = readonly;
	uci->sectors = secspertrack;
	uci->surfaces = surfaces;
	uci->reserved = reserved;
	uci->blocksize = blocksize;
	uci->bootpri = bootpri;
	uci->donotmount = 0;
	uci->autoboot = 0;
	if (bootpri < -128)
		uci->donotmount = 1;
	else if (bootpri >= -127)
		uci->autoboot = 1;
	uci->controller = hdc;
	_tcscpy (uci->filesys, filesysdir ? filesysdir : "");
	if (!uci->devname[0]) {
		TCHAR base[32];
		TCHAR base2[32];
		int num = 0;
		if (uci->rootdir[0] == 0 && !uci->ishdf)
			_tcscpy (base, "RDH");
		else
			_tcscpy (base, "DH");
		_tcscpy (base2, base);
		for (i = 0; i < p->mountitems; i++) {
			_stprintf (base2, "%s%d", base, num);
			if (!_tcscmp(base2, p->mountconfig[i].devname)) {
				num++;
				i = -1;
				continue;
			}
		}
		_tcscpy (uci->devname, base2);
		validatedevicename (uci->devname);
	}
	s = filesys_createvolname (volname, rootdir, "Harddrive");
	_tcscpy (uci->volname, s);
	xfree (s);
	return uci;
}

static void parse_addmem (struct uae_prefs *p, TCHAR *buf, int num)
{
	int size = 0, addr = 0;

	if (!getintval2 (&buf, &addr, ','))
		return;
	if (!getintval2 (&buf, &size, 0))
		return;
	if (addr & 0xffff)
		return;
	if ((size & 0xffff) || (size & 0xffff0000) == 0)
		return;
	p->custom_memory_addrs[num] = addr;
	p->custom_memory_sizes[num] = size;
}

static int cfgfile_parse_hardware (struct uae_prefs *p, const TCHAR *option, TCHAR *value)
{
	int tmpval, dummyint, i;
	bool tmpbool, dummybool;
	TCHAR *section = 0;
	TCHAR tmpbuf[CONFIG_BLEN];

	if (cfgfile_yesno (option, value, "cpu_cycle_exact", &p->cpu_cycle_exact)
		|| cfgfile_yesno (option, value, "blitter_cycle_exact", &p->blitter_cycle_exact)) {
#ifdef JIT
			if (p->cpu_model >= 68020 && p->cachesize > 0)
				p->cpu_cycle_exact = p->blitter_cycle_exact = 0;
			/* we don't want cycle-exact in 68020/40+JIT modes */
#endif
			return 1;
	}
	if (cfgfile_yesno (option, value, "cycle_exact", &tmpbool)) {
		p->cpu_cycle_exact = p->blitter_cycle_exact = tmpbool;
#ifdef JIT
		if (p->cpu_model >= 68020 && p->cachesize > 0)
			p->cpu_cycle_exact = p->blitter_cycle_exact = false;
#endif
		return 1;
	}

	if (cfgfile_string (option, value, "cpu_multiplier", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->cpu_clock_multiplier = (int)(_tstof (tmpbuf) * 256.0);
		return 1;
	}


	if (cfgfile_yesno (option, value, "scsi_a3000", &dummybool)) {
		if (dummybool)
			p->cs_mbdmac = 1;
		return 1;
	}
	if (cfgfile_yesno (option, value, "scsi_a4000t", &dummybool)) {
		if (dummybool)
			p->cs_mbdmac = 2;
		return 1;
	}

	if (cfgfile_string (option, value, "a2065", p->a2065name, sizeof p->a2065name / sizeof (TCHAR)))
		return 1;

	if (cfgfile_yesno (option, value, "immediate_blits", &p->immediate_blits)
		|| cfgfile_yesno (option, value, "waiting_blits", &p->waiting_blits)
		|| cfgfile_yesno (option, value, "cd32cd", &p->cs_cd32cd)
		|| cfgfile_yesno (option, value, "cd32c2p", &p->cs_cd32c2p)
		|| cfgfile_yesno (option, value, "cd32nvram", &p->cs_cd32nvram)
		|| cfgfile_yesno (option, value, "cdtvcd", &p->cs_cdtvcd)
		|| cfgfile_yesno (option, value, "cdtvram", &p->cs_cdtvram)
		|| cfgfile_yesno (option, value, "a1000ram", &p->cs_a1000ram)
		|| cfgfile_yesno (option, value, "pcmcia", &p->cs_pcmcia)
		|| cfgfile_yesno (option, value, "scsi_cdtv", &p->cs_cdtvscsi)
		|| cfgfile_yesno (option, value, "scsi_a4091", &p->cs_a4091)
		|| cfgfile_yesno (option, value, "scsi_a2091", &p->cs_a2091)
		|| cfgfile_yesno (option, value, "cia_overlay", &p->cs_ciaoverlay)
		|| cfgfile_yesno (option, value, "bogomem_fast", &p->cs_slowmemisfast)
		|| cfgfile_yesno (option, value, "ksmirror_e0", &p->cs_ksmirror_e0)
		|| cfgfile_yesno (option, value, "ksmirror_a8", &p->cs_ksmirror_a8)
		|| cfgfile_yesno (option, value, "resetwarning", &p->cs_resetwarning)
		|| cfgfile_yesno (option, value, "denise_noehb", &p->cs_denisenoehb)
		|| cfgfile_yesno (option, value, "ics_agnus", &p->cs_dipagnus)
		|| cfgfile_yesno (option, value, "agnus_bltbusybug", &p->cs_agnusbltbusybug)

		|| cfgfile_yesno (option, value, "kickshifter", &p->kickshifter)
		|| cfgfile_yesno (option, value, "ntsc", &p->ntscmode)
		|| cfgfile_yesno (option, value, "sana2", &p->sana2)
		|| cfgfile_yesno (option, value, "genlock", &p->genlock)
		|| cfgfile_yesno (option, value, "cpu_compatible", &p->cpu_compatible)
		|| cfgfile_yesno (option, value, "cpu_24bit_addressing", &p->address_space_24)
		|| cfgfile_yesno (option, value, "parallel_on_demand", &p->parallel_demand)
		|| cfgfile_yesno (option, value, "parallel_postscript_emulation", &p->parallel_postscript_emulation)
		|| cfgfile_yesno (option, value, "parallel_postscript_detection", &p->parallel_postscript_detection)
		|| cfgfile_yesno (option, value, "serial_on_demand", &p->serial_demand)
		|| cfgfile_yesno (option, value, "serial_hardware_ctsrts", &p->serial_hwctsrts)
		|| cfgfile_yesno (option, value, "serial_direct", &p->serial_direct)
#ifdef JIT
		|| cfgfile_yesno (option, value, "comp_nf", &p->compnf)
		|| cfgfile_yesno (option, value, "comp_constjump", &p->comp_constjump)
		|| cfgfile_yesno (option, value, "comp_oldsegv", &p->comp_oldsegv)
		|| cfgfile_yesno (option, value, "compforcesettings", &dummybool)
		|| cfgfile_yesno (option, value, "compfpu", &p->compfpu)
#endif
#ifdef FPU
		|| cfgfile_yesno (option, value, "fpu_strict", &p->fpu_strict)
#endif
#ifdef JIT
		|| cfgfile_yesno (option, value, "comp_midopt", &p->comp_midopt)
		|| cfgfile_yesno (option, value, "comp_lowopt", &p->comp_lowopt)
#endif
		|| cfgfile_yesno (option, value, "rtg_nocustom", &p->picasso96_nocustom)
		|| cfgfile_yesno (option, value, "uaeserial", &p->uaeserial))
		return 1;

	if (cfgfile_intval (option, value, "serial_stopbits", &p->serial_stopbits, 1)
		|| cfgfile_intval (option, value, "cpu060_revision", &p->cpu060_revision, 1)
		|| cfgfile_intval (option, value, "fpu_revision", &p->fpu_revision, 1)
		|| cfgfile_intval (option, value, "cdtvramcard", &p->cs_cdtvcard, 1)
		|| cfgfile_intval (option, value, "fatgary", &p->cs_fatgaryrev, 1)
		|| cfgfile_intval (option, value, "ramsey", &p->cs_ramseyrev, 1)
		|| cfgfile_doubleval (option, value, "chipset_refreshrate", &p->chipset_refreshrate)
		|| cfgfile_intval (option, value, "fastmem_size", &p->fastmem_size, 0x100000)
		|| cfgfile_intval (option, value, "a3000mem_size", &p->mbresmem_low_size, 0x100000)
		|| cfgfile_intval (option, value, "mbresmem_size", &p->mbresmem_high_size, 0x100000)
		|| cfgfile_intval (option, value, "z3mem_size", &p->z3fastmem_size, 0x100000)
		|| cfgfile_intval (option, value, "z3mem2_size", &p->z3fastmem2_size, 0x100000)
		|| cfgfile_intval (option, value, "megachipmem_size", &p->z3chipmem_size, 0x100000)
		|| cfgfile_intval (option, value, "z3mem_start", &p->z3fastmem_start, 1)
		|| cfgfile_intval (option, value, "bogomem_size", &p->bogomem_size, 0x40000)
		|| cfgfile_intval (option, value, "gfxcard_size", &p->gfxmem_size, 0x100000)
		|| cfgfile_intval (option, value, "rtg_modes", &p->picasso96_modeflags, 1)
		|| cfgfile_intval (option, value, "floppy_speed", &p->floppy_speed, 1)
		|| cfgfile_intval (option, value, "floppy_write_length", &p->floppy_write_length, 1)
		|| cfgfile_intval (option, value, "floppy_random_bits_min", &p->floppy_random_bits_min, 1)
		|| cfgfile_intval (option, value, "floppy_random_bits_max", &p->floppy_random_bits_max, 1)
		|| cfgfile_intval (option, value, "nr_floppies", &p->nr_floppies, 1)
		|| cfgfile_intval (option, value, "floppy0type", &p->floppyslots[0].dfxtype, 1)
		|| cfgfile_intval (option, value, "floppy1type", &p->floppyslots[1].dfxtype, 1)
		|| cfgfile_intval (option, value, "floppy2type", &p->floppyslots[2].dfxtype, 1)
		|| cfgfile_intval (option, value, "floppy3type", &p->floppyslots[3].dfxtype, 1)
		|| cfgfile_intval (option, value, "maprom", &p->maprom, 1)
		|| cfgfile_intval (option, value, "parallel_autoflush", &p->parallel_autoflush_time, 1)
		|| cfgfile_intval (option, value, "uae_hide", &p->uae_hide, 1)
		|| cfgfile_intval (option, value, "cpu_frequency", &p->cpu_frequency, 1)
		|| cfgfile_intval (option, value, "kickstart_ext_rom_file2addr", &p->romextfile2addr, 1)
		|| cfgfile_intval (option, value, "catweasel", &p->catweasel, 1))
	return 1;

#ifdef JIT
	if (cfgfile_intval (option, value, "cachesize", &p->cachesize, 1)
#ifdef NATMEM_OFFSET
	 || cfgfile_strval (option, value, "comp_trustbyte",  &p->comptrustbyte,  compmode, 0)
	 || cfgfile_strval (option, value, "comp_trustword",  &p->comptrustword,  compmode, 0)
	 || cfgfile_strval (option, value, "comp_trustlong",  &p->comptrustlong,  compmode, 0)
	 || cfgfile_strval (option, value, "comp_trustnaddr", &p->comptrustnaddr, compmode, 0)
#else
	 || cfgfile_strval (option, value, "comp_trustbyte",  &p->comptrustbyte,  compmode, 1)
	 || cfgfile_strval (option, value, "comp_trustword",  &p->comptrustword,  compmode, 1)
	 || cfgfile_strval (option, value, "comp_trustlong",  &p->comptrustlong,  compmode, 1)
	 || cfgfile_strval (option, value, "comp_trustnaddr", &p->comptrustnaddr, compmode, 1)
#endif
	 || cfgfile_strval (option, value, "comp_flushmode", &p->comp_hardflush, flushmode, 0))
	return 1;
#endif

	if (cfgfile_strval (option, value, "chipset_compatible", &p->cs_compatible, cscompa, 0)
	 || cfgfile_strval (option, value, "rtc", &p->cs_rtc, rtctype, 0)
	 || cfgfile_strval (option, value, "ciaatod", &p->cs_ciaatod, ciaatodmode, 0)
	 || cfgfile_strval (option, value, "ide", &p->cs_ide, idemode, 0)
	 || cfgfile_strval (option, value, "scsi", &p->scsi, scsimode, 0)
	 || cfgfile_strval (option, value, "collision_level", &p->collision_level, collmode, 0)
	 || cfgfile_strval (option, value, "parallel_matrix_emulation", &p->parallel_matrix_emulation, epsonprinter, 0))
	return 1;

	if (cfgfile_path (option, value, "kickstart_rom_file", p->romfile, sizeof p->romfile / sizeof (TCHAR))
	 || cfgfile_path (option, value, "kickstart_ext_rom_file", p->romextfile, sizeof p->romextfile / sizeof (TCHAR))
	 || cfgfile_path (option, value, "kickstart_ext_rom_file2", p->romextfile2, sizeof p->romextfile2 / sizeof (TCHAR))
	 || cfgfile_rom (option, value, "kickstart_rom_file_id", p->romfile, sizeof p->romfile / sizeof (TCHAR))
	 || cfgfile_rom (option, value, "kickstart_ext_rom_file_id", p->romextfile, sizeof p->romextfile / sizeof (TCHAR))
	 || cfgfile_path (option, value, "amax_rom_file", p->amaxromfile, sizeof p->amaxromfile / sizeof (TCHAR))
	 || cfgfile_path (option, value, "flash_file", p->flashfile, sizeof p->flashfile / sizeof (TCHAR))
	 || cfgfile_path (option, value, "cart_file", p->cartfile, sizeof p->cartfile / sizeof (TCHAR))
	 || cfgfile_string (option, value, "pci_devices", p->pci_devices, sizeof p->pci_devices / sizeof (TCHAR))
	 || cfgfile_string (option, value, "ghostscript_parameters", p->ghostscript_parameters, sizeof p->ghostscript_parameters / sizeof (TCHAR)))
	return 1;

	if (cfgfile_strval (option, value, "cart_internal", &p->cart_internal, cartsmode, 0)) {
		if (p->cart_internal) {
			struct romdata *rd = getromdatabyid (63);
			if (rd)
				_stprintf (p->cartfile, ":%s", rd->configname);
		}
		return 1;
	}
	if (cfgfile_string (option, value, "kickstart_rom", p->romident, sizeof p->romident / sizeof (TCHAR))) {
		decode_rom_ident (p->romfile, sizeof p->romfile / sizeof (TCHAR), p->romident, ROMTYPE_ALL_KICK);
		return 1;
	}
	if (cfgfile_string (option, value, "kickstart_ext_rom", p->romextident, sizeof p->romextident / sizeof (TCHAR))) {
		decode_rom_ident (p->romextfile, sizeof p->romextfile / sizeof (TCHAR), p->romextident, ROMTYPE_ALL_EXT);
		return 1;
	}
	if (cfgfile_string (option, value, "cart", p->cartident, sizeof p->cartident / sizeof (TCHAR))) {
		decode_rom_ident (p->cartfile, sizeof p->cartfile / sizeof (TCHAR), p->cartident, ROMTYPE_ALL_CART);
		return 1;
	}

	for (i = 0; i < 4; i++) {
		_stprintf (tmpbuf, "floppy%d", i);
		if (cfgfile_path (option, value, tmpbuf, p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof (TCHAR)))
			return 1;
	}

	if (cfgfile_intval (option, value, "chipmem_size", &dummyint, 1)) {
		if (dummyint < 0)
			p->chipmem_size = 0x20000; /* 128k, prototype support */
		else if (dummyint == 0)
			p->chipmem_size = 0x40000; /* 256k */
		else
			p->chipmem_size = dummyint * 0x80000;
		return 1;
	}

	if (cfgfile_string (option, value, "addmem1", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 0);
		return 1;
	}
	if (cfgfile_string (option, value, "addmem2", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 1);
		return 1;
	}

	if (cfgfile_strval (option, value, "chipset", &tmpval, csmode, 0)) {
		set_chipset_mask (p, tmpval);
		return 1;
	}

	if (cfgfile_string (option, value, "mmu_model", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->mmu_model = _tstol(tmpbuf);
		return 1;
	}

	if (cfgfile_string (option, value, "fpu_model", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->fpu_model = _tstol(tmpbuf);
		return 1;
	}

	if (cfgfile_string (option, value, "cpu_model", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->cpu_model = _tstol(tmpbuf);
		p->fpu_model = 0;
		return 1;
	}

	/* old-style CPU configuration */
	if (cfgfile_string (option, value, "cpu_type", tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->fpu_model = 0;
		p->address_space_24 = 0;
		p->cpu_model = 680000;
		if (!_tcscmp (tmpbuf, "68000")) {
			p->cpu_model = 68000;
		} else if (!_tcscmp (tmpbuf, "68010")) {
			p->cpu_model = 68010;
		} else if (!_tcscmp (tmpbuf, "68ec020")) {
			p->cpu_model = 68020;
			p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, "68020")) {
			p->cpu_model = 68020;
		} else if (!_tcscmp (tmpbuf, "68ec020/68881")) {
			p->cpu_model = 68020;
			p->fpu_model = 68881;
			p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, "68020/68881")) {
			p->cpu_model = 68020;
			p->fpu_model = 68881;
		} else if (!_tcscmp (tmpbuf, "68040")) {
			p->cpu_model = 68040;
			p->fpu_model = 68040;
		} else if (!_tcscmp (tmpbuf, "68060")) {
			p->cpu_model = 68060;
			p->fpu_model = 68060;
		}
		return 1;
	}

	if (p->config_version < (21 << 16)) {
		if (cfgfile_strval (option, value, "cpu_speed", &p->m68k_speed, speedmode, 1)
			/* Broken earlier versions used to write this out as a string.  */
			|| cfgfile_strval (option, value, "finegraincpu_speed", &p->m68k_speed, speedmode, 1))
		{
			p->m68k_speed--;
			return 1;
		}
	}

	if (cfgfile_intval (option, value, "cpu_speed", &p->m68k_speed, 1)) {
		p->m68k_speed *= CYCLE_UNIT;
		return 1;
	}

	if (cfgfile_intval (option, value, "finegrain_cpu_speed", &p->m68k_speed, 1)) {
		if (OFFICIAL_CYCLE_UNIT > CYCLE_UNIT) {
			int factor = OFFICIAL_CYCLE_UNIT / CYCLE_UNIT;
			p->m68k_speed = (p->m68k_speed + factor - 1) / factor;
		}
		if (strcasecmp (value, "max") == 0)
			p->m68k_speed = -1;
		return 1;
	}

	if (cfgfile_intval (option, value, "dongle", &p->dongle, 1)) {
		if (p->dongle == 0)
			cfgfile_strval (option, value, "dongle", &p->dongle, dongles, 0);
		return 1;
	}

	if (strcasecmp (option, "quickstart") == 0) {
		int model = 0;
		TCHAR *tmpp = _tcschr (value, ',');
		if (tmpp) {
			*tmpp++ = 0;
			TCHAR *tmpp2 = _tcschr (value, ',');
			if (tmpp2)
				*tmpp2 = 0;
			cfgfile_strval (option, value, option, &model, qsmodes,  0);
			if (model >= 0) {
				int config = _tstol (tmpp);
				built_in_prefs (p, model, config, 0, 0);
			}
		}
		return 1;
	}

	for (i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
		TCHAR tmp[100];
		_stprintf (tmp, "uaehf%d", i);
		if (_tcscmp (option, tmp) == 0)
			return 1;
	}

	if (_tcscmp (option, "filesystem") == 0
		|| _tcscmp (option, "hardfile") == 0)
	{
		int secs, heads, reserved, bs;
		bool ro;
		TCHAR *aname, *root;
		TCHAR *tmpp = _tcschr (value, ',');
		TCHAR *str;

		if (config_newfilesystem)
			return 1;

		if (tmpp == 0)
			goto invalid_fs;

		*tmpp++ = '\0';
		if (_tcscmp (value, "1") == 0 || strcasecmp (value, "ro") == 0
			|| strcasecmp (value, "readonly") == 0
			|| strcasecmp (value, "read-only") == 0)
			ro = true;
		else if (_tcscmp (value, "0") == 0 || strcasecmp (value, "rw") == 0
			|| strcasecmp (value, "readwrite") == 0
			|| strcasecmp (value, "read-write") == 0)
			ro = false;
		else
			goto invalid_fs;
		secs = 0; heads = 0; reserved = 0; bs = 0;

		value = tmpp;
		if (_tcscmp (option, "filesystem") == 0) {
			tmpp = _tcschr (value, ':');
			if (tmpp == 0)
				goto invalid_fs;
			*tmpp++ = '\0';
			aname = value;
			root = tmpp;
		} else {
			if (! getintval (&value, &secs, ',')
				|| ! getintval (&value, &heads, ',')
				|| ! getintval (&value, &reserved, ',')
				|| ! getintval (&value, &bs, ','))
				goto invalid_fs;
			root = value;
			aname = 0;
		}
		str = cfgfile_subst_path_load (UNEXPANDED, &p->path_hardfile, root, true);
#ifdef FILESYS
		add_filesys_config (p, -1, NULL, aname, str, ro, secs, heads, reserved, bs, 0, NULL, 0, 0);
#endif
		free (str);
		return 1;

	}

	if (_tcscmp (option, "filesystem2") == 0
		|| _tcscmp (option, "hardfile2") == 0)
	{
		int secs, heads, reserved, bs, bp, hdcv;
		bool ro;
		TCHAR *dname = NULL, *aname = "", *root = NULL, *fs = NULL, *hdc;
		TCHAR *tmpp = _tcschr (value, ',');
		TCHAR *str = NULL;

		config_newfilesystem = 1;
		if (tmpp == 0)
			goto invalid_fs;

		*tmpp++ = '\0';
		if (strcasecmp (value, "ro") == 0)
			ro = true;
		else if (strcasecmp (value, "rw") == 0)
			ro = false;
		else
			goto invalid_fs;
		secs = 0; heads = 0; reserved = 0; bs = 0; bp = 0;
		fs = 0; hdc = 0; hdcv = 0;

		value = tmpp;
		if (_tcscmp (option, "filesystem2") == 0) {
			tmpp = _tcschr (value, ':');
			if (tmpp == 0)
				goto empty_fs;
			*tmpp++ = 0;
			dname = value;
			aname = tmpp;
			tmpp = _tcschr (tmpp, ':');
			if (tmpp == 0)
				goto empty_fs;
			*tmpp++ = 0;
			root = tmpp;
			tmpp = _tcschr (tmpp, ',');
			if (tmpp == 0)
				goto empty_fs;
			*tmpp++ = 0;
			if (! getintval (&tmpp, &bp, 0))
				goto empty_fs;
		} else {
			tmpp = _tcschr (value, ':');
			if (tmpp == 0)
				goto invalid_fs;
			*tmpp++ = '\0';
			dname = value;
			root = tmpp;
			tmpp = _tcschr (tmpp, ',');
			if (tmpp == 0)
				goto invalid_fs;
			*tmpp++ = 0;
			aname = 0;
			if (! getintval (&tmpp, &secs, ',')
				|| ! getintval (&tmpp, &heads, ',')
				|| ! getintval (&tmpp, &reserved, ',')
				|| ! getintval (&tmpp, &bs, ','))
				goto invalid_fs;
			if (getintval2 (&tmpp, &bp, ',')) {
				fs = tmpp;
				tmpp = _tcschr (tmpp, ',');
				if (tmpp != 0) {
					*tmpp++ = 0;
					hdc = tmpp;
					if(_tcslen (hdc) >= 4 && !_tcsncmp (hdc, "ide", 3)) {
						hdcv = hdc[3] - '0' + HD_CONTROLLER_IDE0;
						if (hdcv < HD_CONTROLLER_IDE0 || hdcv > HD_CONTROLLER_IDE3)
							hdcv = 0;
					}
					if(_tcslen (hdc) >= 5 && !_tcsncmp (hdc, "scsi", 4)) {
						hdcv = hdc[4] - '0' + HD_CONTROLLER_SCSI0;
						if (hdcv < HD_CONTROLLER_SCSI0 || hdcv > HD_CONTROLLER_SCSI6)
							hdcv = 0;
					}
					if (_tcslen (hdc) >= 6 && !_tcsncmp (hdc, "scsram", 6))
						hdcv = HD_CONTROLLER_PCMCIA_SRAM;
				}
			}
		}
empty_fs:
		if (root) {
			if (_tcslen (root) > 3 && root[0] == 'H' && root[1] == 'D' && root[2] == '_') {
				root += 2;
				*root = ':';
			}
			str = cfgfile_subst_path_load (UNEXPANDED, &p->path_hardfile, root, false);
		}
#ifdef FILESYS
		add_filesys_config (p, -1, dname, aname, str, ro, secs, heads, reserved, bs, bp, fs, hdcv, 0);
#endif
	//FIXME:	xfree (str);
		return 1;

invalid_fs:
		write_log ("Invalid filesystem/hardfile specification.\n");
		return 1;
	}

	return 0;
}

int cfgfile_parse_option (struct uae_prefs *p, TCHAR *option, TCHAR *value, int type)
{
	if (!_tcscmp (option, "config_hardware"))
		return 1;
	if (!_tcscmp (option, "config_host"))
		return 1;
	if (cfgfile_path (option, value, "config_hardware_path", p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR)))
		return 1;
	if (cfgfile_path (option, value, "config_host_path", p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR)))
		return 1;
	if (type == 0 || (type & CONFIG_TYPE_HARDWARE)) {
		if (cfgfile_parse_hardware (p, option, value))
			return 1;
	}
	if (type == 0 || (type & CONFIG_TYPE_HOST)) {
		if (cfgfile_parse_host (p, option, value))
			return 1;
	}
	if (type > 0)
		return 1;
	return 0;
}

static int isutf8ext (TCHAR *s)
{
	if (_tcslen (s) > _tcslen (UTF8NAME) && !_tcscmp (s + _tcslen (s) - _tcslen (UTF8NAME), UTF8NAME)) {
		s[_tcslen (s) - _tcslen (UTF8NAME)] = 0;
		return 1;
	}
	return 0;
}

static int cfgfile_separate_linea (char *line, TCHAR *line1b, TCHAR *line2b)
{
	char *line1, *line2;
	int i;

	line1 = line;
	line2 = strchr (line, '=');
	if (! line2) {
		write_log ("CFGFILE: linea was incomplete with only %s\n", line1);
		return 0;
	}
	*line2++ = '\0';

	/* Get rid of whitespace.  */
	i = strlen (line2);
	while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		|| line2[i - 1] == '\r' || line2[i - 1] == '\n'))
		line2[--i] = '\0';
	line2 += strspn (line2, "\t \r\n");

	i = strlen (line);
	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		|| line[i - 1] == '\r' || line[i - 1] == '\n'))
		line[--i] = '\0';
	line += strspn (line, "\t \r\n");
	au_copy (line1b, MAX_DPATH, line);
	if (isutf8ext (line1b)) {
		if (line2[0]) {
			_tcscpy (line2b, line2);
		}
	} else {
		au_copy (line2b, MAX_DPATH, line2);
	}
	return 1;
}

static int cfgfile_separate_line (TCHAR *line, TCHAR *line1b, TCHAR *line2b)
{
	TCHAR *line1, *line2;
	int i;

	line1 = line;
	line2 = _tcschr (line, '=');
	if (! line2) {
		write_log ("CFGFILE: line was incomplete with only %s\n", line1);
		return 0;
	}
	*line2++ = '\0';

	/* Get rid of whitespace.  */
	i = _tcslen (line2);
	while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		|| line2[i - 1] == '\r' || line2[i - 1] == '\n'))
		line2[--i] = '\0';
	line2 += _tcsspn (line2, "\t \r\n");
	_tcscpy (line2b, line2);
	i = _tcslen (line);
	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		|| line[i - 1] == '\r' || line[i - 1] == '\n'))
		line[--i] = '\0';
	line += _tcsspn (line, "\t \r\n");
	_tcscpy (line1b, line);

	if (line2b[0] == '"' || line2b[0] == '\"') {
		TCHAR c = line2b[0];
		int i = 0;
		memmove (line2b, line2b + 1, (_tcslen (line2b) + 1) * sizeof (TCHAR));
		while (line2b[i] != 0 && line2b[i] != c)
			i++;
		line2b[i] = 0;
	}

	if (isutf8ext (line1b))
		return 0;
	return 1;
}

static int isobsolete (TCHAR *s)
{
	int i = 0;
	while (obsolete[i]) {
		if (!strcasecmp (s, obsolete[i])) {
			write_log ("obsolete config entry '%s'\n", s);
			return 1;
		}
		i++;
	}
	if (_tcslen (s) > 2 && !_tcsncmp (s, "w.", 2))
		return 1;
	if (_tcslen (s) >= 10 && !_tcsncmp (s, "gfx_opengl", 10)) {
		write_log ("obsolete config entry '%s\n", s);
		return 1;
	}
	if (_tcslen (s) >= 6 && !_tcsncmp (s, "gfx_3d", 6)) {
		write_log ("obsolete config entry '%s\n", s);
		return 1;
	}
	return 0;
}

static void cfgfile_parse_separated_line (struct uae_prefs *p, TCHAR *line1b, TCHAR *line2b, int type)
{
	TCHAR line3b[CONFIG_BLEN], line4b[CONFIG_BLEN];
	struct strlist *sl;
	int ret;

	_tcscpy (line3b, line1b);
	_tcscpy (line4b, line2b);
	ret = cfgfile_parse_option (p, line1b, line2b, type);
	if (!isobsolete (line3b)) {
		for (sl = p->all_lines; sl; sl = sl->next) {
			if (sl->option && !strcasecmp (line1b, sl->option)) break;
		}
		if (!sl) {
			struct strlist *u = xcalloc (struct strlist, 1);
			u->option = my_strdup (line3b);
			u->value = my_strdup (line4b);
			u->next = p->all_lines;
			p->all_lines = u;
			if (!ret) {
				u->unknown = 1;
				write_log ("unknown config entry: '%s=%s'\n", u->option, u->value);
			}
		}
	}
}

void cfgfile_parse_lines (struct uae_prefs *p, const TCHAR *lines, int type)
{
  TCHAR *buf = my_strdup (lines);
  TCHAR *t = buf;
  for (;;) {
    if (_tcslen (t) == 0)
      break;
    TCHAR *t2 = _tcschr (t, '\n');
    if (t2)
      *t2 = 0;
    cfgfile_parse_line (p, t, type);
    if (!t2)
      break;
    t = t2 + 1;
  }
  xfree (buf);
}

void cfgfile_parse_line (struct uae_prefs *p, TCHAR *line, int type)
{
	TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

	if (!cfgfile_separate_line (line, line1b, line2b))
		return;
	cfgfile_parse_separated_line (p, line1b, line2b, type);
}

static void subst (TCHAR *p, TCHAR *f, int n)
{
	TCHAR *str = cfgfile_subst_path (UNEXPANDED, p, f);
	_tcsncpy (f, str, n - 1);
	f[n - 1] = '\0';
	//FIXME: free (str);
}

static char *cfg_fgets (char *line, int max, struct zfile *fh)
{
#ifdef SINGLEFILE
	extern TCHAR singlefile_config[];
	static TCHAR *sfile_ptr;
	TCHAR *p;
#endif

	if (fh)
		return zfile_fgetsa (line, max, fh);
#ifdef SINGLEFILE
	if (sfile_ptr == 0) {
		sfile_ptr = singlefile_config;
		if (*sfile_ptr) {
			write_log ("singlefile config found\n");
			while (*sfile_ptr++);
		}
	}
	if (*sfile_ptr == 0) {
		sfile_ptr = singlefile_config;
		return 0;
	}
	p = sfile_ptr;
	while (*p != 13 && *p != 10 && *p != 0) p++;
	memset (line, 0, max);
	memcpy (line, sfile_ptr, (p - sfile_ptr) * sizeof (TCHAR));
	sfile_ptr = p + 1;
	if (*sfile_ptr == 13)
		sfile_ptr++;
	if (*sfile_ptr == 10)
		sfile_ptr++;
	return line;
#endif
	return 0;
}

static int cfgfile_load_2 (struct uae_prefs *p, const TCHAR *filename, bool real, int *type)
{
	int i;
	struct zfile *fh;
	char linea[CONFIG_BLEN];
	TCHAR line[CONFIG_BLEN], line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];
	struct strlist *sl;
	bool type1 = false, type2 = false;
	int askedtype = 0;

	if (type) {
		askedtype = *type;
		*type = 0;
	}
	if (real) {
		p->config_version = 0;
		config_newfilesystem = 0;
		//reset_inputdevice_config (p);
	}

	write_log ("Opening cfgfile '%s': ", filename);
	fh = zfile_fopen (filename, "r", ZFD_NORMAL);
#ifndef	SINGLEFILE
	if (! fh) {
		write_log ("failed\n");
		return 0;
	}
#endif
	write_log ("OK\n");

	while (cfg_fgets (linea, sizeof (linea), fh) != 0) {
		trimwsa (linea);
		if (strlen (linea) > 0) {
			if (linea[0] == '#' || linea[0] == ';') {
				struct strlist *u = xcalloc (struct strlist, 1);
				u->option = NULL;
				u->value = my_strdup (linea);
				u->unknown = 1;
				u->next = p->all_lines;
				p->all_lines = u;
				continue;
			}
			if (!cfgfile_separate_linea (linea, line1b, line2b))
				continue;
			type1 = type2 = 0;
			if (cfgfile_yesno (line1b, line2b, "config_hardware", &type1) ||
				cfgfile_yesno (line1b, line2b, "config_host", &type2)) {
					if (type1 && type)
						*type |= CONFIG_TYPE_HARDWARE;
					if (type2 && type)
						*type |= CONFIG_TYPE_HOST;
					continue;
			}
			if (real) {
				cfgfile_parse_separated_line (p, line1b, line2b, askedtype);
			} else {
				cfgfile_string (line1b, line2b, "config_description", p->description, sizeof p->description / sizeof (TCHAR));
				cfgfile_path (line1b, line2b, "config_hardware_path", p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR));
				cfgfile_path (line1b, line2b, "config_host_path", p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR));
				cfgfile_string (line1b, line2b, "config_window_title", p->config_window_title, sizeof p->config_window_title / sizeof (TCHAR));
			}
		}
	}

	if (type && *type == 0)
		*type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
	if (fh)
		zfile_fclose (fh);

	if (!real)
		return 1;

	for (sl = temp_lines; sl; sl = sl->next) {
		_stprintf (line, "%s=%s", sl->option, sl->value);
		cfgfile_parse_line (p, line, 0);
	}

	for (i = 0; i < 4; i++)
		subst (p->path_floppy.path[0], p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romfile, sizeof p->romfile / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romextfile, sizeof p->romextfile / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romextfile2, sizeof p->romextfile2 / sizeof (TCHAR));

	return 1;
}

int cfgfile_load (struct uae_prefs *p, const TCHAR *filename, int *type, int ignorelink, int userconfig)
{
	int v;
	TCHAR tmp[MAX_DPATH];
	int type2;
	static int recursive;

	if (recursive > 1)
		return 0;
	recursive++;
	write_log ("load config '%s':%d\n", filename, type ? *type : -1);
	v = cfgfile_load_2 (p, filename, 1, type);
	if (!v) {
		write_log ("load failed\n");
		goto end;
	}


	if (!ignorelink) {
		if (p->config_hardware_path[0]) {
			fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
			_tcsncat (tmp, p->config_hardware_path, sizeof (tmp) / sizeof (TCHAR));
			type2 = CONFIG_TYPE_HARDWARE;
			cfgfile_load (p, tmp, &type2, 1, 0);
		}
		if (p->config_host_path[0]) {
			fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
			_tcsncat (tmp, p->config_host_path, sizeof (tmp) / sizeof (TCHAR));
			type2 = CONFIG_TYPE_HOST;
			cfgfile_load (p, tmp, &type2, 1, 0);
		}
	}
end:
	recursive--;
	fixup_prefs (p);
	return v;
}

void cfgfile_backup (const TCHAR *path)
{
	TCHAR dpath[MAX_DPATH];

	fetch_configurationpath (dpath, sizeof (dpath) / sizeof (TCHAR));
	_tcscat (dpath, "configuration.backup");
	//bool hidden = my_isfilehidden (dpath);
	//my_unlink (dpath);
	//my_rename (path, dpath);
	//if (hidden)
		//my_setfilehidden (dpath, hidden);
}

int cfgfile_save (struct uae_prefs *p, const TCHAR *filename, int type)
{
	struct zfile *fh;

	cfgfile_backup (filename);
	fh = zfile_fopen (filename, unicode_config ? "w, ccs=UTF-8" : "w", ZFD_NORMAL);
	if (! fh)
		return 0;

	if (!type)
		type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
	cfgfile_save_options (fh, p, type);
	zfile_fclose (fh);
	return 1;
}

int cfgfile_get_description (const TCHAR *filename, TCHAR *description, TCHAR *hostlink, TCHAR *hardwarelink, int *type)
{
	int result = 0;
	struct uae_prefs *p = xmalloc (struct uae_prefs, 1);

	p->description[0] = 0;
	p->config_host_path[0] = 0;
	p->config_hardware_path[0] = 0;
	if (cfgfile_load_2 (p, filename, 0, type)) {
		result = 1;
		if (description)
			_tcscpy (description, p->description);
		if (hostlink)
			_tcscpy (hostlink, p->config_host_path);
		if (hardwarelink)
			_tcscpy (hardwarelink, p->config_hardware_path);
	}
	xfree (p);
	return result;
}

int cfgfile_configuration_change (int v)
{
	static int mode;
	if (v >= 0)
		mode = v;
	return mode;
}

void cfgfile_show_usage (void)
{
	unsigned int i;
	write_log ("UAE Configuration Help:\n" \
		"=======================\n");
	for (i = 0; i < sizeof opttable / sizeof *opttable; i++)
		write_log ("%s: %s\n", opttable[i].config_label, opttable[i].config_help);
}

/* This implements the old commandline option parsing.  I've re-added this
   because the new way of doing things is painful for me (it requires me
   to type a couple hundred characters when invoking UAE).  The following
   is far less annoying to use.  */
static void parse_gfx_specs (struct uae_prefs *p, const TCHAR *spec)
{
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2;

	x1 = _tcschr (x0, ':');
	if (x1 == 0)
		goto argh;
	x2 = _tcschr (x1+1, ':');
	if (x2 == 0)
		goto argh;
	*x1++ = 0; *x2++ = 0;

	p->gfx_size_win.width = p->gfx_size_fs.width = _tstoi (x0);
	p->gfx_size_win.height = p->gfx_size_fs.height = _tstoi (x1);
	p->gfx_resolution = _tcschr (x2, 'l') != 0 ? 1 : 0;
	p->gfx_xcenter = _tcschr (x2, 'x') != 0 ? 1 : _tcschr (x2, 'X') != 0 ? 2 : 0;
	p->gfx_ycenter = _tcschr (x2, 'y') != 0 ? 1 : _tcschr (x2, 'Y') != 0 ? 2 : 0;
	p->gfx_vresolution = _tcschr (x2, 'd') != 0 ? VRES_DOUBLE : VRES_NONDOUBLE;
	p->gfx_scanlines = _tcschr (x2, 'D') != 0;
	if (p->gfx_scanlines)
		p->gfx_vresolution = VRES_DOUBLE;
	p->gfx_afullscreen = _tcschr (x2, 'a') != 0;
	p->gfx_pfullscreen = _tcschr (x2, 'p') != 0;

	free (x0);
	return;

argh:
	write_log ("Bad display mode specification.\n");
	write_log ("The format to use is: \"width:height:modifiers\"\n");
	write_log ("Type \"uae -h\" for detailed help.\n");
	free (x0);
}

static void parse_sound_spec (struct uae_prefs *p, const TCHAR *spec)
{
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2 = NULL, *x3 = NULL, *x4 = NULL, *x5 = NULL;

	x1 = _tcschr (x0, ':');
	if (x1 != NULL) {
		*x1++ = '\0';
		x2 = _tcschr (x1 + 1, ':');
		if (x2 != NULL) {
			*x2++ = '\0';
			x3 = _tcschr (x2 + 1, ':');
			if (x3 != NULL) {
				*x3++ = '\0';
				x4 = _tcschr (x3 + 1, ':');
				if (x4 != NULL) {
					*x4++ = '\0';
					x5 = _tcschr (x4 + 1, ':');
				}
			}
		}
	}
	p->produce_sound = _tstoi (x0);
	if (x1) {
		p->sound_stereo_separation = 0;
		if (*x1 == 'S') {
			p->sound_stereo = SND_STEREO;
			p->sound_stereo_separation = 7;
		} else if (*x1 == 's')
			p->sound_stereo = SND_STEREO;
		else
			p->sound_stereo = SND_MONO;
	}
	if (x3)
		p->sound_freq = _tstoi (x3);
	if (x4)
		p->sound_maxbsiz = _tstoi (x4);
	free (x0);
}


static void parse_joy_spec (struct uae_prefs *p, const TCHAR *spec)
{
	int v0 = 2, v1 = 0;
	if (_tcslen(spec) != 2)
		goto bad;

	switch (spec[0]) {
	case '0': v0 = JSEM_JOYS; break;
	case '1': v0 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v0 = JSEM_MICE; break;
	case 'A': case 'a': v0 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v0 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v0 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}

	switch (spec[1]) {
	case '0': v1 = JSEM_JOYS; break;
	case '1': v1 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v1 = JSEM_MICE; break;
	case 'A': case 'a': v1 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v1 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v1 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}
	if (v0 == v1)
		goto bad;
	/* Let's scare Pascal programmers */
	if (0)
bad:
	write_log ("Bad joystick mode specification. Use -J xy, where x and y\n"
		"can be 0 for joystick 0, 1 for joystick 1, M for mouse, and\n"
		"a, b or c for different keyboard settings.\n");

	p->jports[0].id = v0;
	p->jports[1].id = v1;
}

static void parse_filesys_spec (struct uae_prefs *p, bool readonly, const TCHAR *spec)
{
	TCHAR buf[256];
	TCHAR *s2;

	_tcsncpy (buf, spec, 255); buf[255] = 0;
	s2 = _tcschr (buf, ':');
	if (s2) {
		*s2++ = '\0';
#ifdef __DOS__
		{
			TCHAR *tmp;

			while ((tmp = _tcschr (s2, '\\')))
				*tmp = '/';
		}
#endif
#ifdef FILESYS
		add_filesys_config (p, -1, NULL, buf, s2, readonly, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
	} else {
		write_log ("Usage: [-m | -M] VOLNAME:mount_point\n");
	}
}

static void parse_hardfile_spec (struct uae_prefs *p, const TCHAR *spec)
{
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2, *x3, *x4;

	x1 = _tcschr (x0, ':');
	if (x1 == NULL)
		goto argh;
	*x1++ = '\0';
	x2 = _tcschr (x1 + 1, ':');
	if (x2 == NULL)
		goto argh;
	*x2++ = '\0';
	x3 = _tcschr (x2 + 1, ':');
	if (x3 == NULL)
		goto argh;
	*x3++ = '\0';
	x4 = _tcschr (x3 + 1, ':');
	if (x4 == NULL)
		goto argh;
	*x4++ = '\0';
#ifdef FILESYS
	add_filesys_config (p, -1, NULL, NULL, x4, 0, _tstoi (x0), _tstoi (x1), _tstoi (x2), _tstoi (x3), 0, 0, 0, 0);
#endif
	free (x0);
	return;

argh:
	free (x0);
	write_log ("Bad hardfile parameter specified - type \"uae -h\" for help.\n");
	return;
}

static void parse_cpu_specs (struct uae_prefs *p, const TCHAR *spec)
{
	if (*spec < '0' || *spec > '4') {
		write_log ("CPU parameter string must begin with '0', '1', '2', '3' or '4'.\n");
		return;
	}

	p->cpu_model = (*spec++) * 10 + 68000;
	p->address_space_24 = p->cpu_model < 68020;
	p->cpu_compatible = 0;
	while (*spec != '\0') {
		switch (*spec) {
		case 'a':
			if (p->cpu_model < 68020)
				write_log ("In 68000/68010 emulation, the address space is always 24 bit.\n");
			else if (p->cpu_model >= 68040)
				write_log ("In 68040/060 emulation, the address space is always 32 bit.\n");
			else
				p->address_space_24 = 1;
			break;
		case 'c':
			if (p->cpu_model != 68000)
				write_log ("The more compatible CPU emulation is only available for 68000\n"
				"emulation, not for 68010 upwards.\n");
			else
				p->cpu_compatible = 1;
			break;
		default:
			write_log ("Bad CPU parameter specified - type \"uae -h\" for help.\n");
			break;
		}
		spec++;
	}
}

static void cmdpath (TCHAR *dst, const TCHAR *src, int maxsz)
{
	_tcsncpy (dst, src, maxsz);
	dst[maxsz] = 0;
}

/* Returns the number of args used up (0 or 1).  */
int parse_cmdline_option (struct uae_prefs *p, TCHAR c, const TCHAR *arg)
{
	struct strlist *u = xcalloc (struct strlist, 1);
	const TCHAR arg_required[] = "0123rKpImWSAJwNCZUFcblOdHRv";

	if (_tcschr (arg_required, c) && ! arg) {
		write_log ("Missing argument for option `-%c'!\n", c);
		return 0;
	}

	u->option = xmalloc (TCHAR, 2);
	u->option[0] = c;
	u->option[1] = 0;
	u->value = my_strdup (arg);
	u->next = p->all_lines;
	p->all_lines = u;

	switch (c) {
	case 'h': usage (); exit (0);

	case '0': cmdpath (p->floppyslots[0].df, arg, 255); break;
	case '1': cmdpath (p->floppyslots[1].df, arg, 255); break;
	case '2': cmdpath (p->floppyslots[2].df, arg, 255); break;
	case '3': cmdpath (p->floppyslots[3].df, arg, 255); break;
	case 'r': cmdpath (p->romfile, arg, 255); break;
	case 'K': cmdpath (p->romextfile, arg, 255); break;
	case 'p': _tcsncpy (p->prtname, arg, 255); p->prtname[255] = 0; break;
	case 'I': cmdpath (p->sername, arg, 255); currprefs.use_serial = 1; break;
	case 'm': case 'M': parse_filesys_spec (p, c == 'M', arg); break;
	case 'W': parse_hardfile_spec (p, arg); break;
	case 'S': parse_sound_spec (p, arg); break;
	case 'R': p->gfx_framerate = _tstoi (arg); break;
	case 'i': p->illegal_mem = 1; break;
	case 'J': parse_joy_spec (p, arg); break;

#if defined USE_X11_GFX
    case 'L': p->x11_use_low_bandwidth = 1; break;
    case 'T': p->x11_use_mitshm = 1; break;
#elif defined USE_AMIGA_GFX
    case 'T': p->amiga_use_grey = 1; break;
    case 'x': p->amiga_use_dither = 0; break;
#elif defined USE_CURSES_GFX
    case 'x': p->curses_reverse_video = 1; break;
#endif
	case 'w': p->m68k_speed = _tstoi (arg); break;

		/* case 'g': p->use_gfxlib = 1; break; */
	case 'G': p->start_gui = 0; break;
#ifdef DEBUGGER
	case 'D': p->start_debugger = 1; break;
#endif

	case 'n':
		if (_tcschr (arg, 'i') != 0)
			p->immediate_blits = 1;
		break;

	case 'v':
		set_chipset_mask (p, _tstoi (arg));
		break;

	case 'C':
		parse_cpu_specs (p, arg);
		break;

	case 'Z':
		p->z3fastmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'U':
		p->gfxmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'F':
		p->fastmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'b':
		p->bogomem_size = _tstoi (arg) * 0x40000;
		break;

	case 'c':
		p->chipmem_size = _tstoi (arg) * 0x80000;
		break;

	case 'l':
		if (0 == strcasecmp(arg, "de"))
			p->keyboard_lang = KBD_LANG_DE;
		else if (0 == strcasecmp(arg, "dk"))
			p->keyboard_lang = KBD_LANG_DK;
		else if (0 == strcasecmp(arg, "us"))
			p->keyboard_lang = KBD_LANG_US;
		else if (0 == strcasecmp(arg, "se"))
			p->keyboard_lang = KBD_LANG_SE;
		else if (0 == strcasecmp(arg, "fr"))
			p->keyboard_lang = KBD_LANG_FR;
		else if (0 == strcasecmp(arg, "es"))
			p->keyboard_lang = KBD_LANG_ES;
		else if (0 == strcasecmp(arg, "it"))
			p->keyboard_lang = KBD_LANG_IT;
		else if (0 == strcasecmp(arg, "fi"))
			p->keyboard_lang = KBD_LANG_FI;
		else if (0 == strcasecmp(arg, "tr"))
			p->keyboard_lang = KBD_LANG_TR;
		break;

	case 'O': parse_gfx_specs (p, arg); break;
	case 'd':
		if (_tcschr (arg, 'S') != NULL || _tcschr (arg, 's')) {
			write_log ("  Serial on demand.\n");
			p->serial_demand = 1;
		}
		if (_tcschr (arg, 'P') != NULL || _tcschr (arg, 'p')) {
			write_log ("  Parallel on demand.\n");
			p->parallel_demand = 1;
		}

		break;

	case 'H':
#ifndef USE_AMIGA_GFX
		p->color_mode = _tstoi (arg);
		if (p->color_mode < 0) {
			write_log ("Bad color mode selected. Using default.\n");
			p->color_mode = 0;
		}
#else
	p->amiga_screen_type = atoi (arg);
	if (p->amiga_screen_type < 0 || p->amiga_screen_type > 2) {
	    write_log ("Bad screen-type selected. Defaulting to public screen.\n");
	    p->amiga_screen_type = 2;
	}
#endif
		break;
	default:
		write_log ("Unknown option `-%c'!\n", c);
		break;
	}
	return !! _tcschr (arg_required, c);
}

void cfgfile_addcfgparam (TCHAR *line)
{
	struct strlist *u;
	TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

	if (!line) {
		struct strlist **ps = &temp_lines;
		while (*ps) {
			struct strlist *s = *ps;
			*ps = s->next;
			xfree (s->value);
			xfree (s->option);
			xfree (s);
		}
		temp_lines = 0;
		return;
	}
	if (!cfgfile_separate_line (line, line1b, line2b))
		return;
	u = xcalloc (struct strlist, 1);
	u->option = my_strdup (line1b);
	u->value = my_strdup (line2b);
	u->next = temp_lines;
	temp_lines = u;
}

static int getconfigstoreline (struct zfile *z, TCHAR *option, TCHAR *value)
{
	TCHAR tmp[CONFIG_BLEN * 2];
	int idx = 0;

	for (;;) {
		TCHAR b = 0;
		if (zfile_fread (&b, 1, sizeof (TCHAR), z) != 1)
			return 0;
		tmp[idx++] = b;
		tmp[idx] = 0;
		if (b == '\n' || b == 0)
			break;
	}
	return cfgfile_separate_line (tmp, option, value);
}

#if 0
static int cfgfile_handle_custom_event (TCHAR *custom, int mode)
{
	TCHAR option[CONFIG_BLEN], value[CONFIG_BLEN];
	TCHAR option2[CONFIG_BLEN], value2[CONFIG_BLEN];
	TCHAR *tmp, *p, *nextp;
	struct zfile *configstore = NULL;
	int cnt = 0, cnt_ok = 0;

	if (!mode) {
		TCHAR zero = 0;
		configstore = zfile_fopen_empty ("configstore", 50000);
		cfgfile_save_options (configstore, &currprefs, 0);
		cfg_write (&zero, configstore);
	}

	nextp = NULL;
	tmp = p = xcalloc (TCHAR, _tcslen (custom) + 2);
	_tcscpy (tmp, custom);
	while (p && *p) {
		if (*p == '\"') {
			TCHAR *p2;
			p++;
			p2 = p;
			while (*p2 != '\"' && *p2 != 0)
				p2++;
			if (*p2 == '\"') {
				*p2++ = 0;
				nextp = p2 + 1;
				if (*nextp == ' ')
					nextp++;
			}
		}
		if (cfgfile_separate_line (p, option, value)) {
			cnt++;
			if (mode) {
				cfgfile_parse_option (&changed_prefs, option, value, 0);
			} else {
				zfile_fseek (configstore, 0, SEEK_SET);
				for (;;) {
					if (!getconfigstoreline (configstore, option2, value2))
						break;
					if (!_tcscmpi (option, option2) && !_tcscmpi (value, value2)) {
						cnt_ok++;
						break;
					}
				}
			}
		}
		p = nextp;
	}
	xfree (tmp);
	zfile_fclose (configstore);
	if (cnt > 0 && cnt == cnt_ok)
		return 1;
	return 0;
}
#endif

int cmdlineparser (const TCHAR *s, TCHAR *outp[], int max)
{
	int j;
	unsigned int cnt = 0;
	int slash = 0;
	int quote = 0;
	TCHAR tmp1[MAX_DPATH];
	const TCHAR *prev;
	int doout;

	doout = 0;
	prev = s;
	j = 0;
	outp[0] = 0;
	while (cnt < max) {
		TCHAR c = *s++;
		if (!c)
			break;
		if (c < 32)
			continue;
		if (c == '\\')
			slash = 1;
		if (!slash && c == '"') {
			if (quote) {
				quote = 0;
				doout = 1;
			} else {
				quote = 1;
				j = -1;
			}
		}
		if (!quote && c == ' ')
			doout = 1;
		if (!doout) {
			if (j >= 0) {
				tmp1[j] = c;
				tmp1[j + 1] = 0;
			}
			j++;
		}
		if (doout) {
			if (_tcslen (tmp1) > 0) {
				outp[cnt++] = my_strdup (tmp1);
				outp[cnt] = 0;
			}
			tmp1[0] = 0;
			doout = 0;
			j = 0;
		}
		slash = 0;
	}
	if (j > 0 && cnt < max) {
		outp[cnt++] = my_strdup (tmp1);
		outp[cnt] = 0;
	}
	return cnt;
}

#define UAELIB_MAX_PARSE 100

uae_u32 cfgfile_modify (uae_u32 index, TCHAR *parms, uae_u32 size, TCHAR *out, uae_u32 outsize)
{
	TCHAR *p;
	TCHAR *argc[UAELIB_MAX_PARSE];
	unsigned int argv, i;
	uae_u32 err;
	TCHAR zero = 0;
	static struct zfile *configstore;
	static TCHAR *configsearch;
	static int configsearchfound;

	config_changed = 1;
	err = 0;
	argv = 0;
	p = 0;
	if (index != 0xffffffff) {
		if (!configstore) {
			err = 20;
			goto end;
		}
		if (configsearch) {
			TCHAR tmp[CONFIG_BLEN];
			unsigned int j = 0;
			TCHAR *in = configsearch;
			unsigned int inlen = _tcslen (configsearch);
			int joker = 0;

			if (in[inlen - 1] == '*') {
				joker = 1;
				inlen--;
			}

			for (;;) {
				uae_u8 b = 0;

				if (zfile_fread (&b, 1, 1, configstore) != 1) {
					err = 10;
					if (configsearch)
						err = 5;
					if (configsearchfound)
						err = 0;
					goto end;
				}
				if (j >= sizeof (tmp) / sizeof (TCHAR) - 1)
					j = sizeof (tmp) / sizeof (TCHAR) - 1;
				if (b == 0) {
					err = 10;
					if (configsearch)
						err = 5;
					if (configsearchfound)
						err = 0;
					goto end;
				}
				if (b == '\n') {
					if (configsearch && !_tcsncmp (tmp, in, inlen) &&
						((inlen > 0 && _tcslen (tmp) > inlen && tmp[inlen] == '=') || (joker))) {
							TCHAR *p;
							if (joker)
								p = tmp - 1;
							else
								p = _tcschr (tmp, '=');
							if (p) {
								for (i = 0; out && i < outsize - 1; i++) {
									TCHAR b = *++p;
									out[i] = b;
									out[i + 1] = 0;
									if (!b)
										break;
								}
							}
							err = 0xffffffff;
							configsearchfound++;
							goto end;
					}
					index--;
					j = 0;
				} else {
					tmp[j++] = b;
					tmp[j] = 0;
				}
			}
		}
		err = 0xffffffff;
		for (i = 0; out && i < outsize - 1; i++) {
			uae_u8 b = 0;
			if (zfile_fread (&b, 1, 1, configstore) != 1)
				err = 0;
			if (b == 0)
				err = 0;
			if (b == '\n')
				b = 0;
			out[i] = b;
			out[i + 1] = 0;
			if (!b)
				break;
		}
		goto end;
	}

	if (size > 10000)
		return 10;
	argv = cmdlineparser (parms, argc, UAELIB_MAX_PARSE);

	if (argv <= 1 && index == 0xffffffff) {
		zfile_fclose (configstore);
		xfree (configsearch);
		configstore = zfile_fopen_empty (NULL, "configstore", 50000);
		configsearch = NULL;
		if (argv > 0 && _tcslen (argc[0]) > 0)
			configsearch = my_strdup (argc[0]);
		if (!configstore) {
			err = 20;
			goto end;
		}
		zfile_fseek (configstore, 0, SEEK_SET);
		uaeconfig++;
		cfgfile_save_options (configstore, &currprefs, 0);
		uaeconfig--;
		cfg_write (&zero, configstore);
		zfile_fseek (configstore, 0, SEEK_SET);
		err = 0xffffffff;
		configsearchfound = 0;
		goto end;
	}

	for (i = 0; i < argv; i++) {
		if (i + 2 <= argv) {
			if (!_tcsicmp (argc[i], "dbg")) {
				debug_parser (argc[i + 1], out, outsize);
			} else if (!inputdevice_uaelib (argc[i], argc[i + 1])) {
				if (!cfgfile_parse_option (&changed_prefs, argc[i], argc[i + 1], 0)) {
					err = 5;
					break;
				}
			}
			set_special (SPCFLAG_BRK);
			i++;
		}
	}
end:
	for (i = 0; i < argv; i++)
		xfree (argc[i]);
	xfree (p);
	return err;
}

uae_u32 cfgfile_uaelib_modify (uae_u32 index, uae_u32 parms, uae_u32 size, uae_u32 out, uae_u32 outsize)
{
	char *p, *parms_p = NULL, *out_p = NULL;
	unsigned int i;
	int ret;

	if (out)
		put_byte (out, 0);
	if (size == 0) {
		while (get_byte (parms + size) != 0)
			size++;
	}
	parms_p = xmalloc (uae_char, size + 1);
	if (!parms_p) {
		ret = 10;
		goto end;
	}
	if (out) {
		out_p = xmalloc (TCHAR, outsize + 1);
		if (!out_p) {
			ret = 10;
			goto end;
		}
		out_p[0] = 0;
	}
	p = parms_p;
	for (i = 0; i < size; i++) {
		p[i] = get_byte (parms + i);
		if (p[i] == 10 || p[i] == 13 || p[i] == 0)
			break;
	}
	p[i] = 0;
	ret = cfgfile_modify (index, parms_p, size, out_p, outsize);
	if (out) {
		p = out_p;
		for (i = 0; i < outsize - 1; i++) {
			uae_u8 b = *p++;
			put_byte (out + i, b);
			put_byte (out + i + 1, 0);
			if (!b)
				break;
		}
	}
end:
	xfree (out_p);
	xfree (parms_p);
	return ret;
}

uae_u32 cfgfile_uaelib (int mode, uae_u32 name, uae_u32 dst, uae_u32 maxlen)
{
	TCHAR tmp[CONFIG_BLEN];
	int i;
	struct strlist *sl;

	if (mode)
		return 0;

	for (i = 0; i < sizeof (tmp) / sizeof (TCHAR); i++) {
		tmp[i] = get_byte (name + i);
		if (tmp[i] == 0)
			break;
	}
	tmp[sizeof(tmp) / sizeof (TCHAR) - 1] = 0;
	if (tmp[0] == 0)
		return 0;
	for (sl = currprefs.all_lines; sl; sl = sl->next) {
		if (!strcasecmp (sl->option, tmp))
			break;
	}

	if (sl) {
		for (i = 0; i < maxlen; i++) {
			put_byte (dst + i, sl->value[i]);
			if (sl->value[i] == 0)
				break;
		}
		return dst;
	}
	return 0;
}

uae_u8 *restore_configuration (uae_u8 *src)
{
	TCHAR *s = ((char*)src);
	//write_log (s);
	xfree (s);
	src += strlen ((char*)src) + 1;
	return src;
}

uae_u8 *save_configuration (int *len)
{
	int tmpsize = 30000;
	uae_u8 *dstbak, *dst, *p;
	int index = -1;

	dstbak = dst = xmalloc (uae_u8, tmpsize);
	p = dst;
	for (;;) {
		TCHAR tmpout[256];
		int ret;
		tmpout[0] = 0;
		ret = cfgfile_modify (index, "*", 1, tmpout, sizeof (tmpout) / sizeof (TCHAR));
		index++;
		if (_tcslen (tmpout) > 0) {
			char *out;
			if (!_tcsncmp (tmpout, "input.", 6))
				continue;
			strcpy ((char*)p, tmpout);
			strcat ((char*)p, "\n");
			p += strlen ((char*)p);
			if (p - dstbak >= tmpsize - sizeof (tmpout))
				break;
		}
		if (ret >= 0)
			break;
	}
	*len = p - dstbak + 1;
	return dstbak;
}

static void default_prefs_mini (struct uae_prefs *p, int type)
{
	_tcscpy (p->description, "UAE default A500 configuration");

	p->nr_floppies = 1;
	p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cpu_model = 68000;
	p->address_space_24 = 1;
	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
}

void default_prefs (struct uae_prefs *p, int type)
{
	int i;
	int roms[] = { 6, 7, 8, 9, 10, 14, 5, 4, 3, 2, 1, -1 };
	TCHAR zero = 0;
	struct zfile *f;

	reset_inputdevice_config (p);
	memset (p, 0, sizeof (*p));
	_tcscpy (p->description, "UAE default configuration");
	p->config_hardware_path[0] = 0;
	p->config_host_path[0] = 0;

	p->gfx_scandoubler = 0;
	p->start_gui = 1;
#ifdef DEBUGGER
	p->start_debugger = 0;
#endif

	p->all_lines = 0;
	/* Note to porters: please don't change any of these options! UAE is supposed
	 * to behave identically on all platforms if possible.
	 * (TW says: maybe it is time to update default config..) */
	p->illegal_mem = 0;
	p->use_serial = 0;
	p->serial_demand = 0;
	p->serial_hwctsrts = 1;
	p->serial_stopbits = 0;
	p->parallel_demand = 0;
	p->parallel_matrix_emulation = 0;
	p->parallel_postscript_emulation = 0;
	p->parallel_postscript_detection = 0;
	p->parallel_autoflush_time = 5;
	p->ghostscript_parameters[0] = 0;
	p->uae_hide = 0;

	memset (&p->jports[0], 0, sizeof (struct jport));
	memset (&p->jports[1], 0, sizeof (struct jport));
	memset (&p->jports[2], 0, sizeof (struct jport));
	memset (&p->jports[3], 0, sizeof (struct jport));
	p->jports[0].id = JSEM_MICE;
	p->jports[1].id = JSEM_KBDLAYOUT;
	p->jports[2].id = -1;
	p->jports[3].id = -1;
	p->keyboard_lang = KBD_LANG_US;

	p->produce_sound = 3;
	p->sound_stereo = SND_STEREO;
	p->sound_stereo_separation = 7;
	p->sound_mixed_stereo_delay = 0;
	p->sound_freq = DEFAULT_SOUND_FREQ;
	p->sound_maxbsiz = DEFAULT_SOUND_MAXB;
	p->sound_latency = 100;
	p->sound_interpol = 1;
	p->sound_filter = FILTER_SOUND_EMUL;
	p->sound_filter_type = 0;
	p->sound_auto = 1;

#ifdef JIT
#ifdef NATMEM_OFFSET
	p->comptrustbyte = 0;
	p->comptrustword = 0;
	p->comptrustlong = 0;
	p->comptrustnaddr= 0;
#else
	p->comptrustbyte = 1;
	p->comptrustword = 1;
	p->comptrustlong = 1;
	p->comptrustnaddr= 1;
#endif
	p->compnf = 1;
	p->comp_hardflush = 0;
	p->comp_constjump = 1;
	p->comp_oldsegv = 0;
	p->compfpu = 1;
	p->fpu_strict = 0;
	p->cachesize = 0;
	p->avoid_cmov = 0;
	p->comp_midopt = 0;
	p->comp_lowopt = 0;

	for (i = 0;i < 10; i++)
		p->optcount[i] = -1;
	p->optcount[0] = 4;	/* How often a block has to be executed before it is translated */
	p->optcount[1] = 0;	/* How often to use the naive translation */
	p->optcount[2] = 0;
	p->optcount[3] = 0;
	p->optcount[4] = 0;
	p->optcount[5] = 0;
#endif
	p->gfx_framerate = 1;
	p->gfx_autoframerate = 50;
	p->gfx_size_fs.width = 800;
	p->gfx_size_fs.height = 600;
	p->gfx_size_win.width = 720;
	p->gfx_size_win.height = 568;
	for (i = 0; i < 4; i++) {
		p->gfx_size_fs_xtra[i].width = 0;
		p->gfx_size_fs_xtra[i].height = 0;
		p->gfx_size_win_xtra[i].width = 0;
		p->gfx_size_win_xtra[i].height = 0;
	}
	p->gfx_resolution = RES_HIRES;
	p->gfx_vresolution = VRES_DOUBLE;
	p->gfx_afullscreen = GFX_WINDOW;
	p->gfx_pfullscreen = GFX_WINDOW;
	p->gfx_xcenter = 0; p->gfx_ycenter = 0;
	p->gfx_xcenter_pos = -1;
	p->gfx_ycenter_pos = -1;
	p->gfx_xcenter_size = -1;
	p->gfx_ycenter_size = -1;
	p->gfx_max_horizontal = RES_HIRES;
	p->gfx_max_vertical = VRES_DOUBLE;
	p->color_mode = 2;
	p->gfx_blackerthanblack = 0;
	p->gfx_backbuffers = 1;

#ifdef USE_X11_GFX
	p->x11_use_low_bandwidth = 0;
	p->x11_use_mitshm = 0;
	p->x11_hide_cursor = 1;
#endif
#ifdef SVGA
	p->svga_no_linear = 0;
#endif
#ifdef NCURSES
	p->curses_reverse_video = 0;
#endif
        machdep_default_options (p);
	target_default_options (p, type);
        gfx_default_options (p);
        audio_default_options (p);

	p->immediate_blits = 0;
	p->waiting_blits = 0;
	p->collision_level = 2;
	p->leds_on_screen = 0;
	p->keyboard_leds_in_use = 0;
	p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
	p->scsi = 0;
	p->uaeserial = 0;
	p->cpu_idle = 0;
	p->turbo_emulation = 0;
	p->headless = 0;
	p->catweasel = 0;
	p->tod_hack = 0;
	p->maprom = 0;
	p->filesys_no_uaefsdb = 0;
	p->filesys_custom_uaefsdb = 1;
	p->picasso96_nocustom = 1;
	p->cart_internal = 1;
	p->sana2 = 0;
	p->clipboard_sharing = false;

	p->cs_compatible = 1;
	p->cs_rtc = 2;
	p->cs_df0idhw = 1;
	p->cs_a1000ram = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_agnusrev = -1;
	p->cs_deniserev = -1;
	p->cs_mbdmac = 0;
	p->cs_a2091 = 0;
	p->cs_a4091 = 0;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = false;
	p->cs_cdtvcd = p->cs_cdtvram = false;
	p->cs_cdtvcard = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_slowmemisfast = 0;
	p->cs_resetwarning = 1;

#ifdef GFXFILTER
	p->gfx_filter = 0;
	p->gfx_filtershader[0] = 0;
	p->gfx_filtermask[0] = 0;
	p->gfx_filter_horiz_zoom_mult = 1000;
	p->gfx_filter_vert_zoom_mult = 1000;
	p->gfx_filter_bilinear = 0;
	p->gfx_filter_filtermode = 0;
	p->gfx_filter_scanlineratio = (1 << 4) | 1;
	p->gfx_filter_keep_aspect = 0;
	p->gfx_filter_autoscale = AUTOSCALE_STATIC_AUTO;
	p->gfx_filteroverlay_overscan = 0;
#endif

	_tcscpy (p->floppyslots[0].df, "df0.adf");
	_tcscpy (p->floppyslots[1].df, "df1.adf");
	_tcscpy (p->floppyslots[2].df, "df2.adf");
	_tcscpy (p->floppyslots[3].df, "df3.adf");

	configure_rom (p, roms, 0);
	_tcscpy (p->romfile, "kick.rom");
	_tcscpy (p->romextfile, "");
	_tcscpy (p->romextfile2, "");
	p->romextfile2addr = 0;
        _tcscpy (p->flashfile, "");
#ifdef ACTION_REPLAY
        _tcscpy (p->cartfile, "");
#endif

	_tcscpy (p->path_rom.path[0], "./");
	_tcscpy (p->path_floppy.path[0], "./");
	_tcscpy (p->path_hardfile.path[0], "./");

	p->prtname[0] = 0;
	p->sername[0] = 0;

	p->fpu_model = 0;
	p->cpu_model = 68000;
	p->cpu_clock_multiplier = 0;
	p->cpu_frequency = 0;
	p->mmu_model = 0;
	p->cpu060_revision = 6;
	p->fpu_revision = -1;
	p->m68k_speed = 0;
	p->cpu_compatible = 1;
	p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->genlock = 0;
	p->ntscmode = 0;

	p->fastmem_size = 0x00000000;
	p->mbresmem_low_size = 0x00000000;
	p->mbresmem_high_size = 0x00000000;
	p->z3fastmem_size = 0x00000000;
	p->z3fastmem2_size = 0x00000000;
	p->z3fastmem_start = 0x10000000;
	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
	p->gfxmem_size = 0x00000000;
	p->custom_memory_addrs[0] = 0;
	p->custom_memory_sizes[0] = 0;
	p->custom_memory_addrs[1] = 0;
	p->custom_memory_sizes[1] = 0;

	p->nr_floppies = 2;
	p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_35_DD;
	p->floppyslots[2].dfxtype = DRV_NONE;
	p->floppyslots[3].dfxtype = DRV_NONE;
	p->floppy_speed = 100;
	p->floppy_write_length = 0;
	p->floppy_random_bits_min = 1;
	p->floppy_random_bits_max = 3;
#ifdef DRIVESOUND
	p->dfxclickvolume = 33;
	p->dfxclickchannelmask = 0xffff;
#endif

#ifdef SAVESTATE
	p->statecapturebuffersize = 100;
	p->statecapturerate = 5 * 50;
	p->inprec_autoplay = true;
#endif

#ifdef UAE_MINI
	default_prefs_mini (p, 0);
#endif

	p->input_tablet = TABLET_OFF;
	p->input_magic_mouse = 0;
	p->input_magic_mouse_cursor = 0;

	inputdevice_default_prefs (p);

#ifdef SCSIEMU
	blkdev_default_prefs (p);
#endif

	  p->cr_selected = -1;
	  struct chipset_refresh *cr;
	  for (i = 0; i < MAX_CHIPSET_REFRESH_TOTAL; i++) {
	    cr = &p->cr[i];
	    cr->rate = -1;
	  }
	  cr = &p->cr[CHIPSET_REFRESH_PAL];
	  cr->horiz = -1;
	  cr->vert = -1;
	  cr->lace = -1;
	  cr->vsync = - 1;
	  cr->framelength = -1;
	  cr->rate = 50.0;
	  cr->ntsc = 0;
	  cr->locked = false;
	  _tcscpy (cr->label, "PAL");
	  cr = &p->cr[CHIPSET_REFRESH_NTSC];
	  cr->horiz = -1;
	  cr->vert = -1;
	  cr->lace = -1;
	  cr->vsync = - 1;
	  cr->framelength = -1;
	  cr->rate = 60.0;
	  cr->ntsc = 1;
	  cr->locked = false;
	  _tcscpy (cr->label, "NTSC");

	zfile_fclose (default_file);
	default_file = NULL;
	f = zfile_fopen_empty (NULL, "configstore", 100000);
	if (f) {
		uaeconfig++;
		cfgfile_save_options (f, p, 0);
		uaeconfig--;
		cfg_write (&zero, f);
		default_file = f;
	}
}

static void buildin_default_prefs_68020 (struct uae_prefs *p)
{
	p->cpu_model = 68020;
	p->address_space_24 = 1;
	p->cpu_compatible = 1;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
	p->chipmem_size = 0x200000;
	p->bogomem_size = 0;
	p->m68k_speed = -1;
}

static void buildin_default_host_prefs (struct uae_prefs *p)
{
}

static void buildin_default_prefs (struct uae_prefs *p)
{
	buildin_default_host_prefs (p);

	p->floppyslots[0].dfxtype = DRV_35_DD;
	if (p->nr_floppies != 1 && p->nr_floppies != 2)
		p->nr_floppies = 2;
	p->floppyslots[1].dfxtype = p->nr_floppies >= 2 ? DRV_35_DD : DRV_NONE;
	p->floppyslots[2].dfxtype = DRV_NONE;
	p->floppyslots[3].dfxtype = DRV_NONE;
	p->floppy_speed = 100;

	p->fpu_model = 0;
	p->cpu_model = 68000;
	p->cpu_clock_multiplier = 0;
	p->cpu_frequency = 0;
	p->cpu060_revision = 1;
	p->fpu_revision = -1;
	p->m68k_speed = 0;
	p->cpu_compatible = 1;
	p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->immediate_blits = 0;
	p->waiting_blits = 0;
	p->collision_level = 2;
	if (p->produce_sound < 1)
		p->produce_sound = 1;
	p->scsi = 0;
	p->uaeserial = 0;
	p->cpu_idle = 0;
	p->turbo_emulation = 0;
	p->catweasel = 0;
	p->tod_hack = 0;
	p->maprom = 0;
#ifdef JIT
	p->cachesize = 0;
#endif
	p->socket_emu = 0;
	p->sound_volume = 0;
	p->sound_volume_cd = -1;
	p->clipboard_sharing = false;

	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
	p->fastmem_size = 0x00000000;
	p->mbresmem_low_size = 0x00000000;
	p->mbresmem_high_size = 0x00000000;
	p->z3fastmem_size = 0x00000000;
	p->z3fastmem2_size = 0x00000000;
	p->z3chipmem_size = 0x00000000;
	p->gfxmem_size = 0x00000000;

	p->cs_rtc = 0;
	p->cs_a1000ram = false;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_agnusrev = -1;
	p->cs_deniserev = -1;
	p->cs_mbdmac = 0;
	p->cs_a2091 = false;
	p->cs_a4091 = false;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = false;
	p->cs_cdtvcd = p->cs_cdtvram = p->cs_cdtvcard = false;
	p->cs_ide = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_resetwarning = 0;

	_tcscpy (p->romfile, "");
	_tcscpy (p->romextfile, "");
	_tcscpy (p->flashfile, "");
	_tcscpy (p->cartfile, "");
	_tcscpy (p->amaxromfile, "");
	p->prtname[0] = 0;
	p->sername[0] = 0;

	p->mountitems = 0;

	target_default_options (p, 1);
}

static void set_68020_compa (struct uae_prefs *p, int compa, int cd32)
{
	if (compa == 0) {
		p->blitter_cycle_exact = 1;
		p->m68k_speed = 0;
#ifdef JIT
		if (p->cpu_model == 68020 && p->cachesize == 0) {
			p->cpu_cycle_exact = 1;
			p->cpu_clock_multiplier = 4 << 8;
		}
#endif
	}
	if (compa > 1) {
		p->cpu_compatible = 0;
		p->address_space_24 = 0;
#ifdef JIT
		p->cachesize = 8192;
#endif
	}
}

/* 0: cycle-exact
* 1: more compatible
* 2: no more compatible, no 100% sound
* 3: no more compatible, immediate blits, no 100% sound
*/

static void set_68000_compa (struct uae_prefs *p, int compa)
{
	p->cpu_clock_multiplier = 2 << 8;
	switch (compa)
	{
	case 0:
		p->cpu_cycle_exact = p->blitter_cycle_exact = 1;
		break;
	case 1:
		break;
	case 2:
		p->cpu_compatible = 0;
		break;
	case 3:
		p->immediate_blits = 1;
		p->produce_sound = 2;
		p->cpu_compatible = 0;
		break;
	}
}

static int bip_a3000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	if (config == 2)
		roms[0] = 61;
	else if (config == 1)
		roms[0] = 71;
	else
		roms[0] = 59;
	roms[1] = -1;
	p->immediate_blits = 1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
#ifdef JIT
	p->cachesize = 8192;
#endif
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A3000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}
static int bip_a4000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[8];

	roms[0] = 16;
	roms[1] = 31;
	roms[2] = 13;
	roms[3] = 12;
	roms[4] = -1;
	p->immediate_blits = 1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	if (config > 0)
		p->cpu_model = p->fpu_model = 68040;
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
#ifdef JIT
	p->cachesize = 8192;
#endif
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A4000;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}
static int bip_a4000t (struct uae_prefs *p, int config, int compa, int romcheck)
{

	int roms[8];

	roms[0] = 16;
	roms[1] = 31;
	roms[2] = 13;
	roms[3] = -1;
	p->immediate_blits = 1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	if (config > 0)
		p->cpu_model = p->fpu_model = 68040;
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
#ifdef JIT
	p->cachesize = 8192;
#endif
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A4000T;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}

static int bip_a1000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = 24;
	roms[1] = 23;
	roms[2] = -1;
	p->chipset_mask = 0;
	p->bogomem_size = 0;
	p->sound_filter = FILTER_SOUND_ON;
	set_68000_compa (p, compa);
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cs_compatible = CP_A1000;
	p->cs_slowmemisfast = 1;
	p->cs_dipagnus = 1;
	p->cs_agnusbltbusybug = 1;
	built_in_chipset_prefs (p);
	if (config > 0)
		p->cs_denisenoehb = 1;
	if (config > 1)
		p->chipmem_size = 0x40000;
	return configure_rom (p, roms, romcheck);
}

static int bip_cdtv (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = 6;
	roms[1] = 32;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	roms[0] = 20;
	roms[1] = 21;
	roms[2] = 22;
	roms[3] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->cs_cdtvcd = p->cs_cdtvram = 1;
	if (config > 0)
		p->cs_cdtvcard = 64;
	p->cs_rtc = 1;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	if (config > 0)
		p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_CDTV;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, "cdtv.nvr");
	return 1;
}

static int bip_cd32 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	buildin_default_prefs_68020 (p);
	roms[0] = 64;
	roms[1] = -1;
	if (!configure_rom (p, roms, 0)) {
		roms[0] = 18;
		roms[1] = -1;
		if (!configure_rom (p, roms, romcheck))
			return 0;
		roms[0] = 19;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}
	if (config > 0) {
		roms[0] = 23;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 1;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68020_compa (p, compa, 1);
	p->cs_compatible = CP_CD32;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, "cd32.nvr");
	return 1;
}

static int bip_a1200 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	buildin_default_prefs_68020 (p);
	roms[0] = 11;
	roms[1] = 15;
	roms[2] = 31;
	roms[3] = -1;
	p->cs_rtc = 0;
	if (config == 1) {
		p->fastmem_size = 0x400000;
		p->cs_rtc = 2;
	}
	set_68020_compa (p, compa, 0);
	p->cs_compatible = CP_A1200;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}

static int bip_a600 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = 10;
	roms[1] = 9;
	roms[2] = 8;
	roms[3] = -1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem_size = 0x200000;
	if (config == 2)
		p->fastmem_size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A600;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}

static int bip_a500p (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	roms[0] = 7;
	roms[1] = -1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem_size = 0x200000;
	if (config == 2)
		p->fastmem_size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500P;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}
static int bip_a500 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = roms[1] = roms[2] = roms[3] = -1;
	switch (config)
	{
	case 0: // KS 1.3, OCS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 6;
		roms[1] = 32;
		p->chipset_mask = 0;
		break;
	case 1: // KS 1.3, ECS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 6;
		roms[1] = 32;
		break;
	case 2: // KS 1.3, ECS Agnus, 1.0M Chip
		roms[0] = 6;
		roms[1] = 32;
		p->bogomem_size = 0;
		p->chipmem_size = 0x100000;
		break;
	case 3: // KS 1.3, OCS Agnus, 0.5M Chip
		roms[0] = 6;
		roms[1] = 32;
		p->bogomem_size = 0;
		p->chipset_mask = 0;
		p->cs_rtc = 0;
		p->floppyslots[1].dfxtype = DRV_NONE;
		break;
	case 4: // KS 1.2, OCS Agnus, 0.5M Chip
		roms[0] = 5;
		roms[1] = 4;
		roms[2] = 3;
		p->bogomem_size = 0;
		p->chipset_mask = 0;
		p->cs_rtc = 0;
		p->floppyslots[1].dfxtype = DRV_NONE;
		break;
	case 5: // KS 1.2, OCS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 5;
		roms[1] = 4;
		roms[2] = 3;
		p->chipset_mask = 0;
		break;
	}
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}

static int bip_super (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[8];

	roms[0] = 46;
	roms[1] = 16;
	roms[2] = 31;
	roms[3] = 15;
	roms[4] = 14;
	roms[5] = 12;
	roms[6] = 11;
	roms[7] = -1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x400000;
	p->z3fastmem_size = 8 * 1024 * 1024;
	p->gfxmem_size = 8 * 1024 * 1024;
	p->cpu_model = 68040;
	p->fpu_model = 68040;
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 1;
	p->produce_sound = 2;
#ifdef JIT
	p->cachesize = 8192;
#endif
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->scsi = 1;
	p->uaeserial = 1;
	p->socket_emu = 1;
	p->cart_internal = 0;
	p->picasso96_nocustom = 1;
	p->cs_compatible = 1;
	built_in_chipset_prefs (p);
	p->cs_ide = -1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	//_tcscat(p->flashfile, "battclock.nvr");
	return configure_rom (p, roms, romcheck);
}

static int bip_arcadia (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4], i;
	struct romlist **rl;

	p->bogomem_size = 0;
	p->chipset_mask = 0;
	p->cs_rtc = 0;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, "arcadia.nvr");
	roms[0] = 5;
	roms[1] = 4;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	roms[0] = 49;
	roms[1] = 50;
	roms[2] = 51;
	roms[3] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	rl = getarcadiaroms ();
	for (i = 0; rl[i]; i++) {
		if (config-- == 0) {
			roms[0] = rl[i]->rd->id;
			roms[1] = -1;
			configure_rom (p, roms, 0);
			break;
		}
	}
	xfree (rl);
	return 1;
}

int built_in_prefs (struct uae_prefs *p, int model, int config, int compa, int romcheck)
{
	write_log("built in model: %d, config: %d, compa: %d, romchk: %d\n", model, config, compa, romcheck);

	int v = 0;

	buildin_default_prefs (p);
	switch (model)
	{
	case 0:
		v = bip_a500 (p, config, compa, romcheck);
		break;
	case 1:
		v = bip_a500p (p, config, compa, romcheck);
		break;
	case 2:
		v = bip_a600 (p, config, compa, romcheck);
		break;
	case 3:
		v = bip_a1000 (p, config, compa, romcheck);
		break;
	case 4:
		v = bip_a1200 (p, config, compa, romcheck);
		break;
	case 5:
		v = bip_a3000 (p, config, compa, romcheck);
		break;
	case 6:
		v = bip_a4000 (p, config, compa, romcheck);
		break;
	case 7:
		v = bip_a4000t (p, config, compa, romcheck);
		break;
	case 8:
		v = bip_cd32 (p, config, compa, romcheck);
		break;
	case 9:
		v = bip_cdtv (p, config, compa, romcheck);
		break;
	case 10:
		v = bip_arcadia (p, config , compa, romcheck);
		break;
	case 11:
		v = bip_super (p, config, compa, romcheck);
		break;
	}
	return v;
}

int built_in_chipset_prefs (struct uae_prefs *p)
{
	if (!p->cs_compatible)
		return 1;

	p->cs_a1000ram = 0;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
	p->cs_cdtvcd = p->cs_cdtvram = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ide = 0;
	p->cs_ramseyrev = -1;
	p->cs_deniserev = -1;
	p->cs_agnusrev = -1;
	p->cs_mbdmac = 0;
	p->cs_a2091 = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_resetwarning = 1;

	switch (p->cs_compatible)
	{
	case CP_GENERIC: // generic
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ide = -1;
		p->cs_mbdmac = 1;
		p->cs_ramseyrev = 0x0f;
		break;
	case CP_CDTV: // CDTV
		p->cs_rtc = 1;
		p->cs_cdtvcd = p->cs_cdtvram = 1;
		p->cs_df0idhw = 1;
		p->cs_ksmirror_e0 = 0;
		break;
	case CP_CD32: // CD32
		p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		p->cs_resetwarning = 0;
		break;
	case CP_A500: // A500
		p->cs_df0idhw = 0;
		p->cs_resetwarning = 0;
		if (p->bogomem_size || p->chipmem_size > 1 || p->fastmem_size)
			p->cs_rtc = 1;
		break;
	case CP_A500P: // A500+
		p->cs_rtc = 1;
		p->cs_resetwarning = 0;
		break;
	case CP_A600: // A600
		p->cs_rtc = 1;
		p->cs_ide = IDE_A600A1200;
		p->cs_pcmcia = 1;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		p->cs_resetwarning = 0;
		break;
	case CP_A1000: // A1000
		p->cs_a1000ram = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_rtc = 0;
		p->cs_agnusbltbusybug = 1;
		p->cs_dipagnus = 1;
		break;
	case CP_A1200: // A1200
		p->cs_ide = IDE_A600A1200;
		p->cs_pcmcia = 1;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		break;
	case CP_A2000: // A2000
		p->cs_rtc = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		break;
	case CP_A3000: // A3000
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0d;
		p->cs_mbdmac = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		break;
	case CP_A3000T: // A3000T
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0d;
		p->cs_mbdmac = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		break;
	case CP_A4000: // A4000
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0f;
		p->cs_ide = IDE_A4000;
		p->cs_mbdmac = 0;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		break;
	case CP_A4000T: // A4000T
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0f;
		p->cs_ide = IDE_A4000;
		p->cs_mbdmac = 2;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		break;
	}
	return 1;
}

void config_check_vsync (void)
{
	if (config_changed) {
//		if (config_changed == 1)
//			write_log ("* configuration check trigger\n");
		config_changed++;
		if (config_changed > 10)
			config_changed = 0;
	}
}

