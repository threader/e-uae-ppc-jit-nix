 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Amiga interface
  *
  * Copyright 1996,1997,1998 Samuel Devulder.
  * Copyright 2003-2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

/* sam: Argg!! Why did phase5 change the path to cybergraphics ? */
//#define CGX_CGX_H <cybergraphics/cybergraphics.h>

#ifndef __AROS__
/*
 * Don't use CGX on AROS yet!
 */
#ifdef HAVE_LIBRARIES_CYBERGRAPHICS_H
# define CGX_CGX_H <libraries/cybergraphics.h>
# define USE_CYBERGFX           /* define this to have cybergraphics support */
#else
# ifdef HAVE_CYBERGRAPHX_CYBERGRAPHICS_H
#  define USE_CYBERGFX
#  define CGX_CGX_H <cybergraphx/cybergraphics.h>
# endif
#endif

#endif /* !__AROS__ */

/****************************************************************************/

#include <exec/execbase.h>
#include <exec/memory.h>

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <graphics/gfxbase.h>
#include <graphics/displayinfo.h>

#include <libraries/asl.h>
#include <intuition/pointerclass.h>

/****************************************************************************/

#if defined(POWERUP) /* holger jakob */
# include <powerup/ppclib/interface.h>
# include <powerup/ppclib/object.h>
# include <powerup/clib/ppc_protos.h>

/* inlines are too unstable at the moment */
# ifndef USE_CLIB
#  include <powerup/ppcproto/intuition.h>
#  include <powerup/ppcproto/graphics.h> /* sam: beware I had to rebuild inlines to have ppcproto/graphics.h or else I only gor ppcinlines/graphics.h from phase5 package */
#  include <powerup/ppcproto/exec.h>
#  include <powerup/ppcproto/asl.h>
# else
/* These includes are needed instead */
#  include <clib/exec_protos.h>
#  include <clib/asl_protos.h>
#  include <clib/intuition_protos.h>
#  include <graphics/scale.h>
#  include <clib/graphics_protos.h>
#  define ObtainBestPen(a0, a1, a2, a3, tags...) \
	({ULONG _tags[] = { tags }; ObtainBestPenA((a0), (a1), (a2), (a3), (struct TagItem *)_tags);})
#  define OpenScreenTags(a0, tags...) \
	({ULONG _tags[] = { tags }; OpenScreenTagList((a0), (struct TagItem *)_tags);})
#  define OpenWindowTags(a0, tags...) \
	({ULONG _tags[] = { tags }; OpenWindowTagList((a0), (struct TagItem *)_tags);})
#  define AslRequestTags(a0, tags...) \
	({ULONG _tags[] = { tags }; AslRequest((a0), (struct TagItem *)_tags);})
#  define NewObject(a0, a1, tags...) \
	({ULONG _tags[] = { tags }; NewObjectA((a0), (a1), (struct TagItem *)_tags);})
# endif

# undef  AllocVec
# undef  FreeVec
# define AllocVec PPCAllocVec
# define FreeVec  PPCFreeVec

/*extern struct ExecBase *SysBase;*/
struct	GfxBase	*GfxBase=NULL;
struct	IntuitionBase	*IntuitionBase=NULL;
struct	Library	*CyberGfxBase=NULL;

# include <powerup/ppclib/interface.h>
/* Sam: this will prevent spilled register problem */
static void myBltBitMapRastPort(struct BitMap * srcBitMap, long xSrc,
				long ySrc, struct RastPort * destRP,
				long xDest, long yDest, long xSize,
				long ySize, unsigned long minterm);
static void myWritePixelLine8(struct RastPort*, int, int, int, char *,
			      struct RastPort*);
static void myWritePixelArray8(struct RastPort*, int, int, int, int,
			       char *, struct RastPort*);

/****************************************************************************/

#else /* amigaos */
# ifdef __amigaos4__
#  define __USE_BASETYPE__
# endif
# include <proto/intuition.h>
# include <proto/graphics.h>
# include <proto/layers.h>
# include <proto/exec.h>
# include <proto/dos.h>
# include <proto/asl.h>
# define myBltBitMapRastPort BltBitMapRastPort
# define myWritePixelLine8   WritePixelLine8
# define myWritePixelArray8  WritePixelArray8
#endif

#ifdef USE_CYBERGFX
# ifdef __SASC
#  include CGX_CGX_H
#  include <proto/cybergraphics.h>
# else /* not SAS/C => gcc */
#  include CGX_CGX_H
#  if defined(POWERUP)
#   include <powerup/ppcproto/cybergraphics.h>
#  else /* AMIGAOS */
#   include <proto/cybergraphics.h>
#  endif
# endif
# ifndef BMF_SPECIALFMT
#  define BMF_SPECIALFMT 0x80	/* should be cybergraphics.h but isn't for  */
				/* some strange reason */
# endif
#endif /* USE_CYBERGFX */

/****************************************************************************/

#include <ctype.h>
#include <signal.h>

/****************************************************************************/

#include "uae.h"
#include "config.h"
#include "options.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"
#include "debug.h"
#include "hotkeys.h"

#define BitMap Picasso96BitMap  /* Argh! */
#include "picasso96.h"
#undef BitMap

/****************************************************************************/

/* this doesn't do anything yet*/
int pause_emulation;

/****************************************************************************/

#define UAEIFF "UAEIFF"        /* env: var to trigger iff dump */
#define UAESM  "UAESM"         /* env: var for screen mode */

static int need_dither;        /* well.. guess :-) */
static int use_delta_buffer;   /* this will redraw only needed places */
static int use_cyb;            /* this is for cybergfx truecolor mode */
static int use_approx_color;
int dump_iff;

extern xcolnr xcolors[4096];

static int inwindow;

static char *oldpixbuf;

/* Values for amiga_screen_type */
enum {
    UAESCREENTYPE_CUSTOM,
    UAESCREENTYPE_PUBLIC,
    UAESCREENTYPE_ASK,
    UAESCREENTYPE_LAST
};

/****************************************************************************/
/*
 * prototypes & global vars
 */

struct IntuitionBase    *IntuitionBase = NULL;
struct GfxBase          *GfxBase = NULL;
struct Library          *LayersBase = NULL;
struct Library          *AslBase = NULL;
struct Library          *CyberGfxBase = NULL;

struct AslIFace *IAsl;
struct GraphicsIFace *IGraphics;
struct LayersIFace *ILayers;
struct IntuitionIFace *IIntuition;
struct CyberGfxIFace *ICyberGfx;

unsigned long            frame_num; /* for arexx */

static UBYTE            *Line;
static struct RastPort  *RP;
static struct Screen    *S;
static struct Window    *W;
static struct RastPort  *TempRPort;
static struct BitMap    *BitMap;
#ifdef USE_CYBERGFX
static struct BitMap    *CybBitMap;
#endif
static struct ColorMap  *CM;
static int              XOffset,YOffset;

static int os39;        /* kick 39 present */
static int usepub;      /* use public screen */
static int usecyb;      /* use cybergraphics.library */
static int is_halfbrite;
static int is_ham;

static int   get_color_failed;
static int   maxpen;
static UBYTE pen[256];

#ifdef __amigaos4__
static int mouseGrabbed;
static int grabTicks;
#define GRAB_TIMEOUT 50
#endif

static struct BitMap *myAllocBitMap(ULONG,ULONG,ULONG,ULONG,struct BitMap *);
static void set_title(void);
static void myFreeBitMap(struct BitMap *);
static LONG ObtainColor(ULONG, ULONG, ULONG);
static void ReleaseColors(void);
static int  DoSizeWindow(struct Window *,int,int);
static void disk_hotkeys(void);
static int  SaveIFF(char *filename, struct Screen *scr);
static int  init_ham(void);
static void ham_conv(UWORD *src, UBYTE *buf, UWORD len);
static int  RPDepth(struct RastPort *RP);
static int get_nearest_color(int r, int g, int b);

/****************************************************************************/

void main_window_led(int led, int on);
int do_inhibit_frame(int onoff);

extern void initpseudodevices(void);
extern void closepseudodevices(void);
extern void appw_init(struct Window *W);
extern void appw_exit(void);
extern void appw_events(void);

extern int ievent_alive;


/****************************************************************************/
/* This is because on powerup, calling 68k CopyMem is too slow              */

#ifdef CopyMem
#undef CopyMem
#endif

#ifdef POWERUP	/* holger, sam: on powerup, use the alignment-optimised
		  code provided by memcpy() */
#define CopyMem(src,dst,len) memcpy(dst,src,len)
#else 		/* holger, sam: else do not use ixemul memcpy() but
		   inline instead */
#define CopyMem(src,dst,len) myCopyMem(src,dst,len)
static __inline__ void myCopyMem(void *src, void *dst, int len)
{
    char *s=src;char *d=dst;
    if(len) do *d++=*s++; while(--len);
}
#endif

/***************************************************************************
 *
 * Default hotkeys
 *
 * We need a better way of doing this. ;-)
 */
static struct uae_hotkeyseq ami_hotkeys[] =
{
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_Q, -1,      INPUTEVENT_SPC_QUIT) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_R, -1,      INPUTEVENT_SPC_WARM_RESET) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_LSH, AK_R,  INPUTEVENT_SPC_COLD_RESET) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_D, -1,      INPUTEVENT_SPC_ENTERDEBUGGER) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_S, -1,      INPUTEVENT_SPC_TOGGLEFULLSCREEN) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_M, -1,      INPUTEVENT_SPC_TOGGLEMOUSEMODE) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_G, -1,      INPUTEVENT_SPC_TOGGLEMOUSEGRAB) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_I, -1,      INPUTEVENT_SPC_INHIBITSCREEN) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_P, -1,      INPUTEVENT_SPC_SCREENSHOT) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_A, -1,      INPUTEVENT_SPC_SWITCHINTERPOL) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_NPADD, -1,  INPUTEVENT_SPC_INCRFRAMERATE) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_NPSUB, -1,  INPUTEVENT_SPC_DECRFRAMERATE) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_F1, -1,     INPUTEVENT_SPC_FLOPPY0) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_F2, -1,     INPUTEVENT_SPC_FLOPPY1) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_F3, -1,     INPUTEVENT_SPC_FLOPPY2) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_F4, -1,     INPUTEVENT_SPC_FLOPPY3) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_LSH, AK_F1, INPUTEVENT_SPC_EFLOPPY0) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_LSH, AK_F2, INPUTEVENT_SPC_EFLOPPY1) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_LSH, AK_F3, INPUTEVENT_SPC_EFLOPPY2) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_LSH, AK_F4, INPUTEVENT_SPC_EFLOPPY3) },
    { MAKE_HOTKEYSEQ (AK_CTRL, AK_LALT, AK_F, -1,      INPUTEVENT_SPC_FREEZEBUTTON) },
    { HOTKEYS_END }
};

/****************************************************************************/

extern UBYTE cidx[4][8*4096];

__inline__ void flush_line (int y)
{
    int   xs = 0;
    int   len;
    int   yoffset = y * gfxvidinfo.rowbytes;
    char *linebuf = gfxvidinfo.bufmem + yoffset;
    char *src;
    char *dst;

    if (y < 0 || y >= gfxvidinfo.height) {
/*       printf("flush_line out of window: %d\n", y); */
       return;
    }

    len = gfxvidinfo.width;

    if (is_ham) {
        ham_conv ((void*)linebuf, Line, len);
        myWritePixelLine8 (RP, 0, y, len, Line, TempRPort);
        return;
    }

#ifdef USE_CYBERGFX
    /*
     * cybergfx bitmap
     */
    if (use_cyb) {
//        CopyMem (?, linebuf, gfxvidinfo.rowbytes);
        myBltBitMapRastPort (CybBitMap, 0, y,
			     RP, XOffset, YOffset+y,
			     len, 1, 0xc0);
	/* sam: I'm worried because BltBitMapRastPort() is known to */
	/* produce spilled registers with gcc */
	return;
    }
#endif

    if (!use_delta_buffer) {
	dst = linebuf;
    } else switch (gfxvidinfo.pixbytes) {
	case 2: {
	    short *newp = (short *)linebuf;
	    short *oldp = (short *)(oldpixbuf + yoffset);
	    while (*newp++ == *oldp++) if (!--len) return;
	    src   = (char *)--newp;
	    dst   = (char *)--oldp;
	    newp += len;
	    oldp += len;
	    while (*--newp == *--oldp);
	    len   = 1 + (oldp - (short *)dst);
	    xs    = (src - linebuf)/2;
	    CopyMem (src, dst, len * 2);
	    break;
	}
	case 1: {
	    char *newp = (char *)linebuf;
	    char *oldp = (char *)(oldpixbuf + yoffset);
	    while (*newp++ == *oldp++) if(!--len) return;
	    src   = (char *)--newp;
	    dst   = (char *)--oldp;
	    newp += len;
	    oldp += len;
	    while (*--newp == *--oldp);
	    len   = 1 + (oldp - (char *)dst);
	    xs    = (src - linebuf);
	    CopyMem (src, dst, len);
	    break;
	}
	default:
	case 4: {
	    /* sam: we should not arrive here on the amiga */
	    write_log ("Bug in flush_line() !\n");
	    write_log ("use_cyb=%d\n", use_cyb);
	    write_log ("need_dither=%d\n", need_dither);
	    write_log ("depth=%d\n", RPDepth(RP));
	    write_log ("Please return those values to maintainer.\n");
	    abort ();
	}
    }

    if (need_dither) {
	DitherLine (Line, (UWORD *)dst, xs, y, (len + 3) & ~3, 8);
    } else
	CopyMem (dst, Line, len);

    myWritePixelLine8 (RP, xs + XOffset, y + YOffset, len, Line, TempRPort);
}

/****************************************************************************/

void flush_block (int ystart, int ystop)
{
    int y;

#ifdef USE_CYBERGFX
    if (use_cyb) {
	int len = gfxvidinfo.width;
	myBltBitMapRastPort (CybBitMap,
			     0, ystart,
			     RP, XOffset, YOffset + ystart,
			     len, ystop-ystart + 1, 0xc0);
			     return;
    }
#endif

#if defined(POWERUP)
    /* sam: on powerup we have to minimize call to the 68k-OS, so better
       call WritePixelArray8(); once instead of several WritePixelLine8(); */
    if (!need_dither) {
	myWritePixelArray8 (RP, XOffset,
			    ystart + YOffset,
			    XOffset + gfxvidinfo.width - 1,
			    ystop + YOffset,
			    gfxvidinfo.bufmem + ystart * gfxvidinfo.rowbytes,
			    TempRPort);
    	return;
    }
#endif
    for (y = ystart; y <= ystop; ++y)
	flush_line (y);
}

/****************************************************************************/

#if 0
static void save_frame (void)
{
    char *file;
    static int cpt = 0;
    char name[80];

    if (!dump_iff) return;
    if (!(file = getenv (UAEIFF))) return;
    if (strchr (file, '%')) sprintf(name,file,cpt++);
    else sprintf (name, "%s.%05d", file, cpt++);
    if (W->WScreen) SaveIFF (name, W->WScreen);
}
#endif

/****************************************************************************/

void flush_screen (int ystart, int ystop)
{
/* WaitBOVP() ? */
}

/****************************************************************************/

void flush_clear_screen (void)
{
    if (RP) {
#ifdef USE_CYBERGFX
	if (use_cyb)
	     FillPixelArray (RP, W->BorderLeft, W->BorderTop,
			     W->Width - W->BorderLeft - W->BorderRight,
			     W->Height - W->BorderTop - W->BorderBottom,
			     0);
        else
#endif
	{
	    SetAPen  (RP, get_nearest_color (0,0,0));
	    RectFill (RP, W->BorderLeft, W->BorderTop, W->Width - W->BorderRight,
		      W->Height - W->BorderBottom);
	}
    }
    if (use_delta_buffer)
        memset (oldpixbuf, 0, gfxvidinfo.rowbytes * currprefs.gfx_height_win);
}

/****************************************************************************/

int lockscr (void)
{
    return 1;
}

/****************************************************************************/

void unlockscr (void)
{
}

/****************************************************************************/

static int RPDepth (struct RastPort *RP)
{
#ifdef USE_CYBERGFX
    if (use_cyb)
	return GetCyberMapAttr (RP->BitMap, (LONG)CYBRMATTR_DEPTH);
#endif
    return RP->BitMap->Depth;
}

/****************************************************************************/

static int get_color(int r, int g, int b, xcolnr *cnp)
{
    int col;

    if (currprefs.amiga_use_grey)
	r = g = b = (77 * r + 151 * g + 29 * b) / 16;
    else {
	r *= 0x11;
	g *= 0x11;
	b *= 0x11;
    }

    r *= 0x01010101;
    g *= 0x01010101;
    b *= 0x01010101;
    col = ObtainColor (r, g, b);

    if (col == -1) {
	get_color_failed = 1;
	return 0;
    }

    *cnp = col;
    return 1;
}

/****************************************************************************/
/*
 * FIXME: find a better way to determine closeness of colors (closer to
 * human perception).
 */
static __inline__ void rgb2xyz (int r, int g, int b,
				int *x, int *y, int *z)
{
    *x = r * 1024 - (g + b) * 512;
    *y = 886 * (g - b);
    *z = (r + g + b) * 341;
}

static __inline__ int calc_err (int r1, int g1, int b1,
				int r2, int g2, int b2)
{
    int x1, y1, z1, x2, y2, z2;

    rgb2xyz (r1, g1, b1, &x1, &y1, &z1);
    rgb2xyz (r2, g2, b2, &x2, &y2, &z2);
    x1 -= x2; y1 -= y2; z1 -= z2;
    return x1 * x1 + y1 * y1 + z1 * z1;
}

/****************************************************************************/

static int get_nearest_color (int r, int g, int b)
{
    int i, best, err, besterr;
    int colors;
    int br=0,bg=0,bb=0;
#ifdef POWERUP
    static int *RGB_cache=NULL;
#endif

   if (currprefs.amiga_use_grey)
	r = g = b = (77 * r + 151 * g + 29 * b) / 256;

    best    = 0;
    besterr = calc_err (0, 0, 0, 15, 15, 15);
    colors  = is_halfbrite ? 32 :(1 << RPDepth (RP));

#ifdef POWERUP
    if (!RGB_cache && (RGB_cache = malloc (sizeof (*RGB_cache) * colors))) {
	/* note: The code can work if RGB_cache is not allocated ! */
	for (i = 0; i < colors; ++i)
	    RGB_cache[i] = -1;
    }
#endif

    for (i = 0; i < colors; i++) {
	long rgb;
	int cr, cg, cb;

#ifdef POWERUP
	/* sam: On powerup, calling GetRGB4() takes plenty of time. Holger
	   Jakob told me that the remapping of the 4096 colors on a 256 color
	   screen calls GetRGB4() more than 1000000 times, resulting in a loop
	   lasting for more than one hour ! To fix this, we'll use a cache
	   in order to avoid OS calls. CONCLUSION: ON POWERUP AVOID CALLING THE
	   OS ROUTINES :((( */
	if (RGB_cache) {
  	    rgb = RGB_cache[i];
	    if (rgb < 0)
		rgb = RGB_cache[i] = GetRGB4 (CM, i);
	} else
	    rgb = GetRGB4 (CM,i);
#else
	rgb = GetRGB4 (CM, i);
#endif
	cr = (rgb >> 8) & 15;
	cg = (rgb >> 4) & 15;
	cb = (rgb >> 0) & 15;

	err = calc_err (r, g, b, cr, cg, cb);

	if(err < besterr) {
	    best = i;
	    besterr = err;
	    br = cr; bg = cg; bb = cb;
	}

	if (is_halfbrite) {
	    cr /= 2; cg /= 2; cb /= 2;
	    err = calc_err (r, g, b, cr, cg, cb);
	    if (err < besterr) {
		best = i + 32;
		besterr = err;
		br = cr; bg = cg; bb = cb;
	    }
	}
    }
    return best;
}

/****************************************************************************/

static int init_colors (void)
{
    gfxvidinfo.can_double = 0;

    if (need_dither) {
	/* first try color allocation */
	int bitdepth = usepub ? 8 : RPDepth (RP);
	int maxcol;

	if (!currprefs.amiga_use_grey && bitdepth >= 3)
	    do {
		get_color_failed = 0;
		setup_dither (bitdepth, get_color);
		if (get_color_failed)
		    ReleaseColors ();
	    } while (get_color_failed && --bitdepth >= 3);

	if( !currprefs.amiga_use_grey && bitdepth >= 3) {
	    write_log ("Color dithering with %d bits\n", bitdepth);
	    return 1;
	}

	/* if that fail then try grey allocation */
	maxcol = 1 << (usepub ? 8 : RPDepth (RP));

	do {
	    get_color_failed = 0;
	    setup_greydither_maxcol (maxcol, get_color);
	    if (get_color_failed)
		ReleaseColors ();
	} while (get_color_failed && --maxcol >= 2);

	/* extra pass with approximated colors */
	if (get_color_failed)
	    do {
		maxcol=2;
		use_approx_color = 1;
		get_color_failed = 0;
		setup_greydither_maxcol (maxcol, get_color);
		if (get_color_failed)
		    ReleaseColors ();
	    } while (get_color_failed && --maxcol >= 2);

	if (maxcol >= 2) {
	    write_log ("Gray dither with %d shades.\n", maxcol);
	    return 1;
	}

	return 0; /* everything failed :-( */
    }

    /* No dither */
    switch (RPDepth (RP)) {
	case 6:
	    if (is_halfbrite) {
		static int tab[]= {
		    0x000, 0x00f, 0x0f0, 0x0ff, 0x08f, 0x0f8, 0xf00, 0xf0f,
		    0x80f, 0xff0, 0xfff, 0x88f, 0x8f0, 0x8f8, 0x8ff, 0xf08,
		    0xf80, 0xf88, 0xf8f, 0xff8, /* end of regular pattern */
		    0xa00, 0x0a0, 0xaa0, 0x00a, 0xa0a, 0x0aa, 0xaaa,
		    0xfaa, 0xf6a, 0xa80, 0x06a, 0x6af
		};
		int i;
		for (i = 0; i < 32; ++i)
		    get_color (tab[i] >> 8, (tab[i] >> 4) & 15, tab[i] & 15, xcolors);
		for (i=0; i<4096; ++i)
		    xcolors[i] = get_nearest_color (i >> 8, (i >> 4) & 15, i & 15);
		write_log ("Using %d colors + halfbrite\n", 32);
		break;
	    } else if (is_ham) {
		int i;
		for (i = 0; i < 16; ++i)
		    get_color (i, i, i, xcolors);
		write_log ("Using %d bits pseudo-truecolor (HAM).\n", 12);
		alloc_colors64k (4, 4, 4, 10, 5, 0, 0, 0, 0);
		return init_ham ();
	    }
	    /* Fall through if !is_halfbrite && !is_ham */
	case 1: case 2: case 3: case 4: case 5: case 7: case 8: {
	    int maxcol = 1 << RPDepth (RP);
	    if (maxcol >= 8 && !currprefs.amiga_use_grey)
		do {
		    get_color_failed = 0;
		    setup_maxcol (maxcol);
		    alloc_colors256 (get_color);
		    if (get_color_failed)
			ReleaseColors ();
		} while (get_color_failed && --maxcol >= 8);
	    else {
		int i;
		for (i = 0; i < maxcol; ++i) {
		    get_color ((i * 15)/(maxcol - 1), (i * 15)/(maxcol - 1),
			       (i * 15)/(maxcol - 1), xcolors);
		}
	    }
	    write_log ("Using %d colors.\n", maxcol);
	    for (maxcol = 0; maxcol < 4096; ++maxcol)
		xcolors[maxcol] = get_nearest_color (maxcol >> 8, (maxcol >> 4) & 15,
						     maxcol&15);
	    break;
	}
	case 15:
	    write_log ("Using %d bits truecolor.\n", 15);
	    alloc_colors64k (5, 5, 5, 10, 5, 0, 0, 0, 0);
	    break;
	case 16:
	    write_log ("Using %d bits truecolor.\n", 16);
	    alloc_colors64k (5, 6, 5, 11, 5, 0, 0, 0, 0);
	    break;
	case 24:
	    write_log ("Using %d bits truecolor.\n", 24);
	    alloc_colors64k (8, 8, 8, 16, 8, 0, 0, 0, 0);
	    break;
	case 32:
	    write_log ("Using %d bits truecolor.\n", 32);
	    alloc_colors64k (8, 8, 8, 16, 8, 0, 0, 0, 0);
	    break;
    }
    return 1;
}

/****************************************************************************/

static APTR blank_pointer;

/*
 * Initializes a pointer object containing a blank pointer image.
 * Used for hiding the mouse pointer
 */
static void init_pointer (void)
{
    static struct BitMap bitmap;
    static UWORD	 row[2] = {0, 0};

    InitBitMap (&bitmap, 2, 16, 1);
    bitmap.Planes[0] = (PLANEPTR) &row[0];
    bitmap.Planes[1] = (PLANEPTR) &row[1];

    blank_pointer = NewObject (NULL, POINTERCLASS,
			       POINTERA_BitMap,	(ULONG)&bitmap,
			       POINTERA_WordWidth,	1,
			       TAG_DONE);

    if (!blank_pointer)
	write_log ("Warning: Unable to allocate blank mouse pointer.\n");
}

/*
 * Free up blank pointer object
 */
static void free_pointer (void)
{
    if (blank_pointer) {
	DisposeObject (blank_pointer);
	blank_pointer = NULL;
    }
}

/*
 * Hide mouse pointer for window
 */
static void hide_pointer (struct Window *w)
{
    SetWindowPointer (w, WA_Pointer, (ULONG)blank_pointer, TAG_DONE);
}

/*
 * Restore default mouse pointer for window
 */
static void show_pointer (struct Window *w)
{
    SetWindowPointer (w, WA_Pointer, 0, TAG_DONE);
}

#ifdef __amigaos4__
/*
 * Grab mouse pointer under OS4.0. Needs to be called periodically
 * to maintain grabbed status.
 */
static void grab_pointer (struct Window *w)
{
    struct IBox box = {
	W->BorderLeft,
	W->BorderTop,
	W->Width  - W->BorderLeft - W->BorderRight,
	W->Height - W->BorderTop  - W->BorderBottom
    };

    SetWindowAttrs (W, WA_MouseLimits, &box, sizeof box);
    SetWindowAttrs (W, WA_GrabFocus, mouseGrabbed ? GRAB_TIMEOUT : 0, sizeof (ULONG));
}
#endif

/****************************************************************************/

typedef enum {
    DONT_KNOW = -1,
    INSIDE_WINDOW,
    OUTSIDE_WINDOW
} POINTER_STATE;

static POINTER_STATE pointer_state;

static POINTER_STATE get_pointer_state (const struct Window *w, int mousex, int mousey)
{
    POINTER_STATE new_state = OUTSIDE_WINDOW;

    /*
     * Is pointer within the bounds of the inner window?
     */
    if ((mousex >= w->BorderLeft)
     && (mousey >= w->BorderTop)
     && (mousex < (w->Width - w->BorderRight))
     && (mousey < (w->Height - w->BorderBottom))) {
	/*
	 * Yes. Now check whetehr the window is obscured by
	 * another window at the pointer position
	 */
	struct Screen *scr = w->WScreen;
	struct Layer  *layer;

	/* Find which layer the pointer is in */
	LockLayerInfo (&scr->LayerInfo);
	layer = WhichLayer (&scr->LayerInfo, scr->MouseX, scr->MouseY);
	UnlockLayerInfo (&scr->LayerInfo);

	/* Is this layer our window's layer? */
	if (layer == w->WLayer) {
	    /*
	     * Yes. Therefore, pointer is inside the window.
	     */
	    new_state = INSIDE_WINDOW;
	}
    }
    return new_state;
}

/****************************************************************************/

#ifdef USE_CYBERGFX
/*
 * Try to find a CGX/P96 screen mode which suits the requested size and depth
 */
static ULONG find_rtg_mode (ULONG *width, ULONG *height, ULONG depth)
{
    ULONG mode           = INVALID_ID;

    ULONG best_mode      = INVALID_ID;
    ULONG best_width     = (ULONG)-1;
    ULONG best_height    = (ULONG)-1;

    ULONG largest_mode   = INVALID_ID;
    ULONG largest_width  = 0;
    ULONG largest_height = 0;

    if (CyberGfxBase) {
	while ((mode = NextDisplayInfo (mode)) != (ULONG)INVALID_ID) {
	    if (IsCyberModeID (mode)) {
		ULONG cwidth  = GetCyberIDAttr (CYBRIDATTR_WIDTH, mode);
		ULONG cheight = GetCyberIDAttr (CYBRIDATTR_HEIGHT, mode);
		ULONG cdepth  = GetCyberIDAttr (CYBRIDATTR_DEPTH, mode);
#ifdef DEBUG
		write_log ("Checking mode:%08x w:%d h:%d d:%d\n", mode, cwidth, cheight, cdepth);
#endif
		if (cdepth == depth) {
		    /*
		     * If this mode has the largest screen size we've seen so far,
		     * remember it, just in case we don't find one big enough
		     */
		    if (cheight > largest_height && cwidth > largest_width) {
			largest_mode   = mode;
			largest_width  = cwidth;
			largest_height = cheight;
		    }

		    /*
		     * Is it large enough for our requirements?
		     */
		    if (cwidth >= *width && cheight >= *height) {
			/*
			 * Yes. Is it the best fit that we've seen so far?
			 */
			if (cwidth < best_width && cheight < best_height) {
			    best_width  = cwidth;
			    best_height = cheight;
			    best_mode   = mode;
			}
		    }
		} /* if (cdepth == depth) */
	    } /* if (IsCyberModeID (mode)) */
	} /* while */

	if (best_mode != (ULONG)INVALID_ID) {
	    /* We found a match. Return it */
	    *height = best_height;
	    *width  = best_width;
	} else if (largest_mode != (ULONG)INVALID_ID) {
	    /* We didn't find a large enough mode. Return the largest
	     * mode found at the depth - if we found one */
	    best_mode = largest_mode;
	    *height   = largest_height;
	    *width    = largest_width;
	}
#ifdef DEBUG
	if (best_mode != (ULONG) INVALID_ID)
	    write_log ("Best mode: %08x w:%d h:%d d:%d\n", best_mode, *width, *height, depth);
#endif
    }
    return best_mode;
}
#endif

static int setup_customscreen (void)
{
    static struct NewWindow NewWindowStructure = {
	0, 0, 800, 600, 0, 1,
	IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_DISKINSERTED | IDCMP_DISKREMOVED
		| IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_MOUSEMOVE
		| IDCMP_DELTAMOVE,
	WFLG_SMART_REFRESH | WFLG_BACKDROP | WFLG_RMBTRAP | WFLG_NOCAREREFRESH
	 | WFLG_BORDERLESS | WFLG_ACTIVATE | WFLG_REPORTMOUSE,
	NULL, NULL, NULL, NULL, NULL, 5, 5, 800, 600,
	CUSTOMSCREEN
    };

    LONG  width  = currprefs.gfx_width_win;
    LONG  height = currprefs.gfx_height_win;
    ULONG depth  = 0; // FIXME: Need to add some way of letting user specify preferred depth
    ULONG mode   = INVALID_ID;
    struct Screen *screen;
    ULONG error;

#ifdef USE_CYBERGFX
    /* First try to find an RTG screen that matches the requested size  */
    {
	unsigned int i;
	const UBYTE preferred_depth[] = {15, 16, 32, 8}; /* Try depths in this order of preference */

	for (i = 0; i < sizeof preferred_depth && mode == (ULONG) INVALID_ID; i++) {
	    depth = preferred_depth[i];
	    mode = find_rtg_mode (&width, &height, depth);
	}
    }

    if (mode != (ULONG) INVALID_ID) {
	if (depth > 8)
	    use_cyb = 1;
    } else {
#endif
	/* No (suitable) RTG screen available. Try a native mode */
	depth = os39 ? 8 : (currprefs.gfx_lores ? 5 : 4);
	mode = PAL_MONITOR_ID; // FIXME: should check whether to use PAL or NTSC.
	if (currprefs.gfx_lores)
	    mode |= (currprefs.gfx_height_win > 256) ? LORESLACE_KEY : LORES_KEY;
	else
	    mode |= (currprefs.gfx_height_win > 256) ? HIRESLACE_KEY : HIRES_KEY;
#ifdef USE_CYBERGFX
    }
#endif

    /* If the screen is larger than requested, centre UAE's display */
    if (width > currprefs.gfx_width_win)
	XOffset = (width - currprefs.gfx_width_win) / 2;
    if (height > currprefs.gfx_height_win)
	YOffset = (height - currprefs.gfx_height_win) / 2;

    do {
	screen = OpenScreenTags (NULL,
				 SA_Width,     width,
				 SA_Height,    height,
				 SA_Depth,     depth,
				 SA_DisplayID, mode,
				 SA_Behind,    TRUE,
				 SA_ShowTitle, FALSE,
				 SA_Quiet,     TRUE,
				 SA_ErrorCode, (ULONG)&error,
				 TAG_DONE);
    } while (!screen && error == OSERR_TOODEEP && --depth > 1); /* Keep trying until we find a supported depth */

    if (!screen) {
	/* TODO; Make this error report more useful based on the error code we got */
	write_log ("Error opening screen:%d\n", error);
	gui_message ("Cannot open custom screen for UAE.\n");
	return 0;
    }

    S  = screen;
    CM = screen->ViewPort.ColorMap;
    RP = &screen->RastPort;

    NewWindowStructure.Width  = screen->Width;
    NewWindowStructure.Height = screen->Height;
    NewWindowStructure.Screen = screen;

    W = (void*)OpenWindow (&NewWindowStructure);
    if (!W) {
	write_log ("Cannot open UAE window on custom screen.\n");
	return 0;
    }

    hide_pointer (W);

    return 1;
}

/****************************************************************************/

static int setup_publicscreen(void)
{
    UWORD ZoomArray[4] = {0, 0, 0, 0};
    char *pubscreen = strlen (currprefs.amiga_publicscreen)
	? currprefs.amiga_publicscreen : NULL;

    S = LockPubScreen (pubscreen);
    if (!S) {
	gui_message ("Cannot open UAE window on public screen '%s'\n",
		pubscreen ? pubscreen : "default");
	return 0;
    }

    ZoomArray[2] = 128;
    ZoomArray[3] = S->BarHeight + 1;

    CM = S->ViewPort.ColorMap;

    if ((S->ViewPort.Modes & (HIRES | LACE)) == HIRES) {
	if (currprefs.gfx_height_win + S->BarHeight + 1 >= S->Height) {
	    currprefs.gfx_height_win >>= 1;
	    currprefs.gfx_correct_aspect = 1;
	}
    }

    W = OpenWindowTags (NULL,
			WA_Title,        (ULONG)PACKAGE_NAME,
			WA_AutoAdjust,   TRUE,
			WA_InnerWidth,   currprefs.gfx_width_win,
			WA_InnerHeight,  currprefs.gfx_height_win,
			WA_PubScreen,    (ULONG)S,
			WA_Zoom,         (ULONG)ZoomArray,
			WA_IDCMP,        IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY
				       | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW
				       | IDCMP_MOUSEMOVE    | IDCMP_DELTAMOVE
				       | IDCMP_CLOSEWINDOW  | IDCMP_REFRESHWINDOW
				       | IDCMP_NEWSIZE      | IDCMP_INTUITICKS,
			WA_Flags,	 WFLG_DRAGBAR     | WFLG_DEPTHGADGET
				       | WFLG_REPORTMOUSE | WFLG_RMBTRAP
				       | WFLG_ACTIVATE    | WFLG_CLOSEGADGET
				       | WFLG_SMART_REFRESH,
			TAG_DONE);

    UnlockPubScreen (NULL, S);

    if (!W) {
	write_log ("Can't open window on public screen!\n");
	CM = NULL;
	return 0;
    }

    gfxvidinfo.width  = (W->Width  - W->BorderRight - W->BorderLeft);
    gfxvidinfo.height = (W->Height - W->BorderTop   - W->BorderBottom);
    XOffset = W->BorderLeft;
    YOffset = W->BorderTop;

    RP = W->RPort;

    appw_init (W);

#ifdef USE_CYBERGFX
    if (CyberGfxBase && GetCyberMapAttr (RP->BitMap, (LONG)CYBRMATTR_ISCYBERGFX) &&
			(GetCyberMapAttr (RP->BitMap, (LONG)CYBRMATTR_DEPTH) > 8)) {
	use_cyb = 1;
    }

#endif

    return 1;
}

/****************************************************************************/

static char *get_num (char *s, int *n)
{
   int i=0;
   while(isspace(*s)) ++s;
   if(*s=='0') {
     ++s;
     if(*s=='x' || *s=='X') {
       do {char c=*++s;
           if(c>='0' && c<='9') {i*=16; i+= c-'0';}    else
           if(c>='a' && c<='f') {i*=16; i+= c-'a'+10;} else
           if(c>='A' && c<='F') {i*=16; i+= c-'A'+10;} else break;
       } while(1);
     } else while(*s>='0' && *s<='7') {i*=8; i+= *s++ - '0';}
   } else {
     while(*s>='0' && *s<='9') {i*=10; i+= *s++ - '0';}
   }
   *n=i;
   while(isspace(*s)) ++s;
   return s;
}

/****************************************************************************/

static void get_displayid (ULONG *DI, LONG *DE)
{
   char *s;
   int di,de;

   *DI=INVALID_ID;
   s=getenv(UAESM);if(!s) return;
   s=get_num(s,&di);
   if(*s!=':') return;
   s=get_num(s+1,&de);
   if(!de) return;
   *DI=di; *DE=de;
}

/****************************************************************************/

static int setup_userscreen (void)
{
    struct ScreenModeRequester *ScreenRequest;
    ULONG DisplayID;


    LONG ScreenWidth = 0, ScreenHeight = 0, Depth = 0;
    UWORD OverscanType = OSCAN_STANDARD;
    BOOL AutoScroll = TRUE;
    int release_asl = 0;

    if (!AslBase) {
	AslBase = OpenLibrary ("asl.library", 36);
	if (!AslBase) {
	    write_log ("Can't open asl.library v36.\n");
	    return 0;
	} else {
#ifdef __amigaos4__
	    IAsl = (struct AslIFace *) GetInterface ((struct Library *)AslBase, "main", 1, NULL);
	    if (!IAsl) {
		CloseLibrary (AslBase);
		AslBase = 0;
		write_log ("Can't get asl.library interface\n");
	    }
#endif
	}
#ifdef __amigaos4__
    } else {
        IAsl->Obtain ();
        release_asl = 1;
#endif
    }

    ScreenRequest = AllocAslRequest (ASL_ScreenModeRequest, NULL);

    if (!ScreenRequest) {
	write_log ("Unable to allocate screen mode requester.\n");
	return 0;
    }

    get_displayid (&DisplayID, &Depth);

    if (DisplayID == (ULONG)INVALID_ID) {
	if (AslRequestTags (ScreenRequest,
			ASLSM_TitleText, (ULONG)"Select screen display mode",
			ASLSM_InitialDisplayID,    0,
			ASLSM_InitialDisplayDepth, 8,
			ASLSM_InitialDisplayWidth, currprefs.gfx_width_win,
			ASLSM_InitialDisplayHeight,currprefs.gfx_height_win,
			ASLSM_MinWidth,            320, //currprefs.gfx_width_win,
			ASLSM_MinHeight,           200, //currprefs.gfx_height_win,
			ASLSM_DoWidth,             TRUE,
			ASLSM_DoHeight,            TRUE,
			ASLSM_DoDepth,             TRUE,
			ASLSM_DoOverscanType,      TRUE,
			ASLSM_PropertyFlags,       0,
			ASLSM_PropertyMask,        DIPF_IS_DUALPF | DIPF_IS_PF2PRI,
			TAG_DONE)) {
	    ScreenWidth  = ScreenRequest->sm_DisplayWidth;
	    ScreenHeight = ScreenRequest->sm_DisplayHeight;
	    Depth        = ScreenRequest->sm_DisplayDepth;
	    DisplayID    = ScreenRequest->sm_DisplayID;
	    OverscanType = ScreenRequest->sm_OverscanType;
	    AutoScroll   = ScreenRequest->sm_AutoScroll;
	} else
	    DisplayID = INVALID_ID;
    }
    FreeAslRequest (ScreenRequest);

    if (DisplayID == (ULONG)INVALID_ID)
	return 0;

#ifdef USE_CYBERGFX
    if (CyberGfxBase && IsCyberModeID (DisplayID) && (Depth > 8)) {
	use_cyb = 1;

    }
#endif
    if ((DisplayID & HAM_KEY) && !use_cyb )
	Depth = 6; /* only ham6 for the moment */
#if 0
    if(DisplayID & DIPF_IS_HAM) Depth = 6; /* only ham6 for the moment */
#endif
    /* If chosen screen is smaller than UAE display size than clip
     * to screen size */
    if (ScreenWidth  < currprefs.gfx_width_win)
	ScreenWidth  = currprefs.gfx_width_win;
    if (ScreenHeight < currprefs.gfx_height_win)
	ScreenHeight = currprefs.gfx_height_win;

    /* If chosen screen is larger, than centre UAE's display */
    if (ScreenWidth > currprefs.gfx_width_win)
	XOffset = (ScreenWidth - currprefs.gfx_width_win) / 2;
    if (ScreenHeight > currprefs.gfx_height_win)
	YOffset = (ScreenHeight - currprefs.gfx_height_win) / 2;

    S = OpenScreenTags (NULL,
			SA_DisplayID,			 DisplayID,
			SA_Width,			 ScreenWidth,
			SA_Height,			 ScreenHeight,
			SA_Depth,			 Depth,
			SA_Overscan,			 OverscanType,
			SA_AutoScroll,			 AutoScroll,
			SA_ShowTitle,			 FALSE,
			SA_Quiet,			 TRUE,
			SA_Behind,			 TRUE,
			SA_PubName,			 (ULONG)"UAE",
			/* v39 stuff here: */
			(os39 ? SA_BackFill : TAG_DONE), (ULONG)LAYERS_NOBACKFILL,
			SA_SharePens,			 TRUE,
			SA_Exclusive,			 (use_cyb ? TRUE : FALSE),
			SA_Draggable,			 (use_cyb ? FALSE : TRUE),
			SA_Interleaved,			 TRUE,
			TAG_DONE);
    if (!S) {
	gui_message ("Unable to open the requested screen.\n");
	return 0;
    }

    CM           =  S->ViewPort.ColorMap;
    is_halfbrite = (S->ViewPort.Modes & EXTRA_HALFBRITE);
    is_ham       = (S->ViewPort.Modes & HAM);

    W = OpenWindowTags (NULL,
			WA_Width,		S->Width,
			WA_Height,		S->Height,
			WA_CustomScreen,	(ULONG)S,
			WA_Backdrop,		TRUE,
			WA_Borderless,		TRUE,
			WA_RMBTrap,		TRUE,
			WA_Activate,		TRUE,
			WA_ReportMouse,		TRUE,
			WA_IDCMP,		IDCMP_MOUSEBUTTONS
					      | IDCMP_RAWKEY
					      | IDCMP_DISKINSERTED
					      | IDCMP_DISKREMOVED
					      | IDCMP_ACTIVEWINDOW
					      | IDCMP_INACTIVEWINDOW
					      | IDCMP_MOUSEMOVE
					      | IDCMP_DELTAMOVE,
			(os39 ? WA_BackFill : TAG_IGNORE),   (ULONG) LAYERS_NOBACKFILL,
			TAG_DONE);

    if(!W) {
	write_log ("Unable to open the window.\n");
	CloseScreen (S);
	S  = NULL;
	RP = NULL;
	CM = NULL;
	return 0;
    }

    hide_pointer (W);

    RP = W->RPort; /* &S->Rastport if screen is not public */

    PubScreenStatus (S, 0);
    write_log ("Using screenmode: 0x%x:%d (%u:%d)\n",
	DisplayID, Depth, DisplayID, Depth);

    return 1;
}

/****************************************************************************/

int graphics_setup (void)
{
    if (((struct ExecBase *)SysBase)->LibNode.lib_Version < 36) {
	write_log ("UAE needs OS 2.0+ !\n");
	return 0;
    }
    os39 = (((struct ExecBase *)SysBase)->LibNode.lib_Version >= 39);

    atexit (graphics_leave);

    IntuitionBase = (void*) OpenLibrary ("intuition.library", 0L);
    if (!IntuitionBase) {
	write_log ("No intuition.library ?\n");
	return 0;
    } else {
#ifdef __amigaos4__
	IIntuition = (struct IntuitionIFace *) GetInterface ((struct Library *) IntuitionBase, "main", 1, NULL);
	if (!IIntuition) {
	    CloseLibrary ((struct Library *) IntuitionBase);
	    IntuitionBase = 0;
	    return 0;
	}
#endif
    }

    GfxBase = (void*) OpenLibrary ("graphics.library", 0L);
    if (!GfxBase) {
	write_log ("No graphics.library ?\n");
	return 0;
    } else {
#ifdef __amigaos4__
	IGraphics = (struct GraphicsIFace *) GetInterface ((struct Library *) GfxBase, "main", 1, NULL);
	if (!IGraphics) {
	    CloseLibrary ((struct Library *) GfxBase);
	    GfxBase = 0;
	    return 0;
	}
#endif
    }

    LayersBase = OpenLibrary ("layers.library", 0L);
    if (!LayersBase) {
	write_log ("No layers.library\n");
	return 0;
    } else {
#ifdef __amigaos4__
	ILayers = (struct LayersIFace *) GetInterface (LayersBase, "main", 1, NULL);
	if (!ILayers) {
	    CloseLibrary (LayersBase);
	    LayersBase = 0;
	    return 0;
	}
#endif
    }

#ifdef USE_CYBERGFX
    if (!CyberGfxBase) {
        CyberGfxBase = OpenLibrary ("cybergraphics.library", 40);
#ifdef __amigaos4__
        if (CyberGfxBase) {
	   ICyberGfx = (struct CyberGfxIFace *) GetInterface (CyberGfxBase, "main", 1, NULL);
           if (!ICyberGfx) {
	       CloseLibrary (CyberGfxBase);
	       CyberGfxBase = 0;
	   }
	}
#endif
    }
#endif
    init_pointer ();

    initpseudodevices ();

    return 1;
}

/****************************************************************************/

static struct Window *saved_prWindowPtr;

static void set_prWindowPtr (struct Window *w)
{
   struct Process *self = (struct Process *) FindTask (NULL);

   if (!saved_prWindowPtr)
	saved_prWindowPtr = self->pr_WindowPtr;
   self->pr_WindowPtr = w;
}

static void restore_prWindowPtr (void)
{
   struct Process *self = (struct Process *) FindTask (NULL);

   if (saved_prWindowPtr)
	self->pr_WindowPtr = saved_prWindowPtr;
}

/****************************************************************************/

int graphics_init (void)
{
    int i, bitdepth;

    use_delta_buffer = 0;
    need_dither = 0;
    use_cyb = 0;

    if (currprefs.gfx_width_win < 320)
	currprefs.gfx_width_win = 320;
    if (!currprefs.gfx_correct_aspect && (currprefs.gfx_height_win < 64/*200*/))
	currprefs.gfx_height_win = 200;
    currprefs.gfx_width_win += 7;
    currprefs.gfx_width_win &= ~7;

/* We'll ignore color_mode for now.
    if (currprefs.color_mode > 5) {
        write_log ("Bad color mode selected. Using default.\n");
        currprefs.color_mode = 0;
    }
*/

    gfxvidinfo.width  = currprefs.gfx_width_win;
    gfxvidinfo.height = currprefs.gfx_height_win;

    switch (currprefs.amiga_screen_type) {
	case UAESCREENTYPE_ASK:
	    if (setup_userscreen ())
		break;
	    write_log ("Trying on public screen...\n");
	    /* fall trough */
	case UAESCREENTYPE_PUBLIC:
	    is_halfbrite = 0;
	    if (setup_publicscreen ()) {
		usepub = 1;
		break;
	    }
	    write_log ("Trying on custom screen...\n");
	    /* fall trough */
	case UAESCREENTYPE_CUSTOM:
	default:
	    if (!setup_customscreen ())
		return 0;
	    break;
    }

    set_prWindowPtr (W);

    Line = AllocVec ((currprefs.gfx_width_win + 15) & ~15, MEMF_ANY | MEMF_PUBLIC);
    if (!Line) {
	write_log ("Unable to allocate raster buffer.\n");
	return 0;
    }
    BitMap = myAllocBitMap (currprefs.gfx_width_win, 1, 8, BMF_CLEAR | BMF_MINPLANES, RP->BitMap);
    if (!BitMap) {
	write_log ("Unable to allocate BitMap.\n");
	return 0;
    }
    TempRPort = AllocVec (sizeof (struct RastPort), MEMF_ANY | MEMF_PUBLIC);
    if (!TempRPort) {
	write_log ("Unable to allocate RastPort.\n");
	return 0;
    }
    CopyMem (RP, TempRPort, sizeof (struct RastPort));
    TempRPort->Layer  = NULL;
    TempRPort->BitMap = BitMap;

    if (usepub)
	set_title ();

    bitdepth = RPDepth (RP);

    gfxvidinfo.emergmem = 0;
    gfxvidinfo.linemem  = 0;

#ifdef USE_CYBERGFX
    if (use_cyb) {
	/*
	 * If using P96/CGX for output try to allocate on off-screen bitmap
	 * as the display buffer
	 *
	 * We do this now, so if it fails we can easily fall back on using
	 * graphics.library and palette-based rendering.
	 */
	ULONG fmt;

	switch (bitdepth) {
	    case 15: fmt = PIXFMT_RGB15;  break;
	    case 16: fmt = PIXFMT_RGB16;  break;
	    case 24:
	    case 32: fmt = PIXFMT_ARGB32; break;
	    default: write_log ("Unsupported bitdepth %d.\n", bitdepth); return 0;
	}

	CybBitMap = myAllocBitMap (currprefs.gfx_width_win, currprefs.gfx_height_win + 1,
	                           bitdepth,
	                           (fmt << 24) | BMF_SPECIALFMT | BMF_MINPLANES,
	                           NULL);

	if (CybBitMap) {
	    gfxvidinfo.bufmem   = (char *) GetCyberMapAttr (CybBitMap,CYBRMATTR_DISPADR);
	    gfxvidinfo.rowbytes = 	   GetCyberMapAttr (CybBitMap,CYBRMATTR_XMOD);
	    gfxvidinfo.pixbytes = 	   GetCyberMapAttr (CybBitMap,CYBRMATTR_BPPIX);
	} else {
	    /*
	     * Failed to allocate bitmap - we need to fall back on gfx.lib rendering
	     */
	    gfxvidinfo.bufmem = NULL;
	    use_cyb = 0;
	    if (bitdepth > 8) {
		bitdepth = 8;
		write_log ("Failed to allocate off-screen bitmap - falling back on 8-bit mode\n");
	    }
	}
    }
#endif

    if (is_ham) {
	/* ham 6 */
	use_delta_buffer    = 0; /* needless as the line must be fully */
	need_dither         = 0; /* recomputed */
	gfxvidinfo.pixbytes = 2;
    } else if (bitdepth <= 8) {
	/* chunk2planar is slow so we define use_delta_buffer for all modes */
	use_delta_buffer    = 1;
	need_dither         = currprefs.amiga_use_dither || (bitdepth <= 1);
	gfxvidinfo.pixbytes = need_dither ? 2 : 1;
    } else {
	/* Cybergfx mode */
	use_delta_buffer    = 0;
	need_dither         = 0;
	gfxvidinfo.pixbytes = (bitdepth >= 24) ? 4 : (bitdepth >= 12) ? 2 : 1;
    }

    if (!use_cyb) {
	/*
	 * We're not using GGX/P96 for output. Not allocate a dumb
	 * display buffer
	 */
	gfxvidinfo.rowbytes = gfxvidinfo.pixbytes * currprefs.gfx_width_win;
	gfxvidinfo.bufmem   = (char *) calloc (gfxvidinfo.rowbytes, currprefs.gfx_height_win + 1);
	/*										     ^^^ */
	/*				       This is because DitherLine may read one extra row */
    }

    if (!gfxvidinfo.bufmem) {
	write_log ("Not enough memory for video bufmem.\n");
	return 0;
    }


    if (use_delta_buffer) {
	oldpixbuf = (char *)calloc (gfxvidinfo.rowbytes, currprefs.gfx_height_win);
	if (!oldpixbuf) {
	    write_log ("Not enough memory for oldpixbuf.\n");
	    return 0;
	}
    }

    gfxvidinfo.maxblocklines = currprefs.gfx_height_win + 1;

    if (!init_colors ()) {
        write_log ("Failed to init colors.\n");
        return 0;
    }
    switch (gfxvidinfo.pixbytes) {
	case 2:
	    for (i = 0; i < 4096; i++)
		xcolors[i] *= 0x00010001;
	    gfxvidinfo.can_double = 1;
	   break;
	case 1:
	    for (i = 0; i < 4096; i++)
		xcolors[i] *= 0x01010101;
	    gfxvidinfo.can_double = 1;
	   break;
	default:
	    gfxvidinfo.can_double = 0;
	    break;
    }

    if (!usepub)
	ScreenToFront (S);

    set_default_hotkeys (ami_hotkeys);

    pointer_state = DONT_KNOW;

#if 0
    if (getenv (UAEIFF) && !use_cyb) {
	dump_iff = 1;
	write_log ("Saving to \"%s\"\n", getenv (UAEIFF));
    }
#endif
   return 1;
}

/****************************************************************************/

void graphics_leave (void)
{
    closepseudodevices ();
    appw_exit ();

#ifdef USE_CYBERGFX
    if (CybBitMap) {
	WaitBlit ();
	myFreeBitMap (CybBitMap);
	CybBitMap = NULL;
    }
#endif
    if (BitMap) {
	WaitBlit ();
	myFreeBitMap (BitMap);
	BitMap = NULL;
    }
    if (TempRPort) {
	FreeVec (TempRPort);
	TempRPort = NULL;
    }
    if (Line) {
	FreeVec (Line);
	Line = NULL;
    }
    if (CM) {
	ReleaseColors();
	CM = NULL;
    }
    if (W) {
	restore_prWindowPtr ();
	CloseWindow (W);
	W = NULL;
    }

    free_pointer ();

    if (!usepub && S) {
	if (!CloseScreen (S)) {
	    gui_message ("Please close all opened windows on UAE's screen.\n");
	    do
		Delay (50);
	    while (!CloseScreen (S));
	}
	S = NULL;
    }
    if (AslBase) {
	CloseLibrary( (void*) AslBase);
	AslBase = NULL;
    }
    if (GfxBase) {
	CloseLibrary ((void*)GfxBase);
	GfxBase = NULL;
    }
    if (LayersBase) {
	CloseLibrary (LayersBase);
	LayersBase = NULL;
    }
    if (IntuitionBase) {
	CloseLibrary ((void*)IntuitionBase);
	IntuitionBase = NULL;
    }
    if (CyberGfxBase) {
	CloseLibrary((void*)CyberGfxBase);
	CyberGfxBase = NULL;
    }
}

/****************************************************************************/

int do_inhibit_frame (int onoff)
{
    if (onoff != -1) {
	inhibit_frame = onoff ? 1 : 0;
	if (inhibit_frame)
	    write_log ("display disabled\n");
	else
	    write_log ("display enabled\n");
	set_title ();
    }
    return inhibit_frame;
}

/***************************************************************************/

void handle_events(void)
{
    struct IntuiMessage *msg;
    int dmx, dmy, mx, my, class, code, qualifier;

   /* this function is called at each frame, so: */
    ++frame_num;       /* increase frame counter */
#if 0
    save_frame();      /* possibly save frame    */
#endif

    /*
     * This is a hack to simulate ^C as is seems that break_handler
     * is lost when system() is called.
     */
    if (SetSignal (0L, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D) &
		(SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_D)) {
	activate_debugger ();
    }

#if defined(POWERUP)
    /* Holger: The while-loop caused problems if a key was pressed to fast */
    if ((msg = (struct IntuiMessage*) GetMsg (W->UserPort))) {
#else
    while ((msg = (struct IntuiMessage*) GetMsg (W->UserPort))) {
#endif
	class     = msg->Class;
	code      = msg->Code;
	dmx       = msg->MouseX;
	dmy       = msg->MouseY;
	mx        = msg->IDCMPWindow->MouseX; // Absolute pointer coordinates
	my        = msg->IDCMPWindow->MouseY; // relative to the window
	qualifier = msg->Qualifier;

	ReplyMsg ((struct Message*)msg);

	switch (class) {
	    case IDCMP_NEWSIZE:
		do_inhibit_frame ((W->Flags & WFLG_ZOOMED) ? 1 : 0);
		break;

	    case IDCMP_REFRESHWINDOW:
		if (use_delta_buffer) {
		    /* hack: this forces refresh */
		    char *ptr = oldpixbuf;
		    int i, len = gfxvidinfo.width;
		    len *= gfxvidinfo.pixbytes;
		    for (i=0; i < currprefs.gfx_height_win; ++i) {
			ptr[00000] ^= 255;
			ptr[len-1] ^= 255;
			ptr += gfxvidinfo.rowbytes;
		    }
		}
		BeginRefresh (W);
		flush_block (0, currprefs.gfx_height_win - 1);
		EndRefresh (W, TRUE);
		break;

	    case IDCMP_CLOSEWINDOW:
		uae_quit ();
		break;

	    case IDCMP_RAWKEY: {
		int keycode = code & 127;
		int state   = code & 128 ? 0 : 1;
		int ievent;

		if ((qualifier & IEQUALIFIER_REPEAT) == 0) {
		    /* We just want key up/down events - not repeats */
		    if ((ievent = match_hotkey_sequence (keycode, state)))
			handle_hotkey_event (ievent, state);
		    else
			inputdevice_do_keyboard (keycode, state);
		}
		break;
	     }

	    case IDCMP_MOUSEMOVE:
		setmousestate (0, 0, dmx, 0);
		setmousestate (0, 1, dmy, 0);

		if (usepub) {
		    POINTER_STATE new_state = get_pointer_state (W, mx, my);
		    if (new_state != pointer_state) {
			pointer_state = new_state;
			if (pointer_state == INSIDE_WINDOW)
			    hide_pointer (W);
			else
			    show_pointer (W);
		    }
		}
		break;

	    case IDCMP_MOUSEBUTTONS:
		if (code == SELECTDOWN) setmousebuttonstate (0, 0, 1);
		if (code == SELECTUP)   setmousebuttonstate (0, 0, 0);
		if (code == MIDDLEDOWN) setmousebuttonstate (0, 2, 1);
		if (code == MIDDLEUP)   setmousebuttonstate (0, 2, 0);
		if (code == MENUDOWN)   setmousebuttonstate (0, 1, 1);
		if (code == MENUUP)     setmousebuttonstate (0, 1, 0);
		break;

	    /* Those 2 could be of some use later. */
	    case IDCMP_DISKINSERTED:
		/*printf("diskinserted(%d)\n",code);*/
		break;

	    case IDCMP_DISKREMOVED:
		/*printf("diskremoved(%d)\n",code);*/
		break;

	    case IDCMP_ACTIVEWINDOW:
		/* When window regains focus (presumably after losing focus at some
		 * point) UAE needs to know any keys that have changed state in between.
		 * A simple fix is just to tell UAE that all keys have been released.
		 * This avoids keys appearing to be "stuck" down.
		 */
		inputdevice_release_all_keys ();
		reset_hotkeys ();

		break;

	    case IDCMP_INACTIVEWINDOW:
		break;

	    case IDCMP_INTUITICKS:
#ifdef __amigaos4__
		grabTicks--;
		if (grabTicks < 0) {
		    grabTicks = GRAB_TIMEOUT;
		    #ifdef __amigaos4__
			if (mouseGrabbed)
			    grab_pointer (W);
		    #endif
		}
#endif
		break;

	    default:
		write_log ("Unknown event class: %x\n", class);
		break;
        }
    }

    gui_handle_events();
    appw_events();
}

/***************************************************************************/

int debuggable (void)
{
    return 1;
}

/***************************************************************************/

int needmousehack (void)
{
    return 0;
}

/***************************************************************************/

void LED (int on)
{
}

/***************************************************************************/

/* sam: need to put all this in a separate module */

#ifdef PICASSO96

void DX_Invalidate (int first, int last)
{
}

int DX_BitsPerCannon (void)
{
    return 8;
}

void DX_SetPalette (int start, int count)
{
}

int DX_FillResolutions (uae_u16 *ppixel_format)
{
    return 0;
}

void gfx_set_picasso_modeinfo (int w, int h, int depth)
{
}

void gfx_set_picasso_baseaddr (uaecptr a)
{
}

void gfx_set_picasso_state (int on)
{
}

void begindrawing (void)
{
}

void enddrawing (void)
{
}

uae_u8 *lockscr (void)
{
return NULL;
}

void unlockscr (void)
{
}
#endif

/***************************************************************************/

static int led_state[5];

#define WINDOW_TITLE PACKAGE_NAME " " PACKAGE_VERSION

static void set_title (void)
{
#if 0
    static char title[80];
    static char ScreenTitle[200];

    if (!usepub)
	return;

    sprintf (title,"%sPower: [%c] Drives: [%c] [%c] [%c] [%c]",
	     inhibit_frame? WINDOW_TITLE " (PAUSED) - " : WINDOW_TITLE,
	     led_state[0] ? 'X' : ' ',
	     led_state[1] ? '0' : ' ',
	     led_state[2] ? '1' : ' ',
	     led_state[3] ? '2' : ' ',
	     led_state[4] ? '3' : ' ');

    if (!*ScreenTitle) {
	sprintf (ScreenTitle,
                 "UAE-%d.%d.%d (%s%s%s)  by Bernd Schmidt & contributors, "
#ifdef POWERUP
                 "Amiga Port by Samuel Devulder & Holger Jakob (PPC extensions).",
#else
                 "Amiga Port by Samuel Devulder.",
#endif
		  UAEMAJOR, UAEMINOR, UAESUBREV,
		  currprefs.cpu_level==0?"68000":
		  currprefs.cpu_level==1?"68010":
		  currprefs.cpu_level==2?"68020":"68020/68881",
		  currprefs.address_space_24?" 24bits":"",
		  currprefs.cpu_compatible?" compat":"");
        SetWindowTitles(W, title, ScreenTitle);
    } else SetWindowTitles(W, title, (char*)-1);
#endif
    char *title = inhibit_frame ? WINDOW_TITLE " (Display off)" : WINDOW_TITLE;
    SetWindowTitles (W, title, (char*)-1);
}

/****************************************************************************/

void main_window_led (int led, int on)                /* is used in amigui.c */
{
#if 0
    if (led >= 0 && led <= 4)
	led_state[led] = on;
#endif
    set_title ();
}

/****************************************************************************/
/*
 * Routines for OS2.0 (code taken out of mpeg_play by Michael Balzer)
 */
static struct BitMap *myAllocBitMap(ULONG sizex, ULONG sizey, ULONG depth,
                                    ULONG flags, struct BitMap *friend_bitmap)
{
    struct BitMap *bm;

#if !defined __amigaos4__ && !defined __MORPHOS__ && !defined __AROS__
    if (!os39) {
	unsigned long extra = (depth > 8) ? depth - 8 : 0;
	bm = AllocVec (sizeof *bm + (extra * 4), MEMF_CLEAR);
	if (bm) {
	    ULONG i;
	    InitBitMap (bm, depth, sizex, sizey);
	    for (i = 0; i<depth; i++) {
		if (!(bm->Planes[i] = AllocRaster (sizex, sizey))) {
		    while (i--)
			FreeRaster (bm->Planes[i], sizex, sizey);
		    FreeVec (bm);
		    bm = 0;
		    break;
		}
	    }
	}
    } else
#endif
	bm = AllocBitMap (sizex, sizey, depth, flags, friend_bitmap);

    return bm;
}

/****************************************************************************/

static void myFreeBitMap(struct BitMap *bm)
{
#if !defined __amigaos4__ && !defined __MORPHOS__ && !defined __AROS__
    if (!os39) {
	while(bm->Depth--)
	    FreeRaster(bm->Planes[bm->Depth], bm->BytesPerRow*8, bm->Rows);
	FreeVec(bm);
    } else
#endif
	FreeBitMap (bm);

    return;
}

/****************************************************************************/
/*
 * find the best appropriate color. return -1 if none is available
 */
static LONG ObtainColor (ULONG r,ULONG g,ULONG b)
{
    int i, crgb;
    int colors;

    if (os39 && usepub && CM) {
	i = ObtainBestPen (CM, r, g, b,
			   OBP_Precision, (use_approx_color ? PRECISION_GUI
							    : PRECISION_EXACT),
			   OBP_FailIfBad, TRUE,
			   TAG_DONE);
	if (i != -1) {
	    if (maxpen<256)
		pen[maxpen++] = i;
	    else
		i = -1;
        }
        return i;
    }

    colors = is_halfbrite ? 32 : (1 << RPDepth (RP));

    /* private screen => standard allocation */
    if (!usepub) {
	if (maxpen >= colors)
	    return -1; /* no more colors available */
	if (os39)
	    SetRGB32 (&S->ViewPort, maxpen, r, g, b);
	else
	    SetRGB4 (&S->ViewPort, maxpen, r >> 28, g >> 28, b >> 28);
	return maxpen++;
    }

    /* public => find exact match */
    r >>= 28; g >>= 28; b >>= 28;
    if (use_approx_color)
	return get_nearest_color (r, g, b);
    crgb = (r << 8) | (g << 4) | b;
    for (i = 0; i < colors; i++ ) {
	int rgb = GetRGB4 (CM, i);
	if (rgb == crgb)
	    return i;
    }
    return -1;
}

/****************************************************************************/
/*
 * free a color entry
 */
static void ReleaseColors(void)
{
    if (os39 && usepub && CM)
	while (maxpen > 0)
	    ReleasePen (CM, pen[--maxpen]);
    else
	maxpen = 0;
}

/****************************************************************************/

static int DoSizeWindow (struct Window *W, int wi, int he)
{
    register int x,y;
    int ret = 1;

    wi += W->BorderRight + W->BorderLeft;
    he += W->BorderBottom + W->BorderTop;
    x   = W->LeftEdge;
    y   = W->TopEdge;

    if (x + wi >= W->WScreen->Width)  x = W->WScreen->Width  - wi;
    if (y + he >= W->WScreen->Height) y = W->WScreen->Height - he;

    if (x < 0 || y < 0) {
	write_log ("Working screen too small to open window (%dx%d).\n", wi, he);
	if (x < 0) {
	    x = 0;
	    wi = W->WScreen->Width;
	}
	if (y < 0) {
	    y = 0;
	    he = W->WScreen->Height;
	}
	ret = 0;
    }

    x  -= W->LeftEdge;
    y  -= W->TopEdge;
    wi -= W->Width;
    he -= W->Height;

    if (x | y)	 MoveWindow (W, x, y);
    if (wi | he) SizeWindow (W, wi, he);

    return ret;
}

/****************************************************************************/

#if 0

#define MAKEID(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|d)

static int SaveIFF (char *filename, struct Screen *scr)
{
    struct DisplayInfo DI;
    FILE *file;
    ULONG BODYsize;
    ULONG modeid;
    ULONG count;
    ULONG i;

    struct {ULONG iff_type, iff_length;} chunk;
    struct {ULONG fc_type, fc_length, fc_subtype;} FORM;
    struct {
	UWORD w,h,x,y;
	UBYTE depth, masking, compression, pad1;
	UWORD transparentColor;
	UBYTE xAspect, yAspect;
	WORD  pagewidth,pageheight;
    } BMHD;

    BODYsize = scr->BitMap.Depth * scr->BitMap.Rows * 2 * ((scr->Width+15)/16);
    modeid   = GetVPModeID(&S->ViewPort);
    count    = scr->ViewPort.ColorMap->Count;

    FORM.fc_type    = MAKEID('F','O','R','M');
    FORM.fc_length  = 4 +                 /* ILBM */
                      8 + sizeof(BMHD) +  /* BMHD */
                      8 + 4 +             /* CAMG */
                      8 + 3*count +       /* CMAP */
                      8 + BODYsize;       /* BODY */
    FORM.fc_subtype = MAKEID('I','L','B','M');

    if(!(file = fopen(filename,"w"))) return 0;
    if(fwrite(&FORM,sizeof(FORM),1,file)!=1) goto err;

    BMHD.w           =
    BMHD.pagewidth   = scr->Width;
    BMHD.h           =
    BMHD.pageheight  = scr->Height;
    BMHD.x           = 0;
    BMHD.y           = 0;
    BMHD.depth       = scr->BitMap.Depth;
    BMHD.masking     = 0;
    BMHD.compression = 0;
    BMHD.pad1        = 0;
    BMHD.transparentColor = 0;
    BMHD.xAspect     = 22;
    BMHD.yAspect     = 11;

    if(GetDisplayInfoData(NULL, (UBYTE *)&DI, sizeof(struct DisplayInfo),
                          DTAG_DISP, modeid)) {
    BMHD.xAspect     = DI.Resolution.x;
    BMHD.yAspect     = DI.Resolution.y;
    }

    chunk.iff_type   = MAKEID('B','M','H','D');
    chunk.iff_length = sizeof(BMHD);
    if(fwrite(&chunk,sizeof(chunk),1,file)!=1
    || fwrite(&BMHD, sizeof(BMHD), 1,file)!=1) goto err;

    chunk.iff_type   = MAKEID('C','A','M','G');
    chunk.iff_length = sizeof(modeid);
    if(fwrite(&chunk, sizeof(chunk),   1,file)!=1
    || fwrite(&modeid,chunk.iff_length,1,file)!=1) goto err;

   chunk.iff_type    = MAKEID('C','M','A','P');
   chunk.iff_length  = 3 * count;
   if(fwrite(&chunk,sizeof(chunk),1,file)!=1) goto err;
   for(i=0; i<count; ++i) {
      ULONG c = GetRGB4(scr->ViewPort.ColorMap, i);
      UBYTE d;
      d = (c>>8)&15;d |= d<<4;if(fwrite(&d,1,1,file)!=1) goto err;
      d = (c>>4)&15;d |= d<<4;if(fwrite(&d,1,1,file)!=1) goto err;
      d = (c>>0)&15;d |= d<<4;if(fwrite(&d,1,1,file)!=1) goto err;
   }

   chunk.iff_type    = MAKEID('B','O','D','Y');
   chunk.iff_length  = BODYsize;
   if(fwrite(&chunk,sizeof(chunk),1,file)!=1) goto err;
   {
   int r,p;
   struct BitMap *bm = S->RastPort.BitMap;
   for(r=0; r<bm->Rows; ++r) for(p=0; p<bm->Depth; ++p)
   if(fwrite(bm->Planes[p] + r*bm->BytesPerRow, 2*((S->Width+15)/16), 1, file)!=1) goto err;
   }

   fclose(file);
   return 1;
err:
   gui_message ("Error writing to \"%s\"\n", filename);
   fclose(file);
   return 0;
   }
#endif

/****************************************************************************/
/* Here lies an algorithm to convert a 12bits truecolor buffer into a HAM
 * buffer. That algorithm is quite fast and if you study it closely, you'll
 * understand why there is no need for MMX cpu to subtract three numbers in
 * the same time. I can think of a quicker algorithm but it'll need 4096*4096
 * = 1<<24 = 16Mb of memory. That's why I'm quite proud of this one which
 * only need roughly 64Kb (could be reduced down to 40Kb, but it's not
 * worth as I use cidx as a buffer which is 128Kb long)..
 ****************************************************************************/

static int dist4 (LONG rgb1, LONG rgb2) /* computes distance very quickly */
{
    int d = 0, t;
    t = (rgb1&0xF00)-(rgb2&0xF00); t>>=8; if (t<0) d -= t; else d += t;
    t = (rgb1&0x0F0)-(rgb2&0x0F0); t>>=4; if (t<0) d -= t; else d += t;
    t = (rgb1&0x00F)-(rgb2&0x00F); t>>=0; if (t<0) d -= t; else d += t;
#if 0
    t = rgb1^rgb2;
    if(t&15) ++d; t>>=4;
    if(t&15) ++d; t>>=4;
    if(t&15) ++d;
#endif
    return d;
}

#define d_dst (00000+(UBYTE*)cidx) /* let's use cidx as a buffer */
#define d_cmd (16384+(UBYTE*)cidx)
#define h_buf (32768+(UBYTE*)cidx)

static int init_ham (void)
{
    int i,t,RGB;

    /* try direct color first */
    for (RGB = 0; RGB < 4096; ++RGB) {
	int c,d;
	c = d = 50;
	for (i = 0; i < 16; ++i) {
	    t = dist4 (i*0x111, RGB);
	    if (t<d) {
		d = t;
		c = i;
	    }
	}
	i = (RGB & 0x00F) | ((RGB & 0x0F0) << 1) | ((RGB & 0xF00) << 2);
	d_dst[i] = (d << 2) | 3; /* the "|3" is a trick to speedup comparison */
	d_cmd[i] = c;		 /* in the conversion process */
    }
    /* then hold & modify */
    for (i = 0; i < 32768; ++i) {
	int dr, dg, db, d, c;
	dr = (i>>10) & 0x1F; dr -= 0x10; if (dr < 0) dr = -dr;
	dg = (i>>5)  & 0x1F; dg -= 0x10; if (dg < 0) dg = -dg;
	db = (i>>0)  & 0x1F; db -= 0x10; if (db < 0) db = -db;
	c  = 0; d = 50;
	t = dist4 (0,  0*256 + dg*16 + db); if (t < d) {d = t; c = 0;}
	t = dist4 (0, dr*256 +  0*16 + db); if (t < d) {d = t; c = 1;}
	t = dist4 (0, dr*256 + dg*16 +  0); if (t < d) {d = t; c = 2;}
	h_buf[i] = (d<<2) | c;
    }
    return 1;
}

/* great algorithm: convert trucolor into ham using precalc buffers */
#undef USE_BITFIELDS
static void ham_conv (UWORD *src, UBYTE *buf, UWORD len)
{
    /* A good compiler (ie. gcc :) will use bfext/bfins instructions */
#ifdef __SASC
    union { struct { unsigned int _:17, r:5, g:5, b:5; } _;
	    int all;} rgb, RGB;
#else
    union { struct { ULONG _:17,r:5,g:5,b:5;} _; ULONG all;} rgb, RGB;
#endif
    rgb.all = 0;
    while(len--) {
        UBYTE c,t;
        RGB.all = *src++;
        c = d_cmd[RGB.all];
        /* cowabonga! */
        t = h_buf[16912 + RGB.all - rgb.all];
#ifndef USE_BITFIELDS
        if(t<=d_dst[RGB.all]) {
	    static int ht[]={32+10,48+5,16+0}; ULONG m;
	    t &= 3; m = 0x1F<<(ht[t]&15);
            m = ~m; rgb.all &= m;
            m = ~m; m &= RGB.all;rgb.all |= m;
	    m >>= ht[t]&15;
	    c = (ht[t]&~15) | m;
        } else {
	    rgb.all = c;
	    rgb.all <<= 5; rgb.all |= c;
	    rgb.all <<= 5; rgb.all |= c;
        }
#else
        if(t<=d_dst[RGB.all]) {
            t&=3;
            if(!t)        {c = 32; c |= (rgb._.r = RGB._.r);}
            else {--t; if(!t) {c = 48; c |= (rgb._.g = RGB._.g);}
            else              {c = 16; c |= (rgb._.b = RGB._.b);} }
        } else rgb._.r = rgb._.g = rgb._.b = c;
#endif
        *buf++ = c;
    }
}

/****************************************************************************/

#ifdef POWERUP
/* sam: here is the code to avoid spilled register trouble */
static void myBltBitMapRastPort (struct BitMap *srcBitMap, long xSrc,
				 long ySrc, struct RastPort * destRP,
				 long xDest, long yDest, long xSize,
				 long ySize, unsigned long minterm)
{
    BltBitMapRastPort (srcBitMap, xSrc, ySrc, destRP,
		       xDest, yDest, xSize, ySize, minterm);
}

static void myWritePixelLine8 (struct RastPort* a, int b, int c,
			       int d, char *e, struct RastPort*f)
{
    WritePixelLine8 (a, b, c, d, e, f);
}

static void myWritePixelArray8 (struct RastPort *a, int b, int c,
				int d, int e, char *f, struct RastPort *g)
{
    WritePixelArray8(a, b, c, d, e, f, g);
}
#endif

/****************************************************************************/

int check_prefs_changed_gfx (void)
{
   return 0;
}

/****************************************************************************/

void toggle_mousegrab (void)
{
#ifdef __amigaos4__
    mouseGrabbed = 1 - mouseGrabbed;
    grabTicks    = GRAB_TIMEOUT;
    if (W)
	grab_pointer (W);
#else
    write_log ("Mouse grab not supported\n");
#endif
}

void framerate_up (void)
{
    if (currprefs.gfx_framerate < 20)
	changed_prefs.gfx_framerate = currprefs.gfx_framerate + 1;
}

void framerate_down (void)
{
    if (currprefs.gfx_framerate > 1)
	changed_prefs.gfx_framerate = currprefs.gfx_framerate - 1;
}

int is_fullscreen (void)
{
    return 0;
}

void toggle_fullscreen (void)
{
}

void screenshot (int type)
{
    write_log ("Screenshot not implemented yet\n");
}

/****************************************************************************
 *
 * Mouse inputdevice functions
 */

#define MAX_BUTTONS     3
#define MAX_AXES        3
#define FIRST_AXIS      0
#define FIRST_BUTTON    MAX_AXES

static int init_mouse (void)
{
   return 1;
}

static void close_mouse (void)
{
   return;
}

static int acquire_mouse (int num, int flags)
{
   return 1;
}

static void unacquire_mouse (int num)
{
   return;
}

static int get_mouse_num (void)
{
    return 1;
}

static char *get_mouse_name (int mouse)
{
    return 0;
}

static int get_mouse_widget_num (int mouse)
{
    return MAX_AXES + MAX_BUTTONS;
}

static int get_mouse_widget_first (int mouse, int type)
{
    switch (type) {
        case IDEV_WIDGET_BUTTON:
            return FIRST_BUTTON;
        case IDEV_WIDGET_AXIS:
            return FIRST_AXIS;
    }
    return -1;
}

static int get_mouse_widget_type (int mouse, int num, char *name, uae_u32 *code)
{
    if (num >= MAX_AXES && num < MAX_AXES + MAX_BUTTONS) {
        if (name)
            sprintf (name, "Button %d", num + 1 + MAX_AXES);
        return IDEV_WIDGET_BUTTON;
    } else if (num < MAX_AXES) {
        if (name)
            sprintf (name, "Axis %d", num + 1);
        return IDEV_WIDGET_AXIS;
    }
    return IDEV_WIDGET_NONE;
}

static void read_mouse (void)
{
    /* We handle mouse input in handle_events() */
}

struct inputdevice_functions inputdevicefunc_mouse = {
    init_mouse, close_mouse, acquire_mouse, unacquire_mouse, read_mouse,
    get_mouse_num, get_mouse_name,
    get_mouse_widget_num, get_mouse_widget_type,
    get_mouse_widget_first
};

/*
 * Default inputdevice config for mouse
 */
void input_get_default_mouse (struct uae_input_device *uid)
{
    /* Supports only one mouse for now */
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
    uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
    uid[0].enabled = 1;
}

/****************************************************************************
 *
 * Keyboard inputdevice functions
 */
static int get_kb_num (void)
{
    return 1;
}

static char *get_kb_name (int kb)
{
    return 0;
}

static int get_kb_widget_num (int kb)
{
    return 128;
}

static int get_kb_widget_first (int kb, int type)
{
    return 0;
}

static int get_kb_widget_type (int kb, int num, char *name, uae_u32 *code)
{
    // fix me
    *code = num;
    return IDEV_WIDGET_KEY;
}

static int keyhack (int scancode, int pressed, int num)
{
    return scancode;
}

static void read_kb (void)
{
}

static int init_kb (void)
{
    return 1;
}

static void close_kb (void)
{
}

static int acquire_kb (int num, int flags)
{
    return 1;
}

static void unacquire_kb (int num)
{
}

struct inputdevice_functions inputdevicefunc_keyboard =
{
    init_kb, close_kb, acquire_kb, unacquire_kb,
    read_kb, get_kb_num, get_kb_name, get_kb_widget_num,
    get_kb_widget_type, get_kb_widget_first
};

int getcapslockstate (void)
{
    return 0;
}

void setcapslockstate (int state)
{
}

/****************************************************************************
 *
 * Handle gfx specific cfgfile options
 */

static const char *screen_type[] = { "custom", "public", "ask", 0 };

void gfx_default_options (struct uae_prefs *p)
{
    p->amiga_screen_type     = UAESCREENTYPE_PUBLIC;
    p->amiga_publicscreen[0] = '\0';
    p->amiga_use_dither      = 1;
    p->amiga_use_grey        = 0;
}

void gfx_save_options (FILE *f, struct uae_prefs *p)
{
    cfgfile_write (f, GFX_NAME ".screen_type=%s\n",  screen_type[p->amiga_screen_type]);
    cfgfile_write (f, GFX_NAME ".publicscreen=%s\n", p->amiga_publicscreen);
    cfgfile_write (f, GFX_NAME ".use_dither=%s\n",   p->amiga_use_dither ? "true" : "false");
    cfgfile_write (f, GFX_NAME ".use_grey=%s\n",     p->amiga_use_grey ? "true" : "false");
}

int gfx_parse_option (struct uae_prefs *p, char *option, char *value)
{
    return (cfgfile_yesno  (option, value, "use_dither",   &p->amiga_use_dither)
	 || cfgfile_yesno  (option, value, "use_grey",	 &p->amiga_use_grey)
         || cfgfile_strval (option, value, "screen_type",  &p->amiga_screen_type, screen_type, 0)
         || cfgfile_string (option, value, "publicscreen", &p->amiga_publicscreen[0], 256)
    );
}

/****************************************************************************/
