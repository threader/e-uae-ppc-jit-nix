 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Support for mapping SDL raw keycodes
  *
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "inputdevice.h"
#include "keyboard.h"

#include "keymap/keymap_all.h"
#include "sdlgfx.h"

#include <SDL.h>

/*
 * This stuff is hacked together for now to get
 * raw keyboard support working and tested.
 *
 * A cleaner implementation will be coming . . .
 */
struct sdl_raw_keymap
{
    int sdl_gfx_driver;
    char *name;
    struct uae_input_device_kbr_default *keymap;
    struct uae_hotkeyseq *hotkeys;
};

/*
 * Table used to pick keymapping based on SDL gfx driver
 */
static struct sdl_raw_keymap keymaps[] = {
#if (defined __i386__ || defined __powerpc__ || defined __ppc__) && defined __linux__
    { SDLGFX_DRIVER_X11, "x11pc", keytrans_x11pc, hotkeys_x11pc },
    { SDLGFX_DRIVER_DGA, "x11pc", keytrans_x11pc, hotkeys_x11pc },
#endif
//#if (defined __powerpc__ || defined __ppc__) && defined __APPLE__
//    { SDLGFX_DRIVER_DGA, "quartz", keytrans_quartz, hotkeys_quartz },
//#endif
#ifdef __BEOS__
    { SDLGFX_DRIVER_BWINDOW, "beos", keytrans_beos, hotkeys_beos },
#endif
    { 0, 0, 0, 0 }
};

const void *get_default_raw_keymap (int type)
{
    struct sdl_raw_keymap *k = &keymaps[0];

    while (k->sdl_gfx_driver != type && k->sdl_gfx_driver != 0)
	k++;

    if (k->keymap)
	write_log ("Found %s raw keyboard mapping\n", k->name);

    return k->keymap;
}

struct uae_hotkeyseq *get_default_raw_hotkeys (void)
{
    struct sdl_raw_keymap *k = &keymaps[0];

    while (k->sdl_gfx_driver != get_sdlgfx_type())
	k++;

    return k->hotkeys;
}

#if 0
static int keyhack (int scancode, int pressed, int num)
{
    static int old_modifiers = 0;
    int modifiers = SDL_GetModState();

    if (modifiers != old_modifiers) {
	if ((modifiers & KMOD_LSHIFT) != (old_modifiers & KMOD_LSHIFT))
	     scancode = RAWKEY_LEFT_SHIFT;
	else if ((modifiers & KMOD_RSHIFT) != (old_modifiers & KMOD_RSHIFT))
	    scancode = RAWKEY_RIGHT_SHIFT;
	else if ((modifiers & KMOD_LCTRL) != (old_modifiers & KMOD_LCTRL))
	    scancode = RAWKEY_LEFT_CTRL;
	else if ((modifiers & KMOD_RCTRL) != (old_modifiers & KMOD_RCTRL))
	    scancode = RAWKEY_RIGHT_CTRL;
	else if ((modifiers & KMOD_LALT) != (old_modifiers & KMOD_LALT))
	    scancode = RAWKEY_LEFT_ALT;
	else if ((modifiers & KMOD_RALT) != (old_modifiers & KMOD_RALT))
	    scancode = RAWKEY_RIGHT_ALT;
	else if ((modifiers & KMOD_LMETA) != (old_modifiers & KMOD_LMETA))
	    scancode = RAWKEY_LEFT_SUPER;
	else if ((modifiers & KMOD_RMETA) != (old_modifiers & KMOD_RMETA))
	    scancode = RAWKEY_RIGHT_SUPER;
	else if ((modifiers & KMOD_CAPS) != (old_modifiers & KMOD_CAPS))
	    scancode = RAWKEY_CAPSLOCK;

	old_modifiers = modifiers;
    }
    return scancode;
}
#endif
