/*
 * UAE - The Un*x Amiga Emulator
 *
 * Win32 interface
 *
 * Copyright 1997-1998 Mathias Ortmann
 * Copyright 1997-1999 Brian King
 */

/* Uncomment this line if you want the logs time-stamped */
/* #define TIMESTAMP_LOGS */

#include "config.h"
#include "sysconfig.h"

#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <zmouse.h>
#include <ddraw.h>
#include <dbt.h>
#include <math.h>
#include <mmsystem.h>

#include "sysdeps.h"
#include "options.h"
#include "sound.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "events.h"
#include "xwin.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "keybuf.h"
#include "drawing.h"
#include "dxwrap.h"
#include "picasso96.h"
#include "bsdsocket.h"
#include "win32.h"
#include "win32gfx.h"
#include "win32gui.h"
#include "autoconf.h"
#include "gui.h"
#include "newcpu.h"
#include "sys/mman.h"
#include "avioutput.h"
#include "ahidsound.h"
#include "zfile.h"
#include "savestate.h"

extern void WIN32GFX_WindowMove ( void );
extern void WIN32GFX_WindowSize ( void );
unsigned long *win32_stackbase; 
unsigned long *win32_freestack[42]; //EXTRA_STACK_SIZE

extern FILE *debugfile;
extern int console_logging;
static OSVERSIONINFO osVersion;

int useqpc = 0; /* Set to TRUE to use the QueryPerformanceCounter() function instead of rdtsc() */
int cpu_mmx = 0;

HINSTANCE hInst = NULL;
HMODULE hUIDLL = NULL;

HWND (WINAPI *pHtmlHelp)(HWND, LPCSTR, UINT, LPDWORD ) = NULL;

HWND hAmigaWnd, hMainWnd;
RECT amigawin_rect;

char VersionStr[256];

int in_sizemove = 0;
int manual_painting_needed = 0;
int win_x_diff = 0, win_y_diff = 0;

int toggle_sound;

HKEY hWinUAEKey    = NULL;
COLORREF g_dwBackgroundColor  = RGB(10, 0, 10);

static int emulation_paused;
static int activatemouse = 1;
static int ignore_messages_all;
int pause_emulation;

static int didmousepos;
int mouseactive, focus;

static int mm_timerres;
static int timermode, timeon;
static HANDLE timehandle;

static int timeend (void)
{
    if (!timeon)
	return 1;
    timeon = 0;
    if (timeEndPeriod (mm_timerres) == TIMERR_NOERROR)
	return 1;
    write_log ("TimeEndPeriod() failed\n");
    return 0;
}

static int timebegin (void)
{
    if (timeon) {
	timeend();
	return timebegin();
    }
    timeon = 0;
    if (timeBeginPeriod (mm_timerres) == TIMERR_NOERROR) {
	timeon = 1;
	return 1;
    }
    write_log ("TimeBeginPeriod() failed\n");
    return 0;
}

static void init_mmtimer (void)
{
    TIMECAPS tc;
    mm_timerres = 0;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	return;
    mm_timerres = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
    timehandle = CreateEvent (NULL, TRUE, FALSE, NULL);
}

void sleep_millis (int ms)
{
    UINT TimerEvent;

    if (mm_timerres <= 0 || timermode) {
	Sleep (ms);
	return;
    }
    TimerEvent = timeSetEvent (ms, 0, timehandle, 0, TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
    if (!TimerEvent) {
	Sleep (ms);
    } else {
	WaitForSingleObject (timehandle, ms);
        ResetEvent (timehandle);
	timeKillEvent (TimerEvent);
    }
}

void sleep_millis_busy (int ms)
{
    if (timermode < 0)
	return;
    sleep_millis (ms);
}

#include <process.h>
static volatile int dummythread_die;
static void dummythread (void *dummy)
{
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    while (!dummythread_die);
}

static uae_u64 win32_read_processor_time (void)
{
    uae_u32 foo, bar;
     __asm
    {
        rdtsc
        mov foo, eax
        mov bar, edx
    }
    return ((uae_u64)bar << 32) | foo;
}

static int figure_processor_speed (void)
{
    extern volatile frame_time_t vsynctime;
    extern unsigned long syncbase;
    uae_u64 clockrate, clockrateidle, qpfrate, ratea1, ratea2;
    uae_u32 rate1, rate2;
    double limit, clkdiv = 1, clockrate1000 = 0;
    int i, ratecnt = 6;
    LARGE_INTEGER freq;
    int qpc_avail = 0;
    int mmx = 0; 

    rpt_available = 1;
    __try
    {
	__asm 
	{
	    rdtsc
	}
    } __except( GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ) {
	rpt_available = 0;
	write_log ("CLOCKFREQ: RDTSC not supported\n");
    }
    __try
    {
	__asm 
	{
	    mov eax,1
	    cpuid
	    and edx,0x800000
	    mov mmx,edx
	}
	if (mmx)
	    cpu_mmx = 1;
    } __except( GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ) {
    }

    if (QueryPerformanceFrequency(&freq)) {
	qpc_avail = 1;
	write_log("CLOCKFREQ: QPF %.2fMHz\n", freq.QuadPart / 1000000.0);
	qpfrate = freq.QuadPart;
	 /* we don't want 32-bit overflow */
	if (qpfrate > 100000000) {
	    qpfrate >>= 6;
	    qpc_avail = -1;
	}
    } else {
	write_log("CLOCKREQ: QPF not supported\n");
    }

    if (!rpt_available && !qpc_avail) {
	pre_gui_message ("No timing reference found\n(no RDTSC or QPF support detected)\nWinUAE will exit\n");
	return 0;
    }

    init_mmtimer();
    SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    sleep_millis (100);
    dummythread_die = -1;

    if (qpc_avail || rpt_available)  {

	if (rpt_available) {
	    clockrateidle = win32_read_processor_time();
	    sleep_millis (500);
	    clockrateidle = (win32_read_processor_time() - clockrateidle) * 2;
	    dummythread_die = 0;
	    _beginthread(&dummythread, 0, 0);
	    sleep_millis (100);
	    clockrate = win32_read_processor_time();
	    sleep_millis (500);
	    clockrate = (win32_read_processor_time() - clockrate) * 2;
	    write_log("CLOCKFREQ: RDTSC %.2fMHz (busy) / %.2fMHz (idle)\n",
		clockrate / 1000000.0, clockrateidle / 1000000.0);
	    clkdiv = (double)clockrate / (double)clockrateidle;
	    clockrate >>= 6;
	    clockrate1000 = clockrate / 1000.0;
	}
	if (clkdiv <= 0.95 || clkdiv >= 1.05 || !rpt_available) {
	    if (rpt_available)
		write_log ("CLOCKFREQ: CPU throttling detected, using QPF instead of RDTSC\n");
	    useqpc = qpc_avail;
	    rpt_available = 1;
	    clkdiv = 1.0;
	    clockrate = qpfrate;
	    clockrate1000 = clockrate / 1000.0;
	    if (dummythread_die < 0) {
	        dummythread_die = 0;
		_beginthread(&dummythread, 0, 0);
	    }
	    if (!qpc_avail)
		write_log ("No working timing reference detected\n");
	}
	timermode = 0;
	if (mm_timerres) {
	    sleep_millis (50);
	    timebegin ();
	    sleep_millis (50);
	    ratea1 = 0;
	    write_log ("Testing MM-timer resolution:\n");
	    for (i = 0; i < ratecnt; i++) {
		rate1 = read_processor_time();
		sleep_millis (1);
		rate1 = read_processor_time() - rate1;
		write_log ("%1.2fms ", rate1 / clockrate1000);
		ratea1 += rate1;
	    }
	    write_log("\n");
	    timeend ();
	    sleep_millis (50);
	}
	timermode = 1;
	ratea2 = 0;
	write_log ("Testing Sleep() resolution:\n");
	for (i = 0; i < ratecnt; i++) {
	    rate2 = read_processor_time();
	    sleep_millis (1);
	    rate2 = read_processor_time() - rate2;
	    write_log ("%1.2fms ", rate2 / clockrate1000);
	    ratea2 += rate2;
	}
	write_log("\n");
    }

    SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    dummythread_die = 1;
   
    if (clkdiv >= 0.90 && clkdiv <= 1.10 && rpt_available) {
	limit = 2.5;
	if ((ratea2 / ratecnt) < limit * clockrate1000) { /* regular Sleep() is ok */
	    timermode = 1;
	    write_log ("Using Sleep() (resolution < %.1fms)\n", limit);
	} else if (mm_timerres && (ratea1 / ratecnt) < limit * clockrate1000) { /* MM-timer is ok */
	    timermode = 0;
	    timebegin ();
	    write_log ("Using MultiMedia timers (resolution < %.1fms)\n", limit);
	} else {
	    timermode = -1; /* both timers are bad, fall back to busy-wait */
	    write_log ("falling back to busy-loop waiting (timer resolution > %.1fms)\n", limit);
	}
    } else {
	timermode = -1;
	write_log ("forcing busy-loop wait mode\n");
    }
    syncbase = (unsigned long)clockrate;
    return 1;
}

static void setcursor(int oldx, int oldy)
{
    int x = (amigawin_rect.right - amigawin_rect.left) / 2;
    int y = (amigawin_rect.bottom - amigawin_rect.top) / 2;
    if (oldx == x && oldy == y)
	return;
    SetCursorPos (amigawin_rect.left + x, amigawin_rect.top + y);
}

void setmouseactive (int active)
{
    int oldactive = mouseactive;
    static int mousecapture, showcursor;

    if (active > 0 && ievent_alive > 0) {
	mousehack_set (mousehack_follow);
	return;
    }
    mousehack_set (mousehack_dontcare);
    inputdevice_unacquire ();
    mouseactive = active;
    if (mouseactive > 0) {
	focus = 1;
        if( currprefs.win32_middle_mouse )
	    SetWindowText (hMainWnd, "WinUAE - [Mouse active - press Alt-Tab or middle-button to cancel]");
	else
    	    SetWindowText (hMainWnd, "WinUAE - [Mouse active - press Alt-Tab to cancel]");
    } else {
	SetWindowText (hMainWnd, "WinUAE" );
    }
    if (mousecapture) {
	ClipCursor (0);
	ReleaseCapture ();
	mousecapture = 0;
    }
    if (showcursor) {
	ShowCursor (TRUE);
	showcursor = 0;
    }
    if (mouseactive) {
	if (focus) {
	    if (!showcursor)
		ShowCursor (FALSE);
	    showcursor = 1;
	    if (!isfullscreen()) {
		if (!mousecapture) {
		    SetCapture (hAmigaWnd);
		    ClipCursor (&amigawin_rect);
		}
		mousecapture = 1;
	    }
	    setcursor (-1, -1);
	}
	inputdevice_acquire (mouseactive);
    }
}

#ifndef AVIOUTPUT
static int avioutput_video = 0;
#endif

static void winuae_active (HWND hWnd, int minimized)
{
    int ot, pri;
    
    /* without this returning from hibernate-mode causes wrong timing
     */
    ot = timermode;
    timermode = 0;
    timebegin();
    sleep_millis (2);
    timermode = ot;
    if (timermode != 0)
	timeend();  

    focus = 1;
    write_log( "WinUAE now active via WM_ACTIVATE\n" );
    pri = THREAD_PRIORITY_NORMAL;
#ifndef _DEBUG
    if (!minimized)
	pri = priorities[currprefs.win32_activepriority].value;
#endif
    SetThreadPriority ( GetCurrentThread(), pri);
    write_log ("priority set to %d\n", pri);

    if (!minimized) {
        if (!avioutput_video) {
	    clear_inhibit_frame( IHF_WINDOWHIDDEN );
	}
    }
    if (emulation_paused > 0)
	emulation_paused = -1;
    ShowWindow (hWnd, SW_RESTORE);
#ifdef AHI
    ahi_close_sound ();
#endif
    close_sound ();
#ifdef AHI
    ahi_open_sound ();
#endif
    init_sound ();
    if (WIN32GFX_IsPicassoScreen ())
	WIN32GFX_EnablePicasso();
    getcapslock ();
    inputdevice_acquire (mouseactive);
    wait_keyrelease ();
    inputdevice_acquire (mouseactive);
    if (isfullscreen())
	setmouseactive (1);
}

static void winuae_inactive (HWND hWnd, int minimized)
{
    focus = 0;
    write_log( "WinUAE now inactive via WM_ACTIVATE\n" );
    wait_keyrelease ();
    setmouseactive (0);
    close_sound ();
#ifdef AHI
    ahi_close_sound ();
#endif
    init_sound ();
#ifdef AHI
    ahi_open_sound ();
#endif
    SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    if (minimized && !quit_program) {
	if( currprefs.win32_iconified_nospeed )
	    SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_IDLE);
        if (currprefs.win32_iconified_nosound) {
	    close_sound ();
#ifdef AHI
	    ahi_close_sound ();
#endif
	}
	if (!avioutput_video) {
	    set_inhibit_frame( IHF_WINDOWHIDDEN );
	}
	if (currprefs.win32_iconified_pause) {
	    close_sound ();
#ifdef AHI
	    ahi_close_sound ();
#endif
	    emulation_paused = 1;
	}
    }
    getcapslock ();
}

void minimizewindow (void)
{
    ShowWindow (hMainWnd, SW_MINIMIZE);
}

void disablecapture (void)
{
    setmouseactive (0);
    close_sound ();
#ifdef AHI
    ahi_close_sound ();
#endif
}

static long FAR PASCAL AmigaWindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hDC;
    BOOL minimized;
    LPMINMAXINFO lpmmi;
    RECT rect;
    int mx, my;
    static int mm;

    if (ignore_messages_all)
	return DefWindowProc (hWnd, message, wParam, lParam);

    if (hMainWnd == 0)
	hMainWnd = hWnd;

    switch( message ) 
    {
    case WM_ACTIVATE:
    	minimized = HIWORD( wParam );
	if (LOWORD (wParam) != WA_INACTIVE)
      	    winuae_active (hWnd, minimized);
        else
	    winuae_inactive (hWnd, minimized);
    break;

    case WM_ACTIVATEAPP:
	if (!wParam)
	    setmouseactive (0);
    break;

    case WM_PALETTECHANGED:
        if( (HWND)wParam != hWnd ) {
	    write_log( "WM_PALETTECHANGED Request\n" );
	    WIN32GFX_PaletteChange();
	}
    break;

    case WM_LBUTTONDOWN:
	if (!mouseactive && !isfullscreen()) {
	    setmouseactive (1);
	}
    break;

    case WM_PAINT:
	notice_screen_contents_lost ();
        hDC = BeginPaint (hWnd, &ps);
        /* Check to see if this WM_PAINT is coming while we've got the GUI visible */
        if (manual_painting_needed)
	    updatedisplayarea ();
	EndPaint (hWnd, &ps);
    break;

    case WM_DROPFILES:
	if (DragQueryFile ((HDROP) wParam, (UINT) - 1, NULL, 0)) {
	    if (DragQueryFile ((HDROP) wParam, 0, NULL, 0) < 255)
		DragQueryFile ((HDROP) wParam, 0, changed_prefs.df[0], sizeof (changed_prefs.df[0]));
	}
	DragFinish ((HDROP) wParam);
    break;

    case WM_TIMER:
#ifdef PARALLEL_PORT
	finishjob ();
#endif
    break;

    case WM_CREATE:
	DragAcceptFiles (hWnd, TRUE);
    break;

    case WM_CLOSE:
	if( !currprefs.win32_ctrl_F11_is_quit )
	    uae_quit ();
    return 0;

    case WM_WINDOWPOSCHANGED:
	if( !isfullscreen())
	    GetWindowRect( hWnd, &amigawin_rect);
    break;

    case WM_MOUSEMOVE:
        mx = (signed short) LOWORD (lParam);
        my = (signed short) HIWORD (lParam);
        if (!mouseactive && !isfullscreen()) {
	    setmousestate (0, 0, mx, 1);
	    setmousestate (0, 1, my, 1);
	} else {
#if 0
	    int mxx = (amigawin_rect.right - amigawin_rect.left) / 2;
	    int myy = (amigawin_rect.bottom - amigawin_rect.top) / 2;
	    mx = mx - mxx;
	    my = my - myy;
	    setmousestate (0, 0, mx, 0);
	    setmousestate (0, 1, my, 0);
#endif
	}
	if (mouseactive && !isfullscreen()) {
	    setcursor (LOWORD (lParam), HIWORD (lParam));
	}
    break;

    case WM_MOVING:
    case WM_MOVE:
	WIN32GFX_WindowMove();
    return TRUE;

    case WM_SIZING:
	WIN32GFX_WindowSize();
    return TRUE;

    case WM_SIZE:
	WIN32GFX_WindowSize();
    return 0;

    case WM_GETMINMAXINFO:
	rect.left=0;
	rect.top=0;
	lpmmi=(LPMINMAXINFO)lParam;
	rect.right=320;
	rect.bottom=256;
	//AdjustWindowRectEx(&rect,WSTYLE,0,0);
	lpmmi->ptMinTrackSize.x=rect.right-rect.left;
	lpmmi->ptMinTrackSize.y=rect.bottom-rect.top;
    return 0;

#ifdef FILESYS
    case WM_DEVICECHANGE:
    {
	extern void win32_spti_media_change (char driveletter, int insert);
	extern void win32_ioctl_media_change (char driveletter, int insert);
	extern void win32_aspi_media_change (char driveletter, int insert);
	DEV_BROADCAST_HDR *pBHdr = (DEV_BROADCAST_HDR *)lParam;
	if( pBHdr && ( pBHdr->dbch_devicetype == DBT_DEVTYP_VOLUME ) ) {
	    DEV_BROADCAST_VOLUME *pBVol = (DEV_BROADCAST_VOLUME *)lParam;
	    if( pBVol->dbcv_flags & DBTF_MEDIA ) {
		if( pBVol->dbcv_unitmask ) {
		    int inserted, i;
		    char drive;
		    for (i = 0; i <= 'Z'-'A'; i++) {
			if (pBVol->dbcv_unitmask & (1 << i)) {
			    drive = 'A' + i;
			    inserted = -1;
			    if (wParam == DBT_DEVICEARRIVAL)
				inserted = 1;
			    else if (wParam == DBT_DEVICEREMOVECOMPLETE)
				inserted = 0;
	#ifdef WINDDK
			    win32_spti_media_change (drive, inserted);
			    win32_ioctl_media_change (drive, inserted);
	#endif
			    win32_aspi_media_change (drive, inserted);
			}
		    }
		}
	    }
	}
    }
#endif
    return TRUE;

    case WM_SYSCOMMAND:
	if (!manual_painting_needed && focus) {
	    switch (wParam) // Check System Calls
	    {
		case SC_SCREENSAVE: // Screensaver Trying To Start?
		case SC_MONITORPOWER: // Monitor Trying To Enter Powersave?
		return 0; // Prevent From Happening
	    }
	}
    break;

    //case WM_INPUT:
    //handle_rawinput (lParam);
    //return 0;
    
    default:
    break;
    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}

static long FAR PASCAL MainWindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC hDC;

    switch (message) {

     case WM_MOUSEMOVE:
     case WM_ACTIVATEAPP:
     case WM_DROPFILES:
     case WM_ACTIVATE:
     case WM_SETCURSOR:
     case WM_SYSCOMMAND:
     case WM_KEYUP:
     case WM_SYSKEYUP:
     case WM_KEYDOWN:
     case WM_SYSKEYDOWN:
     case WM_LBUTTONDOWN:
     case WM_LBUTTONUP:
     case WM_MBUTTONDOWN:
     case WM_MBUTTONUP:
     case WM_RBUTTONDOWN:
     case WM_RBUTTONUP:
     case WM_MOVING:
     case WM_MOVE:
     case WM_SIZING:
     case WM_SIZE:
     case WM_GETMINMAXINFO:
     case WM_CREATE:
     case WM_DESTROY:
     case WM_CLOSE:
     case WM_HELP:
     case WM_DEVICECHANGE:
	return AmigaWindowProc (hWnd, message, wParam, lParam);

     case WM_DISPLAYCHANGE:
	if (!isfullscreen() && !currprefs.gfx_filter && (wParam + 7) / 8 != DirectDraw_GetBytesPerPixel() )
	    WIN32GFX_DisplayChangeRequested();
	break;

     case WM_ENTERSIZEMOVE:
	in_sizemove++;
	break;

     case WM_EXITSIZEMOVE:
	in_sizemove--;
	/* fall through */

     case WM_WINDOWPOSCHANGED:
	WIN32GFX_WindowMove();
	if( hAmigaWnd && GetWindowRect (hAmigaWnd, &amigawin_rect) )
	{
	    if (in_sizemove > 0)
		break;

	    if( !isfullscreen() && hAmigaWnd )
	    {
	        static int store_xy;
	        RECT rc2;
		if( GetWindowRect( hMainWnd, &rc2 )) {
		    if (amigawin_rect.left & 3)
		    {
			MoveWindow (hMainWnd, rc2.left+ 4 - amigawin_rect.left % 4, rc2.top,
				    rc2.right - rc2.left, rc2.bottom - rc2.top, TRUE);

		    }
		    if( hWinUAEKey && store_xy++)
		    {
			DWORD left = rc2.left - win_x_diff;
			DWORD top = rc2.top - win_y_diff;
			RegSetValueEx( hWinUAEKey, "xPos", 0, REG_DWORD, (LPBYTE)&left, sizeof( LONG ) );
			RegSetValueEx( hWinUAEKey, "yPos", 0, REG_DWORD, (LPBYTE)&top, sizeof( LONG ) );
		    }
		}
		return 0;
	    }
	}
	break;

     case WM_PAINT:
	hDC = BeginPaint (hWnd, &ps);
	GetClientRect (hWnd, &rc);
	DrawEdge (hDC, &rc, EDGE_SUNKEN, BF_RECT);
	EndPaint (hWnd, &ps);
	break;
     case WM_NCLBUTTONDBLCLK:
	if (wParam == HTCAPTION) {
	    WIN32GFX_ToggleFullScreen();
	    return 0;
	}
	break;
    default:
    break;

    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}

void handle_events (void)
{
    MSG msg;
    int was_paused = 0;

    while (emulation_paused > 0 || pause_emulation) {
	if ((emulation_paused > 0 || pause_emulation) && was_paused == 0) {
	    close_sound ();
#ifdef AHI
	    ahi_close_sound ();
#endif
	    was_paused = 1;
	    manual_painting_needed++;
	}
	if (PeekMessage (&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage (&msg);
	    DispatchMessage (&msg);
	}
	sleep_millis (50);
	inputdevicefunc_keyboard.read();
    }
    while (PeekMessage (&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
    if (was_paused) {
        // This is a hack to fix the fact that time is passing while the GUI was present,
	// and we don't want our frames-per-second calculation in drawing.c to get skewed.
#ifdef HAVE_GETTIMEOFDAY
	extern unsigned long int msecs, seconds_base;
	struct timeval tv;
	gettimeofday(&tv,NULL);
	msecs = (tv.tv_sec-seconds_base) * 1000 + tv.tv_usec / 1000;
#endif
	init_sound ();
#ifdef AHI
        ahi_open_sound ();
#endif
	emulation_paused = 0;
	manual_painting_needed--;
    }

}

/* We're not a console-app anymore! */
void setup_brkhandler (void)
{
}
void remove_brkhandler (void)
{
}

int WIN32_RegisterClasses( void )
{
    WNDCLASS wc;
    HDC hDC = GetDC( NULL ); 

    if( GetDeviceCaps( hDC, NUMCOLORS ) != -1 ) 
        g_dwBackgroundColor = RGB( 255, 0, 255 );    
    ReleaseDC( NULL, hDC );

    wc.style = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_DBLCLKS | CS_OWNDC;
    wc.lpfnWndProc = AmigaWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = 0;
    wc.hIcon = LoadIcon (GetModuleHandle (NULL), MAKEINTRESOURCE( IDI_APPICON ) );
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "AmigaPowah";
    wc.hbrBackground = CreateSolidBrush( g_dwBackgroundColor ); 
    if (!RegisterClass (&wc))
	return 0;

    wc.style = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = 0;
    wc.hIcon = LoadIcon (GetModuleHandle (NULL), MAKEINTRESOURCE( IDI_APPICON ) );
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush( g_dwBackgroundColor ); 
    wc.lpszMenuName = 0;
    wc.lpszClassName = "PCsuxRox";
    if (!RegisterClass (&wc))
	return 0;
    return 1;
}

#ifdef __GNUC__
#undef WINAPI
#define WINAPI
#endif

static HINSTANCE hRichEdit = NULL, hHtmlHelp = NULL;

int WIN32_CleanupLibraries( void )
{
    if (hRichEdit)
	FreeLibrary (hRichEdit);
    
    if( hHtmlHelp )
        FreeLibrary( hHtmlHelp );

    if( hUIDLL )
	FreeLibrary( hUIDLL );

    return 1;
}

/* HtmlHelp Initialization - optional component */
int WIN32_InitHtmlHelp( void )
{
    int result = 0;
    if( hHtmlHelp = LoadLibrary( "HHCTRL.OCX" ) )
    {
        pHtmlHelp = ( HWND(WINAPI *)(HWND, LPCSTR, UINT, LPDWORD ) )GetProcAddress( hHtmlHelp, "HtmlHelpA" );
        result = 1;
    }


    return result;
}

#if 0
#define TESTING_LANGUAGES
#define TEST_LANGID LANG_GERMAN
//#define TEST_LANGID LANG_FRENCH
//#define TEST_LANGID LANG_TURKISH
#endif

static HMODULE LoadGUI( void )
{
    HMODULE result = NULL;
    LPCTSTR dllname = NULL;
    LANGID language = GetUserDefaultLangID() & 0x3FF; // low 9-bits form the primary-language ID
#ifdef TESTING_LANGUAGES
    language = TEST_LANGID;
#endif

    switch( language )
    {
    case LANG_AFRIKAANS:
	dllname = "WinUAE_Afrikaans.dll";
	break;
    case LANG_ARABIC:
	dllname = "WinUAE_Arabic.dll";
	break;
    case LANG_ARMENIAN:
	dllname = "WinUAE_Armenian.dll";
	break;
    case LANG_ASSAMESE:
	dllname = "WinUAE_Assamese.dll";
	break;
    case LANG_AZERI:
	dllname = "WinUAE_Azeri.dll";
	break;
    case LANG_BASQUE:
	dllname = "WinUAE_Basque.dll";
	break;
    case LANG_BELARUSIAN:
	dllname = "WinUAE_Belarusian.dll";
	break;
    case LANG_BENGALI:
	dllname = "WinUAE_Bengali.dll";
	break;
    case LANG_BULGARIAN:
	dllname = "WinUAE_Bulgarian.dll";
	break;
    case LANG_CATALAN:
	dllname = "WinUAE_Catalan.dll";
	break;
    case LANG_CHINESE:
	dllname = "WinUAE_Chinese.dll";
	break;
    case LANG_CROATIAN:
	dllname = "WinUAE_CroatianSerbian.dll";
	break;
    case LANG_CZECH:
	dllname = "WinUAE_Czech.dll";
	break;
    case LANG_DANISH:
	dllname = "WinUAE_Danish.dll";
	break;
    case LANG_DUTCH:
	dllname = "WinUAE_Dutch.dll";
	break;
    case LANG_ESTONIAN:
	dllname = "WinUAE_Estonian.dll";
	break;
    case LANG_FAEROESE:
	dllname = "WinUAE_Faeroese.dll";
	break;
    case LANG_FARSI:
	dllname = "WinUAE_Farsi.dll";
	break;
    case LANG_FINNISH:
	dllname = "WinUAE_Finnish.dll";
	break;
    case LANG_FRENCH:
	dllname = "WinUAE_French.dll";
	break;
    case LANG_GEORGIAN:
	dllname = "WinUAE_Georgian.dll";
	break;
    case LANG_GERMAN:
	dllname = "WinUAE_German.dll";
	break;
    case LANG_GREEK:
	dllname = "WinUAE_Greek.dll";
	break;
    case LANG_GUJARATI:
	dllname = "WinUAE_Gujarati.dll";
	break;
    case LANG_HEBREW:
	dllname = "WinUAE_Hebrew.dll";
	break;
    case LANG_HINDI:
	dllname = "WinUAE_Hindi.dll";
	break;
    case LANG_HUNGARIAN:
	dllname = "WinUAE_Hungarian.dll";
	break;
    case LANG_ICELANDIC:
	dllname = "WinUAE_Icelandic.dll";
	break;
    case LANG_INDONESIAN:
	dllname = "WinUAE_Indonesian.dll";
	break;
    case LANG_ITALIAN:
	dllname = "WinUAE_Italian.dll";
	break;
    case LANG_JAPANESE:
	dllname = "WinUAE_Japanese.dll";
	break;
    case LANG_KANNADA:
	dllname = "WinUAE_Kannada.dll";
	break;
    case LANG_KASHMIRI:
	dllname = "WinUAE_Kashmiri.dll";
	break;
    case LANG_KAZAK:
	dllname = "WinUAE_Kazak.dll";
	break;
    case LANG_KONKANI:
	dllname = "WinUAE_Konkani.dll";
	break;
    case LANG_KOREAN:
	dllname = "WinUAE_Korean.dll";
	break;
    case LANG_LATVIAN:
	dllname = "WinUAE_Latvian.dll";
	break;
    case LANG_LITHUANIAN:
	dllname = "WinUAE_Lithuanian.dll";
	break;
    case LANG_MACEDONIAN:
	dllname = "WinUAE_Macedonian.dll";
	break;
    case LANG_MALAY:
	dllname = "WinUAE_Malay.dll";
	break;
    case LANG_MALAYALAM:
	dllname = "WinUAE_Malayalam.dll";
	break;
    case LANG_MANIPURI:
	dllname = "WinUAE_Manipuri.dll";
	break;
    case LANG_MARATHI:
	dllname = "WinUAE_Marathi.dll";
	break;
    case LANG_NEPALI:
	dllname = "WinUAE_Nepali.dll";
	break;
    case LANG_NORWEGIAN:
	dllname = "WinUAE_Norwegian.dll";
	break;
    case LANG_ORIYA:
	dllname = "WinUAE_Oriya.dll";
	break;
    case LANG_POLISH:
	dllname = "WinUAE_Polish.dll";
	break;
    case LANG_PORTUGUESE:
	dllname = "WinUAE_Portuguese.dll";
	break;
    case LANG_PUNJABI:
	dllname = "WinUAE_Punjabi.dll";
	break;
    case LANG_ROMANIAN:
	dllname = "WinUAE_Romanian.dll";
	break;
    case LANG_RUSSIAN:
	dllname = "WinUAE_Russian.dll";
	break;
    case LANG_SANSKRIT:
	dllname = "WinUAE_Sanskrit.dll";
	break;
    case LANG_SINDHI:
	dllname = "WinUAE_Sindhi.dll";
	break;
    case LANG_SLOVAK:
	dllname = "WinUAE_Slovak.dll";
	break;
    case LANG_SLOVENIAN:
	dllname = "WinUAE_Slovenian.dll";
	break;
    case LANG_SPANISH:
	dllname = "WinUAE_Spanish.dll";
	break;
    case LANG_SWAHILI:
	dllname = "WinUAE_Swahili.dll";
	break;
    case LANG_SWEDISH:
	dllname = "WinUAE_Swedish.dll";
	break;
    case LANG_TAMIL:
	dllname = "WinUAE_Tamil.dll";
	break;
    case LANG_TATAR:
	dllname = "WinUAE_Tatar.dll";
	break;
    case LANG_TELUGU:
	dllname = "WinUAE_Telugu.dll";
	break;
    case LANG_THAI:
	dllname = "WinUAE_Thai.dll";
	break;
    case LANG_TURKISH:
	dllname = "WinUAE_Turkish.dll";
	break;
    case LANG_UKRAINIAN:
	dllname = "WinUAE_Ukrainian.dll";
	break;
    case LANG_URDU:
	dllname = "WinUAE_Urdu.dll";
	break;
    case LANG_UZBEK:
	dllname = "WinUAE_Uzbek.dll";
	break;
    case LANG_VIETNAMESE:
	dllname = "WinUAE_Vietnamese.dll";
	break;
    case 0x400:
	dllname = "guidll.dll";
	break;
    }

    if( dllname )
    {
	TCHAR  szFilename[ MAX_PATH ];
	DWORD  dwVersionHandle, dwFileVersionInfoSize;
	LPVOID lpFileVersionData = NULL;
	BOOL   success = FALSE;
	result = LoadLibrary( dllname );
	if( result && GetModuleFileName( result, (LPTSTR)&szFilename, MAX_PATH ) )
	{
	    dwFileVersionInfoSize = GetFileVersionInfoSize( szFilename, &dwVersionHandle );
	    if( dwFileVersionInfoSize )
	    {
		if( lpFileVersionData = calloc( 1, dwFileVersionInfoSize ) )
		{
		    if( GetFileVersionInfo( szFilename, dwVersionHandle, dwFileVersionInfoSize, lpFileVersionData ) )
		    {
			VS_FIXEDFILEINFO *vsFileInfo = NULL;
			UINT uLen;
			if( VerQueryValue( lpFileVersionData, TEXT("\\"), (void **)&vsFileInfo, &uLen ) )
			{
			    if( vsFileInfo &&
				( HIWORD(vsFileInfo->dwProductVersionMS) == UAEMAJOR ) 
				&& ( LOWORD(vsFileInfo->dwProductVersionMS) == UAEMINOR ) 
				&& ( HIWORD(vsFileInfo->dwProductVersionLS) == UAESUBREV )
// Change this to an #if 1 when the WinUAE Release version (as opposed to UAE-core version) 
// requires a GUI-DLL change...
#if 0
				&& ( LOWORD(vsFileInfo->dwProductVersionLS) == WINUAERELEASE) 
#endif
				)
			    {
				success = TRUE;
			    }
			}
		    }
		    free( lpFileVersionData );
		}
	    }
	}
	if( result && !success )
	{
	    FreeLibrary( result );
	    result = NULL;
	}
    }

    return result;
}


/* try to load COMDLG32 and DDRAW, initialize csDraw */
int WIN32_InitLibraries( void )
{
    int result = 1;
    /* Determine our processor speed and capabilities */
    if (!figure_processor_speed())
	return 0;
    
    /* Make sure we do an InitCommonControls() to get some advanced controls */
    InitCommonControls();
    
    hRichEdit = LoadLibrary( "RICHED32.DLL" );
    
    hUIDLL = LoadGUI();

    return result;
}

int debuggable (void)
{
    return 0;
}

int needmousehack (void)
{
    return 1;
}

void LED (int a)
{
}

void logging_init( void )
{
    static int started;
    static int first;
    char debugfilename[MAX_PATH];

    if (first > 1) {
	write_log ("** RESTART **\n");
	return;
    }
    if (first == 1) {
	if (debugfile)
	    fclose (debugfile);
        debugfile = 0;
    }
    if( currprefs.win32_logfile ) {
	sprintf( debugfilename, "%s\\winuaelog.txt", start_path );
	if( !debugfile )
	    debugfile = fopen( debugfilename, "wt" );
    } else if (!first) {
	sprintf( debugfilename, "%s\\winuaebootlog.txt", start_path );
	if( !debugfile )
	    debugfile = fopen( debugfilename, "wt" );
    }
    first++;
    write_log ( "%s", VersionStr );
    write_log (" (OS: %s %d.%d%s)", os_winnt ? "NT" : "W9X/ME", osVersion.dwMajorVersion, osVersion.dwMinorVersion, os_winnt_admin ? " Administrator privileges" : "");
    write_log ("\n(c) 1995-2001 Bernd Schmidt   - Core UAE concept and implementation."
	       "\n(c) 1998-2003 Toni Wilen      - Win32 port, core code updates."
	       "\n(c) 1996-2001 Brian King      - Win32 port, Picasso96 RTG, and GUI."
	       "\n(c) 1996-1999 Mathias Ortmann - Win32 port and bsdsocket support."
	       "\n(c) 2000-2001 Bernd Meyer     - JIT engine."
	       "\n(c) 2000-2001 Bernd Roesch    - MIDI input, many fixes."
	       "\nPress F12 to show the Settings Dialog (GUI), Alt-F4 to quit."
	       "\nEnd+F1 changes floppy 0, End+F2 changes floppy 1, etc."
	       "\n");
}

void logging_cleanup( void )
{
    if( debugfile )
        fclose( debugfile );
    debugfile = 0;
}

static const char *obsolete[] = {
    "killwinkeys", "sound_force_primary", "iconified_highpriority",
    "sound_sync", "sound_tweak", "directx6", "sound_style",
    "file_path",
    0
};

void target_save_options (FILE *f, struct uae_prefs *p)
{
    cfgfile_write (f, "win32.middle_mouse=%s\n", p->win32_middle_mouse ? "true" : "false");
    cfgfile_write (f, "win32.logfile=%s\n", p->win32_logfile ? "true" : "false");
    cfgfile_write (f, "win32.map_drives=%s\n", p->win32_automount_drives ? "true" : "false" );
    cfgfile_write (f, "win32.serial_port=%s\n", p->use_serial ? p->sername : "none" );
    cfgfile_write (f, "win32.parallel_port=%s\n", p->prtname[0] ? p->prtname : "none" );
    cfgfile_write (f, "win32.activepriority=%d\n", priorities[p->win32_activepriority].value);
    cfgfile_write (f, "win32.iconified_nospeed=%s\n", p->win32_iconified_nospeed ? "true" : "false");
    cfgfile_write (f, "win32.iconified_pause=%s\n", p->win32_iconified_pause ? "true" : "false");
    cfgfile_write (f, "win32.iconified_nosound=%s\n", p->win32_iconified_nosound ? "true" : "false");
    cfgfile_write (f, "win32.ctrl_f11_is_quit=%s\n", p->win32_ctrl_F11_is_quit ? "true" : "false");
    cfgfile_write (f, "win32.midiout_device=%d\n", p->win32_midioutdev );
    cfgfile_write (f, "win32.midiin_device=%d\n", p->win32_midiindev );
    cfgfile_write (f, "win32.no_overlay=%s\n", p->win32_no_overlay ? "true" : "false" );
    cfgfile_write (f, "win32.aspi=%s\n", p->win32_aspi ? "true" : "false" );
    cfgfile_write (f, "win32.soundcard=%d\n", p->win32_soundcard );
    cfgfile_write (f, "win32.cpu_idle=%d\n", p->cpu_idle);
}

int target_parse_option (struct uae_prefs *p, char *option, char *value)
{
    int cpuidle, i;
    int result = (cfgfile_yesno (option, value, "middle_mouse", &p->win32_middle_mouse)
	    || cfgfile_yesno (option, value, "logfile", &p->win32_logfile)
	    || cfgfile_yesno  (option, value, "networking", &p->socket_emu)
	    || cfgfile_yesno (option, value, "no_overlay", &p->win32_no_overlay)
	    || cfgfile_yesno (option, value, "aspi", &p->win32_aspi)
	    || cfgfile_yesno  (option, value, "map_drives", &p->win32_automount_drives)
	    || cfgfile_yesno (option, value, "iconified_nospeed", &p->win32_iconified_nospeed)
	    || cfgfile_yesno (option, value, "iconified_pause", &p->win32_iconified_pause)
	    || cfgfile_yesno (option, value, "iconified_nosound", &p->win32_iconified_nosound)
	    || cfgfile_yesno  (option, value, "ctrl_f11_is_quit", &p->win32_ctrl_F11_is_quit)
	    || cfgfile_intval (option, value, "midi_device", &p->win32_midioutdev, 1)
	    || cfgfile_intval (option, value, "midiout_device", &p->win32_midioutdev, 1)
	    || cfgfile_intval (option, value, "midiin_device", &p->win32_midiindev, 1)
	    || cfgfile_intval (option, value, "soundcard", &p->win32_soundcard, 1)
	    || cfgfile_string (option, value, "serial_port", &p->sername[0], 256)
	    || cfgfile_string (option, value, "parallel_port", &p->prtname[0], 256)
	    || cfgfile_intval  (option, value, "cpu_idle", &p->cpu_idle, 1));

    if (cfgfile_intval (option, value, "activepriority", &p->win32_activepriority, 1)) {
	result = 1;
	i = 0;
	while (priorities[i].name) {
	    if (priorities[i].value == p->win32_activepriority) {
		p->win32_activepriority = i;
		break;
	    }
	    i++;
	}
	if (priorities[i].name == 0)
	    p->win32_activepriority = 2;
    }

    cpuidle = -1;
    if (cfgfile_yesno (option, value, "cpu_idle", &cpuidle)) {
	if (cpuidle == 1)
	     p->cpu_idle = 60;
    }

    if (p->sername[0] == 'n')
	p->use_serial = 0;
    else
	p->use_serial = 1;

    i = 0;
    while (obsolete[i]) {
	if (!strcasecmp (obsolete[i], option)) {
	    write_log ("obsolete config entry '%s'\n", option);
	    return 1;
	}
	i++;
    }

    return result;
}

static void WIN32_HandleRegistryStuff( void )
{
    RGBFTYPE colortype      = RGBFB_NONE;
    DWORD dwType            = REG_DWORD;
    DWORD dwDisplayInfoSize = sizeof( colortype );
    DWORD disposition;
    char path[MAX_PATH] = "";
    HKEY hWinUAEKeyLocal = NULL;

    /* Create/Open the hWinUAEKey which points to our config-info */
    if( RegCreateKeyEx( HKEY_CLASSES_ROOT, ".uae", 0, "", REG_OPTION_NON_VOLATILE,
                          KEY_ALL_ACCESS, NULL, &hWinUAEKey, &disposition ) == ERROR_SUCCESS )
    {
	// Regardless of opening the existing key, or creating a new key, we will write the .uae filename-extension
	// commands in.  This way, we're always up to date.

        /* Set our (default) sub-key to point to the "WinUAE" key, which we then create */
        RegSetValueEx( hWinUAEKey, "", 0, REG_SZ, (CONST BYTE *)"WinUAE", strlen( "WinUAE" ) + 1 );

        if( ( RegCreateKeyEx( HKEY_CLASSES_ROOT, "WinUAE\\shell\\Edit\\command", 0, "", REG_OPTION_NON_VOLATILE,
                              KEY_ALL_ACCESS, NULL, &hWinUAEKeyLocal, &disposition ) == ERROR_SUCCESS ) )
        {
            /* Set our (default) sub-key to BE the "WinUAE" command for editing a configuration */
            sprintf( path, "%s\\WinUAE.exe -f \"%%1\" -s use_gui=yes", start_path );
            RegSetValueEx( hWinUAEKeyLocal, "", 0, REG_SZ, (CONST BYTE *)path, strlen( path ) + 1 );
        }
		RegCloseKey( hWinUAEKeyLocal );

        if( ( RegCreateKeyEx( HKEY_CLASSES_ROOT, "WinUAE\\shell\\Open\\command", 0, "", REG_OPTION_NON_VOLATILE,
                              KEY_ALL_ACCESS, NULL, &hWinUAEKeyLocal, &disposition ) == ERROR_SUCCESS ) )
        {
            /* Set our (default) sub-key to BE the "WinUAE" command for launching a configuration */
            sprintf( path, "%s\\WinUAE.exe -f \"%%1\"", start_path );
            RegSetValueEx( hWinUAEKeyLocal, "", 0, REG_SZ, (CONST BYTE *)path, strlen( path ) + 1 );
        }
	RegCloseKey( hWinUAEKeyLocal );
    }
    RegCloseKey( hWinUAEKey );

    /* Create/Open the hWinUAEKey which points our config-info */
    if( RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Arabuusimiehet\\WinUAE", 0, "", REG_OPTION_NON_VOLATILE,
                          KEY_ALL_ACCESS, NULL, &hWinUAEKey, &disposition ) == ERROR_SUCCESS )
    {
        if( disposition == REG_CREATED_NEW_KEY )
        {
            /* Create and initialize all our sub-keys to the default values */
            colortype = 0;
            RegSetValueEx( hWinUAEKey, "DisplayInfo", 0, REG_DWORD, (CONST BYTE *)&colortype, sizeof( colortype ) );
            RegSetValueEx( hWinUAEKey, "xPos", 0, REG_DWORD, (CONST BYTE *)&colortype, sizeof( colortype ) );
            RegSetValueEx( hWinUAEKey, "yPos", 0, REG_DWORD, (CONST BYTE *)&colortype, sizeof( colortype ) );
            RegSetValueEx( hWinUAEKey, "FloppyPath", 0, REG_SZ, (CONST BYTE *)start_path, strlen( start_path ) + 1 );
            RegSetValueEx( hWinUAEKey, "KickstartPath", 0, REG_SZ, (CONST BYTE *)start_path, strlen( start_path ) + 1 );
            RegSetValueEx( hWinUAEKey, "hdfPath", 0, REG_SZ, (CONST BYTE *)start_path, strlen( start_path ) + 1 );
        }
	// Set this even when we're opening an existing key, so that the version info is always up to date.
        RegSetValueEx( hWinUAEKey, "Version", 0, REG_SZ, (CONST BYTE *)VersionStr, strlen( VersionStr ) + 1 );
        
		RegQueryValueEx( hWinUAEKey, "DisplayInfo", 0, &dwType, (LPBYTE)&colortype, &dwDisplayInfoSize );
		if( colortype == 0 ) /* No color information stored in the registry yet */
		{
			char szMessage[ 4096 ];
			char szTitle[ MAX_PATH ];
			WIN32GUI_LoadUIString( IDS_GFXCARDCHECK, szMessage, 4096 );
			WIN32GUI_LoadUIString( IDS_GFXCARDTITLE, szTitle, MAX_PATH );
		    
			if( MessageBox( NULL, szMessage, szTitle, 
			MB_YESNO | MB_ICONWARNING | MB_TASKMODAL | MB_SETFOREGROUND ) == IDYES )
			{
			ignore_messages_all++;
			colortype = WIN32GFX_FigurePixelFormats(0);
			ignore_messages_all--;
			RegSetValueEx( hWinUAEKey, "DisplayInfo", 0, REG_DWORD, (CONST BYTE *)&colortype, sizeof( colortype ) );
			}
		}
		if( colortype ) {
			/* Set the 16-bit pixel format for the appropriate modes */
			WIN32GFX_FigurePixelFormats( colortype );
		}
	}
}

static void betamessage (void)
{
}

static void init_zlib (void)
{
    HMODULE h = LoadLibrary ("zlib.dll");
    if (h) {
	is_zlib = 1;
	FreeLibrary(h);
    } else {
	write_log ("zlib.dll not found, gzip/zip support disabled\n");
    }
}

static int dxdetect (void)
{
    /* believe or not but this is MS supported way of detecting DX8+ */
    HMODULE h = LoadLibrary("D3D8.DLL");
    char szWrongDXVersion[ MAX_PATH ];
    if (h) {
	FreeLibrary (h);
	return 1;
    }
    WIN32GUI_LoadUIString( IDS_WRONGDXVERSION, szWrongDXVersion, MAX_PATH );
    pre_gui_message( szWrongDXVersion );
    return 0;
}

int os_winnt, os_winnt_admin;

static int osdetect (void)
{
    HANDLE hAccessToken;
    UCHAR InfoBuffer[1024];
    PTOKEN_GROUPS ptgGroups = (PTOKEN_GROUPS)InfoBuffer;
    DWORD dwInfoBufferSize;
    PSID psidAdministrators;
    SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
    UINT x;
    BOOL bSuccess;

    os_winnt = 0;
    os_winnt_admin = 0;

    osVersion.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
    if( GetVersionEx( &osVersion ) )
    {
	if( ( osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT ) &&
	    ( osVersion.dwMajorVersion <= 4 ) )
	{
	    /* WinUAE not supported on this version of Windows... */
	    char szWrongOSVersion[ MAX_PATH ];
	    WIN32GUI_LoadUIString( IDS_WRONGOSVERSION, szWrongOSVersion, MAX_PATH );
	    pre_gui_message( szWrongOSVersion );
	    return FALSE;
	}
	if (osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)
	    os_winnt = 1;
    }

    if (!os_winnt) {
	return 1;
    }

    if(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE,
         &hAccessToken )) {
         if(GetLastError() != ERROR_NO_TOKEN)
            return 1;
         // 
         // retry against process token if no thread token exists
         // 
         if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY,
            &hAccessToken))
            return 1;
      }

      bSuccess = GetTokenInformation(hAccessToken,TokenGroups,InfoBuffer,
         1024, &dwInfoBufferSize);

      CloseHandle(hAccessToken);

      if(!bSuccess )
         return 1;

      if(!AllocateAndInitializeSid(&siaNtAuthority, 2,
         SECURITY_BUILTIN_DOMAIN_RID,
         DOMAIN_ALIAS_RID_ADMINS,
         0, 0, 0, 0, 0, 0,
         &psidAdministrators))
         return 1;

   // assume that we don't find the admin SID.
      bSuccess = FALSE;

      for(x=0;x<ptgGroups->GroupCount;x++)
      {
         if( EqualSid(psidAdministrators, ptgGroups->Groups[x].Sid) )
         {
            bSuccess = TRUE;
            break;
         }

      }
      FreeSid(psidAdministrators);
      os_winnt_admin = bSuccess ? 1 : 0;
      return 1;
   }


static int resolution_compare (const void *a, const void *b)
{
    struct PicassoResolution *ma = (struct PicassoResolution *)a;
    struct PicassoResolution *mb = (struct PicassoResolution *)b;
    if (ma->res.width < mb->res.width)
	return -1;
    if (ma->res.width > mb->res.width)
	return 1;
    if (ma->res.height < mb->res.height)
	return -1;
    if (ma->res.height > mb->res.height)
	return 1;
    return ma->depth - mb->depth;
}
static void sortmodes (void)
{
    int	count = 0;
    while (DisplayModes[count].depth >= 0)
	count++;
    qsort (DisplayModes, count, sizeof (struct PicassoResolution), resolution_compare);
}

char *start_path = NULL;
char help_file[ MAX_PATH ];
extern int harddrive_dangerous, do_rdbdump, aspi_allow_all, dsound_hardware_mixing;

int PASCAL WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		    int nCmdShow)
{
    char *posn;
    HANDLE hMutex;
    char **argv;
    int argc;
    int i;
    char tmp[2000];

#ifdef __GNUC__
    __asm__ ("leal -2300*1024(%%esp),%0" : "=r" (win32_stackbase) :);
#else
__asm{
    mov eax,esp
    sub eax,2300*1024
    mov win32_stackbase,eax
 }
#endif

#ifdef _DEBUG
    {
	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmp |= _CRTDBG_CHECK_ALWAYS_DF;
	tmp |= _CRTDBG_CHECK_CRT_DF;
	//tmp |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmp);
    }
#endif

    if (!osdetect())
	return 0;
    if (!dxdetect())
	return 0;

    hInst = hInstance;
    hMutex = CreateMutex( NULL, FALSE, "WinUAE Instantiated" ); // To tell the installer we're running
#ifdef AVIOUTPUT
    AVIOutput_Initialize();
#endif

#ifdef __MINGW32__
    argc = _argc; argv = _argv;
#else
    argc = __argc; argv = __argv;
#endif
    for (i = 1; i < argc; i++) {
	if (!strcmp (argv[i], "-log")) console_logging = 1;
#ifdef FILESYS
	if (!strcmp (argv[i], "-rdbdump")) do_rdbdump = 1;
	if (!strcmp (argv[i], "-disableharddrivesafetycheck")) harddrive_dangerous = 0x1234dead;
	if (!strcmp (argv[i], "-noaspifiltering")) aspi_allow_all = 1;
#endif
	if (!strcmp (argv[i], "-dsaudiomix")) dsound_hardware_mixing = 1;
    }

    /* Get our executable's root-path */
    if( ( start_path = xmalloc( MAX_PATH ) ) )
    {
	GetModuleFileName( NULL, start_path, MAX_PATH );
	if( ( posn = strrchr( start_path, '\\' ) ) )
	    *posn = 0;
	sprintf (help_file, "%s\\WinUAE.chm", start_path );
	sprintf (tmp, "%s\\SaveStates", start_path);
	CreateDirectory (tmp, NULL);
	strcat (tmp, "\\default.uss");
	strcpy (savestate_fname, tmp);
	sprintf (tmp, "%s\\SaveImages", start_path);
	CreateDirectory (tmp, NULL);
	sprintf (tmp, "%s\\ScreenShots", start_path);
	CreateDirectory (tmp, NULL);

	sprintf( VersionStr, "WinUAE %d.%d.%d, Release %d%s", UAEMAJOR, UAEMINOR, UAESUBREV, WINUAERELEASE, WINUAEBETA ? WINUAEBETASTR : "" );

	logging_init ();

	if( WIN32_RegisterClasses() && WIN32_InitLibraries() && DirectDraw_Start() )
	{
	    struct foo {
		DEVMODE actual_devmode;
		char overrun[8];
	    } devmode;

	    DWORD i = 0;

	    DisplayModes[0].depth = -1;
	    DirectDraw_EnumDisplayModes( DDEDM_REFRESHRATES , modesCallback );
	    sortmodes ();
	    
	    memset( &devmode, 0, sizeof(DEVMODE) + 8 );
	    devmode.actual_devmode.dmSize = sizeof(DEVMODE);
	    devmode.actual_devmode.dmDriverExtra = 8;
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
	    if( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, (LPDEVMODE)&devmode ) )
	    {
		default_freq = devmode.actual_devmode.dmDisplayFrequency;
		//write_log( "Your Windows desktop refresh frequency is %d Hz\n", default_freq );
		if( default_freq >= 70 )
		    default_freq = 70;
		else
		    default_freq = 60;
	    }

	    WIN32_HandleRegistryStuff();
	    if( WIN32_InitHtmlHelp() == 0 )
	    {
		char szMessage[ MAX_PATH ];
		WIN32GUI_LoadUIString( IDS_NOHELP, szMessage, MAX_PATH );
		write_log( szMessage );
	    }

	    DirectDraw_Release();
	    betamessage ();
	    keyboard_settrans ();
	    init_zlib ();
	    real_main (argc, argv);
	}
	free( start_path );
    }
	
    if (mm_timerres && timermode == 0)
	timeend ();
#ifdef AVIOUTPUT
    AVIOutput_Release ();
#endif
#ifdef AHI
    ahi_close_sound ();
#endif
    WIN32_CleanupLibraries();
    _fcloseall();
    if( hWinUAEKey )
	RegCloseKey( hWinUAEKey );
    CloseHandle( hMutex );
#ifdef _DEBUG
    // show memory leaks
    //_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
    return FALSE;
}

int execute_command (char *cmd)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset (&si, 0, sizeof (si));
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if( CreateProcess( NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi ) )  {
	WaitForSingleObject( pi.hProcess, INFINITE );
	return 1;
    }
    return 0;
}

struct threadpriorities priorities[] = {
    { "Highest", THREAD_PRIORITY_HIGHEST },
    { "Above Normal", THREAD_PRIORITY_ABOVE_NORMAL },
    { "Normal", THREAD_PRIORITY_NORMAL },
    { "Below Normal", THREAD_PRIORITY_BELOW_NORMAL },
    { "Low", THREAD_PRIORITY_LOWEST },
    { 0, -1 }
};

