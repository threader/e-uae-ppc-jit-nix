/*
 * PUAE - The Un*x Amiga Emulator
 *
 * A collection of ugly and random stuff brought in from Win32
 * which desparately needs to be tidied up
 *
 * Copyright 2004 Richard Drummond
 * Copyright 2010-2013 Mustafa TUFAN
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "misc.h"
#include "options.h"
#include "memory_uae.h"
#include "custom.h"
#include "newcpu.h"
#include "events.h"
#include "uae.h"
#include "autoconf.h"
#include "traps.h"
#include "enforcer.h"
#include "picasso96.h"
#include "driveclick.h"
#include "inputdevice.h"
#include "keymap/keymap.h"
#include "keyboard.h"
#include <stdarg.h>
#include "clipboard.h"
#include "fsdb.h"
#include "debug.h"
#include "hrtimer.h"
#include "sleep.h"

static int logging_started;
#define LOG_BOOT "puae_bootlog.txt"
#define LOG_NORMAL "puae_log.txt"

static int tablet;
static int axmax, aymax, azmax;
static int xmax, ymax, zmax;
static int xres, yres;
static int maxpres;
static TCHAR *tabletname;
static int tablet_x, tablet_y, tablet_z, tablet_pressure, tablet_buttons, tablet_proximity;
static int tablet_ax, tablet_ay, tablet_az, tablet_flags;

unsigned int log_scsi = 1;
int log_net, uaelib_debug;

struct winuae_currentmode {
	unsigned int flags;
	int native_width, native_height, native_depth, pitch;
	int current_width, current_height, current_depth;
	int amiga_width, amiga_height;
	int frequency;
	int initdone;
	int fullfill;
	int vsync;
};

static struct winuae_currentmode currentmodestruct;
static struct winuae_currentmode *currentmode = &currentmodestruct;

static int serial_period_hsyncs, serial_period_hsync_counter;
static int data_in_serdatr; /* new data received */

// win32
int log_vsync, debug_vsync_min_delay, debug_vsync_forced_delay;

// serial
unsigned int seriallog = 0;

// dinput
int rawkeyboard = -1;
static bool rawinput_enabled_mouse, rawinput_enabled_keyboard;
int no_rawinput;
int tablet_log = 0;

int is_tablet (void)
{
	return tablet ? 1 : 0;
}

//win32gfx.cpp
int screen_is_picasso = 0;
struct uae_filter *usedfilter;
uae_u32 redc[3 * 256], grec[3 * 256], bluc[3 * 256];

volatile bool vblank_found_chipset = false;
static struct remembered_vsync *vsyncmemory;

static int wasfullwindow_a, wasfullwindow_p;
static int vblankbasewait1, vblankbasewait2, vblankbasewait3, vblankbasefull, vblankbaseadjust;
static bool vblankbaselace;
static int vblankbaselace_chipset;
static bool vblankthread_oddeven;

#define VBLANKTH_KILL 0
#define VBLANKTH_CALIBRATE 1
#define VBLANKTH_IDLE 2
#define VBLANKTH_ACTIVE_WAIT 3
#define VBLANKTH_ACTIVE 4
#define VBLANKTH_ACTIVE_START 5
#define VBLANKTH_ACTIVE_SKIPFRAME 6
#define VBLANKTH_ACTIVE_SKIPFRAME2 7

static volatile bool vblank_found;
static volatile int flipthread_mode;
volatile bool vblank_found_chipset;
volatile bool vblank_found_rtg;
static int flipevent, flipevent2, vblankwaitevent;
static volatile int flipevent_mode;

static double remembered_vblank;
static volatile int vblankthread_mode, vblankthread_counter;
static int vblankbasewait, vblankbasefull;
static volatile frame_time_t vblank_prev_time, thread_vblank_time;
static volatile int vblank_found_flipdelay;

static int frame_missed, frame_counted, frame_errors;
static int frame_usage, frame_usage_avg, frame_usage_total;
extern int log_vsync;
static bool dooddevenskip;
static volatile bool vblank_skipeveryother;
static int vblank_flip_delay;
static volatile bool vblank_first_time;

/* internal prototypes */
void getgfxoffset (int *dxp, int *dyp, int *mxp, int *myp);
bool vsync_isdone (void);
int vsync_switchmode (int hz);
void serial_check_irq (void);
void serial_uartbreak (int v);
void serial_hsynchandler (void);
uae_u32 emulib_target_getcpurate (uae_u32 v, uae_u32 *low);
void setmouseactivexy (int x, int y, int dir);
char *au_fs_copy (char *dst, int maxlen, const char *src);
int get_guid_target (uae_u8 *out);
char *ua_fs (const char *s, int defchar);
char *ua_copy (char *dst, int maxlen, const char *src);
uae_u8 *save_log (int bootlog, int *len);
void refreshtitle (void);
int scan_roms (int show);
void setid (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt);
void setid_af (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt, int af);
void fetch_path (TCHAR *name, TCHAR *out, int size);
void fetch_saveimagepath (TCHAR *out, int size, int dir);
void fetch_screenshotpath (TCHAR *out, int size);
void fetch_ripperpath (TCHAR *out, int size);
void close_console (void);
bool console_isch (void);
TCHAR console_getch (void);
struct MultiDisplay *getdisplay (struct uae_prefs *p);
void addmode (struct MultiDisplay *md, int w, int h, int d, int freq, int rawmode);
void updatedisplayarea (void);
double vblank_calibrate (double approx_vblank, bool waitonly);
frame_time_t vsync_busywait_end (int *flipdelay);
void vsync_busywait_start (void);
bool vsync_busywait_do (int *freetime, bool lace, bool oddeven);
void serialuartbreak (int v);
void doflashscreen (void);

void getgfxoffset (int *dxp, int *dyp, int *mxp, int *myp)
{
	*dxp = 0;
	*dyp = 0;
	*mxp = 0;
	*myp = 0;
}

bool vsync_isdone (void)
{
        return vblank_found_chipset || dooddevenskip;
}

int vsync_switchmode (int hz)
{
    static struct PicassoResolution *oldmode;
    static int oldhz;
	int w = currentmode->native_width;
	int h = currentmode->native_height;
	int d = currentmode->native_depth / 8;
//        struct MultiDisplay *md = getdisplay (&currprefs);
	struct PicassoResolution *found;
	int newh, i, cnt;

    newh = h * (currprefs.ntscmode ? 60 : 50) / hz;

	found = NULL;
/*    for (cnt = 0; cnt <= abs (newh - h) + 1 && !found; cnt++) {
            for (i = 0; md->DisplayModes[i].depth >= 0 && !found; i++) {
                    struct PicassoResolution *r = &md->DisplayModes[i];
                    if (r->res.width == w && (r->res.height == newh + cnt || r->res.height == newh - cnt) && r->depth == d) {
                            int j;
                            for (j = 0; r->refresh[j] > 0; j++) {
                                    if (r->refresh[j] == hz || r->refresh[j] == hz * 2) {
                                            found = r;
                                            hz = r->refresh[j];
                                            break;
                                    }
                            }
                    }
            }
    }*/
    if (found == oldmode && hz == oldhz)
            return true;
    oldmode = found;
    oldhz = hz;
    if (!found) {
            changed_prefs.gfx_apmode[0].gfx_vsync = 0;
            if (currprefs.gfx_apmode[0].gfx_vsync != changed_prefs.gfx_apmode[0].gfx_vsync) {
                    config_changed = 1;
            }
            write_log (_T("refresh rate changed to %d but no matching screenmode found, vsync disabled\n"), hz);
            return false;
    } else {
            newh = found->res.height;
            changed_prefs.gfx_size_fs.height = newh;
            changed_prefs.gfx_apmode[0].gfx_refreshrate = hz;
            if (changed_prefs.gfx_size_fs.height != currprefs.gfx_size_fs.height ||
                    changed_prefs.gfx_apmode[0].gfx_refreshrate != currprefs.gfx_apmode[0].gfx_refreshrate) {
                    write_log (_T("refresh rate changed to %d, new screenmode %dx%d\n"), hz, w, newh);
                    config_changed = 1;
            }
            return true;
    }
}
///////////////////////////////////////////////////
// serial_win32
///////////////////////////////////////////////////
void serial_check_irq (void)
{
	if (data_in_serdatr)
		INTREQ_0 (0x8000 | 0x0800);
}

void serial_uartbreak (int v)
{
#ifdef SERIAL_PORT
	serialuartbreak (v);
#endif
}

void serial_hsynchandler (void)
{
#ifdef AHI
	extern void hsyncstuff(void);
	hsyncstuff();
#endif
/*
	if (serial_period_hsyncs == 0)
		return;
	serial_period_hsync_counter++;
	if (serial_period_hsyncs == 1 || (serial_period_hsync_counter % (serial_period_hsyncs - 1)) == 0) {
		checkreceive_serial (0);
		checkreceive_enet (0);
	}
	if ((serial_period_hsync_counter % serial_period_hsyncs) == 0)
		checksend (0);
*/
}

//win32.cpp
int extraframewait = 5;
int log_vsync;

void sleep_millis_main (int ms)
{
	uae_msleep (ms);
}

void target_restart (void)
{
}

// driveclick_win32
int driveclick_loadresource (struct drvsample *sp, int drivetype) { return 0; }
void driveclick_fdrawcmd_close(int drive){}
static int driveclick_fdrawcmd_open_2(int drive){ return 0; }
int driveclick_fdrawcmd_open(int drive){ return 0; }
void driveclick_fdrawcmd_detect(void){}
void driveclick_fdrawcmd_seek(int drive, int cyl){}
void driveclick_fdrawcmd_motor (int drive, int running){}
void driveclick_fdrawcmd_vsync(void){}
static int driveclick_fdrawcmd_init(int drive){ return 1; }

// win32
uae_u32 emulib_target_getcpurate (uae_u32 v, uae_u32 *low)
{
#ifdef _WIN32
        *low = 0;
        if (v == 1) {
                LARGE_INTEGER pf;
                pf.QuadPart = 0;
                QueryPerformanceFrequency (&pf);
                *low = pf.LowPart;
                return pf.HighPart;
        } else if (v == 2) {
                LARGE_INTEGER pf;
                pf.QuadPart = 0;
                QueryPerformanceCounter (&pf);
                *low = pf.LowPart;
                return pf.HighPart;
        }
#else
/*
	static struct timeval _tstart, _tend;
	static struct timezone tz;

	*low = 0;
	if (v == 1) {
		gettimeofday (&_tstart, &tz);
	} else if (v == 2) {
		gettimeofday (&_tend, &tz);
	}
	double t1, t2;

	t1 =  (double)_tstart.tv_sec + (double)_tstart.tv_usec/(1000*1000);
	t2 =  (double)_tend.tv_sec + (double)_tend.tv_usec/(1000*1000);
	return t2-t1;
*/
#endif
	return 0;
}


void setmouseactivexy (int x, int y, int dir)
{
/*        int diff = 8;

        if (isfullscreen () > 0)
                return;
        x += amigawin_rect.left;
        y += amigawin_rect.top;
        if (dir & 1)
                x = amigawin_rect.left - diff;
        if (dir & 2)
                x = amigawin_rect.right + diff;
        if (dir & 4)
                y = amigawin_rect.top - diff;
        if (dir & 8)
                y = amigawin_rect.bottom + diff;
        if (!dir) {
                x += (amigawin_rect.right - amigawin_rect.left) / 2;
                y += (amigawin_rect.bottom - amigawin_rect.top) / 2;
        }
        if (mouseactive) {
                disablecapture ();
                SetCursorPos (x, y);
                if (dir)
                        recapture = 1;
        }*/
}

void setmouseactive (int active)
{
}

// unicode
char *au_fs_copy (char *dst, int maxlen, const char *src)
{
	unsigned int i;

	for (i = 0; src[i] && i < maxlen - 1; i++)
		dst[i] = src[i];
	dst[i] = 0;
	return dst;
}

// fsdb_mywin32
int my_existsfile (const char *name)
{
	struct stat sonuc;
	if (lstat (name, &sonuc) == -1) {
		return 0;
	} else {
		if (!S_ISDIR(sonuc.st_mode))
			return 1;
	}
	return 0;
}

int my_existsdir (const char *name)
{
	struct stat sonuc;

	if (lstat (name, &sonuc) == -1) {
		return 0;
	} else {
		if (S_ISDIR(sonuc.st_mode))
			return 1;
	}
	return 0;
}

int my_getvolumeinfo (const char *root)
{
	struct stat sonuc;
	int ret = 0;

	if (lstat (root, &sonuc) == -1)
		return -1;
	if (!S_ISDIR(sonuc.st_mode))
		return -1;
	return ret;
}

// clipboard
static uaecptr clipboard_data;
static int vdelay, signaling, initialized;

void amiga_clipboard_die (void)
{
	signaling = 0;
	write_log ("clipboard not initialized\n");
}

void amiga_clipboard_init (void)
{
	signaling = 0;
	write_log ("clipboard initialized\n");
	initialized = 1;
}

void amiga_clipboard_task_start (uaecptr data)
{
	clipboard_data = data;
	signaling = 1;
	write_log ("clipboard task init: %08x\n", clipboard_data);
}

uae_u32 amiga_clipboard_proc_start (void)
{
	write_log ("clipboard process init: %08x\n", clipboard_data);
	signaling = 1;
	return clipboard_data;
}

void amiga_clipboard_got_data (uaecptr data, uae_u32 size, uae_u32 actual)
{
	uae_u8 *addr;
	if (!initialized) {
		write_log ("clipboard: got_data() before initialized!?\n");
		return;
	}
}

// win32
int get_guid_target (uae_u8 *out)
{
	unsigned Data1, Data2, Data3, Data4;

	srand(time(NULL));
	Data1 = rand();
	Data2 = ((rand() & 0x0fff) | 0x4000);
	Data3 = rand() % 0x3fff + 0x8000;
	Data4 = rand();

	out[0] = Data1 >> 24;
	out[1] = Data1 >> 16;
	out[2] = Data1 >>  8;
	out[3] = Data1 >>  0;
	out[4] = Data2 >>  8;
	out[5] = Data2 >>  0;
	out[6] = Data3 >>  8;
	out[7] = Data3 >>  0;
	memcpy (out + 8, Data4, 8);
	return 1;
}

void machdep_free (void)
{
}

void target_run (void)
{
	//shellexecute (currprefs.win32_commandpathstart);
}

// dinput
int input_get_default_keyboard (int i)
{
	if (rawinput_enabled_keyboard) {
		return 1;
	} else {
		if (i == 0)
			return 1;
		return 0;
	}
}

// unicode
static unsigned int fscodepage;

char *ua_fs (const char *s, int defchar)
{
	return strdup(s);
}

char *ua_copy (char *dst, int maxlen, const char *src)
{
	dst[0] = 0;
	strncpy (dst, src, maxlen);
	return dst;
}

// win32gui
static int qs_override;

int target_cfgfile_load (struct uae_prefs *p, const TCHAR *filename, int type, int isdefault)
{
	int v, i, type2;
	int ct, ct2 = 0, size;
	char tmp1[MAX_DPATH], tmp2[MAX_DPATH];
	char fname[MAX_DPATH];

	_tcscpy (fname, filename);
	if (!zfile_exists (fname)) {
		fetch_configurationpath (fname, sizeof (fname) / sizeof (TCHAR));
		if (_tcsncmp (fname, filename, _tcslen (fname)))
			_tcscat (fname, filename);
		else
			_tcscpy (fname, filename);
	}

	if (!isdefault)
		qs_override = 1;
	if (type < 0) {
		type = 0;
		cfgfile_get_description (fname, NULL, NULL, NULL, &type);
	}
	if (type == 0 || type == 1) {
		discard_prefs (p, 0);
	}
	type2 = type;
	if (type == 0) {
		default_prefs (p, type);
	}
		
	//regqueryint (NULL, "ConfigFile_NoAuto", &ct2);
	v = cfgfile_load (p, fname, &type2, ct2, isdefault ? 0 : 1);
	if (!v)
		return v;
	if (type > 0)
		return v;
	for (i = 1; i <= 2; i++) {
		if (type != i) {
			size = sizeof (ct);
			ct = 0;
			//regqueryint (NULL, configreg2[i], &ct);
			if (ct && ((i == 1 && p->config_hardware_path[0] == 0) || (i == 2 && p->config_host_path[0] == 0) || ct2)) {
				size = sizeof (tmp1) / sizeof (TCHAR);
				//regquerystr (NULL, configreg[i], tmp1, &size);
				fetch_path ("ConfigurationPath", tmp2, sizeof (tmp2) / sizeof (TCHAR));
				_tcscat (tmp2, tmp1);
				v = i;
				cfgfile_load (p, tmp2, &v, 1, 0);
			}
		}
	}
	v = 1;
	return v;
}

// win32
uae_u8 *save_log (int bootlog, int *len)
{
	FILE *f;
	uae_u8 *dst = NULL;
	int size;

	if (!logging_started)
		return NULL;
	f = fopen (bootlog ? LOG_BOOT : LOG_NORMAL, "rb");
	if (!f)
		return NULL;
	fseek (f, 0, SEEK_END);
	size = ftell (f);
	fseek (f, 0, SEEK_SET);
	if (size > 30000)
		size = 30000;
	if (size > 0) {
		dst = xcalloc (uae_u8, size + 1);
		if (dst)
			fread (dst, 1, size, f);
		fclose (f);
		*len = size + 1;
	}
	return dst;
}

void stripslashes (TCHAR *p)
{
	while (_tcslen (p) > 0 && (p[_tcslen (p) - 1] == '\\' || p[_tcslen (p) - 1] == '/'))
		p[_tcslen (p) - 1] = 0;
}

void fixtrailing (TCHAR *p)
{
	if (_tcslen(p) == 0)
		return;
	if (p[_tcslen(p) - 1] == '/' || p[_tcslen(p) - 1] == '\\')
		return;
	_tcscat(p, "\\");
}

void getpathpart (TCHAR *outpath, int size, const TCHAR *inpath)
{
	_tcscpy (outpath, inpath);
	TCHAR *p = _tcsrchr (outpath, '\\');
	if (p)
		p[0] = 0;
	fixtrailing (outpath);
}

void getfilepart (TCHAR *out, int size, const TCHAR *path)
{
	out[0] = 0;
	const TCHAR *p = _tcsrchr (path, '\\');
	if (p)
		_tcscpy (out, p + 1);
	else
		_tcscpy (out, path);
}

void refreshtitle (void)
{
	if (isfullscreen () == 0)
		setmaintitle ();
}

// win32gui
#define MAX_ROM_PATHS 10
int scan_roms (int show)
{
	TCHAR path[MAX_DPATH];
	static int recursive;
	int id, i, ret, keys, cnt;
	TCHAR *paths[MAX_ROM_PATHS];

	if (recursive)
		return 0;
	recursive++;

//FIXME:
	cnt = 0;
	ret = 0;
	for (i = 0; i < MAX_ROM_PATHS; i++)
		paths[i] = NULL;

end:
	recursive--;
	return ret;
}

// dinput
int input_get_default_lightpen (struct uae_input_device *uid, int num, int port, int af, bool gp)
{
/*        struct didata *did;

        if (num >= num_mouse)
                return 0;
        did = &di_mouse[i];
        uid[num].eventid[ID_AXIS_OFFSET + 0][0] = INPUTEVENT_LIGHTPEN_HORIZ;
        uid[num].eventid[ID_AXIS_OFFSET + 1][0] = INPUTEVENT_LIGHTPEN_VERT;
        uid[num].eventid[ID_BUTTON_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_3RD_BUTTON : INPUTEVENT_JOY1_3RD_BUTTON;
        if (num == 0)
                return 1;*/
        return 0;
}

int input_get_default_joystick_analog (struct uae_input_device *uid, int num, int port, int af, bool gp)
{
/*        int j;
        struct didata *did;

        if (num >= num_joystick)
                return 0;
        did = &di_joystick[i];
        uid[num].eventid[ID_AXIS_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_HORIZ_POT : INPUTEVENT_JOY1_HORIZ_POT;
        uid[num].eventid[ID_AXIS_OFFSET + 1][0] = port ? INPUTEVENT_JOY2_VERT_POT : INPUTEVENT_JOY1_VERT_POT;
        uid[num].eventid[ID_BUTTON_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_LEFT : INPUTEVENT_JOY1_LEFT;
        if (isrealbutton (did, 1))
                uid[num].eventid[ID_BUTTON_OFFSET + 1][0] = port ? INPUTEVENT_JOY2_RIGHT : INPUTEVENT_JOY1_RIGHT;
        if (isrealbutton (did, 2))
                uid[num].eventid[ID_BUTTON_OFFSET + 2][0] = port ? INPUTEVENT_JOY2_UP : INPUTEVENT_JOY1_UP;
        if (isrealbutton (did, 3))
                uid[num].eventid[ID_BUTTON_OFFSET + 3][0] = port ? INPUTEVENT_JOY2_DOWN : INPUTEVENT_JOY1_DOWN;
        for (j = 2; j < MAX_MAPPINGS - 1; j++) {
                int am = did->axismappings[j];
                if (am == DIJOFS_POV(0) || am == DIJOFS_POV(1) || am == DIJOFS_POV(2) || am == DIJOFS_POV(3)) {
                        uid[num].eventid[ID_AXIS_OFFSET + j + 0][0] = port ? INPUTEVENT_JOY2_HORIZ_POT : INPUTEVENT_JOY1_HORIZ_POT;
                        uid[num].eventid[ID_AXIS_OFFSET + j + 1][0] = port ? INPUTEVENT_JOY2_VERT_POT : INPUTEVENT_JOY1_VERT_POT;
                        j++;
                }
        }
        if (num == 0)
                return 1;*/
        return 0;
}

// writelog
TCHAR* buf_out (TCHAR *buffer, int *bufsize, const TCHAR *format, ...)
{
	int count;
	va_list parms;
	va_start (parms, format);

	if (buffer == NULL)
		return 0;
	count = vsnprintf (buffer, (*bufsize) - 1, format, parms);
	va_end (parms);
	*bufsize -= _tcslen (buffer);
	return buffer + _tcslen (buffer);
}

// dinput
void setid (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt)
{
	// wrong place!
	uid->eventid[slot][SPARE_SUB_EVENT] = uid->eventid[slot][sub];
	uid->flags[slot][SPARE_SUB_EVENT] = uid->flags[slot][sub];
	uid->port[slot][SPARE_SUB_EVENT] = MAX_JPORTS + 1;
	xfree (uid->custom[slot][SPARE_SUB_EVENT]);
	uid->custom[slot][SPARE_SUB_EVENT] = uid->custom[slot][sub];
	uid->custom[slot][sub] = NULL;

	uid[i].eventid[slot][sub] = evt;
	uid[i].port[slot][sub] = port + 1;
}

void setid_af (struct uae_input_device *uid, int i, int slot, int sub, int port, int evt, int af)
{
	setid (uid, i, slot, sub, port, evt);
	uid[i].flags[slot][sub] &= ~(ID_FLAG_AUTOFIRE | ID_FLAG_TOGGLE);
	if (af >= JPORT_AF_NORMAL)
		uid[i].flags[slot][sub] |= ID_FLAG_AUTOFIRE;
	if (af == JPORT_AF_TOGGLE)
		uid[i].flags[slot][sub] |= ID_FLAG_TOGGLE;
}

// win32
void target_quit (void)
{
        //shellexecute (currprefs.win32_commandpathend);
}

void target_fixup_options (struct uae_prefs *p)
{

}

TCHAR start_path_data[MAX_DPATH];

void fetch_path (TCHAR *name, TCHAR *out, int size)
{
        int size2 = size;

	_tcscpy (start_path_data, "./");
        _tcscpy (out, start_path_data);
        if (!name)
                return;
/*        if (!_tcscmp (name, "FloppyPath"))
                _tcscat (out, "../shared/adf/");
        if (!_tcscmp (name, "CDPath"))
                _tcscat (out, "../shared/cd/");
        if (!_tcscmp (name, "hdfPath"))
                _tcscat (out, "../shared/hdf/");
        if (!_tcscmp (name, "KickstartPath"))
                _tcscat (out, "../shared/rom/");
        if (!_tcscmp (name, "ConfigurationPath"))
                _tcscat (out, "Configurations/");
*/
        if (!_tcscmp (name, "FloppyPath"))
                _tcscat (out, "./");
        if (!_tcscmp (name, "CDPath"))
                _tcscat (out, "./");
        if (!_tcscmp (name, "hdfPath"))
                _tcscat (out, "./");
        if (!_tcscmp (name, "KickstartPath"))
                _tcscat (out, "./");
        if (!_tcscmp (name, "ConfigurationPath"))
                _tcscat (out, "./");

}

void fetch_saveimagepath (TCHAR *out, int size, int dir)
{
/*        assert (size > MAX_DPATH);
        fetch_path ("SaveimagePath", out, size);
        if (dir) {
                out[_tcslen (out) - 1] = 0;
                createdir (out);*/
                fetch_path ("SaveimagePath", out, size);
//        }
}

void fetch_configurationpath (TCHAR *out, int size)
{
	fetch_path ("ConfigurationPath", out, size);
}

void fetch_screenshotpath (TCHAR *out, int size)
{
	fetch_path ("ScreenshotPath", out, size);
}

void fetch_ripperpath (TCHAR *out, int size)
{
	fetch_path ("RipperPath", out, size);
}

void fetch_statefilepath (TCHAR *out, int size)
{
	fetch_path ("StatefilePath", out, size);
}

void fetch_inputfilepath (TCHAR *out, int size)
{
	fetch_path ("InputPath", out, size);
}

void fetch_datapath (TCHAR *out, int size)
{
	fetch_path (NULL, out, size);
}

// convert path to absolute or relative
void fullpath (TCHAR *path, int size)
{
	if (path[0] == 0 || (path[0] == '\\' && path[1] == '\\') || path[0] == ':')
		return;
        /* <drive letter>: is supposed to mean same as <drive letter>:\ */
}

TCHAR *au (const char *s)
{
	return strdup(s);
}

//
TCHAR *au_copy (TCHAR *dst, int maxlen, const char *src)
{
	dst[0] = 0;
	memcpy (dst, src, maxlen);
	return dst;
}

// writelog
int consoleopen = 0;
static int realconsole = 1;

static int debugger_type = -1;

static void openconsole (void)
{
	if (realconsole) {
		if (debugger_type == 2) {
			//open_debug_window ();
			consoleopen = 1;
		} else {
			//close_debug_window ();
			consoleopen = -1;
		}
		return;
	}
}

void close_console (void)
{
	if (realconsole)
		return;
}

bool console_isch (void)
{
	return false;
}

TCHAR console_getch (void)
{
        return 0;
}

void debugger_change (int mode)
{
	if (mode < 0)
		debugger_type = debugger_type == 2 ? 1 : 2;
	else
		debugger_type = mode;
	if (debugger_type != 1 && debugger_type != 2)
		debugger_type = 2;
//	  regsetint (NULL, "DebuggerType", debugger_type);
	openconsole ();
}

// unicode
char *ua (const TCHAR *s)
{
	return strdup(s);
}

char *uutf8 (const char *s)
{
	return strdup(s);
}

char *utf8u (const char *s)
{
	return strdup(s);
}

// fsdb_mywin32
FILE *my_opentext (const TCHAR *name)
{
	FILE *f;
	uae_u8 tmp[4];
	int v;

	f = _tfopen (name, "rb");
	if (!f)
		return NULL;
	v = fread (tmp, 1, 4, f);
	fclose (f);
	if (v == 4) {
		if (tmp[0] == 0xef && tmp[1] == 0xbb && tmp[2] == 0xbf)
			return _tfopen (name, "r, ccs=UTF-8");
		if (tmp[0] == 0xff && tmp[1] == 0xfe)
			return _tfopen (name, "r, ccs=UTF-16LE");
	}
	return _tfopen (name, "r");
}

// dxwrap
int DirectDraw_CurrentRefreshRate (void)
{
	//DirectDraw_GetDisplayMode ();
	//return dxdata.native.dwRefreshRate;
	return 50;
}

int DirectDraw_GetVerticalBlankStatus (void)
{
//        BOOL status;
//        if (FAILED (IDirectDraw7_GetVerticalBlankStatus (dxdata.maindd, &status)))
                return -1;
//        return status;
}

// direct3d
int D3D_goodenough (void)
{
	return 0;
}

// debug_win32
void update_debug_info(void)
{
}

///////////////////////////////////////////////////
// win32gfx.cpp
///////////////////////////////////////////////////
#define MAX_DISPLAYS 10
struct MultiDisplay Displays[MAX_DISPLAYS];

static struct MultiDisplay *getdisplay2 (struct uae_prefs *p, int index)
{
	write_log ("Multimonitor detection disabled\n");
	Displays[0].primary = 1;
	Displays[0].monitorname = "Display";

	int max;
	int display = index < 0 ? p->gfx_apmode[screen_is_picasso ? APMODE_RTG : APMODE_NATIVE].gfx_display - 1 : index;

	max = 0;
	while (Displays[max].monitorname)
		max++;
	if (max == 0) {
		gui_message (_T("no display adapters! Exiting"));
		exit (0);
	}
	if (index >= 0 && display >= max)
		return NULL;
	if (display >= max)
		display = 0;
	if (display < 0)
		display = 0;
	return &Displays[display];
}

struct MultiDisplay *getdisplay (struct uae_prefs *p)
{
	return getdisplay2 (p, -1);
}

int target_get_display (const TCHAR *name)
{
	int oldfound = -1;
	int found = -1;
	unsigned int i;
	for (i = 0; Displays[i].monitorname; i++) {
		struct MultiDisplay *md = &Displays[i];
		if (!_tcscmp (md->adapterid, name))
			found = i + 1;
		if (!_tcscmp (md->adaptername, name))
			found = i + 1;
		if (!_tcscmp (md->monitorname, name))
			found = i + 1;
		if (!_tcscmp (md->monitorid, name))
			found = i + 1;
		if (found >= 0) {
			if (oldfound != found)
				return -1;
			oldfound = found;
		}
	}
	return -1;
}
const TCHAR *target_get_display_name (int num, bool friendlyname)
{
	if (num <= 0)
		return NULL;
	struct MultiDisplay *md = getdisplay2 (NULL, num - 1);
	if (!md)
		return NULL;
	if (friendlyname)
		return md->monitorname;
	return md->monitorid;
}

static int isfullscreen_2 (struct uae_prefs *p)
{
    int idx = screen_is_picasso ? 1 : 0;
    return p->gfx_apmode[idx].gfx_fullscreen == GFX_FULLSCREEN ? 1 : (p->gfx_apmode[idx].gfx_fullscreen == GFX_FULLWINDOW ? -1 : 0);
}

int isfullscreen (void)
{
	return isfullscreen_2 (&currprefs);
}

void addmode (struct MultiDisplay *md, int w, int h, int d, int freq, int rawmode)
{
	int ct;
	int i, j;
//	int w = dm->dmPelsWidth;
//	int h = dm->dmPelsHeight;
//	int d = dm->dmBitsPerPel;
	bool lace = false;

/*	int freq = 0;
	if (dm->dmFields & DM_DISPLAYFREQUENCY) {
		freq = dm->dmDisplayFrequency;
		if (freq < 10)
			freq = 0;
	}
	if (dm->dmFields & DM_DISPLAYFLAGS) {
		lace = (dm->dmDisplayFlags & DM_INTERLACED) != 0;
	}*/

	ct = 0;
	if (d == 8)
		ct = RGBMASK_8BIT;
	if (d == 15)
		ct = RGBMASK_15BIT;
	if (d == 16)
		ct = RGBMASK_16BIT;
	if (d == 24)
		ct = RGBMASK_24BIT;
	if (d == 32)
		ct = RGBMASK_32BIT;
	if (ct == 0)
		return;
	d /= 8;
	i = 0;
	while (md->DisplayModes[i].depth >= 0) {
		if (md->DisplayModes[i].depth == d && md->DisplayModes[i].res.width == w && md->DisplayModes[i].res.height == h) {
			for (j = 0; j < MAX_REFRESH_RATES; j++) {
				if (md->DisplayModes[i].refresh[j] == 0 || md->DisplayModes[i].refresh[j] == freq)
					break;
			}
			if (j < MAX_REFRESH_RATES) {
				md->DisplayModes[i].refresh[j] = freq;
				md->DisplayModes[i].refreshtype[j] = rawmode;
				md->DisplayModes[i].refresh[j + 1] = 0;
				return;
			}
		}
		i++;
	}
	i = 0;
	while (md->DisplayModes[i].depth >= 0)
		i++;
	if (i >= MAX_PICASSO_MODES - 1)
		return;
//	md->DisplayModes[i].rawmode = rawmode;
//	md->DisplayModes[i].lace = lace;
	md->DisplayModes[i].res.width = w;
	md->DisplayModes[i].res.height = h;
	md->DisplayModes[i].depth = d;
	md->DisplayModes[i].refresh[0] = freq;
	md->DisplayModes[i].refreshtype[0] = rawmode;
	md->DisplayModes[i].refresh[1] = 0;
	md->DisplayModes[i].colormodes = ct;
	md->DisplayModes[i + 1].depth = -1;
	_stprintf (md->DisplayModes[i].name, _T("%dx%d%s, %d-bit"),
		md->DisplayModes[i].res.width, md->DisplayModes[i].res.height,
		lace ? _T("i") : _T(""),
		md->DisplayModes[i].depth * 8);
}

void updatedisplayarea (void)
{
/*
	if (!screen_is_initialized)
		return;
	if (dx_islost ())
		return;
	if (picasso_on)
		return;
#if defined (GFXFILTER)
	if (currentmode->flags & DM_D3D) {
#if defined (D3D)
		D3D_refresh ();
#endif
	} else
#endif
		if (currentmode->flags & DM_DDRAW) {
#if defined (GFXFILTER)
			if (currentmode->flags & DM_SWSCALE)
				S2X_refresh ();
#endif
			DirectDraw_Flip (0);
		} 
*/
}


bool target_graphics_buffer_update (void)
{
/*
	int w, h;
	
	if (screen_is_picasso) {
		w = picasso96_state.Width > picasso_vidinfo.width ? picasso96_state.Width : picasso_vidinfo.width;
		h = picasso96_state.Height > picasso_vidinfo.height ? picasso96_state.Height : picasso_vidinfo.height;
	} else {
		struct vidbuffer *vb = gfxvidinfo.drawbuffer.tempbufferinuse ? &gfxvidinfo.tempbuffer : &gfxvidinfo.drawbuffer;
		gfxvidinfo.outbuffer = vb;
		w = vb->outwidth;
		h = vb->outheight;
	}
	
	if (oldtex_w == w && oldtex_h == h && oldtex_rtg == screen_is_picasso)
		return true;
	oldtex_w = w;
	oldtex_h = h;
	oldtex_rtg = screen_is_picasso;

	write_log (_T("Buffer size (%d*%d) %s\n"), w, h, screen_is_picasso ? _T("RTG") : _T("Native"));

	S2X_free ();
	if (currentmode->flags & DM_D3D) {
		D3D_alloctexture (w, h);
	} else {
		DirectDraw_ClearSurface (NULL);
	}
	if (currentmode->flags & DM_SWSCALE) {
		S2X_init (currentmode->native_width, currentmode->native_height, currentmode->native_depth);
	}
*/
	return true;
}

static bool render_ok;

int vsync_busy_wait_mode;

static bool vblanklaceskip (void)
{
        if (vblankbaselace_chipset >= 0 && vblankbaselace) {
                if ((vblankbaselace_chipset && !vblankthread_oddeven) || (!vblankbaselace_chipset && vblankthread_oddeven))
                        return true;
        }
        return false;
}

static bool vblanklaceskip_check (void)
{
        int vp = -2;
        if (!vblanklaceskip ()) {
//              if (vblankbaselace_chipset >= 0)
//                      write_log (_T("%d == %d\n"), vblankbaselace_chipset, vblankthread_oddeven);
                return false;
        }
        write_log (_T("Interlaced frame type mismatch %d<>%d\n"), vblankbaselace_chipset, vblankthread_oddeven);
        return true; 
}

static void vsync_sleep (bool preferbusy)
{
	struct apmode *ap = picasso_on ? &currprefs.gfx_apmode[1] : &currprefs.gfx_apmode[0];
	bool dowait;

	if (vsync_busy_wait_mode == 0) {
		dowait = ap->gfx_vflip || !preferbusy;
		//dowait = !preferbusy;
	} else if (vsync_busy_wait_mode < 0) {
		dowait = true;
	} else {
		dowait = false;
	}
	if (dowait && (currprefs.m68k_speed >= 0 || currprefs.m68k_speed_throttle < 0))
		sleep_millis_main (1);
}

static void changevblankthreadmode_do (int newmode, bool fast)
{ 
	int t = vblankthread_counter;
	vblank_found = false;
	vblank_found_chipset = false;
	vblank_found_rtg = false;
	if (vblankthread_mode <= 0 || vblankthread_mode == newmode)
		return;
	vblankthread_mode = newmode;
	if (newmode == VBLANKTH_KILL) {
		flipthread_mode = 0;
//		SetEvent (flipevent);
		while (flipthread_mode == 0)
			sleep_millis_main (1);
//		CloseHandle (flipevent);
//		CloseHandle (flipevent2);
//		CloseHandle (vblankwaitevent);
		flipevent = NULL;
		flipevent2 = NULL;
		vblankwaitevent = NULL;
	}
	if (!fast) {
		while (t == vblankthread_counter && vblankthread_mode > 0);
	}
}
         
static void changevblankthreadmode (int newmode)
{
	changevblankthreadmode_do (newmode, false);
}
static void changevblankthreadmode_fast (int newmode)                         
{
	changevblankthreadmode_do (newmode, true);
}

static void waitflipevent (void)
{
/*	while (flipevent_mode) {
		if (WaitForSingleObject (flipevent2, 10) == WAIT_ABANDONED)
			break;
	}*/
}
static void doflipevent (void)
{
	if (flipevent == NULL)
		return;
	waitflipevent ();
	flipevent_mode = 1;
//	SetEvent (flipevent);
}

bool show_screen_maybe (bool show)
{
	struct apmode *ap = picasso_on ? &currprefs.gfx_apmode[1] : &currprefs.gfx_apmode[0];
	if (!ap->gfx_vflip || ap->gfx_vsyncmode == 0 || !ap->gfx_vsync) {
		if (show)
			show_screen ();
		return false;
	}
#if 0
	if (ap->gfx_vflip < 0) {
		doflipevent ();
		return true;
	}
#endif
	return false;
}

bool render_screen (bool immediate)
{
	render_ok = false;
	return render_ok;
}

void show_screen (void)
{
	render_ok = false;
}

static int maxscanline, minscanline, prevvblankpos;

static bool getvblankpos (int *vp)
{
	int sl;
#if 0
	frame_time_t t = read_processor_time ();
#endif
	*vp = -2;
/*	if (currprefs.gfx_api) {
		if (!D3D_getvblankpos (&sl))
			return false;
	} else {
		if (!DD_getvblankpos (&sl))
			return false;
	}*/
#if 0
	t = read_processor_time () - t;
	write_log (_T("(%d:%d)"), t, sl);
#endif	
	prevvblankpos = sl;
	if (sl > maxscanline)
		maxscanline = sl;
	if (sl > 0) {
		vblankthread_oddeven = (sl & 1) != 0;
		if (sl < minscanline || minscanline < 0)
			minscanline = sl;
	}
	*vp = sl;
	return true;
}

static bool getvblankpos2 (int *vp, int *flags)
{
	if (!getvblankpos (vp))
		return false;
	if (*vp > 100 && flags) {
		if ((*vp) & 1)
			*flags |= 2;
		else
			*flags |= 1;
	}
	return true;
}

double vblank_calibrate (double approx_vblank, bool waitonly)
{
	frame_time_t t1, t2;
	double tsum, tsum2, tval, tfirst, div;
	int maxcnt, maxtotal, total, cnt, tcnt2;
//	HANDLE th;
	int maxvpos, mult;
	int width, height, depth, rate, mode;
	struct remembered_vsync *rv;
	double rval = -1;
	struct apmode *ap = picasso_on ? &currprefs.gfx_apmode[1] : &currprefs.gfx_apmode[0];
	bool remembered = false;
	bool lace = false;

	if (picasso_on) {
		width = picasso96_state.Width;
		height = picasso96_state.Height;
		depth = picasso96_state.BytesPerPixel;
	} else {
		width = currentmode->native_width;
		height = currentmode->native_height;
		depth = (currentmode->native_depth + 7) / 8;
	}

	rate = ap->gfx_refreshrate;
	mode = isfullscreen ();

/*	
	// clear remembered modes if restarting and start thread again.
	if (vblankthread_mode <= 0) {
		rv = vsyncmemory;
		while (rv) {
			struct remembered_vsync *rvo = rv->next;
			xfree (rv);
			rv = rvo;
		}
		vsyncmemory = NULL;
	}

	rv = vsyncmemory;
	while (rv) {
		if (rv->width == width && rv->height == height && rv->depth == depth && rv->rate == rate && rv->mode == mode && rv->rtg == picasso_on) {
			approx_vblank = rv->remembered_rate2;
			tsum = rval = rv->remembered_rate;
			maxscanline = rv->maxscanline;
			minscanline = rv->minscanline;
			vblankbaseadjust = rv->remembered_adjust;
			maxvpos = rv->maxvpos;
			lace = rv->lace;
			waitonly = true;
			remembered = true;
			goto skip;
		}
		rv = rv->next;
	}
	
	th = GetCurrentThread ();
	int oldpri = GetThreadPriority (th);
	SetThreadPriority (th, THREAD_PRIORITY_HIGHEST);
	if (vblankthread_mode <= VBLANKTH_KILL) {
		unsigned th;
		vblankthread_mode = VBLANKTH_CALIBRATE;
		_beginthreadex (NULL, 0, vblankthread, 0, 0, &th);
		flipthread_mode = 1;
		flipevent_mode = 0;
		flipevent = CreateEvent (NULL, FALSE, FALSE, NULL);
		flipevent2 = CreateEvent (NULL, FALSE, FALSE, NULL);
		vblankwaitevent = CreateEvent (NULL, FALSE, FALSE, NULL);
		_beginthreadex (NULL, 0, flipthread, 0, 0, &th);
	} else {
		changevblankthreadmode (VBLANKTH_CALIBRATE);
	}
	sleep_millis (100);

	maxtotal = 10;
	maxcnt = maxtotal;
	maxscanline = 0;
	minscanline = -1;
	tsum2 = 0;
	tcnt2 = 0;
	for (maxcnt = 0; maxcnt < maxtotal; maxcnt++) {
		total = 5;
		tsum = 0;
		cnt = total;
		for (cnt = 0; cnt < total; cnt++) {
			int maxvpos1, maxvpos2;
			int flags1, flags2;
			if (!waitvblankstate (true, NULL, NULL))
				goto fail;
			if (!waitvblankstate (false, NULL, NULL))
				goto fail;
			if (!waitvblankstate (true, NULL, NULL))
				goto fail;
			t1 = read_processor_time ();
			if (!waitvblankstate (false, NULL, NULL))
				goto fail;
			maxscanline = 0;
			if (!waitvblankstate (true, &maxvpos1, &flags1))
				goto fail;
			if (!waitvblankstate (false, NULL, NULL))
				goto fail;
			maxscanline = 0;
			if (!waitvblankstate (true, &maxvpos2, &flags2))
				goto fail;
			t2 = read_processor_time ();
			maxvpos = maxvpos1 > maxvpos2 ? maxvpos1 : maxvpos2;
			// count two fields: works with interlaced modes too.
			tval = (double)syncbase * 2.0 / (t2 - t1);
			if (cnt == 0)
				tfirst = tval;
			if (abs (tval - tfirst) > 1) {
				write_log (_T("Very unstable vsync! %.6f vs %.6f, retrying..\n"), tval, tfirst);
				break;
			}
			tsum2 += tval;
			tcnt2++;
			if (abs (tval - tfirst) > 0.1) {
				write_log (_T("Unstable vsync! %.6f vs %.6f\n"), tval, tfirst);
				break;
			}
			tsum += tval;
			if ((flags1 > 0 && flags1 < 3) && (flags2 > 0 && flags2 < 3) && (flags1 != flags2)) {
				lace = true;
			}
		}
		if (cnt >= total)
			break;
	}
	vblankbaseadjust = timezeroonevblank (-1, 1);

	changevblankthreadmode (VBLANKTH_IDLE);

	if (maxcnt >= maxtotal) {
		tsum = tsum2 / tcnt2;
		write_log (_T("Unstable vsync reporting, using average value\n"));
	} else {
		tsum /= total;
	}

	if (ap->gfx_vflip == 0) {
		int vsdetect = 0;
		int detectcnt = 6;
		for (cnt = 0; cnt < detectcnt; cnt++) {
			render_screen (true);
			show_screen ();
			sleep_millis (1);
			frame_time_t t = read_processor_time () + 1 * (syncbase / tsum);
			for (int cnt2 = 0; cnt2 < 4; cnt2++) {
				render_ok = true;
				show_screen ();
			}
			int diff = (int)read_processor_time () - (int)t;
			if (diff >= 0)
				vsdetect++;
		}
		if (vsdetect >= detectcnt / 2) {
			write_log (L"Forced vsync detected, switching to double buffered\n");
			changed_prefs.gfx_apmode[0].gfx_backbuffers = 1;
		}
	}

	SetThreadPriority (th, oldpri);

	if (waitonly)
		tsum = approx_vblank;
skip:

	vblank_skipeveryother = false;
	getvsyncrate (tsum, &mult);
	if (mult < 0) {
		div = 2.0;
		vblank_skipeveryother = true;
	} else if (mult > 0) {
		div = 0.5;
	} else {
		div = 1.0;
	}
	tsum2 = tsum / div;

	vblankbasefull = (syncbase / tsum2);
	vblankbasewait1 = (syncbase / tsum2) * 70 / 100;
	vblankbasewait2 = (syncbase / tsum2) * 55 / 100;
	vblankbasewait3 = (syncbase / tsum2) * 99 / 100 - syncbase / (250 * (vblank_skipeveryother ? 1 : 2)); // at least 2ms before vblank
	vblankbaselace = lace;

	write_log (_T("VSync %s: %.6fHz/%.1f=%.6fHz. MinV=%d MaxV=%d%s Adj=%d Units=%d %.1f%%\n"),
		waitonly ? _T("remembered") : _T("calibrated"), tsum, div, tsum2,
		minscanline, maxvpos, lace ? _T("i") : _T(""), vblankbaseadjust, vblankbasefull,
		vblankbasewait3 * 100 / (syncbase / tsum2));

	if (minscanline == 1) {
		if (vblankbaseadjust < 0)
			vblankbaseadjust = 0;
		else if (vblankbaseadjust > vblankbasefull / 10)
			vblankbaseadjust = vblankbasefull / 10;
	} else {
		vblankbaseadjust = 0;
	}

	remembered_vblank = tsum;
	vblank_prev_time = read_processor_time ();
	
	if (!remembered) {
		rv = xcalloc (struct remembered_vsync, 1);
		rv->width = width;
		rv->height = height;
		rv->depth = depth;
		rv->rate = rate;
		rv->mode = isfullscreen ();
		rv->rtg = picasso_on;
		rv->remembered_rate = tsum;
		rv->remembered_rate2 = tsum2;
		rv->remembered_adjust = vblankbaseadjust;
		rv->maxscanline = maxscanline;
		rv->minscanline = minscanline;
		rv->maxvpos = maxvpos;
		rv->lace = lace;
		if (vsyncmemory == NULL) {
			vsyncmemory = rv;
		} else {
			rv->next = vsyncmemory;
			vsyncmemory = rv;
		}
	}
	
	vblank_reset (tsum);
	return tsum;
fail:*/
	write_log (_T("VSync calibration failed\n"));
	ap->gfx_vsync = 0;
	return -1;
}

static bool waitvblankstate (bool state, int *maxvpos, int *flags)
{
	int vp;
	if (flags)
		*flags = 0;
	for (;;) {
		int omax = maxscanline;
		if (!getvblankpos2 (&vp, flags))
			return false;
		while (omax != maxscanline) {
			omax = maxscanline;
			if (!getvblankpos2 (&vp, flags))
				return false;
		}
		if (maxvpos)
			*maxvpos = maxscanline;
		if (vp < 0) {
			if (state)
				return true;
		} else {
			if (!state)
				return true;
		}
	}
}

static int vblank_wait (void)
{
	int vp;

	for (;;) {
		int opos = prevvblankpos;
		if (!getvblankpos (&vp))
			return -2;
		if (opos > (maxscanline + minscanline) / 2 && vp < (maxscanline + minscanline) / 3)
			return vp;
		if (vp <= 0)
			return vp;
		vsync_sleep (true);
	}
}

static bool isthreadedvsync (void)
{
	return isvsync_chipset () <= -2 || isvsync_rtg () < 0;
}

frame_time_t vsync_busywait_end (int *flipdelay)
{
/*
	if (isthreadedvsync ()) {
		frame_time_t prev;

		if (!currprefs.turbo_emulation) {
			for (;;) {
				int v = vblankthread_mode;
				if (v != VBLANKTH_ACTIVE_START && v != VBLANKTH_ACTIVE_SKIPFRAME && v != VBLANKTH_ACTIVE_SKIPFRAME2)
					break;
				sleep_millis_main (1);
			}
			prev = vblank_prev_time;
			if (!dooddevenskip) {
				int delay = 10;
				frame_time_t t = read_processor_time ();
				while (delay-- > 0) {
					if (WaitForSingleObject (vblankwaitevent, 10) != WAIT_TIMEOUT)
						break;
				}
				idletime += read_processor_time () - t;
			}
			if (flipdelay)
				*flipdelay = vblank_found_flipdelay;
		} else {
			show_screen ();
			prev = read_processor_time ();
		}
		changevblankthreadmode_fast (VBLANKTH_ACTIVE_WAIT);
		return prev + vblankbasefull;
	} else {
		if (flipdelay)
			*flipdelay = vblank_flip_delay;
		return vblank_prev_time;
	}
*/
}

void vsync_busywait_start (void)
{
	if (vblankthread_mode < 0)
		write_log (_T("low latency threaded mode but thread is not running!?\n"));
	else if (vblankthread_mode != VBLANKTH_ACTIVE_WAIT)
		write_log (_T("low latency vsync state mismatch %d\n"), vblankthread_mode);
	changevblankthreadmode_fast (VBLANKTH_ACTIVE_START);
}

bool vsync_busywait_do (int *freetime, bool lace, bool oddeven)
{
	bool v;
	static bool framelost;
	int ti;
	frame_time_t t;
	frame_time_t prevtime = vblank_prev_time;
	struct apmode *ap = picasso_on ? &currprefs.gfx_apmode[1] : &currprefs.gfx_apmode[0];

	dooddevenskip = false;
	if (lace)
		vblankbaselace_chipset = oddeven == true ? 1 : 0;
	else
		vblankbaselace_chipset = -1;

	t = read_processor_time ();
	ti = t - prevtime;
	if (ti > 2 * vblankbasefull || ti < -2 * vblankbasefull) {
		changevblankthreadmode_fast (VBLANKTH_ACTIVE_WAIT);
		waitvblankstate (false, NULL, NULL);
		vblank_prev_time = t;
		thread_vblank_time = t;
		frame_missed++;
		return true;
	}

	if (log_vsync & 1) {
		write_log (_T("F:%8d M:%8d E:%8d %3d%% (%3d%%) %10d\r"), frame_counted, frame_missed, frame_errors, frame_usage, frame_usage_avg, (t - vblank_prev_time) - vblankbasefull);
	}

	if (freetime)
		*freetime = 0;

	frame_usage = (t - prevtime) * 100 / vblankbasefull;
	if (frame_usage > 99)
		frame_usage = 99;
	else if (frame_usage < 0)
		frame_usage = 0;
	frame_usage_total += frame_usage;
	if (freetime)
		*freetime = frame_usage;
	if (frame_counted)
		frame_usage_avg = frame_usage_total / frame_counted;

	v = 0;

	if (isthreadedvsync ()) {

		framelost = false;
		v = 1;

	} else {
		int vp;

		vblank_flip_delay = 0;
		dooddevenskip = false;

		if (vblanklaceskip_check ()) {

			vblank_prev_time = read_processor_time () + vblankbasewait1;
			dooddevenskip = true;
			framelost = false;
			v = -1;

		} else if (currprefs.turbo_emulation) {

			show_screen ();
			vblank_prev_time = read_processor_time ();
			framelost = true;
			v = -1;

		} else {

			while (!framelost && read_processor_time () - prevtime < vblankbasewait1) {
				vsync_sleep (false);
			}

			vp = vblank_wait ();
			if (vp >= -1) {
				vblank_prev_time = read_processor_time ();
				if (ap->gfx_vflip == 0) {
					show_screen ();
					vblank_flip_delay = (read_processor_time () - vblank_prev_time) / (vblank_skipeveryother ? 2 : 1);
					if (vblank_flip_delay < 0)
						vblank_flip_delay = 0;
					else if (vblank_flip_delay > vblankbasefull * 2 / 3)
						vblank_flip_delay = vblankbasefull * 2 / 3;
				}
				for (;;) {
					if (!getvblankpos (&vp))
						break;
					if (vp > 0)
						break;
					sleep_millis (1);
				}
				if (ap->gfx_vflip != 0) {
					show_screen ();
				}
				vblank_prev_time -= vblankbaseadjust;
				vblank_prev_time -= (vblankbasefull * vp / maxscanline) / (vblank_skipeveryother ? 2 : 1 );

				v = framelost ? -1 : 1;
			}

			framelost = false;
		}
		getvblankpos (&vp);
	}

	if (v) {
		frame_counted++;
		return v;
	}
	frame_errors++;
	return 0;
}

double getcurrentvblankrate (void)
{
	if (remembered_vblank)
		return remembered_vblank;
/*
	if (currprefs.gfx_api)
		return D3D_getrefreshrate ();
	else
		return DirectDraw_CurrentRefreshRate ();
*/
	return 50;
}

///////////////////////////////////////////////////
// parser.c
///////////////////////////////////////////////////
void serialuartbreak (int v)
{
	if (/*hCom == INVALID_HANDLE_VALUE ||*/ !currprefs.use_serial)
		return;

/*
	if (v)
		EscapeCommFunction (hCom, SETBREAK);
	else
		EscapeCommFunction (hCom, CLRBREAK);
*/
}

unsigned int flashscreen;   

void doflashscreen (void)
{
/*
        flashscreen = 10;
        init_colors ();
        picasso_refresh ();
        reset_drawing ();
        flush_screen (gfxvidinfo.outbuffer, 0, 0);
*/
}

// posix
uae_u32 getlocaltime (void)
{
/*
        SYSTEMTIME st;
        FILETIME ft;
        ULARGE_INTEGER t;

        GetLocalTime (&st);
        SystemTimeToFileTime (&st, &ft);
        t.LowPart = ft.dwLowDateTime;
        t.HighPart = ft.dwHighDateTime;
        t.QuadPart -= 11644473600000 * 10000;
        return (uae_u32)(t.QuadPart / 10000000);
*/
}

/*
#ifndef HAVE_ISINF
int isinf (double x)
{
        const int nClass = _fpclass (x);
        int result;
        if (nClass == _FPCLASS_NINF || nClass == _FPCLASS_PINF)
                result = 1;
        else
                result = 0;
        return result;
}
#endif
*/
