/***********************************************************/
//  BeUAE - The Be Un*x Amiga Emulator
//
//  BeOS port keyboard routines
//
//  (c) 2004 Richard Drummond
//  (c) 2000-2001 Axel D�fler
//  (c) 1999 Be/R4 Sound - Raphael Moll
//  (c) 1998-1999 David Sowsy
//  (c) 1996-1998 Christian Bauer
//  (c) 1996 Patrick Hanevold
//
/***********************************************************/

#include <Joystick.h>

#include "be-UAE.h"
#include "be-Window.h"
#include "be-Input.h"

extern "C" {
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "keybuf.h"
#include "inputdevice.h"
#include "hotkeys.h"
#include "keymap/beos_rawkeys.h"
#include "uae.h"
};


static key_info lastKeyInfo;
static bool lastKeyInfoInitialized = false;

// Speed control hacks by David Sowsy
int mouse_speed_rate = 1;

#define key_pressed(k) (keyInfo.key_states[k >> 3] & (1 << (~k & 7)))



//  Poll mouse and keyboard
void handle_events(void)
{
	int be_code,be_byte,be_bit,amiga_code;
	key_info keyInfo;

	if (!lastKeyInfoInitialized)
	{
		get_key_info(&lastKeyInfo);
		lastKeyInfoInitialized = true;
	}

	// Redraw drive LEDs
	/*for (int i=0; i<4; i++)
		DriveLED[i]->SetState(LEDs[i]);*/

	if (gEmulationWindow->UpdateMouseButtons())
	{
		get_key_info(&keyInfo);

		// Keyboard
		if (memcmp(keyInfo.key_states, lastKeyInfo.key_states, sizeof(keyInfo.key_states)))
		{
			for(be_code = 0;be_code < 0x80;be_code++)
			{
				be_byte = be_code >> 3;
				be_bit = 1 << (~be_code & 7);

				// Key state changed?
				if (	(keyInfo.key_states[be_byte] & be_bit)
					!= 	(lastKeyInfo.key_states[be_byte] & be_bit))
				{
					int state = (keyInfo.key_states[be_byte] & be_bit) !=0;
					int ievent;
					if ((ievent = match_hotkey_sequence (be_code, state)))
						handle_hotkey_event (ievent, state);
					else
						inputdevice_translatekeycode (0, be_code, state);
				}
			}
			lastKeyInfo = keyInfo;
		}
	}
}


/*
 * Keyboard inputdevice functions
 */

/* Default translation table */
struct uae_input_device_kbr_default *default_keyboard;

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
    default_keyboard = uaekey_make_default_kbr (beos_keymap);
    inputdevice_setkeytranslation (default_keyboard);
    set_default_hotkeys (beos_hotkeys);
    return 1;
}

static void close_kb (void)
{
    if (default_keyboard) {
        free (default_keyboard);
	default_keyboard = 0;
    }
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

int getcapslockstate (void)
{
    return 0;
}
void setcapslockstate (int state)
{
}
