/*
 * PUAE - The Un*x Amiga Emulator
 *
 * A collection of ugly and random stuff brought in from Win32
 * which desparately needs to be tidied up
 *
 * Copyright 2004 Richard Drummond
 * Copyright 2010-2011 Mustafa TUFAN
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "misc.h"
#include "options.h"
#include "memory.h"
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
#include "sleep.h"

#define TRUE 1
#define FALSE 0

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
unsigned int flashscreen;

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

// serial
unsigned int seriallog = 0;

// dinput
int rawkeyboard = -1;
static bool rawinput_enabled_mouse, rawinput_enabled_keyboard;
int no_rawinput;

int is_tablet (void)
{
	return tablet ? 1 : 0;
}

//win32gfx.cpp
static double remembered_vblank;
static int vblankbasewait, vblankbasefull;

void getgfxoffset (int *dxp, int *dyp, int *mxp, int *myp)
{
	*dxp = 0;
	*dyp = 0;
	*mxp = 0;
	*myp = 0;
}

int vsync_switchmode (int hz, int oldhz)
{
	static int tempvsync;
	int w = currentmode->native_width;
	int h = currentmode->native_height;
	int d = currentmode->native_depth / 8;
//        struct MultiDisplay *md = getdisplay (&currprefs);
	struct PicassoResolution *found;

	int newh, i, cnt;
	int dbl = getvsyncrate (currprefs.chipset_refreshrate) != currprefs.chipset_refreshrate ? 2 : 1;

	if (hz < 0)
		return tempvsync;

	newh = h * oldhz / hz;
	hz = hz * dbl;

	found = NULL;
        
/*	for (i = 0; md->DisplayModes[i].depth >= 0 && !found; i++) {
		struct PicassoResolution *r = &md->DisplayModes[i];
		if (r->res.width == w && r->res.height == h && r->depth == d) {
			int j;
			for (j = 0; r->refresh[j] > 0; j++) {
				if (r->refresh[j] == oldhz) {
					found = r;
					break;
				}
			}
		}
	}*/
	if (found == NULL) {
		write_log ("refresh rate changed to %d but original rate was not found\n", hz);
		return 0;
	}

	found = NULL;
/*        for (cnt = 0; cnt <= abs (newh - h) + 1 && !found; cnt++) {
                for (i = 0; md->DisplayModes[i].depth >= 0 && !found; i++) {
                        struct PicassoResolution *r = &md->DisplayModes[i];
                        if (r->res.width == w && (r->res.height == newh + cnt || r->res.height == newh - cnt) && r->depth == d) {
                                int j;
                                for (j = 0; r->refresh[j] > 0; j++) {
                                        if (r->refresh[j] == hz) {
                                                found = r;
                                                break;
                                        }
                                }
                        }
                }
        }*/
        if (!found) {
                tempvsync = currprefs.gfx_avsync;
                changed_prefs.gfx_avsync = 0;
                write_log ("refresh rate changed to %d but no matching screenmode found, vsync disabled\n", hz);
        } else {
                newh = found->res.height;
                changed_prefs.gfx_size_fs.height = newh;
                changed_prefs.gfx_refreshrate = hz;
                write_log ("refresh rate changed to %d, new screenmode %dx%d\n", hz, w, newh);
        }
/*
        reopen (1);
*/
        return 0;
}

// serial_win32
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

/*
static int drvsampleres[] = {
        IDR_DRIVE_CLICK_A500_1, DS_CLICK,
        IDR_DRIVE_SPIN_A500_1, DS_SPIN,
        IDR_DRIVE_SPINND_A500_1, DS_SPINND,
        IDR_DRIVE_STARTUP_A500_1, DS_START,
        IDR_DRIVE_SNATCH_A500_1, DS_SNATCH,
        -1
};
*/

// driveclick_win32
int driveclick_loadresource (struct drvsample *sp, int drivetype)
{
/*
        int i, ok;

        ok = 1;
        for (i = 0; drvsampleres[i] >= 0; i += 2) {
                struct drvsample *s = sp + drvsampleres[i + 1];
                HRSRC res = FindResource (NULL, MAKEINTRESOURCE (drvsampleres[i + 0]), "WAVE");
                if (res != 0) {
                        HANDLE h = LoadResource (NULL, res);
                        int len = SizeofResource (NULL, res);
                        uae_u8 *p = LockResource (h);
                        s->p = decodewav (p, &len);
                        s->len = len;
                } else {
                        ok = 0;
                }
        }
        return ok;
*/
	return 0;
}

void driveclick_fdrawcmd_close(int drive)
{
/*
        if (h[drive] != INVALID_HANDLE_VALUE)
                CloseHandle(h[drive]);
        h[drive] = INVALID_HANDLE_VALUE;
        motors[drive] = 0;
*/
}

static int driveclick_fdrawcmd_open_2(int drive)
{
/*
        TCHAR s[32];

        driveclick_fdrawcmd_close(drive);
        _stprintf (s, "\\\\.\\fdraw%d", drive);
        h[drive] = CreateFile(s, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h[drive] == INVALID_HANDLE_VALUE)
                return 0;
        return 1;
*/
	return 0;
}

int driveclick_fdrawcmd_open(int drive)
{
/*
        if (!driveclick_fdrawcmd_open_2(drive))
                return 0;
        driveclick_fdrawcmd_init(drive);
        return 1;
*/
                return 0;
}

void driveclick_fdrawcmd_detect(void)
{
/*
        static int detected;
        if (detected)
                return;
        detected = 1;
        if (driveclick_fdrawcmd_open_2(0))
                driveclick_pcdrivemask |= 1;
        driveclick_fdrawcmd_close(0);
        if (driveclick_fdrawcmd_open_2(1))
                driveclick_pcdrivemask |= 2;
        driveclick_fdrawcmd_close(1);
*/
}

void driveclick_fdrawcmd_seek(int drive, int cyl)
{
//        write_comm_pipe_int (dc_pipe, (drive << 8) | cyl, 1);
}
void driveclick_fdrawcmd_motor (int drive, int running)
{
//        write_comm_pipe_int (dc_pipe, 0x8000 | (drive << 8) | (running ? 1 : 0), 1);
}

void driveclick_fdrawcmd_vsync(void)
{
/*
        int i;
        for (i = 0; i < 2; i++) {
                if (motors[i] > 0) {
                        motors[i]--;
                        if (motors[i] == 0)
                                CmdMotor(h[i], 0);
                }
        }
*/
}

static int driveclick_fdrawcmd_init(int drive)
{
/*
        static int thread_ok;

        if (h[drive] == INVALID_HANDLE_VALUE)
                return 0;
        motors[drive] = 0;
        SetDataRate(h[drive], 3);
        CmdSpecify(h[drive], 0xd, 0xf, 0x1, 0);
        SetMotorDelay(h[drive], 0);
        CmdMotor(h[drive], 0);
        if (thread_ok)
                return 1;
        thread_ok = 1;
        init_comm_pipe (dc_pipe, DC_PIPE_SIZE, 3);
        uae_start_thread ("DriveClick", driveclick_thread, NULL, NULL);
        return 1;
*/
	return 1;
}

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

// win32gfx
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
	return s;
}

char *ua_copy (char *dst, int maxlen, const char *src)
{
        dst[0] = 0;
		strncpy (dst, src, maxlen);
        return dst;
}

// win32gui
static int qs_override;

int target_cfgfile_load (struct uae_prefs *p, char *filename, int type, int isdefault)
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

// win32gfx
int screen_is_picasso = 0;
struct uae_filter *usedfilter;
uae_u32 redc[3 * 256], grec[3 * 256], bluc[3 * 256];

static int isfullscreen_2 (struct uae_prefs *p)
{
        if (screen_is_picasso)
                return p->gfx_pfullscreen == 1 ? 1 : (p->gfx_pfullscreen == 2 ? -1 : 0);
        else
                return p->gfx_afullscreen == 1 ? 1 : (p->gfx_afullscreen == 2 ? -1 : 0);
}
int isfullscreen (void)
{
        return isfullscreen_2 (&currprefs);
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
int input_get_default_lightpen (struct uae_input_device *uid, int i, int port, int af)
{
/*        struct didata *did;

        if (i >= num_mouse)
                return 0;
        did = &di_mouse[i];
        uid[i].eventid[ID_AXIS_OFFSET + 0][0] = INPUTEVENT_LIGHTPEN_HORIZ;
        uid[i].eventid[ID_AXIS_OFFSET + 1][0] = INPUTEVENT_LIGHTPEN_VERT;
        uid[i].eventid[ID_BUTTON_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_3RD_BUTTON : INPUTEVENT_JOY1_3RD_BUTTON;
        if (i == 0)
                return 1;*/
        return 0;
}

int input_get_default_joystick_analog (struct uae_input_device *uid, int i, int port, int af)
{
/*        int j;
        struct didata *did;

        if (i >= num_joystick)
                return 0;
        did = &di_joystick[i];
        uid[i].eventid[ID_AXIS_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_HORIZ_POT : INPUTEVENT_JOY1_HORIZ_POT;
        uid[i].eventid[ID_AXIS_OFFSET + 1][0] = port ? INPUTEVENT_JOY2_VERT_POT : INPUTEVENT_JOY1_VERT_POT;
        uid[i].eventid[ID_BUTTON_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_LEFT : INPUTEVENT_JOY1_LEFT;
        if (isrealbutton (did, 1))
                uid[i].eventid[ID_BUTTON_OFFSET + 1][0] = port ? INPUTEVENT_JOY2_RIGHT : INPUTEVENT_JOY1_RIGHT;
        if (isrealbutton (did, 2))
                uid[i].eventid[ID_BUTTON_OFFSET + 2][0] = port ? INPUTEVENT_JOY2_UP : INPUTEVENT_JOY1_UP;
        if (isrealbutton (did, 3))
                uid[i].eventid[ID_BUTTON_OFFSET + 3][0] = port ? INPUTEVENT_JOY2_DOWN : INPUTEVENT_JOY1_DOWN;
        for (j = 2; j < MAX_MAPPINGS - 1; j++) {
                int am = did->axismappings[j];
                if (am == DIJOFS_POV(0) || am == DIJOFS_POV(1) || am == DIJOFS_POV(2) || am == DIJOFS_POV(3)) {
                        uid[i].eventid[ID_AXIS_OFFSET + j + 0][0] = port ? INPUTEVENT_JOY2_HORIZ_POT : INPUTEVENT_JOY1_HORIZ_POT;
                        uid[i].eventid[ID_AXIS_OFFSET + j + 1][0] = port ? INPUTEVENT_JOY2_VERT_POT : INPUTEVENT_JOY1_VERT_POT;
                        j++;
                }
        }
        if (i == 0)
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
	if (p->gfx_avsync)
		p->gfx_avsyncmode = 0;

#ifdef RETROPLATFORM
	//rp_fixup_options (p);
#endif
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

void debugger_change (int mode)
{
        if (mode < 0)
                debugger_type = debugger_type == 2 ? 1 : 2;
        else
                debugger_type = mode;
        if (debugger_type != 1 && debugger_type != 2)
                debugger_type = 2;
//        regsetint (NULL, "DebuggerType", debugger_type);
        openconsole ();
}

// unicode
char *ua (const TCHAR *s)
{
	return s;
}

char *uutf8 (const char *s)
{
	return s;
}

char *utf8u (const char *s)
{
	return s;
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

// keyboard_win32

//extern int ispressed (int key);
#define MAX_KEYCODES 256
static uae_u8 di_keycodes[MAX_INPUT_DEVICES][MAX_KEYCODES];

int ispressed (int key)
{
        unsigned int i;
        for (i = 0; i < MAX_INPUT_DEVICES; i++) {
                if (di_keycodes[i][key])
                        return 1;
        }
        return 0;
}

static int specialkeycode (void)
{
        return 0; //currprefs.win32_specialkey;
}
static int specialpressed (void)
{
        return ispressed (specialkeycode ());
}

static int shiftpressed (void)
{
        return ispressed (DIK_LSHIFT) || ispressed (DIK_RSHIFT);
}

static int altpressed (void)
{
        return ispressed (DIK_LMENU) || ispressed (DIK_RMENU);
}

static int ctrlpressed (void)
{
        return ispressed (DIK_LCONTROL) || ispressed (DIK_RCONTROL);
}

static int capslockstate;
static int host_capslockstate, host_numlockstate, host_scrolllockstate;

int getcapslock (void)
{
        int capstable[7];

        // this returns bogus state if caps change when in exclusive mode..
	host_capslockstate = 1; //GetKeyState (VK_CAPITAL) & 1;
        host_numlockstate = 0; //GetKeyState (VK_NUMLOCK) & 1;
        host_scrolllockstate = 0; //GetKeyState (VK_SCROLL) & 1;
        capstable[0] = DIK_CAPITAL;
        capstable[1] = host_capslockstate;
        capstable[2] = DIK_NUMLOCK;
        capstable[3] = host_numlockstate;
        capstable[4] = DIK_SCROLL;
        capstable[5] = host_scrolllockstate;
        capstable[6] = 0;
        capslockstate = inputdevice_synccapslock (capslockstate, capstable);
        return capslockstate;
}

void clearallkeys (void)
{
        inputdevice_updateconfig (&currprefs);
}

static int np[] = {
        DIK_NUMPAD0, 0, DIK_NUMPADPERIOD, 0, DIK_NUMPAD1, 1, DIK_NUMPAD2, 2,
        DIK_NUMPAD3, 3, DIK_NUMPAD4, 4, DIK_NUMPAD5, 5, DIK_NUMPAD6, 6, DIK_NUMPAD7, 7,
        DIK_NUMPAD8, 8, DIK_NUMPAD9, 9, -1 };

void my_kbd_handler (int keyboard, int scancode, int newstate)
{
	int code = 0;
	int scancode_new;
	static int swapperdrive = 0;

	if (scancode == specialkeycode ())
		return;

#ifdef WIN32
        if (scancode == DIK_F11 && currprefs.win32_ctrl_F11_is_quit && ctrlpressed ())
                code = AKS_QUIT;
#endif

	scancode_new = scancode;
        if (!specialpressed () && inputdevice_iskeymapped (keyboard, scancode))
                scancode = 0;

#ifdef WIN32
        // GUI must be always available
        if (scancode_new == DIK_F12 && currprefs.win32_guikey < 0)
                scancode = scancode_new;
        if (scancode_new == currprefs.win32_guikey && scancode_new != DIK_F12)
                scancode = scancode_new;
#endif

	//write_log ("kbd= %d, sc_new= %d, scancode= %d (0x%02x), state= %d\n", keyboard, scancode_new, scancode, scancode, newstate);

	if (newstate == 0 && code == 0) {
		switch (scancode)
		{
                        case DIK_SYSRQ:
                        screenshot (specialpressed () ? 1 : 0, 1);
                        break;
		}
	}

        if (newstate && code == 0) {
                if (scancode == DIK_F12 /*|| scancode == currprefs.win32_guikey*/) {
                        if (ctrlpressed ()) {
                                code = AKS_TOGGLEDEFAULTSCREEN;
                        } else if (shiftpressed () || specialpressed ()) {
                                if (isfullscreen() <= 0) {
                                        //disablecapture ();
                                        code = AKS_ENTERDEBUGGER;
                                }
                        } else {
                                code = AKS_ENTERGUI;
                        }
                }

                switch (scancode)
                {
                case DIK_F1:
                case DIK_F2:
                case DIK_F3:
                case DIK_F4:
                        if (specialpressed ()) {
                                if (ctrlpressed ()) {
                                } else {
                                        if (shiftpressed ())
                                                code = AKS_EFLOPPY0 + (scancode - DIK_F1);
                                        else
                                                code = AKS_FLOPPY0 + (scancode - DIK_F1);
                                }
                        }
                        break;
                case DIK_F5:
                        if (specialpressed ()) {
                                if (shiftpressed ())
                                        code = AKS_STATESAVEDIALOG;
                                else
                                        code = AKS_STATERESTOREDIALOG;
                        }
                        break;
                case DIK_1:
                case DIK_2:
                case DIK_3:
                case DIK_4:
                case DIK_5:
                case DIK_6:
                case DIK_7:
                case DIK_8:
                case DIK_9:
                case DIK_0:
                        if (specialpressed ()) {
                                int num = scancode - DIK_1;
                                if (shiftpressed ())
                                        num += 10;
                                if (ctrlpressed ()) {
                                       swapperdrive = num;
                                        if (swapperdrive > 3)
                                                swapperdrive = 0;
                                } else {
                                        int i;
                                        for (i = 0; i < 4; i++) {
                                                if (!_tcscmp (currprefs.floppyslots[i].df, currprefs.dfxlist[num]))
                                                        changed_prefs.floppyslots[i].df[0] = 0;
                                        }
                                        _tcscpy (changed_prefs.floppyslots[swapperdrive].df, currprefs.dfxlist[num]);
                                        config_changed = 1;
                                }
                        }
                        break;
                case DIK_NUMPAD0:
                case DIK_NUMPAD1:
                case DIK_NUMPAD2:
                case DIK_NUMPAD3:
                case DIK_NUMPAD4:
                case DIK_NUMPAD5:
                case DIK_NUMPAD6:
                case DIK_NUMPAD7:
                case DIK_NUMPAD8:
                case DIK_NUMPAD9:
                case DIK_NUMPADPERIOD:
                        if (specialpressed ()) {
                                int i = 0, v = -1;
                                while (np[i] >= 0) {
                                        v = np[i + 1];
                                        if (np[i] == scancode)
                                                break;
                                        i += 2;
                                }
                                if (v >= 0)
                                        code = AKS_STATESAVEQUICK + v * 2 + ((shiftpressed () || ctrlpressed ()) ? 0 : 1);
                        }
                        break;
                case DIK_PAUSE:
                        if (specialpressed ()) {
                                if (shiftpressed ())
                                        code = AKS_IRQ7;
                                else
                                        code = AKS_WARP;
                        } else {
                                code = AKS_PAUSE;
                        }
                        break;
                case DIK_SCROLL:
                        code = AKS_INHIBITSCREEN;
                        break;
                case DIK_NUMPADMINUS:
                        if (specialpressed ()) {
                                if (shiftpressed ())
                                        code = AKS_DECREASEREFRESHRATE;
                                else if (ctrlpressed ())
                                        code = AKS_MVOLDOWN;
                                else
                                        code = AKS_VOLDOWN;
                        }
                        break;
                case DIK_NUMPADPLUS:
                        if (specialpressed ()) {
                                if (shiftpressed ())
                                        code = AKS_INCREASEREFRESHRATE;
                                else if (ctrlpressed ())
                                        code = AKS_MVOLUP;
                                else
                                        code = AKS_VOLUP;
                        }
                        break;
                case DIK_NUMPADSTAR:
                        if (specialpressed ()) {
                                if (ctrlpressed ())
                                        code = AKS_MVOLMUTE;
                                else
                                        code = AKS_VOLMUTE;
                        }
                        break;
                case DIK_NUMPADSLASH:
                        if (specialpressed ())
                                code = AKS_STATEREWIND;
                        break;
                }
        }

        if (code) {
                inputdevice_add_inputcode (code, 1);
                return;
        }

        scancode = scancode_new;
        if (!specialpressed () && newstate) {
                if (scancode == DIK_CAPITAL) {
                        host_capslockstate = host_capslockstate ? 0 : 1;
                        capslockstate = host_capslockstate;
                }
                if (scancode == DIK_NUMLOCK) {
                        host_numlockstate = host_numlockstate ? 0 : 1;
                        capslockstate = host_numlockstate;
                }
                if (scancode == DIK_SCROLL) {
                        host_scrolllockstate = host_scrolllockstate ? 0 : 1;
                        capslockstate = host_scrolllockstate;
                }
        }
        if (specialpressed ())
                return;

//        write_log ("kbd2 = %d, scancode = %d (0x%02x), state = %d\n", keyboard, scancode, scancode, newstate);

        inputdevice_translatekeycode (keyboard, scancode, newstate);
}

// win32gfx
#define MAX_DISPLAYS 10
struct MultiDisplay Displays[MAX_DISPLAYS];

struct MultiDisplay *getdisplay (struct uae_prefs *p)
{
        int i;
        int display = p->gfx_display;

        write_log ("Multimonitor detection disabled\n");
        Displays[0].primary = 1;
    	Displays[0].name = "Display";
		Displays[0].disabled = 0;

        i = 0;
        while (Displays[i].name) {
                struct MultiDisplay *md = &Displays[i];
                if (p->gfx_display_name[0] && !_tcscmp (md->name, p->gfx_display_name))
                        return md;
                if (p->gfx_display_name[0] && !_tcscmp (md->name2, p->gfx_display_name))
                        return md;
                i++;
        }

        if (i == 0) {
                write_log ("no display adapters! Exiting");
                exit (0);
        }
        if (display >= i)
                display = 0;
        return &Displays[display];
}

void addmode (struct MultiDisplay *md, int w, int h, int d, int rate, int nondx)
{
        int ct;
        int i, j;

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
                                if (md->DisplayModes[i].refresh[j] == 0 || md->DisplayModes[i].refresh[j] == rate)
                                        break;
                        }
                        if (j < MAX_REFRESH_RATES) {
                                md->DisplayModes[i].refresh[j] = rate;
                                md->DisplayModes[i].refreshtype[j] = nondx;
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
        md->DisplayModes[i].nondx = nondx;
        md->DisplayModes[i].res.width = w;
        md->DisplayModes[i].res.height = h;
        md->DisplayModes[i].depth = d;
        md->DisplayModes[i].refresh[0] = rate;
        md->DisplayModes[i].refreshtype[0] = nondx;
        md->DisplayModes[i].refresh[1] = 0;
        md->DisplayModes[i].colormodes = ct;
        md->DisplayModes[i + 1].depth = -1;
        _stprintf (md->DisplayModes[i].name, "%dx%d, %d-bit",
                md->DisplayModes[i].res.width, md->DisplayModes[i].res.height, md->DisplayModes[i].depth * 8);
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

//win32gfx.cpp
static bool render_ok;

bool render_screen (void)
{
	bool v = false;
	render_ok = false;
//
	render_ok = v;
	return render_ok;
}

void show_screen (void)
{
	if (!render_ok)
		return;
	//
}

double vblank_calibrate (double approx_vblank, bool waitonly)
{
  frame_time_t t1, t2;
  double tsum, tsum2, tval, tfirst;
  int maxcnt, maxtotal, total, cnt, tcnt2;
  
  if (remembered_vblank > 0)
    return remembered_vblank;
  if (waitonly) {
    vblankbasefull = syncbase / approx_vblank;
    vblankbasewait = (syncbase / approx_vblank) * 3 / 4;
    remembered_vblank = -1;
    return -1;
  }
/*
  th = GetCurrentThread ();
  int oldpri = GetThreadPriority (th);
  SetThreadPriority (th, THREAD_PRIORITY_HIGHEST);
  dummythread_die = -1;
  dummy_counter = 0;
  _beginthread (&dummythread, 0, 0);
  sleep_millis (100);
 maxtotal = 10;
 maxcnt = maxtotal;
  tsum2 = 0;
  tcnt2 = 0;
  for (maxcnt = 0; maxcnt < maxtotal; maxcnt++) {
    total = 10;
    tsum = 0;
    cnt = total;
    for (cnt = 0; cnt < total; cnt++) {
      if (!waitvblankstate (true))
        return -1;
      if (!waitvblankstate (false))
        return -1;
      if (!waitvblankstate (true))
        return -1;
      t1 = read_processor_time ();
      if (!waitvblankstate (false))
        return -1;
      if (!waitvblankstate (true))
        return -1;
      t2 = read_processor_time ();
      tval = (double)syncbase / (t2 - t1);
      if (cnt == 0)
        tfirst = tval;
      if (abs (tval - tfirst) > 1) {
        write_log ("very unstable vsync! %.6f vs %.6f, retrying..\n", tval, tfirst);
        break;
      }
      tsum2 += tval;
      tcnt2++;
      if (abs (tval - tfirst) > 0.1) {
        write_log ("unstable vsync! %.6f vs %.6f\n", tval, tfirst);
        break;
      }
      tsum += tval;
    }
    if (cnt >= total)
      break;
  }
  dummythread_die = 0;
  SetThreadPriority (th, oldpri);
  if (maxcnt >= maxtotal) {
    tsum = tsum2 / tcnt2;
    write_log ("unstable vsync reporting, using average value\n");
  } else {
    tsum /= total;
  }
  if (tsum >= 85)
    tsum /= 2;
  vblankbasefull = (syncbase / tsum);
  vblankbasewait = (syncbase / tsum) * 3 / 4;
  write_log ("VSync calibration: %.6fHz\n", tsum);
  remembered_vblank = tsum;
  return tsum;
*/
 return -1;

}

bool vsync_busywait (void)
{
  bool v;
  static frame_time_t prevtime;
  static bool framelost;

  if (currprefs.turbo_emulation)
    return true;

  if (!framelost && uae_gethrtime () - prevtime > vblankbasefull) {
    framelost = true;
    prevtime = uae_gethrtime ();
    return true;
  }
  if (framelost) {
    framelost = false;
    prevtime = uae_gethrtime ();
    return true;
  }

  while (uae_gethrtime () - prevtime < vblankbasewait)
    uae_msleep (1);
  v = false;
/*
  if (currprefs.gfx_api) {
    v = D3D_vblank_busywait ();
  } else {
    v = DirectDraw_vblank_busywait ();
  }
*/
  if (v) {
    prevtime = uae_gethrtime ();
    return true;
  }
  return false;
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

// parser.c

void serialuartbreak (int v)
{
        if (/*hCom == INVALID_HANDLE_VALUE ||*/ !currprefs.use_serial)
                return;

/*        if (v)
                EscapeCommFunction (hCom, SETBREAK);
        else
                EscapeCommFunction (hCom, CLRBREAK);
*/
}
