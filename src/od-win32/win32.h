/* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Win32-specific header file
  * 
  * (c) 1997-1999 Mathias Ortmann
  * (c) 1998-2001 Brian King
  */

#ifndef __WIN32_H__
#define __WIN32_H__

#define IHF_WINDOWHIDDEN 6
#define NORMAL_WINDOW_STYLE (WS_VISIBLE | WS_BORDER | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU )

extern HMODULE hUIDLL;
extern HWND hAmigaWnd, hMainWnd;
extern RECT amigawin_rect;
extern int in_sizemove;
extern int manual_painting_needed;
extern int mouseactive, focus;
#define WINUAERELEASE 9
#define WINUAEBETA 0
#define WINUAEBETASTR ""

extern void my_kbd_handler (int, int, int);
extern void clearallkeys(void);
extern int getcapslock (void);

void releasecapture (void);
int WIN32_RegisterClasses( void );
int WIN32_InitHtmlHelp( void );
int WIN32_InitLibraries( void );
int WIN32_CleanupLibraries( void );
void WIN32_MouseDefaults( int, int );
void WIN32_HandleRegistryStuff( void );
extern void setup_brkhandler (void);
extern void remove_brkhandler (void);
extern void disablecapture (void);
extern void fullscreentoggle (void);

extern void setmouseactive (int active);
extern void minimizewindow (void);
extern uae_u32 OSDEP_minimize_uae(void);

void finishjob (void);
void updatedisplayarea (void);
void init_colors (void);

extern int pause_emulation;
extern int sound_available;
extern int framecnt;
extern char prtname[];
extern char VersionStr[256];
extern int os_winnt, os_winnt_admin;

/* For StatusBar when running in a Window */
#define LED_NUM_PARTS 9
#define LED_POWER_WIDTH 42
#define LED_HD_WIDTH 24
#define LED_CD_WIDTH 24
#define LED_DRIVE_WIDTH 24
#define LED_FPS_WIDTH 58

extern HKEY hWinUAEKey;
extern int screen_is_picasso;
extern HINSTANCE hInst;
extern int win_x_diff, win_y_diff;

extern void sleep_millis (int ms);
extern void sleep_millis_busy (int ms);
extern void screenshot(int mode);
extern void wait_keyrelease (void);
extern void keyboard_settrans (void);

#ifndef _WIN32_WCE
#include "win32gui.h"
#include "resource.h"
#endif

extern void handle_rawinput (DWORD lParam);

#define DEFAULT_PRIORITY 2
struct threadpriorities {
    char *name;
    int value;
};
extern struct threadpriorities priorities[];

#endif