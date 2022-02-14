
#include <windows.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "dxwrap.h"
#include "win32.h"
#include "win32gfx.h"
#include "filter.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern void AdMame2x(u8 *srcPtr, u32 srcPitch, /* u8 deltaPtr, */
              u8 *dstPtr, u32 dstPitch, int width, int height);
extern void AdMame2x32(u8 *srcPtr, u32 srcPitch, /* u8 deltaPtr, */
                u8 *dstPtr, u32 dstPitch, int width, int height);

static int dst_width, dst_height, amiga_width, amiga_height, depth, scale;

void S2X_configure (int rb, int gb, int bb, int rs, int gs, int bs)
{
    Init_2xSaI (rb, gb, bb, rs, gs, bs);
}

void S2X_init (int dw, int dh, int aw, int ah, int d, int mult)
{
    dst_width = dw;
    dst_height = dh;
    amiga_width = aw;
    amiga_height = ah;
    depth = d;
    scale = mult;
}

void S2X_render (void)
{
    int aw = amiga_width, ah = amiga_height, v, pitch;
    int f = currprefs.gfx_filter;
    uae_u8 *dptr, *sptr;

    sptr = gfxvidinfo.bufmem;

    if (depth > 16 && (f == UAE_FILTER_SUPEREAGLE || f == UAE_FILTER_SUPER2XSAI || f == UAE_FILTER_2XSAI)) {
	f = UAE_FILTER_NULL;
	changed_prefs.gfx_filter = f;
	return;
    }

    v = currprefs.gfx_filter_horiz_offset;
    v += (dst_width / scale - amiga_width) / 8;
    sptr += -v * (depth / 8) * 4;
    aw -= -v * 4;

    v = currprefs.gfx_filter_vert_offset;
    v += (dst_height / scale - amiga_height) / 8;
    sptr += -v * gfxvidinfo.rowbytes * 4;
    ah -= -v * 4;

    if (ah < 16)
	return;
    if (aw < 16)
	return;

    if (!DirectDraw_SurfaceLock (lockable_surface))
    	return;

    dptr = DirectDraw_GetSurfacePointer ();
    pitch = DirectDraw_GetSurfacePitch();

    if (aw * scale > dst_width)
        aw = (dst_width / scale) & ~(scale - 1);
    if (ah * scale > dst_height)
        ah = (dst_height / scale) & ~(scale - 1);

    if (f == UAE_FILTER_SCALE2X ) {

	if (scale == 2) {
	    if (depth == 16)
		AdMame2x (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    else if (depth == 32)
		AdMame2x32 (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	}

    } else if (f == UAE_FILTER_SUPEREAGLE) {

	if (scale == 2) {
	    if (depth == 16)
		SuperEagle (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	}

    } else if (f == UAE_FILTER_SUPER2XSAI) {

	if (scale == 2) {
	    if (depth == 16)
		Super2xSaI (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	}

    } else if (f == UAE_FILTER_2XSAI) {

	if (scale == 2) {
	    if (depth == 16)
		_2xSaI (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	}

    } else { /* null */

	int y;
        for (y = 0; y < dst_height; y++) {
	    memcpy (dptr, sptr, dst_width * depth / 8);
	    sptr += gfxvidinfo.rowbytes;
	    dptr += pitch;
	}

    }

    DirectDraw_SurfaceUnlock ();

}

void S2X_refresh (void)
{
    int y, pitch;
    uae_u8 *dptr;

    if (!DirectDraw_SurfaceLock (lockable_surface))
    	return;
    dptr = DirectDraw_GetSurfacePointer ();
    pitch = DirectDraw_GetSurfacePitch();
    for (y = 0; y < dst_height; y++)
	memset (dptr + y * pitch, 0, dst_width * depth / 8);
    DirectDraw_SurfaceUnlock ();
    S2X_render ();
}
