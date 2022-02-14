/* 
 * UAE - The Un*x Amiga Emulator
 *
 * OpenGL renderer
 *
 * Copyright 2002 Toni Wilen
 */

#include <windows.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "dxwrap.h"
#include "opengl.h"
#include "custom.h"
#include "win32.h"
#include "win32gfx.h"
#include "filter.h"

#ifdef OPENGL

#include <gl\gl.h>
#include <gl\glu.h>

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);

/* not defined in MSVC's opengl.h */
#ifndef GL_UNSIGNED_SHORT_5_5_5_1_EXT
#define GL_UNSIGNED_SHORT_5_5_5_1_EXT       0x8034
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4_EXT
#define GL_UNSIGNED_SHORT_4_4_4_4_EXT       0x8033
#endif

static GLint max_texture_size;
static GLint tex[4];
static GLint scanlinetex;
static int total_textures;
static int required_texture_size;
static int required_sl_texture_size;
static GLint ti2d_internalformat, ti2d_format, ti2d_type;
static GLint sl_ti2d_internalformat, sl_ti2d_format, sl_ti2d_type;
static int w_width, w_height, t_width, t_height;
static int packed_pixels;
static int doublevsync;
static int ogl_enabled;

static HDC openglhdc;
static HGLRC hrc;
static HWND hwnd;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
static PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT;

static PIXELFORMATDESCRIPTOR pfd;

static void testerror (char *s)
{
    for (;;) {
	GLint err = glGetError();
	if (err == 0)
	    return;
	write_log ("OpenGL error %d (%s)\n", err, s);
    }
}

static int exact_log2 (int v)
{
    int l = 0;
    while ((v >>= 1) != 0)
	l++;
    return l;
}

const char *OGL_init (HWND ahwnd, int w_w, int w_h, int t_w, int t_h, int depth)
{
    int PixelFormat;
    const char *ext1;
    static char errmsg[100] = { 0 };
    static int init;

    ogl_enabled = 0;
    if (currprefs.gfx_filter != UAE_FILTER_OPENGL) {
	strcpy (errmsg, "OPENGL: not enabled");
	return errmsg;
    }

    w_width = w_w;
    w_height = w_h;
    t_width = t_w;
    t_height = t_h;

    memset (&pfd, 0, sizeof (pfd));
    pfd.nSize = sizeof (PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = depth;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    hwnd = ahwnd;
    openglhdc = GetDC (hwnd);
    total_textures = 2;

    if (currprefs.gfx_afullscreen && WIN32GFX_GetDepth (TRUE) < 15) {
	strcpy (errmsg, "OPENGL: display depth must be at least 15 bit");
	return errmsg;
    }

    if (!(PixelFormat = ChoosePixelFormat (openglhdc, &pfd))) {
        strcpy (errmsg, "OPENGL: can't find suitable pixelformat");
	return errmsg;
    }
    
    if (!SetPixelFormat (openglhdc, PixelFormat, &pfd)) {
        strcpy (errmsg, "OPENGL: can't set pixelformat");
        return errmsg;
    }
    
    if (!(hrc = wglCreateContext (openglhdc))) {
        strcpy (errmsg, "OPENGL: can't create gl rendering context");
        return errmsg;
    }
    
    if (!wglMakeCurrent (openglhdc, hrc)) {
        strcpy (errmsg, "OPENGL: can't activate gl rendering context");
        return errmsg;
    }
    
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
    required_texture_size = 2 << exact_log2 (t_width > t_height ? t_width : t_height);
    if (max_texture_size < t_width || max_texture_size < t_height) {
	sprintf (errmsg, "OPENGL: %d * %d or bigger texture support required\nYour card's maximum texture size is only %d * %d",
	    required_texture_size, required_texture_size, max_texture_size, max_texture_size);
	return errmsg;
    }
    required_sl_texture_size = 2 << exact_log2 (w_width > w_height ? w_width : w_height);
    if (currprefs.gfx_filter_scanlines > 0 && (max_texture_size < w_width || max_texture_size < w_height)) {
	gui_message ("OPENGL: %d * %d or bigger texture support required for scanlines (max is only %d * %d)\n"
	    "Scanlines disabled.",
	    required_sl_texture_size, required_sl_texture_size, max_texture_size, max_texture_size);
	changed_prefs.gfx_filter_scanlines = currprefs.gfx_filter_scanlines = 0;
    }

    ext1 = glGetString (GL_EXTENSIONS);
    if (!init)
	write_log("OpenGL extensions: %s\n", ext1);
    if (strstr (ext1, "EXT_packed_pixels"))
	packed_pixels = 1;
    if (strstr (ext1, "WGL_EXT_swap_control")) {
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	if (!wglGetSwapIntervalEXT || !wglSwapIntervalEXT) {
	    write_log ("OPENGL: WGL_EXT_swap_control extension found but no wglGetSwapIntervalEXT or wglSwapIntervalEXT found!?\n");
	    wglSwapIntervalEXT = 0;
	    wglGetSwapIntervalEXT = 0;
	}

    }

    sl_ti2d_internalformat = GL_RGBA4;
    sl_ti2d_format = GL_RGBA;
    sl_ti2d_type = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
    ti2d_type = -1;
    if (depth == 15 || depth == 16) {
	if (!packed_pixels) {
	    gui_message (
		"OPENGL: can't use 15/16 bit screen depths because EXT_packed_pixels extension was not found.\n"
		"Falling back to 32-bit mode");
	    depth = 32;
	}   
	ti2d_internalformat = GL_RGB5_A1;
        ti2d_format = GL_RGBA;
        ti2d_type = GL_UNSIGNED_SHORT_5_5_5_1_EXT;
    }
    if (depth == 32) {
	ti2d_internalformat = GL_RGBA;
	ti2d_format = GL_RGBA;
	ti2d_type = GL_UNSIGNED_BYTE;
	if (!packed_pixels) {
	    sl_ti2d_internalformat = GL_RGBA;
	    sl_ti2d_format = GL_RGBA;
	    sl_ti2d_type = GL_UNSIGNED_BYTE;
	}
    }
    if (ti2d_type < 0) {
    	sprintf (errmsg, "OPENGL: Only 15, 16 or 32 bit screen depths supported (was %d)", depth);
	return errmsg;
    }
    
    glGenTextures (total_textures, tex);

    /* "bitplane" texture */
    glBindTexture (GL_TEXTURE_2D, tex [0]);
    glTexImage2D (GL_TEXTURE_2D, 0, ti2d_internalformat,
	required_texture_size, required_texture_size,0,  ti2d_format, ti2d_type, 0);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glClearColor (0.0, 0.0, 0.0, 0.0);
    glShadeModel (GL_FLAT); 
    glDisable (GL_DEPTH_TEST);
    glEnable (GL_TEXTURE_2D);
    glDisable (GL_LIGHTING);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ogl_enabled = 1;
    OGL_resize (w_width, w_height);
    OGL_refresh ();
    init = 1;

    write_log("OPENGL: using texture depth %d texture size %d * %d scanline texture size %d * %d\n",
	depth, required_texture_size, required_texture_size, required_sl_texture_size, required_sl_texture_size);
    return 0;
}

static void createscanlines (int force)
{
    int x, y, yy;
    uae_u8 *sld, *p;
    int sl4, sl8, sl42, sl82;
    int l1, l2;
    static int osl1, osl2, osl3;

    if (osl1 == currprefs.gfx_filter_scanlines && osl3 == currprefs.gfx_filter_scanlinelevel && osl2 == currprefs.gfx_filter_scanlineratio && !force)
	return;
    osl1 = currprefs.gfx_filter_scanlines;
    osl3 = currprefs.gfx_filter_scanlinelevel;
    osl2 = currprefs.gfx_filter_scanlineratio;
    if (!currprefs.gfx_filter_scanlines) {
        glDisable (GL_BLEND);
	return;
    }   

    glEnable (GL_BLEND);
    scanlinetex = tex[total_textures - 1];
    glBindTexture (GL_TEXTURE_2D, scanlinetex);
    glTexImage2D (GL_TEXTURE_2D, 0, sl_ti2d_internalformat,
        required_sl_texture_size, required_sl_texture_size, 0, sl_ti2d_format, sl_ti2d_type, 0);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    sl4 = currprefs.gfx_filter_scanlines * 16 / 100;
    sl8 = currprefs.gfx_filter_scanlines * 256 / 100;
    sl42 = currprefs.gfx_filter_scanlinelevel * 16 / 100;
    sl82 = currprefs.gfx_filter_scanlinelevel * 256 / 100;
    if (sl4 > 15) sl4 = 15;
    if (sl8 > 255) sl8 = 255;
    if (sl42 > 15) sl42 = 15;
    if (sl82 > 255) sl82 = 255;
    sld = malloc (w_width * w_height * 4);
    memset (sld, 0, w_width * w_height * 4);
    l1 = currprefs.gfx_filter_scanlineratio & 15;
    l2 = currprefs.gfx_filter_scanlineratio >> 4;
    if (!l1) l1 = 1;
    if (!l2) l2 = 1;
    for (y = 1; y < w_height; y += l1 + l2) {
	for (yy = 0; yy < l2 && y + yy < w_height; yy++) {
	    for (x = 0; x < w_width; x++) {
		if (packed_pixels) {
		    /* 16-bit, R4G4B4A4 */
		    uae_u8 sll = sl42;
    		    p = &sld[((y + yy) * w_width + x) * 2];
		    p[0] = sl4 | (sll << 4);
		    p[1] = (sll << 4) | (sll << 0);
		} else {
		    /* 32-bit, R8G8B8A8 */
    		    p = &sld[((y + yy) * w_width + x) * 4];
		    p[0] = p[1] = p[2] = sl82;
		    p[3] = sl8;
		}
	    }
	}
    }
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, w_width, w_height, sl_ti2d_format, sl_ti2d_type, sld);
    free (sld);
}

static void setfilter (void)
{
    int filtering;
    switch (currprefs.gfx_filter_filtermode & 1)
    {
	case 0:
	filtering = GL_NEAREST;
	break;
	case 1:
	default:
	filtering = GL_LINEAR;
	break;
    }
    if (currprefs.gfx_filter_scanlines > 0) {
        glBindTexture (GL_TEXTURE_2D, scanlinetex);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    }
    glBindTexture (GL_TEXTURE_2D, tex[0]);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
}

static void OGL_swapinterval (void)
{
    doublevsync = 0;
    if (wglSwapIntervalEXT) {
	int i1, i2;
	i1 = (currprefs.gfx_vsync > 0 && currprefs.gfx_afullscreen) ? (currprefs.gfx_refreshrate > 85 ? 2 : 1) : 0;
	if (turbo_emulation) i1 = 0;
	wglSwapIntervalEXT (i1);
	i2 = wglGetSwapIntervalEXT ();
	if (i1 == 2 && i2 < i1) /* did display driver support SwapInterval == 2 ? */
	    doublevsync = 1; /* nope, we must wait for vblank twice */
    }
}

void OGL_resize (int width, int height)
{
    if (!ogl_enabled)
	return;

    w_width = width;
    w_height = height;
    glViewport (0, 0, w_width, w_height);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0.0f, w_width, w_height, 0, -1.0f, 1.0f);
    createscanlines (1);
    setfilter ();
    OGL_swapinterval ();
}

static void OGL_dorender (int newtex)
{
    float x1, y1, x2, y2;
    uae_u8 *data = gfxvidinfo.bufmem;
    int fx, fy, xm, ym;

    xm = currprefs.gfx_lores ? 2 : 1;
    ym = currprefs.gfx_linedbl ? 1 : 2;
    if (w_width >= 1024)
	xm *= 2;
    if (w_height >= 960)
	ym *= 2;
    fx = (t_width * xm - w_width) / 2;
    fy = (t_height * ym - w_height) / 2;

    glClear (GL_COLOR_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    x1 = (float)(w_width * currprefs.gfx_filter_horiz_offset / 100.0);
    y1 = (float)(w_height * currprefs.gfx_filter_vert_offset / 100.0);
    x2 = x1 + (float)((required_texture_size * w_width / t_width) * (currprefs.gfx_filter_horiz_zoom + 100) / 100.0);
    y2 = y1 + (float)((required_texture_size * w_height / t_height) * (currprefs.gfx_filter_vert_zoom + 100)/ 100.0);
    x1 -= fx; y1 -= fy;
    x2 += 2 * fx; y2 += 2 * fy;

    glBindTexture (GL_TEXTURE_2D, tex[0]);
    if (newtex)
	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, t_width, t_height, ti2d_format, ti2d_type, data);

    glBegin (GL_QUADS);
    glTexCoord2f (0, -1.0f); glVertex2f (x1, y1);
    glTexCoord2f (0, 0); glVertex2f (x1, y2);
    glTexCoord2f (1.0f, 0); glVertex2f (x2, y2);
    glTexCoord2f (1.0f, -1.0f); glVertex2f (x2, y1);
    glEnd();

    if (currprefs.gfx_filter_scanlines > 0) {
	float v = (float)required_sl_texture_size;
	glBindTexture (GL_TEXTURE_2D, scanlinetex);
	glBegin (GL_QUADS);
        glTexCoord2f (0, -1.0f); glVertex2f (0, 0);
	glTexCoord2f (0, 0); glVertex2f (0, v);
	glTexCoord2f (1.0f, 0); glVertex2f (v, v);
	glTexCoord2f (1.0f, -1.0f); glVertex2f (v, 0);
	glEnd();
    }
}

void OGL_render (void)
{
    if (!ogl_enabled)
	return;

    OGL_dorender (1);
    SwapBuffers (openglhdc);
    if (doublevsync) {
	OGL_dorender (0);
	SwapBuffers (openglhdc);
    }
}

void OGL_refresh (void)
{
    if (!ogl_enabled)
	return;

    createscanlines (0);
    setfilter ();
    OGL_swapinterval ();
    OGL_render ();
 }

void OGL_getpixelformat (int depth,int *rb, int *gb, int *bb, int *rs, int *gs, int *bs, int *ab, int *as, int *a)
{
    switch (depth)
    {
	case 32:
        *rb = 8;
	*gb = 8;
	*bb = 8;
	*ab = 8;
	*rs = 0;
	*gs = 8;
	*bs = 16;
	*as = 24;
	*a = 255;
	break;
	case 15:
	case 16:
        *rb = 5;
	*gb = 5;
	*bb = 5;
	*ab = 1;
	*rs = 11;
	*gs = 6;
	*bs = 1;
	*as = 0;
	*a = 1;
	break;
    }
}

void OGL_free (void)
{
    if (hrc) {
        wglMakeCurrent (NULL, NULL);
        wglDeleteContext (hrc);
        hrc = 0;
    }
    if (openglhdc) {
	ReleaseDC (hwnd, openglhdc);
	openglhdc = 0;
    }
    ogl_enabled = 0;
}

HDC OGL_getDC (HDC hdc)
{
    return openglhdc;
}

int OGL_isenabled (void)
{
    return ogl_enabled;
}

#endif
