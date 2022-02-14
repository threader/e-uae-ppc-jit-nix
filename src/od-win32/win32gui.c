/*==========================================================================
 *
 *  Copyright (C) 1996 Brian King
 *
 *  File:       win32gui.c
 *  Content:    Win32-specific gui features for UAE port.
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winspool.h>
#include <winuser.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <process.h>
#include <prsht.h>
#include <richedit.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <ddraw.h>

#include "config.h"
#include "resource.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "gui.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "events.h"
#include "newcpu.h"
#include "disk.h"
#include "uae.h"
#include "threaddep/thread.h"
#include "filesys.h"
#include "autoconf.h"
#include "inputdevice.h"
#include "xwin.h"
#include "keyboard.h"
#include "zfile.h"

#include "dxwrap.h"
#include "win32.h"
#include "picasso96_win.h"
#include "win32gui.h"
#include "win32gfx.h"
#include "sounddep/sound.h"
#include "od-win32/parser.h"
#include "od-win32/ahidsound.h"
#include "target.h"
#include "savestate.h"
#include "avioutput.h"
#include "opengl.h"
#include "direct3d.h"
#include "akiko.h"
#include "filter.h"

#define DISK_FORMAT_STRING "(*.adf;*.adz;*.gz;*.dms;*.fdi;*.ipf;*.zip;*.exe)\0*.adf;*.adz;*.gz;*.dms;*.fdi;*.ipf;*.zip;*.exe\0"
#define ROM_FORMAT_STRING "(*.rom;*.zip;*.roz)\0*.rom;*.zip;*.roz\0"
#define USS_FORMAT_STRING_RESTORE "(*.uss;*.gz;*.zip)\0*.uss;*.gz;*.zip\0"
#define USS_FORMAT_STRING_SAVE "(*.uss)\0*.uss\0"

static int allow_quit;
static int full_property_sheet = 1;
static struct uae_prefs *pguiprefs;
struct uae_prefs workprefs;
static int currentpage;

extern HWND (WINAPI *pHtmlHelp)(HWND, LPCSTR, UINT, LPDWORD );
#undef HtmlHelp
#ifndef HH_DISPLAY_TOPIC
#define HH_DISPLAY_TOPIC 0
#endif
#define HtmlHelp(a,b,c,d) if( pHtmlHelp ) (*pHtmlHelp)(a,b,c,(LPDWORD)d); else \
{ char szMessage[ MAX_PATH ]; WIN32GUI_LoadUIString( IDS_NOHELP, szMessage, MAX_PATH ); gui_message( szMessage ); }

extern HWND hAmigaWnd;
extern char help_file[ MAX_PATH ];

extern int mouseactive;
extern char *start_path;

extern char configname[256];
static char config_filename[ MAX_PATH ] = "";

static drive_specs blankdrive =
{"", "", 1, 32, 1, 2, 0, 0};

static int input_update;

#define Error(x) MessageBox( NULL, (x), "WinUAE Error", MB_OK )

void WIN32GUI_LoadUIString( DWORD id, char *string, DWORD dwStringLen )
{
    if( LoadString( hUIDLL ? hUIDLL : hInst, id, string, dwStringLen ) == 0 )
	LoadString( hInst, id, string, dwStringLen );
}

static HWND hPropertySheet = NULL;

static int C_PAGES;
#define MAX_C_PAGES 30
static int LOADSAVE_ID, MEMORY_ID, KICKSTART_ID, CPU_ID,
    DISPLAY_ID, HW3D_ID, CHIPSET_ID, SOUND_ID, FLOPPY_ID, HARDDISK_ID,
    PORTS_ID, INPUT_ID, MISC_ID, AVIOUTPUT_ID, ABOUT_ID;
static int refreshtab[30];
static HWND pages[30];

static void exit_gui (int ok)
{
    PropSheet_PressButton( hPropertySheet, ok ? PSBTN_OK : PSBTN_CANCEL );
}

#define MIN_CHIP_MEM 0
#define MAX_CHIP_MEM 5
#define MIN_FAST_MEM 0
#define MAX_FAST_MEM 4
#define MIN_SLOW_MEM 0
#define MAX_SLOW_MEM 3
#define MIN_Z3_MEM 0
#define MAX_Z3_MEM 10
#define MIN_P96_MEM 0
#define MAX_P96_MEM 6
#define MIN_M68K_PRIORITY 1
#define MAX_M68K_PRIORITY 16
#define MIN_CACHE_SIZE 0
#define MAX_CACHE_SIZE 8
#define MIN_REFRESH_RATE 1
#define MAX_REFRESH_RATE 10
#define MIN_SOUND_MEM 0
#define MAX_SOUND_MEM 6

static char szNone[ MAX_PATH ] = "None";

static int cfgfile_doload (struct uae_prefs *p, const char *filename)
{
    discard_prefs (p);
#ifdef FILESYS
    free_mountinfo (currprefs.mountinfo);
    currprefs.mountinfo = alloc_mountinfo ();
#endif
    default_prefs (p);
    input_update = 1;
    return cfgfile_load (p, filename);
}

/* if drive is -1, show the full GUI, otherwise file-requester for DF[drive] */
void WIN32GUI_DisplayGUI( int shortcut )
{
    int flipflop = 0;
    HRESULT hr;

#ifdef D3D
    D3D_guimode (TRUE);
#endif
#ifdef CD32
    akiko_entergui ();
#endif
    inputdevice_unacquire ();
    clearallkeys ();
#ifdef AHI
    ahi_close_sound ();
#endif
    pause_sound ();
    setmouseactive (0);

    if( ( !WIN32GFX_IsPicassoScreen() && currprefs.gfx_afullscreen && ( currprefs.gfx_width < 640 ) )
#ifdef PICASSO96
        || ( WIN32GFX_IsPicassoScreen() && currprefs.gfx_pfullscreen && ( picasso96_state.Width < 640 ) )
#endif
    ) {
        flipflop = 1;
    }

    WIN32GFX_ClearPalette();
    manual_painting_needed++; /* So that WM_PAINT will refresh the display */

    hr = DirectDraw_FlipToGDISurface();
    if (hr != DD_OK)
	write_log ("FlipToGDISurface failed, %s\n", DXError (hr));

    if( shortcut == -1 ) {
	int ret;
#ifdef AVIOUTPUT
        AVIOutput_End ();
#endif
        if( flipflop )
            ShowWindow( hAmigaWnd, SW_MINIMIZE );
	ret = GetSettings (0);
        if( flipflop )
            ShowWindow( hAmigaWnd, SW_RESTORE );
	if (!ret) {
	    savestate_state = 0;
	}
#ifdef AVIOUTPUT
        AVIOutput_Begin ();
#endif
    } else if (shortcut >= 0 && shortcut < 4) {
        DiskSelection( hAmigaWnd, IDC_DF0+shortcut, 0, &changed_prefs );
    } else if (shortcut == 5) {
        if (DiskSelection( hAmigaWnd, IDC_DOSAVESTATE, 9, &changed_prefs ))
	    savestate_state = STATE_DOSAVE;
    } else if (shortcut == 4) {
        if (DiskSelection( hAmigaWnd, IDC_DOLOADSTATE, 10, &changed_prefs ))
	    savestate_state = STATE_DORESTORE;
    }
    WIN32GFX_SetPalette();
#ifdef PICASSO96
    DX_SetPalette (0, 256);
#endif
    manual_painting_needed--; /* So that WM_PAINT doesn't need to use custom refreshing */
    resume_sound ();
#ifdef AHI
    ahi_open_sound ();
#endif
    inputdevice_copyconfig (&changed_prefs, &currprefs);
    inputdevice_config_change_test ();
    clearallkeys ();
    inputdevice_acquire (mouseactive);
#ifdef CD32
    akiko_exitgui ();
#endif
    setmouseactive (1);
#ifdef D3D
    D3D_guimode (FALSE);
#endif

    // This is a hack to fix the fact that time is passing while the GUI was present,
    // and we don't want our frames-per-second calculation in drawing.c to get skewed.
#ifdef HAVE_GETTIMEOFDAY
    {
	extern unsigned long int msecs, seconds_base;
	struct timeval tv;
	gettimeofday(&tv,NULL);
	msecs = (tv.tv_sec-seconds_base) * 1000 + tv.tv_usec / 1000;
    }
#endif
}

static void prefs_to_gui (struct uae_prefs *p)
{
    workprefs = *p;
    updatewinfsmode (&workprefs);
    /* Could also duplicate unknown lines, but no need - we never
       modify those.  */
#ifdef _DEBUG
    if (workprefs.gfx_framerate < 5)
	workprefs.gfx_framerate = 5;
#endif
}

static void gui_to_prefs (void)
{
    struct uaedev_mount_info *mi = currprefs.mountinfo;
    /* Always copy our prefs to changed_prefs, ... */
    //free_mountinfo (workprefs.mountinfo);
    changed_prefs = workprefs;
    updatewinfsmode (&changed_prefs);
    currprefs.mountinfo = mi;
}

static int notifycheck (LPARAM lParam, char *htm)
{
    switch (((NMHDR *) lParam)->code) 
    {
	case PSN_HELP:
	if (htm)
	    HtmlHelp( NULL, help_file, HH_DISPLAY_TOPIC, htm );
        return TRUE;
	case PSN_APPLY:
	/* Copy stuff from workprefs and config_xxx settings */
	gui_to_prefs ();
        return TRUE;
	case PSN_RESET:
	if (allow_quit) 
        {
	    quit_program = 1;
	    regs.spcflags |= SPCFLAG_BRK;
	}
        return TRUE;
    }
    return FALSE;
}


// Common routine for popping up a file-requester
// flag - 0 for floppy loading, 1 for floppy creation, 2 for loading hdf, 3 for saving hdf
// flag - 4 for loading .uae config-files, 5 for saving .uae config-files
// flag = 6 for loading .rom files, 7 for loading .key files
// flag = 8 for loading configurations
// flag = 9 for saving snapshots
// flag = 10 for loading snapshots
// flag = 11 for selecting flash files
// flag = 12 for loading anything
int DiskSelection( HWND hDlg, WPARAM wParam, int flag, struct uae_prefs *prefs )
{
    OPENFILENAME openFileName;
    char regfloppypath[MAX_PATH] = "";
    char regrompath[MAX_PATH] = "";
    char reghdfpath[MAX_PATH] = "";
    DWORD dwType = REG_SZ;
    DWORD dwRFPsize = MAX_PATH;
    DWORD dwRRPsize = MAX_PATH;
    DWORD dwRHPsize = MAX_PATH;
    
    char full_path[MAX_PATH] = "";
    char file_name[MAX_PATH] = "";
    char init_path[MAX_PATH] = "";
    BOOL result = FALSE;
    char *amiga_path = NULL;
    char description[ CFG_DESCRIPTION_LENGTH ] = "";
    char *p;
    int i;
    int all = 1;

    char szTitle[ MAX_PATH ];
    char szFormat[ MAX_PATH ];
    char szFilter[ MAX_PATH ] = { 0 };
    
    memset (&openFileName, 0, sizeof (OPENFILENAME));
    if( hWinUAEKey )
    {
        RegQueryValueEx( hWinUAEKey, "FloppyPath", 0, &dwType, (LPBYTE)regfloppypath, &dwRFPsize );
        RegQueryValueEx( hWinUAEKey, "KickstartPath", 0, &dwType, (LPBYTE)regrompath, &dwRRPsize );
        RegQueryValueEx( hWinUAEKey, "hdfPath", 0, &dwType, (LPBYTE)reghdfpath, &dwRHPsize );
    }
    
    strncpy( init_path, start_path, MAX_PATH );
    switch( flag )
    {
	case 0:
	case 1:
	    if( regfloppypath[0] )
		strncpy( init_path, regfloppypath, MAX_PATH );
	    else
		strncat( init_path, "\\..\\shared\\adf\\", MAX_PATH );
	break;
	case 2:
	case 3:
	    if( reghdfpath[0] )
		strncpy( init_path, reghdfpath, MAX_PATH );
	    else
		strncat( init_path, "\\..\\shared\\hdf\\", MAX_PATH );
	break;
	case 6:
	case 7:
	case 11:
	    if( regrompath[0] )
		strncpy( init_path, regrompath, MAX_PATH );
	    else
		strncat( init_path, "\\..\\shared\\rom\\", MAX_PATH );
	break;
	case 4:
	case 5:
	case 8:
	    strncat( init_path, "\\Configurations\\", MAX_PATH );
	break;
	case 9:
	case 10:
	    strncat( init_path, "\\SaveStates\\", MAX_PATH );
	break;
	
    }

    openFileName.lStructSize = sizeof (OPENFILENAME);
    openFileName.hwndOwner = hDlg;
    openFileName.hInstance = hInst;
    
    switch (flag) {
    case 0:
	WIN32GUI_LoadUIString( IDS_SELECTADF, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_ADF, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), DISK_FORMAT_STRING, sizeof( DISK_FORMAT_STRING ) + 1 );

	openFileName.lpstrTitle = szTitle;
	openFileName.lpstrDefExt = "ADF";
	openFileName.lpstrFilter = szFilter;
	break;
    case 1:
	WIN32GUI_LoadUIString( IDS_CHOOSEBLANK, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_ADF, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), "(*.adf)\0*.adf\0", 15 );

	openFileName.lpstrTitle = szTitle;
	openFileName.lpstrDefExt = "ADF";
	openFileName.lpstrFilter = szFilter;
	break;
    case 2:
    case 3:
	WIN32GUI_LoadUIString( IDS_SELECTHDF, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_HDF, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), "(*.hdf)\0*.hdf\0", 15 );

	openFileName.lpstrTitle = szTitle;
	openFileName.lpstrDefExt = "HDF";
	openFileName.lpstrFilter = szFilter;
	break;
    case 4:
    case 5:
	WIN32GUI_LoadUIString( IDS_SELECTUAE, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_UAE, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), "(*.uae)\0*.uae\0", 15 );

	openFileName.lpstrTitle  = szTitle;
	openFileName.lpstrDefExt = "UAE";
	openFileName.lpstrFilter = szFilter;
	break;
    case 6:
	WIN32GUI_LoadUIString( IDS_SELECTROM, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_ROM, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), ROM_FORMAT_STRING, sizeof (ROM_FORMAT_STRING) + 1);

        openFileName.lpstrTitle = szTitle;
        openFileName.lpstrDefExt = "ROM";
        openFileName.lpstrFilter = szFilter;
        break;
    case 7:
	WIN32GUI_LoadUIString( IDS_SELECTKEY, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_KEY, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), "(*.key)\0*.key\0", 15 );

	openFileName.lpstrTitle = szTitle;
	openFileName.lpstrDefExt = "KEY";
        openFileName.lpstrFilter = szFilter;
        break;
    case 9:
    case 10:
	WIN32GUI_LoadUIString( flag == 10 ? IDS_RESTOREUSS : IDS_SAVEUSS, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_USS, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	if (flag == 10) {
	    memcpy( szFilter + strlen( szFilter ), USS_FORMAT_STRING_RESTORE, sizeof (USS_FORMAT_STRING_RESTORE) + 1);
	    all = 1;
	} else {
	    memcpy( szFilter + strlen( szFilter ), USS_FORMAT_STRING_SAVE, sizeof (USS_FORMAT_STRING_SAVE) + 1);
	    p = szFilter;
	    while (p[0] != 0 || p[1] !=0 ) p++;
	    p++;
	    strcpy (p, "Uncompressed (*.uss)");
	    p += strlen(p) + 1;
	    strcpy (p, "*.uss");
	    p += strlen(p) + 1;
	    strcpy (p, "RAM dump (*.dat)");
	    p += strlen(p) + 1;
	    strcpy (p, "*.dat");
	    p += strlen(p) + 1;
	    *p = 0;
	    all = 0;
	}
	openFileName.lpstrTitle  = szTitle;
	openFileName.lpstrDefExt = "USS";
	openFileName.lpstrFilter = szFilter;
	break;
    case 11:
	WIN32GUI_LoadUIString( IDS_SELECTFLASH, szTitle, MAX_PATH );
	WIN32GUI_LoadUIString( IDS_FLASH, szFormat, MAX_PATH );
	sprintf( szFilter, "%s ", szFormat );
	memcpy( szFilter + strlen( szFilter ), "(*.nvr)\0*.nvr\0", 15 );

	openFileName.lpstrTitle  = szTitle;
	openFileName.lpstrDefExt = "NVR";
	openFileName.lpstrFilter = szFilter;
	break;
    case 8:
    default:
	WIN32GUI_LoadUIString( IDS_SELECTINFO, szTitle, MAX_PATH );

	openFileName.lpstrTitle = szTitle;
	openFileName.lpstrFilter = NULL;
	openFileName.lpstrDefExt = NULL;
	break;
    }
    if (all) {
	p = szFilter;
	while (p[0] != 0 || p[1] !=0 ) p++;
	p++;
	strcpy (p, "All files (*.*)");
	p += strlen(p) + 1;
	strcpy (p, "*.*");
	p += strlen(p) + 1;
	*p = 0;
    }
    openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    openFileName.lpstrCustomFilter = NULL;
    openFileName.nMaxCustFilter = 0;
    openFileName.nFilterIndex = 0;
    openFileName.lpstrFile = full_path;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.lpstrFileTitle = file_name;
    openFileName.nMaxFileTitle = MAX_PATH;
    openFileName.lpstrInitialDir = init_path;
    openFileName.lpfnHook = NULL;
    openFileName.lpTemplateName = NULL;
    openFileName.lCustData = 0;
    if (flag == 1 || flag == 3 || flag == 5 || flag == 9 || flag == 11)
    {
	if( !(result = GetSaveFileName (&openFileName)) )
	    write_log ("GetSaveFileName() failed.\n");
    }
    else
    {
	if( !(result = GetOpenFileName (&openFileName)) )
	    write_log ("GetOpenFileName() failed.\n");
    }

    if (result)
    {
	switch (wParam) 
        {
	case IDC_PATH_NAME:
	case IDC_PATH_FILESYS:
	    if( flag == 8 )
	    {
		if( strstr( full_path, "Configurations\\" ) )
		{
		    strcpy( full_path, init_path );
		    strcat( full_path, file_name );
		}
	    }
	    SetDlgItemText (hDlg, wParam, full_path);
            break;
	case IDC_DF0:
	    SetDlgItemText (hDlg, IDC_DF0TEXT, full_path);
	    strcpy( prefs->df[0], full_path );
	    disk_insert( 0, full_path );
            break;
	case IDC_DF1:
	    SetDlgItemText (hDlg, IDC_DF1TEXT, full_path);
	    strcpy( prefs->df[1], full_path );
	    disk_insert( 1, full_path );
            break;
	case IDC_DF2:
	    SetDlgItemText (hDlg, IDC_DF2TEXT, full_path);
	    strcpy( prefs->df[2], full_path );
	    disk_insert( 2, full_path );
            break;
	case IDC_DF3:
	    SetDlgItemText (hDlg, IDC_DF3TEXT, full_path);
	    strcpy( prefs->df[3], full_path );
	    disk_insert( 3, full_path );
            break;
	case IDC_DOSAVESTATE:
	case IDC_DOLOADSTATE:
	    savestate_initsave (full_path, openFileName.nFilterIndex);
	    break;
	case IDC_CREATE:
	    disk_creatediskfile( full_path, 0, SendDlgItemMessage( hDlg, IDC_FLOPPYTYPE, CB_GETCURSEL, 0, 0L ));
            break;
	case IDC_CREATE_RAW:
	    disk_creatediskfile( full_path, 1, SendDlgItemMessage( hDlg, IDC_FLOPPYTYPE, CB_GETCURSEL, 0, 0L ));
	    break;
	case IDC_LOAD:
	    if( cfgfile_doload( &workprefs, full_path ) == 0 )
	    {
		char szMessage[ MAX_PATH ];
		WIN32GUI_LoadUIString( IDS_COULDNOTLOADCONFIG, szMessage, MAX_PATH );
		gui_message( szMessage );
	    }
	    else
	    {
		cfgfile_get_description( full_path, description );
		for( i = 0; i < C_PAGES; i++ )
		    SendMessage( pages[i], WM_USER, 0, 0 );
		SetDlgItemText( hDlg, IDC_EDITDESCRIPTION, description );
		SetDlgItemText( hDlg, IDC_EDITNAME, full_path );
	    }
            break;
	case IDC_SAVE:
	    SetDlgItemText( hDlg, IDC_EDITNAME, full_path );
            break;
	case IDC_ROMFILE:
	    SetDlgItemText( hDlg, IDC_ROMFILE, full_path );
	    strcpy( workprefs.romfile, full_path );
	    break;
	case IDC_ROMFILE2:
	    SetDlgItemText( hDlg, IDC_ROMFILE2, full_path );
	    strcpy( workprefs.romextfile, full_path );
	    break;
	case IDC_KEYFILE:
	    SetDlgItemText( hDlg, IDC_KEYFILE, full_path );
	    strcpy( workprefs.keyfile, full_path );
	    break;
	case IDC_FLASHFILE:
	    SetDlgItemText( hDlg, IDC_FLASHFILE, full_path );
	    strcpy( workprefs.flashfile, full_path );
	    break;
	case IDC_CARTFILE:
	    SetDlgItemText( hDlg, IDC_CARTFILE, full_path );
	    strcpy( workprefs.cartfile, full_path );
	    break;
        }
        if( flag == 0 || flag == 1 )
        {
            amiga_path = strstr( openFileName.lpstrFile, openFileName.lpstrFileTitle );
            if( amiga_path && amiga_path != openFileName.lpstrFile )
            {
                *amiga_path = 0;
                if( hWinUAEKey )
                    RegSetValueEx( hWinUAEKey, "FloppyPath", 0, REG_SZ, (CONST BYTE *)openFileName.lpstrFile, strlen( openFileName.lpstrFile ) );
            }
        }
        else if( flag == 2 || flag == 3 )
        {
            amiga_path = strstr( openFileName.lpstrFile, openFileName.lpstrFileTitle );
            if( amiga_path && amiga_path != openFileName.lpstrFile )
            {
                *amiga_path = 0;
                if( hWinUAEKey )
                    RegSetValueEx( hWinUAEKey, "hdfPath", 0, REG_SZ, (CONST BYTE *)openFileName.lpstrFile, strlen( openFileName.lpstrFile ) );
            }
        }
        else if( flag == 6 || flag == 7 )
        {
            amiga_path = strstr( openFileName.lpstrFile, openFileName.lpstrFileTitle );
            if( amiga_path && amiga_path != openFileName.lpstrFile )
            {
                *amiga_path = 0;
                if( hWinUAEKey )
                    RegSetValueEx( hWinUAEKey, "KickstartPath", 0, REG_SZ, (CONST BYTE *)openFileName.lpstrFile, strlen( openFileName.lpstrFile ) );
            }
        }
    }
    return result;
}

static BOOL CreateHardFile (HWND hDlg, UINT hfsizem)
{
    HANDLE hf;
    int i = 0;
    BOOL result = FALSE;
    LONG highword = 0;
    DWORD ret;
    char init_path[MAX_PATH] = "";
    uae_u64 hfsize;

    hfsize = (uae_u64)hfsizem * 1024 * 1024;
    DiskSelection (hDlg, IDC_PATH_NAME, 3, &workprefs);
    GetDlgItemText (hDlg, IDC_PATH_NAME, init_path, MAX_PATH);
    if (*init_path && hfsize) {
	SetCursor (LoadCursor(NULL, IDC_WAIT));
	if ((hf = CreateFile (init_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL) ) != INVALID_HANDLE_VALUE) {
	    if (hfsize >= 0x80000000) {
		highword = (DWORD)(hfsize >> 32);
		ret = SetFilePointer (hf, (DWORD)hfsize, &highword, FILE_BEGIN);
	    } else {
		ret = SetFilePointer (hf, (DWORD)hfsize, NULL, FILE_BEGIN);
	    }
	    if (ret == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                write_log ("SetFilePointer() failure for %s to posn %ud\n", init_path, hfsize);
	    else
                result = SetEndOfFile (hf);
            CloseHandle (hf);
        } else {
            write_log ("CreateFile() failed to create %s\n", init_path);
        }
    	SetCursor (LoadCursor (NULL, IDC_ARROW));
    }
    return result;
}

static const char *memsize_names[] = {
/* 0 */ szNone,
/* 1 */ "256 K",
/* 2 */ "512 K",
/* 3 */ "1 MB",
/* 4 */ "2 MB",
/* 5 */ "4 MB",
/* 6 */ "8 MB",
/* 7 */ "16 MB",
/* 8 */ "32 MB",
/* 9 */ "64 MB",
/* 10*/ "128 MB",
/* 11*/ "256 MB",
/* 12*/ "512 MB",
/* 13*/ "1 GB",
/* 14*/ "1.5MB",
};

static unsigned long memsizes[] = {
/* 0 */ 0,
/* 1 */ 0x00040000, /*  256-K */
/* 2 */ 0x00080000, /*  512-K */
/* 3 */ 0x00100000, /*  1-meg */
/* 4 */ 0x00200000, /*  2-meg */
/* 5 */ 0x00400000, /*  4-meg */
/* 6 */ 0x00800000, /*  8-meg */
/* 7 */ 0x01000000, /* 16-meg */
/* 8 */ 0x02000000, /* 32-meg */
/* 9 */ 0x04000000, /* 64-meg */
/* 10*/ 0x08000000, //128 Meg
/* 11*/ 0x10000000, //256 Meg 
/* 12*/ 0x20000000, //512 Meg The correct size is set in mman.c
/* 13*/ 0x40000000, //1GB
/* 14*/ 0x00180000, //1.5MB
};

static int msi_chip[] = { 1, 2, 3, 4, 5, 6 };
static int msi_bogo[] = { 0, 2, 3, 14, 4 };
static int msi_fast[] = { 0, 3, 4, 5, 6 };
static int msi_z3fast[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13 };
static int msi_gfx[] = { 0, 3, 4, 5, 6,7,8};

static int CalculateHardfileSize (HWND hDlg)
{
    BOOL Translated = FALSE;
    UINT mbytes = 0;

    mbytes = GetDlgItemInt( hDlg, IDC_HFSIZE, &Translated, FALSE );
    if (mbytes <= 0)
	mbytes = 0;
    if( !Translated )
        mbytes = 0;
    return mbytes;
}

static void updatepage (int id)
{
    if(!pages[id])
	return;
    SendMessage(pages[id], WM_USER, 0, 0);
}

static const char *nth[] = {
    "", "second ", "third ", "fourth ", "fifth ", "sixth ", "seventh ", "eighth ", "ninth ", "tenth "
};

ConfigStructPtr AllocateConfigStruct( void )
{
    return xmalloc( sizeof( ConfigStruct ) );
}

void FreeConfigStruct( ConfigStructPtr cfgptr )
{
    free( cfgptr );
}

static ConfigStructPtr addconfigentry (char *start_path, LPWIN32_FIND_DATA find_data)
{
    char init_path[ MAX_PATH ] = "", *posn;
    char description[ CFG_DESCRIPTION_LENGTH ] = "";

    ConfigStructPtr config = AllocateConfigStruct();
    sprintf( init_path, "%s\\Configurations\\%s", start_path, find_data->cFileName );
    if( cfgfile_get_description( init_path, description ) ) {
	if( ( posn = strrchr( find_data->cFileName, '.' ) ) != NULL )
	    *posn = '\0';
	strcpy( config->Name, find_data->cFileName );
	strcpy( config->Description, description );
    } else {
	FreeConfigStruct( config );
	config = NULL;
    }
    return config;
}

static ConfigStructPtr GetFirstConfigEntry( HANDLE *file_handle, LPWIN32_FIND_DATA find_data )
{
    DWORD num_bytes = 0;
    char init_path[ MAX_PATH ] = "";
    ConfigStructPtr config = NULL;

    if( start_path )
    {
        strncpy( init_path, start_path, MAX_PATH );
        strncat( init_path, "\\Configurations\\*.UAE", MAX_PATH );
    }

    if( ( *file_handle = FindFirstFile( init_path, find_data ) ) != INVALID_HANDLE_VALUE )
    {
	config = addconfigentry (start_path, find_data);
    }
    else
    {
        // Either the directory has no .CFG files, or doesn't exist.

        // Create the directory, even if it already exists.  No harm, and don't check return codes, because
        // we may be doing this on a read-only media like CD-ROM.
        sprintf( init_path, "%s\\Configurations", start_path );
        CreateDirectory( init_path, NULL );
    }
    return config;
}

static ConfigStructPtr GetNextConfigEntry( HANDLE *file_handle, LPWIN32_FIND_DATA find_data )
{
    ConfigStructPtr config = NULL;
    char init_path[ MAX_PATH ] = "";
    char desc[ CFG_DESCRIPTION_LENGTH ] = "";

    if( FindNextFile( *file_handle, find_data ) == 0 )
    {
        FindClose( *file_handle );
    }
    else
    {
	config = addconfigentry (start_path, find_data);
    }
    return config;
}

static char *HandleConfiguration( HWND hDlg, int flag )
{
    static char full_path[MAX_PATH];
    char file_name[MAX_PATH] = "";
    char init_path[MAX_PATH] = "";
    ConfigStructPtr cfgptr = NULL;
    int i;

    full_path[0] = 0;
    if( ( cfgptr = AllocateConfigStruct() ) != NULL )
    {
        switch( flag )
        {
            case CONFIG_SAVE_FULL:
                DiskSelection( hDlg, IDC_SAVE, 5, &workprefs );
                GetDlgItemText( hDlg, IDC_EDITNAME, full_path, MAX_PATH );
                GetDlgItemText( hDlg, IDC_EDITDESCRIPTION, workprefs.description, 256 );
                cfgfile_save( &workprefs, full_path );
            break;

            case CONFIG_LOAD_FULL:
                DiskSelection( hDlg, IDC_LOAD, 4, &workprefs );
                for( i = 0; i < C_PAGES; i++ )
		    SendMessage( pages[i], WM_USER, 0, 0 );
		EnableWindow( GetDlgItem( hDlg, IDC_VIEWINFO ), workprefs.info[0] );
            break;
            
            case CONFIG_SAVE:
                GetDlgItemText( hDlg, IDC_EDITNAME, cfgptr->Name, MAX_PATH );
                if( cfgptr->Name[0] == '\0' )
                {
		    char szMessage[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_MUSTENTERNAME, szMessage, MAX_PATH );
		    gui_message( szMessage );
                }
                else
                {
                    if( !strchr( cfgptr->Name, '\\' ) && !strchr( cfgptr->Name, '/' ) )
                    {
                        if( start_path )
                        {
                            strncpy( init_path, start_path, MAX_PATH );
                            strncat( init_path, "\\Configurations\\", MAX_PATH );
                        }

                        sprintf( full_path, "%s%s.UAE", init_path, cfgptr->Name );
                    }
                    else
                    {
                        strcpy( full_path, cfgptr->Name );
                    }
                    GetDlgItemText( hDlg, IDC_EDITDESCRIPTION, workprefs.description, 256 );
                    cfgfile_save( &workprefs, full_path );
                }
            break;
        
            case CONFIG_LOAD:
                GetDlgItemText( hDlg, IDC_EDITNAME, cfgptr->Name, MAX_PATH );
                if( cfgptr->Name[0] == '\0' )
                {
		    char szMessage[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_MUSTSELECTCONFIG, szMessage, MAX_PATH );
		    gui_message( szMessage );
                }
                else
                {
                    if( start_path )
                    {
                        strncpy( init_path, start_path, MAX_PATH );
                        strncat( init_path, "\\Configurations\\", MAX_PATH );
                    }

                    sprintf( full_path, "%s%s.UAE", init_path, cfgptr->Name );
                    strcpy( config_filename, cfgptr->Name );

                    if( cfgfile_doload( &workprefs, full_path ) == 0 )
                    {
			char szMessage[ MAX_PATH ];
			WIN32GUI_LoadUIString( IDS_COULDNOTLOADCONFIG, szMessage, MAX_PATH );
                        gui_message( szMessage );
                    }

                    for( i = 0; i < C_PAGES; i++ )
			SendMessage( pages[i], WM_USER, 0, 0 );
		    EnableWindow( GetDlgItem( hDlg, IDC_VIEWINFO ), workprefs.info[0] );
                }
            break;

            case CONFIG_DELETE:
                GetDlgItemText( hDlg, IDC_EDITNAME, cfgptr->Name, MAX_PATH );
                if( cfgptr->Name[0] == '\0' )
                {
		    char szMessage[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_MUSTSELECTCONFIGFORDELETE, szMessage, MAX_PATH );
		    gui_message( szMessage );
                }
                else
                {
		    char szMessage[ MAX_PATH ];
		    char szTitle[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_DELETECONFIGCONFIRMATION, szMessage, MAX_PATH );
		    WIN32GUI_LoadUIString( IDS_DELETECONFIGTITLE, szTitle, MAX_PATH );
                    if( MessageBox( hDlg, szMessage, szTitle,
                                    MB_YESNO | MB_ICONWARNING | MB_APPLMODAL | MB_SETFOREGROUND ) == IDYES )
                    {
                        if( start_path )
                        {
                            strncpy( init_path, start_path, MAX_PATH );
                            strncat( init_path, "\\Configurations\\", MAX_PATH );
                        }

                        sprintf( full_path, "%s%s.UAE", init_path, cfgptr->Name );
                        DeleteFile( full_path );
                    }
                }
            break;
        }
        FreeConfigStruct( cfgptr );
    }
    return full_path;
}


static int input_selected_device, input_selected_widget;
static int input_selected_event, input_selected_sub_num;

static void set_lventry_input (HWND list, int index)
{
    int flags, i, sub;
    char name[256];
    char af[10];

    inputdevice_get_mapped_name (input_selected_device, index, &flags, name, input_selected_sub_num);
    if (flags & IDEV_MAPPED_AUTOFIRE_SET)
	strcpy (af, "yes");
    else if (flags & IDEV_MAPPED_AUTOFIRE_POSSIBLE)
	strcpy (af, "no");
    else
	strcpy (af,"-");
    ListView_SetItemText(list, index, 1, name);
    ListView_SetItemText(list, index, 2, af);
    sub = 0;
    for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
	if (inputdevice_get_mapped_name (input_selected_device, index, &flags, name, i)) sub++;
    }
    sprintf (name, "%d", sub);
    ListView_SetItemText(list, index, 3, name);
}

static void update_listview_input (HWND hDlg)
{
    int i;
    for (i = 0; i < inputdevice_get_widget_num (input_selected_device); i++)
	set_lventry_input (GetDlgItem (hDlg, IDC_INPUTLIST), i);
}
	    
static int clicked_entry = -1;

#define LOADSAVE_COLUMNS 2
#define INPUT_COLUMNS 4
#define HARDDISK_COLUMNS 7
#define MAX_COLUMN_HEADING_WIDTH 20

#define LV_LOADSAVE 1
#define LV_HARDDISK 2
#define LV_INPUT 3

void InitializeListView( HWND hDlg )
{
    int lv_type;
    HANDLE file_handle = NULL;
    WIN32_FIND_DATA find_data;
    BOOL rc = TRUE;
    HWND list;
    LV_ITEM lvstruct;
    LV_COLUMN lvcolumn;
    RECT rect;
    int num_columns;
    char column_heading[ HARDDISK_COLUMNS ][ MAX_COLUMN_HEADING_WIDTH ];
    char blocksize_str[6] = "";
    char readwrite_str[4] = "";
    char size_str[32] = "";
    char volname_str[ MAX_PATH ] = "";
    char devname_str[ MAX_PATH ] = "";
    char bootpri_str[6] = "";
    int width = 0, column_width[ HARDDISK_COLUMNS ];
    int items = 0, result = 0, i, entry = 0, temp = 0;
    ConfigStructPtr config = NULL;

    if (hDlg == pages[LOADSAVE_ID]) {
	num_columns = LOADSAVE_COLUMNS;
	lv_type = LV_LOADSAVE;
	WIN32GUI_LoadUIString( IDS_NAME, column_heading[0], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_DESCRIPTION, column_heading[1], MAX_COLUMN_HEADING_WIDTH );
        list = GetDlgItem( hDlg, IDC_CONFIGLIST );
    } else if (hDlg == pages[HARDDISK_ID]) {
	num_columns = HARDDISK_COLUMNS;
	lv_type = LV_HARDDISK;
	WIN32GUI_LoadUIString( IDS_DEVICE, column_heading[0], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_VOLUME, column_heading[1], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_PATH, column_heading[2], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_RW, column_heading[3], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_BLOCKSIZE, column_heading[4], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_HFDSIZE, column_heading[5], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_BOOTPRI, column_heading[6], MAX_COLUMN_HEADING_WIDTH );
        list = GetDlgItem( hDlg, IDC_VOLUMELIST );
    } else {
	num_columns = INPUT_COLUMNS;
	lv_type = LV_INPUT;
	WIN32GUI_LoadUIString( IDS_INPUTHOSTWIDGET, column_heading[0], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_INPUTAMIGAEVENT, column_heading[1], MAX_COLUMN_HEADING_WIDTH );
	WIN32GUI_LoadUIString( IDS_INPUTAUTOFIRE, column_heading[2], MAX_COLUMN_HEADING_WIDTH );
	strcpy (column_heading[3], "#");
        list = GetDlgItem( hDlg, IDC_INPUTLIST );
    }

    ListView_DeleteAllItems( list );

    for( i = 0; i < num_columns; i++ )
        column_width[i] = ListView_GetStringWidth( list, column_heading[i] ) + 15;

    // If there are no columns, then insert some
    lvcolumn.mask = LVCF_WIDTH;
    if( ListView_GetColumn( list, 1, &lvcolumn ) == FALSE )
    {
        for( i = 0; i < num_columns; i++ )
        {
            lvcolumn.mask     = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvcolumn.iSubItem = i;
            lvcolumn.fmt      = LVCFMT_LEFT;
            lvcolumn.pszText  = column_heading[i];
            lvcolumn.cx       = column_width[i];
            ListView_InsertColumn( list, i, &lvcolumn );
        }
    }
    if( lv_type == LV_LOADSAVE )
    {
        if( ( config = GetFirstConfigEntry( &file_handle, &find_data ) ) != NULL )
        {
            while( config )
            {
                lvstruct.mask     = LVIF_TEXT | LVIF_PARAM;
                lvstruct.pszText  = config->Name;
                lvstruct.lParam   = 0;
                lvstruct.iItem    = entry;
                lvstruct.iSubItem = 0;
                result = ListView_InsertItem( list, &lvstruct );
                if( result != -1 )
                {
                    width = ListView_GetStringWidth( list, lvstruct.pszText ) + 15;
                    if( width > column_width[ lvstruct.iSubItem ] )
                        column_width[ lvstruct.iSubItem ] = width;

                    ListView_SetItemText( list, result, 1, config->Description );
                    width = ListView_GetStringWidth( list, config->Description ) + 15;
                    if( width > column_width[ 1 ] )
                        column_width[ 1 ] = width;

                    entry++;
                }
                FreeConfigStruct( config );
                config = GetNextConfigEntry( &file_handle, &find_data );
            }
        }
    }
    else if (lv_type == LV_INPUT)
    {
	for (i = 0; i < inputdevice_get_widget_num (input_selected_device); i++) {
	    char name[100];
	    inputdevice_get_widget_type (input_selected_device, i, name);
	    lvstruct.mask     = LVIF_TEXT | LVIF_PARAM;
	    lvstruct.pszText  = name;
	    lvstruct.lParam   = 0;
	    lvstruct.iItem    = i;
	    lvstruct.iSubItem = 0;
	    result = ListView_InsertItem( list, &lvstruct );
	    width = ListView_GetStringWidth( list, lvstruct.pszText ) + 15;
	    if( width > column_width[ 0 ] )
		column_width[ 0 ] = width;
	    entry++;
	}
	column_width [1] = 250;
	column_width [2] = 60;
	column_width [3] = 60;
	update_listview_input (hDlg);
    }
    else if (lv_type == LV_HARDDISK)
    {
#ifdef FILESYS
        for( i = 0; i < nr_units( currprefs.mountinfo ); i++ )
        {
	    int secspertrack, surfaces, reserved, blocksize, bootpri;
	    uae_u64 size;
	    int cylinders, readonly, type;
	    char *volname, *devname, *rootdir;
            char *failure;

	    failure = get_filesys_unit (currprefs.mountinfo, i,
				        &devname, &volname, &rootdir, &readonly,
					&secspertrack, &surfaces, &reserved,
					&cylinders, &size, &blocksize, &bootpri, 0);
	    type = is_hardfile (currprefs.mountinfo, i);
	    
	    if (size >= 1024 * 1024 * 1024)
	        sprintf (size_str, "%.1fG", ((double)(uae_u32)(size / (1024 * 1024))) / 1024.0);
	    else
	        sprintf (size_str, "%.1fM", ((double)(uae_u32)(size / (1024))) / 1024.0);
            if (type == FILESYS_HARDFILE) {
	        sprintf (blocksize_str, "%d", blocksize);
	        strcpy (devname_str, devname);
	        strcpy (volname_str, "n/a");
		sprintf (bootpri_str, "%d", bootpri);
            } else if (type == FILESYS_HARDFILE_RDB || type == FILESYS_HARDDRIVE) {
                sprintf (blocksize_str, "%d", blocksize);
	        strcpy (devname_str, "n/a");
		strcpy (volname_str, "n/a");
		strcpy (bootpri_str, "n/a");
	    } else {
                strcpy (blocksize_str, "n/a");
	        strcpy (devname_str, devname);
	        strcpy (volname_str, volname);
		strcpy (size_str, "n/a");
		sprintf (bootpri_str, "%d", bootpri);
            }
            sprintf( readwrite_str, "%s", readonly ? "no" : "yes" );

	    lvstruct.mask     = LVIF_TEXT | LVIF_PARAM;
            lvstruct.pszText  = devname_str;
            lvstruct.lParam   = 0;
            lvstruct.iItem    = i;
            lvstruct.iSubItem = 0;
            result = ListView_InsertItem (list, &lvstruct);
            if (result != -1) {
                width = ListView_GetStringWidth( list, devname_str) + 15;
                if( width > column_width[0] )
                    column_width[0] = width;

		ListView_SetItemText( list, result, 1, volname_str );
                width = ListView_GetStringWidth( list, volname_str ) + 15;
                if( width > column_width[ 1 ] )
                    column_width[ 1 ] = width;

		column_width [ 2 ] = 150;
                ListView_SetItemText( list, result, 2, rootdir );
                width = ListView_GetStringWidth( list, rootdir ) + 15;
                if( width > column_width[ 2 ] )
                    column_width[ 2 ] = width;

		ListView_SetItemText( list, result, 3, readwrite_str );
                width = ListView_GetStringWidth( list, readwrite_str ) + 15;
                if( width > column_width[ 3 ] )
                    column_width[ 3 ] = width;

		ListView_SetItemText( list, result, 4, blocksize_str );
                width = ListView_GetStringWidth( list, blocksize_str ) + 15;
                if( width > column_width[ 4 ] )
                    column_width[ 4 ] = width;

		ListView_SetItemText( list, result, 5, size_str );
                width = ListView_GetStringWidth( list, size_str ) + 15;
                if( width > column_width[ 5 ] )
                    column_width[ 5 ] = width;

		ListView_SetItemText( list, result, 6, bootpri_str );
                width = ListView_GetStringWidth( list, bootpri_str ) + 15;
                if( width > column_width[ 6 ] )
                    column_width[ 6 ] = width;
	    }
        }
#endif
    }
    if( rc == FALSE )
    {
        FreeConfigStruct( config );
    }

    if( result != -1 )
    {
        if( GetWindowRect( list, &rect ) )
        {
            ScreenToClient( hDlg, (LPPOINT)&rect );
            ScreenToClient( hDlg, (LPPOINT)&rect.right );
            if( num_columns == 2 )
            {
                if( ( temp = rect.right - rect.left - column_width[ 0 ] - 4 ) > column_width[1] )
                    column_width[1] = temp;
            }
        }

        // Adjust our column widths so that we can see the contents...
        for( i = 0; i < num_columns; i++ )
        {
            ListView_SetColumnWidth( list, i, column_width[i] );
        }

        // Turn on full-row-select option
        ListView_SetExtendedListViewStyle( list, LVS_EX_FULLROWSELECT );

        // Redraw the items in the list...
        items = ListView_GetItemCount( list );
        ListView_RedrawItems( list, 0, items );
    }
}

static int listview_find_selected (HWND list)
{
    int i, items;
    items = ListView_GetItemCount (list);
    for (i = 0; i < items; i++) {
	if (ListView_GetItemState (list, i, LVIS_SELECTED) == LVIS_SELECTED)
	    return i;
    }
    return -1;
}

static int listview_entry_from_click (HWND list)
{
    POINT point;
    DWORD pos = GetMessagePos ();
    int items, entry;

    point.x = LOWORD (pos);
    point.y = HIWORD (pos);
    ScreenToClient (list, &point);
    entry = ListView_GetTopIndex (list);
    items = entry + ListView_GetCountPerPage (list);
    if (items > ListView_GetItemCount (list))
	items = ListView_GetItemCount (list);

    while (entry <= items) {
	RECT rect;
	/* Get the bounding rectangle of an item. If the mouse
	 * location is within the bounding rectangle of the item,
	 * you know you have found the item that was being clicked.  */
	ListView_GetItemRect (list, entry, &rect, LVIR_BOUNDS);
	if (PtInRect (&rect, point)) {
	    UINT flag = LVIS_SELECTED | LVIS_FOCUSED;
	    ListView_SetItemState (list, entry, flag, flag);
	    return entry;
	}
	entry++;
    }
    return -1;
}

static int CALLBACK InfoSettingsProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    
    switch (msg) 
    {
	case WM_INITDIALOG:
	    recursive++;
	    SetDlgItemText (hDlg, IDC_PATH_NAME, workprefs.info);
	    recursive--;
	return TRUE;
	
	case WM_COMMAND:
	    if (recursive)
		break;
	    recursive++;
	
	    switch( wParam ) 
	    {
		case IDC_SELECTOR:
		    DiskSelection (hDlg, IDC_PATH_NAME, 8, &workprefs );
		break;
		case IDOK:
		    EndDialog (hDlg, 1);
		break;
		case IDCANCEL:
		    EndDialog (hDlg, 0);
		break;
	    }
	
	    GetDlgItemText( hDlg, IDC_PATH_NAME, workprefs.info, sizeof workprefs.info );
	    recursive--;
	break;
    }
    return FALSE;
}

static BOOL CALLBACK LoadSaveDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char name_buf[MAX_PATH] = "", desc_buf[CFG_DESCRIPTION_LENGTH] = "";
    char *posn = NULL;
    HWND list;
    int dblclick = 0;
    NM_LISTVIEW *nmlistview;
    int items = 0, entry = 0;
    LPHELPINFO lpHelpInfo;
    char *cfgfile;
    
    switch (msg) {
    case WM_INITDIALOG:
	pages[LOADSAVE_ID] = hDlg;
	currentpage = LOADSAVE_ID;
	InitializeListView(hDlg);
	EnableWindow( GetDlgItem( hDlg, IDC_VIEWINFO ), workprefs.info[0] );
	SetDlgItemText( hDlg, IDC_EDITNAME, config_filename );
	SetDlgItemText( hDlg, IDC_EDITDESCRIPTION, workprefs.description );
	return TRUE;
	
    case WM_USER:
	break;
	
    case WM_HELP:
        lpHelpInfo = (LPHELPINFO)lParam;
        break;
	
    case WM_COMMAND:
        switch (wParam) 
        {
	    case IDC_EXIT:
		if (full_property_sheet) {
		    uae_quit();
		    exit_gui (0);
		} else {
		    uae_restart (1, 0);
		    exit_gui(1);
		}
	    break;
	    case IDC_SAVE:
		HandleConfiguration( hDlg, CONFIG_SAVE_FULL );
		InitializeListView( hDlg );
	    break;
	    case IDC_QUICKSAVE:
		HandleConfiguration( hDlg, CONFIG_SAVE );
		InitializeListView( hDlg );
	    break;
	    case IDC_QUICKLOAD:
	        cfgfile = HandleConfiguration( hDlg, CONFIG_LOAD );
		if (full_property_sheet) {
		    inputdevice_updateconfig (&workprefs);
		} else {
		    uae_restart (1, cfgfile);
		    exit_gui(1);
		}
	    break;
	    case IDC_LOAD:
		cfgfile = HandleConfiguration( hDlg, CONFIG_LOAD_FULL );
		if (full_property_sheet) {
		    inputdevice_updateconfig (&workprefs);
		} else {
		    uae_restart (1, cfgfile);
		    exit_gui(1);
		}
	    break;
	    case IDC_DELETE:
		HandleConfiguration( hDlg, CONFIG_DELETE );
		InitializeListView( hDlg );
	    break;
	    case IDC_VIEWINFO:
		if( workprefs.info[0] )
		{
		    if( strstr( workprefs.info, "Configurations\\" ) )
			sprintf( name_buf, "%s\\%s", start_path, workprefs.info );
		    else
			strcpy( name_buf, workprefs.info );
		    ShellExecute( NULL, NULL, name_buf, NULL, NULL, SW_SHOWNORMAL );
		}
	    break;
	    case IDC_SETINFO:
		if( DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_SETINFO), hDlg, InfoSettingsProc ) )
		{
		    EnableWindow( GetDlgItem( hDlg, IDC_VIEWINFO ), workprefs.info[0] );
		}
	    break;
	}
	break;
	
	case WM_NOTIFY:
	    if (((LPNMHDR) lParam)->idFrom == IDC_CONFIGLIST) 
	    {
		nmlistview = (NM_LISTVIEW *) lParam;
		list = nmlistview->hdr.hwndFrom;
		
		switch (nmlistview->hdr.code) 
		{
		case NM_DBLCLK:
		    dblclick = 1;
		    /* fall-through */
		case NM_CLICK:
		    entry = listview_entry_from_click (list);
		    /* Copy the item's name and description to the gadgets at the bottom... */
		    if (entry >= 0) 
                    {
			ListView_GetItemText (list, entry, 0, name_buf, MAX_PATH);
			ListView_GetItemText (list, entry, 1, desc_buf, 128);
			SetDlgItemText (hDlg, IDC_EDITNAME, name_buf);
			SetDlgItemText (hDlg, IDC_EDITDESCRIPTION, desc_buf);
			ListView_RedrawItems (list, 0, items);
			
			if (dblclick) 
                        {
                            cfgfile = HandleConfiguration( hDlg, CONFIG_LOAD );
			    if (!full_property_sheet)
			        uae_restart (0, cfgfile);
			    exit_gui (1);
			}
		    }
		    break;
		}
	    }
	    else
	    {
		return notifycheck (lParam, "gui/configurations.htm");
	    }
	    break;
    }
    
    return FALSE;
}

#define MAX_CONTRIBUTORS_LENGTH 2048

static int CALLBACK ContributorsProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CHARFORMAT CharFormat;
    char szContributors1[ MAX_CONTRIBUTORS_LENGTH ];
    char szContributors2[ MAX_CONTRIBUTORS_LENGTH ];
    char szContributors[ MAX_CONTRIBUTORS_LENGTH*2 ];

    switch (msg) {
     case WM_COMMAND:
	if (wParam == ID_OK) {
	    EndDialog (hDlg, 1);
	    return TRUE;
	}
	break;
     case WM_INITDIALOG:
	CharFormat.cbSize = sizeof (CharFormat);

	WIN32GUI_LoadUIString( IDS_CONTRIBUTORS1, szContributors1, MAX_CONTRIBUTORS_LENGTH );
	WIN32GUI_LoadUIString( IDS_CONTRIBUTORS2, szContributors2, MAX_CONTRIBUTORS_LENGTH );
	sprintf( szContributors, "%s%s", szContributors1, szContributors2 );

	SetDlgItemText (hDlg, IDC_CONTRIBUTORS, szContributors );
	SendDlgItemMessage (hDlg, IDC_CONTRIBUTORS, EM_GETCHARFORMAT, 0, (LPARAM) & CharFormat);
	CharFormat.dwMask |= CFM_SIZE | CFM_FACE;
	CharFormat.yHeight = 10 * 20;	/* height in twips, where a twip is 1/20th of a point - for a pt.size of 18 */

	strcpy (CharFormat.szFaceName, "Times New Roman");
	SendDlgItemMessage (hDlg, IDC_CONTRIBUTORS, EM_SETCHARFORMAT, SCF_ALL, (LPARAM) & CharFormat);
	/* SendDlgItemMessage(hDlg, IDC_CONTRIBUTORS, EM_SETBKGNDCOLOR,0,GetSysColor( COLOR_3DFACE ) ); */

	return TRUE;
    }
    return FALSE;
}

static void DisplayContributors (HWND hDlg)
{
    DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_CONTRIBUTORS), hDlg, ContributorsProc);
}

#define NUM_URLS 7
typedef struct url_info
{
    int   id;
    BOOL  state;
    char *display;
    char *url;
} urlinfo;

static urlinfo urls[NUM_URLS] = 
{
    {IDC_CLOANTOHOME, FALSE, "Cloanto's Amiga Forever", "http://www.cloanto.com/amiga/forever/"},
    {IDC_AMIGAHOME, FALSE, "Amiga Inc.", "http://www.amiga.com"},
    {IDC_PICASSOHOME, FALSE, "Picasso96 Home Page", "http://www.picasso96.cogito.de/"}, 
    {IDC_UAEHOME, FALSE, "UAE Home Page", "http://www.freiburg.linux.de/~uae/"},
    {IDC_WINUAEHOME, FALSE, "WinUAE Home Page", "http://www.winuae.net/"},
    {IDC_AIABHOME, FALSE, "AIAB", "http://aiab.emuunlim.com/"},
    {IDC_THEROOTS, FALSE, "Back To The Roots", "http://back2roots.emuunlim.com/"}
};

static void SetupRichText( HWND hDlg, urlinfo *url )
{
    CHARFORMAT CharFormat;
    CharFormat.cbSize = sizeof (CharFormat);

    SetDlgItemText( hDlg, url->id, url->display );
    SendDlgItemMessage( hDlg, url->id, EM_GETCHARFORMAT, 0, (LPARAM)&CharFormat );
    CharFormat.dwMask   |= CFM_UNDERLINE | CFM_SIZE | CFM_FACE | CFM_COLOR;
    CharFormat.dwEffects = url->state ? CFE_UNDERLINE : 0;
    CharFormat.yHeight = 10 * 20;	/* height in twips, where a twip is 1/20th of a point - for a pt.size of 18 */

    CharFormat.crTextColor = GetSysColor( COLOR_ACTIVECAPTION );
    strcpy( CharFormat.szFaceName, "Tahoma" );
    SendDlgItemMessage( hDlg, url->id, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&CharFormat );
    SendDlgItemMessage( hDlg, url->id, EM_SETBKGNDCOLOR, 0, GetSysColor( COLOR_3DFACE ) );
}

static void url_handler(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static int last_rectangle = -1;
    int i;
    BOOL found = FALSE;
    HCURSOR m_hCursor = NULL;
    POINT point;
    point.x = LOWORD (lParam);
    point.y = HIWORD (lParam);
    
    for (i = 0; i < NUM_URLS; i++) 
    {
	RECT rect;
	GetWindowRect( GetDlgItem( hDlg, urls[i].id), &rect );
	ScreenToClient( hDlg, (POINT *) &rect );
	ScreenToClient( hDlg, (POINT *) &(rect.right) );
	if( PtInRect( &rect, point ) ) 
        {
            if( msg == WM_LBUTTONDOWN )
            {
		ShellExecute (NULL, NULL, urls[i].url , NULL, NULL, SW_SHOWNORMAL);
                SetCursor( LoadCursor( NULL, MAKEINTRESOURCE(IDC_ARROW) ) );
            }
            else
            {
                if( ( i != last_rectangle ) )
                {
		    // try and load the system hand (Win2000+)
		    m_hCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND) );
		    if (!m_hCursor)
		    {
			// retry with our fallback hand
			m_hCursor = LoadCursor(hInst, MAKEINTRESOURCE(IDC_MYHAND) );
		    }
                    SetCursor( m_hCursor );
                    urls[i].state = TRUE;
                    SetupRichText( hDlg, &urls[i] );

		    if( last_rectangle != -1 )
		    {
			urls[last_rectangle].state = FALSE;
			SetupRichText( hDlg, &urls[last_rectangle] );
		    }
                }
            }
	    last_rectangle = i;
            found = TRUE;
	    break;
	}
    }

    if( !found && last_rectangle >= 0 )
    {
        SetCursor( LoadCursor( NULL, MAKEINTRESOURCE(IDC_ARROW) ) );
        urls[last_rectangle].state = FALSE;
        SetupRichText( hDlg, &urls[last_rectangle] );
	last_rectangle = -1;
    }
}

static void init_aboutdlg (HWND hDlg)
{
    CHARFORMAT CharFormat;
    int i;

    CharFormat.cbSize = sizeof (CharFormat);

    SetDlgItemText (hDlg, IDC_RICHEDIT1, "WinUAE");
    SendDlgItemMessage (hDlg, IDC_RICHEDIT1, EM_GETCHARFORMAT, 0, (LPARAM) & CharFormat);
    CharFormat.dwMask |= CFM_BOLD | CFM_SIZE | CFM_FACE;
    CharFormat.dwEffects = CFE_BOLD;
    CharFormat.yHeight = 18 * 20;	/* height in twips, where a twip is 1/20th of a point - for a pt.size of 18 */

    strcpy (CharFormat.szFaceName, "Times New Roman");
    SendDlgItemMessage (hDlg, IDC_RICHEDIT1, EM_SETCHARFORMAT, SCF_ALL, (LPARAM) & CharFormat);
    SendDlgItemMessage (hDlg, IDC_RICHEDIT1, EM_SETBKGNDCOLOR, 0, GetSysColor (COLOR_3DFACE));

    SetDlgItemText (hDlg, IDC_RICHEDIT2, VersionStr );
    SendDlgItemMessage (hDlg, IDC_RICHEDIT2, EM_GETCHARFORMAT, 0, (LPARAM) & CharFormat);
    CharFormat.dwMask |= CFM_SIZE | CFM_FACE;
    CharFormat.yHeight = 10 * 20;
    strcpy (CharFormat.szFaceName, "Times New Roman");
    SendDlgItemMessage (hDlg, IDC_RICHEDIT2, EM_SETCHARFORMAT, SCF_ALL, (LPARAM) & CharFormat);
    SendDlgItemMessage (hDlg, IDC_RICHEDIT2, EM_SETBKGNDCOLOR, 0, GetSysColor (COLOR_3DFACE));

    for( i = 0; i < NUM_URLS; i++ )
    {
        SetupRichText( hDlg, &urls[i] );
    }
}

static BOOL CALLBACK AboutDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch( msg )
    {
	case WM_INITDIALOG:
	    pages[ABOUT_ID] = hDlg;
	    currentpage = ABOUT_ID;
	    init_aboutdlg (hDlg);
	    break;
	    
	case WM_COMMAND:
	    if (wParam == IDC_CONTRIBUTORS) 
	    {
		DisplayContributors (hDlg);
	    }
	    break;
	case WM_SETCURSOR:
	    return TRUE;
	    break;
	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
	    url_handler( hDlg, msg, wParam, lParam );
	    break;
	case WM_NOTIFY:
	    return notifycheck (lParam, NULL);
    }
    
    return FALSE;
}

static void enable_for_displaydlg (HWND hDlg)
{
    int rtg = ! workprefs.address_space_24;
#ifndef PICASSO96
    rtg = FALSE;
#endif
    EnableWindow( GetDlgItem( hDlg, IDC_PFULLSCREEN ), rtg);
    if (! full_property_sheet) 
    {
	/* Disable certain controls which are only to be set once at start-up... */
        EnableWindow (GetDlgItem (hDlg, IDC_TEST16BIT), FALSE);
    }
    else
    {
        CheckDlgButton( hDlg, IDC_VSYNC, workprefs.gfx_vsync);
        if (workprefs.gfx_filter) {
	    EnableWindow (GetDlgItem (hDlg, IDC_XCENTER), FALSE);
	    EnableWindow (GetDlgItem (hDlg, IDC_YCENTER), FALSE);
	    EnableWindow (GetDlgItem (hDlg, IDC_LM_SCANLINES), FALSE);
	    if (workprefs.gfx_linedbl == 2)
		workprefs.gfx_linedbl = 1;
	    workprefs.gfx_xcenter = workprefs.gfx_ycenter = 0;
	} else {
	    EnableWindow (GetDlgItem (hDlg, IDC_XCENTER), TRUE);
	    EnableWindow (GetDlgItem (hDlg, IDC_YCENTER), TRUE);
	    EnableWindow (GetDlgItem (hDlg, IDC_LM_SCANLINES), TRUE);
	}
    }
}

static void enable_for_chipsetdlg (HWND hDlg)
{
    int enable = workprefs.cpu_cycle_exact ? FALSE : TRUE;
#if !defined (CPUEMU_6)
    EnableWindow (GetDlgItem (hDlg, IDC_CYCLEEXACT), FALSE);
#endif
    EnableWindow (GetDlgItem (hDlg, IDC_FASTCOPPER), enable);
    EnableWindow (GetDlgItem (hDlg, IDC_BLITIMM), enable);
    if (enable == FALSE) {
	workprefs.fast_copper = 0;
	workprefs.immediate_blits = 0;
	CheckDlgButton (hDlg, IDC_FASTCOPPER, FALSE);
	CheckDlgButton (hDlg, IDC_BLITIMM, FALSE);
    }
}

static void LoadNthString( DWORD value, char *nth, DWORD dwNthMax )
{
    switch( value )
    {
	case 1:
	    WIN32GUI_LoadUIString( IDS_SECOND, nth, dwNthMax );
	break;

	case 2:
	    WIN32GUI_LoadUIString( IDS_THIRD, nth, dwNthMax );	
	break;
	
	case 3:
	    WIN32GUI_LoadUIString( IDS_FOURTH, nth, dwNthMax );	
	break;
	
	case 4:
	    WIN32GUI_LoadUIString( IDS_FIFTH, nth, dwNthMax );	
	break;
	
	case 5:
	    WIN32GUI_LoadUIString( IDS_SIXTH, nth, dwNthMax );	
	break;
	
	case 6:
	    WIN32GUI_LoadUIString( IDS_SEVENTH, nth, dwNthMax );	
	break;
	
	case 7:
	    WIN32GUI_LoadUIString( IDS_EIGHTH, nth, dwNthMax );	
	break;
	
	case 8:
	    WIN32GUI_LoadUIString( IDS_NINTH, nth, dwNthMax );	
	break;
	
	case 9:
	    WIN32GUI_LoadUIString( IDS_TENTH, nth, dwNthMax );	
	break;
	
	default:
	    strcpy( nth, "" );
    }
}

static int fakerefreshrates[] = { 50, 60, 100, 120, 0 };
static int storedrefreshrates[MAX_REFRESH_RATES + 1];

static void init_frequency_combo (HWND hDlg, int dmode)
{
    int i, j, freq, index, tmp;
    char hz[20], hz2[20];
    
    i = 0; index = 0;
    while ((freq = DisplayModes[dmode].refresh[i]) > 0 && index < MAX_REFRESH_RATES) {
	storedrefreshrates[index++] = freq;
	i++;
    }
    i = 0;
    while ((freq = fakerefreshrates[i]) > 0 && index < MAX_REFRESH_RATES) {
	for (j = 0; j < index; j++) {
	    if (storedrefreshrates[j] == freq) break;
	}
	if (j == index)
	    storedrefreshrates[index++] = -freq;
	i++;
    }
    storedrefreshrates[index] = 0;
    for (i = 0; i < index; i++) {
	for (j = i + 1; j < index; j++) {
	    if (abs(storedrefreshrates[i]) >= abs(storedrefreshrates[j])) {
		tmp = storedrefreshrates[i];
		storedrefreshrates[i] = storedrefreshrates[j];
		storedrefreshrates[j] = tmp;
	    }
	}
    }

    hz[0] = hz2[0] = 0;
    SendDlgItemMessage(hDlg, IDC_REFRESHRATE, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage( hDlg, IDC_REFRESHRATE, CB_ADDSTRING, 0, (LPARAM)"Default");
    for (i = 0; i < index; i++) {
	freq = storedrefreshrates[i];
	if (freq < 0) {
	    freq = -freq;
	    sprintf (hz, "(%dHz)", freq);
	} else {
	    sprintf (hz, "%dHz", freq);
	}
	if (freq == 50 || freq == 100)
	    strcat (hz, " PAL");
	if (freq == 60 || freq == 120)
	    strcat (hz, " NTSC");
	if (workprefs.gfx_refreshrate == freq)
	    strcpy (hz2, hz);
	SendDlgItemMessage( hDlg, IDC_REFRESHRATE, CB_ADDSTRING, 0, (LPARAM)hz);
    }
    index = CB_ERR;
    if (hz2[0] >= 0)
    	index = SendDlgItemMessage( hDlg, IDC_REFRESHRATE, CB_SELECTSTRING, 0, (LPARAM)hz2 );
    if (index == CB_ERR) {
	SendDlgItemMessage( hDlg, IDC_REFRESHRATE, CB_SELECTSTRING, i, (LPARAM)"Default" );
	workprefs.gfx_refreshrate = 0;
    }
}

#define MAX_FRAMERATE_LENGTH 40
#define MAX_NTH_LENGTH 20

static int display_mode_index( uae_u32 x, uae_u32 y, uae_u32 d )
{
    int i;
    i = 0;
    while (DisplayModes[i].depth >= 0) {
        if( DisplayModes[i].res.width == x &&
            DisplayModes[i].res.height == y &&
            DisplayModes[i].depth == d )
            break;
	i++;
    }
    if(DisplayModes[i].depth < 0)
        i = -1;
    return i;
}

#if 0
static int da_mode_selected;

static int *getp_da (void)
{
    int *p = 0;
    switch (da_mode_selected)
    {
	case 0:
	p = &workprefs.gfx_hue;
	break;
	case 1:
	p = &workprefs.gfx_saturation;
	break;
	case 2:
	p = &workprefs.gfx_luminance;
	break;
	case 3:
	p = &workprefs.gfx_contrast;
	break;
	case 4:
	p = &workprefs.gfx_gamma;
	break;
    }
    return p;
}

static void handle_da (HWND hDlg)
{
    int *p;
    int v;

    p = getp_da ();
    if (!p)
	return;
    v = SendDlgItemMessage (hDlg, IDC_DA_SLIDER, TBM_GETPOS, 0, 0) * 10;
    if (v == *p)
	return;
    *p = v;
    currprefs.gfx_hue = workprefs.gfx_hue;
    currprefs.gfx_saturation = workprefs.gfx_saturation;
    currprefs.gfx_luminance = workprefs.gfx_luminance;
    currprefs.gfx_contrast = workprefs.gfx_contrast;
    currprefs.gfx_gamma = workprefs.gfx_gamma;
    init_colors ();
    reset_drawing ();
    redraw_frame ();
    updatedisplayarea ();
}

void init_da (HWND hDlg)
{
    int *p;
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_ADDSTRING, 0, (LPARAM)"Hue");
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_ADDSTRING, 0, (LPARAM)"Saturation");
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_ADDSTRING, 0, (LPARAM)"Luminance");
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_ADDSTRING, 0, (LPARAM)"Contrast");
    SendDlgItemMessage(hDlg, IDC_DA_MODE, CB_ADDSTRING, 0, (LPARAM)"Gamma");
    if (da_mode_selected == CB_ERR)
	da_mode_selected = 0;
    SendDlgItemMessage (hDlg, IDC_DA_MODE, CB_SETCURSEL, da_mode_selected, 0);
    SendDlgItemMessage (hDlg, IDC_DA_SLIDER, TBM_SETPAGESIZE, 0, 1);
    SendDlgItemMessage (hDlg, IDC_DA_SLIDER, TBM_SETRANGE, TRUE, MAKELONG (-99, 99));
    p = getp_da ();
    if (p)
        SendDlgItemMessage (hDlg, IDC_DA_SLIDER, TBM_SETPOS, TRUE, (*p) / 10);
}
#endif

static void values_to_displaydlg (HWND hDlg)
{
    char buffer[ MAX_FRAMERATE_LENGTH + MAX_NTH_LENGTH ];
    char Nth[ MAX_NTH_LENGTH ];
    LPSTR blah[1] = { Nth };
    LPTSTR string = NULL;
    int d, d2, index;

    switch( workprefs.color_mode )
    {
    case 2:
        d = 16;
        break;
    case 5:
        d = 32;
        break;
    default:
        d = 8;
        break;
    }

    if( workprefs.gfx_afullscreen )
    {
        d2 = d;
        if( ( index = WIN32GFX_AdjustScreenmode( &workprefs.gfx_width_fs, &workprefs.gfx_height_fs, &d2 ) ) >= 0 )
        {
            switch( d2 )
            {
            case 15:
                workprefs.color_mode = 1;
                d = 2;
                break;
            case 16:
                workprefs.color_mode = 2;
                d = 2;
                break;
            case 32:
                workprefs.color_mode = 5;
                d = 4;
                break;
            default:
                workprefs.color_mode = 0;
                d = 1;
                break;
            }
        }
    }
    else
    {
        d = d / 8;
    }

    if ((index = display_mode_index (workprefs.gfx_width_fs, workprefs.gfx_height_fs, d)) >= 0) {
        SendDlgItemMessage( hDlg, IDC_RESOLUTION, CB_SETCURSEL, index, 0 );
        init_frequency_combo (hDlg, index);
    }

    SetDlgItemInt( hDlg, IDC_XSIZE, workprefs.gfx_width_win, FALSE );
    SetDlgItemInt( hDlg, IDC_YSIZE, workprefs.gfx_height_win, FALSE );

    SendDlgItemMessage (hDlg, IDC_FRAMERATE, TBM_SETPOS, TRUE, workprefs.gfx_framerate);

    WIN32GUI_LoadUIString( IDS_FRAMERATE, buffer, MAX_FRAMERATE_LENGTH );
    LoadNthString( workprefs.gfx_framerate - 1, Nth, MAX_NTH_LENGTH );
    if( FormatMessage( FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
	               buffer, 0, 0, (LPTSTR)&string, MAX_FRAMERATE_LENGTH + MAX_NTH_LENGTH, (va_list *)blah ) == 0 )
    {
	DWORD dwLastError = GetLastError();
        sprintf (buffer, "Every %s Frame", nth[workprefs.gfx_framerate - 1]);
	SetDlgItemText( hDlg, IDC_RATETEXT, buffer );
    }
    else
    {
	SetDlgItemText( hDlg, IDC_RATETEXT, string );
	LocalFree( string );
    }

    CheckRadioButton( hDlg, IDC_LM_NORMAL, IDC_LM_SCANLINES, IDC_LM_NORMAL + workprefs.gfx_linedbl );
    CheckDlgButton (hDlg, IDC_AFULLSCREEN, workprefs.gfx_afullscreen);
    CheckDlgButton (hDlg, IDC_PFULLSCREEN, workprefs.gfx_pfullscreen);
    CheckDlgButton (hDlg, IDC_ASPECT, workprefs.gfx_correct_aspect);
    CheckDlgButton (hDlg, IDC_LORES, workprefs.gfx_linedbl ? FALSE : TRUE);
    //CheckDlgButton (hDlg, IDC_LORES, workprefs.gfx_lores);
    CheckDlgButton (hDlg, IDC_VSYNC, workprefs.gfx_vsync);
    
    CheckDlgButton (hDlg, IDC_XCENTER, workprefs.gfx_xcenter);
    CheckDlgButton (hDlg, IDC_YCENTER, workprefs.gfx_ycenter);

#if 0
    init_da (hDlg);
#endif
}

static void init_resolution_combo (HWND hDlg)
{
    int i = 0;
    while (DisplayModes[i].depth >= 0) {
        SendDlgItemMessage( hDlg, IDC_RESOLUTION, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)DisplayModes[i].name );
	i++;
    }
}

static void values_from_displaydlg (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL success = FALSE;
    int gfx_width = workprefs.gfx_width_win;
    int gfx_height = workprefs.gfx_height_win;

    workprefs.gfx_pfullscreen    = IsDlgButtonChecked (hDlg, IDC_PFULLSCREEN);
    workprefs.gfx_afullscreen    = IsDlgButtonChecked (hDlg, IDC_AFULLSCREEN);

    //workprefs.gfx_lores          = IsDlgButtonChecked (hDlg, IDC_LORES);
    workprefs.gfx_correct_aspect = IsDlgButtonChecked (hDlg, IDC_ASPECT);
    workprefs.gfx_linedbl = ( IsDlgButtonChecked( hDlg, IDC_LM_SCANLINES ) ? 2 :
                              IsDlgButtonChecked( hDlg, IDC_LM_DOUBLED ) ? 1 : 0 );
    if (workprefs.gfx_linedbl)
	workprefs.gfx_lores = 0;
    else
	workprefs.gfx_lores = 1;

    workprefs.gfx_framerate = SendDlgItemMessage (hDlg, IDC_FRAMERATE, TBM_GETPOS, 0, 0);
    workprefs.gfx_vsync = IsDlgButtonChecked (hDlg, IDC_VSYNC);

    {
	char buffer[ MAX_FRAMERATE_LENGTH ];
	char Nth[ MAX_NTH_LENGTH ];
	LPSTR blah[1] = { Nth };
	LPTSTR string = NULL;

	WIN32GUI_LoadUIString( IDS_FRAMERATE, buffer, MAX_FRAMERATE_LENGTH );
	LoadNthString( workprefs.gfx_framerate - 1, Nth, MAX_NTH_LENGTH );
	if( FormatMessage( FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			   buffer, 0, 0, (LPTSTR)&string, MAX_FRAMERATE_LENGTH + MAX_NTH_LENGTH, (va_list *)blah ) == 0 )
	{
	    DWORD dwLastError = GetLastError();
	    sprintf (buffer, "Every %s Frame", nth[workprefs.gfx_framerate - 1]);
	    SetDlgItemText( hDlg, IDC_RATETEXT, buffer );
	}
	else
	{
	    SetDlgItemText( hDlg, IDC_RATETEXT, string );
	    LocalFree( string );
	}
	workprefs.gfx_width_win  = GetDlgItemInt( hDlg, IDC_XSIZE, &success, FALSE );
        if( !success )
            workprefs.gfx_width_win = 800;
	workprefs.gfx_height_win = GetDlgItemInt( hDlg, IDC_YSIZE, &success, FALSE );
        if( !success )
            workprefs.gfx_height_win = 600;
    }
    workprefs.gfx_xcenter = (IsDlgButtonChecked (hDlg, IDC_XCENTER) ? 2 : 0 ); /* Smart centering */
    workprefs.gfx_ycenter = (IsDlgButtonChecked (hDlg, IDC_YCENTER) ? 2 : 0 ); /* Smart centering */

    if (msg == WM_COMMAND && HIWORD (wParam) == CBN_SELCHANGE) 
    {
	if (LOWORD (wParam) == IDC_RESOLUTION) {
	    LONG posn;
	    posn = SendDlgItemMessage (hDlg, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
	    if (posn == CB_ERR)
		return;
	    workprefs.gfx_width_fs  = DisplayModes[posn].res.width;
	    workprefs.gfx_height_fs = DisplayModes[posn].res.height;
	    switch( DisplayModes[posn].depth )
	    {
	    case 2:
		workprefs.color_mode = 2;
		break;
	    case 3:
	    case 4:
		workprefs.color_mode = 5;
		break;
	    default:
		workprefs.color_mode = 0;
		break;
	    }
	    /* Set the Int boxes */
	    SetDlgItemInt( hDlg, IDC_XSIZE, workprefs.gfx_width_win, FALSE );
	    SetDlgItemInt( hDlg, IDC_YSIZE, workprefs.gfx_height_win, FALSE );
	    init_frequency_combo (hDlg, posn);
	} else if (LOWORD (wParam) == IDC_REFRESHRATE) {
	    LONG posn1, posn2;
	    posn1 = SendDlgItemMessage (hDlg, IDC_REFRESHRATE, CB_GETCURSEL, 0, 0);
	    if (posn1 == CB_ERR)
		return;
	    posn2 = SendDlgItemMessage (hDlg, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
	    if (posn2 == CB_ERR)
		return;
	    if (posn1 == 0) {
		workprefs.gfx_refreshrate = 0;
	    } else {
		posn1--;
	        workprefs.gfx_refreshrate = storedrefreshrates[posn1];
	    }
#if 0
	} else if (LOWORD (wParam) == IDC_DA_MODE) {
	    da_mode_selected = SendDlgItemMessage (hDlg, IDC_DA_MODE, CB_GETCURSEL, 0, 0);
	    init_da (hDlg);
	    handle_da (hDlg);
#endif
	}
    }

    updatewinfsmode (&workprefs);

#ifdef AVIOUTPUT
    if(workprefs.gfx_width != gfx_width)
	SetDlgItemInt(pages[AVIOUTPUT_ID], IDC_AVIOUTPUT_WIDTH, workprefs.gfx_width, FALSE);
    if(workprefs.gfx_height != gfx_height)
	SetDlgItemInt(pages[AVIOUTPUT_ID], IDC_AVIOUTPUT_HEIGHT, workprefs.gfx_height, FALSE);
#endif
}

static int hw3d_changed;

static BOOL CALLBACK DisplayDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    HKEY hPixelFormatKey;
    RGBFTYPE colortype      = RGBFB_NONE;
    DWORD dwType            = REG_DWORD;
    DWORD dwDisplayInfoSize = sizeof( colortype );

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[DISPLAY_ID] = hDlg;
	currentpage = DISPLAY_ID;
	SendDlgItemMessage (hDlg, IDC_FRAMERATE, TBM_SETPAGESIZE, 0, 1);
	SendDlgItemMessage (hDlg, IDC_FRAMERATE, TBM_SETRANGE, TRUE, MAKELONG (MIN_REFRESH_RATE, MAX_REFRESH_RATE));
	init_resolution_combo( hDlg );
#if 0
	init_da (hDlg);
#endif

    case WM_USER:
	recursive++;
	values_to_displaydlg (hDlg);
	enable_for_displaydlg (hDlg);
	recursive--;
        break;

    case WM_HSCROLL:
    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	if( ( wParam == IDC_TEST16BIT ) && DirectDraw_Start() )
	{
	    if( RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Arabuusimiehet\\WinUAE", 0, KEY_ALL_ACCESS, &hPixelFormatKey ) == ERROR_SUCCESS )
	    {
		char szMessage[ 4096 ];
		char szTitle[ MAX_PATH ];
		WIN32GUI_LoadUIString( IDS_GFXCARDCHECK, szMessage, 4096 );
		WIN32GUI_LoadUIString( IDS_GFXCARDTITLE, szTitle, MAX_PATH );
		    
		if( MessageBox( NULL, szMessage, szTitle, 
				MB_YESNO | MB_ICONWARNING | MB_TASKMODAL | MB_SETFOREGROUND ) == IDYES )
		{
		    colortype = WIN32GFX_FigurePixelFormats(0);
		    RegSetValueEx( hPixelFormatKey, "DisplayInfo", 0, REG_DWORD, (CONST BYTE *)&colortype, sizeof( colortype ) );
		}
		RegCloseKey( hPixelFormatKey );
	    }
	    DirectDraw_Release();
	}
	else
	{
#if 0
	    handle_da (hDlg);
#endif
	    values_from_displaydlg (hDlg, msg, wParam, lParam);
	    enable_for_displaydlg( hDlg );
	}
	recursive--;
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/display.htm");
    }
    if (hw3d_changed && recursive == 0) {
	recursive++;
        enable_for_displaydlg (hDlg);
        values_to_displaydlg (hDlg);
	hw3d_changed = 0;
	recursive--;
    }
    return FALSE;
}

static void values_to_chipsetdlg (HWND hDlg)
{
    char Nth[ MAX_NTH_LENGTH ];
    LPSTR blah[1] = { Nth };
    LPTSTR string = NULL;

    switch( workprefs.chipset_mask )
    {
    case 0:
	CheckRadioButton( hDlg, IDC_OCS, IDC_AGA, IDC_OCS+0 );
	break;
    case CSMASK_ECS_AGNUS:
	CheckRadioButton( hDlg, IDC_OCS, IDC_AGA, IDC_OCS+1 );
	break;
    case CSMASK_ECS_DENISE:
	CheckRadioButton( hDlg, IDC_OCS, IDC_AGA, IDC_OCS+2 );
	break;
    case CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE:
	CheckRadioButton( hDlg, IDC_OCS, IDC_AGA, IDC_OCS+3 );
	break;
    case CSMASK_AGA:
    case CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA:
	CheckRadioButton( hDlg, IDC_OCS, IDC_AGA, IDC_OCS+4 );
	break;
    }
    CheckDlgButton (hDlg, IDC_NTSC, workprefs.ntscmode);
    CheckDlgButton (hDlg, IDC_FASTCOPPER, workprefs.fast_copper);
    CheckDlgButton (hDlg, IDC_BLITIMM, workprefs.immediate_blits);
    CheckRadioButton (hDlg, IDC_COLLISION0, IDC_COLLISION3, IDC_COLLISION0 + workprefs.collision_level);
    CheckDlgButton (hDlg, IDC_CYCLEEXACT, workprefs.cpu_cycle_exact);
}

static void values_from_chipsetdlg (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL success = FALSE;
    int n;

    workprefs.fast_copper = IsDlgButtonChecked (hDlg, IDC_FASTCOPPER);
    workprefs.immediate_blits = IsDlgButtonChecked (hDlg, IDC_BLITIMM);
    n = IsDlgButtonChecked (hDlg, IDC_CYCLEEXACT) ? 1 : 0;
    if (workprefs.cpu_cycle_exact != n) {
	workprefs.cpu_cycle_exact = workprefs.blitter_cycle_exact = n;
	if (n) {
	    if (workprefs.cpu_level == 0)
		workprefs.cpu_compatible = 1;
	    workprefs.immediate_blits = 0;
	    workprefs.fast_copper = 0;
	    updatepage (CPU_ID);
	}
    }
    workprefs.collision_level = IsDlgButtonChecked (hDlg, IDC_COLLISION0) ? 0
				 : IsDlgButtonChecked (hDlg, IDC_COLLISION1) ? 1
				 : IsDlgButtonChecked (hDlg, IDC_COLLISION2) ? 2 : 3;
    workprefs.chipset_mask = IsDlgButtonChecked( hDlg, IDC_OCS ) ? 0
			      : IsDlgButtonChecked( hDlg, IDC_ECS_AGNUS ) ? CSMASK_ECS_AGNUS
			      : IsDlgButtonChecked( hDlg, IDC_ECS_DENISE ) ? CSMASK_ECS_DENISE
			      : IsDlgButtonChecked( hDlg, IDC_ECS ) ? CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE
			      : CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
    n = IsDlgButtonChecked (hDlg, IDC_NTSC) ? 1 : 0;
    if (workprefs.ntscmode != n) {
	workprefs.ntscmode = n;
#ifdef AVIOUTPUT
	avioutput_fps = n ? VBLANK_HZ_NTSC : VBLANK_HZ_PAL;
	updatepage (AVIOUTPUT_ID);
#endif
    }
}

static BOOL CALLBACK ChipsetDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    RGBFTYPE colortype      = RGBFB_NONE;
    DWORD dwType            = REG_DWORD;
    DWORD dwDisplayInfoSize = sizeof( colortype );

    switch (msg) {
    case WM_INITDIALOG:
	pages[CHIPSET_ID] = hDlg;
	currentpage = CHIPSET_ID;
#ifndef AGA
        EnableWindow (GetDlgItem (hDlg, IDC_AGA), FALSE);
#endif

    case WM_USER:
	recursive++;
	values_to_chipsetdlg (hDlg);
	enable_for_chipsetdlg (hDlg);
	recursive--;
        break;

    case WM_HSCROLL:
    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	values_from_chipsetdlg (hDlg, msg, wParam, lParam);
	enable_for_chipsetdlg( hDlg );
	recursive--;
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/chipset.htm");
    }

    if (refreshtab[CHIPSET_ID]) {
	refreshtab[CHIPSET_ID] = 0;
	values_to_chipsetdlg (hDlg);
	enable_for_chipsetdlg (hDlg);
    }
    return FALSE;
}

static void enable_for_memorydlg (HWND hDlg)
{
    int z3 = ! workprefs.address_space_24;
    int fast = workprefs.chipmem_size <= 0x200000;

#ifndef AUTOCONFIG
    z3 = FALSE;
    fast = FALSE;
#endif
    EnableWindow (GetDlgItem (hDlg, IDC_Z3TEXT), z3);
    EnableWindow (GetDlgItem (hDlg, IDC_Z3FASTRAM), z3);
    EnableWindow (GetDlgItem (hDlg, IDC_Z3FASTMEM), z3);
    EnableWindow (GetDlgItem (hDlg, IDC_FASTMEM), fast);
    EnableWindow (GetDlgItem (hDlg, IDC_FASTRAM), fast);
    EnableWindow (GetDlgItem (hDlg, IDC_FASTTEXT), fast);
    EnableWindow (GetDlgItem (hDlg, IDC_GFXCARDTEXT), z3);
    EnableWindow (GetDlgItem (hDlg, IDC_P96RAM), z3);
    EnableWindow (GetDlgItem (hDlg, IDC_P96MEM), z3);
}

static void values_to_memorydlg (HWND hDlg)
{
    uae_u32 mem_size = 0;

    switch (workprefs.chipmem_size) {
     case 0x00040000: mem_size = 0; break;
     case 0x00080000: mem_size = 1; break;
     case 0x00100000: mem_size = 2; break;
     case 0x00200000: mem_size = 3; break;
     case 0x00400000: mem_size = 4; break;
     case 0x00800000: mem_size = 5; break;
    }
    SendDlgItemMessage (hDlg, IDC_CHIPMEM, TBM_SETPOS, TRUE, mem_size);
    SetDlgItemText (hDlg, IDC_CHIPRAM, memsize_names[msi_chip[mem_size]]);

    mem_size = 0;
    switch (workprefs.fastmem_size) {
     case 0x00000000: mem_size = 0; break;
     case 0x00100000: mem_size = 1; break;
     case 0x00200000: mem_size = 2; break;
     case 0x00400000: mem_size = 3; break;
     case 0x00800000: mem_size = 4; break;
     case 0x01000000: mem_size = 5; break;
    }
    SendDlgItemMessage (hDlg, IDC_FASTMEM, TBM_SETPOS, TRUE, mem_size);
    SetDlgItemText (hDlg, IDC_FASTRAM, memsize_names[msi_fast[mem_size]]);

    mem_size = 0;
    switch (workprefs.bogomem_size) {
     case 0x00000000: mem_size = 0; break;
     case 0x00080000: mem_size = 1; break;
     case 0x00100000: mem_size = 2; break;
     case 0x00180000: mem_size = 3; break;
     case 0x00200000: mem_size = 4; break;
    }
    SendDlgItemMessage (hDlg, IDC_SLOWMEM, TBM_SETPOS, TRUE, mem_size);
    SetDlgItemText (hDlg, IDC_SLOWRAM, memsize_names[msi_bogo[mem_size]]);

    mem_size = 0;
    switch (workprefs.z3fastmem_size) {
     case 0x00000000: mem_size = 0; break; /*   0-megs */
     case 0x00100000: mem_size = 1; break; /*   1-megs */
     case 0x00200000: mem_size = 2; break; /*   2-megs */
     case 0x00400000: mem_size = 3; break; /*   4-megs */
     case 0x00800000: mem_size = 4; break; /*   8-megs */
     case 0x01000000: mem_size = 5; break; /*  16-megs */
     case 0x02000000: mem_size = 6; break; /*  32-megs */
     case 0x04000000: mem_size = 7; break; /*  64-megs */
     case 0x08000000: mem_size = 8; break; /* 128-megs */
     case 0x10000000: mem_size = 9; break; /* 256-megs */
     case 0x20000000: mem_size = 10; break; /* 512-megs */
     case 0x40000000: mem_size = 11; break; /* 1 GB-megs */

    }
    SendDlgItemMessage (hDlg, IDC_Z3FASTMEM, TBM_SETPOS, TRUE, mem_size);
    SetDlgItemText (hDlg, IDC_Z3FASTRAM, memsize_names[msi_z3fast[mem_size]]);

    mem_size = 0;
    switch (workprefs.gfxmem_size) {
     case 0x00000000: mem_size = 0; break;
     case 0x00100000: mem_size = 1; break;
     case 0x00200000: mem_size = 2; break;
     case 0x00400000: mem_size = 3; break;
     case 0x00800000: mem_size = 4; break;
     case 0x01000000: mem_size = 5; break;
     case 0x02000000: mem_size = 6; break;
    }
    SendDlgItemMessage (hDlg, IDC_P96MEM, TBM_SETPOS, TRUE, mem_size);
    SetDlgItemText (hDlg, IDC_P96RAM, memsize_names[msi_gfx[mem_size]]);
}

static void fix_values_memorydlg (void)
{
    if (workprefs.chipmem_size > 0x200000)
	workprefs.fastmem_size = 0;
    if (workprefs.chipmem_size > 0x80000) {
	workprefs.chipset_mask |= CSMASK_ECS_AGNUS;
	refreshtab[CHIPSET_ID] = 1;
    }
}

static BOOL CALLBACK MemoryDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    unsigned int max_z3_mem = MAX_Z3_MEM;
    MEMORYSTATUS memstats;

    switch (msg)
    {
    case WM_INITDIALOG:
	pages[MEMORY_ID] = hDlg;
	currentpage = MEMORY_ID;

	memstats.dwLength = sizeof( memstats );
	GlobalMemoryStatus( &memstats );
	while( ( memstats.dwAvailPageFile + memstats.dwAvailPhys - 32000000) < (DWORD)( 1 << (max_z3_mem + 19) ) )
	    max_z3_mem--;

	SendDlgItemMessage (hDlg, IDC_CHIPMEM, TBM_SETRANGE, TRUE, MAKELONG (MIN_CHIP_MEM, MAX_CHIP_MEM));
	SendDlgItemMessage (hDlg, IDC_FASTMEM, TBM_SETRANGE, TRUE, MAKELONG (MIN_FAST_MEM, MAX_FAST_MEM));
	SendDlgItemMessage (hDlg, IDC_SLOWMEM, TBM_SETRANGE, TRUE, MAKELONG (MIN_SLOW_MEM, MAX_SLOW_MEM));
	SendDlgItemMessage (hDlg, IDC_Z3FASTMEM, TBM_SETRANGE, TRUE, MAKELONG (MIN_Z3_MEM, max_z3_mem));
	SendDlgItemMessage (hDlg, IDC_P96MEM, TBM_SETRANGE, TRUE, MAKELONG (MIN_P96_MEM, MAX_P96_MEM));

    case WM_USER:
	recursive++;
	fix_values_memorydlg ();
	values_to_memorydlg (hDlg);
	enable_for_memorydlg (hDlg);
	recursive--;
    	break;

    case WM_HSCROLL:
	workprefs.chipmem_size = memsizes[msi_chip[SendMessage (GetDlgItem (hDlg, IDC_CHIPMEM), TBM_GETPOS, 0, 0)]];
	workprefs.bogomem_size = memsizes[msi_bogo[SendMessage (GetDlgItem (hDlg, IDC_SLOWMEM), TBM_GETPOS, 0, 0)]];
	workprefs.fastmem_size = memsizes[msi_fast[SendMessage (GetDlgItem (hDlg, IDC_FASTMEM), TBM_GETPOS, 0, 0)]];
	workprefs.z3fastmem_size = memsizes[msi_z3fast[SendMessage (GetDlgItem (hDlg, IDC_Z3FASTMEM), TBM_GETPOS, 0, 0)]];
	workprefs.gfxmem_size = memsizes[msi_gfx[SendMessage (GetDlgItem (hDlg, IDC_P96MEM), TBM_GETPOS, 0, 0)]];
	fix_values_memorydlg ();
	values_to_memorydlg (hDlg);
	enable_for_memorydlg (hDlg);
        break;

    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	values_to_memorydlg (hDlg);
	recursive--;
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/ram.htm");
    }
    return FALSE;
}

static void values_to_kickstartdlg (HWND hDlg)
{
    SetDlgItemText( hDlg, IDC_ROMFILE, workprefs.romfile );
    SetDlgItemText( hDlg, IDC_ROMFILE2, workprefs.romextfile );
    SetDlgItemText( hDlg, IDC_KEYFILE, workprefs.keyfile );
    SetDlgItemText( hDlg, IDC_FLASHFILE, workprefs.flashfile );
    SetDlgItemText( hDlg, IDC_CARTFILE, workprefs.cartfile );
    CheckDlgButton( hDlg, IDC_KICKSHIFTER, workprefs.kickshifter );
}

static BOOL CALLBACK KickstartDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch( msg ) 
    {
    case WM_INITDIALOG:
	pages[KICKSTART_ID] = hDlg;
	currentpage = KICKSTART_ID;
#if !defined (CDTV) && !defined (CD32)
        EnableWindow( GetDlgItem( hDlg, IDC_FLASHFILE), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_ROMFILE2), FALSE );
#endif
#if !defined (ACTION_REPLAY)
        EnableWindow( GetDlgItem( hDlg, IDC_CARTFILE), FALSE );
#endif
#if defined (UAE_MINI)
        EnableWindow( GetDlgItem( hDlg, IDC_KICKSHIFTER), FALSE );
#endif	
    case WM_USER:
	values_to_kickstartdlg (hDlg);
	return TRUE;

    case WM_COMMAND:
	switch( wParam ) 
	{
	case IDC_KICKCHOOSER:
	    DiskSelection( hDlg, IDC_ROMFILE, 6, &workprefs );
	    break;

	case IDC_ROMCHOOSER2:
	    DiskSelection( hDlg, IDC_ROMFILE2, 6, &workprefs );
	    break;
	            
	case IDC_KEYCHOOSER:
	    DiskSelection( hDlg, IDC_KEYFILE, 7, &workprefs );
	    break;
                
	case IDC_FLASHCHOOSER:
	    DiskSelection( hDlg, IDC_FLASHFILE, 11, &workprefs );
	    break;

	case IDC_CARTCHOOSER:
	    DiskSelection( hDlg, IDC_CARTFILE, 6, &workprefs );
	    break;

	case IDC_KICKSHIFTER:
	    workprefs.kickshifter = IsDlgButtonChecked( hDlg, IDC_KICKSHIFTER );
	    break;

	default:
	    if( SendMessage( GetDlgItem( hDlg, IDC_ROMFILE ), EM_GETMODIFY, 0, 0 ) )
	    {
		GetDlgItemText( hDlg, IDC_ROMFILE, workprefs.romfile, CFG_ROM_LENGTH);
		SendMessage( GetDlgItem( hDlg, IDC_ROMFILE ), EM_SETMODIFY, 0, 0 );
	    }
	    if( SendMessage( GetDlgItem( hDlg, IDC_ROMFILE2 ), EM_GETMODIFY, 0, 0 ) )
	    {
		GetDlgItemText( hDlg, IDC_ROMFILE2, workprefs.romextfile, CFG_ROM_LENGTH);
		SendMessage( GetDlgItem( hDlg, IDC_ROMFILE2 ), EM_SETMODIFY, 0, 0 );
	    }
	    if( SendMessage( GetDlgItem( hDlg, IDC_KEYFILE ), EM_GETMODIFY, 0, 0 ) )
	    {
		GetDlgItemText( hDlg, IDC_KEYFILE, workprefs.keyfile, CFG_KEY_LENGTH);
		SendMessage( GetDlgItem( hDlg, IDC_KEYFILE ), EM_SETMODIFY, 0, 0 );
	    }
	    if( SendMessage( GetDlgItem( hDlg, IDC_FLASHFILE ), EM_GETMODIFY, 0, 0 ) )
	    {
		GetDlgItemText( hDlg, IDC_FLASHFILE, workprefs.flashfile, CFG_ROM_LENGTH);
		SendMessage( GetDlgItem( hDlg, IDC_FLASHFILE ), EM_SETMODIFY, 0, 0 );
	    }
	    if( SendMessage( GetDlgItem( hDlg, IDC_CARTFILE ), EM_GETMODIFY, 0, 0 ) )
	    {
		GetDlgItemText( hDlg, IDC_CARTFILE, workprefs.cartfile, CFG_ROM_LENGTH);
		SendMessage( GetDlgItem( hDlg, IDC_CARTFILE ), EM_SETMODIFY, 0, 0 );
	    }
	    break;
	}
    	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/rom.htm");
    }
    return FALSE;
}

static void enable_for_miscdlg (HWND hDlg)
{
    if( !full_property_sheet )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_JULIAN), TRUE);
        EnableWindow( GetDlgItem( hDlg, IDC_CTRLF11), TRUE);
        EnableWindow( GetDlgItem( hDlg, IDC_SOCKETS), FALSE);
        EnableWindow( GetDlgItem( hDlg, IDC_SHOWGUI ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_CREATELOGFILE ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_ILLEGAL ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_NOSPEED ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_HIGHPRIORITY ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_NOSPEEDPAUSE ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_NOSOUND ), TRUE );
	EnableWindow( GetDlgItem( hDlg, IDC_NOOVERLAY ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_RESETAMIGA ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_DOSAVESTATE ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_ASPI ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_SCSIDEVICE ), FALSE );
    }
    else
    {
#if !defined (BSDSOCKET)
        EnableWindow( GetDlgItem( hDlg, IDC_SOCKETS), FALSE);
#endif
#if !defined (SCSIEMU)
        EnableWindow( GetDlgItem( hDlg, IDC_SCSIDEVICE), FALSE);
        EnableWindow( GetDlgItem( hDlg, IDC_ASPI ), FALSE );
#endif
        if( workprefs.win32_logfile )
        {
            EnableWindow( GetDlgItem( hDlg, IDC_ILLEGAL ), TRUE );
        }
        else
        {
            EnableWindow( GetDlgItem( hDlg, IDC_ILLEGAL ), FALSE );
        }
        EnableWindow( GetDlgItem( hDlg, IDC_RESETAMIGA ), FALSE );
        EnableWindow( GetDlgItem( hDlg, IDC_DOSAVESTATE ), FALSE );
    }
}

static void misc_kbled (HWND hDlg, int v, int nv)
{
    char *defname = v == IDC_KBLED1 ? "(NumLock)" : v == IDC_KBLED2 ? "(CapsLock)" : "(ScrollLock)";
    SendDlgItemMessage (hDlg, v, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)defname);
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"POWER");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"DF0");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"DF1");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"DF2");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"DF3");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"HD");
    SendDlgItemMessage (hDlg, v, CB_ADDSTRING, 0, (LPARAM)"CD");
    SendDlgItemMessage (hDlg, v, CB_SETCURSEL, nv, 0);
}

static void misc_getkbled (HWND hDlg, int v, int n)
{
    int nv = SendDlgItemMessage(hDlg, v, CB_GETCURSEL, 0, 0L);
    if (nv != CB_ERR) {
	workprefs.keyboard_leds[n] = nv;
	misc_kbled (hDlg, v, nv);
    }
}


static void values_to_miscdlg (HWND hDlg)
{
    OSVERSIONINFO osVersion;
    int i;

    CheckDlgButton( hDlg, IDC_SOCKETS, workprefs.socket_emu );
    CheckDlgButton( hDlg, IDC_ILLEGAL, workprefs.illegal_mem);
    CheckDlgButton( hDlg, IDC_SHOWGUI, workprefs.start_gui);
    CheckDlgButton( hDlg, IDC_JULIAN, workprefs.win32_middle_mouse );
    CheckDlgButton( hDlg, IDC_CREATELOGFILE, workprefs.win32_logfile );
    CheckDlgButton( hDlg, IDC_NOSPEED, workprefs.win32_iconified_nospeed );
    CheckDlgButton( hDlg, IDC_NOSPEEDPAUSE, workprefs.win32_iconified_pause );
    CheckDlgButton( hDlg, IDC_NOSOUND, workprefs.win32_iconified_nosound );
    CheckDlgButton( hDlg, IDC_CTRLF11, workprefs.win32_ctrl_F11_is_quit );
    CheckDlgButton( hDlg, IDC_NOOVERLAY, workprefs.win32_no_overlay );
    CheckDlgButton( hDlg, IDC_SHOWLEDS, workprefs.leds_on_screen );
    CheckDlgButton( hDlg, IDC_SCSIDEVICE, workprefs.scsi );
    CheckDlgButton( hDlg, IDC_ASPI, workprefs.win32_aspi );

    osVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if(GetVersionEx(&osVersion)) {
	if(osVersion.dwPlatformId != VER_PLATFORM_WIN32_NT) {
	    EnableWindow( GetDlgItem( hDlg, IDC_ASPI), FALSE );
	    CheckDlgButton( hDlg, IDC_ASPI, BST_CHECKED );
	}
    }

    misc_kbled (hDlg, IDC_KBLED1, workprefs.keyboard_leds[0]);
    misc_kbled (hDlg, IDC_KBLED2, workprefs.keyboard_leds[1]);
    misc_kbled (hDlg, IDC_KBLED3, workprefs.keyboard_leds[2]);

    SendDlgItemMessage (hDlg, IDC_ACTIVEPRIORITY, CB_RESETCONTENT, 0, 0L);
    i = 0;
    while (priorities[i].name) {
	SendDlgItemMessage (hDlg, IDC_ACTIVEPRIORITY, CB_ADDSTRING, 0, (LPARAM)priorities[i].name);
	i++;
    }
    SendDlgItemMessage (hDlg, IDC_ACTIVEPRIORITY, CB_SETCURSEL, workprefs.win32_activepriority, 0);
}

static BOOL CALLBACK MiscDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int v;

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[MISC_ID] = hDlg;
	currentpage = MISC_ID;

    case WM_USER:
	values_to_miscdlg (hDlg);
	enable_for_miscdlg (hDlg);
	return TRUE;

    case WM_COMMAND:
        misc_getkbled (hDlg, IDC_KBLED1, 0);
        misc_getkbled (hDlg, IDC_KBLED2, 1);
        misc_getkbled (hDlg, IDC_KBLED3, 2);

	v = SendDlgItemMessage(hDlg, IDC_ACTIVEPRIORITY, CB_GETCURSEL, 0, 0L);
	if (v != CB_ERR)
	    workprefs.win32_activepriority = v;

	switch( wParam )
	{
	case IDC_DOSAVESTATE:
	    if (DiskSelection( hDlg, wParam, 9, &workprefs )) 
		savestate_state = STATE_DOSAVE;
	    break;
	case IDC_DOLOADSTATE:
	    if (DiskSelection( hDlg, wParam, 10, &workprefs ))
		savestate_state = STATE_DORESTORE;
	    break;
	case IDC_RESETAMIGA:
	    uae_reset(0);
	    break;
	case IDC_QUITEMU:
	    uae_quit();
	    exit_gui (0);
	    break;
	case IDC_SOCKETS:
	    workprefs.socket_emu   = IsDlgButtonChecked( hDlg, IDC_SOCKETS );
	    break;
	case IDC_ILLEGAL:
	    workprefs.illegal_mem = IsDlgButtonChecked (hDlg, IDC_ILLEGAL);
	    break;
	case IDC_JULIAN:
	    workprefs.win32_middle_mouse = IsDlgButtonChecked( hDlg, IDC_JULIAN );
	    break;
	case IDC_NOOVERLAY:
	    workprefs.win32_no_overlay = IsDlgButtonChecked( hDlg, IDC_NOOVERLAY );
	    break;
	case IDC_SHOWLEDS:
	    workprefs.leds_on_screen = IsDlgButtonChecked( hDlg, IDC_SHOWLEDS );
	    break;
	case IDC_SHOWGUI:
	    workprefs.start_gui = IsDlgButtonChecked (hDlg, IDC_SHOWGUI);
	    break;
	case IDC_CREATELOGFILE:
	    workprefs.win32_logfile = IsDlgButtonChecked( hDlg, IDC_CREATELOGFILE );
	    enable_for_miscdlg( hDlg );
	    break;
	case IDC_NOSPEED:
	    workprefs.win32_iconified_nospeed = IsDlgButtonChecked( hDlg, IDC_NOSPEED );
	    break;
	case IDC_NOSPEEDPAUSE:
	    workprefs.win32_iconified_pause = IsDlgButtonChecked( hDlg, IDC_NOSPEEDPAUSE );
	    break;
	case IDC_NOSOUND:
	    workprefs.win32_iconified_nosound = IsDlgButtonChecked( hDlg, IDC_NOSOUND );
	    break;
	case IDC_CTRLF11:
	    workprefs.win32_ctrl_F11_is_quit = IsDlgButtonChecked( hDlg, IDC_CTRLF11 );
	    break;
	case IDC_SCSIDEVICE:
	    workprefs.scsi = IsDlgButtonChecked( hDlg, IDC_SCSIDEVICE );
	    break;
	case IDC_ASPI:
	    workprefs.win32_aspi = IsDlgButtonChecked( hDlg, IDC_ASPI );
	    break;
	}
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/misc.htm");
    }
    return FALSE;
}

static int cpu_ids[]   = { IDC_CPU0, IDC_CPU0, IDC_CPU1, IDC_CPU1, IDC_CPU2, IDC_CPU4, IDC_CPU3, IDC_CPU5, IDC_CPU6, IDC_CPU6 };
static int trust_ids[] = { IDC_TRUST0, IDC_TRUST1, IDC_TRUST1, IDC_TRUST2 };

static void enable_for_cpudlg (HWND hDlg)
{
    BOOL enable = FALSE, enable2 = FALSE;
    BOOL cpu_based_enable = FALSE;
    int compa = workprefs.cpu_level == 0 && workprefs.cpu_compatible >= 0;

#if !defined (CPUEMU_5) && !defined (CPUEMU_6)
    compa = 0;
#endif

    /* The "compatible" checkbox is only available when CPU type is 68000 */
    EnableWindow (GetDlgItem (hDlg, IDC_COMPATIBLE), !workprefs.cpu_cycle_exact && compa);

    /* These four items only get enabled when adjustable CPU style is enabled */
    EnableWindow (GetDlgItem (hDlg, IDC_SPEED), workprefs.m68k_speed > 0);
    EnableWindow (GetDlgItem (hDlg, IDC_CS_CPU_TEXT), workprefs.m68k_speed > 0);
    EnableWindow (GetDlgItem (hDlg, IDC_CS_CHIPSET_TEXT), workprefs.m68k_speed > 0);
    EnableWindow (GetDlgItem (hDlg, IDC_CPUTEXT), workprefs.m68k_speed > 0 );
    EnableWindow (GetDlgItem (hDlg, IDC_CPUIDLE), workprefs.m68k_speed != 0 ? TRUE : FALSE);
#if !defined(CPUEMU_0) || defined(CPUEMU_68000_ONLY)
    EnableWindow (GetDlgItem (hDlg, IDC_CPU1), FALSE);
    EnableWindow (GetDlgItem (hDlg, IDC_CPU2), FALSE);
    EnableWindow (GetDlgItem (hDlg, IDC_CPU3), FALSE);
    EnableWindow (GetDlgItem (hDlg, IDC_CPU4), FALSE);
    EnableWindow (GetDlgItem (hDlg, IDC_CPU5), FALSE);
    EnableWindow (GetDlgItem (hDlg, IDC_CPU6), FALSE);
#endif

    cpu_based_enable = ( workprefs.cpu_level >= 2 ) &&
		       ( workprefs.address_space_24 == 0 );

    enable = cpu_based_enable && ( workprefs.cachesize );
#ifndef JIT
    enable = FALSE;
#endif
    enable2 = enable && workprefs.compforcesettings;

    EnableWindow( GetDlgItem( hDlg, IDC_TRUST0 ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_TRUST1 ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_TRUST2 ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_HARDFLUSH ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_CONSTJUMP ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_JITFPU ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_NOFLAGS ), enable2 );
    EnableWindow( GetDlgItem( hDlg, IDC_CS_CACHE_TEXT ), cpu_based_enable );
    EnableWindow( GetDlgItem( hDlg, IDC_CACHE ), cpu_based_enable );
    EnableWindow( GetDlgItem( hDlg, IDC_CACHETEXT ), cpu_based_enable );
    EnableWindow( GetDlgItem( hDlg, IDC_FORCE ), enable );

#ifdef JIT
    if( enable )
    {
	if(!canbang)
	{
	    workprefs.compforcesettings = TRUE;
	    workprefs.comptrustbyte = 1;
	    workprefs.comptrustword = 1;
	    workprefs.comptrustlong = 1;
	    workprefs.comptrustnaddr= 1;
	}
    }
    else
    {
	workprefs.cachesize = 0; // Disable JIT
    }
#endif
}

static void values_to_cpudlg (HWND hDlg)
{
    char cache[ 8 ] = "";
    BOOL enable = FALSE;
    BOOL cpu_based_enable = FALSE;

    SendDlgItemMessage (hDlg, IDC_SPEED, TBM_SETPOS, TRUE, workprefs.m68k_speed <= 0 ? 1 : workprefs.m68k_speed / CYCLE_UNIT );
    SetDlgItemInt( hDlg, IDC_CPUTEXT, workprefs.m68k_speed <= 0 ? 1 : workprefs.m68k_speed / CYCLE_UNIT, FALSE );
    CheckDlgButton (hDlg, IDC_COMPATIBLE, workprefs.cpu_compatible);
    SendDlgItemMessage (hDlg, IDC_CPUIDLE, TBM_SETPOS, TRUE, workprefs.cpu_idle == 0 ? 0 : 12 - workprefs.cpu_idle / 15);
    CheckRadioButton (hDlg, IDC_CPU0, IDC_CPU6, cpu_ids[workprefs.cpu_level * 2 + !workprefs.address_space_24]);

    if (workprefs.m68k_speed == -1)
	CheckRadioButton( hDlg, IDC_CS_HOST, IDC_CS_ADJUSTABLE, IDC_CS_HOST );
    else if (workprefs.m68k_speed == 0)
	CheckRadioButton( hDlg, IDC_CS_HOST, IDC_CS_ADJUSTABLE, IDC_CS_68000 );
    else
	CheckRadioButton( hDlg, IDC_CS_HOST, IDC_CS_ADJUSTABLE, IDC_CS_ADJUSTABLE );

    cpu_based_enable = ( workprefs.cpu_level >= 2 ) &&
		       ( workprefs.address_space_24 == 0 );

    enable = cpu_based_enable && ( workprefs.cachesize );

#ifdef JIT
    if( enable ) {
	if( !canbang ) {
	    workprefs.compforcesettings = TRUE;
	    workprefs.comptrustbyte = 1;
	    workprefs.comptrustword = 1;
	    workprefs.comptrustlong = 1;
	    workprefs.comptrustnaddr= 1;
	}
    } else {
#endif
	workprefs.cachesize = 0; // Disable JIT
#ifdef JIT
    }
#endif

    if( !workprefs.compforcesettings ) {
	workprefs.comptrustbyte = 0;
	workprefs.comptrustword = 0;
	workprefs.comptrustlong = 0;
	workprefs.comptrustnaddr= 0;
    }

    CheckRadioButton( hDlg, IDC_TRUST0, IDC_TRUST2, trust_ids[ workprefs.comptrustbyte ] );

    SendDlgItemMessage( hDlg, IDC_CACHE, TBM_SETPOS, TRUE, workprefs.cachesize / 1024 );
    sprintf( cache, "%d MB", workprefs.cachesize / 1024 );
    SetDlgItemText( hDlg, IDC_CACHETEXT, cache );

    CheckDlgButton( hDlg, IDC_FORCE, workprefs.compforcesettings );
    CheckDlgButton( hDlg, IDC_NOFLAGS, workprefs.compnf );
    CheckDlgButton( hDlg, IDC_JITFPU, workprefs.compfpu );
    CheckDlgButton( hDlg, IDC_HARDFLUSH, workprefs.comp_hardflush );
    CheckDlgButton( hDlg, IDC_CONSTJUMP, workprefs.comp_constjump );
}

static void values_from_cpudlg (HWND hDlg)
{
    int newcpu, newtrust, oldcache;
    
    workprefs.cpu_compatible = workprefs.cpu_cycle_exact | (IsDlgButtonChecked (hDlg, IDC_COMPATIBLE) ? 1 : 0);
    workprefs.m68k_speed = IsDlgButtonChecked (hDlg, IDC_CS_HOST) ? -1
	: IsDlgButtonChecked (hDlg, IDC_CS_68000) ? 0
	: SendMessage (GetDlgItem (hDlg, IDC_SPEED), TBM_GETPOS, 0, 0) * CYCLE_UNIT;
    
    newcpu = (IsDlgButtonChecked (hDlg, IDC_CPU0) ? 0
	: IsDlgButtonChecked (hDlg, IDC_CPU1) ? 1
	: IsDlgButtonChecked (hDlg, IDC_CPU2) ? 2
	: IsDlgButtonChecked (hDlg, IDC_CPU3) ? 3
	: IsDlgButtonChecked (hDlg, IDC_CPU4) ? 4
	: IsDlgButtonChecked (hDlg, IDC_CPU5) ? 5 : 6);
    /* When switching away from 68000, disable 24 bit addressing.  */
    switch( newcpu )
    {
	case 0: // 68000
	case 1: // 68010
	case 2: // 68EC020
	case 3: // 68EC020+FPU
	    workprefs.address_space_24 = 1;
	    workprefs.cpu_level = newcpu;
	break;

	case 4: // 68020
	case 5: // 68020+FPU
	case 6: // 68040
	    workprefs.address_space_24 = 0;
	    workprefs.cpu_level = newcpu - 2;
	break;
    }
    if (newcpu > 0) {
	workprefs.cpu_compatible = 0;
        updatepage (CHIPSET_ID);
    }

    newtrust = (IsDlgButtonChecked( hDlg, IDC_TRUST0 ) ? 0
	: IsDlgButtonChecked( hDlg, IDC_TRUST1 ) ? 1 : 3 );
    workprefs.comptrustbyte = newtrust;
    workprefs.comptrustword = newtrust;
    workprefs.comptrustlong = newtrust;
    workprefs.comptrustnaddr= newtrust;

    workprefs.compforcesettings = IsDlgButtonChecked( hDlg, IDC_FORCE );
    workprefs.compnf            = IsDlgButtonChecked( hDlg, IDC_NOFLAGS );
    workprefs.compfpu           = IsDlgButtonChecked( hDlg, IDC_JITFPU );
    workprefs.comp_hardflush    = IsDlgButtonChecked( hDlg, IDC_HARDFLUSH );
    workprefs.comp_constjump    = IsDlgButtonChecked( hDlg, IDC_CONSTJUMP );

    oldcache = workprefs.cachesize;
    workprefs.cachesize = SendMessage(GetDlgItem(hDlg, IDC_CACHE), TBM_GETPOS, 0, 0) * 1024;
#ifdef JIT
    if (oldcache == 0 && workprefs.cachesize > 0)
	canbang = 1;
#endif
    workprefs.cpu_idle = SendMessage(GetDlgItem(hDlg, IDC_CPUIDLE), TBM_GETPOS, 0, 0);
    if (workprefs.cpu_idle > 0)
	workprefs.cpu_idle = (12 - workprefs.cpu_idle) * 15;

    if( pages[ KICKSTART_ID ] )
	SendMessage( pages[ KICKSTART_ID ], WM_USER, 0, 0 );
    if( pages[ DISPLAY_ID ] )
	SendMessage( pages[ DISPLAY_ID ], WM_USER, 0, 0 );
    if( pages[ MEMORY_ID ] )
        SendMessage( pages[ MEMORY_ID ], WM_USER, 0, 0 );
}

static BOOL CALLBACK CPUDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;

    switch (msg) {
    case WM_INITDIALOG:
	pages[CPU_ID] = hDlg;
	currentpage = CPU_ID;
	SendDlgItemMessage (hDlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELONG (MIN_M68K_PRIORITY, MAX_M68K_PRIORITY));
	SendDlgItemMessage (hDlg, IDC_SPEED, TBM_SETPAGESIZE, 0, 1);
	SendDlgItemMessage (hDlg, IDC_CACHE, TBM_SETRANGE, TRUE, MAKELONG (MIN_CACHE_SIZE, MAX_CACHE_SIZE));
	SendDlgItemMessage (hDlg, IDC_CACHE, TBM_SETPAGESIZE, 0, 1);
	SendDlgItemMessage (hDlg, IDC_CPUIDLE, TBM_SETRANGE, TRUE, MAKELONG (0, 10));
	SendDlgItemMessage (hDlg, IDC_CPUIDLE, TBM_SETPAGESIZE, 0, 1);

    case WM_USER:
	recursive++;
	values_to_cpudlg (hDlg);
	enable_for_cpudlg (hDlg);
	recursive--;
	return TRUE;

    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	values_from_cpudlg (hDlg);
	values_to_cpudlg (hDlg);
	enable_for_cpudlg (hDlg);
	recursive--;
	break;

    case WM_HSCROLL:
	recursive++;
	values_from_cpudlg( hDlg );
	values_to_cpudlg( hDlg );
	enable_for_cpudlg( hDlg );
	recursive--;
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/cpu.htm");
    }
    return FALSE;
}

static void enable_for_sounddlg (HWND hDlg)
{
    int numdevs;

    enumerate_sound_devices (&numdevs);
    if( numdevs == 0 )
	EnableWindow( GetDlgItem( hDlg, IDC_SOUNDCARDLIST ), FALSE );
    else
	EnableWindow( GetDlgItem( hDlg, IDC_SOUNDCARDLIST ), workprefs.produce_sound );

    EnableWindow( GetDlgItem( hDlg, IDC_FREQUENCY ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDFREQ ), workprefs.produce_sound ? TRUE : FALSE );

    EnableWindow( GetDlgItem( hDlg, IDC_STEREOMODE ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_STEREOMODE0 ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_STEREOMODE1 ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_STEREOMODE2 ), workprefs.produce_sound );

    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDINTERPOLATION ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_INTERPOLATION0 ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_INTERPOLATION1 ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_INTERPOLATION2 ), workprefs.produce_sound );

    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDBUFFERMEM ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDBUFFERRAM ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDADJUST ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDADJUSTNUM ), workprefs.produce_sound );
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDBUFFERTEXT ), workprefs.produce_sound );

    EnableWindow( GetDlgItem( hDlg, IDC_AUDIOSYNC ), workprefs.produce_sound );
 
    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDFILTER ), workprefs.produce_sound );

    EnableWindow( GetDlgItem( hDlg, IDC_SOUNDCALIBRATE ), workprefs.produce_sound && full_property_sheet);
}

static int exact_log2 (int v)
{
    int l = 0;
    while ((v >>= 1) != 0)
	l++;
    return l;
}

extern int soundpercent;

static void update_soundgui (HWND hDlg)
{
    int bufsize;
    char txt[20];

    bufsize = exact_log2 (workprefs.sound_maxbsiz / 1024);
    sprintf (txt, "%d (%dms)", bufsize, 1000 * (workprefs.sound_maxbsiz >> 1) / workprefs.sound_freq );
    SetDlgItemText (hDlg, IDC_SOUNDBUFFERMEM, txt);

    if (workprefs.sound_adjust < -100)
	workprefs.sound_adjust = -100;
    if (workprefs.sound_adjust > 30)
	workprefs.sound_adjust = 30;
    SendDlgItemMessage( hDlg, IDC_SOUNDADJUST, TBM_SETPOS, TRUE, workprefs.sound_adjust );
    
    sprintf (txt, "%.1f%%", workprefs.sound_adjust / 10.0);
    SetDlgItemText (hDlg, IDC_SOUNDADJUSTNUM, txt);
}

static int soundfreqs[] = { 11025, 15000, 22050, 32000, 44100, 48000, 0 };

static void values_to_sounddlg (HWND hDlg)
{
    int which_button;
    int sound_freq = workprefs.sound_freq;
    int produce_sound = workprefs.produce_sound;
    int stereo = workprefs.stereo;
    char txt[10];
    int i, selected;

    if (workprefs.sound_maxbsiz & (workprefs.sound_maxbsiz - 1))
	workprefs.sound_maxbsiz = DEFAULT_SOUND_MAXB;


    SendDlgItemMessage(hDlg, IDC_SOUNDFILTER, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(hDlg, IDC_SOUNDFILTER, CB_ADDSTRING, 0, (LPARAM)"Always off");
    SendDlgItemMessage(hDlg, IDC_SOUNDFILTER, CB_ADDSTRING, 0, (LPARAM)"Emulated");
    SendDlgItemMessage(hDlg, IDC_SOUNDFILTER, CB_ADDSTRING, 0, (LPARAM)"Always on");
    SendDlgItemMessage(hDlg, IDC_SOUNDFILTER, CB_SETCURSEL, workprefs.sound_filter, 0 );

    SendDlgItemMessage(hDlg, IDC_SOUNDFREQ, CB_RESETCONTENT, 0, 0);
    i = 0;
    selected = -1;
    while (soundfreqs[i]) {
	sprintf (txt, "%d", soundfreqs[i]);
	SendDlgItemMessage( hDlg, IDC_SOUNDFREQ, CB_ADDSTRING, 0, (LPARAM)txt);
	i++;
    }
    sprintf (txt, "%d", workprefs.sound_freq);
    SendDlgItemMessage( hDlg, IDC_SOUNDFREQ, WM_SETTEXT, 0, (LPARAM)txt); 

    switch (workprefs.produce_sound) {
     case 0: which_button = IDC_SOUND0; break;
     case 1: which_button = IDC_SOUND1; break;
     case 2: which_button = IDC_SOUND2; break;
     case 3: which_button = IDC_SOUND3; break;
    }
    
    CheckRadioButton( hDlg, IDC_SOUND0, IDC_SOUND3, which_button );

    switch (workprefs.stereo) 
    {
    case 0:
	// mono
	which_button = IDC_STEREOMODE0;
	break;
    case 1:
    default:
	// stereo, but which type?
	if( workprefs.mixed_stereo )
	    which_button = IDC_STEREOMODE2;
	else
	    which_button = IDC_STEREOMODE1;
	break;
    }
    CheckRadioButton( hDlg, IDC_STEREOMODE0, IDC_STEREOMODE2, which_button );

    CheckRadioButton( hDlg, IDC_INTERPOLATION0, IDC_INTERPOLATION2, IDC_INTERPOLATION0 + workprefs.sound_interpol );

    workprefs.sound_maxbsiz = 1 << exact_log2 (workprefs.sound_maxbsiz);
    if (workprefs.sound_maxbsiz < 2048)
	workprefs.sound_maxbsiz = 2048;
    SendDlgItemMessage( hDlg, IDC_SOUNDBUFFERRAM, TBM_SETPOS, TRUE, exact_log2 (workprefs.sound_maxbsiz / 2048));
    SendDlgItemMessage( hDlg, IDC_SOUNDCARDLIST, CB_SETCURSEL, workprefs.win32_soundcard, 0 );

    update_soundgui (hDlg);
}

static void values_from_sounddlg (HWND hDlg)
{
    int idx;
    char txt[6];

    idx = SendDlgItemMessage (hDlg, IDC_SOUNDFREQ, CB_GETCURSEL, 0, 0);
    if (idx >= 0) {
	workprefs.sound_freq = soundfreqs[idx];
    } else {
	SendDlgItemMessage (hDlg, IDC_SOUNDFREQ, WM_GETTEXT, (WPARAM)sizeof (txt), (LPARAM)txt);
	workprefs.sound_freq = atol (txt);
    }
    if (workprefs.sound_freq < 8000)
	workprefs.sound_freq = 8000;
    if (workprefs.sound_freq > 96000)
	workprefs.sound_freq = 96000;

    workprefs.produce_sound = (IsDlgButtonChecked (hDlg, IDC_SOUND0) ? 0
			       : IsDlgButtonChecked (hDlg, IDC_SOUND1) ? 1
			       : IsDlgButtonChecked (hDlg, IDC_SOUND2) ? 2 : 3);
    workprefs.mixed_stereo = 0;
    workprefs.stereo = IsDlgButtonChecked (hDlg, IDC_STEREOMODE0) ? 0 :
		       IsDlgButtonChecked (hDlg, IDC_STEREOMODE1) ? 1 : (workprefs.mixed_stereo = 1);

    workprefs.sound_interpol = IsDlgButtonChecked (hDlg, IDC_INTERPOLATION1 ) ? 1 : (IsDlgButtonChecked( hDlg, IDC_INTERPOLATION2 ) ? 2 : 0);;

    workprefs.win32_soundcard = SendDlgItemMessage (hDlg, IDC_SOUNDCARDLIST, CB_GETCURSEL, 0, 0L);

    workprefs.sound_filter = SendDlgItemMessage (hDlg, IDC_SOUNDFILTER, CB_GETCURSEL, 0, 0);

#ifdef AVIOUTPUT
    updatepage (AVIOUTPUT_ID);
#endif
}

extern int sound_calibrate (HWND, struct uae_prefs*);

static BOOL CALLBACK SoundDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    int numdevs;
    int card;
    char **sounddevs;

    switch (msg) {
    case WM_INITDIALOG:
	SendDlgItemMessage (hDlg, IDC_SOUNDBUFFERRAM, TBM_SETRANGE, TRUE, MAKELONG (MIN_SOUND_MEM, MAX_SOUND_MEM));
	SendDlgItemMessage (hDlg, IDC_SOUNDBUFFERRAM, TBM_SETPAGESIZE, 0, 1);

	SendDlgItemMessage( hDlg, IDC_SOUNDADJUST, TBM_SETRANGE, TRUE, MAKELONG (-100, +30) );
	SendDlgItemMessage( hDlg, IDC_SOUNDADJUST, TBM_SETPAGESIZE, 0, 1 );

	SendDlgItemMessage( hDlg, IDC_SOUNDCARDLIST, CB_RESETCONTENT, 0, 0L );
	sounddevs = enumerate_sound_devices (&numdevs);
	for (card = 0; card < numdevs; card++)
	    SendDlgItemMessage (hDlg, IDC_SOUNDCARDLIST, CB_ADDSTRING, 0, (LPARAM)sounddevs[card]);
	if (numdevs == 0)
	    workprefs.produce_sound = 0; /* No sound card in system, enable_for_sounddlg will accomodate this */

	pages[SOUND_ID] = hDlg;
	currentpage = SOUND_ID;

    case WM_USER:
	recursive++;
	values_to_sounddlg (hDlg);
	enable_for_sounddlg (hDlg);
	recursive--;
	return TRUE;

    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	if (wParam == IDC_SOUNDCALIBRATE) {
	    int pct = sound_calibrate (hDlg, &workprefs);
	    workprefs.sound_adjust = (pct - 1000);
	    update_soundgui (hDlg);
	}
	values_from_sounddlg (hDlg);
	enable_for_sounddlg (hDlg);
	recursive--;
	break;

     case WM_HSCROLL:
	workprefs.sound_maxbsiz = 2048 << SendMessage( GetDlgItem( hDlg, IDC_SOUNDBUFFERRAM ), TBM_GETPOS, 0, 0 );
	workprefs.sound_adjust = SendMessage( GetDlgItem( hDlg, IDC_SOUNDADJUST ), TBM_GETPOS, 0, 0 );
	update_soundgui (hDlg);
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/sound.htm");
    }
    return FALSE;
}

#ifdef FILESYS

struct fsvdlg_vals
{
    char volume[4096];
    char device[4096];
    char rootdir[4096];
    int bootpri;
    int rw;
    int rdb;
};

static struct fsvdlg_vals empty_fsvdlg = { "", "", "", 0, 1, 0 };
static struct fsvdlg_vals current_fsvdlg;

struct hfdlg_vals
{
    char volumename[4096];
    char devicename[4096];
    char filename[4096];
    char fsfilename[4096];
    int sectors;
    int reserved;
    int surfaces;
    int cylinders;
    int blocksize;
    int rw;
    int rdb;
    int bootpri;
};

static struct hfdlg_vals empty_hfdlg = { "", "", "", "", 32, 2, 1, 0, 512, 1, 0, 0 };
static struct hfdlg_vals current_hfdlg;

static int CALLBACK VolumeSettingsProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    BROWSEINFO browse_info;
    char directory_path[MAX_PATH] = "";
    LPITEMIDLIST browse;
    char szTitle[ MAX_PATH ];

    WIN32GUI_LoadUIString( IDS_SELECTFILESYSROOT, szTitle, MAX_PATH );

    browse_info.hwndOwner = hDlg;
    browse_info.pidlRoot = NULL;
    browse_info.pszDisplayName = directory_path;
    browse_info.lpszTitle = "";
    browse_info.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS;
    browse_info.lpfn = NULL;
    browse_info.iImage = 0;

    switch (msg) {
     case WM_INITDIALOG:
	recursive++;
	SetDlgItemText (hDlg, IDC_VOLUME_NAME, current_fsvdlg.volume);
	SetDlgItemText (hDlg, IDC_VOLUME_DEVICE, current_fsvdlg.device);
	SetDlgItemText (hDlg, IDC_PATH_NAME, current_fsvdlg.rootdir);
        SetDlgItemInt (hDlg, IDC_VOLUME_BOOTPRI, current_fsvdlg.bootpri, TRUE);
	CheckDlgButton (hDlg, IDC_RW, current_fsvdlg.rw);
	recursive--;
	return TRUE;

     case WM_COMMAND:
	if (recursive)
	    break;
	recursive++;
	if (HIWORD (wParam) == BN_CLICKED) {
	    switch (LOWORD (wParam)) {
	     case IDC_SELECTOR:
		if ((browse = SHBrowseForFolder (&browse_info)) != NULL) {
		    SHGetPathFromIDList (browse, directory_path);
		    SetDlgItemText (hDlg, IDC_PATH_NAME, directory_path);
		}
		break;
	     case IDOK:
		    if( strlen( current_fsvdlg.rootdir ) == 0 ) 
		    {
			char szMessage[ MAX_PATH ];
			char szTitle[ MAX_PATH ];
			WIN32GUI_LoadUIString( IDS_MUSTSELECTPATH, szMessage, MAX_PATH );
			WIN32GUI_LoadUIString( IDS_SETTINGSERROR, szTitle, MAX_PATH );

			MessageBox( hDlg, szMessage, szTitle,
				MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
			break;
		    }
		    if( strlen( current_fsvdlg.volume ) == 0 )
		    {
			char szMessage[ MAX_PATH ];
			char szTitle[ MAX_PATH ];
			WIN32GUI_LoadUIString( IDS_MUSTSELECTNAME, szMessage, MAX_PATH );
			WIN32GUI_LoadUIString( IDS_SETTINGSERROR, szTitle, MAX_PATH );

			MessageBox( hDlg, szMessage, szTitle,
				MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
			break;
		    }
		EndDialog (hDlg, 1);

		break;
	     case IDCANCEL:
		EndDialog (hDlg, 0);
		break;
	    }
	}
	GetDlgItemText (hDlg, IDC_PATH_NAME, current_fsvdlg.rootdir, sizeof current_fsvdlg.rootdir);
	GetDlgItemText (hDlg, IDC_VOLUME_NAME, current_fsvdlg.volume, sizeof current_fsvdlg.volume);
	GetDlgItemText (hDlg, IDC_VOLUME_DEVICE, current_fsvdlg.device, sizeof current_fsvdlg.device);
	current_fsvdlg.rw = IsDlgButtonChecked (hDlg, IDC_RW);
	current_fsvdlg.bootpri = GetDlgItemInt( hDlg, IDC_VOLUME_BOOTPRI, NULL, TRUE );
	recursive--;
	break;
    }
    return FALSE;
}

static void sethardfile (HWND hDlg)
{
    SetDlgItemText (hDlg, IDC_PATH_NAME, current_hfdlg.filename);
    SetDlgItemText (hDlg, IDC_PATH_FILESYS, current_hfdlg.fsfilename);
    SetDlgItemText (hDlg, IDC_HARDFILE_DEVICE, current_hfdlg.devicename);
    SetDlgItemInt( hDlg, IDC_SECTORS, current_hfdlg.sectors, FALSE);
    SetDlgItemInt( hDlg, IDC_HEADS, current_hfdlg.surfaces, FALSE);
    SetDlgItemInt( hDlg, IDC_RESERVED, current_hfdlg.reserved, FALSE);
    SetDlgItemInt( hDlg, IDC_BLOCKSIZE, current_hfdlg.blocksize, FALSE);
    SetDlgItemInt( hDlg, IDC_HARDFILE_BOOTPRI, current_hfdlg.bootpri, TRUE);
    CheckDlgButton (hDlg, IDC_RW, current_hfdlg.rw);
}

static void hardfile_testrdb (HWND hDlg)
{
    void *f = zfile_fopen (current_hfdlg.filename, "rb");
    char tmp[8] = { 0 };
    if (!f)
	return;
    zfile_fread (tmp, 1, sizeof (tmp), f);
    zfile_fclose (f);
    if (memcmp (tmp, "RDSK\0\0\0", 7))
	return;
    current_hfdlg.sectors = 0;
    current_hfdlg.surfaces = 0;
    current_hfdlg.reserved = 0;
    current_hfdlg.fsfilename[0] = 0;
    current_hfdlg.bootpri = 0;
    current_hfdlg.devicename[0] = 0;
    sethardfile (hDlg);
}

static int CALLBACK HardfileSettingsProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    UINT setting;

    switch (msg) {
    case WM_INITDIALOG:
	recursive++;
	sethardfile (hDlg);
	recursive--;
	return TRUE;

    case WM_COMMAND:
	if (recursive)
	    break;
	recursive++;

	if (HIWORD (wParam) == BN_CLICKED) {
	    switch (LOWORD (wParam)) {
	    case IDC_CREATEHF:
		setting = CalculateHardfileSize (hDlg);
		if( !CreateHardFile(hDlg, setting) )
		{
		    char szMessage[ MAX_PATH ];
		    char szTitle[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_FAILEDHARDFILECREATION, szMessage, MAX_PATH );
		    WIN32GUI_LoadUIString( IDS_CREATIONERROR, szTitle, MAX_PATH );

		    MessageBox( hDlg, szMessage, szTitle,
				MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
		}
		break;
	    case IDC_SELECTOR:
		DiskSelection (hDlg, IDC_PATH_NAME, 2, &workprefs );
		GetDlgItemText (hDlg, IDC_PATH_NAME, current_hfdlg.filename, sizeof current_hfdlg.filename);
		hardfile_testrdb (hDlg);
		break;
	    case IDC_FILESYS_SELECTOR:
		DiskSelection (hDlg, IDC_PATH_FILESYS, 12, &workprefs );
		break;
	    case IDOK:
		if( strlen( current_hfdlg.filename ) == 0 ) 
		{
		    char szMessage[ MAX_PATH ];
		    char szTitle[ MAX_PATH ];
		    WIN32GUI_LoadUIString( IDS_MUSTSELECTFILE, szMessage, MAX_PATH );
		    WIN32GUI_LoadUIString( IDS_SETTINGSERROR, szTitle, MAX_PATH );

		    MessageBox( hDlg, szMessage, szTitle,
				MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
		    break;
		}
		EndDialog (hDlg, 1);
		break;
	    case IDCANCEL:
		EndDialog (hDlg, 0);
		break;
	    }
	}

	GetDlgItemText (hDlg, IDC_PATH_NAME, current_hfdlg.filename, sizeof current_hfdlg.filename);
	GetDlgItemText (hDlg, IDC_PATH_FILESYS, current_hfdlg.fsfilename, sizeof current_hfdlg.fsfilename);
	GetDlgItemText (hDlg, IDC_HARDFILE_DEVICE, current_hfdlg.devicename, sizeof current_hfdlg.devicename);
	current_hfdlg.sectors   = GetDlgItemInt( hDlg, IDC_SECTORS, NULL, FALSE );
	current_hfdlg.reserved  = GetDlgItemInt( hDlg, IDC_RESERVED, NULL, FALSE );
	current_hfdlg.surfaces  = GetDlgItemInt( hDlg, IDC_HEADS, NULL, FALSE );
	current_hfdlg.blocksize = GetDlgItemInt( hDlg, IDC_BLOCKSIZE, NULL, FALSE );
	current_hfdlg.bootpri = GetDlgItemInt( hDlg, IDC_HARDFILE_BOOTPRI, NULL, TRUE );
	current_hfdlg.rw = IsDlgButtonChecked (hDlg, IDC_RW);
	recursive--;

	break;
    }
    return FALSE;
}

static int CALLBACK HarddriveSettingsProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    int i, posn, index;

    switch (msg) {
    case WM_INITDIALOG:
	hdf_init ();
	recursive++;
	CheckDlgButton (hDlg, IDC_RW, current_hfdlg.rw);
	SendDlgItemMessage(hDlg, IDC_HARDDRIVE, CB_RESETCONTENT, 0, 0);
	index = -1;
	for (i = 0; i < hdf_getnumharddrives(); i++) {
            SendDlgItemMessage( hDlg, IDC_HARDDRIVE, CB_ADDSTRING, 0, (LPARAM)hdf_getnameharddrive(i, 1));
	    if (!strcmp (current_hfdlg.filename, hdf_getnameharddrive (i, 0))) index = i;
	}
	if (index >= 0)
	    SendDlgItemMessage (hDlg, IDC_HARDDRIVE, CB_SETCURSEL, index, 0);
	recursive--;
	return TRUE;

    case WM_COMMAND:
	if (recursive)
	    break;
	recursive++;

	if (HIWORD (wParam) == BN_CLICKED) {
	    switch (LOWORD (wParam)) {
	    case IDOK:
		EndDialog (hDlg, 1);
		break;
	    case IDCANCEL:
		EndDialog (hDlg, 0);
		break;
	    }
	}

        posn = SendDlgItemMessage (hDlg, IDC_HARDDRIVE, CB_GETCURSEL, 0, 0);
	if (posn != CB_ERR)
	    strcpy (current_hfdlg.filename, hdf_getnameharddrive (posn, 0));
	current_hfdlg.rw = IsDlgButtonChecked (hDlg, IDC_RW);
	recursive--;
	break;
    }
    return FALSE;
}

static void new_filesys (HWND hDlg)
{
    const char *result;

    result = add_filesys_unit (currprefs.mountinfo, current_fsvdlg.device, current_fsvdlg.volume,
		    current_fsvdlg.rootdir, ! current_fsvdlg.rw, 0, 0, 0, 0, current_fsvdlg.bootpri, 0);
    if (result)
	MessageBox (hDlg, result, "Bad directory",
		    MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
}

static void new_hardfile (HWND hDlg)
{
    const char *result;

    result = add_filesys_unit (currprefs.mountinfo, current_hfdlg.devicename, 0,
				current_hfdlg.filename, ! current_hfdlg.rw,
				current_hfdlg.sectors, current_hfdlg.surfaces,
			       current_hfdlg.reserved, current_hfdlg.blocksize,
			       current_hfdlg.bootpri, current_hfdlg.fsfilename);
    if (result)
	MessageBox (hDlg, result, "Bad hardfile",
		    MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
}

static void new_harddrive (HWND hDlg)
{
    const char *result;

    result = add_filesys_unit (currprefs.mountinfo, 0, 0,
				current_hfdlg.filename, ! current_hfdlg.rw, 0, 0,
			       0, current_hfdlg.blocksize, 0, 0);
    if (result)
	MessageBox (hDlg, result, "Bad harddrive",
		    MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
}

static void harddisk_remove (HWND hDlg)
{
    int entry = listview_find_selected (GetDlgItem (hDlg, IDC_VOLUMELIST));
    if (entry < 0)
	return;
    kill_filesys_unit (currprefs.mountinfo, entry);
}

static void harddisk_move (HWND hDlg, int up)
{
    int entry = listview_find_selected (GetDlgItem (hDlg, IDC_VOLUMELIST));
    if (entry < 0)
	return;
    move_filesys_unit (currprefs.mountinfo, entry, up ? entry - 1 : entry + 1);
}

static void harddisk_edit (HWND hDlg)
{
    int entry = listview_find_selected (GetDlgItem (hDlg, IDC_VOLUMELIST));
    char *volname, *devname, *rootdir, *filesys;
    int secspertrack, surfaces, cylinders, reserved, blocksize, readonly, type, bootpri;
    uae_u64 size;
    const char *failure;

    if (entry < 0)
	return;
    
    failure = get_filesys_unit (currprefs.mountinfo, entry, &devname, &volname, &rootdir, &readonly,
			    &secspertrack, &surfaces, &reserved, &cylinders, &size,
			    &blocksize, &bootpri, &filesys);

    type = is_hardfile( currprefs.mountinfo, entry );
    if( type == FILESYS_HARDFILE || type == FILESYS_HARDFILE_RDB )
    {
	current_hfdlg.sectors = secspertrack;
	current_hfdlg.surfaces = surfaces;
	current_hfdlg.reserved = reserved;
	current_hfdlg.cylinders = cylinders;
	current_hfdlg.blocksize = blocksize;

	strncpy (current_hfdlg.filename, rootdir, (sizeof current_hfdlg.filename) - 1);
	current_hfdlg.filename[(sizeof current_hfdlg.filename) - 1] = '\0';
	current_hfdlg.fsfilename[0] = 0;
	if (filesys) {
	    strncpy (current_hfdlg.fsfilename, filesys, (sizeof current_hfdlg.fsfilename) - 1);
	    current_hfdlg.fsfilename[(sizeof current_hfdlg.fsfilename) - 1] = '\0';
	}
	current_fsvdlg.device[0] = 0;
	if (devname) {
	    strncpy (current_hfdlg.devicename, devname, (sizeof current_hfdlg.devicename) - 1);
	    current_hfdlg.devicename[(sizeof current_hfdlg.devicename) - 1] = '\0';
	}
	current_hfdlg.rw = !readonly;
	current_hfdlg.bootpri = bootpri;
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_HARDFILE), hDlg, HardfileSettingsProc)) 
        {
	    const char *result;
	    result = set_filesys_unit (currprefs.mountinfo, entry, current_hfdlg.devicename, 0, current_hfdlg.filename,
				       ! current_hfdlg.rw, current_hfdlg.sectors, current_hfdlg.surfaces,
				       current_hfdlg.reserved, current_hfdlg.blocksize, current_hfdlg.bootpri, current_hfdlg.fsfilename);
	    if (result)
		MessageBox (hDlg, result, "Bad hardfile",
		MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
	}
    }
    else if (type == FILESYS_HARDDRIVE) /* harddisk */
    {
	current_hfdlg.rw = !readonly;
	strncpy (current_hfdlg.filename, rootdir, (sizeof current_hfdlg.filename) - 1);
	current_hfdlg.filename[(sizeof current_hfdlg.filename) - 1] = '\0';
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_HARDDRIVE), hDlg, HarddriveSettingsProc)) 
        {
	    const char *result;
	    result = set_filesys_unit (currprefs.mountinfo, entry, 0, 0, current_hfdlg.filename,
				       ! current_hfdlg.rw, 0, 0,
				       0, current_hfdlg.blocksize, current_hfdlg.bootpri, 0);
	    if (result)
		MessageBox (hDlg, result, "Bad harddrive",
		MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
	}
    }
    else /* Filesystem */
    {
	strncpy (current_fsvdlg.rootdir, rootdir, (sizeof current_fsvdlg.rootdir) - 1);
	current_fsvdlg.rootdir[(sizeof current_fsvdlg.rootdir) - 1] = '\0';
	strncpy (current_fsvdlg.volume, volname, (sizeof current_fsvdlg.volume) - 1);
	current_fsvdlg.volume[(sizeof current_fsvdlg.volume) - 1] = '\0';
	current_fsvdlg.device[0] = 0;
	if (devname) {
	    strncpy (current_fsvdlg.device, devname, (sizeof current_fsvdlg.device) - 1);
	    current_fsvdlg.device[(sizeof current_fsvdlg.device) - 1] = '\0';
	}
	current_fsvdlg.rw = !readonly;
	current_fsvdlg.bootpri = bootpri;
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_FILESYS), hDlg, VolumeSettingsProc)) {
	    const char *result;
	    result = set_filesys_unit (currprefs.mountinfo, entry, current_fsvdlg.device, current_fsvdlg.volume,
				       current_fsvdlg.rootdir, ! current_fsvdlg.rw, 0, 0, 0, 0, current_fsvdlg.bootpri, 0);
	    if (result)
		MessageBox (hDlg, result, "Bad hardfile",
		MB_OK | MB_ICONERROR | MB_APPLMODAL | MB_SETFOREGROUND);
	}
    }
}

static HWND cachedlist = NULL;

static void harddiskdlg_button (HWND hDlg, int button)
{
    switch (button) {
     case IDC_NEW_FS:
	current_fsvdlg = empty_fsvdlg;
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_FILESYS), hDlg, VolumeSettingsProc))
	    new_filesys (hDlg);
	break;

     case IDC_NEW_HF:
	current_hfdlg = empty_hfdlg;
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_HARDFILE), hDlg, HardfileSettingsProc))
	    new_hardfile (hDlg);
	break;

     case IDC_NEW_HD:
	memset (&current_hfdlg, 0, sizeof (current_hfdlg));
	if (DialogBox( hUIDLL ? hUIDLL : hInst, MAKEINTRESOURCE (IDD_HARDDRIVE), hDlg, HarddriveSettingsProc))
	    new_harddrive (hDlg);
	break;

     case IDC_EDIT:
	harddisk_edit (hDlg);
	break;

     case IDC_REMOVE:
	harddisk_remove (hDlg);
	break;

     case IDC_UP:
	harddisk_move (hDlg, 1);
	clicked_entry--;
	break;

     case IDC_DOWN:
	harddisk_move (hDlg, 0);
	clicked_entry++;
	break;
     
    case IDC_MAPDRIVES:
        workprefs.win32_automount_drives = IsDlgButtonChecked( hDlg, button );
        break;
    }
}

static void harddiskdlg_volume_notify (HWND hDlg, NM_LISTVIEW *nmlistview)
{
    HWND list = nmlistview->hdr.hwndFrom;
    int dblclick = 0;
    int entry = 0;

    switch (nmlistview->hdr.code) {
     case NM_DBLCLK:
	dblclick = 1;
	/* fall through */
     case NM_CLICK:
	entry = listview_entry_from_click (list);
	if (entry >= 0)
	{
	    if(dblclick)
		harddisk_edit (hDlg);
	    InitializeListView( hDlg );
	    clicked_entry = entry;
	    cachedlist = list;
	    // Hilite the current selected item
	    ListView_SetItemState( cachedlist, clicked_entry, LVIS_SELECTED, LVIS_SELECTED );
	}
	break;
    }
}

static BOOL CALLBACK HarddiskDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HICON hMoveUp = NULL, hMoveDown = NULL;

    switch (msg) {
    case WM_INITDIALOG:
	clicked_entry = 0;
	pages[HARDDISK_ID] = hDlg;
	currentpage = HARDDISK_ID;
	if( !hMoveUp )
	    hMoveUp = (HICON)LoadImage( hInst, MAKEINTRESOURCE( IDI_MOVE_UP ), IMAGE_ICON, 16, 16, LR_LOADMAP3DCOLORS );
	if( !hMoveDown )
	    hMoveDown = (HICON)LoadImage( hInst, MAKEINTRESOURCE( IDI_MOVE_DOWN ), IMAGE_ICON, 16, 16, LR_LOADMAP3DCOLORS );
	SendMessage( GetDlgItem( hDlg, IDC_UP ), BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hMoveUp );
	SendMessage( GetDlgItem( hDlg, IDC_DOWN ), BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hMoveDown );
	EnableWindow (GetDlgItem(hDlg, IDC_NEW_HD), os_winnt ? TRUE : FALSE);
	
    case WM_USER:
        CheckDlgButton( hDlg, IDC_MAPDRIVES, workprefs.win32_automount_drives );
        InitializeListView( hDlg );
	break;
	
    case WM_COMMAND:
	if (HIWORD (wParam) == BN_CLICKED)
	{
	    harddiskdlg_button (hDlg, LOWORD (wParam));
	    InitializeListView( hDlg );

	    if( clicked_entry < 0 )
		clicked_entry = 0;
	    if( clicked_entry >= ListView_GetItemCount( cachedlist ) )
		clicked_entry = ListView_GetItemCount( cachedlist ) - 1;

	    if( cachedlist && clicked_entry >= 0 )
	    {
    		// Hilite the current selected item
		ListView_SetItemState( cachedlist, clicked_entry, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	    }
	}
	break;
	
    case WM_NOTIFY:
	if (((LPNMHDR) lParam)->idFrom == IDC_VOLUMELIST)
	    harddiskdlg_volume_notify (hDlg, (NM_LISTVIEW *) lParam);
	else {
	    return notifycheck (lParam, "gui/hard-drives.htm");
	}
	return TRUE;
    default:
	return FALSE;
    }
    
    return FALSE;
}

#endif

static void out_floppyspeed (HWND hDlg)
{
    char txt[30];
    if (workprefs.floppy_speed)
	sprintf (txt, "%d%%%s", workprefs.floppy_speed, workprefs.floppy_speed == 100 ? " (compatible)" : "");
    else
        strcpy (txt, "Turbo");
    SetDlgItemText (hDlg, IDC_FLOPPYSPDTEXT, txt);
}

#define BUTTONSPERFLOPPY 5
static int floppybuttons[][BUTTONSPERFLOPPY] = {
    { IDC_DF0TEXT,IDC_DF0,IDC_EJECT0,IDC_DF0TYPE,IDC_DF0WP },
    { IDC_DF1TEXT,IDC_DF1,IDC_EJECT1,IDC_DF1TYPE,IDC_DF1WP },
    { IDC_DF2TEXT,IDC_DF2,IDC_EJECT2,IDC_DF2TYPE,IDC_DF2WP },
    { IDC_DF3TEXT,IDC_DF3,IDC_EJECT3,IDC_DF3TYPE,IDC_DF3WP }
};
    
static void addfloppytype (HWND hDlg, int n)
{
    int f_text = floppybuttons[n][0];
    int f_drive = floppybuttons[n][1];
    int f_eject = floppybuttons[n][2];
    int f_type = floppybuttons[n][3];
    int f_wp = floppybuttons[n][4];
    int nn = workprefs.dfxtype[n] + 1;
    int state;

    if (nn <= 0) state = FALSE; else state = TRUE;
    SendDlgItemMessage (hDlg, f_type, CB_SETCURSEL, nn, 0);

    EnableWindow(GetDlgItem(hDlg, f_text), state);
    EnableWindow(GetDlgItem(hDlg, f_eject), state);
    EnableWindow(GetDlgItem(hDlg, f_text), state);
    CheckDlgButton(hDlg, f_wp, disk_getwriteprotect (workprefs.df[n]) && state == TRUE ? BST_CHECKED : 0);
    EnableWindow(GetDlgItem(hDlg, f_wp), state && DISK_validate_filename (workprefs.df[n], 0, 0) ? TRUE : FALSE);
}

static void addallfloppies (HWND hDlg)
{
    int i;
    
    for (i = 0; i < 4; i++) addfloppytype (hDlg, i);
}

static void getfloppytype (HWND hDlg, int n)
{
    int f_type = floppybuttons[n][3];
    int val = SendDlgItemMessage (hDlg, f_type, CB_GETCURSEL, 0, 0L);
    if (val != CB_ERR && workprefs.dfxtype[n] != val - 1) {
	workprefs.dfxtype[n] = val - 1;
	addfloppytype (hDlg, n);
    }
}

static void floppysetwriteprotect (HWND hDlg, int n, int protect)
{
    disk_setwriteprotect (n, workprefs.df[n], protect);
    addfloppytype (hDlg, n);
}

static BOOL CALLBACK FloppyDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    int i;

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[FLOPPY_ID] = hDlg;
	if (workprefs.floppy_speed > 0 && workprefs.floppy_speed < 10)
	    workprefs.floppy_speed = 100;
	currentpage = FLOPPY_ID;
	SendDlgItemMessage (hDlg, IDC_FLOPPYSPD, TBM_SETRANGE, TRUE, MAKELONG (0, 4));
	SendDlgItemMessage (hDlg, IDC_FLOPPYSPD, TBM_SETPAGESIZE, 0, 1);
        SendDlgItemMessage (hDlg, IDC_FLOPPYTYPE, CB_RESETCONTENT, 0, 0L);
	SendDlgItemMessage (hDlg, IDC_FLOPPYTYPE, CB_ADDSTRING, 0, (LPARAM)"3.5\" DD");
	SendDlgItemMessage (hDlg, IDC_FLOPPYTYPE, CB_ADDSTRING, 0, (LPARAM)"3.5\" HD");
	SendDlgItemMessage (hDlg, IDC_FLOPPYTYPE, CB_ADDSTRING, 0, (LPARAM)"5.25\" DD");
        SendDlgItemMessage (hDlg, IDC_FLOPPYTYPE, CB_SETCURSEL, 0, 0);
	for (i = 0; i < 4; i++) {
	    int f_type = floppybuttons[i][3];
	    SendDlgItemMessage (hDlg, f_type, CB_RESETCONTENT, 0, 0L);
	    SendDlgItemMessage (hDlg, f_type, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	    SendDlgItemMessage (hDlg, f_type, CB_ADDSTRING, 0, (LPARAM)"3.5\" DD");
	    SendDlgItemMessage (hDlg, f_type, CB_ADDSTRING, 0, (LPARAM)"3.5\" HD");
	    SendDlgItemMessage (hDlg, f_type, CB_ADDSTRING, 0, (LPARAM)"5.25\" SD");
	}

    case WM_USER:
	recursive++;
	SetDlgItemText (hDlg, IDC_DF0TEXT, workprefs.df[0]);
	SetDlgItemText (hDlg, IDC_DF1TEXT, workprefs.df[1]);
	SetDlgItemText (hDlg, IDC_DF2TEXT, workprefs.df[2]);
	SetDlgItemText (hDlg, IDC_DF3TEXT, workprefs.df[3]);
	SendDlgItemMessage (hDlg, IDC_FLOPPYSPD, TBM_SETPOS, TRUE,
	    workprefs.floppy_speed ? exact_log2 ((workprefs.floppy_speed) / 100) + 1 : 0);
	out_floppyspeed (hDlg);
        addallfloppies (hDlg);
 	recursive--;
	break;
	    
    case WM_COMMAND:
	if (recursive > 0)
	    break;
	recursive++;
	switch (wParam) 
	{
	case IDC_DF0WP:
	    floppysetwriteprotect (hDlg, 0, IsDlgButtonChecked (hDlg, IDC_DF0WP));
	    break;
	case IDC_DF1WP:
	    floppysetwriteprotect (hDlg, 1, IsDlgButtonChecked (hDlg, IDC_DF1WP));
	    break;
	case IDC_DF2WP:
	    floppysetwriteprotect (hDlg, 2, IsDlgButtonChecked (hDlg, IDC_DF2WP));
	    break;
	case IDC_DF3WP:
	    floppysetwriteprotect (hDlg, 3, IsDlgButtonChecked (hDlg, IDC_DF3WP));
	    break;
	case IDC_DF0:
	    DiskSelection (hDlg, wParam, 0, &workprefs );
	    addfloppytype (hDlg, 0);
	    break;
	case IDC_DF1:
	    DiskSelection (hDlg, wParam, 0, &workprefs );
	    addfloppytype (hDlg, 1);
	    break;
	case IDC_DF2:
	    DiskSelection (hDlg, wParam, 0, &workprefs );
	    addfloppytype (hDlg, 2);
	    break;
	case IDC_DF3:
	    DiskSelection (hDlg, wParam, 0, &workprefs );
	    addfloppytype (hDlg, 3);
	    break;
	case IDC_EJECT0:
	    disk_eject(0);
	    SetDlgItemText (hDlg, IDC_DF0TEXT, "");
	    workprefs.df[0][0] = 0;
	    addfloppytype (hDlg, 0);
	    break;
	case IDC_EJECT1:
	    disk_eject(1);
	    SetDlgItemText (hDlg, IDC_DF1TEXT, "");
	    workprefs.df[1][0] = 0;
	    addfloppytype (hDlg, 1);
	    break;
	case IDC_EJECT2:
	    disk_eject(2);
	    SetDlgItemText (hDlg, IDC_DF2TEXT, "");
	    workprefs.df[2][0] = 0;
	    addfloppytype (hDlg, 2);
	    break;
	case IDC_EJECT3:
	    disk_eject(3);
	    SetDlgItemText (hDlg, IDC_DF3TEXT, "");
	    workprefs.df[3][0] = 0;
	    addfloppytype (hDlg, 3);
	    break;
	case IDC_CREATE:
	    DiskSelection (hDlg, wParam, 1, &workprefs );
	    break;
	case IDC_CREATE_RAW:
	    DiskSelection( hDlg, wParam, 1, &workprefs );
	    break;
	}
	if( SendMessage( GetDlgItem( hDlg, IDC_DF0TEXT ), EM_GETMODIFY, 0, 0 ) )
	{
	    GetDlgItemText (hDlg, IDC_DF0TEXT, workprefs.df[0], 255);
	    SendMessage( GetDlgItem( hDlg, IDC_DF0TEXT ), EM_SETMODIFY, 0, 0 );
	}

	if( SendMessage( GetDlgItem( hDlg, IDC_DF1TEXT ), EM_GETMODIFY, 0, 0 ) )
	{
	    GetDlgItemText (hDlg, IDC_DF1TEXT, workprefs.df[1], 255);
	    SendMessage( GetDlgItem( hDlg, IDC_DF1TEXT ), EM_SETMODIFY, 0, 0 );
	}

	if( SendMessage( GetDlgItem( hDlg, IDC_DF2TEXT ), EM_GETMODIFY, 0, 0 ) )
	{
	    GetDlgItemText (hDlg, IDC_DF2TEXT, workprefs.df[2], 255);
	    SendMessage( GetDlgItem( hDlg, IDC_DF2TEXT ), EM_SETMODIFY, 0, 0 );
	}

	if( SendMessage( GetDlgItem( hDlg, IDC_DF3TEXT ), EM_GETMODIFY, 0, 0 ) )
	{
	    GetDlgItemText (hDlg, IDC_DF3TEXT, workprefs.df[3], 255);
	    SendMessage( GetDlgItem( hDlg, IDC_DF3TEXT ), EM_SETMODIFY, 0, 0 );
	}
        getfloppytype (hDlg, 0);
        getfloppytype (hDlg, 1);
        getfloppytype (hDlg, 2);
        getfloppytype (hDlg, 3);
	recursive--;
	break;

    case WM_HSCROLL:
    workprefs.floppy_speed = SendMessage( GetDlgItem( hDlg, IDC_FLOPPYSPD ), TBM_GETPOS, 0, 0 );
    if (workprefs.floppy_speed > 0) {
	workprefs.floppy_speed--;
	workprefs.floppy_speed = 1 << workprefs.floppy_speed;
        workprefs.floppy_speed *= 100;
    }
    out_floppyspeed (hDlg);
    break;

    case WM_NOTIFY:
	notifycheck (lParam, "gui/floppies.htm");
	return TRUE;
    default:
	return FALSE;
    }

    return FALSE;
}

static PRINTER_INFO_1 *pInfo = NULL;
static DWORD dwEnumeratedPrinters = 0;
#define MAX_PRINTERS 10
#define MAX_SERIALS 8
static char comports[MAX_SERIALS][8];

static int joy0idc[] = {
    IDC_PORT0_JOY0, IDC_PORT0_JOY1, IDC_PORT0_MOUSE, IDC_PORT0_KBDA, IDC_PORT0_KBDB, IDC_PORT0_KBDC
};

static int joy1idc[] = {
    IDC_PORT1_JOY0, IDC_PORT1_JOY1, IDC_PORT1_MOUSE, IDC_PORT1_KBDA, IDC_PORT1_KBDB, IDC_PORT1_KBDC
};

static BOOL bNoMidiIn = FALSE;

static void enable_for_portsdlg( HWND hDlg )
{
    int i, v;

    v = workprefs.input_selected_setting > 0 ? FALSE : TRUE;
    for( i = 0; i < 6; i++ )
    {
        EnableWindow( GetDlgItem( hDlg, joy0idc[i] ), v );
        EnableWindow( GetDlgItem( hDlg, joy1idc[i] ), v );
    }
    EnableWindow (GetDlgItem (hDlg, IDC_SWAP), v);
#if !defined (SERIAL_PORT)
    EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_SHARED), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_SER_CTSRTS), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_SERIAL_DIRECT), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_SERIAL), FALSE );
#endif
#if !defined (PARALLEL_PORT)
    EnableWindow( GetDlgItem( hDlg, IDC_PRINTERLIST), FALSE );
#endif
}

static void UpdatePortRadioButtons( HWND hDlg )
{
    int which_button1, which_button2;

    enable_for_portsdlg( hDlg );
	which_button1 = joy0idc[workprefs.jport0];
	if (CheckRadioButton (hDlg, IDC_PORT0_JOY0, IDC_PORT0_KBDC, which_button1) == 0)
	    which_button1 = 0;
    else
    {
        EnableWindow( GetDlgItem( hDlg, joy1idc[workprefs.jport0] ), FALSE );
    }
	which_button2 = joy1idc[workprefs.jport1];
    if( workprefs.jport1 == workprefs.jport0 )
    {
        if( which_button2 == IDC_PORT1_KBDC )
            which_button2 = IDC_PORT1_KBDB;
        else
            which_button2++;
    }
	if (CheckRadioButton (hDlg, IDC_PORT1_JOY0, IDC_PORT1_KBDC, which_button2) == 0)
	    which_button2 = 0;
    else
    {
        EnableWindow( GetDlgItem( hDlg, joy0idc[ workprefs.jport1 ] ), FALSE );
    }
}

static void values_from_portsdlg (HWND hDlg)
{
    int item;
    char tmp[256];
    /* 0 - joystick 0
     * 1 - joystick 1
     * 2 - mouse
     * 3 - numpad
     * 4 - cursor keys
     * 5 - elsewhere
     */
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_JOY0)) {
	    workprefs.jport0 = 0;
    }
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_JOY1)) {
	    workprefs.jport0 = 1;
    }
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_MOUSE))
	    workprefs.jport0 = 2;
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_KBDA))
	    workprefs.jport0 = 3;
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_KBDB))
	    workprefs.jport0 = 4;
    if (IsDlgButtonChecked (hDlg, IDC_PORT0_KBDC))
	    workprefs.jport0 = 5;

    if (IsDlgButtonChecked (hDlg, IDC_PORT1_JOY0)) {
	    workprefs.jport1 = 0;
    }
    if (IsDlgButtonChecked (hDlg, IDC_PORT1_JOY1)) {
	    workprefs.jport1 = 1;
    }
    if (IsDlgButtonChecked (hDlg, IDC_PORT1_MOUSE))
	    workprefs.jport1 = 2;
    if (IsDlgButtonChecked (hDlg, IDC_PORT1_KBDA))
	    workprefs.jport1 = 3;
    if (IsDlgButtonChecked (hDlg, IDC_PORT1_KBDB))
	    workprefs.jport1 = 4;
    if (IsDlgButtonChecked (hDlg, IDC_PORT1_KBDC))
	    workprefs.jport1 = 5;

    item = SendDlgItemMessage( hDlg, IDC_PRINTERLIST, CB_GETCURSEL, 0, 0L );
    if( item != CB_ERR )
    {
	strcpy (tmp, workprefs.prtname);
	if( item )
	    strcpy( workprefs.prtname, pInfo[item-1].pName);
	else
	    strcpy( workprefs.prtname, "none" );
#ifdef PARALLEL_PORT
	if (strcmp (workprefs.prtname, tmp))
	    closeprinter ();
#endif
    }

    workprefs.win32_midioutdev = SendDlgItemMessage( hDlg, IDC_MIDIOUTLIST, CB_GETCURSEL, 0, 0 );
    workprefs.win32_midioutdev--; /* selection zero is always 'default midi device', so we make it -1 */

    if( bNoMidiIn )
    {
	workprefs.win32_midiindev = -1;
    }
    else
    {
	workprefs.win32_midiindev = SendDlgItemMessage( hDlg, IDC_MIDIINLIST, CB_GETCURSEL, 0, 0 );
    }

    item = SendDlgItemMessage (hDlg, IDC_SERIAL, CB_GETCURSEL, 0, 0L);
    switch( item ) 
    {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	    workprefs.use_serial = 1;
	    strcpy (workprefs.sername, comports[item - 1]);
	    EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST ), TRUE );
	    EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST ), TRUE );
	break;

	default:
	    workprefs.use_serial = 0;
	    strcpy( workprefs.sername, "none" );
	    EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST ), FALSE );
	    EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST ), FALSE );
	break;
    }
    workprefs.serial_demand = 0;
    if( IsDlgButtonChecked( hDlg, IDC_SHARED ) )
        workprefs.serial_demand = 1;
    workprefs.serial_hwctsrts = 0;
    if( IsDlgButtonChecked( hDlg, IDC_SER_CTSRTS ) )
        workprefs.serial_hwctsrts = 1;
    workprefs.serial_direct = 0;
    if( IsDlgButtonChecked( hDlg, IDC_SERIAL_DIRECT ) )
        workprefs.serial_direct = 1;
}

static void values_to_portsdlg (HWND hDlg)
{
    LONG item_height, result = 0;
    RECT rect;

    if( strcmp (workprefs.prtname, "none"))
    {
	result = SendDlgItemMessage( hDlg, IDC_PRINTERLIST, CB_FINDSTRINGEXACT, -1, (LPARAM)workprefs.prtname );
	if( result < 0 )
	{
	    // Warn the user that their printer-port selection is not valid on this machine
	    char szMessage[ MAX_PATH ];
	    WIN32GUI_LoadUIString( IDS_INVALIDPRTPORT, szMessage, MAX_PATH );
	    gui_message( szMessage );
	    
	    // Disable the invalid parallel-port selection
	    strcpy( workprefs.prtname, "none" );

	    result = 0;
	}
    }
    SendDlgItemMessage( hDlg, IDC_PRINTERLIST, CB_SETCURSEL, result, 0 );
    SendDlgItemMessage( hDlg, IDC_MIDIOUTLIST, CB_SETCURSEL, workprefs.win32_midioutdev + 1, 0 ); /* we +1 here because 1st entry is 'default' */
    if( !bNoMidiIn && ( workprefs.win32_midiindev >= 0 ) )
	SendDlgItemMessage( hDlg, IDC_MIDIINLIST, CB_SETCURSEL, workprefs.win32_midiindev, 0 );
    else
	SendDlgItemMessage( hDlg, IDC_MIDIINLIST, CB_SETCURSEL, 0, 0 );
    
    CheckDlgButton( hDlg, IDC_SHARED, workprefs.serial_demand );
    CheckDlgButton( hDlg, IDC_SER_CTSRTS, workprefs.serial_hwctsrts );
    CheckDlgButton( hDlg, IDC_SERIAL_DIRECT, workprefs.serial_direct );
    
    if( strcasecmp( workprefs.sername, szNone ) == 0 ) 
    {
	SendDlgItemMessage (hDlg, IDC_SERIAL, CB_SETCURSEL, 0, 0L);
        workprefs.use_serial = 0;
    }
    else
    {
	int t = (workprefs.sername[0] == '\0' ? 0 : workprefs.sername[3] - '0');
	int i, result = -1;
	for (i = 0; i < MAX_SERIALS; i++) {
	    if (!strcmp (comports[i], workprefs.sername)) {
	        result = SendDlgItemMessage( hDlg, IDC_SERIAL, CB_SETCURSEL, i + 1, 0L );
		break;
	    }
	}
	if( result < 0 )
	{
	    if (t > 0) {
		// Warn the user that their COM-port selection is not valid on this machine
		char szMessage[ MAX_PATH ];
		WIN32GUI_LoadUIString( IDS_INVALIDCOMPORT, szMessage, MAX_PATH );
		gui_message( szMessage );

		// Select "none" as the COM-port
		SendDlgItemMessage( hDlg, IDC_SERIAL, CB_SETCURSEL, 0L, 0L );		
	    }
	    // Disable the chosen serial-port selection
	    strcpy( workprefs.sername, "none" );
	    workprefs.use_serial = 0;
	}
	else
	{
	    workprefs.use_serial = 1;
	}
    }

    if( workprefs.use_serial )
    {
	EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST ), TRUE );
	if( !bNoMidiIn )
	    EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST ), TRUE );
    }
    else
    {
	EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST ), FALSE );
	EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST ), FALSE );
    }
    /* Retrieve the height, in pixels, of a list item. */
    item_height = SendDlgItemMessage (hDlg, IDC_SERIAL, CB_GETITEMHEIGHT, 0, 0L);
    if (item_height != CB_ERR) {
	/* Get actual box position and size. */
	GetWindowRect (GetDlgItem (hDlg, IDC_SERIAL), &rect);
	rect.bottom = (rect.top + item_height * 5
	    + SendDlgItemMessage (hDlg, IDC_SERIAL, CB_GETITEMHEIGHT, (WPARAM) - 1, 0L)
	    + item_height);
	SetWindowPos (GetDlgItem (hDlg, IDC_SERIAL), 0, 0, 0, rect.right - rect.left,
	    rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
    }
}

static int portsdlg_init;

static void init_portsdlg( HWND hDlg )
{
    int port, portcnt, numdevs;
    COMMCONFIG cc;
    DWORD size = sizeof(COMMCONFIG);

    MIDIOUTCAPS midiOutCaps;
    MIDIINCAPS midiInCaps;

    SendDlgItemMessage (hDlg, IDC_SERIAL, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_SERIAL, CB_ADDSTRING, 0, (LPARAM)szNone );
    portcnt = 0;
    for( port = 0; port < MAX_SERIALS; port++ )
    {
        sprintf( comports[portcnt], "COM%d", port );
        if( GetDefaultCommConfig( comports[portcnt], &cc, &size ) )
        {
            SendDlgItemMessage( hDlg, IDC_SERIAL, CB_ADDSTRING, 0, (LPARAM)comports[portcnt++] );
	}
    }

    SendDlgItemMessage (hDlg, IDC_PRINTERLIST, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_PRINTERLIST, CB_ADDSTRING, 0, (LPARAM)szNone );
    if( !pInfo ) {
	int flags = PRINTER_ENUM_LOCAL | (os_winnt ? PRINTER_ENUM_CONNECTIONS : 0);
	DWORD needed = 0;
	EnumPrinters( flags, NULL, 1, (LPBYTE)pInfo, 0, &needed, &dwEnumeratedPrinters );
	if (needed > 0) {
	    DWORD size = needed;
	    pInfo = calloc(1, size);
	    dwEnumeratedPrinters = 0;
	    EnumPrinters( flags, NULL, 1, (LPBYTE)pInfo, size, &needed, &dwEnumeratedPrinters );
	}
	if (dwEnumeratedPrinters == 0) {
	    free (pInfo);
	    pInfo = 0;
	}
    }
    if (pInfo) {
        for( port = 0; port < (int)dwEnumeratedPrinters; port++ )
	    SendDlgItemMessage( hDlg, IDC_PRINTERLIST, CB_ADDSTRING, 0, (LPARAM)pInfo[port].pName );
    } else {
	EnableWindow( GetDlgItem( hDlg, IDC_PRINTERLIST ), FALSE );
    }

    if( ( numdevs = midiOutGetNumDevs() ) == 0 )
    {
	EnableWindow( GetDlgItem( hDlg, IDC_MIDIOUTLIST ), FALSE );
    }
    else
    {
	char szMidiOut[ MAX_PATH ];
	WIN32GUI_LoadUIString( IDS_DEFAULTMIDIOUT, szMidiOut, MAX_PATH );
        SendDlgItemMessage( hDlg, IDC_MIDIOUTLIST, CB_RESETCONTENT, 0, 0L );
        SendDlgItemMessage( hDlg, IDC_MIDIOUTLIST, CB_ADDSTRING, 0, (LPARAM)szMidiOut );

        for( port = 0; port < numdevs; port++ )
        {
            if( midiOutGetDevCaps( port, &midiOutCaps, sizeof( midiOutCaps ) ) == MMSYSERR_NOERROR )
            {
                SendDlgItemMessage( hDlg, IDC_MIDIOUTLIST, CB_ADDSTRING, 0, (LPARAM)midiOutCaps.szPname );
            }
        }
    }

    if( ( numdevs = midiInGetNumDevs() ) == 0 )
    {
	EnableWindow( GetDlgItem( hDlg, IDC_MIDIINLIST ), FALSE );
	bNoMidiIn = TRUE;
    }
    else
    {
        SendDlgItemMessage( hDlg, IDC_MIDIINLIST, CB_RESETCONTENT, 0, 0L );

        for( port = 0; port < numdevs; port++ )
        {
            if( midiInGetDevCaps( port, &midiInCaps, sizeof( midiInCaps ) ) == MMSYSERR_NOERROR )
            {
                SendDlgItemMessage( hDlg, IDC_MIDIINLIST, CB_ADDSTRING, 0, (LPARAM)midiInCaps.szPname );
            }
        }
    }
    portsdlg_init = 1;
}

static void values_to_portsdlg_input (HWND hDlg)
{
    if (!portsdlg_init)
	init_portsdlg (hDlg);
    values_to_portsdlg (hDlg);
}

/* Handle messages for the Joystick Settings page of our property-sheet */
static BOOL CALLBACK PortsDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive = 0;
    int temp;

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[PORTS_ID] = hDlg;
	currentpage = PORTS_ID;
	init_portsdlg( hDlg );
	    
    case WM_USER:
	recursive++;
	enable_for_portsdlg( hDlg );
	values_to_portsdlg (hDlg);
	UpdatePortRadioButtons( hDlg );
	recursive--;
	return TRUE;

    case WM_COMMAND:
        if (recursive > 0)
	    break;
	recursive++;
	if( wParam == IDC_SWAP )
	{
	    temp = workprefs.jport0;
	    workprefs.jport0 = workprefs.jport1;
	    workprefs.jport1 = temp;
	    UpdatePortRadioButtons( hDlg );
	}
	else
	{
	    values_from_portsdlg (hDlg);
	    UpdatePortRadioButtons( hDlg );
	}
	input_update = 1;
        inputdevice_updateconfig (&workprefs);
	inputdevice_config_change ();
        recursive--;
	break;

    case WM_NOTIFY:
	return notifycheck (lParam, "gui/ports.htm");
    }
    return FALSE;
}

static char *eventnames[INPUTEVENT_END];

static void values_to_inputdlg (HWND hDlg)
{
    SendDlgItemMessage( hDlg, IDC_INPUTTYPE, CB_SETCURSEL, workprefs.input_selected_setting, 0 );
    SendDlgItemMessage( hDlg, IDC_INPUTDEVICE, CB_SETCURSEL, input_selected_device, 0 );
    SetDlgItemInt( hDlg, IDC_INPUTDEADZONE, workprefs.input_joystick_deadzone, FALSE );
    SetDlgItemInt( hDlg, IDC_INPUTAUTOFIRERATE, workprefs.input_autofire_framecnt, FALSE );
    SetDlgItemInt( hDlg, IDC_INPUTSPEEDD, workprefs.input_joymouse_speed, FALSE );
    SetDlgItemInt( hDlg, IDC_INPUTSPEEDA, workprefs.input_joymouse_multiplier, FALSE );
    SetDlgItemInt( hDlg, IDC_INPUTSPEEDM, workprefs.input_mouse_speed, FALSE );
    CheckDlgButton ( hDlg, IDC_INPUTDEVICEDISABLE, inputdevice_get_device_status (input_selected_device) || workprefs.input_selected_setting == 0 ? BST_CHECKED : BST_UNCHECKED);
}

static void init_inputdlg_2( HWND hDlg )
{
    char name1[256], name2[256];
    int cnt, index, af, aftmp;

    if (input_selected_widget < 0) {
	EnableWindow( GetDlgItem( hDlg, IDC_INPUTAMIGA), FALSE );
	EnableWindow( GetDlgItem( hDlg, IDC_INPUTAUTOFIRE), FALSE );
    } else {
	EnableWindow( GetDlgItem( hDlg, IDC_INPUTAMIGA), TRUE );
	EnableWindow( GetDlgItem( hDlg, IDC_INPUTAUTOFIRE), TRUE );
    }
    EnableWindow( GetDlgItem( hDlg, IDC_INPUTAMIGACNT), TRUE );
    SendDlgItemMessage (hDlg, IDC_INPUTAMIGA, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_INPUTAMIGA, CB_ADDSTRING, 0, (LPARAM)"<none>");
    index = -1; af = 0;
    if (input_selected_widget >= 0) {
	inputdevice_get_mapped_name (input_selected_device, input_selected_widget, 0, name1, input_selected_sub_num);
	cnt = 1;
	while(inputdevice_iterate (input_selected_device, input_selected_widget, name2, &aftmp)) {
	    free (eventnames[cnt]);
	    eventnames[cnt] = strdup (name2);
	    if (name1 && !strcmp (name1, name2)) {
		index = cnt;
		af = aftmp;
	    }
	    cnt++;
	    SendDlgItemMessage (hDlg, IDC_INPUTAMIGA, CB_ADDSTRING, 0, (LPARAM)name2);
	}
	if (index >= 0) {
	    SendDlgItemMessage( hDlg, IDC_INPUTAMIGA, CB_SETCURSEL, index, 0 );
	    SendDlgItemMessage( hDlg, IDC_INPUTAMIGACNT, CB_SETCURSEL, input_selected_sub_num, 0 );
	    CheckDlgButton( hDlg, IDC_INPUTAUTOFIRE, af ? BST_CHECKED : BST_UNCHECKED);
	}
    }
}

static void init_inputdlg( HWND hDlg )
{
    int i;
    DWORD size = sizeof(COMMCONFIG);
    char buf[10];

    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_ADDSTRING, 0, (LPARAM)"Compatibility mode");
    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_ADDSTRING, 0, (LPARAM)"Configuration #1");
    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_ADDSTRING, 0, (LPARAM)"Configuration #2");
    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_ADDSTRING, 0, (LPARAM)"Configuration #3");
    SendDlgItemMessage (hDlg, IDC_INPUTTYPE, CB_ADDSTRING, 0, (LPARAM)"Configuration #4");

    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_ADDSTRING, 0, (LPARAM)"Default");
    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_ADDSTRING, 0, (LPARAM)"Config #1");
    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_ADDSTRING, 0, (LPARAM)"Config #2");
    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_ADDSTRING, 0, (LPARAM)"Config #3");
    SendDlgItemMessage (hDlg, IDC_INPUTCOPYFROM, CB_ADDSTRING, 0, (LPARAM)"Config #4");

    SendDlgItemMessage (hDlg, IDC_INPUTAMIGACNT, CB_RESETCONTENT, 0, 0L);
    for (i = 0; i < MAX_INPUT_SUB_EVENT; i++) {
	sprintf (buf, "%d", i + 1);
	SendDlgItemMessage (hDlg, IDC_INPUTAMIGACNT, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendDlgItemMessage( hDlg, IDC_INPUTAMIGACNT, CB_SETCURSEL, input_selected_sub_num, 0 );

    SendDlgItemMessage (hDlg, IDC_INPUTDEVICE, CB_RESETCONTENT, 0, 0L);
    for (i = 0; i < inputdevice_get_device_total (IDTYPE_JOYSTICK); i++) {
	SendDlgItemMessage (hDlg, IDC_INPUTDEVICE, CB_ADDSTRING, 0, (LPARAM)inputdevice_get_device_name(IDTYPE_JOYSTICK, i));
    }
    for (i = 0; i < inputdevice_get_device_total (IDTYPE_MOUSE); i++) {
	SendDlgItemMessage (hDlg, IDC_INPUTDEVICE, CB_ADDSTRING, 0, (LPARAM)inputdevice_get_device_name(IDTYPE_MOUSE, i));
    }
    for (i = 0; i < inputdevice_get_device_total (IDTYPE_KEYBOARD); i++) {
	SendDlgItemMessage (hDlg, IDC_INPUTDEVICE, CB_ADDSTRING, 0, (LPARAM)inputdevice_get_device_name(IDTYPE_KEYBOARD, i));
    }
    InitializeListView(hDlg);
    init_inputdlg_2 (hDlg);
    values_to_inputdlg (hDlg);
}

static void enable_for_inputdlg (HWND hDlg)
{
    int v = workprefs.input_selected_setting == 0 ? FALSE : TRUE;
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTLIST), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTAMIGA), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTAMIGACNT), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTAUTOFIRE), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTDEADZONE), TRUE);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTAUTOFIRERATE), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTSPEEDA), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTSPEEDD), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTSPEEDM), TRUE);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTCOPY), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTCOPYFROM), v);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTSWAP), TRUE);
    EnableWindow (GetDlgItem (hDlg, IDC_INPUTDEVICEDISABLE), workprefs.input_selected_setting == 0 ? FALSE : TRUE);
}

static void clearinputlistview (HWND hDlg)
{
    ListView_DeleteAllItems( GetDlgItem( hDlg, IDC_INPUTLIST ) );
}

static void values_from_inputdlg (HWND hDlg)
{
    int item, doselect = 0, v;
    BOOL success;

    v  = GetDlgItemInt( hDlg, IDC_INPUTDEADZONE, &success, FALSE );
    if (success) {
	currprefs.input_joystick_deadzone = workprefs.input_joystick_deadzone = v;
	currprefs.input_joystick_deadzone = workprefs.input_joymouse_deadzone = v;
    }
    v  = GetDlgItemInt( hDlg, IDC_INPUTAUTOFIRERATE, &success, FALSE );
    if (success)
	currprefs.input_autofire_framecnt = workprefs.input_autofire_framecnt = v;
    v  = GetDlgItemInt( hDlg, IDC_INPUTSPEEDD, &success, FALSE );
    if (success)
	currprefs.input_joymouse_speed = workprefs.input_joymouse_speed = v;
    v  = GetDlgItemInt( hDlg, IDC_INPUTSPEEDA, &success, FALSE );
    if (success)
	currprefs.input_joymouse_multiplier = workprefs.input_joymouse_multiplier = v;
    v  = GetDlgItemInt( hDlg, IDC_INPUTSPEEDM, &success, FALSE );
    if (success)
	currprefs.input_mouse_speed = workprefs.input_mouse_speed = v;

    item = SendDlgItemMessage( hDlg, IDC_INPUTAMIGACNT, CB_GETCURSEL, 0, 0L );
    if (item != CB_ERR && input_selected_sub_num != item) {
	input_selected_sub_num = item;
	doselect = 0;
        init_inputdlg_2 (hDlg);
	update_listview_input (hDlg);
	return;
    }

    item = SendDlgItemMessage( hDlg, IDC_INPUTTYPE, CB_GETCURSEL, 0, 0L );
    if( item != CB_ERR ) {
	if (item != workprefs.input_selected_setting) {
	    workprefs.input_selected_setting = item;
	    input_selected_widget = -1;
	    inputdevice_updateconfig (&workprefs);
	    enable_for_inputdlg( hDlg );
	    InitializeListView (hDlg);
	    doselect = 1;
	}
    }
    item = SendDlgItemMessage( hDlg, IDC_INPUTDEVICE, CB_GETCURSEL, 0, 0L );
    if( item != CB_ERR ) {
	if (item != input_selected_device) {
	    input_selected_device = item;
	    input_selected_widget = -1;
	    input_selected_event = -1;
	    InitializeListView (hDlg);
	    init_inputdlg_2 (hDlg);
	    values_to_inputdlg (hDlg);
	    doselect = 1;
	}
    }
    item = SendDlgItemMessage( hDlg, IDC_INPUTAMIGA, CB_GETCURSEL, 0, 0L );
    if( item != CB_ERR) {
	input_selected_event = item;
	doselect = 1;
    }

    if (doselect && input_selected_device >= 0 && input_selected_event >= 0) {
	int af = IsDlgButtonChecked( hDlg, IDC_INPUTAUTOFIRE) ? 1 : 0;
        inputdevice_set_mapping (input_selected_device, input_selected_widget,
	    eventnames[input_selected_event], af, input_selected_sub_num);
	update_listview_input (hDlg);
        inputdevice_updateconfig (&workprefs);
    }
}

static void input_swap (HWND hDlg)
{
    inputdevice_swap_ports (&workprefs, input_selected_device);
    init_inputdlg (hDlg);
}

static void input_copy (HWND hDlg)
{
    int dst = workprefs.input_selected_setting;
    int src = SendDlgItemMessage( hDlg, IDC_INPUTCOPYFROM, CB_GETCURSEL, 0, 0L );
    if (src == CB_ERR)
	return;
    inputdevice_copy_single_config (&workprefs, src, workprefs.input_selected_setting, input_selected_device);
    init_inputdlg (hDlg);
}

static BOOL CALLBACK InputDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char name_buf[MAX_PATH] = "", desc_buf[128] = "";
    char *posn = NULL;
    HWND list;
    int dblclick = 0;
    NM_LISTVIEW *nmlistview;
    int items = 0, entry = 0;
    static int recursive;

    if (input_update) {
	input_update = 0;
        inputdevice_updateconfig (&workprefs);
	inputdevice_config_change ();
        init_inputdlg( hDlg);
    }

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[INPUT_ID] = hDlg;
	currentpage = INPUT_ID;
	inputdevice_updateconfig (&workprefs);
	input_selected_widget = -1;
	init_inputdlg( hDlg );
	    
    case WM_USER:
	recursive++;
	enable_for_inputdlg (hDlg);
	values_to_inputdlg (hDlg);
	recursive--;
	return TRUE;
    case WM_COMMAND:
        if (recursive)
	    break;
	recursive++;
	switch (wParam)
	{
	    case IDC_INPUTCOPY:
	    input_copy (hDlg);
	    break;
	    case IDC_INPUTSWAP:
	    input_swap (hDlg);
	    break;
	    case IDC_INPUTDEVICEDISABLE:
	    inputdevice_set_device_status (input_selected_device, IsDlgButtonChecked( hDlg, IDC_INPUTDEVICEDISABLE) ? 1 : 0);
	    break;
	    default:
	    values_from_inputdlg (hDlg);
	    break;
	}
	enable_for_portsdlg (hDlg);
	values_to_portsdlg_input (hDlg);
	inputdevice_config_change ();
	recursive--;
	break;
    case WM_NOTIFY:
        if (((LPNMHDR) lParam)->idFrom == IDC_INPUTLIST) 
        {
	    nmlistview = (NM_LISTVIEW *) lParam;
	    list = nmlistview->hdr.hwndFrom;
	    switch (nmlistview->hdr.code) 
	    {
		case NM_DBLCLK:
		dblclick = 1;
		/* fall-through */
		case NM_CLICK:
		entry = listview_entry_from_click (list);
		input_selected_widget = -1;
		if (entry >= 0) 
                {
		    input_selected_widget = entry;
		}
	        init_inputdlg_2 (hDlg);
	    }
	} else {
	    return notifycheck (lParam, "gui/input.htm");
	}
    }
    return FALSE;
}

#if defined (OPENGL) || defined (D3D)

static int scanlineratios[] = { 1,1,1,2,1,3, 2,1,2,2,2,3, 3,1,3,2,3,3, 0,0 };
static int scanlineindexes[100];

static void enable_for_hw3ddlg (HWND hDlg)
{
    int v = workprefs.gfx_filter ? TRUE : FALSE;
    int vv = (v && (workprefs.gfx_filter == UAE_FILTER_DIRECT3D || workprefs.gfx_filter == UAE_FILTER_OPENGL)) ? TRUE : FALSE;
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLENABLE), TRUE);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLBITS), v);
    CheckDlgButton( hDlg, IDC_OPENGLENABLE, v );
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLHZ), vv);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLVZ), vv);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLHO), v);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLVO), v);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLSLR), vv);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLSL), vv);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLSL2), vv);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLDEFAULT), v);
    EnableWindow (GetDlgItem (hDlg, IDC_OPENGLFILTER), vv);
}

static void values_to_hw3ddlg (HWND hDlg)
{
    char txt[10];
    int i, j;

    SendDlgItemMessage( hDlg, IDC_OPENGLHZ, TBM_SETRANGE, TRUE, MAKELONG (-50, +50) );
    SendDlgItemMessage( hDlg, IDC_OPENGLHZ, TBM_SETPAGESIZE, 0, 1 );
    SendDlgItemMessage( hDlg, IDC_OPENGLVZ, TBM_SETRANGE, TRUE, MAKELONG (-50, +50) );
    SendDlgItemMessage( hDlg, IDC_OPENGLVZ, TBM_SETPAGESIZE, 0, 1 );
    SendDlgItemMessage( hDlg, IDC_OPENGLHO, TBM_SETRANGE, TRUE, MAKELONG (-50, +50) );
    SendDlgItemMessage( hDlg, IDC_OPENGLHO, TBM_SETPAGESIZE, 0, 1 );
    SendDlgItemMessage( hDlg, IDC_OPENGLVO, TBM_SETRANGE, TRUE, MAKELONG (-50, +50) );
    SendDlgItemMessage( hDlg, IDC_OPENGLVO, TBM_SETPAGESIZE, 0, 1 );
    SendDlgItemMessage( hDlg, IDC_OPENGLSL, TBM_SETRANGE, TRUE, MAKELONG (   0, +100) );
    SendDlgItemMessage( hDlg, IDC_OPENGLSL, TBM_SETPAGESIZE, 0, 10 );
    SendDlgItemMessage( hDlg, IDC_OPENGLSL2, TBM_SETRANGE, TRUE, MAKELONG (   0, +100) );
    SendDlgItemMessage( hDlg, IDC_OPENGLSL2, TBM_SETPAGESIZE, 0, 10 );

    SendDlgItemMessage( hDlg, IDC_OPENGLHZ, TBM_SETPOS, TRUE, workprefs.gfx_filter_horiz_zoom);
    SendDlgItemMessage( hDlg, IDC_OPENGLVZ, TBM_SETPOS, TRUE, workprefs.gfx_filter_vert_zoom);
    SendDlgItemMessage( hDlg, IDC_OPENGLHO, TBM_SETPOS, TRUE, workprefs.gfx_filter_horiz_offset);
    SendDlgItemMessage( hDlg, IDC_OPENGLVO, TBM_SETPOS, TRUE, workprefs.gfx_filter_vert_offset);
    SendDlgItemMessage( hDlg, IDC_OPENGLSL, TBM_SETPOS, TRUE, workprefs.gfx_filter_scanlines);
    SendDlgItemMessage( hDlg, IDC_OPENGLSL2, TBM_SETPOS, TRUE, workprefs.gfx_filter_scanlinelevel);
    SetDlgItemInt( hDlg, IDC_OPENGLHZV, workprefs.gfx_filter_horiz_zoom, TRUE );
    SetDlgItemInt( hDlg, IDC_OPENGLVZV, workprefs.gfx_filter_vert_zoom, TRUE );
    SetDlgItemInt( hDlg, IDC_OPENGLHOV, workprefs.gfx_filter_horiz_offset, TRUE );
    SetDlgItemInt( hDlg, IDC_OPENGLVOV, workprefs.gfx_filter_vert_offset, TRUE );
    SetDlgItemInt( hDlg, IDC_OPENGLSLV, workprefs.gfx_filter_scanlines, TRUE );
    SetDlgItemInt( hDlg, IDC_OPENGLSL2V, workprefs.gfx_filter_scanlinelevel, TRUE );

    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_RESETCONTENT, 0, 0L);

    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"Null filter");
#ifdef D3D
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"Direct3D");
#endif
#ifdef OPENGL
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"OpenGL");
#endif
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"Scale2X");
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"SuperEagle");
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"Super2xSaI");
    SendDlgItemMessage (hDlg, IDC_OPENGLBITS, CB_ADDSTRING, 0, (LPARAM)"2xSaI");

    SendDlgItemMessage( hDlg, IDC_OPENGLBITS, CB_SETCURSEL, workprefs.gfx_filter == 0 ? 1 : workprefs.gfx_filter - 1, 0 );

    SendDlgItemMessage (hDlg, IDC_OPENGLFILTER, CB_RESETCONTENT, 0, 0L);
    SendDlgItemMessage (hDlg, IDC_OPENGLFILTER, CB_ADDSTRING, 0, (LPARAM)"No filter (16-bit)");
    SendDlgItemMessage (hDlg, IDC_OPENGLFILTER, CB_ADDSTRING, 0, (LPARAM)"Bilinear (16-bit)");
    SendDlgItemMessage (hDlg, IDC_OPENGLFILTER, CB_ADDSTRING, 0, (LPARAM)"No filter (32-bit)");
    SendDlgItemMessage (hDlg, IDC_OPENGLFILTER, CB_ADDSTRING, 0, (LPARAM)"Bilinear (32-bit)");
    SendDlgItemMessage( hDlg, IDC_OPENGLFILTER, CB_SETCURSEL, workprefs.gfx_filter_filtermode, 0 );

    SendDlgItemMessage (hDlg, IDC_OPENGLSLR, CB_RESETCONTENT, 0, 0L);
    i = j = 0;
    while (scanlineratios[i * 2]) {
	int sl = scanlineratios[i * 2] * 16 + scanlineratios[i * 2 + 1];
	sprintf (txt, "%d:%d", scanlineratios[i * 2], scanlineratios[i * 2 + 1]);
	if (workprefs.gfx_filter_scanlineratio == sl)
	    j = i;
        SendDlgItemMessage (hDlg, IDC_OPENGLSLR, CB_ADDSTRING, 0, (LPARAM)txt);
	scanlineindexes[i] = sl;
	i++;
    }
    SendDlgItemMessage( hDlg, IDC_OPENGLSLR, CB_SETCURSEL, j, 0 );
}

static void values_from_hw3ddlg (HWND hDlg)
{
}

static BOOL CALLBACK hw3dDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int recursive;
    int item, item2;

    switch (msg) 
    {
    case WM_INITDIALOG:
	pages[HW3D_ID] = hDlg;
	currentpage = HW3D_ID;
	enable_for_hw3ddlg (hDlg);
	    
    case WM_USER:
	recursive++;
	enable_for_hw3ddlg( hDlg );
	values_to_hw3ddlg (hDlg);
	recursive--;
	return TRUE;
    case WM_COMMAND:
	if (wParam == IDC_OPENGLDEFAULT) {
	    currprefs.gfx_filter_horiz_zoom = workprefs.gfx_filter_horiz_zoom = 0;
	    currprefs.gfx_filter_vert_zoom = workprefs.gfx_filter_vert_zoom = 0;
	    currprefs.gfx_filter_horiz_offset = workprefs.gfx_filter_horiz_offset = 0;
	    currprefs.gfx_filter_vert_offset = workprefs.gfx_filter_vert_offset = 0;
	    values_to_hw3ddlg (hDlg);
	}
	item = SendDlgItemMessage( hDlg, IDC_OPENGLSLR, CB_GETCURSEL, 0, 0L );
	if (item != CB_ERR)
	    currprefs.gfx_filter_scanlineratio = workprefs.gfx_filter_scanlineratio = scanlineindexes[item];
	item = SendDlgItemMessage( hDlg, IDC_OPENGLMODE, CB_GETCURSEL, 0, 0L );
	if (item != CB_ERR) {
	    item2 = IsDlgButtonChecked( hDlg, IDC_OPENGLENABLE ) ? item + 1 : 0;
	    if (workprefs.gfx_filter != item2) {
		workprefs.gfx_filter = item2;
		enable_for_hw3ddlg (hDlg);
		hw3d_changed = 1;
	    }
	}
	item = SendDlgItemMessage( hDlg, IDC_OPENGLFILTER, CB_GETCURSEL, 0, 0L );
	if (item != CB_ERR)
	    workprefs.gfx_filter_filtermode = item;
	updatedisplayarea ();
	break;
    case WM_HSCROLL:
	currprefs.gfx_filter_horiz_zoom = workprefs.gfx_filter_horiz_zoom = SendMessage( GetDlgItem( hDlg, IDC_OPENGLHZ ), TBM_GETPOS, 0, 0 );
	currprefs.gfx_filter_vert_zoom = workprefs.gfx_filter_vert_zoom = SendMessage( GetDlgItem( hDlg, IDC_OPENGLVZ ), TBM_GETPOS, 0, 0 );
	currprefs.gfx_filter_horiz_offset = workprefs.gfx_filter_horiz_offset = SendMessage( GetDlgItem( hDlg, IDC_OPENGLHO ), TBM_GETPOS, 0, 0 );
	currprefs.gfx_filter_vert_offset = workprefs.gfx_filter_vert_offset = SendMessage( GetDlgItem( hDlg, IDC_OPENGLVO ), TBM_GETPOS, 0, 0 );
	currprefs.gfx_filter_scanlines = workprefs.gfx_filter_scanlines = SendMessage( GetDlgItem( hDlg, IDC_OPENGLSL ), TBM_GETPOS, 0, 0 );
	currprefs.gfx_filter_scanlinelevel = workprefs.gfx_filter_scanlinelevel = SendMessage( GetDlgItem( hDlg, IDC_OPENGLSL2 ), TBM_GETPOS, 0, 0 );
	SetDlgItemInt( hDlg, IDC_OPENGLHZV, workprefs.gfx_filter_horiz_zoom, TRUE );
	SetDlgItemInt( hDlg, IDC_OPENGLVZV, workprefs.gfx_filter_vert_zoom, TRUE );
	SetDlgItemInt( hDlg, IDC_OPENGLHOV, workprefs.gfx_filter_horiz_offset, TRUE );
        SetDlgItemInt( hDlg, IDC_OPENGLVOV, workprefs.gfx_filter_vert_offset, TRUE );
        SetDlgItemInt( hDlg, IDC_OPENGLSLV, workprefs.gfx_filter_scanlines, TRUE );
	SetDlgItemInt( hDlg, IDC_OPENGLSL2V, workprefs.gfx_filter_scanlinelevel, TRUE );
	updatedisplayarea ();
	WIN32GFX_WindowMove ();
	break;
    case WM_NOTIFY:
        return notifycheck (lParam, "gui/opengl.htm");
    }
    return FALSE;
}

#endif

#ifdef AVIOUTPUT
static void values_to_avioutputdlg(HWND hDlg)
{
	char tmpstr[256];
	
        updatewinfsmode (&workprefs);
	SetDlgItemText(hDlg, IDC_AVIOUTPUT_FILETEXT, avioutput_filename);
	
	sprintf(tmpstr, "%d fps", avioutput_fps);
	SendMessage(GetDlgItem(hDlg, IDC_AVIOUTPUT_FPS_STATIC), WM_SETTEXT, (WPARAM) 0, (LPARAM) tmpstr);
	
	sprintf(tmpstr, "Actual: %d x %d", workprefs.gfx_width, workprefs.gfx_height);
	SendMessage(GetDlgItem(hDlg, IDC_AVIOUTPUT_DIMENSIONS_STATIC), WM_SETTEXT, (WPARAM) 0, (LPARAM) tmpstr);
	
	switch(avioutput_fps)
	{
	case VBLANK_HZ_PAL:
		CheckRadioButton(hDlg, IDC_AVIOUTPUT_PAL, IDC_AVIOUTPUT_NTSC, IDC_AVIOUTPUT_PAL);
		break;
		
	case VBLANK_HZ_NTSC:
		CheckRadioButton(hDlg, IDC_AVIOUTPUT_PAL, IDC_AVIOUTPUT_NTSC, IDC_AVIOUTPUT_NTSC);
		break;
		
	default:
		CheckDlgButton(hDlg, IDC_AVIOUTPUT_PAL, BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_AVIOUTPUT_NTSC, BST_UNCHECKED);
		break;
	}
	
	CheckRadioButton(hDlg, IDC_AVIOUTPUT_8BIT, IDC_AVIOUTPUT_24BIT, (avioutput_bits == 8) ? IDC_AVIOUTPUT_8BIT : IDC_AVIOUTPUT_24BIT);
}

static void values_from_avioutputdlg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL success = FALSE;
	
	//AVIOutput_End(); // <sane> TODO if sound settings change, must cease AVI capture
	
        updatewinfsmode (&workprefs);

	avioutput_width = GetDlgItemInt(hDlg, IDC_AVIOUTPUT_WIDTH, &success, FALSE);
	
	if(!success || (avioutput_width < 1))
		avioutput_width = workprefs.gfx_width;
	
	avioutput_height = GetDlgItemInt(hDlg, IDC_AVIOUTPUT_HEIGHT, &success, FALSE);
	
	if(!success || (avioutput_height < 1))
		avioutput_height = workprefs.gfx_height;
	
	avioutput_fps = SendMessage(GetDlgItem(hDlg, IDC_AVIOUTPUT_FPS), TBM_GETPOS, 0, 0);
	
	// = IsDlgButtonChecked(hDlg, );
}

static void enable_for_avioutputdlg(HWND hDlg)
{
	static int sound_bits = 0;
	static int sound_freq = 0;
	static int stereo = 0;
	
	static int avioutput_width_old = 0;
	static int avioutput_height_old = 0;
	static int avioutput_bits_old = 0;
	static int avioutput_fps_old = 0;
	
        EnableWindow(GetDlgItem(hDlg, IDC_SCREENSHOT), full_property_sheet ? FALSE : TRUE);

		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_PAL), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_NTSC), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_FPS), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_8BIT), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_24BIT), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_WIDTH), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_HEIGHT), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_FILE), TRUE);
		
		if(workprefs.produce_sound < 2)
		{
			EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO_STATIC), FALSE);
			avioutput_audio = 0;
		}
		else
		{
			EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO_STATIC), TRUE);
			
			if((workprefs.sound_bits != sound_bits) || (workprefs.sound_freq != sound_freq) || (workprefs.stereo != stereo))
				avioutput_audio = 0;
		}
		
		if(!avioutput_audio)
		{
			CheckDlgButton(hDlg, IDC_AVIOUTPUT_AUDIO, BST_UNCHECKED);
			SetWindowText(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO_STATIC), "no codec selected");
		}
		
		if(!avioutput_video || (avioutput_width != avioutput_width_old) || (avioutput_height != avioutput_height_old) || (avioutput_bits != avioutput_bits_old) || (avioutput_fps != avioutput_fps_old))
		{
			CheckDlgButton(hDlg, IDC_AVIOUTPUT_VIDEO, BST_UNCHECKED);
			SetWindowText(GetDlgItem(hDlg, IDC_AVIOUTPUT_VIDEO_STATIC), "no codec selected");
			
			RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		}

	sound_bits = workprefs.sound_bits;
	sound_freq = workprefs.sound_freq;
	stereo = workprefs.stereo;
	
	avioutput_width_old = avioutput_width;
	avioutput_height_old = avioutput_height;
	avioutput_bits_old = avioutput_bits;
	avioutput_fps_old = avioutput_fps;
}

static BOOL CALLBACK AVIOutputDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int recursive = 0;
	
	switch(msg)
	{
	case WM_INITDIALOG:
		pages[AVIOUTPUT_ID] = hDlg;
		currentpage = AVIOUTPUT_ID;
		SendDlgItemMessage(hDlg, IDC_AVIOUTPUT_FPS, TBM_SETRANGE, TRUE, MAKELONG(1, VBLANK_HZ_NTSC));
		SendDlgItemMessage(hDlg, IDC_AVIOUTPUT_FPS, TBM_SETPOS, TRUE, VBLANK_HZ_PAL);
		SendMessage(hDlg, WM_HSCROLL, (WPARAM) NULL, (LPARAM) NULL);
		
	case WM_USER:
		recursive++;
		
		values_to_avioutputdlg(hDlg);
		enable_for_avioutputdlg(hDlg);
		
		recursive--;
		return TRUE;
		
	case WM_HSCROLL:
		{
			recursive++;
			
			values_from_avioutputdlg(hDlg, msg, wParam, lParam);
			values_to_avioutputdlg(hDlg);
			enable_for_avioutputdlg(hDlg);
			
			recursive--;
			
			return TRUE;
		}
		
	case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC hDC = BeginPaint(hDlg, &Paint);
			
			RECT AmigaRect; // Virtual amiga screen rect
			RECT VideoRect; // AVI Frame dimensions
			
			RECT ScreenRect;
			RECT ClientRect;
			
			POINT p;
			
			HWND hWnd = GetDlgItem(hDlg, IDC_AVIOUTPUT_FRAME);
			
			GetClientRect(hWnd, &ClientRect);
			
			p.x = ClientRect.left + 1;
			p.y = ClientRect.top + 1;
			ClientToScreen(hWnd, &p);
			ScreenToClient(hDlg, &p);
			
			ClientRect.right -= 2;
			ClientRect.bottom -= 2;
			
			ScreenRect.left = p.x;
			ScreenRect.top = p.y;
			ScreenRect.right = ScreenRect.left + ClientRect.right;
			ScreenRect.bottom = ScreenRect.top + ClientRect.bottom;
			
			{
				double x = (double)((avioutput_width > workprefs.gfx_width) ? workprefs.gfx_width : avioutput_width);
				double y = (double)((avioutput_height > workprefs.gfx_height) ? workprefs.gfx_height : avioutput_height);
				
				AmigaRect.left = p.x;
				AmigaRect.top = p.y;
				
				AmigaRect.right = (LONG)(((double)ClientRect.right / (double)avioutput_width) * x);
				AmigaRect.bottom = (LONG)(((double)ClientRect.bottom / (double)avioutput_height) * y);
				
				// center
				AmigaRect.left += ((ClientRect.right / 2) - (AmigaRect.right / 2));
				AmigaRect.right += AmigaRect.left;
				
				AmigaRect.top += (ClientRect.bottom / 2) - (AmigaRect.bottom / 2);
				AmigaRect.bottom += AmigaRect.top;
				
				VideoRect.left = p.x;
				VideoRect.top = p.y;
				
				VideoRect.right = (LONG)(((double)ClientRect.right / (double)workprefs.gfx_width) * x);
				VideoRect.bottom = (LONG)(((double)ClientRect.bottom / (double)workprefs.gfx_height) * y);
				
				// center
				VideoRect.left += (ClientRect.right / 2) - (VideoRect.right / 2);
				VideoRect.right += VideoRect.left;
				
				VideoRect.top += (ClientRect.bottom / 2) - (VideoRect.bottom / 2);
				VideoRect.bottom += VideoRect.top;
			}
			
			{
				HBRUSH hbrush_red = CreateSolidBrush(RGB(255, 0, 0));
				HBRUSH hbrush_gray = CreateSolidBrush(RGB(100, 100, 100));
				
				FillRect(hDC, &ScreenRect, GetStockObject(BLACK_BRUSH));
				
				//if(!BlitAmigaScreenThing(hDC, &AmigaRect))
					FillRect(hDC, &AmigaRect, hbrush_gray);
				
				FrameRect(hDC, &VideoRect, hbrush_red);
				
				DeleteObject(hbrush_red);
				DeleteObject(hbrush_gray);
			}
			
			EndPaint(hDlg, &Paint);
			return TRUE;
		}
		
	case WM_COMMAND:
		if(recursive > 0)
			break;
		
		recursive++;
		
		switch(wParam)
		{
		case IDC_SCREENSHOT:
			screenshot(1);
			break;
			
		case IDC_AVIOUTPUT_8BIT:
			if(avioutput_bits == 24)
				avioutput_video = 0;
			
			avioutput_bits = 8;
			break;
			
		case IDC_AVIOUTPUT_24BIT:
			if(avioutput_bits == 8)
				avioutput_video = 0;
			
			avioutput_bits = 24;
			break;
			
		case IDC_AVIOUTPUT_PAL:
			SendDlgItemMessage(hDlg, IDC_AVIOUTPUT_FPS, TBM_SETPOS, TRUE, VBLANK_HZ_PAL);
			SendMessage(hDlg, WM_HSCROLL, (WPARAM) NULL, (LPARAM) NULL);
			break;
			
		case IDC_AVIOUTPUT_NTSC:
			SendDlgItemMessage(hDlg, IDC_AVIOUTPUT_FPS, TBM_SETPOS, TRUE, VBLANK_HZ_NTSC);
			SendMessage(hDlg, WM_HSCROLL, (WPARAM) NULL, (LPARAM) NULL);
			break;
			
		case IDC_AVIOUTPUT_AUDIO:
			{
				if(IsDlgButtonChecked(hDlg, IDC_AVIOUTPUT_AUDIO) == BST_CHECKED)
				{
					LPSTR string;
					
					if(string = AVIOutput_ChooseAudioCodec(hDlg))
					{
						avioutput_audio = AVIAUDIO_AVI;
						
						SetWindowText(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO_STATIC), string);
					}
					else
						avioutput_audio = 0;
				}
				else
					avioutput_audio = 0;
				
				break;
			}
			
		case IDC_AVIOUTPUT_VIDEO:
			{
				if(IsDlgButtonChecked(hDlg, IDC_AVIOUTPUT_VIDEO) == BST_CHECKED)
				{
					LPSTR string;
					
					if(string = AVIOutput_ChooseVideoCodec(hDlg))
					{
						avioutput_video = 1;
						
						SetWindowText(GetDlgItem(hDlg, IDC_AVIOUTPUT_VIDEO_STATIC), string);
					}
					else
						avioutput_video = 0;
				}
				else
					avioutput_video = 0;
				
				break;
			}
			
		case IDC_AVIOUTPUT_FILE:
			{
				OPENFILENAME ofn;
				
				ZeroMemory(&ofn, sizeof(OPENFILENAME));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hDlg;
				ofn.hInstance = hInst;
				ofn.Flags = OFN_EXTENSIONDIFFERENT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
				ofn.lpstrCustomFilter = NULL;
				ofn.nMaxCustFilter = 0;
				ofn.nFilterIndex = 0;
				ofn.lpstrFile = avioutput_filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.lpfnHook = NULL;
				ofn.lpTemplateName = NULL;
				ofn.lCustData = 0;
				ofn.lpstrFilter = "Video Clip (*.avi)\0*.avi\0Wave Sound (*.wav)\0";
				
				if(!GetSaveFileName(&ofn))
					break;
				if (ofn.nFilterIndex == 2) {
				    avioutput_audio = AVIAUDIO_WAV;
				    SetWindowText(GetDlgItem(hDlg, IDC_AVIOUTPUT_AUDIO_STATIC), "Wave (internal)");
				    if (strlen (avioutput_filename) > 4 && !stricmp (avioutput_filename + strlen (avioutput_filename) - 4, ".avi"))
					strcpy (avioutput_filename + strlen (avioutput_filename) - 4, ".wav");
				}
				break;
			}
		}
		
		values_from_avioutputdlg(hDlg, msg, wParam, lParam);
		values_to_avioutputdlg(hDlg);
		enable_for_avioutputdlg(hDlg);
		
		recursive--;
		
		return TRUE;
		
	case WM_NOTIFY:
		return notifycheck(lParam, "gui/output.htm");
		break;
	}
	
	return FALSE;
}
#endif

static void CALLBACK InitPropertySheet (HWND hDlg, UINT msg, LPARAM lParam)
{
    int i;
    hPropertySheet = hDlg;

    switch (msg) 
    {
    case PSCB_INITIALIZED:
        if (full_property_sheet) {
	    for (i = 0; i < MAX_C_PAGES; i++)
		pages[i] = NULL;
        }
    	break;
    }
}

static int init_page (PROPSHEETPAGE *ppage, int tmpl, int icon, int title,
               BOOL (CALLBACK FAR *func) (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam))
{
    static id = 0;
    ppage[id].dwSize = sizeof (PROPSHEETPAGE);
    ppage[id].dwFlags = PSP_USETITLE | PSP_USEICONID | ((id != ABOUT_ID ) ? PSP_HASHELP : 0);
    ppage[id].pszTemplate = MAKEINTRESOURCE (tmpl);
    ppage[id].hInstance = hUIDLL ? hUIDLL : hInst;
    ppage[id].pszIcon = MAKEINTRESOURCE (icon);

    if( hUIDLL )
    {
	LPTSTR lpstrTitle = calloc( 1, MAX_PATH );
	LoadString( hUIDLL, title, lpstrTitle, MAX_PATH );
	ppage[id].pszTitle = lpstrTitle;
    }
    else
    {
	ppage[id].pszTitle = MAKEINTRESOURCE (title);
    }
    ppage[id].pfnDlgProc = func;
    ppage[id].lParam = 0;
    ppage[id].pfnCallback = NULL;
    ppage[id].pcRefParent = NULL;
    id++;
    return id - 1;
}

static int GetSettings (int all_options)
{
    static int init_called = 0;
    int psresult;
    static PROPSHEETPAGE ppage[MAX_C_PAGES];
    PROPSHEETHEADER pHeader;

    full_property_sheet = all_options;
    allow_quit = all_options;
    pguiprefs = &currprefs;
    default_prefs( &workprefs );

    WIN32GUI_LoadUIString( IDS_NONE, szNone, MAX_PATH );

    prefs_to_gui (&changed_prefs);

    if( !init_called )
    {
	LOADSAVE_ID = init_page (ppage, IDD_LOADSAVE, IDI_LOADSAVE, IDS_LOADSAVE, LoadSaveDlgProc);
	MEMORY_ID = init_page (ppage, IDD_MEMORY, IDI_MEMORY, IDS_MEMORY, MemoryDlgProc);
	KICKSTART_ID = init_page (ppage, IDD_KICKSTART, IDI_MEMORY, IDS_KICKSTART, KickstartDlgProc);
	CPU_ID = init_page (ppage, IDD_CPU, IDI_CPU, IDS_CPU, CPUDlgProc);
	DISPLAY_ID = init_page (ppage, IDD_DISPLAY, IDI_DISPLAY, IDS_DISPLAY, DisplayDlgProc);
#if defined(OPENGL) || defined (D3D)
	HW3D_ID = init_page (ppage, IDD_OPENGL, IDI_OPENGL, IDS_OPENGL, hw3dDlgProc);
#endif
	CHIPSET_ID = init_page (ppage, IDD_CHIPSET, IDI_DISPLAY, IDS_CHIPSET, ChipsetDlgProc);
	SOUND_ID = init_page (ppage, IDD_SOUND, IDI_SOUND, IDS_SOUND, SoundDlgProc);
	FLOPPY_ID = init_page (ppage, IDD_FLOPPY, IDI_FLOPPY, IDS_FLOPPY, FloppyDlgProc);
#ifdef FILESYS
	HARDDISK_ID = init_page (ppage, IDD_HARDDISK, IDI_HARDDISK, IDS_HARDDISK, HarddiskDlgProc);
#endif
	PORTS_ID = init_page (ppage, IDD_PORTS, IDI_PORTS, IDS_PORTS, PortsDlgProc);
	INPUT_ID = init_page (ppage, IDD_INPUT, IDI_INPUT, IDS_INPUT, InputDlgProc);
	MISC_ID = init_page (ppage, IDD_MISC, IDI_MISC, IDS_MISC, MiscDlgProc);
#ifdef AVIOUTPUT
	AVIOUTPUT_ID = init_page (ppage, IDD_AVIOUTPUT, IDI_AVIOUTPUT, IDS_AVIOUTPUT, AVIOutputDlgProc);
#endif
	ABOUT_ID = init_page (ppage, IDD_ABOUT, IDI_ABOUT, IDS_ABOUT, AboutDlgProc);
	C_PAGES = ABOUT_ID + 1;
	init_called = 1;
    }

    pHeader.dwSize = sizeof (PROPSHEETHEADER);
    pHeader.dwFlags = PSH_PROPSHEETPAGE | PSH_PROPTITLE | PSH_USEICONID | PSH_USECALLBACK | PSH_NOAPPLYNOW | PSH_HASHELP;
    if (! all_options && workprefs.gfx_afullscreen && workprefs.gfx_width < 640)
	pHeader.hwndParent = NULL;
    else
	pHeader.hwndParent = hAmigaWnd;
    pHeader.hInstance = hInst;
    pHeader.pszIcon = MAKEINTRESOURCE (IDI_APPICON);
    pHeader.pszCaption = "WinUAE";
    pHeader.nPages = C_PAGES;
    pHeader.nStartPage = currentpage;
    pHeader.ppsp = ppage;
    pHeader.pfnCallback = (PFNPROPSHEETCALLBACK) InitPropertySheet;

    psresult = PropertySheet (&pHeader);

    if (quit_program)
        psresult = -2;

    return psresult;
}

int gui_init (void)
{
    int ret;
    
    ret = GetSettings(1);
    if (ret) {
#ifdef AVIOUTPUT
	AVIOutput_Begin ();
#endif
    }
    return ret;
}

int gui_update (void)
{
    return 1;
}

void gui_exit (void)
{
#ifdef PARALLEL_PORT
    closeprinter(); // Bernd Roesch
#endif
}

extern HWND hStatusWnd;
struct gui_info gui_data;

void check_prefs_changed_gui( void )
{
}

void gui_hd_led (int led)
{
    static int resetcounter;

    int old = gui_data.hd;
    if (led == 0) {
	resetcounter--;
	if (resetcounter > 0)
	    return;
    }
    gui_data.hd = led;
    resetcounter = 6;
    if (old != gui_data.hd)
	gui_led (5, gui_data.hd);
}

void gui_cd_led (int led)
{
    static int resetcounter;

    int old = gui_data.cd;
    if (led == 0) {
	resetcounter--;
	if (resetcounter > 0)
	    return;
    }
    gui_data.cd = led;
    resetcounter = 6;
    if (old != gui_data.cd)
	gui_led (6, gui_data.cd);
}

void gui_fps (int fps)
{
    gui_data.fps = fps;
    gui_led (7, 0);
}

void gui_led (int led, int on)
{
    WORD type;
    static char drive_text[NUM_LEDS * 16];
    char *ptr;
    int pos = -1;

    indicator_leds (led, on);
    if( hStatusWnd )
    {
        if (on)
	    type = SBT_POPOUT;
	else
	    type = 0;
	if( led >= 1 && led <= 4 ) {
	    pos = 4 + (led - 1);
	    ptr = drive_text + pos * 16;
	    if (gui_data.drive_disabled[led - 1])
		strcpy (ptr, "");
	    else
		sprintf (ptr , "%02d", gui_data.drive_track[led - 1]);
	} else if (led == 0) {
	    pos = 1;
	    ptr = strcpy (drive_text + pos * 16, "Power");
	} else if (led == 5) {
	    pos = 2;
	    ptr = strcpy (drive_text + pos * 16, "HD");
	} else if (led == 6) {
	    pos = 3;
	    ptr = strcpy (drive_text + pos * 16, "CD");
	} else if (led == 7) {
	    pos = 0;
	    ptr = drive_text + pos * 16;
	    sprintf(ptr, "FPS:%.1f", (double)((gui_data.fps + 1) / 10.0) );
	}
	if (pos >= 0)
	    PostMessage (hStatusWnd, SB_SETTEXT, (WPARAM) ((pos + 1) | type), (LPARAM) ptr);
    }
}

void gui_filename (int num, const char *name)
{
}

void gui_message (const char *format,...)
{
    char msg[2048];
    char szTitle[ MAX_PATH ];
    va_list parms;
    int flipflop = 0;
    int fullscreen = 0;
    HWND window = NULL;

    if( DirectDraw_GetCooperativeLevel( &window, &fullscreen ) && fullscreen )
        flipflop = 1;
    pause_sound ();
    if( flipflop )
        ShowWindow( window, SW_MINIMIZE );

    va_start (parms, format);
    vsprintf( msg, format, parms );
    va_end (parms);
    write_log( msg );

    WIN32GUI_LoadUIString( IDS_ERRORTITLE, szTitle, MAX_PATH );

    MessageBox( NULL, msg, szTitle, MB_OK | MB_ICONWARNING | MB_TASKMODAL | MB_SETFOREGROUND );

    if( flipflop )
        ShowWindow( window, SW_RESTORE );
    resume_sound();
    setmouseactive( 0 );
}

void pre_gui_message (const char *format,...)
{
    char msg[2048];
    char szTitle[ MAX_PATH ];
    va_list parms;

    va_start (parms, format);
    vsprintf( msg, format, parms );
    va_end (parms);
    write_log( msg );

    WIN32GUI_LoadUIString( IDS_ERRORTITLE, szTitle, MAX_PATH );

    MessageBox( NULL, msg, szTitle, MB_OK | MB_ICONWARNING | MB_TASKMODAL | MB_SETFOREGROUND );

}

void gui_lock (void)
{
}

void gui_unlock (void)
{
}
