
#include <windows.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "custom.h"
#include "drawing.h"
#include "dxwrap.h"
#include "win32.h"
#include "win32gfx.h"
#include "filter.h"

#include <d3d9.h>
#include <d3dx9.h>

#include "direct3d.h"

static int tformat;
static int d3d_enabled, scanlines_ok;
static HINSTANCE d3dDLL;
static LPDIRECT3D9 d3d;
static D3DPRESENT_PARAMETERS dpp;
static LPDIRECT3DDEVICE9 d3ddev;
static D3DSURFACE_DESC dsdbb;
static LPDIRECT3DTEXTURE9 texture, sltexture;

static int twidth, theight, max_texture_w, max_texture_h;
static int tin_w, tin_h, window_h, window_w;
static int t_depth;
static int required_sl_texture_w, required_sl_texture_h;
static int vsync2, guimode;

static char *D3D_ErrorText (HRESULT error)
{
    return "";
}
static char *D3D_ErrorString (HRESULT dival)
{
    static char dierr[200];
    sprintf(dierr, "%08.8X S=%d F=%04.4X C=%04.4X (%d) (%s)",
	dival, (dival & 0x80000000) ? 1 : 0,
	HRESULT_FACILITY(dival),
	HRESULT_CODE(dival),
	HRESULT_CODE(dival),
	D3D_ErrorText (dival));
    return dierr;
}

void D3D_free (void)
{
    if (texture) {
	IDirect3DTexture9_Release (texture);
	texture = NULL;
    }
    if (sltexture) {
	IDirect3DTexture9_Release (sltexture);
	sltexture = NULL;
    }

    if (d3ddev) {
	IDirect3DDevice9_Release (d3ddev);
	d3ddev = NULL;
    }
    if (d3dDLL) {
	FreeLibrary (d3dDLL);
	d3dDLL = NULL;
    }
    d3d_enabled = 0;
}

static int restoredeviceobjects(void)
{
    // Store render target surface desc
    LPDIRECT3DSURFACE9 bb;
    HRESULT hr;
    D3DXMATRIX matrix;
    FLOAT aspect;
    int v;

    hr = IDirect3DDevice9_GetBackBuffer (d3ddev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (!SUCCEEDED (hr)) {
	write_log ("failed to create backbuffer: %s\n", D3D_ErrorString (hr));
	return 0;
    }
    hr = IDirect3DSurface9_GetDesc (bb, &dsdbb);
    hr = IDirect3DSurface9_Release (bb);

    // Set up the texture 
    hr = IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    hr = IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    hr = IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

    // Set miscellaneous render states
    hr = IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DITHERENABLE, TRUE);
    hr = IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ZENABLE, FALSE);

    // Set the projection matrix
    aspect = ((FLOAT)dsdbb.Width) / dsdbb.Height;
    D3DXMatrixPerspectiveFovLH (&matrix, D3DX_PI/4, aspect, 1.0f, 100.0f);
    hr = IDirect3DDevice9_SetTransform(d3ddev, D3DTS_PROJECTION, &matrix);

    // turn off lighting
    hr = IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_LIGHTING, FALSE);

    switch (currprefs.gfx_filter_filtermode & 1)
    {
	case 0:
	v = D3DTEXF_POINT;
	break;
	case 1:
	default:
	v = D3DTEXF_LINEAR;
	break;
    }
    hr = IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, v);
    hr = IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, v);
    return 1;
}

static int createtexture (int w, int h)
{
    HRESULT hr;
    UINT ww = w;
    UINT hh = h;
    D3DFORMAT format = tformat;

    hr = D3DXCheckTextureRequirements(d3ddev, &ww, &hh, NULL, 0, &format, D3DPOOL_MANAGED);
    if (FAILED (hr)) {
	write_log ("D3DXCheckTextureRequirements failed: %s\n", D3D_ErrorString (hr));
    }
    hr = D3DXCreateTexture(d3ddev, ww, hh, 0, 0, format, D3DPOOL_MANAGED, &texture);
    if (FAILED (hr)) {
	write_log ("D3DXCreateTexture failed: %s\n", D3D_ErrorString (hr));
    }
    twidth = ww;
    theight = hh;
    write_log ("D3D: %d*%d texture allocated, bits per pixel %d\n", ww, hh, t_depth);
    return 1;
}
static int createsltexture (void)
{
    HRESULT hr;
    UINT ww = required_sl_texture_w;
    UINT hh = required_sl_texture_h;
    D3DFORMAT format = D3DFMT_A4R4G4B4;

    hr = D3DXCheckTextureRequirements(d3ddev, &ww, &hh, NULL, 0, &format, D3DPOOL_MANAGED);
    if (FAILED (hr)) {
	write_log ("SL D3DXCheckTextureRequirements failed: %s\n", D3D_ErrorString (hr));
	return 0;
    }
    hr = D3DXCreateTexture(d3ddev, ww, hh, 0, 0, format, D3DPOOL_MANAGED, &sltexture);
    if (FAILED (hr)) {
	write_log ("SL D3DXCreateTexture failed: %s\n", D3D_ErrorString (hr));
	return 0;
    }
    required_sl_texture_w = ww;
    required_sl_texture_h = hh;
    write_log ("D3D: SL %d*%d texture allocated\n", ww, hh);

    scanlines_ok = 1;
    return 1;
}

static void createscanlines (int force)
{
    HRESULT hr;
    D3DLOCKED_RECT locked;
    static int osl1, osl2, osl3;
    int sl4, sl42;
    int l1, l2;
    int x, y, yy;
    uae_u8 *sld, *p;

    if (!scanlines_ok)
	return;
    if (osl1 == currprefs.gfx_filter_scanlines && osl3 == currprefs.gfx_filter_scanlinelevel && osl2 == currprefs.gfx_filter_scanlineratio && !force)
	return;
    osl1 = currprefs.gfx_filter_scanlines;
    osl3 = currprefs.gfx_filter_scanlinelevel;
    osl2 = currprefs.gfx_filter_scanlineratio;
    sl4 = currprefs.gfx_filter_scanlines * 16 / 100;
    sl42 = currprefs.gfx_filter_scanlinelevel * 16 / 100;
    if (sl4 > 15) sl4 = 15;
    if (sl42 > 15) sl42 = 15;
    l1 = currprefs.gfx_filter_scanlineratio & 15;
    l2 = currprefs.gfx_filter_scanlineratio >> 4;

    hr = IDirect3DTexture9_LockRect(sltexture, 0, &locked, NULL, D3DLOCK_DISCARD);
    if (FAILED (hr)) {
        write_log ("SL IDirect3DTexture9_LockRect failed: %s\n", D3D_ErrorString (hr));
	return;
    }
    sld = (uae_u8*)locked.pBits;
    for (y = 0; y < window_h; y++)
	memset (sld + y * locked.Pitch, 0, window_w * 2);
    for (y = 1; y < window_h; y += l1 + l2) {
	for (yy = 0; yy < l2 && y + yy < window_h; yy++) {
	    for (x = 0; x < window_w; x++) {
	        /* 16-bit, A4R4G4B4 */
	        uae_u8 sll = sl42;
    	        p = &sld[(y + yy) * locked.Pitch + (x * 2)];
	        p[1] = (sl4 << 4) | (sll << 0);
	        p[0] = (sll << 4) | (sll << 0);
	    }
	}
    }
    IDirect3DTexture9_UnlockRect (sltexture, 0);
    if (scanlines_ok) {
	/* enable alpha blending for scanlines */
	IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    } else {
	IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);
    }
}


const char *D3D_init (HWND ahwnd, int w_w, int w_h, int t_w, int t_h, int depth)
{
    HRESULT ret;
    static char errmsg[100] = { 0 };
    LPDIRECT3D9 (WINAPI *D3DCreate)(UINT);
    D3DDISPLAYMODE mode;
    D3DCAPS9 d3dCaps;

    d3d_enabled = 0;
    scanlines_ok = 0;
    if (currprefs.gfx_filter != UAE_FILTER_DIRECT3D) {
	strcpy (errmsg, "D3D: not enabled");
	return errmsg;
    }

    d3dDLL = LoadLibrary("D3D9.DLL");
    if (d3dDLL == NULL) {
        strcpy (errmsg, "Direct3D: DirectX 9 or newer required");
        return errmsg;
    }
    D3DCreate = (LPDIRECT3D9 (WINAPI *)(UINT))
    GetProcAddress(d3dDLL, "Direct3DCreate9");
    if(D3DCreate == NULL) {
	D3D_free ();
	strcpy (errmsg, "Direct3D: DirectX 9 or newer required");
	return errmsg;
    }
    d3d = D3DCreate(D3D_SDK_VERSION);
    if (d3d == NULL) {
	D3D_free ();
	strcpy (errmsg, "Direct3D: failed to create D3D object");
	return errmsg;
    }

    IDirect3D9_GetAdapterDisplayMode(d3d, D3DADAPTER_DEFAULT, &mode);
    IDirect3D9_GetDeviceCaps(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps);

    memset (&dpp, 0, sizeof (dpp));
    dpp.Windowed = currprefs.gfx_afullscreen ? FALSE : TRUE;
    dpp.BackBufferFormat = mode.Format;
    dpp.BackBufferCount = 1;
    dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    dpp.BackBufferWidth = w_w;
    dpp.BackBufferHeight = w_h;
    dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    vsync2 = 0;
    if (currprefs.gfx_afullscreen) {
	dpp.FullScreen_RefreshRateInHz = currprefs.gfx_refreshrate;
	if (currprefs.gfx_vsync > 0) {
	    dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	    if (currprefs.gfx_vsync > 85) {
		if (d3dCaps.PresentationIntervals & D3DPRESENT_INTERVAL_TWO)
		    dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
		else
		    vsync2 = 1;
	    }
	}    
    }
  
    ret = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ahwnd,
	D3DCREATE_SOFTWARE_VERTEXPROCESSING, &dpp, &d3ddev);
    if(FAILED(ret)) {
	sprintf (errmsg, "CreateDevice failed, %s\n", D3D_ErrorString (ret));
	D3D_free ();
	return errmsg;
    }

    max_texture_w = d3dCaps.MaxTextureWidth;
    max_texture_h = d3dCaps.MaxTextureHeight;

    write_log("D3D: max texture width: %d, max texture height: %d\n",
	max_texture_w, max_texture_h);

    if (max_texture_w < t_w || max_texture_h < t_h) {
	sprintf (errmsg, "Direct3D: %d * %d or bigger texture support required\nYour card's maximum texture size is only %d * %d",
	    t_w, t_h, max_texture_w, max_texture_h);
	return errmsg;
    }

    required_sl_texture_w = w_w;
    required_sl_texture_h = w_h;
    if (currprefs.gfx_filter_scanlines > 0 && (max_texture_w < w_w || max_texture_h < w_h)) {
	gui_message ("Direct3D: %d * %d or bigger texture support required for scanlines (max is only %d * %d)\n"
	    "Scanlines disabled.",
	    required_sl_texture_w, required_sl_texture_h, max_texture_w, max_texture_h);
	changed_prefs.gfx_filter_scanlines = currprefs.gfx_filter_scanlines = 0;
    }

    t_depth = depth;
    switch (depth)
    {
	case 32:
	if (currprefs.gfx_filter_scanlines)
	    tformat = D3DFMT_A8R8G8B8;
	else
	    tformat = D3DFMT_X8R8G8B8;
	break;
	case 15:
	case 16:
	if (currprefs.gfx_filter_scanlines)
	    tformat = D3DFMT_A1R5G5B5;
	else
	    tformat = D3DFMT_X1R5G5B5;
	break;
    }
    restoredeviceobjects ();
    window_w = w_w;
    window_h = w_h;
    tin_w = t_w;
    tin_h = t_h;
    createtexture (t_w, t_h);
    if (currprefs.gfx_filter_scanlines > 0)
	createsltexture ();
    createscanlines (1);
    d3d_enabled = 1;
    return 0;
}

#define D3DFVF_TLVERTEX D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1

typedef struct _D3DTLVERTEX {
    float sx; /* Screen coordinates */
    float sy;
    float sz;
    float rhw; /* Reciprocal of homogeneous w */
    D3DCOLOR color; /* Vertex color */
    float tu; /* Texture coordinates */
    float tv;
} D3DTLVERTEX, *LPD3DTLVERTEX;

static void BlitRect(LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 src,
		float left, float top, float right, float bottom, D3DCOLOR col,float z)
{
    int i;
    HRESULT hr;
    D3DTLVERTEX verts[4];
    float rhw = 1.0f / (z * 990.0f + 10.0f);

    for (i = 0; i < 4; i++) {
	verts[i].rhw = rhw;
	verts[i].color = col;
    }
    verts[0].tu = 0.0f; verts[0].tv = 0.0f;
    verts[0].sx = left - 0.5f; verts[0].sy = top - 0.5f; verts[0].sz = z;
    verts[1].tu = 1.0f; verts[1].tv = 0.0f;
    verts[1].sx = right - 0.5f; verts[1].sy = top - 0.5f; verts[1].sz = z;
    verts[2].tu = 1.0f; verts[2].tv = 1.0f;
    verts[2].sx = right - 0.5f; verts[2].sy = bottom - 0.5f; verts[2].sz = z;
    verts[3].tu = 0.0f; verts[3].tv = 1.0f;
    verts[3].sx = left - 0.5f; verts[3].sy = bottom - 0.5f; verts[3].sz = z;

    // set the texture
    hr = IDirect3DDevice9_SetTexture(dev, 0, src);
    if (FAILED (hr))
        write_log ("IDirect3DDevice9_SetTexture failed: %s\n", D3D_ErrorString (hr));

    hr = IDirect3DDevice9_SetVertexShader(dev, NULL);
    if (FAILED (hr))
        write_log ("IDirect3DDevice9_SetVertexShader failed: %s\n", D3D_ErrorString (hr));
    // configure shader for vertex type
    hr = IDirect3DDevice9_SetFVF(dev, D3DFVF_TLVERTEX);
    if (FAILED (hr))
        write_log ("IDirect3DDevice9_SetFVF failed: %s\n", D3D_ErrorString (hr));

    // draw the rectangle
    hr = IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTLVERTEX));
    if (FAILED (hr))
        write_log ("IDirect3DDevice9_DrawPrimitiveUP failed: %s\n", D3D_ErrorString (hr));
}

static void calc (float *xp, float *yp, float *sxp, float *syp)
{
    int xm, ym;
    int fx, fy;
    float x, y, sx, sy;

    xm = currprefs.gfx_lores ? 2 : 1;
    ym = currprefs.gfx_linedbl ? 1 : 2;
    if (window_w >= 1024)
	xm *= 2;
    if (window_h >= 960)
	ym *= 2;
    fx = (tin_w * xm - window_w) / 2;
    fy = (tin_h * ym - window_h) / 2;
    x = (float)(window_w * currprefs.gfx_filter_horiz_offset / 100.0);
    y = (float)(window_h * currprefs.gfx_filter_vert_offset / 100.0);
    sx = x + (float)(twidth * window_w / tin_w) * ((currprefs.gfx_filter_horiz_zoom + 100) / 100.0);
    sy = y + (float)(theight * window_h / tin_h) * ((currprefs.gfx_filter_vert_zoom + 100) / 100.0);
    x -= fx; y -= fy;
    sx += 2 * fx; sy += 2 * fy;
    *xp = x; *yp = y;
    *sxp = sx; *syp = sy;
}

void D3D_unlocktexture (void)
{
    float x, y, sx, sy;
    IDirect3DTexture9_UnlockRect (texture, 0);
    calc (&x, &y, &sx, &sy);
    BlitRect (d3ddev, texture, x, y, sx, sy, 0xffffff, 0.1f);
    if (scanlines_ok)
	BlitRect (d3ddev, sltexture, 0, 0, required_sl_texture_w, required_sl_texture_h, 0xffffff, 0.2f);
    IDirect3DDevice9_EndScene(d3ddev);
    IDirect3DDevice9_Present (d3ddev, 0, 0, 0 ,0);
    if (vsync2)
	D3D_render ();
}

int D3D_locktexture (void)
{
    D3DLOCKED_RECT locked;
    HRESULT hr;

    if (FAILED(IDirect3DDevice9_TestCooperativeLevel(d3ddev)))
	return 0;
    IDirect3DDevice9_Clear(d3ddev, 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );

    hr = IDirect3DDevice9_BeginScene(d3ddev);
    if (FAILED (hr)) {
	write_log ("IDirect3DDevice9_BeginScene failed: %s\n", D3D_ErrorString (hr));
	return 0;
    }
    hr = IDirect3DTexture9_LockRect(texture, 0, &locked, NULL, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK);
    if (FAILED (hr)) {
        write_log ("IDirect3DTexture9_LockRect failed: %s\n", D3D_ErrorString (hr));
	D3D_unlocktexture ();
	return 0;
    }
    gfxvidinfo.bufmem = locked.pBits;
    gfxvidinfo.rowbytes = locked.Pitch;
    init_row_map ();
    return 1;
}

void D3D_render (void)
{
    float x, y, sx, sy;
    HRESULT hr;

    if (!d3d_enabled)
	return;
    if (FAILED(IDirect3DDevice9_TestCooperativeLevel(d3ddev)))
	return;
    IDirect3DDevice9_Clear(d3ddev, 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );
    hr = IDirect3DDevice9_BeginScene(d3ddev);
    if (FAILED (hr))
	return;
    calc (&x, &y, &sx, &sy);
    BlitRect(d3ddev, texture, x, y, sx, sy, 0xffffff, 0.1f);
    if (scanlines_ok)
	BlitRect (d3ddev, sltexture, 0, 0, required_sl_texture_w, required_sl_texture_h, 0xffffff, 0.2f);
    IDirect3DDevice9_EndScene(d3ddev);
    IDirect3DDevice9_Present (d3ddev, 0, 0, 0 ,0);
}

void D3D_refresh (void)
{
    if (!d3d_enabled)
	return;
    createscanlines (1);
    D3D_render ();
}

void D3D_getpixelformat (int depth,int *rb, int *gb, int *bb, int *rs, int *gs, int *bs, int *ab, int *as, int *a)
{
    switch (depth)
    {
	case 32:
        *rb = 8;
	*gb = 8;
	*bb = 8;
	*ab = 8;
	*rs = 16;
	*gs = 8;
	*bs = 0;
	*as = 24;
	*a = 255;
	break;
	case 15:
	case 16:
        *rb = 5;
	*gb = 5;
	*bb = 5;
	*ab = 1;
	*rs = 10;
	*gs = 5;
	*bs = 0;
	*as = 15;
	*a = 1;
	break;
    }
}

void D3D_guimode (int guion)
{
    if (!d3d_enabled)
	return;
    IDirect3DDevice9_SetDialogBoxMode (d3ddev, guion);
    guimode = guion;
}

HDC D3D_getDC(HDC hdc)
{
    static LPDIRECT3DSURFACE9 bb;
    HRESULT hr;

    if (!d3d_enabled)
	return 0;
    if (!hdc) {
        hr = IDirect3DDevice9_GetBackBuffer (d3ddev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
	if (!SUCCEEDED (hr)) {
	    write_log ("failed to create backbuffer: %s\n", D3D_ErrorString (hr));
	    return 0;
	}
	if (IDirect3DSurface9_GetDC (bb, &hdc) == D3D_OK)
	    return hdc;
	return 0;
    }
    IDirect3DSurface9_ReleaseDC (bb, hdc);
    IDirect3DSurface9_Release (bb);
    return 0;
}

int D3D_isenabled (void)
{
    return d3d_enabled;
}






