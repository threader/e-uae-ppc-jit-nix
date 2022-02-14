/*
 * UAE - The Un*x Amiga Emulator
 *
 * Win32 Drawing and DirectX interface
 *
 * Copyright 1997-1998 Mathias Ortmann
 * Copyright 1997-2000 Brian King
 */

#include "config.h"
#include "sysconfig.h"

#include <stdlib.h>
#include <stdarg.h>

#include <windows.h>
#include <commctrl.h>
#include <ddraw.h>

#include "sysdeps.h"
#include "options.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "events.h"
#include "xwin.h"
#include "keyboard.h"
#include "drawing.h"
#include "dxwrap.h"
#include "picasso96_win.h"
#include "win32.h"
#include "win32gfx.h"
#include "win32gui.h"
#include "sound.h"
#include "inputdevice.h"
#include "opengl.h"
#include "direct3d.h"
#include "midi.h"
#include "gui.h"
#include "serial.h"
#include "avioutput.h"
#include "filter.h"
#include "parser.h"

#define AMIGA_WIDTH_MAX 704
#define AMIGA_HEIGHT_MAX 564

#define DM_DX_FULLSCREEN 1
#define DM_W_FULLSCREEN 2
#define DM_OVERLAY 4
#define DM_OPENGL 8
#define DM_DX_DIRECT 16
#define DM_PICASSO96 32
#define DM_DDRAW 64
#define DM_DC 128
#define DM_D3D 256
#define DM_D3D_FULLSCREEN 512
#define DM_SWSCALE 1024

struct winuae_modes {
    int fallback;
    char *name;
    unsigned int aflags;
    unsigned int pflags;
};
struct winuae_currentmode {
    struct winuae_modes *mode;
    struct winuae_modes *pmode[2];
    struct winuae_modes *amode[2];
    unsigned int flags;
    int current_width, current_height, current_depth, real_depth, pitch;
    int amiga_width, amiga_height;
    int frequency;
    int mapping_is_mainscreen;
    int initdone;
    int modeindex;
    LPPALETTEENTRY pal;
};

struct PicassoResolution DisplayModes[MAX_PICASSO_MODES];

static struct winuae_currentmode currentmodestruct;
int display_change_requested;
extern int console_logging;

#define SM_WINDOW 0
#define SM_WINDOW_OVERLAY 1
#define SM_FULLSCREEN_DX 2
#define SM_OPENGL_WINDOW 3
#define SM_OPENGL_FULLSCREEN_W 4
#define SM_OPENGL_FULLSCREEN_DX 5
#define SM_D3D_WINDOW 6
#define SM_D3D_FULLSCREEN_DX 7
#define SM_NONE 7

static struct winuae_modes wmodes[] =
{
    {
	0, "Windowed",
	DM_DDRAW,
	DM_PICASSO96 | DM_DDRAW
    },
    {
	0, "Windowed Overlay",
	DM_OVERLAY | DM_DX_DIRECT | DM_DDRAW,
	DM_OVERLAY | DM_DX_DIRECT | DM_DDRAW | DM_PICASSO96
    },
    {
	1, "Fullscreen",
	DM_DX_FULLSCREEN | DM_DX_DIRECT | DM_DDRAW,
	DM_DX_FULLSCREEN | DM_DX_DIRECT | DM_DDRAW | DM_PICASSO96
    },
    {
	1, "Windowed OpenGL",
	DM_OPENGL | DM_DC,
	0
    },
    {
	3, "Fullscreen OpenGL",
	DM_OPENGL | DM_W_FULLSCREEN | DM_DC,
	0
    },
    {
	3, "DirectDraw Fullscreen OpenGL",
	DM_OPENGL | DM_DX_FULLSCREEN | DM_DC,
	0
    },
    {
	0, "Windowed Direct3D",
	DM_D3D,
	0
    },
    {
	0, "Fullscreen Direct3D",
	DM_D3D | DM_D3D_FULLSCREEN,
	0
    },
    {
	0, "none",
	0,
	0
    }
};

static struct winuae_currentmode *currentmode = &currentmodestruct;
static int gfx_tempbitmap;

static int modefallback (unsigned int mask)
{
    if (mask == DM_OVERLAY) {
	if (currentmode->amode[0] == &wmodes[SM_WINDOW_OVERLAY])
	    currentmode->amode[0] = &wmodes[0];
	if (currentmode->pmode[0] == &wmodes[SM_WINDOW_OVERLAY])
	    currentmode->pmode[0] = &wmodes[0];
	return 1;
    }
    if (!picasso_on) {
	if (currprefs.gfx_afullscreen) {
	    currprefs.gfx_afullscreen = changed_prefs.gfx_afullscreen = 0;
	    updatewinfsmode (&currprefs);
	    return 1;
	} else {
	    if (currentmode->amode[0] == &wmodes[0])
		return 0;
	    currentmode->amode[0] = &wmodes[0];
	    return 1;
	}
    } else {
	if (currprefs.gfx_pfullscreen) {
	    currprefs.gfx_pfullscreen = changed_prefs.gfx_pfullscreen = 0;
	    return 1;
	} else {
	    if (currentmode->pmode[0] == &wmodes[0]) {
		currprefs.gfx_pfullscreen = changed_prefs.gfx_pfullscreen = 1;
		return 1;
	    }
	    currentmode->pmode[0] = &wmodes[0];
	    return 1;
	}
    }
    return 0;
}

int screen_is_picasso = 0;

int WIN32GFX_IsPicassoScreen( void )
{
    return screen_is_picasso;
}

void WIN32GFX_DisablePicasso( void )
{
    picasso_requested_on = 0;
    picasso_on = 0;
}

void WIN32GFX_EnablePicasso( void )
{
    picasso_requested_on = 1;
}

void WIN32GFX_DisplayChangeRequested( void )
{
    display_change_requested = 1;
}

int isscreen (void)
{
    return hMainWnd ? 1 : 0;
}

int isfullscreen (void)
{
    if (screen_is_picasso)
	return currprefs.gfx_pfullscreen;
    else
	return currprefs.gfx_afullscreen;
}

int WIN32GFX_GetDepth (int real)
{
    if (!currentmode->real_depth)
	return currentmode->current_depth;
    return real ? currentmode->real_depth : currentmode->current_depth;
}

int WIN32GFX_GetWidth( void )
{
    return currentmode->current_width;
}

int WIN32GFX_GetHeight( void )
{
    return currentmode->current_height;
}

#include "dxwrap.h"

static BOOL doInit (void);

uae_u32 default_freq = 0;

HWND hStatusWnd = NULL;
HINSTANCE hDDraw = NULL;
uae_u16 picasso96_pixel_format = RGBFF_CHUNKY;

/* For the DX_Invalidate() and gfx_unlock_picasso() functions */
static int p96_double_buffer_first, p96_double_buffer_last, p96_double_buffer_needs_flushing = 0;

static char scrlinebuf[4096];	/* this is too large, but let's rather play on the safe side here */

static int rgbformat_bits (RGBFTYPE t)
{
    unsigned long f = 1 << t;
    return ((f & RGBMASK_8BIT) != 0 ? 8
	    : (f & RGBMASK_15BIT) != 0 ? 15
	    : (f & RGBMASK_16BIT) != 0 ? 16
	    : (f & RGBMASK_24BIT) != 0 ? 24
	    : (f & RGBMASK_32BIT) != 0 ? 32
	    : 0);
}

static DEVMODE dmScreenSettings;
static volatile cdsthread_ret;

static void cdsthread (void *dummy)
{
    int ret = ChangeDisplaySettings (&dmScreenSettings, CDS_FULLSCREEN);
    if (ret != DISP_CHANGE_SUCCESSFUL && dmScreenSettings.dmDisplayFrequency > 0) {
        dmScreenSettings.dmFields &= ~DM_DISPLAYFREQUENCY;
        ret = ChangeDisplaySettings (&dmScreenSettings, CDS_FULLSCREEN);
    }
    if (ret != DISP_CHANGE_SUCCESSFUL) {
	cdsthread_ret = 0;
	return;
    }
    cdsthread_ret = 1;
}

#include <process.h>
static int do_changedisplaysettings (int width, int height, int bits, int freq)
{
    memset (&dmScreenSettings, 0, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = width;
    dmScreenSettings.dmPelsHeight = height;
    dmScreenSettings.dmBitsPerPel = bits;
    dmScreenSettings.dmDisplayFrequency = freq;
    dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | (freq > 0 ? DM_DISPLAYFREQUENCY : 0);
    cdsthread_ret = -1;
    _beginthread (&cdsthread, 0, 0);
    while (cdsthread_ret < 0)
	Sleep (10);
    return cdsthread_ret;
}


static int set_ddraw (void)
{
    HRESULT ddrval;
    int bits = (currentmode->current_depth + 7) & ~7;
    int width = currentmode->current_width;
    int height = currentmode->current_height;
    int freq = currentmode->frequency;
    int dxfullscreen, wfullscreen, dd, overlay;

    dxfullscreen = (currentmode->flags & DM_DX_FULLSCREEN) ? TRUE : FALSE;
    wfullscreen = (currentmode->flags & DM_W_FULLSCREEN) ? TRUE : FALSE;
    dd = (currentmode->flags & DM_DDRAW) ? TRUE : FALSE;
    overlay = (currentmode->flags & DM_OVERLAY) ? TRUE : FALSE;

    ddrval = DirectDraw_SetCooperativeLevel( hAmigaWnd, dxfullscreen);
    if (ddrval != DD_OK)
	goto oops;

    if (dxfullscreen) 
    {
        write_log( "set_ddraw: Trying %dx%d, bits=%d, refreshrate=%d\n", width, height, bits, freq );
        ddrval = DirectDraw_SetDisplayMode( width, height, bits, freq );
        if (ddrval != DD_OK)
        {
	    write_log ("set_ddraw: failed, trying without forced refresh rate\n");
            ddrval = DirectDraw_SetDisplayMode( width, height, bits, 0 );
	    if (ddrval != DD_OK) {
		write_log( "set_ddraw: Couldn't SetDisplayMode()\n" );
		goto oops;
	    }
        }

	ddrval = DirectDraw_GetDisplayMode();
	if (ddrval != DD_OK)
        {
            write_log( "set_ddraw: Couldn't GetDisplayMode()\n" );
	    goto oops;
        }
    } else if (wfullscreen) {
	if (!do_changedisplaysettings (width, height, bits, currentmode->frequency))
	    goto oops2;
    }

    if (dd) {
        ddrval = DirectDraw_CreateClipper();
        if (ddrval != DD_OK)
	{
	    write_log( "set_ddraw: No clipping support\n" );
	    goto oops;
	}
	ddrval = DirectDraw_CreateSurface( width, height );
	if( ddrval != DD_OK )
	{
	    write_log( "set_ddraw: Couldn't CreateSurface() for primary because %s.\n", DXError( ddrval ) );
	    goto oops;
	}
	if( DirectDraw_GetPrimaryBitCount() != (unsigned)bits && overlay)
	{
	    ddrval = DirectDraw_CreateOverlaySurface( width, height, bits );
	    if( ddrval != DD_OK )
	    {
		write_log( "set_ddraw: Couldn't CreateOverlaySurface(%d,%d,%d) because %s.\n", width, height, bits, DXError( ddrval ) );
		goto oops2;
	    }
	}
	else
	{
	    overlay = 0;
	}

        DirectDraw_ClearSurfaces();

	if( !DirectDraw_DetermineLocking( dxfullscreen ) )
	{
	    write_log( "set_ddraw: Couldn't determine locking.\n" );
	    goto oops;
	}

	ddrval = DirectDraw_SetClipper( hAmigaWnd );

	if (ddrval != DD_OK)
	{
	    write_log( "set_ddraw: Couldn't SetHWnd()\n" );
	    goto oops;
	}

        if (bits == 8) {
	    ddrval = DirectDraw_CreatePalette( currentmode->pal );
	    if (ddrval != DD_OK)
	    {
		write_log( "set_ddraw: Couldn't CreatePalette()\n" );
		goto oops;
	    }
	}
	currentmode->pitch = DirectDraw_GetSurfacePitch();
    }

    write_log( "set_ddraw() called, and is %dx%d@%d-bytes\n", width, height, bits );
    return 1;

oops:
    write_log("set_ddraw(): DirectDraw initialization failed with\n%s\n", DXError( ddrval ));
oops2:
    return 0;
}

HRESULT CALLBACK modesCallback( LPDDSURFACEDESC2 modeDesc, LPVOID context )
{
    RGBFTYPE colortype;
    int i, j, ct, depth;

    colortype = DirectDraw_GetSurfacePixelFormat( modeDesc );
    if (colortype == RGBFB_NONE || colortype == RGBFB_R8G8B8 || colortype == RGBFB_B8G8R8 )
	return DDENUMRET_OK;
    ct = 1 << colortype;
    depth = 0;
    if (ct & RGBMASK_8BIT)
	depth = 1;
    else if (ct & (RGBMASK_15BIT | RGBMASK_16BIT))
	depth = 2;
    else if (ct & RGBMASK_24BIT)
	depth = 3;
    else if (ct & RGBMASK_32BIT)
	depth = 4;
    if (depth == 0)
	return DDENUMRET_OK;
    i = 0;
    while (DisplayModes[i].depth >= 0) {
	if (DisplayModes[i].depth == depth && DisplayModes[i].res.width == modeDesc->dwWidth && DisplayModes[i].res.height == modeDesc->dwHeight) {
	    for (j = 0; j < MAX_REFRESH_RATES; j++) {
		if (DisplayModes[i].refresh[j] == 0 || DisplayModes[i].refresh[j] == modeDesc->dwRefreshRate)
		    break;
	    }
	    if (j < MAX_REFRESH_RATES) {
		DisplayModes[i].refresh[j] = modeDesc->dwRefreshRate;
		DisplayModes[i].refresh[j + 1] = 0;
		return DDENUMRET_OK;
	    }
	}
	i++;
    }
    picasso96_pixel_format |= ct;
    i = 0;
    while (DisplayModes[i].depth >= 0)
	i++;
    if (i >= MAX_PICASSO_MODES - 1)
	return DDENUMRET_OK;
    DisplayModes[i].res.width = modeDesc->dwWidth;
    DisplayModes[i].res.height = modeDesc->dwHeight;
    DisplayModes[i].depth = depth;
    DisplayModes[i].refresh[0] = modeDesc->dwRefreshRate;
    DisplayModes[i].refresh[1] = 0;
    DisplayModes[i].colormodes = ct;
    DisplayModes[i + 1].depth = -1;
    sprintf(DisplayModes[i].name, "%dx%d, %d-bit",
        DisplayModes[i].res.width, DisplayModes[i].res.height, DisplayModes[i].depth * 8);
    return DDENUMRET_OK;
}

static int our_possible_depths[] = { 8, 15, 16, 24, 32 };

RGBFTYPE WIN32GFX_FigurePixelFormats( RGBFTYPE colortype )
{
    HRESULT ddrval;
    int got_16bit_mode = 0;
    int window_created = 0;
    struct PicassoResolution *dm;
    int i;

    if( colortype == 0 ) /* Need to query a 16-bit display mode for its pixel-format.  Do this by opening such a screen */
    {
        hAmigaWnd = CreateWindowEx (WS_EX_TOPMOST,
			       "AmigaPowah", VersionStr,
			       WS_VISIBLE | WS_POPUP,
			       CW_USEDEFAULT, CW_USEDEFAULT,
			       1,//GetSystemMetrics (SM_CXSCREEN),
			       1,//GetSystemMetrics (SM_CYSCREEN),
			       0, NULL, 0, NULL);
        if( hAmigaWnd )
        {
            window_created = 1;
            ddrval = DirectDraw_SetCooperativeLevel( hAmigaWnd, TRUE ); /* TRUE indicates full-screen */
            if( ddrval != DD_OK )
            {
		write_log( "WIN32GFX_FigurePixelFormats: ERROR -  %s\n", DXError(ddrval) );
	        gui_message( "WIN32GFX_FigurePixelFormats: ERROR - %s\n", DXError(ddrval) );
	        goto out;
            }
        }
        else
        {
	    write_log( "WIN32GFX_FigurePixelFormats: ERROR - test-window could not be created.\n" );
            gui_message( "WIN32GFX_FigurePixelFormats: ERROR - test-window could not be created.\n" );
        }
    }
    else
    {
        got_16bit_mode = 1;
    }

    i = 0;
    while (DisplayModes[i].depth >= 0) {
	dm = &DisplayModes[i];
        if (!got_16bit_mode)
        {
    	    write_log ("figure_pixel_formats: Attempting %dx%d: ", dm->res.width, dm->res.height);

            ddrval = DirectDraw_SetDisplayMode( dm->res.width, dm->res.height, 16, 0 ); /* 0 for default freq */
	    if (ddrval != DD_OK)
		continue;

	    ddrval = DirectDraw_GetDisplayMode();
	    if (ddrval != DD_OK)
		continue;

	    colortype = DirectDraw_GetPixelFormat();
	    if (colortype != RGBFB_NONE) 
            {
                /* Clear the 16-bit information, and get the real stuff! */
                dm->colormodes &= ~(RGBFF_R5G6B5PC|RGBFF_R5G5B5PC|RGBFF_R5G6B5|RGBFF_R5G5B5|RGBFF_B5G6R5PC|RGBFF_B5G5R5PC);
                dm->colormodes |= 1 << colortype;
                got_16bit_mode = 1;
                write_log( "Got real 16-bit colour-depth information: 0x%x\n", colortype );
            }
        }
        else if (dm->colormodes & (RGBFF_R5G6B5PC|RGBFF_R5G5B5PC|RGBFF_R5G6B5|RGBFF_R5G5B5|RGBFF_B5G6R5PC|RGBFF_B5G5R5PC) ) 
        {
            /* Clear the 16-bit information, and set the real stuff! */
            dm->colormodes &= ~(RGBFF_R5G6B5PC|RGBFF_R5G5B5PC|RGBFF_R5G6B5|RGBFF_R5G5B5|RGBFF_B5G6R5PC|RGBFF_B5G5R5PC);
            dm->colormodes |= 1 << colortype;
        }
	i++;
    }
    out:
    if (window_created)
    {
        Sleep (1000);
        DestroyWindow (hAmigaWnd);
	hAmigaWnd = NULL;
    }
    return colortype;
}

/* DirectX will fail with "Mode not supported" if we try to switch to a full
 * screen mode that doesn't match one of the dimensions we got during enumeration.
 * So try to find a best match for the given resolution in our list.  */
int WIN32GFX_AdjustScreenmode( uae_u32 *pwidth, uae_u32 *pheight, uae_u32 *ppixbits )
{
    struct PicassoResolution *best;
    uae_u32 selected_mask = (*ppixbits == 8 ? RGBMASK_8BIT
			     : *ppixbits == 15 ? RGBMASK_15BIT
			     : *ppixbits == 16 ? RGBMASK_16BIT
			     : *ppixbits == 24 ? RGBMASK_24BIT
			     : RGBMASK_32BIT);
    int pass, i = 0, index = 0;
    
    for (pass = 0; pass < 2; pass++) 
    {
	struct PicassoResolution *dm;
	uae_u32 mask = (pass == 0
			? selected_mask
			: RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT | RGBMASK_24BIT | RGBMASK_32BIT); /* %%% - BERND, were you missing 15-bit here??? */
        i = 0;
        index = 0;

	best = &DisplayModes[0];
	dm = &DisplayModes[1];

	while (dm->depth >= 0) 
        {
	    if ((dm->colormodes & mask) != 0) 
            {
		if (dm->res.width <= best->res.width && dm->res.height <= best->res.height
		    && dm->res.width >= *pwidth && dm->res.height >= *pheight)
                {
		    best = dm;
                    index = i;
                }
		if (dm->res.width >= best->res.width && dm->res.height >= best->res.height
		    && dm->res.width <= *pwidth && dm->res.height <= *pheight)
                {
		    best = dm;
                    index = i;
                }
	    }
	    dm++;
            i++;
	}
	if (best->res.width == *pwidth && best->res.height == *pheight)
        {
            selected_mask = mask; /* %%% - BERND, I added this - does it make sense?  Otherwise, I'd specify a 16-bit display-mode for my
				     Workbench (using -H 2, but SHOULD have been -H 1), and end up with an 8-bit mode instead*/
	    break;
        }
    }
    *pwidth = best->res.width;
    *pheight = best->res.height;
    if( best->colormodes & selected_mask )
	return index;

    /* Ordering here is done such that 16-bit is preferred, followed by 15-bit, 8-bit, 32-bit and 24-bit */
    if (best->colormodes & RGBMASK_16BIT)
	*ppixbits = 16;
    else if (best->colormodes & RGBMASK_15BIT) /* %%% - BERND, this possibility was missing? */
	*ppixbits = 15;
    else if (best->colormodes & RGBMASK_8BIT)
	*ppixbits = 8;
    else if (best->colormodes & RGBMASK_32BIT)
	*ppixbits = 32;
    else if (best->colormodes & RGBMASK_24BIT)
	*ppixbits = 24;
    else
        index = -1;

    return index;
}

// This function is only called for full-screen Amiga screen-modes, and simply flips
// the front and back buffers.  Additionally, because the emulation is not always drawing
// complete frames, we also need to update the back-buffer with the new contents we just
// flipped to.  Thus, after our flip, we blit.
static int DX_Flip( void )
{
    int result = 0;

    result = DirectDraw_Flip(0);
    if( result )
    {
//	result = DirectDraw_BltFast( primary_surface, 0, 0, secondary_surface, NULL );
//	result = DirectDraw_BltFast( primary_surface, 0, 0, tertiary_surface, NULL );
//	result = DirectDraw_BltFast( secondary_surface, 0, 0, primary_surface, NULL );
//	result = DirectDraw_BltFast( secondary_surface, 0, 0, tertiary_surface, NULL );
	result = DirectDraw_BltFast( tertiary_surface, 0, 0, primary_surface, NULL );
//	result = DirectDraw_BltFast( tertiary_surface, 0, 0, secondary_surface, NULL );
    }
    return result;
}

void flush_line( int lineno )
{

}

void flush_block (int a, int b)
{

}

void flush_screen (int a, int b)
{
    if (currentmode->flags & DM_OPENGL) {
#ifdef OPENGL
	OGL_render ();
#endif
    } else if (currentmode->flags & DM_D3D) {
	return;
    } else if (currentmode->flags & DM_SWSCALE) {
	S2X_render ();
	if( currentmode->flags & DM_DX_FULLSCREEN )
	    DX_Flip ();
	else if(DirectDraw_GetLockableType() != overlay_surface)
	    DX_Blit( 0, 0, 0, 0, WIN32GFX_GetWidth(), WIN32GFX_GetHeight(), BLIT_SRC );
    } else if((currentmode->flags & DM_DDRAW) && DirectDraw_GetLockableType() == secondary_surface ) {
	if( currentmode->flags & DM_DX_FULLSCREEN ) {
	    if( turbo_emulation || DX_Flip() == 0 )
	    	DX_Blit (0, a, 0, a, currentmode->current_width, b - a + 1, BLIT_SRC);
	} else if(DirectDraw_GetLockableType() != overlay_surface)
	    DX_Blit (0, a, 0, a, currentmode->current_width, b - a + 1, BLIT_SRC);
    }
}

static uae_u8 *ddraw_dolock (void)
{
    static char *surface = NULL, *oldsurface;

    if( !DirectDraw_SurfaceLock( lockable_surface ) )
    	return 0;

    surface = DirectDraw_GetSurfacePointer();
    oldsurface = gfxvidinfo.bufmem;
    gfxvidinfo.bufmem = surface;
    if (surface != oldsurface && !screen_is_picasso) 
    {
	init_row_map ();
    }

    clear_inhibit_frame (IHF_WINDOWHIDDEN);
    return surface;
}

int lockscr (void)
{
    if (!isscreen ())
	return 0;
    if (currentmode->flags & DM_D3D) {
#ifdef D3D
	return D3D_locktexture ();
#endif
    } else if (currentmode->flags & DM_SWSCALE) {
	return 1;
    } else if (currentmode->flags & DM_DDRAW) {
	return ddraw_dolock() != 0;
    }
    return 1;
}

void unlockscr (void)
{
    if (currentmode->flags & DM_D3D) {
#ifdef D3D
	D3D_unlocktexture ();
#endif
    } else if (currentmode->flags & DM_SWSCALE) {
	return;
    } else if (currentmode->flags & DM_DDRAW) {
	ddraw_unlockscr ();
    }
}

void flush_clear_screen (void)
{
    if (lockscr ()) {
	int y;
	for (y = 0; y < gfxvidinfo.height; y++) {
	    memset (gfxvidinfo.bufmem + y * gfxvidinfo.rowbytes, 0, gfxvidinfo.width * gfxvidinfo.pixbytes);
	}
	unlockscr ();
	flush_screen (0, 0);
    }
}

uae_u8 *gfx_lock_picasso (void)
{
    return ddraw_dolock ();
}

void gfx_unlock_picasso (void)
{
    DirectDraw_SurfaceUnlock();
    if( p96_double_buffer_needs_flushing )
    {
	/* Here, our flush_block() will deal with a offscreen-plain (back-buffer) to visible-surface (front-buffer) */
        if( DirectDraw_GetLockableType() == secondary_surface )
	{
	    BOOL relock = FALSE;
	    if( DirectDraw_IsLocked() )
	    {
		relock = TRUE;
		unlockscr();
	    }
	    DX_Blit( 0, p96_double_buffer_first, 
		     0, p96_double_buffer_first, 
		     currentmode->current_width, p96_double_buffer_last - p96_double_buffer_first + 1, 
		     BLIT_SRC );
	    if( relock )
	    {
		lockscr();
	    }
	}
        p96_double_buffer_needs_flushing = 0;
    }
}

static void close_hwnds( void )
{
    setmouseactive (0);
    if (hStatusWnd) {
        ShowWindow( hStatusWnd, SW_HIDE );
    	DestroyWindow (hStatusWnd);
    }
    if (hAmigaWnd) {
#ifdef OPENGL
	OGL_free ();
#endif
#ifdef D3D
	D3D_free ();
#endif
	if (currentmode->flags & DM_W_FULLSCREEN)
	    ChangeDisplaySettings (NULL, 0);
        ShowWindow (hAmigaWnd, SW_HIDE);
	DestroyWindow (hAmigaWnd);
	if (hAmigaWnd == hMainWnd)
	    hMainWnd = 0;
	hAmigaWnd = 0;
    }
    if (hMainWnd) {
        ShowWindow (hMainWnd, SW_HIDE);
	DestroyWindow (hMainWnd);
    }
    hMainWnd = 0;
    hStatusWnd = 0;
}

static int open_windows (void)
{
    char *fs_warning = 0;
    int need_fs = 0;
    int ret, i;

    in_sizemove = 0;
    updatewinfsmode (&currprefs);

    if( !DirectDraw_Start() )
	return 0;
    if( DirectDraw_GetDisplayMode() != DD_OK )
	return 0;

#ifdef PICASSO96
    if (screen_is_picasso) {
	currentmode->current_width = picasso_vidinfo.width;
	currentmode->current_height = picasso_vidinfo.height;
	currentmode->current_depth = rgbformat_bits (picasso_vidinfo.selected_rgbformat);
	currentmode->frequency = 0;
    } else {
#endif
	currentmode->current_width = currprefs.gfx_width;
	currentmode->current_height = currprefs.gfx_height;
	currentmode->current_depth = (currprefs.color_mode == 0 ? 8
			 : currprefs.color_mode == 1 ? 15
			 : currprefs.color_mode == 2 ? 16
			 : currprefs.color_mode == 3 ? 8
			 : currprefs.color_mode == 4 ? 8 : 32);
	currentmode->frequency = currprefs.gfx_refreshrate;
#ifdef PICASSO96
    }
#endif
    currentmode->amiga_width = currentmode->current_width;
    currentmode->amiga_height = currentmode->current_height;

    do {
	ret = doInit ();
    } while (ret < 0);

    setmouseactive (1);
    for (i = 0; i < NUM_LEDS; i++)
	gui_led (i, 0);
    gui_fps (0);

    return ret;
}

int check_prefs_changed_gfx (void)
{
    if (display_change_requested || 
	currprefs.gfx_width_fs != changed_prefs.gfx_width_fs ||
	currprefs.gfx_height_fs != changed_prefs.gfx_height_fs ||
	currprefs.gfx_width_win != changed_prefs.gfx_width_win ||
	currprefs.gfx_height_win != changed_prefs.gfx_height_win ||
	currprefs.color_mode != changed_prefs.color_mode ||
        currprefs.gfx_afullscreen != changed_prefs.gfx_afullscreen ||
        currprefs.gfx_pfullscreen != changed_prefs.gfx_pfullscreen ||
        currprefs.gfx_vsync != changed_prefs.gfx_vsync ||
        currprefs.gfx_refreshrate != changed_prefs.gfx_refreshrate ||
        currprefs.gfx_filter != changed_prefs.gfx_filter ||
        currprefs.gfx_filter_filtermode != changed_prefs.gfx_filter_filtermode ||
	currprefs.gfx_lores != changed_prefs.gfx_lores ||
	currprefs.gfx_linedbl != changed_prefs.gfx_linedbl)
    {
	display_change_requested = 0;
	fixup_prefs_dimensions (&changed_prefs);
	currprefs.gfx_width_win = changed_prefs.gfx_width_win;
	currprefs.gfx_height_win = changed_prefs.gfx_height_win;
	currprefs.gfx_width_fs = changed_prefs.gfx_width_fs;
	currprefs.gfx_height_fs = changed_prefs.gfx_height_fs;
	currprefs.color_mode = changed_prefs.color_mode;
        currprefs.gfx_afullscreen = changed_prefs.gfx_afullscreen;
        currprefs.gfx_pfullscreen = changed_prefs.gfx_pfullscreen;
        updatewinfsmode (&currprefs);
        currprefs.gfx_vsync = changed_prefs.gfx_vsync;
        currprefs.gfx_refreshrate = changed_prefs.gfx_refreshrate;
	currprefs.gfx_filter = changed_prefs.gfx_filter;
	currprefs.gfx_filter_filtermode = changed_prefs.gfx_filter_filtermode;
	currprefs.gfx_lores = changed_prefs.gfx_lores;
	currprefs.gfx_linedbl = changed_prefs.gfx_linedbl;
        inputdevice_unacquire ();
	close_windows ();
	graphics_init ();
#ifdef PICASSO96
	DX_SetPalette (0, 256);
#endif
	init_hz ();
	pause_sound ();
	resume_sound ();
	inputdevice_acquire (mouseactive);
	return 1;
    }
    if (currprefs.gfx_correct_aspect != changed_prefs.gfx_correct_aspect ||
	currprefs.gfx_xcenter != changed_prefs.gfx_xcenter ||
	currprefs.gfx_ycenter != changed_prefs.gfx_ycenter)
    {
	currprefs.gfx_correct_aspect = changed_prefs.gfx_correct_aspect;
	currprefs.gfx_xcenter = changed_prefs.gfx_xcenter;
	currprefs.gfx_ycenter = changed_prefs.gfx_ycenter;
	return 1;
    }


    if (currprefs.leds_on_screen != changed_prefs.leds_on_screen ||
        currprefs.keyboard_leds[0] != changed_prefs.keyboard_leds[0] ||
        currprefs.keyboard_leds[1] != changed_prefs.keyboard_leds[1] ||
        currprefs.keyboard_leds[2] != changed_prefs.keyboard_leds[2] ||
        currprefs.win32_middle_mouse != changed_prefs.win32_middle_mouse ||
	currprefs.win32_activepriority != changed_prefs.win32_activepriority ||
	currprefs.win32_iconified_nosound != changed_prefs.win32_iconified_nosound ||
	currprefs.win32_iconified_nospeed != changed_prefs.win32_iconified_nospeed ||
	currprefs.win32_iconified_pause != changed_prefs.win32_iconified_pause ||
	currprefs.win32_ctrl_F11_is_quit != changed_prefs.win32_ctrl_F11_is_quit)
    {
        currprefs.leds_on_screen = changed_prefs.leds_on_screen;
        currprefs.keyboard_leds[0] = changed_prefs.keyboard_leds[0];
        currprefs.keyboard_leds[1] = changed_prefs.keyboard_leds[1];
        currprefs.keyboard_leds[2] = changed_prefs.keyboard_leds[2];
        currprefs.win32_middle_mouse = changed_prefs.win32_middle_mouse;
	currprefs.win32_activepriority = changed_prefs.win32_activepriority;
	currprefs.win32_iconified_nosound = changed_prefs.win32_iconified_nosound;
	currprefs.win32_iconified_nospeed = changed_prefs.win32_iconified_nospeed;
	currprefs.win32_iconified_pause = changed_prefs.win32_iconified_pause;
	currprefs.win32_ctrl_F11_is_quit = changed_prefs.win32_ctrl_F11_is_quit;
        inputdevice_unacquire ();
        currprefs.keyboard_leds_in_use = currprefs.keyboard_leds[0] | currprefs.keyboard_leds[1] | currprefs.keyboard_leds[2];
	pause_sound ();
	resume_sound ();
	inputdevice_acquire (mouseactive);
#ifndef _DEBUG
        SetThreadPriority ( GetCurrentThread(), priorities[currprefs.win32_activepriority].value);
	write_log ("priority set to %d\n", priorities[currprefs.win32_activepriority].value);
#endif
	return 1;
    }


    if (strcmp (currprefs.prtname, changed_prefs.prtname)) {
	strcpy (currprefs.prtname, changed_prefs.prtname);
#ifdef PARALLEL_PORT
	closeprinter ();
#endif
    }
    if (strcmp (currprefs.sername, changed_prefs.sername) || 
	currprefs.serial_hwctsrts != changed_prefs.serial_hwctsrts ||
	currprefs.serial_direct != changed_prefs.serial_direct ||
	currprefs.serial_demand != changed_prefs.serial_demand) {
	strcpy (currprefs.sername, changed_prefs.sername);
	currprefs.serial_hwctsrts = changed_prefs.serial_hwctsrts;
	currprefs.serial_demand = changed_prefs.serial_demand;
	currprefs.serial_direct = changed_prefs.serial_direct;
#ifdef SERIAL_PORT
	serial_exit ();
	serial_init ();
#endif
    }
    if (currprefs.win32_midiindev != changed_prefs.win32_midiindev ||
        currprefs.win32_midioutdev != changed_prefs.win32_midioutdev)
    {
	currprefs.win32_midiindev = changed_prefs.win32_midiindev;
	currprefs.win32_midioutdev = changed_prefs.win32_midioutdev;
#ifdef SERIAL_PORT
	if (midi_ready) {
	    Midi_Close ();
	    Midi_Open ();
	}
#endif
    }

    if (currprefs.win32_automount_drives != changed_prefs.win32_automount_drives) {
	currprefs.win32_automount_drives = changed_prefs.win32_automount_drives;
    }
    return 0;
}

/* Color management */

static xcolnr xcol8[4096];
static PALETTEENTRY colors256[256];
static int ncols256 = 0;

static int red_bits, green_bits, blue_bits, alpha_bits;
static int red_shift, green_shift, blue_shift, alpha_shift;
static int alpha;

static int get_color (int r, int g, int b, xcolnr * cnp)
{
    if (ncols256 == 256)
	return 0;
    colors256[ncols256].peRed = r * 0x11;
    colors256[ncols256].peGreen = g * 0x11;
    colors256[ncols256].peBlue = b * 0x11;
    colors256[ncols256].peFlags = 0;
    *cnp = ncols256;
    ncols256++;
    return 1;
}

void init_colors (void)
{
    int i;
    HRESULT ddrval;

    if (ncols256 == 0) {
	alloc_colors256 (get_color);
	memcpy (xcol8, xcolors, sizeof xcol8);
    }

    /* init colors */
    if (currentmode->flags & DM_OPENGL) {
#ifdef OPENGL
	OGL_getpixelformat (currentmode->current_depth,&red_bits,&green_bits,&blue_bits,&red_shift,&green_shift,&blue_shift,&alpha_bits,&alpha_shift,&alpha);
#endif
    } else if (currentmode->flags & DM_D3D) {
#ifdef D3D
	D3D_getpixelformat (currentmode->current_depth,&red_bits,&green_bits,&blue_bits,&red_shift,&green_shift,&blue_shift,&alpha_bits,&alpha_shift,&alpha);
#endif
    } else {
	switch( currentmode->current_depth >> 3)
	{
	    case 1:
		memcpy (xcolors, xcol8, sizeof xcolors);
		ddrval = DirectDraw_SetPaletteEntries( 0, 256, colors256 );
		if (ddrval != DD_OK)
		    write_log ("DX_SetPalette() failed with %s/%d\n", DXError (ddrval), ddrval);
	    break;

	    case 2:
	    case 3:
	    case 4:
		red_bits = bits_in_mask( DirectDraw_GetPixelFormatBitMask( red_mask ) );
		green_bits = bits_in_mask( DirectDraw_GetPixelFormatBitMask( green_mask ) );
		blue_bits = bits_in_mask( DirectDraw_GetPixelFormatBitMask( blue_mask ) );
		red_shift = mask_shift( DirectDraw_GetPixelFormatBitMask( red_mask ) );
		green_shift = mask_shift( DirectDraw_GetPixelFormatBitMask( green_mask ) );
		blue_shift = mask_shift( DirectDraw_GetPixelFormatBitMask( blue_mask ) );
		alpha_bits = 0;
		alpha_shift = 0;
	    break;
	}
    }
    if (currentmode->current_depth > 8) {
	alloc_colors64k (red_bits, green_bits, blue_bits, red_shift,green_shift, blue_shift, alpha_bits, alpha_shift, alpha);
	S2X_configure (red_bits, green_bits, blue_bits, red_shift,green_shift, blue_shift);
    }
    
    switch (gfxvidinfo.pixbytes) 
    {
     case 2:
	for (i = 0; i < 4096; i++)
	    xcolors[i] = xcolors[i] * 0x00010001;
	gfxvidinfo.can_double = 1;
	break;
     case 1:
	for (i = 0; i < 4096; i++)
	    xcolors[i] = xcolors[i] * 0x01010101;
	gfxvidinfo.can_double = 1;
	break;
     default:
	gfxvidinfo.can_double = 0;
	break;
    }
}

#ifdef PICASSO96
void DX_SetPalette_vsync (void)
{
}

void DX_SetPalette (int start, int count)
{
    HRESULT ddrval;

    if (!screen_is_picasso)
        return;

    if( picasso96_state.RGBFormat != RGBFB_CHUNKY )
    {
        /* notice_screen_contents_lost(); */
        return;
    }

    if (picasso_vidinfo.pixbytes != 1) 
    {
	/* write_log ("DX Setpalette emulation\n"); */
	/* This is the case when we're emulating a 256 color display.  */
	while (count-- > 0) 
        {
	    int r = picasso96_state.CLUT[start].Red;
	    int g = picasso96_state.CLUT[start].Green;
	    int b = picasso96_state.CLUT[start].Blue;
	    picasso_vidinfo.clut[start++] = (doMask256 (r, red_bits, red_shift)
		| doMask256 (g, green_bits, green_shift)
		| doMask256 (b, blue_bits, blue_shift));
	}
	notice_screen_contents_lost();
	return;
    }

    /* Set our DirectX palette here */
    if( currentmode->current_depth == 8 )
    {
	    ddrval = DirectDraw_SetPaletteEntries( start, count, (LPPALETTEENTRY)&(picasso96_state.CLUT[start] ) );
	    if (ddrval != DD_OK)
	        gui_message("DX_SetPalette() failed with %s/%d\n", DXError (ddrval), ddrval);
    }
    else
    {
	    write_log ("ERROR - DX_SetPalette() pixbytes %d\n", currentmode->current_depth >> 3 );
    }
}

void DX_Invalidate (int first, int last)
{
    p96_double_buffer_first = first;
    if(last >= picasso_vidinfo.height )
	last = picasso_vidinfo.height - 1;
    p96_double_buffer_last  = last;
    p96_double_buffer_needs_flushing = 1;
}

#endif

int DX_BitsPerCannon (void)
{
    return 8;
}

static COLORREF BuildColorRef( int color, RGBFTYPE pixelformat )
{
    COLORREF result;

    /* Do special case first */
    if( pixelformat == RGBFB_CHUNKY )
        result = color;
    else
        result = do_get_mem_long( &color );
    return result;
#if 0
    int r,g,b;
    write_log( "DX_Blit() called to fill with color of 0x%x, rgbtype of 0x%x\n", color, pixelformat );

    switch( pixelformat )
    {
        case RGBFB_R5G6B5PC:
            r = color & 0xF800 >> 11;
            g = color & 0x07E0 >> 5;
            b = color & 0x001F;
        break;
        case RGBFB_R5G5B5PC:
            r = color & 0x7C00 >> 10;
            g = color & 0x03E0 >> 5;
            b = color & 0x001F;
        break;
        case RGBFB_B5G6R5PC:
            r = color & 0x001F;
            g = color & 0x07E0 >> 5;
            b = color & 0xF800 >> 11;
        break;
        case RGBFB_B5G5R5PC:
            r = color & 0x001F;
            g = color & 0x03E0 >> 5;
            b = color & 0x7C00 >> 10;
        break;
        case RGBFB_B8G8R8:
            r = color & 0x00FF0000 >> 16;
            g = color & 0x0000FF00 >> 8;
            b = color & 0x000000FF;
        break;
        case RGBFB_A8B8G8R8:
            r = color & 0xFF000000 >> 24;
            g = color & 0x00FF0000 >> 16;
            b = color & 0x0000FF00 >> 8;
        break;
        case RGBFB_R8G8B8:
            r = color & 0x000000FF;
            g = color & 0x0000FF00 >> 8;
            b = color & 0x00FF0000 >> 16;
        break;
        case RGBFB_A8R8G8B8:
            r = color & 0x0000FF00 >> 8;
            g = color & 0x00FF0000 >> 16;
            b = color & 0xFF000000 >> 24;
        break;
        default:
            write_log( "Uknown 0x%x pixel-format\n", pixelformat );
        break;
    }
    result = RGB(r,g,b);
    write_log( "R = 0x%02x, G = 0x%02x, B = 0x%02x - result = 0x%08x\n", r, g, b, result );
    return result;
#endif
}

/* This is a general purpose DirectDrawSurface filling routine.  It can fill within primary surface.
 * Definitions:
 * - primary is the displayed (visible) surface in VRAM, which may have an associated offscreen surface (or back-buffer)
 */
int DX_Fill( int dstx, int dsty, int width, int height, uae_u32 color, RGBFTYPE rgbtype )
{
    int result = 0;
    RECT dstrect;
    RECT srcrect;
    DDBLTFX ddbltfx;
    memset( &ddbltfx, 0, sizeof( ddbltfx ) );
    ddbltfx.dwFillColor = BuildColorRef( color, rgbtype );
    ddbltfx.dwSize = sizeof( ddbltfx );

    /* Set up our source rectangle.  This NEVER needs to be adjusted for windowed display, since the
     * source is ALWAYS in an offscreen buffer, or we're in full-screen mode. */
    SetRect( &srcrect, dstx, dsty, dstx+width, dsty+height );

    /* Set up our destination rectangle, and adjust for blit to windowed display (if necessary ) */
    SetRect( &dstrect, dstx, dsty, dstx+width, dsty+height );
    if( !(currentmode->flags & (DM_DX_FULLSCREEN | DM_OVERLAY)))
	OffsetRect( &dstrect, amigawin_rect.left, amigawin_rect.top );

    /* Render our fill to the visible (primary) surface */
    if( ( result = DirectDraw_Blt( primary_surface, &dstrect, invalid_surface, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx ) ) )
    {
	if( DirectDraw_GetLockableType() == secondary_surface )
	{
	    /* We've colour-filled the visible, but still need to colour-fill the offscreen */
	    result = DirectDraw_Blt( secondary_surface, &srcrect, invalid_surface, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx );
	}
    }
    return result;
}

/* This is a general purpose DirectDrawSurface blitting routine.  It can blit within primary surface
 * Definitions:
 * - primary is the displayed (visible) surface in VRAM, which may have an associated offscreen surface (or back-buffer)
 */

static DDBLTFX fx = { sizeof( DDBLTFX ) };

static DWORD BLIT_OPCODE_TRANSLATION[ BLIT_LAST ] =
{
    BLACKNESS,  /* BLIT_FALSE */
    NOTSRCERASE,/* BLIT_NOR */
    -1,         /* BLIT_ONLYDST NOT SUPPORTED */
    NOTSRCCOPY, /* BLIT_NOTSRC */
    SRCERASE,   /* BLIT_ONLYSRC */
    DSTINVERT,  /* BLIT_NOTDST */
    SRCINVERT,  /* BLIT_EOR */
    -1,         /* BLIT_NAND NOT SUPPORTED */
    SRCAND,     /* BLIT_AND */
    -1,         /* BLIT_NEOR NOT SUPPORTED */
    -1,         /* NO-OP */
    MERGEPAINT, /* BLIT_NOTONLYSRC */
    SRCCOPY,    /* BLIT_SRC */
    -1,         /* BLIT_NOTONLYDST NOT SUPPORTED */
    SRCPAINT,   /* BLIT_OR */
    WHITENESS   /* BLIT_TRUE */
};

int DX_Blit( int srcx, int srcy, int dstx, int dsty, int width, int height, BLIT_OPCODE opcode )
{
    int result = 0;
    RECT dstrect;
    RECT srcrect;
    DWORD dwROP = BLIT_OPCODE_TRANSLATION[ opcode ];

    /* Set up our source rectangle.  This NEVER needs to be adjusted for windowed display, since the
     * source is ALWAYS in an offscreen buffer, or we're in full-screen mode. */
    SetRect( &srcrect, srcx, srcy, srcx+width, srcy+height );

    /* Set up our destination rectangle, and adjust for blit to windowed display (if necessary ) */
    SetRect( &dstrect, dstx, dsty, dstx+width, dsty+height );
    
    if( !(currentmode->flags & (DM_DX_FULLSCREEN | DM_OVERLAY)))
        OffsetRect( &dstrect, amigawin_rect.left, amigawin_rect.top );

    if( dwROP == -1 )
    {
	/* Unsupported blit opcode! */
	return 0;
    }
    else
    {
	fx.dwROP = dwROP;
    }

    /* Render our blit within the primary surface */
    result = DirectDraw_Blt( primary_surface, &dstrect, DirectDraw_GetLockableType(), &srcrect, DDBLT_WAIT | DDBLT_ROP, &fx );

    if( !result )
    {
	BLIT_OPCODE_TRANSLATION[ opcode ] = -1;
    }
    else if( DirectDraw_GetLockableType() == secondary_surface )
    {
	/* We've just blitted from the offscreen to the visible, but still need to blit from offscreen to offscreen
	 * NOTE: reset our destination rectangle again if its been modified above... */
	if( ( srcx != dstx ) || ( srcy != dsty ) )
	{
	    if(!(currentmode->flags & DM_DX_FULLSCREEN))
	        SetRect( &dstrect, dstx, dsty, dstx+width, dsty+height );
            result = DirectDraw_Blt( secondary_surface, &dstrect, secondary_surface, &srcrect, DDBLT_WAIT | DDBLT_ROP, &fx );
	}
    }

    return result;
}

void DX_WaitVerticalSync( void )
{
    DirectDraw_WaitForVerticalBlank (DDWAITVB_BLOCKBEGIN);
}

uae_u32 DX_ShowCursor( uae_u32 activate )
{
    uae_u32 result = 0;
    if( ShowCursor( activate ) > 0 )
	result = 1;
    return result;
}

uae_u32 DX_MoveCursor( uae_u32 x, uae_u32 y )
{
    uae_u32 result = 0;

    // We may need to adjust the x,y values for our window-offset
    if(!(currentmode->flags & DM_DX_FULLSCREEN))
    {
	RECT rect;
	if( GetWindowRect( hAmigaWnd, &rect ) )
	{
	    x = rect.left + x;
	    y = rect.top + y;
	}
    }
    if( SetCursorPos( x, y ) )
	result = 1;
    return result;
}

static void open_screen( void )
{
    close_windows ();
    open_windows();
#ifdef PICASSO96
    DX_SetPalette (0, 256);
#endif
}

#ifdef PICASSO96
void gfx_set_picasso_state( int on )
{
    if (screen_is_picasso == on)
	return;
    screen_is_picasso = on;
    open_screen();
}

void gfx_set_picasso_modeinfo( uae_u32 w, uae_u32 h, uae_u32 depth, RGBFTYPE rgbfmt )
{
    depth >>= 3;
    if( ((unsigned)picasso_vidinfo.width == w ) &&
	    ( (unsigned)picasso_vidinfo.height == h ) &&
	    ( (unsigned)picasso_vidinfo.depth == depth ) &&
	    ( picasso_vidinfo.selected_rgbformat == rgbfmt) )
	return;

    picasso_vidinfo.selected_rgbformat = rgbfmt;
    picasso_vidinfo.width = w;
    picasso_vidinfo.height = h;
    picasso_vidinfo.depth = depth;
    picasso_vidinfo.extra_mem = 1;

    if( screen_is_picasso ) 
    {
	open_screen();
    }
}
#endif

static void gfxmode_reset (void)
{
    currentmode->amode[0] = &wmodes[currprefs.win32_no_overlay ? SM_WINDOW : SM_WINDOW_OVERLAY];
    currentmode->amode[1] = &wmodes[SM_FULLSCREEN_DX];
    currentmode->pmode[0] = &wmodes[currprefs.win32_no_overlay ? SM_WINDOW : SM_WINDOW_OVERLAY];
    currentmode->pmode[1] = &wmodes[SM_FULLSCREEN_DX];
#ifdef OPENGL
    if (currprefs.gfx_filter == UAE_FILTER_OPENGL) {
	currentmode->amode[0] = &wmodes[SM_OPENGL_WINDOW];
	currentmode->amode[1] = &wmodes[SM_OPENGL_FULLSCREEN_W];
    }
#endif
#ifdef D3D
    if (currprefs.gfx_filter == UAE_FILTER_DIRECT3D) {
	currentmode->amode[0] = &wmodes[SM_D3D_WINDOW];
	currentmode->amode[1] = &wmodes[SM_D3D_FULLSCREEN_DX];
    }
#endif
}

void machdep_init (void)
{
    picasso_requested_on = 0;
    picasso_on = 0;
    screen_is_picasso = 0;
    memset (currentmode, 0, sizeof (*currentmode));
}

int graphics_init (void)
{
    gfxmode_reset ();
    return open_windows ();
}

int graphics_setup (void)
{
    if( !DirectDraw_Start() )
	return 0;
    DirectDraw_Release();
#ifdef PICASSO96
    InitPicasso96();
#endif
    return 1;
}

void graphics_leave (void)
{
    close_windows ();
    dumpcustom ();
}

uae_u32 OSDEP_minimize_uae( void )
{
    return ShowWindow (hAmigaWnd, SW_MINIMIZE);
}

void close_windows (void)
{
#ifdef AVIOUTPUT
    AVIOutput_End ();
#endif
    free (gfxvidinfo.realbufmem);
    gfxvidinfo.realbufmem = 0;
    DirectDraw_Release();
    close_hwnds();
}

void WIN32GFX_ToggleFullScreen( void )
{
    display_change_requested = 1;
    if (screen_is_picasso)
	currprefs.gfx_pfullscreen ^= 1;
    else
	currprefs.gfx_afullscreen ^= 1;
}

static int create_windows (void)
{
    int fs = currentmode->flags & (DM_W_FULLSCREEN | DM_DX_FULLSCREEN | DM_D3D_FULLSCREEN);
    if (!fs) 
    {
        RECT rc;
        LONG stored_x = 1, stored_y = GetSystemMetrics( SM_CYMENU ) + GetSystemMetrics( SM_CYBORDER );
        DWORD regkeytype;
        DWORD regkeysize = sizeof(LONG);
        HLOCAL hloc;
	LPINT lpParts;
	int cx = GetSystemMetrics(SM_CXBORDER), cy = GetSystemMetrics(SM_CYBORDER);
	int oldx, oldy;

        RegQueryValueEx( hWinUAEKey, "xPos", 0, &regkeytype, (LPBYTE)&stored_x, &regkeysize );
        RegQueryValueEx( hWinUAEKey, "yPos", 0, &regkeytype, (LPBYTE)&stored_y, &regkeysize );

	if( stored_x < 1 )
            stored_x = 1;
        if( stored_y < GetSystemMetrics( SM_CYMENU ) + cy)
            stored_y = GetSystemMetrics( SM_CYMENU ) + cy;

        if( stored_x > GetSystemMetrics( SM_CXFULLSCREEN ) )
            rc.left = 1;
        else
            rc.left = stored_x;
        
        if( stored_y > GetSystemMetrics( SM_CYFULLSCREEN ) )
            rc.top = 1;
        else
            rc.top = stored_y;

        rc.right = rc.left + 2 + currentmode->current_width + 2;
        rc.bottom = rc.top + 2 + currentmode->current_height + 2 + GetSystemMetrics (SM_CYMENU);

	oldx = rc.left;
	oldy = rc.top;
	AdjustWindowRect (&rc, NORMAL_WINDOW_STYLE, FALSE);
	win_x_diff = rc.left - oldx;
	win_y_diff = rc.top - oldy;
        hMainWnd = CreateWindowEx( picasso_on ? WS_EX_ACCEPTFILES : WS_EX_ACCEPTFILES | WS_EX_APPWINDOW, "PCsuxRox", "WinUAE",
				       NORMAL_WINDOW_STYLE, rc.left, rc.top,
				       rc.right - rc.left + 1, rc.bottom - rc.top + 1,
				       NULL, NULL, 0, NULL);

	if (! hMainWnd)
	    return 0;
	hStatusWnd = CreateStatusWindow (WS_CHILD | WS_VISIBLE, "", hMainWnd, 1);
	if (hStatusWnd) 
        {
	    GetClientRect (hMainWnd, &rc);
	    /* Allocate an array for holding the right edge coordinates. */
	    hloc = LocalAlloc (LHND, sizeof (int) * LED_NUM_PARTS);
	    if (hloc) 
            {
		lpParts = LocalLock (hloc);

		/* Calculate the right edge coordinate for each part, and copy the coords
		 * to the array.  */
		lpParts[0] = rc.right - (LED_DRIVE_WIDTH * 4) - LED_POWER_WIDTH - LED_FPS_WIDTH - LED_CD_WIDTH - LED_HD_WIDTH - 2;
                lpParts[1] = lpParts[0] + LED_FPS_WIDTH;
		lpParts[2] = lpParts[1] + LED_POWER_WIDTH;
		lpParts[3] = lpParts[2] + LED_CD_WIDTH;
		lpParts[4] = lpParts[3] + LED_HD_WIDTH;
		lpParts[5] = lpParts[4] + LED_DRIVE_WIDTH;
		lpParts[6] = lpParts[5] + LED_DRIVE_WIDTH;
		lpParts[7] = lpParts[6] + LED_DRIVE_WIDTH;
		lpParts[8] = lpParts[7] + LED_DRIVE_WIDTH;

		/* Create the parts */
		SendMessage (hStatusWnd, SB_SETPARTS, (WPARAM) LED_NUM_PARTS, (LPARAM) lpParts);

		LocalUnlock (hloc);
		LocalFree (hloc);
	    }
	}
    }
    else
	hMainWnd = NULL;

    hAmigaWnd = CreateWindowEx (fs ? WS_EX_ACCEPTFILES | WS_EX_TOPMOST : WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,
				"AmigaPowah", "WinUAE",
				hMainWnd ? WS_VISIBLE | WS_CHILD : WS_VISIBLE | WS_POPUP,
				hMainWnd ? 2 : CW_USEDEFAULT, hMainWnd ? 2 : CW_USEDEFAULT,
				currentmode->current_width, currentmode->current_height,
				hMainWnd, NULL, 0, NULL);
    
    if (! hAmigaWnd) 
    {
        close_hwnds();
	return 0;
    }


    if (hMainWnd != hAmigaWnd) {
	ShowWindow (hMainWnd, SW_SHOWNORMAL);
        UpdateWindow( hMainWnd );
    }
    if (hAmigaWnd) {
    	UpdateWindow (hAmigaWnd);
	ShowWindow (hAmigaWnd, SW_SHOWNORMAL);
    }

    return 1;
}

static void setoverlay(void)
{
    RECT sr, dr, statusr;
    POINT p = {0,0};
    int maxwidth, maxheight, w, h;

    GetClientRect(hMainWnd, &dr);
    // adjust the dest-rect to avoid the status-bar
    if( hStatusWnd )
    {
	if( GetWindowRect( hStatusWnd, &statusr ) )
	    dr.bottom = dr.bottom - ( statusr.bottom - statusr.top );
    }

    ClientToScreen( hMainWnd, &p );
    dr.left = p.x + 2;
    dr.top = p.y + 2;
    dr.right += p.x;
    dr.bottom += p.y;

    w = currentmode->current_width * (currprefs.gfx_filter_horiz_zoom + 100) / 100;
    h = currentmode->current_height * (currprefs.gfx_filter_vert_zoom + 100) / 100;

    sr.left = 0;
    sr.top = 0;
    sr.right = currentmode->current_width;
    sr.bottom = currentmode->current_height;

    // Adjust our dst-rect to match the dimensions of our src-rect
    if (dr.right - dr.left > sr.right - sr.left)
	dr.right = dr.left + sr.right - sr.left;
    if (dr.bottom - dr.top > sr.bottom - sr.top)
	dr.bottom = dr.top + sr.bottom - sr.top;

    sr.left = 0;
    sr.top = 0;
    sr.right = w;
    sr.bottom = h;

    maxwidth = GetSystemMetrics(SM_CXSCREEN);
    if (dr.right > maxwidth) {
	sr.right = w - (dr.right - maxwidth);
	dr.right = maxwidth;
    }
    maxheight = GetSystemMetrics(SM_CYSCREEN);
    if (dr.bottom > maxheight) {
	sr.bottom = h - (dr.bottom - maxheight);
	dr.bottom = maxheight;
    }
    if (dr.left < 0) {
	sr.left = -dr.left;
	dr.left = 0;
    }
    if (dr.top < 0) {
	sr.top = -dr.top;
	dr.top = 0;
    }
    DirectDraw_UpdateOverlay(sr, dr);
}

static void updatemodes (void)
{
    if (screen_is_picasso) {
    	currentmode->mode = currentmode->pmode[currprefs.gfx_pfullscreen];
	currentmode->flags = currentmode->mode->pflags;
    } else {
	currentmode->mode = currentmode->amode[currprefs.gfx_afullscreen];
	currentmode->flags = currentmode->mode->aflags;
    }
    currentmode->modeindex = currentmode->mode - &wmodes[0];

    currentmode->flags &= ~DM_SWSCALE;
    if (currprefs.gfx_filter == UAE_FILTER_NULL || currprefs.gfx_filter == UAE_FILTER_SCALE2X
        || currprefs.gfx_filter == UAE_FILTER_SUPEREAGLE || currprefs.gfx_filter == UAE_FILTER_SUPER2XSAI
        || currprefs.gfx_filter == UAE_FILTER_2XSAI) {
	    currentmode->flags |= DM_SWSCALE;
	    if (currentmode->current_depth < 15)
		currentmode->current_depth = 16;
    }
}

static BOOL doInit (void)
{
    char *fs_warning = 0;
    char tmpstr[300];
    RGBFTYPE colortype;
    int need_fs = 0;
    int tmp_depth;
    int ret = 0;

    colortype = DirectDraw_GetPixelFormat();

    for (;;) {
	updatemodes ();
	currentmode->real_depth = 0;
	tmp_depth = currentmode->current_depth;

	if (currentmode->current_depth < 15 && (currprefs.chipset_mask & CSMASK_AGA) && isfullscreen ()) {
	    static int warned;
	    if (!warned) {
		currentmode->current_depth = 16;
		gui_message("AGA emulation requires 16 bit or higher display depth\nSwitching from 8-bit to 16-bit");
	    }
	    warned = 1;
	}

	if (!(currentmode->flags & DM_OVERLAY) && !isfullscreen() && !(currentmode->flags & (DM_OPENGL | DM_D3D))) {
	    write_log ("using desktop depth (%d -> %d) because not using overlay or opengl mode\n", currentmode->current_depth,DirectDraw_GetSurfaceBitCount());
	    currentmode->current_depth = DirectDraw_GetSurfaceBitCount();
	    updatemodes ();
	}

	//If screen depth is equal to the desired window_depth then no overlay is needed.
	if (!(currentmode->flags & (DM_OPENGL | DM_D3D)) && DirectDraw_GetSurfaceBitCount() == (unsigned)currentmode->current_depth) {
	    write_log ("ignored overlay because desktop depth == requested depth (%d)\n", currentmode->current_depth);
	    modefallback (DM_OVERLAY);
	    updatemodes ();
	}
    
	if (colortype == RGBFB_NONE && !(currentmode->flags & DM_OVERLAY)) {
	    need_fs = 1;
	    fs_warning = "the desktop is running in an unknown color mode.";
	} else if (colortype == RGBFB_CLUT && !(currentmode->flags & DM_OVERLAY)) {
	    need_fs = 1;
	    fs_warning = "the desktop is running in 8 bit color depth, which UAE can't use in windowed mode.";
	} else if (currentmode->current_width >= DirectDraw_CurrentWidth() || currentmode->current_height >= DirectDraw_CurrentHeight()) {
	    if (!console_logging) {
		need_fs = 1;
	        fs_warning = "the desktop is too small for the specified window size.";
	    }
#ifdef PICASSO96
	} else if (screen_is_picasso && !currprefs.gfx_pfullscreen &&
		  ( picasso_vidinfo.selected_rgbformat != RGBFB_CHUNKY ) &&
		  ( picasso_vidinfo.selected_rgbformat != colortype ) &&
		    !(currentmode->flags & DM_OVERLAY) )
	{
	    need_fs = 1;
	    fs_warning = "you selected a Picasso96 display with a color depth different from that of the desktop and an overlay was unavailable.";
#endif
	}
	if (need_fs && !isfullscreen ()) {
	    // Temporarily drop the DirectDraw stuff
	    DirectDraw_Release();
	    sprintf (tmpstr, "The selected screen mode can't be displayed in a window, because %s\n"
		     "Switching to full-screen display.", fs_warning);
	    gui_message (tmpstr);
	    DirectDraw_Start();
  	    if (screen_is_picasso)
		changed_prefs.gfx_pfullscreen = currprefs.gfx_pfullscreen = 1;
	    else
		changed_prefs.gfx_afullscreen = currprefs.gfx_afullscreen = 1;
	    updatewinfsmode (&currprefs);
	    updatewinfsmode (&changed_prefs);
	    currentmode->current_depth = tmp_depth;
	    updatemodes ();
	}
	if (! create_windows ())
    	    goto oops;
#ifdef PICASSO96
	if (screen_is_picasso) {
	    if (need_fs)
		currprefs.gfx_pfullscreen = 1;
	    currentmode->pal = (LPPALETTEENTRY) & picasso96_state.CLUT;
	    if (! set_ddraw ()) {
		if (!modefallback (0))
		    goto oops;
		close_windows ();
		if (!DirectDraw_Start ()) break;
		continue;
	    }
	    picasso_vidinfo.rowbytes = DirectDraw_GetSurfacePitch();
	    picasso_vidinfo.pixbytes = DirectDraw_GetBytesPerPixel();
	    picasso_vidinfo.rgbformat = DirectDraw_GetPixelFormat();
	    break;
	} else {
#endif
	    if (need_fs) {
		currprefs.gfx_afullscreen = 1;
	        updatewinfsmode (&currprefs);
	    }
	    currentmode->pal = colors256;
	    if (! set_ddraw ()) {
		if (!modefallback (0))
		    goto oops;
		close_windows ();
		if (!DirectDraw_Start ()) break;
		continue;
	    }
	    currentmode->real_depth = currentmode->current_depth;
	    if (currentmode->flags & (DM_OPENGL | DM_D3D | DM_SWSCALE)) {
		currentmode->amiga_width = AMIGA_WIDTH_MAX >> (currprefs.gfx_lores ? 1 : 0);
		currentmode->amiga_height = AMIGA_HEIGHT_MAX >> (currprefs.gfx_linedbl ? 0 : 1);
		if (!(currentmode->flags & DM_SWSCALE))
		    currentmode->current_depth = (currprefs.gfx_filter_filtermode / 2) ? 32 : 16;
	        currentmode->pitch = currentmode->amiga_width * currentmode->current_depth >> 3;
	    } else {
	        currentmode->amiga_width = currentmode->current_width;
	        currentmode->amiga_height = currentmode->current_height;
	    }
	    gfxvidinfo.pixbytes = currentmode->current_depth >> 3;
	    gfxvidinfo.bufmem = 0;
	    gfxvidinfo.linemem = 0;
	    gfxvidinfo.emergmem = scrlinebuf; // memcpy from system-memory to video-memory
	    gfxvidinfo.width = currentmode->amiga_width;
	    gfxvidinfo.height = currentmode->amiga_height;
	    gfxvidinfo.maxblocklines = 0; // flush_screen actually does everything
	    gfxvidinfo.rowbytes = currentmode->pitch;
	    break;
#ifdef PICASSO96
	}
#endif
    }

    if ((currentmode->flags & DM_DDRAW) && !(currentmode->flags & (DM_D3D | DM_SWSCALE))) {
	int flags;
	if( !DirectDraw_SurfaceLock( lockable_surface ) )
	    goto oops;
	flags = DirectDraw_GetPixelFormatFlags();
	DirectDraw_SurfaceUnlock();
        if (flags  & (DDPF_RGB | DDPF_PALETTEINDEXED8 | DDPF_RGBTOYUV )) {
	    write_log( "%s mode (bits: %d, pixbytes: %d)\n", currentmode->flags & DM_DX_FULLSCREEN ? "Full screen" : "Window",
		   DirectDraw_GetSurfaceBitCount(), currentmode->current_depth >> 3 );
	} else {
	    char szMessage[ MAX_PATH ];
	    WIN32GUI_LoadUIString( IDS_UNSUPPORTEDPIXELFORMAT, szMessage, MAX_PATH );
	    gui_message( szMessage);
	    write_log ("PFF was %x\n", flags);
	    goto oops;
	}
    } else if (!(currentmode->flags & DM_SWSCALE)) {
	int size = currentmode->amiga_width * currentmode->amiga_height * gfxvidinfo.pixbytes;
	gfxvidinfo.realbufmem = malloc (size);
	gfxvidinfo.bufmem = gfxvidinfo.realbufmem;
	gfxvidinfo.rowbytes = currentmode->amiga_width * gfxvidinfo.pixbytes;
    } else if (!(currentmode->flags & DM_D3D)) {
	int size = (currentmode->amiga_width * 2) * (currentmode->amiga_height * 3) * gfxvidinfo.pixbytes;
	gfxvidinfo.realbufmem = malloc (size);
	memset (gfxvidinfo.realbufmem, 0, size);
	gfxvidinfo.bufmem = gfxvidinfo.realbufmem + (currentmode->amiga_width + (currentmode->amiga_width * 2) * currentmode->amiga_height) * gfxvidinfo.pixbytes;
	gfxvidinfo.rowbytes = currentmode->amiga_width * 2 * gfxvidinfo.pixbytes;
    }

    init_row_map ();
    init_colors ();

    if (currentmode->flags & DM_OVERLAY)
	setoverlay ();

    if (currentmode->flags & DM_SWSCALE) {
	S2X_init (currentmode->current_width, currentmode->current_height,
	    currentmode->amiga_width, currentmode->amiga_height,
	    currentmode->current_depth, currprefs.gfx_filter == UAE_FILTER_NULL ? 1 : 2);
    }
#ifdef OPENGL
    if (currentmode->flags & DM_OPENGL) {
	const char *err = OGL_init (hAmigaWnd, currentmode->current_width, currentmode->current_height,
	    currentmode->amiga_width, currentmode->amiga_height, currentmode->current_depth);
	if (err) {
	    OGL_free ();
	    gui_message (err);
	    changed_prefs.gfx_filter = currprefs.gfx_filter = 0;
	    currentmode->current_depth = currentmode->real_depth;
	    gfxmode_reset ();
	    ret = -1;
	    goto oops;
	}
    }
#endif
#ifdef D3D
    if (currentmode->flags & DM_D3D) {
	const char *err = D3D_init (hAmigaWnd, currentmode->current_width, currentmode->current_height,
	    currentmode->amiga_width, currentmode->amiga_height, currentmode->current_depth);
	if (err) {
	    D3D_free ();
	    gui_message (err);
	    changed_prefs.gfx_filter = currprefs.gfx_filter = 0;
	    currentmode->current_depth = currentmode->real_depth;
	    gfxmode_reset ();
	    ret = -1;
	    goto oops;
	}
    }
#endif
    return 1;

oops:
    close_hwnds();
    return ret;
}


void WIN32GFX_PaletteChange( void )
{
    if (!(currentmode->flags & DM_DDRAW) || (currentmode->flags & DM_D3D)) return;
    DirectDraw_SetPalette( 1 ); /* Remove current palette */
    DirectDraw_SetPalette( 0 ); /* Set our real palette */
}

void WIN32GFX_ClearPalette( void )
{
    if (!(currentmode->flags & DM_DDRAW) || (currentmode->flags & DM_D3D)) return;
    DirectDraw_SetPalette( 1 ); /* Remove palette */
}

void WIN32GFX_SetPalette( void )
{
    if (!(currentmode->flags & DM_DDRAW) || (currentmode->flags & DM_D3D)) return;
    DirectDraw_SetPalette( 0 ); /* Set palette */
}
void WIN32GFX_WindowMove ( void )
{
    if (currentmode->flags & DM_OVERLAY)
	setoverlay();
}

void WIN32GFX_WindowSize ( void )
{
}

void updatedisplayarea (void)
{
    /* Update the display area */
    if (currentmode->flags & DM_OPENGL) {
#ifdef OPENGL
	OGL_refresh ();
#endif
    } else if (currentmode->flags & DM_D3D) {
#ifdef D3D
	D3D_refresh ();
#endif
    } else if (currentmode->flags & DM_DDRAW) {
	if (currentmode->flags & DM_SWSCALE) {
	    S2X_refresh ();
	    if( !isfullscreen() ) {
		if(DirectDraw_GetLockableType() != overlay_surface)
		    DX_Blit( 0, 0, 0, 0, WIN32GFX_GetWidth(), WIN32GFX_GetHeight(), BLIT_SRC );
	    } else {
		DirectDraw_Blt( primary_surface, NULL, secondary_surface, NULL, DDBLT_WAIT, NULL );
	    }
	} else {
	    if( !isfullscreen() ) {
		if(DirectDraw_GetLockableType() != overlay_surface)
		    DX_Blit( 0, 0, 0, 0, WIN32GFX_GetWidth(), WIN32GFX_GetHeight(), BLIT_SRC );
	    } else {
		DirectDraw_Blt( primary_surface, NULL, secondary_surface, NULL, DDBLT_WAIT, NULL );
	    }
	}
    }
}

void updatewinfsmode (struct uae_prefs *p)
{
    fixup_prefs_dimensions (p);
    if (p->gfx_afullscreen) {
	p->gfx_width = p->gfx_width_fs;
	p->gfx_height = p->gfx_height_fs;
    } else {
	p->gfx_width = p->gfx_width_win;
	p->gfx_height = p->gfx_height_win;
    }
}

void fullscreentoggle (void)
{
    if(picasso_on)
	changed_prefs.gfx_pfullscreen = !changed_prefs.gfx_pfullscreen;
    else
	changed_prefs.gfx_afullscreen = !changed_prefs.gfx_afullscreen;
    updatewinfsmode (&changed_prefs);
}

HDC gethdc (void)
{
    HDC hdc = 0;
#ifdef OPENGL
    if (OGL_isenabled())
	return OGL_getDC (0);
#endif
#ifdef D3D
    if (D3D_isenabled())
	return D3D_getDC (0);
#endif
    if(DirectDraw_GetDC(&hdc, DirectDraw_GetLockableType()) != DD_OK)
        hdc = 0;
    return hdc;
}

void releasehdc (HDC hdc)
{
#ifdef OPENGL
    if (OGL_isenabled()) {
	OGL_getDC (hdc);
	return;
    }
#endif
#ifdef D3D
    if (D3D_isenabled()) {
	D3D_getDC (hdc);
	return;
    }
#endif
    DirectDraw_ReleaseDC(hdc, DirectDraw_GetLockableType());
}