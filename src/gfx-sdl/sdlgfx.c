 /*
  * UAE - The Un*x Amiga Emulator
  *
  * SDL graphics support
  *
  * Copyright 2001 Bernd Lachner (EMail: dev@lachner-net.de)
  * Copyright 2003-2004 Richard Drummond
  *
  * Partialy based on the UAE X interface (xwin.c)
  *
  * Copyright 1995, 1996 Bernd Schmidt
  * Copyright 1996 Ed Hanway, Andre Beck, Samuel Devulder, Bruno Coste
  * Copyright 1998 Marcus Sundberg
  * DGA support by Kai Kollmorgen
  * X11/DGA merge, hotkeys and grabmouse by Marcus Sundberg
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <SDL.h>
#include <SDL_endian.h>

#include "config.h"
#include "options.h"
#include "uae.h"
#include "xwin.h"
#include "custom.h"
#include "drawing.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"
#include "debug.h"
#include "picasso96.h"
#include "inputdevice.h"
#include "hotkeys.h"
#include "sdlgfx.h"

/* Uncomment for debugging output */
//#define DEBUG
#ifdef DEBUG
#define DEBUG_LOG write_log
#else
#define DEBUG_LOG(...) do ; while(0)
#endif

/* SDL variable for output surface */
static SDL_Surface *prSDLScreen = NULL;

/* Possible screen modes (x and y resolutions) */
#define MAX_SCREEN_MODES 12
static int x_size_table[MAX_SCREEN_MODES] = { 320, 320, 320, 320, 640, 640, 640, 800, 1024, 1152, 1280, 1280 };
static int y_size_table[MAX_SCREEN_MODES] = { 200, 240, 256, 400, 350, 480, 512, 600, 768,  864,  960,  1024 };

static int red_bits, green_bits, blue_bits;
static int red_shift, green_shift, blue_shift;

#ifdef PICASSO96
static int screen_is_picasso;
static char picasso_invalid_lines[1201];
static int picasso_has_invalid_lines;
static int picasso_invalid_start, picasso_invalid_stop;
static int picasso_maxw = 0, picasso_maxh = 0;
#endif

static int bitdepth, bit_unit;
static int current_width, current_height;

/* If we have to lock the SDL surface, then we remember the address
 * of its pixel data - and recalculate the row maps only when this
 * address changes */
static void *old_pixels;

static SDL_Color arSDLColors[256];
#ifdef PICASSO96
static SDL_Color p96Colors[256];
#endif
static int ncolors;

static int fullscreen;
static int mousegrab;

static int is_hwsurface;
static int hwsurface_is_profitable = 0;

static int have_rawkeys;

/* This isn't supported yet.
 * gui_handle_events() needs to be reworked fist
 */
int pause_emulation;



/*
 * What graphics platform are we running on . . .?
 *
 * Yes, SDL is supposed to abstract away from the underlying
 * platform, but we need to know this to be able to map raw keys
 * and to work around any platform-specific quirks . . .
 */
int get_sdlgfx_type (void)
{
    char name[16] = "";
    static int driver = SDLGFX_DRIVER_UNKNOWN;
    static int search_done = 0;

    if (!search_done) {
	if (SDL_VideoDriverName (name, sizeof name)) {
	    if (strcmp (name, "x11")==0)
		driver = SDLGFX_DRIVER_X11;
	    else if (strcmp (name, "dga") == 0)
		driver = SDLGFX_DRIVER_DGA;
	    else if (strcmp (name, "svgalib") == 0)
		driver = SDLGFX_DRIVER_SVGALIB;
	    else if (strcmp (name, "fbcon") == 0)
		driver = SDLGFX_DRIVER_FBCON;
	    else if (strcmp (name, "directfb") == 0)
		driver = SDLGFX_DRIVER_DIRECTFB;
	    else if (strcmp (name, "Quartz") == 0)
		driver = SDLGFX_DRIVER_QUARTZ;
	    else if (strcmp (name, "bwindow") == 0)
		driver = SDLGFX_DRIVER_BWINDOW;
	}
	search_done = 1;

	DEBUG_LOG ("SDL video driver: %s\n", name);
    }
    return driver;
}


void flush_line (int y)
{
    /* Not implemented for SDL output */
}

void flush_block (int ystart, int ystop)
{
    DEBUG_LOG ("Function: flush_block %d %d\n", ystart, ystop);

    if (SDL_MUSTLOCK (prSDLScreen))
	SDL_UnlockSurface (prSDLScreen);

    SDL_UpdateRect (prSDLScreen, 0, ystart, current_width, ystop - ystart + 1);

    if (SDL_MUSTLOCK (prSDLScreen))
	SDL_LockSurface (prSDLScreen);
}

void flush_screen (int ystart, int ystop)
{
    /* Not implemented for SDL output */
}

void flush_clear_screen (void)
{
    DEBUG_LOG ("Function: flush_clear_screen\n");

    if (prSDLScreen) {
	SDL_Rect rect = { 0, 0, prSDLScreen->w, prSDLScreen->h };
	SDL_FillRect (prSDLScreen, &rect, SDL_MapRGB (prSDLScreen->format, 0,0,0));
	SDL_UpdateRect (prSDLScreen, 0, 0, rect.w, rect.h);
    }
}

int lockscr (void)
{
    DEBUG_LOG ("Function: lockscr\n");

    if (SDL_MUSTLOCK (prSDLScreen)) {
        /* We must lock the SDL surfaces to
	 * access its pixel data
	 */
	if (SDL_LockSurface (prSDLScreen) == 0) {
	    gfxvidinfo.bufmem   = prSDLScreen->pixels;
	    gfxvidinfo.rowbytes = prSDLScreen->pitch;

	    if (prSDLScreen->pixels != old_pixels) {
		/* If the address of the pixel data has
		 * changed, recalculate the row maps
		 */
		init_row_map ();
		old_pixels = prSDLScreen->pixels;
	    }
	    return 1;
	} else
	    /* Failed to lock surface */
	    return 0;
    } else
    	/* We don't need to lock */
	return 1;
}

void unlockscr (void)
{
    DEBUG_LOG ("Function: unlockscr\n");

    if (SDL_MUSTLOCK (prSDLScreen))
	SDL_UnlockSurface (prSDLScreen);
}


STATIC_INLINE int bitsInMask (unsigned long mask)
{
    /* count bits in mask */
    int n = 0;
    while (mask) {
	n += mask & 1;
	mask >>= 1;
    }
    return n;
}

STATIC_INLINE int maskShift (unsigned long mask)
{
    /* determine how far mask is shifted */
    int n = 0;
    while (!(mask & 1)) {
	n++;
	mask >>= 1;
    }
    return n;
}

static int get_color (int r, int g, int b, xcolnr *cnp)
{
    DEBUG_LOG ("Function: get_color\n");

    arSDLColors[ncolors].r = r << 4;
    arSDLColors[ncolors].g = g << 4;
    arSDLColors[ncolors].b = b << 4;
    *cnp = ncolors++;
    return 1;
}

static int init_colors (void)
{
    int i;

    DEBUG_LOG ("Function: init_colors\n");

    if (bitdepth > 8) {
	red_bits    = bitsInMask (prSDLScreen->format->Rmask);
	green_bits  = bitsInMask (prSDLScreen->format->Gmask);
	blue_bits   = bitsInMask (prSDLScreen->format->Bmask);
	red_shift   = maskShift (prSDLScreen->format->Rmask);
	green_shift = maskShift (prSDLScreen->format->Gmask);
	blue_shift  = maskShift (prSDLScreen->format->Bmask);
	alloc_colors64k (red_bits, green_bits, blue_bits, red_shift, green_shift, blue_shift, 0, 0, 0);
    } else {
	alloc_colors256 (get_color);
	SDL_SetColors (prSDLScreen, arSDLColors, 0, 256);
    }

    switch (gfxvidinfo.pixbytes) {
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

    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	switch (gfxvidinfo.pixbytes) {
	    case 4:
		for (i = 0; i < 4096; i++)
		    SDL_Swap32 (xcolors[i]);
		break;
	    case 2:
		for (i = 0; i < 4096; i++)
		    SDL_Swap16 (xcolors[i]);
		break;
	}
    }

    return 1;
}

/*
 * Find the colour depth of the display
 */
static int get_display_depth (void)
{
    const SDL_VideoInfo *vid_info;
    int depth = 0;

    DEBUG_LOG ("Function: get_display_depth()\n");

    if ((vid_info = SDL_GetVideoInfo())) {
	depth = vid_info->vfmt->BitsPerPixel;

	/* Don't trust the answer if it's 16 bits; the colour
	 * depth may actually be 15 bits. Why we can't just
	 * get a straight answer here, I don't know . . . */
	if (depth == 16) {
	    if (get_sdlgfx_type () == SDLGFX_DRIVER_QUARTZ)
		/* Be extra paranoid for MacOS X. 16 bits always means
		 * 15 bits of colour */
		depth = 15;
	    else
		/* Otherwise, we'll count the bits ourselves */
		depth = bitsInMask (vid_info->vfmt->Rmask) +
			bitsInMask (vid_info->vfmt->Gmask) +
			bitsInMask (vid_info->vfmt->Bmask);
	    }
	    DEBUG_LOG ("Display is %d bits deep\n", depth);
    }
    return depth;
}

/*
 * Test whether the screen mode <width>x<height>x<depth> is
 * available. If not, find a standard screen mode which best
 * matches.
 */
static int find_best_mode (int *width, int *height, int depth)
{
    int found = 0;

    DEBUG_LOG ("Function: find_best_mode(%d,%d,%d)\n", *width, *height, depth);

    /* First test whether the specified mode is supported */
    found = SDL_VideoModeOK (*width, *height, depth, SDL_SWSURFACE);

    if (!found) {
	/* The specified mode wasn't available, so we'll try and find
	 * a standard resolution which best matches it.
	 * Note: this should rarely be necessary.
	 */
        int i;
        DEBUG_LOG ("Requested mode not available\n");

	for (i = 0; i < MAX_SCREEN_MODES && !found; i++) {
	    if (x_size_table[i] < *width || y_size_table[i] < *height)
		continue; /* too small - try next mode */
	    found = SDL_VideoModeOK (x_size_table[i], y_size_table[i], bitdepth, SDL_SWSURFACE);
	    if (found) {
		*width  = x_size_table[i];
		*height = y_size_table[i];

		DEBUG_LOG ("Using mode: %dx%d\n", *width, *height);
	    }
	}
    }
    return found;
}

int graphics_setup (void)
{
    int result = 0;

    DEBUG_LOG ("Function: graphics_setup\n");

    if (SDL_WasInit (SDL_INIT_VIDEO) == 0) {
        if (SDL_InitSubSystem (SDL_INIT_VIDEO) == 0) {
	    const SDL_VideoInfo *info = SDL_GetVideoInfo ();

	    if (info != 0) {
	        /* Does the graphics system support hardware-accelerated
		 * fills? If yes, then it's worthwhile using hardware
		 * surfaces for P96 screens
		 */
	        hwsurface_is_profitable = (info->blit_fill);

		DEBUG_LOG ("HW surfaces are profitable: %d",
			   hwsurface_is_profitable);

		result = 1;
	    }
	}
    }
    else
        result = 1;

    /* Don't need to do anything else right now */
    return result;
}

static int graphics_subinit (void)
{
    Uint32 uiSDLVidModFlags = 0;

    DEBUG_LOG ("Function: graphics_subinit\n");

    if (bitdepth == 8)
	uiSDLVidModFlags |= SDL_HWPALETTE;
    if (fullscreen)
	uiSDLVidModFlags |= SDL_FULLSCREEN;
#ifdef PICASSO96
# ifndef __amigaos4__ /* don't use hardware surface on OS4 yet */
   if (screen_is_picasso && hwsurface_is_profitable)
	uiSDLVidModFlags |= SDL_HWSURFACE;
# endif
#endif

    DEBUG_LOG ("Resolution: %d x %d x %d\n", current_width, current_height, bitdepth);

    prSDLScreen = SDL_SetVideoMode (current_width, current_height, bitdepth, uiSDLVidModFlags);

    if (prSDLScreen == NULL) {
	gui_message ("Unable to set video mode: %s\n", SDL_GetError ());
	return 0;
    } else {
	/* Just in case we didn't get exactly what we asked for . . . */
	fullscreen   = ((prSDLScreen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN);
	is_hwsurface = ((prSDLScreen->flags & SDL_HWSURFACE)  == SDL_HWSURFACE);

	/* Are these values what we expected? */
#	ifdef PICASSO96
	    DEBUG_LOG ("P96 screen?    : %d\n", screen_is_picasso);
#	endif
	DEBUG_LOG ("Fullscreen?    : %d\n", fullscreen);
	DEBUG_LOG ("Mouse grabbed? : %d\n", mousegrab);
	DEBUG_LOG ("HW surface?    : %d\n", is_hwsurface);
	DEBUG_LOG ("Must lock?     : %d\n", SDL_MUSTLOCK (prSDLScreen));
	DEBUG_LOG ("Bytes per Pixel: %d\n", prSDLScreen->format->BytesPerPixel);
	DEBUG_LOG ("Bytes per Line : %d\n", prSDLScreen->pitch);

	/* Set UAE window title and icon name */
	SDL_WM_SetCaption (PACKAGE_NAME, PACKAGE_NAME);

        /* Mouse is now always grabbed when full-screen - to work around
	 * problems with full-screen mouse input in some SDL implementations */
	if (fullscreen)
	    SDL_WM_GrabInput (SDL_GRAB_ON);
	else
	    SDL_WM_GrabInput (mousegrab ? SDL_GRAB_ON : SDL_GRAB_OFF);

	/* Hide mouse cursor */
	SDL_ShowCursor (SDL_DISABLE);

        inputdevice_release_all_keys ();
        reset_hotkeys ();

#ifdef PICASSO96
	if (!screen_is_picasso) {
#endif
	    /* Initialize structure for Amiga video modes */
	    if (is_hwsurface) {
		gfxvidinfo.bufmem	= 0;
		gfxvidinfo.emergmem	= malloc (prSDLScreen->pitch);
	    } else {
		gfxvidinfo.bufmem	= prSDLScreen->pixels;
		gfxvidinfo.emergmem	= 0;
	    }
	    gfxvidinfo.linemem		= 0;
	    gfxvidinfo.pixbytes		= prSDLScreen->format->BytesPerPixel;
	    bit_unit			= prSDLScreen->format->BytesPerPixel * 8;
	    gfxvidinfo.rowbytes		= prSDLScreen->pitch;
	    gfxvidinfo.maxblocklines	= 1000;

	    SDL_SetColors (prSDLScreen, arSDLColors, 0, 256);

    	    reset_drawing ();

	    /* Force recalculation of row maps - if we're locking */
	    old_pixels = (void *)-1;
#ifdef PICASSO96
	} else {
	    /* Initialize structure for Picasso96 video modes */
	    picasso_vidinfo.rowbytes	= prSDLScreen->pitch;
	    picasso_vidinfo.extra_mem	= 1;
	    picasso_vidinfo.depth	= bitdepth;
	    picasso_has_invalid_lines	= 0;
	    picasso_invalid_start	= picasso_vidinfo.height + 1;
	    picasso_invalid_stop	= -1;

	    memset (picasso_invalid_lines, 0, sizeof picasso_invalid_lines);
	}
#endif
    }

    return 1;
}

int graphics_init (void)
{
    int success = 0;

    DEBUG_LOG ("Function: graphics_init\n");

    if (currprefs.color_mode > 5) {
	write_log ("Bad color mode selected. Using default.\n");
	currprefs.color_mode = 0;
    }

#ifdef PICASSO96
    screen_is_picasso = 0;
#endif
    fullscreen = currprefs.gfx_afullscreen;
    mousegrab = 0;

    fixup_prefs_dimensions (&currprefs);

    current_width  = currprefs.gfx_width_win;
    current_height = currprefs.gfx_height_win;
    bitdepth       = get_display_depth();

    if (find_best_mode (&current_width, &current_height, bitdepth)) {
	gfxvidinfo.width  = current_width;
	gfxvidinfo.height = current_height;

	if (graphics_subinit ()) {
	    if (init_colors ()) {
		success = 1;
	    }
	}
    }
    return success;
}

static void graphics_subshutdown (void)
{
    DEBUG_LOG ("Function: graphics_subshutdown\n");

    SDL_FreeSurface (prSDLScreen);
    prSDLScreen = 0;

    if (gfxvidinfo.emergmem) {
	free (gfxvidinfo.emergmem);
	gfxvidinfo.emergmem = 0;
    }
}

void graphics_leave (void)
{
    DEBUG_LOG ("Function: graphics_leave\n");

    graphics_subshutdown ();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    dumpcustom ();
}

static int refresh_necessary = 0;

void handle_events (void)
{
    SDL_Event rEvent;

    gui_handle_events ();

    while (SDL_PollEvent (&rEvent)) {
	switch (rEvent.type) {
	    case SDL_QUIT:
		DEBUG_LOG ("Event: quit\n");
		uae_quit();
		break;

	    case SDL_MOUSEBUTTONDOWN:
	    case SDL_MOUSEBUTTONUP: {
		int state = (rEvent.type == SDL_MOUSEBUTTONDOWN);
		int buttonno = -1;

		DEBUG_LOG ("Event: mouse button %d %s\n", rEvent.button.button, state ? "down" : "up");

		switch (rEvent.button.button) {
		    case SDL_BUTTON_LEFT:      buttonno = 0; break;
		    case SDL_BUTTON_MIDDLE:    buttonno = 2; break;
		    case SDL_BUTTON_RIGHT:     buttonno = 1; break;
#ifdef SDL_BUTTON_WHEELUP
		    case SDL_BUTTON_WHEELUP:   if (state) record_key (0x7a << 1); break;
		    case SDL_BUTTON_WHEELDOWN: if (state) record_key (0x7b << 1); break;
#endif
		}
		if (buttonno >= 0)
		    setmousebuttonstate (0, buttonno, rEvent.type == SDL_MOUSEBUTTONDOWN ? 1:0);
		break;
	    }

  	    case SDL_KEYUP:
	    case SDL_KEYDOWN: {
		int state = (rEvent.type == SDL_KEYDOWN);
		int keycode;
		int ievent;

		if (currprefs.map_raw_keys) {
		    keycode = rEvent.key.keysym.scancode;
		    modifier_hack (&keycode, &state);
		} else
		    keycode = rEvent.key.keysym.sym;

		DEBUG_LOG ("Event: key %d %s\n", keycode, state ? "down" : "up");

		if ((ievent = match_hotkey_sequence (keycode, state))) {
		     DEBUG_LOG ("Hotkey event: %d\n", ievent);
		     handle_hotkey_event (ievent, state);
		} else {
		     if (currprefs.map_raw_keys)
			inputdevice_translatekeycode (0, keycode, state);
		     else
			inputdevice_do_keyboard (keysym2amiga (keycode), state);
		}
		break;
	    }

	    case SDL_MOUSEMOTION:
		DEBUG_LOG ("Event: mouse motion\n");

		if (!fullscreen && !mousegrab) {
		    setmousestate (0, 0,rEvent.motion.x, 1);
		    setmousestate (0, 1,rEvent.motion.y, 1);
		} else {
		    setmousestate (0, 0, rEvent.motion.xrel, 0);
		    setmousestate (0, 1, rEvent.motion.yrel, 0);
		}
		break;

	  case SDL_ACTIVEEVENT:
		if (rEvent.active.state & SDL_APPINPUTFOCUS && !rEvent.active.gain) {
		    DEBUG_LOG ("Lost input focus\n");
		    inputdevice_release_all_keys ();
		    reset_hotkeys ();
		}
		break;
	} /* end switch() */
    } /* end while() */

#ifdef PICASSO96
    if (screen_is_picasso && refresh_necessary) {
	SDL_UpdateRect (prSDLScreen, 0, 0, picasso_vidinfo.width, picasso_vidinfo.height);
	refresh_necessary = 0;
	memset (picasso_invalid_lines, 0, sizeof picasso_invalid_lines);
    } else if (screen_is_picasso && picasso_has_invalid_lines) {
	int i;
	int strt = -1;
	picasso_invalid_lines[picasso_vidinfo.height] = 0;
	for (i = picasso_invalid_start; i < picasso_invalid_stop + 2; i++) {
	    if (picasso_invalid_lines[i]) {
		picasso_invalid_lines[i] = 0;
		if (strt != -1)
		    continue;
		strt = i;
	    } else {
		if (strt == -1)
		    continue;
		SDL_UpdateRect (prSDLScreen, 0, strt, picasso_vidinfo.width, i - strt);
		strt = -1;
	    }
	}
	if (strt != -1)
	    abort ();
    }
    picasso_has_invalid_lines = 0;
    picasso_invalid_start = picasso_vidinfo.height + 1;
    picasso_invalid_stop = -1;
#endif
}

static void switch_keymaps (void)
{
    if (currprefs.map_raw_keys) {
        if (have_rawkeys) {
	    set_default_hotkeys (get_default_raw_hotkeys ());
	    write_log ("Using raw keymap\n");
	} else {
	    currprefs.map_raw_keys = changed_prefs.map_raw_keys = 0;
	    write_log ("Raw keys not supported\n");
	}
    }
    if (!currprefs.map_raw_keys) {
	set_default_hotkeys (get_default_cooked_hotkeys ());
	write_log ("Using cooked keymap\n");
    }
}

int check_prefs_changed_gfx (void)
{
    if (changed_prefs.map_raw_keys != currprefs.map_raw_keys) {
	switch_keymaps ();
	currprefs.map_raw_keys = changed_prefs.map_raw_keys;
    }

    if (changed_prefs.gfx_width_win  != currprefs.gfx_width_win
     || changed_prefs.gfx_height_win != currprefs.gfx_height_win
     || changed_prefs.gfx_width_fs   != currprefs.gfx_width_fs
     || changed_prefs.gfx_height_fs  != currprefs.gfx_height_fs) {
	fixup_prefs_dimensions (&changed_prefs);
    } else if (changed_prefs.gfx_lores          == currprefs.gfx_lores
	    && changed_prefs.gfx_linedbl        == currprefs.gfx_linedbl
	    && changed_prefs.gfx_correct_aspect == currprefs.gfx_correct_aspect
	    && changed_prefs.gfx_xcenter        == currprefs.gfx_xcenter
	    && changed_prefs.gfx_ycenter        == currprefs.gfx_ycenter
	    && changed_prefs.gfx_afullscreen    == currprefs.gfx_afullscreen
	    && changed_prefs.gfx_pfullscreen    == currprefs.gfx_pfullscreen) {
	return 0;
    }

    DEBUG_LOG ("Function: check_prefs_changed_gfx\n");

#ifdef PICASSO96
    if (!screen_is_picasso)
	graphics_subshutdown ();
#endif

    currprefs.gfx_width_win	 = changed_prefs.gfx_width_win;
    currprefs.gfx_height_win	 = changed_prefs.gfx_height_win;
    currprefs.gfx_width_fs	 = changed_prefs.gfx_width_fs;
    currprefs.gfx_height_fs	 = changed_prefs.gfx_height_fs;
    currprefs.gfx_lores		 = changed_prefs.gfx_lores;
    currprefs.gfx_linedbl	 = changed_prefs.gfx_linedbl;
    currprefs.gfx_correct_aspect = changed_prefs.gfx_correct_aspect;
    currprefs.gfx_xcenter	 = changed_prefs.gfx_xcenter;
    currprefs.gfx_ycenter	 = changed_prefs.gfx_ycenter;
    currprefs.gfx_afullscreen	 = changed_prefs.gfx_afullscreen;
    currprefs.gfx_pfullscreen	 = changed_prefs.gfx_pfullscreen;

    gui_update_gfx ();

#ifdef PICASSO96
    if (!screen_is_picasso)
#endif
	graphics_subinit ();

    return 0;
}

int debuggable (void)
{
    return 1;
}

int needmousehack (void)
{
    return 1;
}

void LED (int on)
{
}

#ifdef PICASSO96

void DX_Invalidate (int first, int last)
{
    DEBUG_LOG ("Function: DX_Invalidate %i - %i\n", first, last);

#ifndef __amigaos4__
    if (is_hwsurface)
	/* Not necessary for hardware surfaces - except the current
	 * SDL implementation for OS4 which has a skewed notion of
	 * what constitutes a hardware surface. ;-) */
	return;
#endif

    if (first > last)
	return;

    picasso_has_invalid_lines = 1;
    if (first < picasso_invalid_start)
	picasso_invalid_start = first;
    if (last > picasso_invalid_stop)
	picasso_invalid_stop = last;

    while (first <= last) {
	picasso_invalid_lines[first] = 1;
	first++;
    }
}

int DX_BitsPerCannon (void)
{
    return 8;
}

static int palette_update_start = 256;
static int palette_update_end   = 0;

void DX_SetPalette (int start, int count)
{
    DEBUG_LOG ("Function: DX_SetPalette_real\n");

    if (! screen_is_picasso || picasso96_state.RGBFormat != RGBFB_CHUNKY)
	return;

    if (picasso_vidinfo.pixbytes != 1) {
	/* This is the case when we're emulating a 256 color display. */
	while (count-- > 0) {
	    int r = picasso96_state.CLUT[start].Red;
	    int g = picasso96_state.CLUT[start].Green;
	    int b = picasso96_state.CLUT[start].Blue;
	    picasso_vidinfo.clut[start++] =
	    			 (doMask256 (r, red_bits, red_shift)
				| doMask256 (g, green_bits, green_shift)
				| doMask256 (b, blue_bits, blue_shift));
	}
    } else {
	int i;
	for (i = start; i < start+count && i < 256;  i++) {
	    p96Colors[i].r = picasso96_state.CLUT[i].Red;
	    p96Colors[i].g = picasso96_state.CLUT[i].Green;
	    p96Colors[i].b = picasso96_state.CLUT[i].Blue;
	}
	SDL_SetColors (prSDLScreen, &p96Colors[start], start, count);
    }
}

void DX_SetPalette_vsync(void)
{
    if (palette_update_end > palette_update_start) {
	DX_SetPalette (palette_update_start,
				palette_update_end - palette_update_start);
    palette_update_end   = 0;
    palette_update_start = 0;
  }
}

int DX_Fill (int dstx, int dsty, int width, int height, uae_u32 color, RGBFTYPE rgbtype)
{
    int result = 0;
    SDL_Rect rect = {dstx, dsty, width, height};

    DEBUG_LOG ("DX_Fill (x:%d y:%d w:%d h:%d color=%08x)\n", dstx, dsty, width, height, color);

    if (SDL_FillRect (prSDLScreen, &rect, color) == 0) {
	DX_Invalidate (dsty, dsty + height - 1);
	result = 1;
    }
    return result;
}

int DX_Blit (int srcx, int srcy, int dstx, int dsty, int width, int height, BLIT_OPCODE opcode)
{
    int result = 0;
    SDL_Rect src_rect  = {srcx, srcy, width, height};
    SDL_Rect dest_rect = {dstx, dsty, 0, 0};

    DEBUG_LOG ("DX_Blit (sx:%d sy:%d dx:%d dy:%d w:%d h:%d op:%d)\n",
	       srcx, srcy, dstx, dsty, width, height, opcode);

    if (opcode == BLIT_SRC && SDL_BlitSurface (prSDLScreen, &src_rect, prSDLScreen, &dest_rect) == 0) {
        DX_Invalidate (dsty, dsty + height - 1);
	result = 1;
    }
    return result;
}

int DX_FillResolutions (uae_u16 *ppixel_format)
{
    int i, count = 0;
    int w = 0;
    int h = 0;
    int emulate_chunky = 0;

    DEBUG_LOG ("Function: DX_FillResolutions\n");

    /* In the new scheme of things, this function is called *before* graphics_init.
     * Hence, we need to find the display depth ourselves - Rich */
    bitdepth = get_display_depth ();
    bit_unit = (bitdepth + 1) & 0xF8;

    /* Find out, which is the highest resolution the SDL can offer */
    for (i = MAX_SCREEN_MODES-1; i>=0; i--) {
	if ( SDL_VideoModeOK (x_size_table[i], y_size_table[i],
						bitdepth, SDL_HWSURFACE | SDL_FULLSCREEN)) {
	    w = x_size_table[i];
	    h = y_size_table[i];
	    break;
	}
    }

    DEBUG_LOG ("Max. Picasso screen size: %d x %d\n", w, h);

#ifdef WORDS_BIGENDIAN
    picasso_vidinfo.rgbformat = (bit_unit == 8 ? RGBFB_CHUNKY
				: bitdepth == 15 && bit_unit == 16 ? RGBFB_R5G5B5
				: bitdepth == 16 && bit_unit == 16 ? RGBFB_R5G6B5
				: bit_unit == 24 ? RGBFB_B8G8R8
				: bit_unit == 32 ? RGBFB_A8R8G8B8
				: RGBFB_NONE);
#else
    picasso_vidinfo.rgbformat = (bit_unit == 8 ? RGBFB_CHUNKY
				: bitdepth == 15 && bit_unit == 16 ? RGBFB_R5G5B5PC
				: bitdepth == 16 && bit_unit == 16 ? RGBFB_R5G6B5PC
				: bit_unit == 24 ? RGBFB_B8G8R8
				: bit_unit == 32 ? RGBFB_B8G8R8A8
				: RGBFB_NONE);
#endif

    *ppixel_format = 1 << picasso_vidinfo.rgbformat;
    if (bit_unit == 16 || bit_unit == 32) {
	*ppixel_format |= RGBFF_CHUNKY;
	emulate_chunky = 1;
    }

    for (i = 0; i < MAX_SCREEN_MODES && count < MAX_PICASSO_MODES; i++) {
	int j;
	for (j = 0; j <= emulate_chunky && count < MAX_PICASSO_MODES; j++) {
	    if (x_size_table[i] <= w && y_size_table[i] <= h) {
		if (x_size_table[i] > picasso_maxw)
		    picasso_maxw = x_size_table[i];
		if (y_size_table[i] > picasso_maxh)
		    picasso_maxh = y_size_table[i];
		DisplayModes[count].res.width = x_size_table[i];
		DisplayModes[count].res.height = y_size_table[i];
		DisplayModes[count].depth = j == 1 ? 1 : bit_unit >> 3;
		DisplayModes[count].refresh = 75;

 		DEBUG_LOG ("Picasso resolution %d x %d @ %d allowed\n",
			DisplayModes[count].res.width,
			DisplayModes[count].res.height,
			DisplayModes[count].depth);

		count++;
	    }
	}
    }
    DEBUG_LOG("Max. Picasso screen size: %d x %d\n", picasso_maxw, picasso_maxh);

    return count;
}

static void set_window_for_picasso (void)
{
    DEBUG_LOG ("Function: set_window_for_picasso\n");

    if (current_width == picasso_vidinfo.width && current_height == picasso_vidinfo.height)
	return;

    graphics_subshutdown();
    current_width  = picasso_vidinfo.width;
    current_height = picasso_vidinfo.height;
    graphics_subinit();
}

void gfx_set_picasso_modeinfo (int w, int h, int depth, int rgbfmt)
{
    DEBUG_LOG ("Function: gfx_set_picasso_modeinfo w: %i h: %i depth: %i rgbfmt: %i\n", w, h, depth, rgbfmt);

    picasso_vidinfo.width = w;
    picasso_vidinfo.height = h;
    picasso_vidinfo.depth = depth;
    picasso_vidinfo.pixbytes = bit_unit >> 3;
    if (screen_is_picasso)
	set_window_for_picasso();
}

void gfx_set_picasso_baseaddr (uaecptr a)
{
}

void gfx_set_picasso_state (int on)
{
    DEBUG_LOG ("Function: gfx_set_picasso_state: %d\n", on);

    if (on == screen_is_picasso)
	return;

    graphics_subshutdown ();
    screen_is_picasso = on;

    if (on) {
	// Set height, width for Picasso gfx
	current_width  = picasso_vidinfo.width;
	current_height = picasso_vidinfo.height;
	graphics_subinit ();
    } else {
	// Set height, width for Amiga gfx
	current_width  = gfxvidinfo.width;
	current_height = gfxvidinfo.height;
	graphics_subinit ();
    }

    if (on)
	DX_SetPalette (0, 256);
}

uae_u8 *gfx_lock_picasso (void)
{
    DEBUG_LOG ("Function: gfx_lock_picasso\n");

    if (SDL_MUSTLOCK (prSDLScreen))
	SDL_LockSurface (prSDLScreen);
    picasso_vidinfo.rowbytes = prSDLScreen->pitch;
    return prSDLScreen->pixels;
}

void gfx_unlock_picasso (void)
{
    DEBUG_LOG ("Function: gfx_unlock_picasso\n");

    if (SDL_MUSTLOCK (prSDLScreen))
	SDL_UnlockSurface (prSDLScreen);
}
#endif /* PICASSO96 */

int is_fullscreen (void)
{
    return fullscreen;
}

void toggle_fullscreen (void)
{
    /* FIXME: Add support for separate full-screen/windowed sizes */
    fullscreen = 1 - fullscreen;

    /* Close existing window and open a new one (with the new fullscreen setting) */
    graphics_subshutdown ();
    graphics_subinit ();

    notice_screen_contents_lost ();

    DEBUG_LOG ("ToggleFullScreen: %d\n", fullscreen );
};

void toggle_mousegrab (void)
{
    if (!fullscreen) {
	if (SDL_WM_GrabInput (SDL_GRAB_QUERY) == SDL_GRAB_OFF) {
	    SDL_WM_GrabInput (SDL_GRAB_ON);
	    SDL_WarpMouse (0, 0);
	    mousegrab = 1;
	} else {
	    SDL_WM_GrabInput (SDL_GRAB_OFF);
	    mousegrab = 0;
	}
    }
}

void screenshot (int mode)
{
   write_log ("Screenshot not supported yet\n");
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

/*
 * Mouse inputdevice functions
 */

/* Hardwire for 3 axes and 3 buttons - although SDL doesn't
 * currently support a Z-axis as such. Mousewheel events are supplied
 * as buttons 4 and 5
 */
#define MAX_BUTTONS	3
#define MAX_AXES	3
#define FIRST_AXIS	0
#define FIRST_BUTTON	MAX_AXES

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
    return 255; // fix me
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

static int init_kb (void)
{
    struct uae_input_device_kbr_default *keymap = 0;

    /* We need SDL video to be initialized */
    graphics_setup ();

    /* See if we support raw keys on this platform */
    if ((keymap = get_default_raw_keymap (get_sdlgfx_type ())) != 0) {
	inputdevice_setkeytranslation (keymap);
	have_rawkeys = 1;
    }
    switch_keymaps ();

    return 1;
}

static void close_kb (void)
{
}

static int keyhack (int scancode, int pressed, int num)
{
    return scancode;
}

static void read_kb (void)
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

//static int capslockstate;

int getcapslockstate (void)
{
// TODO
//    return capslockstate;
    return 0;
}
void setcapslockstate (int state)
{
// TODO
//    capslockstate = state;
}


/*
 * Default inputdevice config for SDL mouse
 */
void input_get_default_mouse (struct uae_input_device *uid)
{
    /* SDL supports only one mouse */
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
    uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
    uid[0].enabled = 1;
}

/*
 * Handle gfx specific cfgfile options
 */
void gfx_default_options (struct uae_prefs *p)
{
    p->map_raw_keys = 0;
}

void gfx_save_options (FILE *f, struct uae_prefs *p)
{
    cfgfile_write (f, GFX_NAME ".map_raw_keys=%s\n", p->map_raw_keys ? "true" : "false");
}

int gfx_parse_option (struct uae_prefs *p, char *option, char *value)
{
    int result = (cfgfile_yesno (option, value, "map_raw_keys", &p->map_raw_keys));

    return result;
}
