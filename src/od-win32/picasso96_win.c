/*
* UAE - The U*nix Amiga Emulator
*
* Picasso96 Support Module
*
* Copyright 1997-2001 Brian King <Brian_King@CodePoet.com>
* Copyright 2000-2001 Bernd Roesch <>
*
* Theory of operation:
* On the Amiga side, a Picasso card consists mainly of a memory area that
* contains the frame buffer.  On the UAE side, we allocate a block of memory
* that will hold the frame buffer.  This block is in normal memory, it is
* never directly on the graphics card.  All graphics operations, which are
* mainly reads and writes into this block and a few basic operations like
* filling a rectangle, operate on this block of memory.
* Since the memory is not on the graphics card, some work must be done to
* synchronize the display with the data in the Picasso frame buffer.  There
* are various ways to do this.  One possibility is to allocate a second
* buffer of the same size, and perform all write operations twice.  Since
* we never read from the second buffer, it can actually be placed in video
* memory.  The X11 driver could be made to use the Picasso frame buffer as
* the data buffer of an XImage, which could then be XPutImage()d from time
* to time.  Another possibility is to translate all Picasso accesses into
* Xlib (or GDI, or whatever your graphics system is) calls.  This possibility
* is a bit tricky, since there is a risk of generating very many single pixel
* accesses which may be rather slow.
*
* TODO:
* - we want to add a manual switch to override SetSwitch for hardware banging
*   programs started from a Picasso workbench.
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "threaddep/thread.h"
#include "uae.h"
#include "memory.h" 
#include "custom.h"
#include "events.h"
#include "newcpu.h"
#include "xwin.h"
#include "savestate.h"

#include <ddraw.h>

#include "dxwrap.h"
#include "picasso96_win.h"
#include "win32gfx.h"

int p96hack_vpos, p96hack_vpos2, p96refresh_active; 
int have_done_picasso; /* For the JIT compiler */
int picasso_is_special=PIC_WRITE; /* ditto */
int picasso_is_special_read=PIC_READ; /* ditto */
#define SWAPSPEEDUP 
#ifdef PICASSO96
#ifdef DEBUG // Change this to _DEBUG for debugging
#define P96TRACING_ENABLED 1
#define P96TRACING_LEVEL 1
#endif
#define LOCK_UNLOCK_MADNESS //need for 7 times faster linedraw
#define PIXEL_LOCK         //and scrollable screens
#define MAXFLUSHPIXEL 1600 //pixel draw in a lock
static void flushpixels(void);
int pixelcount,palette_changed;
struct pixel32{
	uaecptr addr;
    uae_u32 value;
	int size;
};
struct pixel32 pixelbase[MAXFLUSHPIXEL+2];
#ifdef P96TRACING_ENABLED
#define P96TRACE(x)	do { write_log x; } while(0)
#else
#define P96TRACE(x)
#endif

#define GetBytesPerPixel(x) GetBytesPerPixel2(x,__FILE__,__LINE__)

static uae_u32 REGPARAM2 gfxmem_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM2 gfxmem_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM2 gfxmem_bget (uaecptr) REGPARAM;
static void REGPARAM2 gfxmem_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 gfxmem_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 gfxmem_bput (uaecptr, uae_u32) REGPARAM;
static int REGPARAM2 gfxmem_check (uaecptr addr, uae_u32 size) REGPARAM;
static uae_u8 *REGPARAM2 gfxmem_xlate (uaecptr addr) REGPARAM;

static void write_gfx_long (uaecptr addr, uae_u32 value);
static void write_gfx_word (uaecptr addr, uae_u16 value);
static void write_gfx_byte (uaecptr addr, uae_u8 value);

static uae_u8 all_ones_bitmap, all_zeros_bitmap; /* yuk */

struct picasso96_state_struct picasso96_state;
struct picasso_vidbuf_description picasso_vidinfo;

/* These are the maximum resolutions... They are filled in by GetSupportedResolutions() */
/* have to fill this in, otherwise problems occur on the Amiga side P96 s/w which expects
/* data here. */
struct ScreenResolution planar = { 320, 240 };
struct ScreenResolution chunky = { 640, 480 };
struct ScreenResolution hicolour = { 640, 480 };
struct ScreenResolution truecolour = { 640, 480 };
struct ScreenResolution alphacolour = { 640, 480 };

static uae_u32 p2ctab[256][2];
static int set_gc_called = 0;
//fastscreen
static uaecptr oldscr=0;
#ifdef _DEBUG
void PICASSO96_Unlock2( char *filename, int linenum )
#else
void PICASSO96_Unlock( void )
#endif
{
#ifdef LOCK_UNLOCK_MADNESS
#if defined( P96TRACING_ENABLED ) && P96TRACING_LEVEL > 1
	// This format of output lets you double-click and jump to file/line
	write_log( "%s(%d) : calling P96 UNLOCK with picasso_on=%d\n", filename, linenum, picasso_on );
#endif
    if( picasso_on )
    {
#ifdef PIXEL_LOCK
	flushpixels();
#endif 
	gfx_unlock_picasso ();

	//picasso96_state.HostAddress = NULL;
    }
#endif
}

#ifdef _DEBUG
void PICASSO96_Lock2( char *filename, int linenum )
#else
void PICASSO96_Lock( void )
#endif
{
#ifdef LOCK_UNLOCK_MADNESS
#if defined( P96TRACING_ENABLED ) && P96TRACING_LEVEL > 1
	// This format of output lets you double-click and jump to file/line
	write_log( "%s(%d) : calling P96 LOCK with picasso_on=%d\n", filename, linenum, picasso_on );
#endif
    if( picasso_on /*&& !picasso96_state.HostAddress*/)
    {
	//gfx_unlock_picasso(); // forces the proper flushing
	picasso96_state.HostAddress = gfx_lock_picasso ();
    }
#endif
}

#ifdef P96TRACING_ENABLED		    
/*
* Debugging dumps
*/
static void DumpModeInfoStructure (uaecptr amigamodeinfoptr)
{
    write_log ("ModeInfo Structure Dump:\n");
    write_log ("  Node.ln_Succ  = 0x%x\n", get_long (amigamodeinfoptr));
    write_log ("  Node.ln_Pred  = 0x%x\n", get_long (amigamodeinfoptr + 4));
    write_log ("  Node.ln_Type  = 0x%x\n", get_byte (amigamodeinfoptr + 8));
    write_log ("  Node.ln_Pri   = %d\n", get_byte (amigamodeinfoptr + 9));
    /*write_log ("  Node.ln_Name  = %s\n", uaememptr->Node.ln_Name); */
    write_log ("  OpenCount     = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_OpenCount));
    write_log ("  Active        = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_Active));
    write_log ("  Width         = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_Width));
    write_log ("  Height        = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_Height));
    write_log ("  Depth         = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_Depth));
    write_log ("  Flags         = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_Flags));
    write_log ("  HorTotal      = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_HorTotal));
    write_log ("  HorBlankSize  = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_HorBlankSize));
    write_log ("  HorSyncStart  = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_HorSyncStart));
    write_log ("  HorSyncSize   = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_HorSyncSize));
    write_log ("  HorSyncSkew   = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_HorSyncSkew));
    write_log ("  HorEnableSkew = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_HorEnableSkew));
    write_log ("  VerTotal      = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_VerTotal));
    write_log ("  VerBlankSize  = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_VerBlankSize));
    write_log ("  VerSyncStart  = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_VerSyncStart));
    write_log ("  VerSyncSize   = %d\n", get_word (amigamodeinfoptr + PSSO_ModeInfo_VerSyncSize));
    write_log ("  Clock         = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_first_union));
    write_log ("  ClockDivide   = %d\n", get_byte (amigamodeinfoptr + PSSO_ModeInfo_second_union));
    write_log ("  PixelClock    = %d\n", get_long (amigamodeinfoptr + PSSO_ModeInfo_PixelClock));
}

static void DumpLibResolutionStructure (uaecptr amigalibresptr)
{
    int i;
    uaecptr amigamodeinfoptr;
    struct LibResolution *uaememptr = (struct LibResolution *)get_mem_bank(amigalibresptr).xlateaddr(amigalibresptr);
    
    write_log ("LibResolution Structure Dump:\n");
    
    if (get_long (amigalibresptr + PSSO_LibResolution_DisplayID) == 0xFFFFFFFF) {
	write_log ("  Finished With LibResolutions...\n");
    } else {
	write_log ("  Name      = %s\n", uaememptr->P96ID);
	write_log ("  DisplayID = 0x%x\n", get_long (amigalibresptr + PSSO_LibResolution_DisplayID));
	write_log ("  Width     = %d\n", get_word (amigalibresptr + PSSO_LibResolution_Width));
	write_log ("  Height    = %d\n", get_word (amigalibresptr + PSSO_LibResolution_Height));
	write_log ("  Flags     = %d\n", get_word (amigalibresptr + PSSO_LibResolution_Flags));
	for (i = 0; i < MAXMODES; i++) {
	    amigamodeinfoptr = get_long (amigalibresptr + PSSO_LibResolution_Modes + i*4);
	    write_log ("  ModeInfo[%d] = 0x%x\n", i, amigamodeinfoptr);
	    if (amigamodeinfoptr)
		DumpModeInfoStructure (amigamodeinfoptr);
	}
	write_log ("  BoardInfo = 0x%x\n", get_long (amigalibresptr + PSSO_LibResolution_BoardInfo));
    }
}

v

static char binary_byte[9] = { 0,0,0,0,0,0,0,0,0 };

static char *BuildBinaryString (uae_u8 value)
{
    int i;
    for (i = 0; i < 8; i++) {
	binary_byte[i] = (value & (1 << (7 - i))) ? '#' : '.';
    }
    return binary_byte;
}

static void DumpPattern (struct Pattern *patt)
{
    uae_u8 *mem;
    int row, col;
    for (row = 0; row < (1 << patt->Size); row++) {
	mem = patt->Memory + row * 2;
	for (col = 0; col < 2; col++) {
	    write_log ("%s ", BuildBinaryString (*mem++));
	}
	write_log ("\n");
    }
}

static void DumpTemplate (struct Template *tmp, unsigned long w, unsigned long h)
{
    uae_u8 *mem = tmp->Memory;
    unsigned int row, col, width;
    width = (w + 7) >> 3;
    write_log ("xoffset = %d, bpr = %d\n", tmp->XOffset, tmp->BytesPerRow);
    for (row = 0; row < h; row++) {
	mem = tmp->Memory + row * tmp->BytesPerRow;
	for (col = 0; col < width; col++) {
	    write_log ("%s ", BuildBinaryString (*mem++));
	}
	write_log ("\n");
    }
}

static void DumpLine( struct Line *line )
{
    if( line )
    {
	write_log( "Line->X = %d\n", line->X );
	write_log( "Line->Y = %d\n", line->Y );
	write_log( "Line->Length = %d\n", line->Length );
	write_log( "Line->dX = %d\n", line->dX );
	write_log( "Line->dY = %d\n", line->dY );
	write_log( "Line->sDelta = %d\n", line->sDelta );
	write_log( "Line->lDelta = %d\n", line->lDelta );
	write_log( "Line->twoSDminusLD = %d\n", line->twoSDminusLD );
	write_log( "Line->LinePtrn = %d\n", line->LinePtrn );
	write_log( "Line->PatternShift = %d\n", line->PatternShift );
	write_log( "Line->FgPen = 0x%x\n", line->FgPen );
	write_log( "Line->BgPen = 0x%x\n", line->BgPen );
	write_log( "Line->Horizontal = %d\n", line->Horizontal );
	write_log( "Line->DrawMode = %d\n", line->DrawMode );
	write_log( "Line->Xorigin = %d\n", line->Xorigin );
	write_log( "Line->Yorigin = %d\n", line->Yorigin );
    }
}
#endif

static void ShowSupportedResolutions (void)
{
    int i = 0;
    
    write_log ("-----------------\n");
    while (DisplayModes[i].depth >= 0) {
	write_log ("%s\n", DisplayModes[i].name);
	i++;
    }
    write_log ("-----------------\n");
}

static uae_u8 GetBytesPerPixel2(uae_u32 RGBfmt, char *file, int line)
{
    static BOOL bFailure = FALSE;

    switch (RGBfmt) {
    case RGBFB_CLUT:
	return 1;
	
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
	return 4;
	
    case RGBFB_B8G8R8:
    case RGBFB_R8G8B8:
	return 3;
	
    case RGBFB_R5G5B5:
    case RGBFB_R5G6B5:
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
	return 2;
    default:
	write_log ("ERROR - GetBytesPerPixel() from %s@%d was unsuccessful with 0x%x?!\n", file, line, RGBfmt);
	if( !bFailure )
	{
	    bFailure = TRUE;
	    return GetBytesPerPixel(picasso_vidinfo.rgbformat);
	}
	else
	{
	    abort();
	}
    }
}

/*
* Amiga <-> native structure conversion functions
*/

static int CopyRenderInfoStructureA2U (uaecptr amigamemptr, struct RenderInfo *ri)
{
    uaecptr memp = get_long (amigamemptr + PSSO_RenderInfo_Memory);
    
    if (valid_address (memp, PSSO_RenderInfo_sizeof)) {
	ri->Memory = get_real_address (memp);
	ri->BytesPerRow = get_word (amigamemptr + PSSO_RenderInfo_BytesPerRow);
	ri->RGBFormat = get_long (amigamemptr + PSSO_RenderInfo_RGBFormat);
	return 1;
    }
    write_log ("ERROR - Invalid RenderInfo memory area...\n");
    return 0;
}

static int CopyPatternStructureA2U (uaecptr amigamemptr, struct Pattern *pattern)
{
    uaecptr memp = get_long (amigamemptr + PSSO_Pattern_Memory);
    if (valid_address (memp, PSSO_Pattern_sizeof)) {
	pattern->Memory = get_real_address (memp);
	pattern->XOffset = get_word (amigamemptr + PSSO_Pattern_XOffset);
	pattern->YOffset = get_word (amigamemptr + PSSO_Pattern_YOffset);
	pattern->FgPen = get_long (amigamemptr + PSSO_Pattern_FgPen);
	pattern->BgPen = get_long (amigamemptr + PSSO_Pattern_BgPen);
	pattern->Size = get_byte (amigamemptr + PSSO_Pattern_Size);
	pattern->DrawMode = get_byte (amigamemptr + PSSO_Pattern_DrawMode);
	return 1;
    }
    write_log ("ERROR - Invalid Pattern memory area...\n");
    return 0;
}

static void CopyColorIndexMappingA2U (uaecptr amigamemptr, struct ColorIndexMapping *cim)
{
    int i;
    cim->ColorMask = get_long (amigamemptr);
    for (i = 0; i < 256; i++, amigamemptr += 4)
	cim->Colors[i] = get_long (amigamemptr + 4);
}

static int CopyBitMapStructureA2U (uaecptr amigamemptr, struct BitMap *bm)
{
    int i;
    
    bm->BytesPerRow = get_word (amigamemptr + PSSO_BitMap_BytesPerRow);
    bm->Rows = get_word (amigamemptr + PSSO_BitMap_Rows);
    bm->Flags = get_byte (amigamemptr + PSSO_BitMap_Flags);
    bm->Depth = get_byte (amigamemptr + PSSO_BitMap_Depth);
    
    /* ARGH - why is THIS happening? */
    if( bm->Depth > 8 )
	bm->Depth = 8;
    
    for (i = 0; i < bm->Depth; i++) {
	uaecptr plane = get_long (amigamemptr + PSSO_BitMap_Planes + i*4);
	switch (plane) {
	case 0:
	    bm->Planes[i] = &all_zeros_bitmap;
	    break;
	case 0xFFFFFFFF:
	    bm->Planes[i] = &all_ones_bitmap;
	    break;
	default:
	    if (valid_address (plane, bm->BytesPerRow * bm->Rows))
		bm->Planes[i] = get_real_address (plane);
	    else
		return 0;
	    break;
	}
    }
    return 1;
}

static int CopyTemplateStructureA2U (uaecptr amigamemptr, struct Template *tmpl)
{
    uaecptr memp = get_long (amigamemptr + PSSO_Template_Memory);
    
    if (valid_address (memp, sizeof(struct Template))) {
	tmpl->Memory = get_real_address (memp);
	tmpl->BytesPerRow = get_word (amigamemptr + PSSO_Template_BytesPerRow);
	tmpl->XOffset = get_byte (amigamemptr + PSSO_Template_XOffset);
	tmpl->DrawMode = get_byte (amigamemptr + PSSO_Template_DrawMode);
	tmpl->FgPen = get_long (amigamemptr + PSSO_Template_FgPen);
	tmpl->BgPen = get_long (amigamemptr + PSSO_Template_BgPen);
	return 1;
    }
    write_log ("ERROR - Invalid Template memory area...\n");
    return 0;
}

static int CopyLineStructureA2U( uaecptr amigamemptr, struct Line *line )
{
    if( valid_address( amigamemptr, sizeof( struct Line ) ) )
    {
	line->X = get_word( amigamemptr + PSSO_Line_X );
	line->Y = get_word( amigamemptr + PSSO_Line_Y );
	line->Length = get_word( amigamemptr + PSSO_Line_Length );
	line->dX = get_word( amigamemptr + PSSO_Line_dX );
	line->dY = get_word( amigamemptr + PSSO_Line_dY );
	line->lDelta = get_word( amigamemptr + PSSO_Line_lDelta );
	line->sDelta = get_word( amigamemptr + PSSO_Line_sDelta );
	line->twoSDminusLD = get_word( amigamemptr + PSSO_Line_twoSDminusLD );
	line->LinePtrn = get_word( amigamemptr + PSSO_Line_LinePtrn );
	line->PatternShift = get_word( amigamemptr + PSSO_Line_PatternShift );
	line->FgPen = get_long( amigamemptr + PSSO_Line_FgPen );
	line->BgPen = get_long( amigamemptr + PSSO_Line_BgPen );
	line->Horizontal = get_word( amigamemptr + PSSO_Line_Horizontal );
	line->DrawMode = get_byte( amigamemptr + PSSO_Line_DrawMode );
	line->Xorigin = get_word( amigamemptr + PSSO_Line_Xorigin );
	line->Yorigin = get_word( amigamemptr + PSSO_Line_Yorigin );
	return 1;
    }
    write_log( "ERROR - Invalid Line structure...\n" );
    return 0;
}

static void CopyLibResolutionStructureU2A (struct LibResolution *libres, uaecptr amigamemptr)
{
    char *uaememptr = 0;
    int i;
    
    uaememptr = gfxmem_xlate (amigamemptr); /* I know that amigamemptr is inside my gfxmem chunk, so I can just do the xlate() */
    memset (uaememptr, 0, PSSO_LibResolution_sizeof); /* zero out our LibResolution structure */
    strcpy (uaememptr + PSSO_LibResolution_P96ID, libres->P96ID);
    put_long (amigamemptr + PSSO_LibResolution_DisplayID, libres->DisplayID);
    put_word (amigamemptr + PSSO_LibResolution_Width, libres->Width);
    put_word (amigamemptr + PSSO_LibResolution_Height, libres->Height);
    put_word (amigamemptr + PSSO_LibResolution_Flags, libres->Flags);
    for (i = 0; i < MAXMODES; i++)
	put_long (amigamemptr + PSSO_LibResolution_Modes + i*4, libres->Modes[i]);
#if 0
    put_long (amigamemptr, libres->Node.ln_Succ);
    put_long (amigamemptr + 4, libres->Node.ln_Pred);
    put_byte (amigamemptr + 8, libres->Node.ln_Type);
    put_byte (amigamemptr + 9, libres->Node.ln_Pri);
#endif
    put_long (amigamemptr + 10, amigamemptr + PSSO_LibResolution_P96ID);
    put_long (amigamemptr + PSSO_LibResolution_BoardInfo, libres->BoardInfo);
}

/* list is Amiga address of list, in correct endian format for UAE
* node is Amiga address of node, in correct endian format for UAE */
static void AmigaListAddTail (uaecptr list, uaecptr node)
{
    uaecptr amigamemptr = 0;
    
    if (get_long (list + 8) == list) {
	/* Empty list - set it up */
	put_long (list, node);     /* point the lh_Head to our new node */
	put_long (list + 4, 0);    /* set the lh_Tail to NULL */
	put_long (list + 8, node); /* point the lh_TailPred to our new node */
	
	/* Adjust the new node - don't rely on it being zeroed out */
	put_long (node, 0); /* ln_Succ */
	put_long (node + 4, 0); /* ln_Pred */
    } else {
	amigamemptr = get_long (list + 8); /* get the lh_TailPred contents */
	
	put_long (list + 8, node); /* point the lh_TailPred to our new node */
	
	/* Adjust the previous lh_TailPred node */
	put_long (amigamemptr, node); /* point the ln_Succ to our new node */
	
	/* Adjust the new node - don't rely on it being zeroed out */
	put_long (node, 0); /* ln_Succ */
	put_long (node + 4, amigamemptr); /* ln_Pred */
    }
}

/*
* Functions to perform an action on the real screen
*/

/*
* Fill a rectangle on the screen.  src points to the start of a line of the
* filled rectangle in the frame buffer; it can be used as a memcpy source if
* there is no OS specific function to fill the rectangle.
*/
static void do_fillrect( uae_u8 *src, unsigned int x, unsigned int y, unsigned int width, unsigned int height, uae_u32 pen, int Bpp, RGBFTYPE rgbtype )
{
    uae_u8 *dst;
   
    /* Try OS specific fillrect function here; and return if successful.  Make sure we adjust for
    * the pen values if we're doing 8-bit display-emulation on a 16-bit or higher screen. */
#ifdef PIXEL_LOCK
     flushpixels(); 
#endif	

    if( picasso_vidinfo.rgbformat == picasso96_state.RGBFormat )
    {
        if( DX_Fill( x, y, width, height, pen, rgbtype ) )
		{
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
#endif
            return;
		}
    }
    else
    {   
        if( DX_Fill( x, y, width, height, picasso_vidinfo.clut[src[0]], rgbtype ) )
		{
#ifdef LOCK_UNLOCK_MADNESS
     //PICASSO96_Unlock();
#endif
	   return;
		} 

    }
   
    P96TRACE(("P96_WARNING: do_fillrect() using fall-back routine!\n"));
    
    if( y+height > picasso_vidinfo.height )
	height = picasso_vidinfo.height - y;
    if( x+width > picasso_vidinfo.width )
	width = picasso_vidinfo.width - x;

    DX_Invalidate (y, y + height - 1);
    if (! picasso_vidinfo.extra_mem)
	{		
#ifdef LOCK_UNLOCK_MADNESS
     //PICASSO96_Unlock();
#endif
	pixelcount=0;
	   return;
		} 
    
    width *= Bpp;
#ifdef LOCK_UNLOCK_MADNESS
    PICASSO96_Lock();
    dst = picasso96_state.HostAddress;
#else
    dst = gfx_lock_picasso ();
#endif
    if (!dst)
	goto out;
    
    dst += y*picasso_vidinfo.rowbytes + x*picasso_vidinfo.pixbytes;
    if (picasso_vidinfo.rgbformat == picasso96_state.RGBFormat) 
    {
        if( Bpp == 1 )
        {
	    while (height-- > 0)
            {
		memset( dst, pen, width );
		dst += picasso_vidinfo.rowbytes;
            } 
        }
        else
        {
	    while (height-- > 0)
            {
		memcpy (dst, src, width);
		dst += picasso_vidinfo.rowbytes;
            } 
        }
    }
    else
    {
	int psiz = GetBytesPerPixel (picasso_vidinfo.rgbformat);
	if (picasso96_state.RGBFormat != RGBFB_CHUNKY)
        {
            write_log ("ERROR - do_fillrect() calling abort 1!\n");
	    abort ();
        }
	
	while (height-- > 0) 
        {
	    unsigned int i;
	    switch (psiz) 
            {
	    case 2:
		for (i = 0; i < width; i++)
		    *((uae_u16 *)dst + i) = picasso_vidinfo.clut[src[i]];
		break;
	    case 4:
		for (i = 0; i < width; i++)
		    *((uae_u32 *)dst + i) = picasso_vidinfo.clut[src[i]];
		break;
	    default:
		write_log ("ERROR - do_fillrect() calling abort 2!\n");
		abort ();			
                break;
	    }
	    dst += picasso_vidinfo.rowbytes;
	}
    }
out:;
#ifndef LOCK_UNLOCK_MADNESS
    gfx_unlock_picasso ();
#else
     PICASSO96_Unlock();
#endif    
}

/*
* This routine modifies the real screen buffer after a blit has been
* performed in the save area. If can_do_blit is nonzero, the blit can
* be performed within the real screen buffer; otherwise, this routine
* must do it by hand using the data in the frame-buffer, calculated using
* the RenderInfo data and our coordinates.
*/
static void do_blit( struct RenderInfo *ri, int Bpp, 
		    unsigned int srcx, unsigned int srcy, unsigned int dstx, unsigned int dsty,
		    unsigned int width, unsigned int height, BLIT_OPCODE opcode, int can_do_blit)
{
    uae_u8 *dstp, *srcp;
    int orig_height = height;
	
    if( picasso96_state.BigAssBitmap && can_do_blit){
	srcx=dstx;
	srcy=dsty;
	can_do_blit=0;
    } //hack to use cpu rotines for scrolling in big Screens
    
    dstx=dstx-picasso96_state.XOffset;
    dsty=dsty-picasso96_state.YOffset;
    if((int)dstx<=0){
	srcx=srcx-dstx;
	dstx=0;
    }
    if((int)dsty<=0){
	srcy=srcy-dsty;
	dsty=0;
    }

#ifdef LOCK_UNLOCK_MADNESS
#ifdef PIXEL_LOCK
	flushpixels();
#endif
    //PICASSO96_Lock();
#endif	
    /* Is our x/y origin on-screen? */
    if( dsty >= picasso_vidinfo.height )
	return;
    if( dstx >= picasso_vidinfo.width )
	return;

    /* Is our area in-range? */
    if( dsty+height >= picasso_vidinfo.height )
	height = picasso_vidinfo.height - dsty;
    if( dstx+width >= picasso_vidinfo.width )
	width = picasso_vidinfo.width - dstx;

    if (can_do_blit) 
    {
	//
	// Call OS blitting function that can do it in video memory.
	// Should return if it was successful
	//
        if( DX_Blit( srcx, srcy, dstx, dsty, width, height, opcode ) )
	    return;
    }
#ifdef LOCK_UNLOCK_MADNESS
        PICASSO96_Lock();
#endif

    srcp = ri->Memory + srcx*Bpp + srcy*ri->BytesPerRow;
    
    
    DX_Invalidate (dsty, dsty + height - 1);
    if (! picasso_vidinfo.extra_mem)
	{
    #ifdef LOCK_UNLOCK_MADNESS
    goto out;	
    #else 
    return;
    #endif	
	}

#ifdef LOCK_UNLOCK_MADNESS
    dstp = picasso96_state.HostAddress;
#else
    dstp = gfx_lock_picasso ();
#endif
    if (dstp == 0)
    {
        write_log ("WARNING: do_blit() couldn't lock\n");
	goto out;
    }
    
    /* The areas can't overlap: the source is always in the Picasso frame buffer,
    * and the destination is a different buffer owned by the graphics code.  */
    dstp += dsty * picasso_vidinfo.rowbytes + dstx * picasso_vidinfo.pixbytes;
    P96TRACE(("do_blit with srcp 0x%x, dstp 0x%x, dst_rowbytes %d, srcx %d, srcy %d, dstx %d, dsty %d, w %d, h %d, dst_pixbytes %d\n",
        srcp, dstp, picasso_vidinfo.rowbytes, srcx, srcy, dstx, dsty, width, height, picasso_vidinfo.pixbytes));
    P96TRACE(("gfxmem is at 0x%x\n",gfxmemory));
	
    if (picasso_vidinfo.rgbformat == picasso96_state.RGBFormat) 
    {
        P96TRACE(("do_blit type-a\n"));
        width *= Bpp;
        while (height-- > 0) 
        {
            memcpy (dstp, srcp, width);
            srcp += ri->BytesPerRow;
            dstp += picasso_vidinfo.rowbytes;
        }
    }
    else
    {
        int psiz = GetBytesPerPixel (picasso_vidinfo.rgbformat);
	P96TRACE(("do_blit type-b\n"));
        if (picasso96_state.RGBFormat != RGBFB_CHUNKY)
        {
            write_log ("ERROR: do_blit() calling abort 1!\n");
            abort ();
        }
        while (height-- > 0) 
        {
	    unsigned int i;
	    switch (psiz) 
            {
	    case 2:
		for (i = 0; i < width; i++)
		    *((uae_u16 *)dstp + i) = picasso_vidinfo.clut[srcp[i]];
		break;
	    case 4:
		for (i = 0; i < width; i++)
		    *((uae_u32 *)dstp + i) = picasso_vidinfo.clut[srcp[i]];
		break;
	    default:
		write_log ("ERROR - do_blit() calling abort 2!\n");
		abort ();
                break;
	    }
	    srcp += ri->BytesPerRow;
	    dstp += picasso_vidinfo.rowbytes;
	}
    }
    out:
#ifndef LOCK_UNLOCK_MADNESS
    gfx_unlock_picasso ();
#else
    PICASSO96_Unlock();
#endif
    ;
}

/*
* Invert a rectangle on the screen.  a render-info is given,
* so that do_blit can be used if
* there is no OS specific function to invert the rectangle.
*/
static void do_invertrect( struct RenderInfo *ri, int Bpp, int x, int y, int width, int height)
{
   /* if( DX_InvertRect( x, y, width, height ) )
	return;*/  //deactivate in 0.8.20 
    P96TRACE(("do_invertrect falling back to do_blit!\n"));
    do_blit (ri, Bpp, x, y, x, y, width, height, BLIT_SRC, 0);
}
		    
static uaecptr wgfx_linestart;
static uaecptr wgfx_lineend;
static uaecptr wgfx_min, wgfx_max;
static unsigned long wgfx_y;

static void wgfx_do_flushline (void)
{
    uae_u8 *src, *dstp;
	
    /* Mark these lines as "dirty" */
    DX_Invalidate (wgfx_y, wgfx_y);

    if (! picasso_vidinfo.extra_mem) /* The "out" will flush the dirty lines directly */
	goto out;
    
#ifdef LOCK_UNLOCK_MADNESS
    dstp = picasso96_state.HostAddress;
	
#else
    dstp = gfx_lock_picasso ();
#endif
     if (dstp == 0)
	goto out;
#if P96TRACING_LEVEL > 0
    P96TRACE(("flushing %d\n", wgfx_y));
#endif
    src = gfxmemory + wgfx_min;
    
    if( picasso_vidinfo.rgbformat == picasso96_state.RGBFormat )
    {
#if P96TRACING_LEVEL > 0
	P96TRACE(("flushing type-a\n"));
#endif
	dstp += wgfx_y * picasso_vidinfo.rowbytes + wgfx_min - wgfx_linestart;
		memcpy (dstp, src, wgfx_max - wgfx_min);
    }
    else
    {
	int width = wgfx_max - wgfx_min;
	int i;
	int psiz = GetBytesPerPixel (picasso_vidinfo.rgbformat);
	P96TRACE(("flushing type-b\n"));
	if (picasso96_state.RGBFormat != RGBFB_CHUNKY)
	{
	    write_log ("ERROR - wgfx_do_flushline() calling abort 1!\n");
	    abort ();
	}
	
	dstp += wgfx_y * picasso_vidinfo.rowbytes + (wgfx_min - wgfx_linestart) * psiz;
	switch (psiz) {
	case 2:
	    for (i = 0; i < width; i++)
		*((uae_u16 *)dstp + i) = picasso_vidinfo.clut[src[i]];
	    break;
	case 4:
	    for (i = 0; i < width; i++)
		*((uae_u32 *)dstp + i) = picasso_vidinfo.clut[src[i]];
	    break;
	default:
	    write_log ("ERROR - wgfx_do_flushline() calling abort 2!\n");
	    abort ();			
	    break;
	}
    }
    
out:
#ifndef LOCK_UNLOCK_MADNESS
    gfx_unlock_picasso ();
#endif
	
    wgfx_linestart = 0xFFFFFFFF;
}

STATIC_INLINE void wgfx_flushline (void)
{
    if (wgfx_linestart == 0xFFFFFFFF || ! picasso_on)
	return;
    wgfx_do_flushline ();
}

static int renderinfo_is_current_screen (struct RenderInfo *ri)
{
    if (! picasso_on)
	return 0;
    if (ri->Memory != gfxmemory + (picasso96_state.Address - gfxmem_start))
	return 0;
    
    return 1;
}

/*
* Fill a rectangle in the screen.
*/
STATIC_INLINE void do_fillrect_frame_buffer( struct RenderInfo *ri, int X, int Y, int Width, int Height, uae_u32 Pen, int Bpp, RGBFTYPE RGBFormat )
{
    int cols;
    uae_u8 *start, *oldstart;
    uae_u8 *src, *dst;
    int lines;

    /* Do our virtual frame-buffer memory.  First, we do a single line fill by hand */
    oldstart = start = src = ri->Memory + X*Bpp + Y*ri->BytesPerRow;
    
    switch (Bpp) 
    {
    case 1:
	memset (start, Pen, Width);
        break;
    case 2:
	for (cols = 0; cols < Width; cols++) 
	{
	    do_put_mem_word ((uae_u16 *)start, (uae_u16)Pen);
	    start += 2;
	}
        break;
    case 3:
	for (cols = 0; cols < Width; cols++) 
	{
	    do_put_mem_byte (start, (uae_u8)Pen);
	    start++;
	    *(uae_u16 *)(start) = (Pen & 0x00FFFF00) >> 8;
	    start+=2;
	}
        break;
    case 4:
	for (cols = 0; cols < Width; cols++) 
	{
	    /**start = Pen; */
	    do_put_mem_long ((uae_u32 *)start, Pen);
	    start += 4;
	}
        break;
    }
    src = oldstart;
    dst = src + ri->BytesPerRow;
    /* next, we do the remaining line fills via memcpy() for > 1 BPP, otherwise some more memset() calls */
    if( Bpp > 1 )
    {
	for (lines = 0; lines < (Height - 1); lines++, dst += ri->BytesPerRow)
	    memcpy (dst, src, Width * Bpp);
    }
    else
    {
	for (lines = 0; lines < (Height - 1); lines++, dst += ri->BytesPerRow)
	    memset( dst, Pen, Width );
    }
}

void picasso_handle_vsync (void)
{
    DX_Invalidate(1,4000);      //so a flushpixel is done every vsync if pixel are in buffer
    PICASSO96_Unlock();
    if (palette_changed) {
        DX_SetPalette (0,256);
        palette_changed = 0;
    }
}

static int set_panning_called = 0;

/* Clear our screen, since we've got a new Picasso screen-mode, and refresh with the proper contents
* This is called on several occasions:
* 1. Amiga-->Picasso transition, via SetSwitch()
* 2. Picasso-->Picasso transition, via SetPanning().
* 3. whenever the graphics code notifies us that the screen contents have been lost.
*/
extern unsigned int new_beamcon0;
void picasso_refresh( int call_setpalette )
{
    struct RenderInfo ri;
   
    if (! picasso_on)
	return;
    {  //for higher P96 mousedraw rate
	/* HACK */
	extern uae_u16 vtotal;
	if (p96hack_vpos2){
	    vtotal=p96hack_vpos2;
	    new_beamcon0 |= 0x80;
	    p96refresh_active=1;
	} else new_beamcon0 |= 0x20;
		/* HACK until ntsc timing is fixed.. */
    } //end for higher P96 mousedraw rate
	
    

    have_done_picasso = 1;
    
    /* Make sure that the first time we show a Picasso video mode, we don't blit any crap.
    * We can do this by checking if we have an Address yet.  */
    if (picasso96_state.Address) {
	unsigned int width, height;

	/* blit the stuff from our static frame-buffer to the gfx-card */
	ri.Memory = gfxmemory + (picasso96_state.Address - gfxmem_start);
	ri.BytesPerRow = picasso96_state.BytesPerRow;
	ri.RGBFormat = picasso96_state.RGBFormat;

	if( set_panning_called )
	{
	    width = ( picasso96_state.VirtualWidth < picasso96_state.Width ) ?
		picasso96_state.VirtualWidth : picasso96_state.Width;
	    height = ( picasso96_state.VirtualHeight < picasso96_state.Height ) ?
		picasso96_state.VirtualHeight : picasso96_state.Height;
	    // Let's put a black-border around the case where we've got a sub-screen...
	    if( !picasso96_state.BigAssBitmap )
	    {
		if (picasso96_state.XOffset || picasso96_state.YOffset)
		    DX_Fill( 0, 0, picasso96_state.Width, picasso96_state.Height, 0,
			picasso96_state.RGBFormat );
	    }
	}
	else
	{
	    width = picasso96_state.Width;
	    height = picasso96_state.Height;
	}
	do_blit(&ri, picasso96_state.BytesPerPixel, 0, 0, 0, 0, width, height, BLIT_SRC, 0);
    } 
    else
    {
	write_log ("ERROR - picasso_refresh() can't refresh!\n");
    }
}


/*
* Functions to perform an action on the frame-buffer
*/
STATIC_INLINE void do_blitrect_frame_buffer( struct RenderInfo *ri, struct
RenderInfo *dstri, unsigned long srcx, unsigned long srcy,
    unsigned long dstx, unsigned long dsty, unsigned long width, unsigned
long height, uae_u8 mask, BLIT_OPCODE opcode )
{
    
    uae_u8 *src, *dst, *tmp, *tmp2, *tmp3;
    uae_u8 Bpp = GetBytesPerPixel(ri->RGBFormat);
    unsigned long total_width = width * Bpp;
    unsigned long linewidth = (total_width + 15) & ~15;
    unsigned long lines;
    int can_do_visible_blit = 0;

    src = ri->Memory + srcx*Bpp + srcy*ri->BytesPerRow;
    dst = dstri->Memory + dstx*Bpp + dsty*dstri->BytesPerRow;
    if (mask != 0xFF && Bpp > 1)
    {
    write_log ("WARNING - BlitRect() has mask 0x%x with Bpp %d.\n", mask,
Bpp);
    }
    
    if (mask == 0xFF || Bpp > 1) 
    {
    if( opcode == BLIT_SRC )
    {
        /* handle normal case efficiently */
        if (ri->Memory == dstri->Memory && dsty == srcy) 
        {
        unsigned long i;
        for (i = 0; i < height; i++, src += ri->BytesPerRow, dst +=
dstri->BytesPerRow)
            memmove (dst, src, total_width);
        }
        else if (dsty < srcy) 
        {
        unsigned long i;
        for (i = 0; i < height; i++, src += ri->BytesPerRow, dst +=
dstri->BytesPerRow)
            memcpy (dst, src, total_width);
        }
        else
        {
        unsigned long i;
        src += (height-1) * ri->BytesPerRow;
        dst += (height-1) * dstri->BytesPerRow;
        for (i = 0; i < height; i++, src -= ri->BytesPerRow, dst -=
dstri->BytesPerRow)
            memcpy (dst, src, total_width);
        }
        return;
    }
    else
    {
        uae_u8 *src2 = src;
        uae_u8 *dst2 = dst;
        uae_u32 *src2_32 = (uae_u32*)src;
        uae_u32 *dst2_32 = (uae_u32*)dst;
        unsigned int y;
        
        for( y = 0; y < height; y++ ) /* Vertical lines */
        {
        int bound;
        bound = src + total_width - 4;
        //copy now the longs
        for( src2_32 = src, dst2_32 = dst; src2_32 < bound; src2_32++,
dst2_32++ ) /* Horizontal bytes */
        {
            switch( opcode )
            {
            case BLIT_FALSE:
            *dst2_32 = 0;
            break;
            case BLIT_NOR:
            *dst2_32 = ~(*src2_32 | *dst2_32);
            break;
            case BLIT_ONLYDST:
            *dst2_32 = *dst2_32 & ~(*src2_32);
            break;
            case BLIT_NOTSRC:
            *dst2_32 = ~(*src2_32);
            break;
            case BLIT_ONLYSRC:
            *dst2_32 = *src2_32 & ~(*dst2_32);
            break;
            case BLIT_NOTDST:
            *dst2_32 = ~(*dst2_32);
            break;
            case BLIT_EOR:
            *dst2_32 = *src2_32 ^ *dst2_32;
            break;
            case BLIT_NAND:
            *dst2_32 = ~(*src2_32 & *dst2_32);
            break;
            case BLIT_AND:
            *dst2_32 = *src2_32 & *dst2_32;
            break;
            case BLIT_NEOR:
            *dst2_32 = ~(*src2_32 ^ *dst2_32);
            break;
            case BLIT_DST:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_DST!\n"
);
            break;
            case BLIT_NOTONLYSRC:
            *dst2_32 = ~(*src2_32) | *dst2_32;
            break;
            case BLIT_SRC:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_SRC!\n"
);
            break;
            case BLIT_NOTONLYDST:
            *dst2_32 = ~(*dst2_32) | *src2_32;
            break;
            case BLIT_OR:
            *dst2_32 = *src2_32 | *dst2_32;
            break;
            case BLIT_TRUE:
            *dst2_32 = 0xFFFFFFFF;
            break;
            case BLIT_LAST:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_LAST!\n"
);
            break;
            } /* switch opcode */
        }// for end
        //now copy the rest few bytes
        for( src2 = src2_32, dst2 = dst2_32; src2 < src + total_width;
src2++, dst2++ ) /* Horizontal bytes */
        {
            switch( opcode )
            {
            case BLIT_FALSE:
            *dst2 = 0;
            break;
            case BLIT_NOR:
            *dst2 = ~(*src2 | *dst2);
            break;
            case BLIT_ONLYDST:
            *dst2 = *dst2 & ~(*src2);
            break;
            case BLIT_NOTSRC:
            *dst2 = ~(*src2);
            break;
            case BLIT_ONLYSRC:
            *dst2 = *src2 & ~(*dst2);
            break;
            case BLIT_NOTDST:
            *dst2 = ~(*dst2);
            break;
            case BLIT_EOR:
            *dst2 = *src2 ^ *dst2;
            break;
            case BLIT_NAND:
            *dst2 = ~(*src2 & *dst2);
            break;
            case BLIT_AND:
            *dst2 = *src2 & *dst2;
            break;
            case BLIT_NEOR:
            *dst2 = ~(*src2 ^ *dst2);
            break;
            case BLIT_DST:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_DST!\n"
);
            break;
            case BLIT_NOTONLYSRC:
            *dst2 = ~(*src2) | *dst2;
            break;
            case BLIT_SRC:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_SRC!\n"
);
            break;
            case BLIT_NOTONLYDST:
            *dst2 = ~(*dst2) | *src2;
            break;
            case BLIT_OR:
            *dst2 = *src2 | *dst2;
            break;
            case BLIT_TRUE:
            *dst2 = 0xFF;
            break;
            case BLIT_LAST:
            write_log( "do_blitrect_frame_buffer shouldn't get BLIT_LAST!\n"
);
            break;
            } /* switch opcode */
        } /* for width */
        src += ri->BytesPerRow;
        dst += dstri->BytesPerRow;
        } /* for height */
    }
    return;
        }
    
    tmp3 = tmp2 = tmp = xmalloc (linewidth * height); /* allocate enough
memory for the src-rect */
    if (!tmp)
    return;
    
    /* copy the src-rect into our temporary buffer space */
    for (lines = 0; lines < height; lines++, src += ri->BytesPerRow, tmp2 +=
linewidth) 
    {
    memcpy (tmp2, src, total_width);
    }
    
    /* copy the temporary buffer to the destination */
    for (lines = 0; lines < height; lines++, dst += dstri->BytesPerRow, tmp
+= linewidth) 
    {
    unsigned long cols;
    for (cols = 0; cols < width; cols++) 
    {
        dst[cols] &= ~mask;
        dst[cols] |= tmp[cols] & mask;
    }
    }
    /* free the temp-buf */
    free (tmp3);
} 

#if 0
/*
* Functions to perform an action on the frame-buffer
*/
STATIC_INLINE void do_blitrect_frame_buffer( struct RenderInfo *ri, struct RenderInfo *dstri, unsigned long srcx, unsigned long srcy,
    unsigned long dstx, unsigned long dsty, unsigned long width, unsigned long height, uae_u8 mask, BLIT_OPCODE opcode )
{
    
    uae_u8 *src, *dst, *tmp, *tmp2, *tmp3;
    uae_u8 Bpp = GetBytesPerPixel(ri->RGBFormat);
    unsigned long total_width = width * Bpp;
    unsigned long linewidth = (total_width + 15) & ~15;
    unsigned long lines;
    int can_do_visible_blit = 0;

    src = ri->Memory + srcx*Bpp + srcy*ri->BytesPerRow;
    dst = dstri->Memory + dstx*Bpp + dsty*dstri->BytesPerRow;
    if (mask != 0xFF && Bpp > 1)
    {
	write_log ("WARNING - BlitRect() has mask 0x%x with Bpp %d.\n", mask, Bpp);
    }
    
    if (mask == 0xFF || Bpp > 1) 
    {
	if( opcode == BLIT_SRC )
	{
	    /* handle normal case efficiently */
	    if (ri->Memory == dstri->Memory && dsty == srcy) 
	    {
		unsigned long i;
		for (i = 0; i < height; i++, src += ri->BytesPerRow, dst += dstri->BytesPerRow)
		    memmove (dst, src, total_width);
	    }
	    else if (dsty < srcy) 
	    {
		unsigned long i;
		for (i = 0; i < height; i++, src += ri->BytesPerRow, dst += dstri->BytesPerRow)
		    memcpy (dst, src, total_width);
	    }
	    else
	    {
		unsigned long i;
		src += (height-1) * ri->BytesPerRow;
		dst += (height-1) * dstri->BytesPerRow;
		for (i = 0; i < height; i++, src -= ri->BytesPerRow, dst -= dstri->BytesPerRow)
		    memcpy (dst, src, total_width);
	    }
	    return;
	}
	else
	{
	    uae_u8 *src2 = src;
	    uae_u8 *dst2 = dst;
	    unsigned int y;
	    
	    for( y = 0; y < height; y++ ) /* Vertical lines */
	    {
		for( src2 = src, dst2 = dst; src2 < src + total_width; src2++, dst2++ ) /* Horizontal bytes */
		{
		    switch( opcode )
		    {
		    case BLIT_FALSE:
			*dst2 = 0;
			break;
		    case BLIT_NOR:
			*dst2 = ~(*src2 | *dst2);
			break;
		    case BLIT_ONLYDST:
			*dst2 = *dst2 & ~(*src2);
			break;
		    case BLIT_NOTSRC:
			*dst2 = ~(*src2);
			break;
		    case BLIT_ONLYSRC:
			*dst2 = *src2 & ~(*dst2);
			break;
		    case BLIT_NOTDST:
			*dst2 = ~(*dst2);
			break;
		    case BLIT_EOR:
			*dst2 = *src2 ^ *dst2;
			break;
		    case BLIT_NAND:
			*dst2 = ~(*src2 & *dst2);
			break;
		    case BLIT_AND:
			*dst2 = *src2 & *dst2;
			break;
		    case BLIT_NEOR:
			*dst2 = ~(*src2 ^ *dst2);
			break;
		    case BLIT_DST:
			write_log( "do_blitrect_frame_buffer shouldn't get BLIT_DST!\n" );
			break;
		    case BLIT_NOTONLYSRC:
			*dst2 = ~(*src2) | *dst2;
			break;
		    case BLIT_SRC:
			write_log( "do_blitrect_frame_buffer shouldn't get BLIT_SRC!\n" );
			break;
		    case BLIT_NOTONLYDST:
			*dst2 = ~(*dst2) | *src2;
			break;
		    case BLIT_OR:
			*dst2 = *src2 | *dst2;
			break;
		    case BLIT_TRUE:
			*dst2 = 0xFF;
			break;
		    case BLIT_LAST:
			write_log( "do_blitrect_frame_buffer shouldn't get BLIT_LAST!\n" );
			break;
		    } /* switch opcode */
		} /* for width */
		src += ri->BytesPerRow;
		dst += dstri->BytesPerRow;
	    } /* for height */
	}
	return;
        }
    
    tmp3 = tmp2 = tmp = xmalloc (linewidth * height); /* allocate enough memory for the src-rect */
    if (!tmp)
	return;
    
    /* copy the src-rect into our temporary buffer space */
    for (lines = 0; lines < height; lines++, src += ri->BytesPerRow, tmp2 += linewidth) 
    {
	memcpy (tmp2, src, total_width);
    }
    
    /* copy the temporary buffer to the destination */
    for (lines = 0; lines < height; lines++, dst += dstri->BytesPerRow, tmp += linewidth) 
    {
	unsigned long cols;
	for (cols = 0; cols < width; cols++) 
	{
	    dst[cols] &= ~mask;
	    dst[cols] |= tmp[cols] & mask;
	}
    }
    /* free the temp-buf */
    free (tmp3);
}
#endif

/*
DrawLine: 
Synopsis: DrawLine(bi, ri, line, Mask, RGBFormat); 
Inputs: a0: struct BoardInfo *bi 
a1: struct RenderInfo *ri 
a2: struct Line *line 
d0.b: Mask 
d7.l: RGBFormat 

This function is used to paint a line on the board memory possibly using the blitter. It is called by Draw
and obeyes the destination RGBFormat as well as ForeGround and BackGround pens and draw modes. 
*/
uae_u32 picasso_DrawLine (void)
{
    uae_u32 result = 0;
#ifdef P96_DRAWLINE
    struct Line line;
    struct RenderInfo ri;
    uae_u8 Mask = m68k_dreg( regs, 0 );
    RGBFTYPE RGBFormat = m68k_dreg( regs, 7 );

    CopyRenderInfoStructureA2U( m68k_areg( regs, 1 ), &ri );
    CopyLineStructureA2U( m68k_areg( regs, 2 ), &line );
#if defined( P96TRACING_ENABLED ) && P96TRACING_LEVEL > 0
    DumpLine( &line );
#endif
#else
    P96TRACE(("DrawLine() - not implemented!\n" ));
#endif
    return result;
}

#ifdef HARDWARE_SPRITE_EMULATION
/*
SetSprite: 
Synopsis: SetSprite(bi, activate, RGBFormat); 
Inputs: a0: struct BoardInfo *bi 
d0: BOOL activate 
d7: RGBFTYPE RGBFormat 

This function activates or deactivates the hardware sprite. 
*/
uae_u32 picasso_SetSprite (void)
{
    uae_u32 result = 0;
    uae_u32 activate = m68k_dreg( regs, 0 );
    result = DX_ShowCursor( activate );
    write_log ("SetSprite() - trying to %s cursor, result = %d\n", activate ? "show":"hide", result);
    return result;
}

/*
SetSpritePosition: 
Synopsis: SetSpritePosition(bi, RGBFormat); 
Inputs: a0: struct BoardInfo *bi 
d7: RGBFTYPE RGBFormat 

This function sets the hardware mouse sprite position according to the values in the BoardInfo structure.
MouseX and MouseY are the coordinates relative to the screen bitmap. XOffset and YOffset must be subtracted
to account for possible screen panning. 
*/
uae_u32 picasso_SetSpritePosition (void)
{
    uae_u32 result = 0;
    uaecptr bi = m68k_areg( regs, 0 );
    uae_u16 MouseX  = get_word( bi + PSSO_BoardInfo_MouseX ) - picasso96_state.XOffset;
    uae_u16 MouseY  = get_word( bi + PSSO_BoardInfo_MouseY ) - picasso96_state.YOffset;
    
    // Keep these around, because we don't want flickering
    static uae_u16 OldMouseX = -1;
    static uae_u16 OldMouseY = -1;

    // Bounds check MouseX and MouseY here, because sometimes they seem to go negative...
    if( (uae_s16)MouseX < 0 )
	MouseX = 0;
    if( (uae_s16)MouseY < 0 )
	MouseY = 0;

    if( ( MouseX != OldMouseX ) || ( MouseY != OldMouseY ) )
    {
	result = DX_MoveCursor( MouseX, MouseY );
	write_log ("SetSpritePosition() - moving cursor to (%d,%d), result = %d\n", MouseX, MouseY, result);
	if( result )
	{
	    OldMouseX = MouseX;
	    OldMouseY = MouseY;
	}
    }
    return result;
}

/*
SetSpriteImage: 
Synopsis: SetSpriteImage(bi, RGBFormat); 
Inputs: a0: struct BoardInfo *bi 
d7: RGBFTYPE RGBFormat 

This function gets new sprite image data from the MouseImage field of the BoardInfo structure and writes
it to the board.

There are three possible cases: 

BIB_HIRESSPRITE is set:
skip the first two long words and the following sprite data is arranged as an array of two longwords. Those form the
two bit planes for one image line respectively. 

BIB_HIRESSPRITE and BIB_BIGSPRITE are not set:
skip the first two words and the following sprite data is arranged as an array of two words. Those form the two
bit planes for one image line respectively. 

BIB_HIRESSPRITE is not set and BIB_BIGSPRITE is set:
skip the first two words and the following sprite data is arranged as an array of two words. Those form the two bit
planes for one image line respectively. You have to double each pixel horizontally and vertically. All coordinates
used in this case already assume a zoomed sprite, only the sprite data is not zoomed yet. You will have to
compensate for this when accounting for hotspot offsets and sprite dimensions. 
*/
uae_u32 picasso_SetSpriteImage (void)
{
    uae_u32 result = 0;

    return result;
}

/*
SetSpriteColor: 
Synopsis: SetSpriteColor(bi, index, red, green, blue, RGBFormat); 
Inputs: a0: struct BoardInfo *bi 
d0.b: index 
d1.b: red 
d2.b: green 
d3.b: blue 
d7: RGBFTYPE RGBFormat 

This function changes one of the possible three colors of the hardware sprite. 
*/
uae_u32 picasso_SetSpriteColor (void)
{
    uae_u32 result = 0;

    return result;
}
#endif

/*
* BOOL FindCard(struct BoardInfo *bi);       and
*
* FindCard is called in the first stage of the board initialisation and
* configuration and is used to look if there is a free and unconfigured
* board of the type the driver is capable of managing. If it finds one,
* it immediately reserves it for use by Picasso96, usually by clearing
* the CDB_CONFIGME bit in the flags field of the ConfigDev struct of
* this expansion card. But this is only a common example, a driver can
* do whatever it wants to mark this card as used by the driver. This
* mechanism is intended to ensure that a board is only configured and
* used by one driver. FindBoard also usually fills some fields of the
* BoardInfo struct supplied by the caller, the rtg.library, for example
* the MemoryBase, MemorySize and RegisterBase fields.
*/
uae_u32 picasso_FindCard (void)
{
    uaecptr AmigaBoardInfo = m68k_areg (regs, 0);

    /* NOTES: See BoardInfo struct definition in Picasso96 dev info */
    
    if (allocated_gfxmem && !picasso96_state.CardFound) {
	/* Fill in MemoryBase, MemorySize */
	put_long (AmigaBoardInfo + PSSO_BoardInfo_MemoryBase, gfxmem_start);
	/* size of memory, minus a 32K chunk: 16K for pattern bitmaps, 16K for resolution list */
	put_long (AmigaBoardInfo + PSSO_BoardInfo_MemorySize, allocated_gfxmem - 32768);
	picasso96_state.CardFound = 1; /* mark our "card" as being found */
	return -1;
    } else
	return 0;
}

static void FillBoardInfo (uaecptr amigamemptr, struct LibResolution *res, struct PicassoResolution *dm)
{
    char *uaememptr;
    switch (dm->depth) {
    case 1:
	res->Modes[CHUNKY] = amigamemptr;
	break;
    case 2:
	res->Modes[HICOLOR] = amigamemptr;
	break;
    case 3:
	res->Modes[TRUECOLOR] = amigamemptr;
	break;
    default:
	res->Modes[TRUEALPHA] = amigamemptr;
	break;
    }
    uaememptr = gfxmem_xlate(amigamemptr); /* I know that amigamemptr is inside my gfxmem chunk, so I can just do the xlate() */
    memset(uaememptr, 0, PSSO_ModeInfo_sizeof); /* zero out our ModeInfo struct */
    
    put_word (amigamemptr + PSSO_ModeInfo_Width, dm->res.width);
    put_word (amigamemptr + PSSO_ModeInfo_Height, dm->res.height);
    put_byte (amigamemptr + PSSO_ModeInfo_Depth, dm->depth * 8);
    put_byte (amigamemptr + PSSO_ModeInfo_Flags, 0);
    put_word (amigamemptr + PSSO_ModeInfo_HorTotal, dm->res.width);
    put_word (amigamemptr + PSSO_ModeInfo_HorBlankSize, 0);
    put_word (amigamemptr + PSSO_ModeInfo_HorSyncStart, 0);
    put_word (amigamemptr + PSSO_ModeInfo_HorSyncSize, 0);
    put_byte (amigamemptr + PSSO_ModeInfo_HorSyncSkew, 0);
    put_byte (amigamemptr + PSSO_ModeInfo_HorEnableSkew, 0);
    
    put_word (amigamemptr + PSSO_ModeInfo_VerTotal, dm->res.height);
    put_word (amigamemptr + PSSO_ModeInfo_VerBlankSize, 0);
    put_word (amigamemptr + PSSO_ModeInfo_VerSyncStart, 0);
    put_word (amigamemptr + PSSO_ModeInfo_VerSyncSize, 0);
    
    put_byte (amigamemptr + PSSO_ModeInfo_first_union, 98);
    put_byte (amigamemptr + PSSO_ModeInfo_second_union, 14);
    
    put_long (amigamemptr + PSSO_ModeInfo_PixelClock, dm->res.width * dm->res.height * (currprefs.gfx_refreshrate ? currprefs.gfx_refreshrate : default_freq));
}

static int AssignModeID( int i, int count )
{
    int result;
    if( DisplayModes[i].res.width == 320 && DisplayModes[i].res.height == 200 )
        result = 0x50001000;
    else  if( DisplayModes[i].res.width == 320 && DisplayModes[i].res.height == 240 )
        result = 0x50011000;
    else if( DisplayModes[i].res.width == 640 && DisplayModes[i].res.height == 400 )
        result = 0x50021000;
    else if( DisplayModes[i].res.width == 640 && DisplayModes[i].res.height == 480 )
	result = 0x50031000;
    else if( DisplayModes[i].res.width == 800 && DisplayModes[i].res.height == 600 )
	result = 0x50041000;
    else if( DisplayModes[i].res.width == 1024 && DisplayModes[i].res.height == 768 )
        result = 0x50051000;
    else if( DisplayModes[i].res.width == 1152 && DisplayModes[i].res.height == 864 )
        result = 0x50061000;
    else if( DisplayModes[i].res.width == 1280 && DisplayModes[i].res.height == 1024 )
        result = 0x50071000;
    else if( DisplayModes[i].res.width == 1600 && DisplayModes[i].res.height == 1280 )
        result = 0x50081000;
    else
	result = 0x50091000 + count * 0x10000;
    return result;
}

/****************************************
* InitCard()
*
* a2: BoardInfo structure ptr - Amiga-based address in Intel endian-format
*
* Job - fill in the following structure members:
* gbi_RGBFormats: the pixel formats that the host-OS of UAE supports
*     If UAE is running in a window, it should ONLY report the pixel format of the host-OS desktop
*     If UAE is running full-screen, it should report ALL pixel formats that the host-OS can handle in full-screen
*     NOTE: If full-screen, and the user toggles to windowed-mode, all hell will break loose visually.  Must inform
*           user that they're doing something stupid (unless their desktop and full-screen colour modes match).
* gbi_SoftSpriteFlags: should be the same as above for now, until actual cursor support is added
* gbi_BitsPerCannon: could be 6 or 8 or ???, depending on the host-OS gfx-card
* gbi_MaxHorResolution: fill this in for all modes (even if you don't support them)
* gbi_MaxVerResolution: fill this in for all modes (even if you don't support them)
*/
uae_u32 picasso_InitCard (void)
{
    struct LibResolution res;
    int i;
    int ModeInfoStructureCount = 1, LibResolutionStructureCount = 0;
    uaecptr amigamemptr = 0;
    uaecptr AmigaBoardInfo = m68k_areg (regs, 2);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_BitsPerCannon, DX_BitsPerCannon());
    put_word (AmigaBoardInfo + PSSO_BoardInfo_RGBFormats, picasso96_pixel_format);
    put_long (AmigaBoardInfo + PSSO_BoardInfo_BoardType, BT_uaegfx);
#ifdef HARDWARE_SPRITE_EMULATION
    put_word (AmigaBoardInfo + PSSO_BoardInfo_SoftSpriteFlags, 0);
#else
    put_word (AmigaBoardInfo + PSSO_BoardInfo_SoftSpriteFlags, picasso96_pixel_format);
#endif
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + 0, planar.width);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + 2, chunky.width);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + 4, hicolour.width);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + 6, truecolour.width);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxHorResolution + 8, alphacolour.width);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + 0, planar.height);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + 2, chunky.height);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + 4, hicolour.height);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + 6, truecolour.height);
    put_word (AmigaBoardInfo + PSSO_BoardInfo_MaxVerResolution + 8, alphacolour.height);
    
    i = 0;
    while (DisplayModes[i].depth >= 0) {    
	int j = i;
	/* Add a LibResolution structure to the ResolutionsList MinList in our BoardInfo */
	res.DisplayID = AssignModeID( i, LibResolutionStructureCount );
	res.BoardInfo = AmigaBoardInfo;
	res.Width = DisplayModes[i].res.width;
	res.Height = DisplayModes[i].res.height;
	res.Flags = P96F_PUBLIC;
	memcpy (res.P96ID, "P96-0:", 6);
	sprintf (res.Name, "uaegfx:%dx%d", res.Width, res.Height);
	res.Modes[PLANAR] = 0;
	res.Modes[CHUNKY] = 0;
	res.Modes[HICOLOR] = 0;
	res.Modes[TRUECOLOR] = 0;
	res.Modes[TRUEALPHA] = 0;
	
	do {
	    /* Handle this display mode's depth */
	    
	    /* New: Only add the modes when there is enough P96 RTG memory to hold the bitmap */
	    if( ( allocated_gfxmem - 32768 ) >
		( DisplayModes[i].res.width * DisplayModes[i].res.height * DisplayModes[i].depth ) )
	    {
		amigamemptr = gfxmem_start + allocated_gfxmem - (PSSO_ModeInfo_sizeof * ModeInfoStructureCount++);
		FillBoardInfo(amigamemptr, &res, &DisplayModes[i]);
	    }
	    i++;
	} while (DisplayModes[i].depth >= 0
	    && DisplayModes[i].res.width == DisplayModes[j].res.width
	    && DisplayModes[i].res.height == DisplayModes[j].res.height);
	
	amigamemptr = gfxmem_start + allocated_gfxmem - 16384 + (PSSO_LibResolution_sizeof * LibResolutionStructureCount++);
	CopyLibResolutionStructureU2A (&res, amigamemptr);
#if defined P96TRACING_ENABLED && P96TRACING_LEVEL > 1
	DumpLibResolutionStructure( amigamemptr);
#endif
	AmigaListAddTail (AmigaBoardInfo + PSSO_BoardInfo_ResolutionsList, amigamemptr);
    }
    
    return -1;
}

extern int x_size, y_size;

/*
* SetSwitch:
* a0:	struct BoardInfo
* d0.w:	BOOL state
* this function should set a board switch to let the Amiga signal pass
* through when supplied with a 0 in d0 and to show the board signal if
* a 1 is passed in d0. You should remember the current state of the
* switch to avoid unneeded switching. If your board has no switch, then
* simply supply a function that does nothing except a RTS.
*
* NOTE: Return the opposite of the switch-state. BDK
*/
uae_u32 picasso_SetSwitch (void)
{
    uae_u16 flag = m68k_dreg (regs, 0) & 0xFFFF;

    /* Do not switch immediately.  Tell the custom chip emulation about the
    * desired state, and wait for custom.c to call picasso_enablescreen
    * whenever it is ready to change the screen state.  */
    picasso_requested_on = flag;
    write_log ("SetSwitch() - trying to show %s screen\n", flag ? "picasso96":"amiga");
    
    /* Put old switch-state in D0 */
	
    return !flag;
}

void picasso_enablescreen (int on)
{  
    wgfx_linestart = 0xFFFFFFFF;
    picasso_refresh (1);
    write_log ("SetSwitch() from threadid %d - showing %s screen\n", GetCurrentThreadId(), on ? "picasso96": "amiga");
}

/*
* SetColorArray:
* a0: struct BoardInfo
* d0.w: startindex
* d1.w: count
* when this function is called, your driver has to fetch "count" color
* values starting at "startindex" from the CLUT field of the BoardInfo
* structure and write them to the hardware. The color values are always
* between 0 and 255 for each component regardless of the number of bits
* per cannon your board has. So you might have to shift the colors
* before writing them to the hardware.
*/
uae_u32 picasso_SetColorArray (void)
{
/* Fill in some static UAE related structure about this new CLUT setting
    * We need this for CLUT-based displays, and for mapping CLUT to hi/true colour */
    uae_u16 start = m68k_dreg (regs, 0);
    uae_u16 count = m68k_dreg (regs, 1);
    int i;
    uaecptr boardinfo = m68k_areg (regs, 0);
    uaecptr clut = boardinfo + PSSO_BoardInfo_CLUT + start * 3;
    
    for (i = start; i < start + count; i++) {
	int r = get_byte (clut);
	int g = get_byte (clut + 1);
	int b = get_byte (clut + 2);
	
	palette_changed |= (picasso96_state.CLUT[i].Red != r 
	    || picasso96_state.CLUT[i].Green != g
	    || picasso96_state.CLUT[i].Blue != b);
	
	picasso96_state.CLUT[i].Red = r; 
	picasso96_state.CLUT[i].Green = g;
	picasso96_state.CLUT[i].Blue = b;
	clut += 3;
    }
    P96TRACE(("SetColorArray(%d,%d)\n", start, count));
    return 1;
}

/*
* SetDAC:
* a0: struct BoardInfo
* d7: RGBFTYPE RGBFormat
* This function is called whenever the RGB format of the display changes,
* e.g. from chunky to TrueColor. Usually, all you have to do is to set
* the RAMDAC of your board accordingly.
*/
uae_u32 picasso_SetDAC (void)
{
/* Fill in some static UAE related structure about this new DAC setting
    * Lets us keep track of what pixel format the Amiga is thinking about in our frame-buffer */
    
    P96TRACE(("SetDAC()\n"));
    return 1;
}


static void init_picasso_screen( void )
{
    if( set_panning_called )
    {
	picasso96_state.Extent = picasso96_state.Address + ( picasso96_state.BytesPerRow * picasso96_state.VirtualHeight );
    }
    if (set_gc_called)
    {	
	gfx_set_picasso_modeinfo (picasso96_state.Width, picasso96_state.Height,
	    picasso96_state.GC_Depth, picasso96_state.RGBFormat);
    }
    if( ( picasso_vidinfo.width == picasso96_state.Width ) &&
	( picasso_vidinfo.height == picasso96_state.Height ) &&
	( picasso_vidinfo.depth == (picasso96_state.GC_Depth >> 3) ) &&
	( picasso_vidinfo.selected_rgbformat == picasso96_state.RGBFormat) ) 
    {
	DX_SetPalette (0, 256);
	picasso_refresh (1); 
    }
}

/*
* SetGC:
* a0: struct BoardInfo
* a1: struct ModeInfo
* d0: BOOL Border
* This function is called whenever another ModeInfo has to be set. This
* function simply sets up the CRTC and TS registers to generate the
* timing used for that screen mode. You should not set the DAC, clocks
* or linear start adress. They will be set when appropriate by their
* own functions.
*/
uae_u32 picasso_SetGC (void)
{
    /* Fill in some static UAE related structure about this new ModeInfo setting */
    uae_u32 border   = m68k_dreg (regs, 0);
    uaecptr modeinfo = m68k_areg (regs, 1);
    
    picasso96_state.Width = get_word (modeinfo + PSSO_ModeInfo_Width);
    picasso96_state.VirtualWidth = picasso96_state.Width; /* in case SetPanning doesn't get called */
    
    picasso96_state.Height = get_word (modeinfo + PSSO_ModeInfo_Height);
    picasso96_state.VirtualHeight = picasso96_state.Height; /* in case SetPanning doesn't get called */
    
    picasso96_state.GC_Depth = get_byte (modeinfo + PSSO_ModeInfo_Depth);
    picasso96_state.GC_Flags = get_byte (modeinfo + PSSO_ModeInfo_Flags);
    
    P96TRACE(("SetGC(%d,%d,%d,%d)\n", picasso96_state.Width, picasso96_state.Height, picasso96_state.GC_Depth, border ));
    set_gc_called = 1;
    init_picasso_screen ();
    
    return 1;
}

/*
* SetPanning:
* a0: struct BoardInfo
* a1: UBYTE *Memory
* d0: uae_u16 Width
* d1: WORD XOffset
* d2: WORD YOffset
* d7: RGBFTYPE RGBFormat
* This function sets the view origin of a display which might also be
* overscanned. In register a1 you get the start address of the screen
* bitmap on the Amiga side. You will have to subtract the starting
* address of the board memory from that value to get the memory start
* offset within the board. Then you get the offset in pixels of the
* left upper edge of the visible part of an overscanned display. From
* these values you will have to calculate the LinearStartingAddress
* fields of the CRTC registers.

  * NOTE: SetPanning() can be used to know when a Picasso96 screen is
  * being opened.  Better to do the appropriate clearing of the
  * background here than in SetSwitch() derived functions,
  * because SetSwitch() is not called for subsequent Picasso screens.
*/

uae_u32 picasso_SetPanning (void)
{   
    uae_u16 Width = m68k_dreg (regs, 0);
    uaecptr start_of_screen = m68k_areg (regs, 1);
    uaecptr bi = m68k_areg( regs, 0 );
    uaecptr bmeptr = get_long( bi + PSSO_BoardInfo_BitMapExtra );  /* Get our BoardInfo ptr's BitMapExtra ptr */
    uae_u16 bme_width, bme_height;
	 
    if(oldscr==0){
	oldscr=start_of_screen;    
    }
    if ((oldscr!=start_of_screen)){
	set_gc_called = 0;
	oldscr=start_of_screen;
    }

    bme_width = get_word( bmeptr + PSSO_BitMapExtra_Width );
    bme_height = get_word( bmeptr + PSSO_BitMapExtra_Height );
    
    picasso96_state.Address = start_of_screen; /* Amiga-side address */
    picasso96_state.XOffset = (uae_s16)(m68k_dreg (regs, 1) & 0xFFFF);
    picasso96_state.YOffset = (uae_s16)(m68k_dreg (regs, 2) & 0xFFFF);
    picasso96_state.VirtualWidth = bme_width;
    picasso96_state.VirtualHeight = bme_height;
    if( ( bme_width > Width ) || ( bme_height > picasso96_state.Height ) ) // NOTE: These were != instead of > before...
	picasso96_state.BigAssBitmap = 1;
    else
	picasso96_state.BigAssBitmap = 0;
    picasso96_state.RGBFormat = m68k_dreg (regs, 7);
    picasso96_state.BytesPerPixel = GetBytesPerPixel (picasso96_state.RGBFormat);
    picasso96_state.BytesPerRow = bme_width * picasso96_state.BytesPerPixel;
    
    set_panning_called = 1;
    P96TRACE(("SetPanning(%d, %d, %d) Start 0x%x, BPR %d Bpp %d RGBF %d\n",
	Width, picasso96_state.XOffset, picasso96_state.YOffset,
	start_of_screen, picasso96_state.BytesPerRow, picasso96_state.BytesPerPixel, picasso96_state.RGBFormat));
    init_picasso_screen ();
    set_panning_called = 0;
    
    return 1;
}

static void do_xor8 (uae_u8 *ptr, long len, uae_u32 val)
{
    int i;
#if 0 && defined ALIGN_POINTER_TO32
    int align_adjust = ALIGN_POINTER_TO32(ptr);
    int len2;
    
    len -= align_adjust;
    while (align_adjust) {
	*ptr ^= val;
	ptr++;
	align_adjust--;
    }
    len2 = len >> 2;
    len -= len2 << 2;
    for (i = 0; i < len2; i++, ptr += 4) {
	*(uae_u32 *)ptr ^= val;
    }
    while (len) {
	*ptr ^= val;
	ptr++;
	len--;
    }
    return;
#endif
    for (i = 0; i < len; i++, ptr++) {
	do_put_mem_byte (ptr, (uae_u8)( do_get_mem_byte (ptr) ^ val ) );
    }
}

/*
* InvertRect:
* 
* Inputs:
* a0:struct BoardInfo *bi
* a1:struct RenderInfo *ri
* d0.w:X
* d1.w:Y
* d2.w:Width
* d3.w:Height
* d4.l:Mask
* d7.l:RGBFormat
* 
* This function is used to invert a rectangular area on the board. It is called by BltBitMap,
* BltPattern and BltTemplate.
*/
uae_u32 picasso_InvertRect (void)
{
    uaecptr renderinfo = m68k_areg (regs, 1);
    unsigned long X = (uae_u16)m68k_dreg (regs, 0);
    unsigned long Y = (uae_u16)m68k_dreg (regs, 1);
    unsigned long Width = (uae_u16)m68k_dreg (regs, 2);
    unsigned long Height = (uae_u16)m68k_dreg (regs, 3);
    uae_u8 mask = (uae_u8)m68k_dreg (regs, 4);
    int Bpp = GetBytesPerPixel (m68k_dreg (regs, 7));
    uae_u32 xorval;
    unsigned int lines;
    struct RenderInfo ri;
    uae_u8 *uae_mem, *rectstart;
    unsigned long width_in_bytes;
    uae_u32 result = 0;

  
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
#else 
	  wgfx_flushline ();    
#endif
    
    if ( CopyRenderInfoStructureA2U (renderinfo, &ri))
    {
        P96TRACE(("InvertRect %dbpp 0x%lx\n", Bpp, (long)mask));
    
	if (mask != 0xFF && Bpp > 1) 
	{
	    mask = 0xFF;
	}

	xorval = 0x01010101 * (mask & 0xFF);
	width_in_bytes = Bpp * Width;
	rectstart = uae_mem = ri.Memory + Y*ri.BytesPerRow + X*Bpp;
    
	for (lines = 0; lines < Height; lines++, uae_mem += ri.BytesPerRow)
	    do_xor8 (uae_mem, width_in_bytes, xorval);
    
	if (renderinfo_is_current_screen (&ri)) {
	    if (mask == 0xFF)
		do_invertrect( &ri, Bpp, X, Y, Width, Height );
	    else
		do_blit( &ri, Bpp, X, Y, X, Y, Width, Height, BLIT_SRC, 0);
	}
	result = 1;
    }
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result; /* 1 if supported, 0 otherwise */
}

/***********************************************************
FillRect:
***********************************************************
* a0: 	struct BoardInfo *
* a1:	struct RenderInfo *
* d0: 	WORD X
* d1: 	WORD Y
* d2: 	WORD Width
* d3: 	WORD Height
* d4:	uae_u32 Pen
* d5:	UBYTE Mask
* d7:	uae_u32 RGBFormat
***********************************************************/
uae_u32 picasso_FillRect (void)
{
    uaecptr renderinfo = m68k_areg (regs, 1);
    uae_u32 X = (uae_u16)m68k_dreg (regs, 0);
    uae_u32 Y = (uae_u16)m68k_dreg (regs, 1);
    uae_u32 Width = (uae_u16)m68k_dreg (regs, 2);
    uae_u32 Height = (uae_u16)m68k_dreg (regs, 3);
    uae_u32 Pen = m68k_dreg (regs, 4);
    uae_u8 Mask = (uae_u8)m68k_dreg (regs, 5);
    RGBFTYPE RGBFormat = m68k_dreg (regs, 7);
    
    uae_u8 *src;
    uae_u8 *oldstart;
    int Bpp;
    struct RenderInfo ri;
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock(); // We need this, because otherwise we're still Locked from custom.c
#else
    wgfx_flushline ();
#endif
    
    if ( CopyRenderInfoStructureA2U (renderinfo, &ri) && Y != 0xFFFF)
    {
	if (ri.RGBFormat != RGBFormat)
	    write_log ("Weird Stuff!\n");
    
	Bpp = GetBytesPerPixel (RGBFormat);
    
	P96TRACE(("FillRect(%d, %d, %d, %d) Pen 0x%x BPP %d BPR %d Mask 0x%x\n",
	    X, Y, Width, Height, Pen, Bpp, ri.BytesPerRow, Mask));
    
	if( Bpp > 1 )
	    Mask = 0xFF;
    
	if (Mask == 0xFF) 
	{
	    if( ( Width == 1 ) || ( Height == 1 ) )
	    {
		int i;
		uaecptr addr;
		if( renderinfo_is_current_screen( &ri ) )
		{
		    uae_u32 diff=gfxmem_start-(uae_u32)gfxmemory;
		    addr = ri.Memory + X*Bpp + Y*ri.BytesPerRow + diff;
		    if( Width == 1 )
		    {
			for( i = 0; i < Height; i++ )
			{
			    if( Bpp == 4 )
				gfxmem_lput( addr + (i*picasso96_state.BytesPerRow ), Pen );
			    else if( Bpp == 2 )
				gfxmem_wput( addr + (i*picasso96_state.BytesPerRow ), Pen );
			    else
				gfxmem_bput( addr + (i*picasso96_state.BytesPerRow ), Pen );
			}
		    }
		    else if( Height == 1 )
		    {
			for( i = 0; i < Width; i++ )
			{
			    if( Bpp == 4 )
				gfxmem_lput( addr + (i*Bpp), Pen );
			    else if( Bpp == 2 )
				gfxmem_wput( addr + (i*Bpp), Pen );
			    else
				gfxmem_bput( addr + (i*Bpp), Pen );
			}
		    }
		    return 1;
		}
	    }

	    /* Do the fill-rect in the frame-buffer */
	    do_fillrect_frame_buffer( &ri, X, Y, Width, Height, Pen, Bpp, RGBFormat ); 
	    /* Now we do the on-screen display, if renderinfo points to it */
	    if (renderinfo_is_current_screen (&ri))
	    {
		src = ri.Memory + X*Bpp + Y*ri.BytesPerRow;
		X=X-picasso96_state.XOffset;
		Y=Y-picasso96_state.YOffset;	    
		if((int)X<0){Width=Width+X;X=0;}
		if((int)Width<1)return 1;
		if((int)Y<0){Height=Height+Y;Y=0;}
		if((int)Height<1)return 1;
		/* Argh - why does P96Speed do this to me, with FillRect only?! */
		if( ( X < picasso96_state.Width) &&
		    ( Y < picasso96_state.Height) )
		{
		    if( X+Width > picasso96_state.Width)
			Width = picasso96_state.Width - X;
		    if( Y+Height > picasso96_state.Height)
			Height = picasso96_state.Height - Y;

		    do_fillrect( src, X, Y, Width, Height, Pen, Bpp, RGBFormat );
		}
	    }
	    result = 1;
	}
	else
	{
	    /* We get here only if Mask != 0xFF */
	    if (Bpp != 1) 
	    {
		write_log( "WARNING - FillRect() has unhandled mask 0x%x with Bpp %d. Using fall-back routine.\n", Mask, Bpp );
	    }
	    else
	    {
		Pen &= Mask;
		Mask = ~Mask;
		oldstart = ri.Memory + Y*ri.BytesPerRow + X*Bpp;
		{
		    uae_u8 *start = oldstart;
		    uae_u8 *end = start + Height * ri.BytesPerRow;
		    for (; start != end; start += ri.BytesPerRow) 
		    {
			uae_u8 *p = start;
			unsigned long cols;
			for (cols = 0; cols < Width; cols++) 
			{
			    uae_u32 tmpval = do_get_mem_byte (p + cols) & Mask;
			    do_put_mem_byte (p + cols, (uae_u8)( Pen | tmpval ) );
			}
		    }
		}
		if (renderinfo_is_current_screen (&ri))
		    do_blit( &ri, Bpp, X, Y, X, Y, Width, Height, BLIT_SRC, 0);
		result = 1;
	    }
	}
    }

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/*
* BlitRect() is a generic (any chunky pixel format) rectangle copier
* NOTE: If dstri is NULL, then we're only dealing with one RenderInfo area, and called from picasso_BlitRect()
*
* OpCodes:
* 0 = FALSE:	dst = 0
* 1 = NOR:	dst = ~(src | dst)
* 2 = ONLYDST:	dst = dst & ~src
* 3 = NOTSRC:	dst = ~src
* 4 = ONLYSRC:	dst = src & ~dst
* 5 = NOTDST:	dst = ~dst
* 6 = EOR:	dst = src^dst
* 7 = NAND:	dst = ~(src & dst)
* 8 = AND:	dst = (src & dst)
* 9 = NEOR:	dst = ~(src ^ dst)
*10 = DST:	dst = dst
*11 = NOTONLYSRC: dst = ~src | dst
*12 = SRC:	dst = src
*13 = NOTONLYDST: dst = ~dst | src
*14 = OR:	dst = src | dst
*15 = TRUE:	dst = 0xFF
*/
struct blitdata
{
    struct RenderInfo ri_struct;
    struct RenderInfo dstri_struct;
    struct RenderInfo *ri; /* Self-referencing pointers */
    struct RenderInfo *dstri;
    unsigned long srcx;
    unsigned long srcy;
    unsigned long dstx;
    unsigned long dsty;
    unsigned long width;
    unsigned long height;
    uae_u8 mask;
    BLIT_OPCODE opcode;
} blitrectdata;

STATIC_INLINE int BlitRectHelper( void )
{
    struct RenderInfo *ri = blitrectdata.ri;
    struct RenderInfo *dstri = blitrectdata.dstri;
    unsigned long srcx = blitrectdata.srcx;
    unsigned long srcy = blitrectdata.srcy;
    unsigned long dstx = blitrectdata.dstx;
    unsigned long dsty = blitrectdata.dsty;
    unsigned long width = blitrectdata.width;
    unsigned long height = blitrectdata.height;
    uae_u8 mask = blitrectdata.mask;
    BLIT_OPCODE opcode = blitrectdata.opcode;
    
    uae_u8 Bpp = GetBytesPerPixel(ri->RGBFormat);
    unsigned long total_width = width * Bpp;
    unsigned long linewidth = (total_width + 15) & ~15;
    int can_do_visible_blit = 0;
    
    if( opcode == BLIT_DST )
    {
	write_log( "WARNING: BlitRect() being called with opcode of BLIT_DST\n" );
	return 1;
    }
    
    /*
    * If we have no destination RenderInfo, then we're dealing with a single-buffer action, called
    * from picasso_BlitRect().  The code in do_blitrect_frame_buffer() deals with the frame-buffer,
    * while the do_blit() code deals with the visible screen.
    *
    * If we have a destination RenderInfo, then we've been called from picasso_BlitRectNoMaskComplete()
    * and we need to put the results on the screen from the frame-buffer.
    */
    if (dstri == NULL) 
    {
        if( mask != 0xFF && Bpp > 1 ) 
        {
            mask = 0xFF;
        }
        dstri = ri;
        can_do_visible_blit = 1;
    }
    
    /* Do our virtual frame-buffer memory first */
    do_blitrect_frame_buffer( ri, dstri, srcx, srcy, dstx, dsty, width, height, mask, opcode );
    /* Now we do the on-screen display, if renderinfo points to it */
    if (renderinfo_is_current_screen (dstri))
    {
        if (mask == 0xFF || Bpp > 1) {
	    if( can_do_visible_blit )
		do_blit( dstri, Bpp, srcx, srcy, dstx, dsty, width, height, opcode, 1 );
	    else
		do_blit( dstri, Bpp, dstx, dsty, dstx, dsty, width, height, opcode, 0 );
        } else {
            do_blit( dstri, Bpp, dstx, dsty, dstx, dsty, width, height, opcode, 0 );
        }
        P96TRACE(("Did do_blit 1 in BlitRect()\n"));
    }
    else
    {
        P96TRACE(("Did not do_blit 1 in BlitRect()\n"));
    }
    
    return 1;
}

STATIC_INLINE int BlitRect (uaecptr ri, uaecptr dstri,
			    unsigned long srcx, unsigned long srcy, unsigned long dstx, unsigned long dsty,
			    unsigned long width, unsigned long height, uae_u8 mask, BLIT_OPCODE opcode )
{
    /* Set up the params */
    CopyRenderInfoStructureA2U( ri, &blitrectdata.ri_struct );
    blitrectdata.ri = &blitrectdata.ri_struct;
    if( dstri )
    {
	CopyRenderInfoStructureA2U( dstri, &blitrectdata.dstri_struct );
	blitrectdata.dstri = &blitrectdata.dstri_struct;
    }
    else
    {
	blitrectdata.dstri = NULL;
    }
    blitrectdata.srcx = srcx;
    blitrectdata.srcy = srcy;
    blitrectdata.dstx = dstx;
    blitrectdata.dsty = dsty;
    blitrectdata.width = width;
    blitrectdata.height = height;
    blitrectdata.mask = mask;
    blitrectdata.opcode = opcode;
    
    return BlitRectHelper();
}

/***********************************************************
BlitRect:
***********************************************************
* a0: 	struct BoardInfo
* a1:	struct RenderInfo
* d0: 	WORD SrcX
* d1: 	WORD SrcY
* d2: 	WORD DstX
* d3: 	WORD DstY
* d4:   WORD Width
* d5:   WORD Height
* d6:	UBYTE Mask
* d7:	uae_u32 RGBFormat
***********************************************************/
uae_u32 picasso_BlitRect (void)
{
    uaecptr renderinfo = m68k_areg (regs, 1);
    unsigned long srcx = (uae_u16)m68k_dreg (regs, 0);
    unsigned long srcy = (uae_u16)m68k_dreg (regs, 1);
    unsigned long dstx = (uae_u16)m68k_dreg (regs, 2);
    unsigned long dsty = (uae_u16)m68k_dreg (regs, 3);
    unsigned long width = (uae_u16)m68k_dreg (regs, 4);
    unsigned long height = (uae_u16)m68k_dreg (regs, 5);
    uae_u8  Mask = (uae_u8)m68k_dreg (regs, 6);
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     
   
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
#else  
    wgfx_flushline ();    
#endif
    
    P96TRACE(("BlitRect(%d, %d, %d, %d, %d, %d, 0x%x)\n", srcx, srcy, dstx, dsty, width, height, Mask));
    result = BlitRect(renderinfo, (uaecptr)NULL, srcx, srcy, dstx, dsty, width, height, Mask, BLIT_SRC );

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/***********************************************************
BlitRectNoMaskComplete:
***********************************************************
* a0: 	struct BoardInfo
* a1:	struct RenderInfo (src)
* a2:   struct RenderInfo (dst)
* d0: 	WORD SrcX
* d1: 	WORD SrcY
* d2: 	WORD DstX
* d3: 	WORD DstY
* d4:   WORD Width
* d5:   WORD Height
* d6:	UBYTE OpCode
* d7:	uae_u32 RGBFormat
* NOTE: MUST return 0 in D0 if we're not handling this operation
*       because the RGBFormat or opcode aren't supported.
*       OTHERWISE return 1
***********************************************************/
uae_u32 picasso_BlitRectNoMaskComplete (void)
{
    uaecptr srcri = m68k_areg (regs, 1);
    uaecptr dstri = m68k_areg (regs, 2);
    unsigned long srcx = (uae_u16)m68k_dreg (regs, 0);
    unsigned long srcy = (uae_u16)m68k_dreg (regs, 1);
    unsigned long dstx = (uae_u16)m68k_dreg (regs, 2);
    unsigned long dsty = (uae_u16)m68k_dreg (regs, 3);
    unsigned long width = (uae_u16)m68k_dreg (regs, 4);
    unsigned long height = (uae_u16)m68k_dreg (regs, 5);
    uae_u8 OpCode = m68k_dreg (regs, 6);
    uae_u32 RGBFmt = m68k_dreg (regs, 7);
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     
    
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
	
#else	 
    wgfx_flushline ();   
#endif
    
    P96TRACE(("BlitRectNoMaskComplete() op 0x%2x, xy(%4d,%4d) --> xy(%4d,%4d), wh(%4d,%4d)\n",
	OpCode, srcx, srcy, dstx, dsty, width, height));
    
    result = BlitRect( srcri, dstri, srcx, srcy, dstx, dsty, width, height, 0xFF, OpCode );

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/* This utility function is used both by BlitTemplate() and BlitPattern() */
STATIC_INLINE void PixelWrite1(uae_u8 *mem, int bits, uae_u32 fgpen, uae_u32 mask)
{
    if (mask != 0xFF)
	fgpen = (fgpen & mask) | (do_get_mem_byte (mem + bits) & ~mask);
    do_put_mem_byte (mem + bits, fgpen);
}

STATIC_INLINE void PixelWrite2(uae_u8 *mem, int bits, uae_u32 fgpen)
{
    do_put_mem_word (((uae_u16 *)mem) + bits, fgpen);
}

STATIC_INLINE void PixelWrite3(uae_u8 *mem, int bits, uae_u32 fgpen)
{
    do_put_mem_byte (mem + bits*3, fgpen & 0x000000FF);
    *(uae_u16 *)(mem + bits*3+1) = (fgpen & 0x00FFFF00) >> 8;
}

STATIC_INLINE void PixelWrite4(uae_u8 *mem, int bits, uae_u32 fgpen)
{
    do_put_mem_long (((uae_u32 *)mem) + bits, fgpen);
}

STATIC_INLINE void PixelWrite(uae_u8 *mem, int bits, uae_u32 fgpen, uae_u8 Bpp, uae_u32 mask)
{
    switch (Bpp) {
    case 1:
	if (mask != 0xFF)
	    fgpen = (fgpen & mask) | (do_get_mem_byte (mem + bits) & ~mask);
	do_put_mem_byte (mem + bits, (uae_u8)fgpen);
	break;
    case 2:
	do_put_mem_word (((uae_u16 *)mem) + bits, (uae_u16)fgpen);
	break;
    case 3:
	do_put_mem_byte (mem + bits*3, (uae_u8)fgpen);
	*(uae_u16 *)(mem + bits*3+1) = (fgpen & 0x00FFFF00) >> 8;
	break;
    case 4:
	do_put_mem_long (((uae_u32 *)mem) + bits, fgpen);
	break;
    }
}

/*
* BlitPattern:
* 
* Synopsis:BlitPattern(bi, ri, pattern, X, Y, Width, Height, Mask, RGBFormat);
* Inputs:
* a0:struct BoardInfo *bi
* a1:struct RenderInfo *ri
* a2:struct Pattern *pattern
* d0.w:X
* d1.w:Y
* d2.w:Width
* d3.w:Height
* d4.w:Mask
* d7.l:RGBFormat
* 
* This function is used to paint a pattern on the board memory using the blitter. It is called by
* BltPattern, if a AreaPtrn is used with positive AreaPtSz. The pattern consists of a b/w image
* using a single plane of image data which will be expanded repeatedly to the destination RGBFormat
* using ForeGround and BackGround pens as well as draw modes. The width of the pattern data is
* always 16 pixels (one word) and the height is calculated as 2^Size. The data must be shifted up
* and to the left by XOffset and YOffset pixels at the beginning.
*/
uae_u32 picasso_BlitPattern (void)
{
    uaecptr rinf = m68k_areg (regs, 1);
    uaecptr pinf = m68k_areg (regs, 2);
    unsigned long X = (uae_u16)m68k_dreg (regs, 0);
    unsigned long Y = (uae_u16)m68k_dreg (regs, 1);
    unsigned long W = (uae_u16)m68k_dreg (regs, 2);
    unsigned long H = (uae_u16)m68k_dreg (regs, 3);
    uae_u8 Mask = (uae_u8)m68k_dreg (regs, 4);
    uae_u32 RGBFmt = m68k_dreg (regs, 7);
    uae_u8 Bpp = GetBytesPerPixel (RGBFmt);
    int inversion = 0;
    struct RenderInfo ri;
    struct Pattern pattern;
    unsigned long rows;
    uae_u32 fgpen;
    uae_u8 *uae_mem;
    int xshift;
    unsigned long ysize_mask;
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     
   
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
	
#else	
    wgfx_flushline ();
#endif
    
    if( CopyRenderInfoStructureA2U (rinf, &ri) && CopyPatternStructureA2U (pinf, &pattern))
    {
	Bpp = GetBytesPerPixel(ri.RGBFormat);
	uae_mem = ri.Memory + Y*ri.BytesPerRow + X*Bpp; /* offset with address */
    
	if (pattern.DrawMode & INVERS)
	    inversion = 1;
    
	pattern.DrawMode &= 0x03;
	if (Mask != 0xFF) 
	{
	    if( Bpp > 1 )
		Mask = 0xFF;

	    if( pattern.DrawMode == COMP)
	    {
		write_log ("WARNING - BlitPattern() has unhandled mask 0x%x with COMP DrawMode. Using fall-back routine.\n", Mask);
	    }
	    else
	    {
		result = 1;
	    }
	}
	else
	{
	    result = 1;
	}

	if( result )
	{
    	    /* write_log ("BlitPattern() xy(%d,%d), wh(%d,%d) draw 0x%x, off(%d,%d), ph %d\n",
	    X, Y, W, H, pattern.DrawMode, pattern.XOffset, pattern.YOffset, 1 << pattern.Size); */
    #ifdef P96TRACING_ENABLED
	    DumpPattern(&pattern);
    #endif
	    ysize_mask = (1 << pattern.Size) - 1;
	    xshift = pattern.XOffset & 15;
    
	    for (rows = 0; rows < H; rows++, uae_mem += ri.BytesPerRow) {
		unsigned long prow = (rows + pattern.YOffset) & ysize_mask;
		unsigned int d = do_get_mem_word (((uae_u16 *)pattern.Memory) + prow);
		uae_u8 *uae_mem2 = uae_mem;
		unsigned long cols;
		
		if (xshift != 0)
		    d = (d << xshift) | (d >> (16 - xshift));
		
		for (cols = 0; cols < W; cols += 16, uae_mem2 += Bpp << 4) 
		{
		    long bits;
		    long max = W - cols;
		    unsigned int data = d;
		    
		    if (max > 16)
			max = 16;
		    
		    for (bits = 0; bits < max; bits++) 
		    {
			int bit_set = data & 0x8000;
			data <<= 1;
			switch (pattern.DrawMode) {
			case JAM1:
			    if (inversion)
				bit_set = !bit_set;
			    if (bit_set)
				PixelWrite (uae_mem2, bits, pattern.FgPen, Bpp, Mask);
			    break;
			case JAM2:
			    if (inversion)
				bit_set = !bit_set;
			    if (bit_set)
				PixelWrite (uae_mem2, bits, pattern.FgPen, Bpp, Mask);
			    else
				PixelWrite (uae_mem2, bits, pattern.BgPen, Bpp, Mask);
			    break;
			case COMP:
			    if (bit_set) {
				fgpen = pattern.FgPen;
				
				switch (Bpp) {
				case 1:
				    {
					uae_u8 *addr = uae_mem2 + bits;
					do_put_mem_byte (addr, (uae_u8)( do_get_mem_byte (addr) ^ fgpen ) );
				    }
				    break;
				case 2:
				    {
					uae_u16 *addr = ((uae_u16 *)uae_mem2) + bits;
					do_put_mem_word (addr, (uae_u16)( do_get_mem_word (addr) ^ fgpen ) );
				    }
				    break;
				case 3:
				    {
					uae_u32 *addr = (uae_u32 *)(uae_mem2 + bits * 3);
					do_put_mem_long (addr, do_get_mem_long (addr) ^ (fgpen & 0x00FFFFFF));
				    }
				    break;
				case 4:
				    {
					uae_u32 *addr = ((uae_u32 *)uae_mem2) + bits;
					do_put_mem_long (addr, do_get_mem_long (addr) ^ fgpen);
				    }
				    break;
				}
			    }
			    break;
			}
		    }
		}
	    }
    
	    /* If we need to update a second-buffer (extra_mem is set), then do it only if visible! */
	    if ( picasso_vidinfo.extra_mem && renderinfo_is_current_screen (&ri))
		do_blit( &ri, Bpp, X, Y, X, Y, W, H, BLIT_SRC, 0);

	    result = 1;
	}
    }

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/*************************************************
BlitTemplate:
**************************************************
* Synopsis: BlitTemplate(bi, ri, template, X, Y, Width, Height, Mask, RGBFormat);
* a0: struct BoardInfo *bi
* a1: struct RenderInfo *ri
* a2: struct Template *template
* d0.w: X
* d1.w: Y
* d2.w: Width
* d3.w: Height
* d4.w: Mask
* d7.l: RGBFormat
*
* This function is used to paint a template on the board memory using the blitter.
* It is called by BltPattern and BltTemplate. The template consists of a b/w image
* using a single plane of image data which will be expanded to the destination RGBFormat
* using ForeGround and BackGround pens as well as draw modes.
***********************************************************************************/
uae_u32 picasso_BlitTemplate (void)
{
    uae_u8 inversion = 0;
    uaecptr rinf = m68k_areg (regs, 1);
    uaecptr tmpl = m68k_areg (regs, 2);
    unsigned long X = (uae_u16)m68k_dreg (regs, 0);
    unsigned long Y = (uae_u16)m68k_dreg (regs, 1);
    unsigned long W = (uae_u16)m68k_dreg (regs, 2);
    unsigned long H = (uae_u16)m68k_dreg (regs, 3);
    uae_u16 Mask = (uae_u16)m68k_dreg (regs, 4);
    struct Template tmp;
    struct RenderInfo ri;
    unsigned long rows;
    int bitoffset;
    uae_u32 fgpen;
    uae_u8 *uae_mem, Bpp;
    uae_u8 *tmpl_base;
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     

#ifdef LOCK_UNLOCK_MADNESS
    PICASSO96_Unlock(); // @@@ We need to unlock here, because do_blit (later) needs to lock...
#else
    wgfx_flushline ();
#endif
    
    if ( CopyRenderInfoStructureA2U (rinf, &ri) && CopyTemplateStructureA2U (tmpl, &tmp))
    {
	Bpp = GetBytesPerPixel(ri.RGBFormat);
	uae_mem = ri.Memory + Y*ri.BytesPerRow + X*Bpp; /* offset into address */
    
	if (tmp.DrawMode & INVERS)
	    inversion = 1;
    
	tmp.DrawMode &= 0x03;

	if (Mask != 0xFF) 
	{
	    if( Bpp > 1 )
		Mask = 0xFF;

	    if( tmp.DrawMode == COMP)
	    {
		write_log ("WARNING - BlitTemplate() has unhandled mask 0x%x with COMP DrawMode. Using fall-back routine.\n", Mask);
		flushpixels();  //only need in the windows Version
		return 0;
	    }
	    else
	    {
		result = 1;
	    }
	}
	else
	{
	    result = 1;
	}
#if 1
	if (tmp.DrawMode == COMP) {
	    /* workaround, let native blitter handle COMP mode */
	    flushpixels();
	    return 0;
	}
#endif
	if( result )
	{
	    P96TRACE(("BlitTemplate() xy(%d,%d), wh(%d,%d) draw 0x%x fg 0x%x bg 0x%x \n",
		X, Y, W, H, tmp.DrawMode, tmp.FgPen, tmp.BgPen));
    
	    bitoffset = tmp.XOffset % 8;
    
#if defined( P96TRACING_ENABLED ) && ( P96TRACING_LEVEL > 0 )
	    DumpTemplate(&tmp, W, H);
#endif
    
	    tmpl_base = tmp.Memory + tmp.XOffset/8;
	    
	    for (rows = 0; rows < H; rows++, uae_mem += ri.BytesPerRow, tmpl_base += tmp.BytesPerRow) {
		unsigned long cols;
		uae_u8 *tmpl_mem = tmpl_base;
		uae_u8 *uae_mem2 = uae_mem;
		unsigned int data = *tmpl_mem;
		
		for (cols = 0; cols < W; cols += 8, uae_mem2 += Bpp << 3) {
		    unsigned int byte;
		    long bits;
		    long max = W - cols;
		    
		    if (max > 8)
			max = 8;
		    
		    data <<= 8;
		    data |= *++tmpl_mem;
		    
		    byte = data >> (8 - bitoffset);
		    
		    for (bits = 0; bits < max; bits++) {
			int bit_set = (byte & 0x80);
			byte <<= 1;
			switch (tmp.DrawMode) {
			case JAM1:
			    if (inversion)
				bit_set = !bit_set;
			    if (bit_set) {
				fgpen = tmp.FgPen;
				PixelWrite(uae_mem2, bits, fgpen, Bpp, Mask);
			    }
			    break;
			case JAM2:
			    if (inversion)
				bit_set = !bit_set;
			    fgpen = tmp.BgPen;
			    if (bit_set)
				fgpen = tmp.FgPen;
			    
			    PixelWrite(uae_mem2, bits, fgpen, Bpp, Mask);
			    break;
			case COMP:
			    if (bit_set) {
				fgpen = tmp.FgPen;
				
				switch (Bpp) {
				case 1:
				    {
					uae_u8 *addr = uae_mem2 + bits;
					do_put_mem_byte (addr, (uae_u8)( do_get_mem_byte (addr) ^ fgpen ) );
				    }
				    break;
				case 2:
				    {
					uae_u16 *addr = ((uae_u16 *)uae_mem2) + bits;
					do_put_mem_word (addr, (uae_u16)( do_get_mem_word (addr) ^ fgpen ) );
				    }
				    break;
				case 3:
				    {
					uae_u32 *addr = (uae_u32 *)(uae_mem2 + bits * 3);
					do_put_mem_long (addr, do_get_mem_long (addr) ^ (fgpen & 0x00FFFFFF));
				    }
				    break;
				case 4:
				    {
					uae_u32 *addr = ((uae_u32 *)uae_mem2) + bits;
					do_put_mem_long (addr, do_get_mem_long (addr) ^ fgpen);
				    }
				    break;
				}
			    }
			    break;
			}
		    }
		}
	    }
	    
	    /* If we need to update a second-buffer (extra_mem is set), then do it only if visible! */
	    if( picasso_vidinfo.extra_mem && renderinfo_is_current_screen( &ri ) )
		do_blit( &ri, Bpp, X, Y, X, Y, W, H, BLIT_SRC, 0 );
	    
	    result = 1;
	}
}    
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return 1;
}

/*
* CalculateBytesPerRow:
* a0: 	struct BoardInfo
* d0: 	uae_u16 Width
* d7:	RGBFTYPE RGBFormat
* This function calculates the amount of bytes needed for a line of
* "Width" pixels in the given RGBFormat.
*/
uae_u32 picasso_CalculateBytesPerRow (void)
{
    uae_u16 width = m68k_dreg (regs, 0);
    uae_u32 type = m68k_dreg (regs, 7);
    
    width = GetBytesPerPixel(type)*width;
    P96TRACE(("CalculateBytesPerRow() = %d\n",width));
    
    return width;
}

/*
* SetDisplay:
* a0:	struct BoardInfo
* d0:	BOOL state
* This function enables and disables the video display.
* 
* NOTE: return the opposite of the state
*/
uae_u32 picasso_SetDisplay (void)
{
    uae_u32 state = m68k_dreg (regs, 0);
    P96TRACE (("SetDisplay(%d)\n", state));
    return !state;
}

/*
* WaitVerticalSync:
* a0:	struct BoardInfo
* This function waits for the next horizontal retrace.
*/
uae_u32 picasso_WaitVerticalSync (void)
{
    P96TRACE(("WaitVerticalSync()\n"));
    DX_WaitVerticalSync();
    return 1;
}

/* NOTE: Watch for those planeptrs of 0x00000000 and 0xFFFFFFFF for all zero / all one bitmaps !!!! */
static void PlanarToChunky(struct RenderInfo *ri, struct BitMap *bm,
			   unsigned long srcx, unsigned long srcy,
			   unsigned long dstx, unsigned long dsty,
			   unsigned long width, unsigned long height,
			   uae_u8 mask)
{
    int j;
    
    uae_u8 *PLANAR[8], *image = ri->Memory + dstx * GetBytesPerPixel (ri->RGBFormat) + dsty*ri->BytesPerRow;
    int Depth = bm->Depth;
    unsigned long rows, bitoffset = srcx & 7;
    long eol_offset;
    
    /* if (mask != 0xFF) 
    write_log ("P2C - pixel-width = %d, bit-offset = %d\n", width, bitoffset); */
    
    /* Set up our bm->Planes[] pointers to the right horizontal offset */
    for (j = 0; j < Depth; j++) {
	uae_u8 *p = bm->Planes[j];
	if (p != &all_zeros_bitmap && p != &all_ones_bitmap)
	    p += srcx/8 + srcy*bm->BytesPerRow;
	PLANAR[j] = p;
	if ((mask & (1 << j)) == 0)
	    PLANAR[j] = &all_zeros_bitmap;
    }
    eol_offset = (long)bm->BytesPerRow - (long)((width + 7) >> 3);
    for (rows = 0; rows < height; rows++, image += ri->BytesPerRow) {
	unsigned long cols;
	
	for (cols = 0; cols < width; cols += 8) {
	    int k;
	    uae_u32 a = 0, b = 0;
	    unsigned int msk = 0xFF;
	    long tmp = cols + 8 - width;
	    if (tmp > 0) {
		msk <<= tmp;
		b = do_get_mem_long ((uae_u32 *)(image + cols + 4));
		if (tmp < 4)
		    b &= 0xFFFFFFFF >> (32 - tmp * 8);
		else if (tmp > 4) {
		    a = do_get_mem_long ((uae_u32 *)(image + cols));
		    a &= 0xFFFFFFFF >> (64 - tmp * 8);
		}
	    }
	    for (k = 0; k < Depth; k++) {
		unsigned int data;
		if (PLANAR[k] == &all_zeros_bitmap)
		    data = 0;
		else if (PLANAR[k] == &all_ones_bitmap)
		    data = 0xFF;
		else {
		    data = (uae_u8)(do_get_mem_word ((uae_u16 *)PLANAR[k]) >> (8 - bitoffset));
		    PLANAR[k]++;
		}
		data &= msk;
		a |= p2ctab[data][0] << k;
		b |= p2ctab[data][1] << k;
	    }
	    do_put_mem_long ((uae_u32 *)(image + cols), a);
	    do_put_mem_long ((uae_u32 *)(image + cols + 4), b);
	}
	for (j = 0; j < Depth; j++) {
	    if (PLANAR[j] != &all_zeros_bitmap && PLANAR[j] != &all_ones_bitmap) {
		PLANAR[j] += eol_offset;
	    }
	}
    }
}

/*
* BlitPlanar2Chunky:
* a0: struct BoardInfo *bi
* a1: struct BitMap *bm - source containing planar information and assorted details
* a2: struct RenderInfo *ri - dest area and its details
* d0.w: SrcX
* d1.w: SrcY
* d2.w: DstX
* d3.w: DstY
* d4.w: SizeX
* d5.w: SizeY
* d6.b: MinTerm - uh oh!
* d7.b: Mask - uh oh!
*
* This function is currently used to blit from planar bitmaps within system memory to chunky bitmaps
* on the board. Watch out for plane pointers that are 0x00000000 (represents a plane with all bits "0")
* or 0xffffffff (represents a plane with all bits "1").
*/
uae_u32 picasso_BlitPlanar2Chunky (void)
{
    uaecptr bm = m68k_areg (regs, 1);
    uaecptr ri = m68k_areg (regs, 2);
    unsigned long srcx = (uae_u16)m68k_dreg (regs, 0);
    unsigned long srcy = (uae_u16)m68k_dreg (regs, 1);
    unsigned long dstx = (uae_u16)m68k_dreg (regs, 2);
    unsigned long dsty = (uae_u16)m68k_dreg (regs, 3);
    unsigned long width = (uae_u16)m68k_dreg (regs, 4);
    unsigned long height = (uae_u16)m68k_dreg (regs, 5);
    uae_u8 minterm = m68k_dreg (regs, 6) & 0xFF;
    uae_u8 mask = m68k_dreg (regs, 7) & 0xFF;
    struct RenderInfo local_ri;
    struct BitMap local_bm;
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     
   
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
#else	
    wgfx_flushline ();
#endif
    
    if (minterm != 0x0C) {
	write_log ("ERROR - BlitPlanar2Chunky() has minterm 0x%x, which I don't handle. Using fall-back routine.\n",
	    minterm);
    }
    else if( CopyRenderInfoStructureA2U (ri, &local_ri) && 
	     CopyBitMapStructureA2U (bm, &local_bm))
    {
	P96TRACE(("BlitPlanar2Chunky(%d, %d, %d, %d, %d, %d) Minterm 0x%x, Mask 0x%x, Depth %d\n",
	    srcx, srcy, dstx, dsty, width, height, minterm, mask, local_bm.Depth));
	P96TRACE(("P2C - BitMap has %d BPR, %d rows\n", local_bm.BytesPerRow, local_bm.Rows));
	PlanarToChunky (&local_ri, &local_bm, srcx, srcy, dstx, dsty, width, height, mask);
	if (renderinfo_is_current_screen (&local_ri))
	{
	    do_blit( &local_ri, GetBytesPerPixel( local_ri.RGBFormat ), dstx, dsty, dstx, dsty, width, height, BLIT_SRC, 0);
	}
	result = 1;
    }

#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/* NOTE: Watch for those planeptrs of 0x00000000 and 0xFFFFFFFF for all zero / all one bitmaps !!!! */
static void PlanarToDirect(struct RenderInfo *ri, struct BitMap *bm,
			   unsigned long srcx, unsigned long srcy,
			   unsigned long dstx, unsigned long dsty,
			   unsigned long width, unsigned long height, uae_u8 mask,
			   struct ColorIndexMapping *cim)
{
    int j;
    int bpp = GetBytesPerPixel(ri->RGBFormat);
    uae_u8 *PLANAR[8];
    uae_u8 *image = ri->Memory + dstx * bpp + dsty * ri->BytesPerRow;
    int Depth = bm->Depth;
    unsigned long rows;
    long eol_offset;
    
    if( !bpp )
        return;
    
    /* Set up our bm->Planes[] pointers to the right horizontal offset */
    for (j = 0; j < Depth; j++) {
	uae_u8 *p = bm->Planes[j];
	if (p != &all_zeros_bitmap && p != &all_ones_bitmap)
	    p += srcx/8 + srcy*bm->BytesPerRow;
	PLANAR[j] = p;
	if ((mask & (1 << j)) == 0)
	    PLANAR[j] = &all_zeros_bitmap;
    }
    
    eol_offset = (long)bm->BytesPerRow - (long)((width + (srcx & 7)) >> 3);
    for (rows = 0; rows < height; rows++, image += ri->BytesPerRow) {
	unsigned long cols;
	uae_u8 *image2 = image;
	unsigned int bitoffs = 7 - (srcx & 7);
	int i;
	
	for (cols = 0; cols < width; cols ++) {
	    int v = 0, k;
	    for (k = 0; k < Depth; k++) {
		if (PLANAR[k] == &all_ones_bitmap)
		    v |= 1 << k;
		else if (PLANAR[k] != &all_zeros_bitmap) {
		    v |= ((*PLANAR[k] >> bitoffs) & 1) << k;
		}
	    }
	    
	    switch (bpp) {
	    case 2:
		do_put_mem_word ((uae_u16 *)image2, (uae_u16)( cim->Colors[v] ) );
		image2 += 2;
		break;
	    case 3:
		do_put_mem_byte (image2++, (uae_u8)cim->Colors[v] );
		do_put_mem_word ((uae_u16 *)image2, (uae_u16)( (cim->Colors[v] & 0x00FFFF00) >> 8) );
		image2 += 2;
		break;
	    case 4:
		do_put_mem_long ((uae_u32 *)image2, cim->Colors[v]);
		image2 += 4;
		break;
	    }
	    bitoffs--;
	    bitoffs &= 7;
	    if (bitoffs == 7) {
		int k;
		for (k = 0; k < Depth; k++) {
		    if (PLANAR[k] != &all_zeros_bitmap && PLANAR[k] != &all_ones_bitmap) {
			PLANAR[k]++;
		    }
		}
	    }
	}
	
	for (i = 0; i < Depth; i++) {
	    if (PLANAR[i] != &all_zeros_bitmap && PLANAR[i] != &all_ones_bitmap) {
		PLANAR[i] += eol_offset;
	    }
	}
    }
}

/*
* BlitPlanar2Direct: 
* 
* Synopsis:
* BlitPlanar2Direct(bi, bm, ri, cim, SrcX, SrcY, DstX, DstY, SizeX, SizeY, MinTerm, Mask);
* Inputs:
* a0:struct BoardInfo *bi
* a1:struct BitMap *bm
* a2:struct RenderInfo *ri
* a3:struct ColorIndexMapping *cmi
* d0.w:SrcX
* d1.w:SrcY
* d2.w:DstX
* d3.w:DstY
* d4.w:SizeX
* d5.w:SizeY
* d6.b:MinTerm
* d7.b:Mask
* 
* This function is currently used to blit from planar bitmaps within system memory to direct color
* bitmaps (15, 16, 24 or 32 bit) on the board. Watch out for plane pointers that are 0x00000000 (represents
* a plane with all bits "0") or 0xffffffff (represents a plane with all bits "1"). The ColorIndexMapping is
* used to map the color index of each pixel formed by the bits in the bitmap's planes to a direct color value
* which is written to the destination RenderInfo. The color mask and all colors within the mapping are words,
* triple bytes or longwords respectively similar to the color values used in FillRect(), BlitPattern() or
* BlitTemplate(). 
*/
uae_u32 picasso_BlitPlanar2Direct (void)
{
    uaecptr bm = m68k_areg (regs, 1);
    uaecptr ri = m68k_areg (regs, 2);
    uaecptr cim = m68k_areg (regs, 3);
    unsigned long srcx = (uae_u16)m68k_dreg (regs, 0);
    unsigned long srcy = (uae_u16)m68k_dreg (regs, 1);
    unsigned long dstx = (uae_u16)m68k_dreg (regs, 2);
    unsigned long dsty = (uae_u16)m68k_dreg (regs, 3);
    unsigned long width = (uae_u16)m68k_dreg (regs, 4);
    unsigned long height = (uae_u16)m68k_dreg (regs, 5);
    uae_u8 minterm = m68k_dreg (regs, 6);
    uae_u8 Mask = m68k_dreg (regs, 7);
    struct RenderInfo local_ri;
    struct BitMap local_bm;
    struct ColorIndexMapping local_cim;
    uae_u32 result = 0;

    special_mem|=picasso_is_special_read|picasso_is_special;     
    
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Unlock();
#else	
    wgfx_flushline ();
#endif
    
    if (minterm != 0x0C) {
	write_log ("WARNING - BlitPlanar2Direct() has unhandled op-code 0x%x. Using fall-back routine.\n",
	    minterm);
    }
    else if( CopyRenderInfoStructureA2U (ri, &local_ri) &&
	     CopyBitMapStructureA2U (bm, &local_bm))
    {
	Mask = 0xFF;
	CopyColorIndexMappingA2U (cim, &local_cim);
	P96TRACE(("BlitPlanar2Direct(%d, %d, %d, %d, %d, %d) Minterm 0x%x, Mask 0x%x, Depth %d\n",
	    srcx, srcy, dstx, dsty, width, height, minterm, Mask, local_bm.Depth));
	PlanarToDirect (&local_ri, &local_bm, srcx, srcy, dstx, dsty, width, height, Mask, &local_cim);
	if (renderinfo_is_current_screen (&local_ri))
	    do_blit( &local_ri, GetBytesPerPixel( local_ri.RGBFormat ), dstx, dsty, dstx, dsty, width, height, BLIT_SRC, 0);
	result = 1;
    }
#ifdef LOCK_UNLOCK_MADNESS
    //PICASSO96_Lock();
#endif
    return result;
}

/* @@@ - Work to be done here!
*
* The address is the offset into our Picasso96 frame-buffer (pointed to by gfxmem_start)
* where the value was put.
*
* Porting work: on some machines you may not need these functions, ie. if the memory for the
* Picasso96 frame-buffer is directly viewable or directly blittable.  On Win32 with DirectX,
* this is not the case.  So I provide some write-through functions (as per Mathias' orders!)
*/
#ifdef PIXEL_LOCK
static void flushpixels( void )
{
    int i,y,x,xbytes,size;
    uae_u8 *dst;
    uaecptr addr,xminaddr=0,xmaxaddr,ydestaddr;
    uae_u32 value;
    
    int lock=0;
    
    if (pixelcount==0)return;
    if (!picasso_on) {
	pixelcount=0;
	return;
    }
	   xmaxaddr=0;
	   //panoffset=picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	   //		  +(picasso96_state.YOffset*picasso96_state.BytesPerRow);
	   
	   DX_Invalidate (1,4000);
#ifndef _DEBUG
	   if(DirectDraw_IsLocked()==FALSE) {
	       dst = gfx_lock_picasso ();
	       lock=1;
	   } else 
#endif
	       dst = picasso96_state.HostAddress;
	   if (!dst)goto out;
	   if( picasso_vidinfo.rgbformat != picasso96_state.RGBFormat )
	   {
	       int psiz = GetBytesPerPixel (picasso_vidinfo.rgbformat);
	       if (picasso96_state.RGBFormat != RGBFB_CHUNKY)
	       {
		   write_log ("ERROR - flushpixels() has non RGBFB_CHUNKY mode!\n");
		   goto out;
	       }
	       for (i=0;i<pixelcount;i++)
	       { 
		   addr=pixelbase[i].addr;
		   value=pixelbase[i].value;	
		   y = addr / picasso96_state.BytesPerRow;
		   
		   if (! picasso_vidinfo.extra_mem)
		       goto next2;	   
		   xbytes = addr - y * picasso96_state.BytesPerRow;
		   //x = xbytes  / picasso96_state.BytesPerPixel;
		   
		   
		   if (xbytes < picasso96_state.Width*picasso96_state.BytesPerPixel && y < picasso96_state.Height)
		   {	
		       if(psiz==4)
		       {
			   int i2;
			   unsigned int addr,val;
			   
			   i2=pixelbase[i].size;
			   addr=dst + y * picasso_vidinfo.rowbytes + ((xbytes)*4);
			   if (i2==4)
			   {
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value)&0xff)];
			       addr+=4;
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value>>8)&0xff)];
			       addr+=4;
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value>>16)&0xff)];
			       addr+=4;
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value>>24)&0xff)];
			       goto next2;
			   }
			   if (i2==2)
			   {
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value>>8)&0xff)];
			       addr+=4;
			       *(uae_u32 *) addr=picasso_vidinfo.clut[((value)&0xff)];
			       goto next2;
			   }
			   if (i2==1)
			   {
			       *(uae_u32 *) addr=picasso_vidinfo.clut[(value&0xff)];
			       goto next2;
			   }
			   
		       }
		       else
		       {
			   int i2;
			   unsigned int addr,val;
			   
			   i2=pixelbase[i].size;
			   addr=dst + y * picasso_vidinfo.rowbytes + ((xbytes)*2);
			   if (i2==4)
			   {
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value)&0xff)];
			       addr+=2;
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value>>8)&0xff)];
			       addr+=2;
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value>>16)&0xff)];
			       addr+=2;
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value>>24)&0xff)];
			       goto next2;
			   }
			   if (i2==2)
			   {
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value>>8)&0xff)];
			       addr+=2;
			       *(uae_u16 *) addr=picasso_vidinfo.clut[((value)&0xff)];
			       goto next2;
			   }
			   if (i2==1)
			   {
			       *(uae_u16 *) addr=picasso_vidinfo.clut[(value&0xff)];
			       goto next2;
			   }
			   
			   
		       }
		   }
		   
next2:;}
	       goto out;  }
	   for (i=0;i<pixelcount;i++)
	   { 
	       addr=pixelbase[i].addr;
	       value=pixelbase[i].value;
	       if(addr>xminaddr && addr<xmaxaddr)
	       {
		   if(pixelbase[i].size==4){
#ifdef SWAPSPEEDUP
    *(uae_u32 *)((addr-xminaddr)+ydestaddr)=value;    
#else      
   do_put_mem_long ((uae_u32 *)((addr-xminaddr)+ydestaddr),value);
#endif			  
			   goto next;
		   }
		   switch (pixelbase[i].size)
		   {
		   case 1:
		       *(uae_u8 *)((addr-xminaddr)+ydestaddr) = value;
		       break;
		   case 2:
		       do_put_mem_word ((uae_u16 *)((addr-xminaddr)+ydestaddr),value);
		       break;	
		   }	
	       }
	       else
	       {
		   y = addr / picasso96_state.BytesPerRow;
		   
		   if (! picasso_vidinfo.extra_mem)
		       goto next;
		   ydestaddr=picasso_vidinfo.rowbytes*y+dst;
		   xminaddr= y*picasso96_state.BytesPerRow;     
		   //xmaxaddr=xminaddr+picasso96_state.BytesPerRow;
		   xmaxaddr=xminaddr+(picasso_vidinfo.width*picasso_vidinfo.pixbytes);
		   
		   xbytes = addr - y * picasso96_state.BytesPerRow;
		   //x = xbytes  / picasso96_state.BytesPerPixel;
		   
		   
		   if (xbytes < picasso96_state.Width*picasso96_state.BytesPerPixel && y < picasso96_state.Height)
		   {		
		       switch (pixelbase[i].size)
		       {
		       case 1:
			   *(uae_u8 *)(dst + y * picasso_vidinfo.rowbytes + xbytes) = value;
			   break;
		       case 2:
			   do_put_mem_word ((uae_u16 *)(dst + y * picasso_vidinfo.rowbytes + xbytes), value);
			   break;
		       case 4:  
#ifdef SWAPSPEEDUP
	   *(uae_u32 *)(dst + y * picasso_vidinfo.rowbytes + xbytes) = value;
#else
				   do_put_mem_long ((uae_u32 *)(dst + y * picasso_vidinfo.rowbytes + xbytes), value);       
#endif	
				   break;		
		       }
		       
		   }
		   else xmaxaddr=0;
	       }
	       
next:;	} 
out:;if(lock)gfx_unlock_picasso ();
    pixelcount=0;
}
#endif

static void write_gfx_long (uaecptr addr, uae_u32 value)
{
    uaecptr oldaddr = addr;
    int y;
#ifdef LOCK_UNLOCK_MADNESS
    int x, xbytes;
    uae_u8 *dst;
#ifdef PIXEL_LOCK
	     addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr > picasso96_state.Address && addr + 4 < picasso96_state.Extent)
    {
	addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	    +(picasso96_state.YOffset*picasso96_state.BytesPerRow);
	if ( pixelcount > MAXFLUSHPIXEL )
	    flushpixels();
	pixelbase[pixelcount].addr=addr;
	pixelbase[pixelcount].value=value;
	pixelbase[pixelcount++].size=4;
    }
    return;
#endif
#endif
	
    if (!picasso_on)
	return;
    
#ifndef LOCK_UNLOCK_MADNESS
	/*
	* Several writes to successive memory locations are a common access pattern.
	* Try to optimize it.
    */
    if (addr >= wgfx_linestart && addr + 4 <= wgfx_lineend) {
	if (addr < wgfx_min)
	    wgfx_min = addr;
	if (addr + 4 > wgfx_max)
	    wgfx_max = addr + 4;
	return;
    } else
    {
#if P96TRACING_LEVEL > 0
	P96TRACE(("write_gfx_long( 0x%x, 0x%x )\n", addr, value ));
#endif
	wgfx_flushline ();
    }
#endif

      addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr < picasso96_state.Address || addr + 4 > picasso96_state.Extent)
	return;
    addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	+(picasso96_state.YOffset*picasso96_state.BytesPerRow);
    y = addr / picasso96_state.BytesPerRow;
    
#ifdef LOCK_UNLOCK_MADNESS
    //DX_Invalidate (y,y);    
    if (! picasso_vidinfo.extra_mem) {
	pixelcount=0;
	return;
    }
    
    xbytes = addr - y * picasso96_state.BytesPerRow;
    //x = xbytes  / picasso96_state.BytesPerPixel;
    
    if (xbytes < (picasso96_state.Width*picasso96_state.BytesPerPixel) && y < picasso96_state.Height)
    {
        
	dst = picasso96_state.HostAddress;
	//dst = gfx_lock_picasso ();
	if (dst) {
	    do_put_mem_long ((uae_u32 *)(dst + y * picasso_vidinfo.rowbytes + xbytes), value);       
	    //gfx_unlock_picasso ();
	}
	else
	    write_log("error\n");
    }
#else
    if (y >= picasso96_state.Height)
	return;
    wgfx_linestart = picasso96_state.Address - gfxmem_start + y * picasso96_state.BytesPerRow;
    wgfx_lineend = wgfx_linestart + picasso96_state.BytesPerRow;
    wgfx_y = y;
    wgfx_min = oldaddr;
    wgfx_max = oldaddr + 4;
#endif
}

static void write_gfx_word (uaecptr addr, uae_u16 value)
{
     uaecptr oldaddr = addr;
    int y;
#ifdef LOCK_UNLOCK_MADNESS
    int x, xbytes;
    uae_u8 *dst;
#ifdef PIXEL_LOCK
    addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr > picasso96_state.Address && addr + 4 < picasso96_state.Extent)
    {
	addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	    +(picasso96_state.YOffset*picasso96_state.BytesPerRow);
	if ( pixelcount > MAXFLUSHPIXEL )
	    flushpixels();
	pixelbase[pixelcount].addr=addr;
	pixelbase[pixelcount].value=value;
	pixelbase[pixelcount++].size=2;
    }
    return;
#endif   
#endif
    if (!picasso_on)
	return;
    
#ifndef LOCK_UNLOCK_MADNESS
    /*
    * Several writes to successive memory locations are a common access pattern.
    * Try to optimize it.
    */
    if (addr >= wgfx_linestart && addr + 2 <= wgfx_lineend) {
	if (addr < wgfx_min)
	    wgfx_min = addr;
	if (addr + 2 > wgfx_max)
	    wgfx_max = addr + 2;
	return;
    } else
	wgfx_flushline ();
#endif
    
    addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr < picasso96_state.Address || addr + 2 > picasso96_state.Extent)
	return;
    addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	+(picasso96_state.YOffset*picasso96_state.BytesPerRow);
    
    y = addr / picasso96_state.BytesPerRow;
    
#ifdef LOCK_UNLOCK_MADNESS
    //DX_Invalidate (y, y);
    if (! picasso_vidinfo.extra_mem) {
	pixelcount=0;
	return;
    }
    
    xbytes = addr - y * picasso96_state.BytesPerRow;
    //x = xbytes / picasso96_state.BytesPerPixel;
    
    if (x < (picasso96_state.Width*picasso96_state.BytesPerPixel) && y < picasso96_state.Height)
    {
	dst = picasso96_state.HostAddress;
	
	//dst = gfx_lock_picasso ();
	if (dst) {
	    do_put_mem_word ((uae_u16 *)(dst + y * picasso_vidinfo.rowbytes + xbytes), value);
	    //gfx_unlock_picasso ();
	}
    }
#else
    if (y >= picasso96_state.Height)
	return;
    wgfx_linestart = picasso96_state.Address - gfxmem_start + y * picasso96_state.BytesPerRow;
    wgfx_lineend = wgfx_linestart + picasso96_state.BytesPerRow;
    wgfx_y = y;
    wgfx_min = oldaddr;
    wgfx_max = oldaddr + 2;
#endif
}

static void write_gfx_byte (uaecptr addr, uae_u8 value)
{
    uaecptr oldaddr = addr;
    int y;
#ifdef LOCK_UNLOCK_MADNESS
    int x, xbytes;
    uae_u8 *dst;
#ifdef PIXEL_LOCK
         addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr > picasso96_state.Address && addr + 4 < picasso96_state.Extent)
    {
	addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	    +(picasso96_state.YOffset*picasso96_state.BytesPerRow);
	if ( pixelcount > MAXFLUSHPIXEL )
	    flushpixels();
	pixelbase[pixelcount].addr=addr;
	pixelbase[pixelcount].value=value;
	pixelbase[pixelcount++].size=1;
    }
    return;
#endif
#endif
    if (!picasso_on)
	return;
#ifndef LOCK_UNLOCK_MADNESS   
	/*
	* Several writes to successive memory locations are a common access pattern.
	* Try to optimize it.
    */
    if (addr >= wgfx_linestart && addr + 4 <= wgfx_lineend) {
	if (addr < wgfx_min)
	    wgfx_min = addr;
	if (addr + 1 > wgfx_max)
	    wgfx_max = addr + 1;
	return;
    } else
	wgfx_flushline ();
#endif

    addr += gfxmem_start;
    /* Check to see if this needs to be written through to the display, or was it an "offscreen" area? */
    if (addr < picasso96_state.Address || addr + 1 > picasso96_state.Extent)
	return;
    addr -= picasso96_state.Address+(picasso96_state.XOffset*picasso96_state.BytesPerPixel)
	+(picasso96_state.YOffset*picasso96_state.BytesPerRow);
    
    y = addr / picasso96_state.BytesPerRow;
    
#ifdef LOCK_UNLOCK_MADNESS
    //DX_Invalidate (y, y);
    if (! picasso_vidinfo.extra_mem) {
	pixelcount=0;
	return;
    }
    
    xbytes = addr - y * picasso96_state.BytesPerRow;
    x = xbytes / picasso96_state.BytesPerPixel;
    
    if (x < picasso96_state.Width && y < picasso96_state.Height) 
	{
	dst = picasso96_state.HostAddress;
		
	//dst = gfx_lock_picasso ();
	if (dst) {
	    *(uae_u8 *)(dst + y * picasso_vidinfo.rowbytes + xbytes) = value;
	    //gfx_unlock_picasso ();
	}
    }
#else
    if (y >= picasso96_state.Height)
	return;
    wgfx_linestart = picasso96_state.Address - gfxmem_start + y * picasso96_state.BytesPerRow;
    wgfx_lineend = wgfx_linestart + picasso96_state.BytesPerRow;
    wgfx_y = y;
    wgfx_min = oldaddr;
    wgfx_max = oldaddr + 1;
#endif
}

static uae_u32 REGPARAM2 gfxmem_lget (uaecptr addr)
{
    uae_u32 *m;

    special_mem|=picasso_is_special_read;  
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u32 *)(gfxmemory + addr);
    return do_get_mem_long(m);
}

static uae_u32 REGPARAM2 gfxmem_wget (uaecptr addr)
{
    uae_u16 *m;
    special_mem|=picasso_is_special_read;  
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u16 *)(gfxmemory + addr);
    return do_get_mem_word(m);
}

static uae_u32 REGPARAM2 gfxmem_bget (uaecptr addr)
{
    special_mem|=picasso_is_special_read;  
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    return gfxmemory[addr];
}

static void REGPARAM2 gfxmem_lput (uaecptr addr, uae_u32 l)
{
    uae_u32 *m;
#ifdef SWAPSPEEDUP
	__asm {         //byteswap now
	mov eax,l
	bswap eax
	mov l,eax
	}
#endif
    special_mem|=picasso_is_special; 
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;

    m = (uae_u32 *)(gfxmemory + addr);
/*                    //only write difference
	__asm {
	mov eax,m
	mov eax,[eax]
	bswap eax
	cmp eax,l
	jne l2
	mov m,0
l2:
    }
	if (!m) return;
*/ 
#ifdef SWAPSPEEDUP	
      *m=l;
#else
	 do_put_mem_long(m, l);
#endif
    /* write the long-word to our displayable memory */
    write_gfx_long(addr, l);
}

static void REGPARAM2 gfxmem_wput (uaecptr addr, uae_u32 w)
{
    uae_u16 *m;
    special_mem|=picasso_is_special; 
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    m = (uae_u16 *)(gfxmemory + addr);
    do_put_mem_word(m, (uae_u16)w);
    
    /* write the word to our displayable memory */
    write_gfx_word(addr, (uae_u16)w);
}

static void REGPARAM2 gfxmem_bput (uaecptr addr, uae_u32 b)
{
    special_mem|=picasso_is_special; 
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    gfxmemory[addr] = b;
    
    /* write the byte to our displayable memory */
    write_gfx_byte(addr, (uae_u8)b);
}

static int REGPARAM2 gfxmem_check (uaecptr addr, uae_u32 size)
{
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    return (addr + size) < allocated_gfxmem;
}

static uae_u8 *REGPARAM2 gfxmem_xlate (uaecptr addr)
{
    addr -= gfxmem_start & gfxmem_mask;
    addr &= gfxmem_mask;
    return gfxmemory + addr;
}

addrbank gfxmem_bank = {
    gfxmem_lget, gfxmem_wget, gfxmem_bget,
	gfxmem_lput, gfxmem_wput, gfxmem_bput,
	gfxmem_xlate, gfxmem_check, NULL
};

/* Call this function first, near the beginning of code flow
* Place in InitGraphics() which seems reasonable...
* Also put it in reset_drawing() for safe-keeping.  */
void InitPicasso96 (void)
{
    have_done_picasso = 0;
    pixelcount = 0;
    palette_changed = 0;
//fastscreen
    oldscr=0;
//fastscreen
    memset (&picasso96_state, 0, sizeof(struct picasso96_state_struct));
    
    if (1) {
	int i, count;
	
	for (i = 0; i < 256; i++) {
	    p2ctab[i][0] = (((i & 128) ? 0x01000000 : 0)
		| ((i & 64) ? 0x010000 : 0)
		| ((i & 32) ? 0x0100 : 0)
		| ((i & 16) ? 0x01 : 0));
	    p2ctab[i][1] = (((i & 8) ? 0x01000000 : 0)
		| ((i & 4) ? 0x010000 : 0)
		| ((i & 2) ? 0x0100 : 0)
		| ((i & 1) ? 0x01 : 0));
	}
	count = 0;
	while (DisplayModes[count].depth >= 0)
	    count++;
	for (i = 0; i < count; i++) {
	    switch (DisplayModes[i].depth) {
	    case 1:
		if (DisplayModes[i].res.width > chunky.width)
		    chunky.width = DisplayModes[i].res.width;
		if (DisplayModes[i].res.height > chunky.height)
		    chunky.height = DisplayModes[i].res.height;
		break;
	    case 2:
		if (DisplayModes[i].res.width > hicolour.width)
		    hicolour.width = DisplayModes[i].res.width;
		if (DisplayModes[i].res.height > hicolour.height)
		    hicolour.height = DisplayModes[i].res.height;
		break;
	    case 3:
		if (DisplayModes[i].res.width > truecolour.width)
		    truecolour.width = DisplayModes[i].res.width;
		if (DisplayModes[i].res.height > truecolour.height)
		    truecolour.height = DisplayModes[i].res.height;
		break;
	    case 4:
		if (DisplayModes[i].res.width > alphacolour.width)
		    alphacolour.width = DisplayModes[i].res.width;
		if (DisplayModes[i].res.height > alphacolour.height)
		    alphacolour.height = DisplayModes[i].res.height;
		break;
	    }
	}
	//ShowSupportedResolutions ();
    }
}

uae_u8 *restore_p96 (uae_u8 *src)
{
    return src;
}

uae_u8 *save_p96 (int *len)
{
    uae_u8 *dstbak,*dst;

    //dstbak = dst = malloc (16 + 12 + 1 + 1);
    return 0;
}




#endif
