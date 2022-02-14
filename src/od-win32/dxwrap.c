/*
 * UAE - The Ultimate Amiga Emulator
 *
 * Win32 DirectX Wrappers, to simplify (?) my life.
 *
 * Copyright 1999 Brian King, under GNU Public License
 *
 */
#ifndef _MSC_VER
#define INITGUID
#endif

#include "config.h"
#include "sysconfig.h"

#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <windows.h>
#include <sys/timeb.h>

#ifdef _MSC_VER
#include <mmsystem.h>
#include <ddraw.h>
#include <dsound.h>
#include <dxerr8.h>
#else
#include "winstuff.h"
#endif

#include "sysdeps.h"
#include "options.h"
#include "picasso96.h"
#include "dxwrap.h"
#include "win32.h"
#include "win32gfx.h"

static BOOL bColourKeyAvailable = FALSE;
static BOOL bOverlayAvailable   = FALSE;
static DDCAPS_DX7 drivercaps, helcaps;
static DWORD overlayflags;
static DDOVERLAYFX overlayfx;
extern COLORREF g_dwBackgroundColor;
static int flipinterval_supported;

#define dxwrite_log

/*
 * FUNCTION:ShowDDCaps
 *
 * PURPOSE:print out the DirectDraw Capabilities
 *
 * PARAMETERS:
 *   caps    - DDCAPS_DX7 structure
 *   hw      - flag indicating if this 'caps' is for real hardware or the HEL
 *
 * RETURNS:    none
 *
 * NOTES:none
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
static void ShowDDCaps( DDCAPS_DX7 caps, int hw )
{
    static int shown = 0;
    static BOOL reset_shown = FALSE;
    if( currprefs.win32_logfile && shown >= 4 && !reset_shown)
    {
	shown = 0;
	reset_shown = TRUE;
    }

    if( shown < 2)
    {
	dxwrite_log( "DirectDraw Capabilities for %s:\n", hw ? "Display Driver Hardware" : "Display Driver Emulation Layer" );
	if( caps.dwCaps & DDCAPS_BLT )
	    dxwrite_log( "DDCAPS_BLT - Capable of blitting\n" );
	if( caps.dwCaps & DDCAPS_BLTCOLORFILL )
	    dxwrite_log( "DDCAPS_BLTCOLORFILL - Color filling with blitter\n" );
	if( caps.dwCaps & DDCAPS_BLTSTRETCH )
	    dxwrite_log( "DDCAPS_BLTSTRETCH - Stretch blitting\n" );
	if( caps.dwCaps & DDCAPS_CANBLTSYSMEM )
	    dxwrite_log( "DDCAPS_CANBLTSYSMEM - Blits from system memory\n" );
	if( caps.dwCaps & DDCAPS_CANCLIP )
	    dxwrite_log( "DDCAPS_CANCLIP - Can clip while blitting\n" );
	if( caps.dwCaps & DDCAPS_CANCLIPSTRETCHED )
	    dxwrite_log( "DDCAPS_CANCLIPSTRETCHED - Can clip while stretch-blitting\n" );
	if( caps.dwCaps & DDCAPS_COLORKEY )
	{
	    dxwrite_log( "DDCAPS_COLORKEY - Can color-key with blits/overlays\n" );
	    bColourKeyAvailable = TRUE;
	}
	if( caps.dwCaps & DDCAPS_GDI )
	    dxwrite_log( "DDCAPS_GDI - Display h/w shared with GDI\n" );
	if( caps.dwCaps & DDCAPS_NOHARDWARE )
	    dxwrite_log( "DDCAPS_NOHARDWARE - no h/w support!\n" );
	if( caps.dwCaps & DDCAPS_OVERLAY )
	{
	    dxwrite_log( "DDCAPS_OVERLAY - support for %d overlay(s)\n", caps.dwMaxVisibleOverlays );
	    if( bColourKeyAvailable )
	    {
		if( caps.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY )
		{
		    dxwrite_log( "DDCKEYCAPS_DESTOVERLAY - colour-keyed overlays\n" );
		    bOverlayAvailable = TRUE;
		}
	    }
	}
	if( caps.dwCaps & DDCAPS_OVERLAYFOURCC )
	    dxwrite_log( "DDCAPS_OVERLAYFOURCC - overlay can do color-space conversions\n" );
	if( caps.dwCaps & DDCAPS_OVERLAYSTRETCH )
	    dxwrite_log( "DDCAPS_OVERLAYSTRETCH - overlay can stretch with min=%d/max=%d\n", caps.dwMinOverlayStretch, caps.dwMaxOverlayStretch );
	if( caps.dwCaps & DDCAPS_VBI )
	    dxwrite_log( "DDCAPS_VBI - h/w can generate a vertical-blanking interrupt\n" );
	if( caps.dwCaps2 & DDCAPS2_CERTIFIED )
	    dxwrite_log( "DDCAPS2_CERTIFIED - certified driver\n" );
	if( caps.dwCaps2 & DDCAPS2_CANRENDERWINDOWED )
	    dxwrite_log( "DDCAPS2_CANRENDERWINDOWED - GDI windows can be seen when in full-screen\n" );
	if( caps.dwCaps2 & DDCAPS2_NOPAGELOCKREQUIRED )
	    dxwrite_log( "DDCAPS2_NOPAGELOCKREQUIRED - no page locking needed for DMA blits\n" );
	if( caps.dwCaps2 & DDCAPS2_FLIPNOVSYNC )
	    dxwrite_log( "DDCAPS2_FLIPNOVSYNC - can pass DDFLIP_NOVSYNC to Flip calls\n" );
	if( caps.dwCaps2 & DDCAPS2_FLIPINTERVAL ) {
	    dxwrite_log( "DDCAPS2_FLIPINTERVAL - can pass DDFLIP_INTERVALx to Flip calls\n" );
	    flipinterval_supported = 1;
	}
	
	dxwrite_log( "Video memory: %d/%d\n", caps.dwVidMemFree, caps.dwVidMemTotal );
    }
    shown++;
}

const char *DXError (HRESULT ddrval)
{
    static char dderr[200];
    sprintf(dderr, "%08.8X S=%d F=%04.4X C=%04.4X (%d) (%s)",
	ddrval, (ddrval & 0x80000000) ? 1 : 0,
	HRESULT_FACILITY(ddrval),
	HRESULT_CODE(ddrval),
	HRESULT_CODE(ddrval),
	DXGetErrorDescription8 (ddrval));
    return dderr;
}

static struct DirectDrawSurfaceMapper DirectDrawState;

static int lockcnt = 0;

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
static int LockStub( surface_type_e type )
{
    int result = 0;
    HRESULT ddrval;
    LPDIRECTDRAWSURFACE7 surface;
    LPDDSURFACEDESC2 surfacedesc;

    switch( type )
    {
    case primary_surface:
	surface = DirectDrawState.primary.surface;
	surfacedesc = &DirectDrawState.primary.desc;
	break;
    case secondary_surface:
	surface = DirectDrawState.secondary.surface;
        surfacedesc = &DirectDrawState.secondary.desc;
	break;
    case tertiary_surface:
	surface = DirectDrawState.tertiary.surface;
        surfacedesc = &DirectDrawState.tertiary.desc;
	break;
    case overlay_surface:
	surface = DirectDrawState.overlay.surface;
        surfacedesc = &DirectDrawState.overlay.desc;
	break;
    }

    if( lockcnt )
    {
#ifdef _DEBUG
	DebugBreak();
#endif
	return 1;
    }

    if( type == secondary_surface && DirectDrawState.flipping != single_buffer )
    {
	IDirectDrawSurface7_Restore( DirectDrawState.primary.surface );
    }

    while ( (ddrval = IDirectDrawSurface7_Lock( surface, NULL, surfacedesc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT,
                                                NULL ) ) != DD_OK )
    {
        if (ddrval == DDERR_SURFACELOST) {
    	    ddrval = IDirectDrawSurface7_Restore( surface );
            if (ddrval != DD_OK)
            {
                result = 0;
                break;
            }
        }
        else if (ddrval != DDERR_SURFACEBUSY) 
        {
	    write_log ("lpDDS->Lock() failed - %s\n", DXError (ddrval));
            result = 0;
            break;
        }
    }
    if( ddrval == DD_OK )
        result = 1;
    
    if( result )
        lockcnt++;

    return result;
}

/* For a given surface-type, update our DirectDrawState structure */
/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
int DirectDraw_SurfaceLock( surface_type_e surface_type )
{
    int result = 0;

    if( surface_type == lockable_surface )
        surface_type = DirectDraw_GetLockableType();

    switch( surface_type )
    {
        case primary_surface:
            DirectDrawState.primary.desc.dwSize = sizeof( DDSURFACEDESC2 );
            result = LockStub( surface_type );
        break;
        case secondary_surface:
            DirectDrawState.secondary.desc.dwSize = sizeof( DDSURFACEDESC2 );
            result = LockStub( surface_type );
        break;
        case tertiary_surface:
            DirectDrawState.tertiary.desc.dwSize = sizeof( DDSURFACEDESC2 );
            result = LockStub( surface_type );
        break;
	case overlay_surface:
	    DirectDrawState.overlay.desc.dwSize = sizeof( DDSURFACEDESC2 );
	    result = LockStub( surface_type );
        case lockable_surface:
        case invalid_surface:
        default:

        break;
    }
    DirectDrawState.locked = result;

    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
char *DirectDraw_GetSurfacePointer( void )
{
    char *pixels = NULL;

    /* Make sure that somebody has done a lock before returning the lpSurface member */
    if( lockcnt )
    {
        pixels = DirectDrawState.lockable.lpdesc->lpSurface;
    }
    return pixels;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
LONG DirectDraw_GetSurfacePitch( void )
{
    LONG pitch = 0;

    pitch = DirectDrawState.lockable.lpdesc->lPitch;
    return pitch;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_GetPixelFormatFlags( void )
{
    DWORD flags = 0;
    flags = DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwFlags;
    return flags;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_GetSurfaceFlags( void )
{
    DWORD flags = 0;
    flags = DirectDrawState.lockable.lpdesc->dwFlags;
    return flags;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_GetSurfaceBitCount( void )
{
    DWORD bits = 0;
    //?????JGI begin:
    if( DirectDrawState.lockable.lpdesc )
	bits = DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwRGBBitCount;
    else
	bits = DirectDrawState.current.desc.ddpfPixelFormat.dwRGBBitCount;
    //?????JGI end.
    return bits;
}

/*
 * FUNCTION:DirectDraw_GetPrimaryBitCount
 *
 * PURPOSE:Return the bit-depth of the primary surface
 *
 * PARAMETERS: none
 *
 * RETURNS:    bit-depth
 *
 * NOTES:
 *
 * HISTORY:
 *   2001.08.25  Brian King             Creation
 *
 */
DWORD DirectDraw_GetPrimaryBitCount( void )
{
    DWORD bits = 0;
    memset(&DirectDrawState.primary.desc,0,sizeof(DirectDrawState.primary.desc));
    DirectDrawState.primary.desc.dwSize = sizeof(DirectDrawState.primary.desc);

    IDirectDrawSurface7_GetSurfaceDesc(DirectDrawState.primary.surface, &DirectDrawState.primary.desc);
    bits = DirectDrawState.primary.desc.ddpfPixelFormat.dwRGBBitCount;
    return bits;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_GetPixelFormatBitMask( DirectDraw_Mask_e mask )
{
    DWORD result = 0;
    switch( mask )
    {
        case red_mask:
            result = DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwRBitMask;
        break;
        case green_mask:
            result = DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwGBitMask;
        break;
        case blue_mask:
            result = DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwBBitMask;
        break;
    }
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
surface_type_e DirectDraw_GetLockableType( void )
{
    return DirectDrawState.surface_type;
}

/*
 * FUNCTION:DirectDraw_IsLocked
 *
 * PURPOSE:Return whether we're currently locked or unlocked
 *
 * PARAMETERS: none
 *
 * RETURNS:    TRUE if already locked, FALSE otherwise
 *
 * NOTES:Used by DX_Blit to possibly unlock during Blit operation
 *
 * HISTORY:
 *   2000.04.30  Brian King             Creation
 *
 */
BOOL DirectDraw_IsLocked( void )
{
    return DirectDrawState.locked ? TRUE : FALSE;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
static surface_type_e try_surface_locks( int want_fullscreen )
{
    surface_type_e result = invalid_surface;

    if( DirectDrawState.isoverlay && DirectDraw_SurfaceLock( overlay_surface ) )
    {
	result = overlay_surface;
	write_log( "try_surface_locks() returning overlay\n" );
    }
    else if( want_fullscreen && WIN32GFX_IsPicassoScreen() )
    {
	if( DirectDraw_SurfaceLock( primary_surface ) )
	{
	    result = primary_surface;
	    write_log( "try_surface_locks() returning primary\n" );
	}
	else if( DirectDraw_SurfaceLock( secondary_surface ) )
	{
	    result = secondary_surface;
	    write_log( "try_surface_locks() returning secondary\n" );
	}
    }
    else
    {
        if( DirectDraw_SurfaceLock( secondary_surface ) )
        {
            result = secondary_surface;
	    write_log( "try_surface_locks() returning secondary\n" );
        }
    }

    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:Named this way for historical reasons
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
void ddraw_unlockscr( void )
{
    if( lockcnt > 0 )
    {
	lockcnt--;
	IDirectDrawSurface7_Unlock( DirectDrawState.lockable.surface,
				    DirectDrawState.lockable.lpdesc->lpSurface );
	DirectDrawState.locked = FALSE;
    }
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
int DirectDraw_Start( void )
{
    HRESULT ddrval;
    DDCAPS_DX7 drivercaps, helcaps;

    /* Prepare our DirectDrawState structure */
    ZeroMemory( &DirectDrawState, sizeof( DirectDrawState ) );

    ZeroMemory( &drivercaps, sizeof( drivercaps ) );
    ZeroMemory( &helcaps, sizeof( helcaps ) );
    drivercaps.dwSize = sizeof( drivercaps );
    helcaps.dwSize = sizeof( helcaps );

    ddrval = DirectDrawCreate( NULL, &DirectDrawState.directdraw.ddx, NULL );
    if (ddrval != DD_OK)
	goto oops;

    DirectDrawState.initialized = TRUE;

    ddrval = IDirectDraw_QueryInterface( DirectDrawState.directdraw.ddx,
                                         &IID_IDirectDraw7,
                                         (LPVOID *)&DirectDrawState.directdraw.dd );
    if( ddrval != DD_OK )
    {
	gui_message("start_ddraw(): DirectX 7 or newer required");
	return 0;
    }

    DirectDraw_GetCaps( &drivercaps, &helcaps );
    ShowDDCaps( drivercaps, 1 );
    ShowDDCaps( helcaps, 0 );

    ddrval = DirectDraw_GetDisplayMode();
    if (ddrval != DD_OK)
	goto oops;

    return 1;

  oops:
    gui_message("start_ddraw(): DirectDraw initialization failed with %s\n", DXError (ddrval));
    DirectDraw_Release();
    return 0;
}

#define releaser(x,y) if( x ) { y( x ); x = NULL; }

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
void DirectDraw_Release( void )
{
    releaser( DirectDrawState.lpDDC, IDirectDrawClipper_Release );
    releaser( DirectDrawState.lpDDP, IDirectDrawPalette_Release );

    if( DirectDrawState.directdraw.dd )
    {
	IDirectDraw7_RestoreDisplayMode( DirectDrawState.directdraw.dd );
	IDirectDraw7_SetCooperativeLevel( DirectDrawState.directdraw.dd, hAmigaWnd, DDSCL_NORMAL);
    }

    releaser( DirectDrawState.overlay.surface, IDirectDrawSurface7_Release );
    releaser( DirectDrawState.primary.surface, IDirectDrawSurface7_Release );

    if( DirectDrawState.flipping == single_buffer)
	releaser( DirectDrawState.secondary.surface, IDirectDrawSurface7_Release );

    releaser( DirectDrawState.directdraw.dd, IDirectDraw_Release );

    DirectDrawState.lockable.lpdesc = NULL;
    DirectDrawState.lockable.lpdesc = NULL;
    DirectDrawState.lockable.surface = NULL;
    DirectDrawState.lockable.surface = NULL;

    DirectDrawState.surface_type = invalid_surface;
    DirectDrawState.initialized = FALSE;
    DirectDrawState.isoverlay = FALSE;
}

/*
 * FUNCTION:DirectDraw_SetCooperativeLevel
 *
 * PURPOSE:Wrapper for setting the cooperative level (fullscreen or normal)
 *
 * PARAMETERS:
 *   window		Window to set the cooperative level for
 *   want_fullscreen	fullscreen mode flag
 *
 * RETURNS:		result of underlying DirectDraw call
 *
 * NOTES:		Updates the .fullscreen and .window members.
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_SetCooperativeLevel( HWND window, int want_fullscreen )
{
    HRESULT ddrval;

    ddrval = IDirectDraw7_SetCooperativeLevel( DirectDrawState.directdraw.dd,
                                               window,
                                               want_fullscreen ?
                                               DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN :
                                               DDSCL_NORMAL );
    if( ddrval == DD_OK )
    {
	DirectDrawState.fullscreen = want_fullscreen;
	DirectDrawState.window = window;
    }
    return ddrval;
}

/*
 * FUNCTION:DirectDraw_GetCooperativeLevel
 *
 * PURPOSE:Wrapper for setting the cooperative level (fullscreen or normal)
 *
 * PARAMETERS:
 *   window		Window to set the cooperative level for
 *   want_fullscreen	fullscreen mode flag
 *
 * RETURNS:		result of underlying DirectDraw call
 *
 * NOTES:		Updates the .fullscreen and .window members.
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
BOOL DirectDraw_GetCooperativeLevel( HWND *window, int *fullscreen )
{
    BOOL result = FALSE;

    if( DirectDrawState.initialized )
    {
	*fullscreen = DirectDrawState.fullscreen;
	*window = DirectDrawState.window;
	result = TRUE;
    }
    return result;
}

/*
 * FUNCTION:DirectDraw_SetDisplayMode
 *
 * PURPOSE:Change the display-mode to width x height pixels, with a given
 *             vertical refresh-rate.
 *
 * PARAMETERS:
 *   width   - width of display in pixels
 *   height  - height of display in pixels
 *   freq    - vertical refresh-rate in Hz
 *
 * RETURNS:
 *   ddrval  - HRESULT indicating success (DD_OK) or failure
 *
 * NOTES:The freq parameter is only obeyed on when we're using DirectX 6
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_SetDisplayMode( int width, int height, int bits, int freq )
{
    HRESULT ddrval;

    ddrval = IDirectDraw7_SetDisplayMode( DirectDrawState.directdraw.dd,
                                          width, height, bits, freq, 0 );
    return ddrval;
}

/*
 * FUNCTION:DirectDraw_GetDisplayMode
 *
 * PURPOSE:Get the display-mode characteristics.
 *
 * PARAMETERS: none
 *
 * RETURNS:
 *   ddrval  - HRESULT indicating success (DD_OK) or failure
 *
 * NOTES:none
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_GetDisplayMode( void )
{
    HRESULT ddrval;

    DirectDrawState.current.desc.dwSize = sizeof( DDSURFACEDESC2 );
    ddrval = IDirectDraw7_GetDisplayMode( DirectDrawState.directdraw.dd,
                                          &DirectDrawState.current.desc );

    /* We fill in the current.desc in all cases */
    DirectDrawState.current.desc.dwSize = sizeof( DDSURFACEDESC2 );
    ddrval = IDirectDraw7_GetDisplayMode( DirectDrawState.directdraw.dd,
                                         &DirectDrawState.current.desc );
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_GetCaps( DDCAPS_DX7 *driver_caps, DDCAPS_DX7 *hel_caps )
{
    HRESULT ddrval;

    ddrval = IDirectDraw7_GetCaps( DirectDrawState.directdraw.dd,
                                   driver_caps, hel_caps );
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_CreateClipper( void )
{
    HRESULT ddrval;
    ddrval = IDirectDraw7_CreateClipper( DirectDrawState.directdraw.dd,
                                         0,
                                         &DirectDrawState.lpDDC,
                                         NULL );
    return ddrval;
}

static DWORD ConvertGDIColor( COLORREF dwGDIColor )
{
    COLORREF rgbT;
    HDC hdc;
    DWORD dw = CLR_INVALID;
    DDSURFACEDESC2 ddsd,pdds;
    HRESULT hr;

    memset(&pdds,0,sizeof(pdds));
    pdds.dwSize = sizeof(pdds);
 
    IDirectDrawSurface7_GetSurfaceDesc(DirectDrawState.primary.surface, &pdds);

    //  Use GDI SetPixel to color match for us
    if( dwGDIColor != CLR_INVALID && IDirectDrawSurface7_GetDC(DirectDrawState.primary.surface, &hdc) == DD_OK)
    {
        rgbT = GetPixel(hdc, 0, 0);     // Save current pixel value
        SetPixel(hdc, 0, 0, dwGDIColor);       // Set our value
        IDirectDrawSurface7_ReleaseDC(DirectDrawState.primary.surface,hdc);
    }

    // Now lock the surface so we can read back the converted color
    ddsd.dwSize = sizeof(ddsd);
    hr = IDirectDrawSurface7_Lock(DirectDrawState.primary.surface, NULL, &ddsd, DDLOCK_WAIT, NULL );
    if( hr == DD_OK)
    {
        dw = *(DWORD *) ddsd.lpSurface; 
        if( ddsd.ddpfPixelFormat.dwRGBBitCount < 32 ) // Mask it to bpp
            dw &= ( 1 << ddsd.ddpfPixelFormat.dwRGBBitCount ) - 1;
	IDirectDrawSurface7_Unlock(DirectDrawState.primary.surface,NULL);
    }

    //  Now put the color that was there back.
    if( dwGDIColor != CLR_INVALID && IDirectDrawSurface7_GetDC(DirectDrawState.primary.surface,&hdc) == DD_OK )
    {
        SetPixel( hdc, 0, 0, rgbT );
	IDirectDrawSurface7_ReleaseDC(DirectDrawState.primary.surface,hdc);
    }
    
    return dw;    
}


HRESULT DirectDraw_CreateOverlaySurface( int width, int height, int bits)
{
    DDSURFACEDESC2 ddsd;
    DDPIXELFORMAT ddpfOverlayFormat;
    HRESULT ddrval = DDERR_UNSUPPORTED;
    DWORD dwDDSColor;

    if( bOverlayAvailable )
    {
	write_log( "CreateOverlaySurface being called with %d-bits!\n", bits );
	if( bits == 16 )
	{
	    // Set the overlay format to 16 bit RGB 5:6:5
	    ZeroMemory( &ddpfOverlayFormat, sizeof(ddpfOverlayFormat) );
	    ddpfOverlayFormat.dwSize        = sizeof(ddpfOverlayFormat);
	    ddpfOverlayFormat.dwFlags       = DDPF_RGB;
	    ddpfOverlayFormat.dwRGBBitCount = 16;
	    ddpfOverlayFormat.dwRBitMask    = 0xF800; 
	    ddpfOverlayFormat.dwGBitMask    = 0x07E0;
	    ddpfOverlayFormat.dwBBitMask    = 0x001F; 
	}
	else if( bits == 32 )
	{
	    // Set the overlay format to 32 bit ARGB 8:8:8:8
	    ZeroMemory( &ddpfOverlayFormat, sizeof(ddpfOverlayFormat) );
	    ddpfOverlayFormat.dwSize        = sizeof(ddpfOverlayFormat);
	    ddpfOverlayFormat.dwFlags       = DDPF_RGB;
	    ddpfOverlayFormat.dwRGBBitCount = 32;
	    ddpfOverlayFormat.dwRBitMask    = 0x00FF0000; 
	    ddpfOverlayFormat.dwGBitMask    = 0x0000FF00;
	    ddpfOverlayFormat.dwBBitMask    = 0x000000FF; 
	}
	else if( bits == 8 )
	{
	    // Set the overlay format to 8 bit palette
	    ZeroMemory( &ddpfOverlayFormat, sizeof(ddpfOverlayFormat) );
	    ddpfOverlayFormat.dwSize        = sizeof(ddpfOverlayFormat);
	    ddpfOverlayFormat.dwFlags       = DDPF_RGB | DDPF_PALETTEINDEXED8;
	    ddpfOverlayFormat.dwRGBBitCount = 8;
	    ddpfOverlayFormat.dwRBitMask    = 0x00000000; 
	    ddpfOverlayFormat.dwGBitMask    = 0x00000000;
	    ddpfOverlayFormat.dwBBitMask    = 0x00000000; 
	}
	else
	{
	    // We don't handle this case...
	    return DDERR_INVALIDPIXELFORMAT;
	}
    
	// Setup the overlay surface's attributes in the surface descriptor
	ZeroMemory( &ddsd, sizeof(ddsd) );
	ddsd.dwSize            = sizeof(ddsd);
	ddsd.dwFlags           = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps    = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
	ddsd.dwWidth           = width;
	ddsd.dwHeight          = height;
	ddsd.ddpfPixelFormat   = ddpfOverlayFormat;

	ZeroMemory(&overlayfx,sizeof(overlayfx));
	overlayfx.dwSize = sizeof(overlayfx);
	overlayflags = DDOVER_SHOW | DDOVER_DDFX | DDOVER_KEYDESTOVERRIDE;

	dwDDSColor = ConvertGDIColor( g_dwBackgroundColor );
	overlayfx.dckDestColorkey.dwColorSpaceLowValue  = dwDDSColor;
	overlayfx.dckDestColorkey.dwColorSpaceHighValue = dwDDSColor;

	// Attempt to create the surface with theses settings
	ddrval = IDirectDraw7_CreateSurface ( DirectDrawState.directdraw.dd, &ddsd, &DirectDrawState.overlay.surface, NULL);
	if( ddrval == DD_OK )
	{
#if 0
	    ddrval = IDirectDrawSurface7_QueryInterface( DirectDrawState.overlay.surface,
			&IID_IDirectDrawSurface7,(LPVOID *)&DirectDrawState.overlay.surface );
#endif
	    DirectDrawState.isoverlay = 1;
	}
	else
	{
	    DirectDrawState.isoverlay = 0;
	}
    }
    else
    {
	write_log( "CreateOverlaySurface being called, but no overlay support with this card...!\n" );
    }
    return ddrval;
}


/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_CreateSurface( int width, int height )
{
    HRESULT ddrval;

    DirectDrawState.flipping = single_buffer;

    if( DirectDrawState.fullscreen ) // Create a flipping pair!
    {
	ZeroMemory( &DirectDrawState.primary.desc, sizeof( DDSURFACEDESC2 ) );
	DirectDrawState.primary.desc.dwSize = sizeof( DDSURFACEDESC2 );
	DirectDrawState.primary.desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	DirectDrawState.primary.desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
	DirectDrawState.primary.desc.dwBackBufferCount = 2;
	ddrval = IDirectDraw7_CreateSurface( DirectDrawState.directdraw.dd, 
					    &DirectDrawState.primary.desc,
					    &DirectDrawState.primary.surface,
					    NULL );
	if( ddrval != DD_OK )
	{
	    // Create a non-flipping pair, since the flipping pair creation failed...
	    ZeroMemory( &DirectDrawState.primary.desc, sizeof( DDSURFACEDESC2 ) );
	    DirectDrawState.primary.desc.dwSize = sizeof( DDSURFACEDESC2 );
	    DirectDrawState.primary.desc.dwFlags = DDSD_CAPS;
	    DirectDrawState.primary.desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	    ddrval = IDirectDraw7_CreateSurface( DirectDrawState.directdraw.dd, 
						&DirectDrawState.primary.desc,
						&DirectDrawState.primary.surface,
						NULL );
	}
	else
	{
	    DirectDrawState.flipping = triple_buffer;
	}
    }
    else
    {
	// We're not full-screen, so you cannot create a flipping pair...

	ZeroMemory( &DirectDrawState.primary.desc, sizeof( DDSURFACEDESC2 ) );
	DirectDrawState.primary.desc.dwSize = sizeof( DDSURFACEDESC2 );
	DirectDrawState.primary.desc.dwFlags = DDSD_CAPS;
	DirectDrawState.primary.desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddrval = IDirectDraw7_CreateSurface( DirectDrawState.directdraw.dd, 
					    &DirectDrawState.primary.desc,
					    &DirectDrawState.primary.surface,
					    NULL );
    }

    if( ddrval != DD_OK )
    {
	goto out;
    }
    else
    {
	write_log( "DDRAW: Primary %ssurface created in video-memory\n", DirectDrawState.flipping != single_buffer ? "flipping " : "" );
    }

    // Check if we can access the back-buffer of our flipping-pair (if present)
    if( DirectDrawState.flipping != single_buffer )
    {
	DDSCAPS2 ddSCaps;
	ZeroMemory(&ddSCaps, sizeof(ddSCaps));
	ddSCaps.dwCaps = DDSCAPS_BACKBUFFER;

	ddrval = IDirectDrawSurface7_GetAttachedSurface( DirectDrawState.primary.surface, &ddSCaps, &DirectDrawState.secondary.surface );
	if( ddrval == DD_OK )
	{
	    /* get third buffer */
	    ZeroMemory(&ddSCaps, sizeof(ddSCaps));
	    ddSCaps.dwCaps = DDSCAPS_FLIP;

	    ddrval = IDirectDrawSurface7_GetAttachedSurface( DirectDrawState.secondary.surface, &ddSCaps, &DirectDrawState.tertiary.surface );
	    if( ddrval == DD_OK )
	    {
    #if 0
		// Get our IDirectDrawSurface7 pointer
		ddrval = IDirectDrawSurface7_QueryInterface( DirectDrawState.tertiary.surface,
							    &IID_IDirectDrawSurface7,
							    (LPVOID *)&DirectDrawState.tertiary.surface );
		if( ddrval != DD_OK )
		{
		    goto out;
		}
    #endif
	    }
	    else
	    {
		DirectDrawState.flipping = single_buffer;
	    }
    #if 0
	    // Get our IDirectDrawSurface7 pointer
	    ddrval = IDirectDrawSurface_QueryInterface( DirectDrawState.secondary.surface,
							&IID_IDirectDrawSurface7,
							(LPVOID *)&DirectDrawState.secondary.surface );
	    if( ddrval != DD_OK )
	    {
		goto out;
	    }
#endif
	}
	else
	{
	    DirectDrawState.flipping = single_buffer;
	}
    }

#if 0
    // Get our IDirectDrawSurface7 pointer
    ddrval = IDirectDrawSurface7_QueryInterface( DirectDrawState.primary.surface,
						&IID_IDirectDrawSurface7,
						(LPVOID *)&DirectDrawState.primary.surface );

    if( ddrval != DD_OK )
    {
	goto out;
    }
#endif

    // We always want a secondary-buffer when creating our primary-surface.  If we're a flipping pair,
    // the secondary buffer is already allocated.  If we failed to create a flipping pair, or because
    // we're not full-screen, then lets create ourselves a back-buffer manually.
    if( DirectDrawState.flipping == single_buffer )
    {
        ZeroMemory( &DirectDrawState.secondary.desc, sizeof( DDSURFACEDESC2 ) );
        DirectDrawState.secondary.desc.dwSize = sizeof( DDSURFACEDESC2 );
        DirectDrawState.secondary.desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        DirectDrawState.secondary.desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        DirectDrawState.secondary.desc.dwWidth = width;
        DirectDrawState.secondary.desc.dwHeight = height;
        ddrval = IDirectDraw7_CreateSurface( DirectDrawState.directdraw.dd, 
                                            &DirectDrawState.secondary.desc,
                                            &DirectDrawState.secondary.surface,
                                            NULL );
        if( ddrval != DD_OK )
        {
	    write_log( "DDRAW:Secondary surface creation attempt #1 failed with %s\n", DXError(ddrval));
            DirectDrawState.secondary.desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
            ddrval = IDirectDraw7_CreateSurface( DirectDrawState.directdraw.dd, 
                                                &DirectDrawState.secondary.desc,
                                                &DirectDrawState.secondary.surface,
                                                NULL );
	    if( ddrval == DD_OK )
		write_log( "DDRAW: Secondary surface created in plain system-memory\n" );
	    else
	    {
		goto out;
	    }
        }
        else
        {
            write_log( "DDRAW: Secondary surface created in video-memory\n" );
        }
#if 0
	// Get our IDirectDrawSurface7 pointer
        ddrval = IDirectDrawSurface7_QueryInterface( DirectDrawState.secondary.surface,
                                                    &IID_IDirectDrawSurface7,
                                                    (LPVOID *)&DirectDrawState.secondary.surface );
        if( ddrval != DD_OK )
        {
	    goto out;
        }
#endif
    }
out:
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
void DirectDraw_ClearSurfaces( void )
{
    DDBLTFX ddbltfx;
    memset( &ddbltfx, 0, sizeof( ddbltfx ) );
    ddbltfx.dwFillColor = 0;
    ddbltfx.dwSize = sizeof( ddbltfx );

    DirectDraw_Blt( secondary_surface, NULL, invalid_surface, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx );
    if( DirectDrawState.isoverlay )
	DirectDraw_Blt( overlay_surface, NULL, invalid_surface, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx );
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
int DirectDraw_DetermineLocking( int wantfull )
{
    int result = 0;

    switch( DirectDrawState.surface_type = try_surface_locks( wantfull ) )
    {
    case invalid_surface:
    case lockable_surface:
        DirectDrawState.lockable.lpdesc = NULL;
        DirectDrawState.lockable.lpdesc = NULL;
        DirectDrawState.lockable.surface = NULL;
        DirectDrawState.lockable.surface = NULL;
        write_log( "set_ddraw: Couldn't lock primary, and no secondary available.\n" );
        break;
    case primary_surface:
        DirectDrawState.lockable.lpdesc = &DirectDrawState.primary.desc;
        DirectDrawState.lockable.lpdesc = &DirectDrawState.primary.desc;
        DirectDrawState.lockable.surface = DirectDrawState.primary.surface;
        DirectDrawState.lockable.surface = DirectDrawState.primary.surface;
	result = 1;
        break;
    case overlay_surface:
        DirectDrawState.lockable.lpdesc = &DirectDrawState.overlay.desc;
        DirectDrawState.lockable.lpdesc = &DirectDrawState.overlay.desc;
        DirectDrawState.lockable.surface = DirectDrawState.overlay.surface;
        DirectDrawState.lockable.surface = DirectDrawState.overlay.surface;
	result = 1;
        break;
    case secondary_surface:
        DirectDrawState.lockable.lpdesc = &DirectDrawState.secondary.desc;
        DirectDrawState.lockable.lpdesc = &DirectDrawState.secondary.desc;
        DirectDrawState.lockable.surface = DirectDrawState.secondary.surface;
        DirectDrawState.lockable.surface = DirectDrawState.secondary.surface;
	result = 1;
        break;
    case tertiary_surface:
        DirectDrawState.lockable.lpdesc = &DirectDrawState.tertiary.desc;
        DirectDrawState.lockable.lpdesc = &DirectDrawState.tertiary.desc;
        DirectDrawState.lockable.surface = DirectDrawState.tertiary.surface;
        DirectDrawState.lockable.surface = DirectDrawState.tertiary.surface;
	result = 1;
        break;
    }

    if( DirectDrawState.lockable.surface )
        DirectDraw_SurfaceUnlock();

    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_SetClipper( HWND hWnd )
{
    HRESULT ddrval;

    ddrval = IDirectDrawSurface7_SetClipper( DirectDrawState.primary.surface,
                                             hWnd ? DirectDrawState.lpDDC : NULL );
    if( hWnd && ( ddrval == DD_OK ) )
    {
        ddrval = IDirectDrawClipper_SetHWnd( DirectDrawState.lpDDC, 0, hWnd );
    }
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_GetClipList( LPRGNDATA cliplist, LPDWORD size )
{
    HRESULT ddrval;

    ddrval = IDirectDrawClipper_GetClipList( DirectDrawState.lpDDC, NULL, cliplist, size );

    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
BYTE DirectDraw_GetBytesPerPixel( void )
{
    int bpp;
    bpp = ( DirectDrawState.lockable.lpdesc->ddpfPixelFormat.dwRGBBitCount + 7 ) >> 3;
    return bpp;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_SetPalette( int remove )
{
    HRESULT ddrval;
    if (DirectDrawState.primary.surface == NULL) return 0;
    ddrval = IDirectDrawSurface7_SetPalette( DirectDrawState.primary.surface,
                                             remove ? NULL : DirectDrawState.lpDDP );
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_CreatePalette( LPPALETTEENTRY pal )
{
    HRESULT ddrval;
    ddrval = IDirectDraw_CreatePalette( DirectDrawState.directdraw.dd,
                                        DDPCAPS_8BIT | DDPCAPS_ALLOW256,
                                        pal,
                                        &DirectDrawState.lpDDP, NULL);
    if( ddrval == DD_OK )
    {
        ddrval = DirectDraw_SetPalette(0);
    }
    return ddrval;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_SetPaletteEntries( int start, int count, PALETTEENTRY *palette )
{
    HRESULT ddrval = DDERR_NOPALETTEATTACHED;
    if( DirectDrawState.lpDDP )
        ddrval = IDirectDrawPalette_SetEntries( DirectDrawState.lpDDP, 0, start, count, palette );
    return ddrval;
}

/* Return one of the pixel formats declared in picasso96.h if the surface
 * is usable for us, or RGBFB_NONE if it is not usable.  */
/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
RGBFTYPE DirectDraw_GetSurfacePixelFormat( LPDDSURFACEDESC2 surface )
{
    int surface_is = 0;
    DDPIXELFORMAT *pfp = NULL;
    DWORD r, g, b;
    DWORD surf_flags;

    surf_flags = surface->dwFlags;
    pfp = &surface->ddpfPixelFormat;

    if( ( surf_flags & DDSD_PIXELFORMAT ) == 0x0 )
	return RGBFB_NONE;

    if ((pfp->dwFlags & DDPF_RGB) == 0)
	return RGBFB_NONE;

    r = pfp->dwRBitMask;
    g = pfp->dwGBitMask;
    b = pfp->dwBBitMask;
    switch (pfp->dwRGBBitCount) {
     case 8:
	if ((pfp->dwFlags & DDPF_PALETTEINDEXED8) != 0)
	    return RGBFB_CHUNKY;
	break;

     case 16:
	if (r == 0xF800 && g == 0x07E0 && b == 0x001F)
	    return RGBFB_R5G6B5PC;
	if (r == 0x7C00 && g == 0x03E0 && b == 0x001F)
	    return RGBFB_R5G5B5PC;
	if (b == 0xF800 && g == 0x07E0 && r == 0x001F)
	    return RGBFB_B5G6R5PC;
	if (b == 0x7C00 && g == 0x03E0 && r == 0x001F)
	    return RGBFB_B5G5R5PC;
	/* This happens under NT - with r == b == g == 0 !!! */
	write_log ("Unknown 16 bit format %d %d %d\n", r, g, b);
	break;

     case 24:
	if (r == 0xFF0000 && g == 0x00FF00 && b == 0x0000FF)
	    return RGBFB_B8G8R8;
	if (r == 0x0000FF && g == 0x00FF00 && b == 0xFF0000)
	    return RGBFB_R8G8B8;
	break;

     case 32:
	if (r == 0x00FF0000 && g == 0x0000FF00 && b == 0x000000FF)
	    return RGBFB_B8G8R8A8;
	if (r == 0x000000FF && g == 0x0000FF00 && b == 0x00FF0000)
	    return RGBFB_R8G8B8A8;
	if (r == 0xFF000000 && g == 0x00FF0000 && b == 0x0000FF00)
	    return RGBFB_A8B8G8R8;
	if (r == 0x0000FF00 && g == 0x00FF0000 && b == 0xFF000000)
	    return RGBFB_A8R8G8B8;
	break;
	
     default:
	write_log ("Unknown %d bit format %d %d %d\n", pfp->dwRGBBitCount, r, g, b); /* %%% - BERND, and here too... */
	break;
    }
    return RGBFB_NONE;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
RGBFTYPE DirectDraw_GetPixelFormat( void )
{
    RGBFTYPE type;
    if( DirectDrawState.lockable.lpdesc )
	type = DirectDraw_GetSurfacePixelFormat( DirectDrawState.lockable.lpdesc );
    else
	type = DirectDraw_GetSurfacePixelFormat( &DirectDrawState.current.desc );
    return type;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_CurrentWidth( void )
{
    DWORD width;
    width = DirectDrawState.current.desc.dwWidth;
    return width;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
DWORD DirectDraw_CurrentHeight( void )
{
    DWORD height;
    height = DirectDrawState.current.desc.dwHeight;
    return height;
}

DWORD DirectDraw_CurrentRefreshRate( void )
{
    DWORD height;
    height = DirectDrawState.current.desc.dwRefreshRate;
    return height;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
static int DirectDraw_BltFastStub4( LPDIRECTDRAWSURFACE7 dstsurf, DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcsurf, LPRECT srcrect )
{
    int result = 0;
    HRESULT ddrval;

    while( ( ddrval = IDirectDrawSurface7_BltFast( dstsurf, x, y, srcsurf, srcrect, DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAIT ) ) != DD_OK )
    {
        if (ddrval == DDERR_SURFACELOST) 
        {
    	    ddrval = IDirectDrawSurface7_Restore( dstsurf );
            if (ddrval != DD_OK)
            {
                break;
            }
        }
        else if (ddrval != DDERR_SURFACEBUSY) 
        {
	    write_log("BltFastStub7(): DirectDrawSURFACE7_BltFast() failed with %s\n", DXError (ddrval));
            break;
        }
    }
    if( ddrval == DD_OK )
    {
        result = 1;
    }
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
int DirectDraw_BltFast( surface_type_e dsttype, DWORD left, DWORD top, surface_type_e srctype, LPRECT srcrect )
{
    int result;

    LPDIRECTDRAWSURFACE7 lpDDS4_dst, lpDDS4_src;
    if( dsttype == primary_surface )
    {
        lpDDS4_dst = DirectDrawState.primary.surface;
    }
    else
    {
        lpDDS4_dst = DirectDrawState.secondary.surface;
    }
    if( srctype == primary_surface )
    {
        lpDDS4_src = DirectDrawState.primary.surface;
    }
    else
    {
        lpDDS4_src = DirectDrawState.secondary.surface;
    }
    result = DirectDraw_BltFastStub4( lpDDS4_dst, left, top, lpDDS4_src, srcrect );
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
static int DirectDraw_BltStub( LPDIRECTDRAWSURFACE7 dstsurf, LPRECT dstrect, LPDIRECTDRAWSURFACE7 srcsurf, LPRECT srcrect, DWORD flags, LPDDBLTFX ddbltfx )
{
    int result = 0, errcnt = 0;
    HRESULT ddrval;

    while( ( ddrval = IDirectDrawSurface7_Blt( dstsurf, dstrect, srcsurf, srcrect, flags, ddbltfx ) ) != DD_OK )
    {
        if (ddrval == DDERR_SURFACELOST) 
        {
	    if (errcnt > 10)
		break;
	    errcnt++;
    	    ddrval = IDirectDrawSurface7_Restore( dstsurf );
            if (ddrval != DD_OK)
            {
                break;
            }
        }
        else if (ddrval != DDERR_SURFACEBUSY) 
        {
	    write_log("BltStub(): DirectDrawSURFACE7_Blt() failed with %s\n", DXError (ddrval));
            break;
        }
#if 0
	else
	{
	    write_log( "Blt() failed - %s\n", DXError (ddrval));
	    result = 0;
	    break;
	}
#endif
    }
    if( ddrval == DD_OK )
    {
        result = 1;
    }
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */

int DirectDraw_Flip( int wait )
{
    int result = 0;
    HRESULT ddrval = DD_OK;
    DWORD flags = DDFLIP_WAIT;

    if( DirectDrawState.flipping == triple_buffer )
    {
	if (!currprefs.gfx_afullscreen && !currprefs.gfx_vsync) {
	    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags | DDFLIP_NOVSYNC);
	} else if (currprefs.gfx_vsync) {
	    if (currprefs.gfx_refreshrate <= 85) {
		ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags );
	    } else {
		if (flipinterval_supported) {
		    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags | DDFLIP_INTERVAL2 );
		} else {
		    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags );
		    result = DirectDraw_BltFast( tertiary_surface, 0, 0, primary_surface, NULL );
		    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags );
		}
	    }
	} else {
	    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags );
	}
    } else if( DirectDrawState.flipping == double_buffer ) {
	if (!currprefs.gfx_afullscreen && !currprefs.gfx_vsync)
	    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags | DDFLIP_NOVSYNC);
	else
	    ddrval = IDirectDrawSurface7_Flip( DirectDrawState.primary.surface, NULL, flags );
    } else {
	return 1;
    }
    if( ddrval == DD_OK )
        result = 1;
    else
	write_log("FLIP: DirectDrawSurface_Flip() failed with %s\n", DXError (ddrval));
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
int DirectDraw_Blt( surface_type_e dsttype, LPRECT dstrect,
                    surface_type_e srctype, LPRECT srcrect,
                    DWORD flags, LPDDBLTFX fx )
{
    int result;

    LPDIRECTDRAWSURFACE7 lpDDS4_dst, lpDDS4_src;
    
    if( dsttype == primary_surface )
    {
	if( DirectDrawState.isoverlay )
	    lpDDS4_dst = DirectDrawState.overlay.surface;
	else
	    lpDDS4_dst = DirectDrawState.primary.surface;
    }
    else if( dsttype == secondary_surface )
    {
        lpDDS4_dst = DirectDrawState.secondary.surface;
    }
    else if( dsttype == tertiary_surface )
    {
        lpDDS4_dst = DirectDrawState.tertiary.surface;
    }
    else
    {
	lpDDS4_dst = DirectDrawState.overlay.surface;
    }

    if( srctype == primary_surface )
    {
        lpDDS4_src = DirectDrawState.primary.surface;
    }
    else if( srctype == secondary_surface )
    {
        lpDDS4_src = DirectDrawState.secondary.surface;
    }
    else if( srctype == tertiary_surface )
    {
        lpDDS4_src = DirectDrawState.tertiary.surface;
    }
    else if( srctype == overlay_surface )
    {
	lpDDS4_src = DirectDrawState.overlay.surface;
    }
    else
    {
	lpDDS4_src = NULL; /* For using BltStub to do rect-fills */
    }
    result = DirectDraw_BltStub( lpDDS4_dst, dstrect, lpDDS4_src, srcrect, flags, fx );
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_WaitForVerticalBlank( DWORD flags )
{
    HRESULT result;
    result = IDirectDraw7_WaitForVerticalBlank( DirectDrawState.directdraw.dd, flags, NULL );
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_EnumDisplayModes( DWORD flags, LPDDENUMMODESCALLBACK2 callback )
{
    HRESULT result;
    result = IDirectDraw7_EnumDisplayModes( DirectDrawState.directdraw.dd, flags, NULL, NULL, callback );    
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_FlipToGDISurface( void )
{
    HRESULT result = DDERR_GENERIC;
    if( DirectDrawState.initialized )
    {
	result = IDirectDraw7_FlipToGDISurface( DirectDrawState.directdraw.dd );
    }
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_GetDC( HDC *hdc, surface_type_e surface )
{
    HRESULT result = ~DD_OK;
    if( surface == primary_surface )
        result = IDirectDrawSurface7_GetDC( DirectDrawState.primary.surface, hdc );
    else if (surface == overlay_surface)
        result = IDirectDrawSurface7_GetDC( DirectDrawState.overlay.surface, hdc );
    else if (surface == secondary_surface)
        result = IDirectDrawSurface7_GetDC( DirectDrawState.secondary.surface, hdc );
    return result;
}

/*
 * FUNCTION:
 *
 * PURPOSE:
 *
 * PARAMETERS:
 *
 * RETURNS:
 *
 * NOTES:
 *
 * HISTORY:
 *   1999.08.02  Brian King             Creation
 *
 */
HRESULT DirectDraw_ReleaseDC( HDC hdc, surface_type_e surface )
{
    HRESULT result;
    if( surface == primary_surface )
        result = IDirectDrawSurface7_ReleaseDC( DirectDrawState.primary.surface, hdc );
    else if (surface == overlay_surface)
        result = IDirectDrawSurface7_ReleaseDC( DirectDrawState.overlay.surface, hdc );
    else
        result = IDirectDrawSurface7_ReleaseDC( DirectDrawState.secondary.surface, hdc );
    return result;
}

extern int display_change_requested;

HRESULT DirectDraw_UpdateOverlay( RECT sr, RECT dr )
{
    HRESULT result = DD_OK;
    if (DirectDrawState.isoverlay && DirectDrawState.overlay.surface)
    {
	if ((drivercaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) && drivercaps.dwAlignBoundarySrc)
	    sr.left = (sr.left + drivercaps.dwAlignBoundarySrc / 2) & ~(drivercaps.dwAlignBoundarySrc - 1);
	if ((drivercaps.dwCaps & DDCAPS_ALIGNSIZESRC) && drivercaps.dwAlignSizeSrc)
	    sr.right = sr.left + (sr.right - sr.left + drivercaps.dwAlignSizeSrc / 2) & ~(drivercaps.dwAlignSizeSrc - 1);
	if ((drivercaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) && drivercaps.dwAlignBoundaryDest)
	    dr.left = (dr.left + drivercaps.dwAlignBoundaryDest / 2) & ~(drivercaps.dwAlignBoundaryDest - 1);    
	if ((drivercaps.dwCaps & DDCAPS_ALIGNSIZEDEST) && drivercaps.dwAlignSizeDest)
	    dr.right = dr.left + (dr.right - dr.left) & ~(drivercaps.dwAlignSizeDest - 1);
	result = IDirectDrawSurface7_UpdateOverlay( DirectDrawState.overlay.surface, &sr, DirectDrawState.primary.surface, &dr, overlayflags, &overlayfx);
	
    }
    if (result != DD_OK) {
	if (result == DDERR_SURFACELOST)
	    display_change_requested++;
	write_log ("UpdateOverlay failed %s\n", DXError (result));
    }
    return DD_OK;
}
