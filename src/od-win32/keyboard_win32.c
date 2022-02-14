/*
 * UAE - The Un*x Amiga Emulator
 *
 * Additional Win32 helper functions not calling any system routines
 *
 * (c) 1997 Mathias Ortmann
 * (c) 1999-2001 Brian King
 * (c) 2000-2001 Bernd Roesch
 * (c) 2002 Toni Wilen
 */

#include "config.h"
#include "sysconfig.h"

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <windows.h>
#include <dinput.h>

#include "sysdeps.h"
#include "uae.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "events.h"
#include "newcpu.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "xwin.h"
#include "drawing.h"
#include "disk.h"
#include "keybuf.h"
#include "win32.h"
#include "debug.h"
#include "ar.h"
#include "ahidsound.h"
#include "savestate.h"
#include "sound.h"

extern void screenshot(int);

//#define DBG_KEYBD 1
//#define DEBUG_KBD

static struct uae_input_device_kbr_default keytrans[] = {

    { DIK_ESCAPE, INPUTEVENT_KEY_ESC },
    
    { DIK_F1, INPUTEVENT_KEY_F1 },
    { DIK_F2, INPUTEVENT_KEY_F2 },
    { DIK_F3, INPUTEVENT_KEY_F3 },
    { DIK_F4, INPUTEVENT_KEY_F4 },
    { DIK_F5, INPUTEVENT_KEY_F5 },
    { DIK_F6, INPUTEVENT_KEY_F6 },
    { DIK_F7, INPUTEVENT_KEY_F7 },
    { DIK_F8, INPUTEVENT_KEY_F8 },
    { DIK_F9, INPUTEVENT_KEY_F9 },
    { DIK_F10, INPUTEVENT_KEY_F10 },

    { DIK_1, INPUTEVENT_KEY_1 },
    { DIK_2, INPUTEVENT_KEY_2 },
    { DIK_3, INPUTEVENT_KEY_3 },
    { DIK_4, INPUTEVENT_KEY_4 },
    { DIK_5, INPUTEVENT_KEY_5 },
    { DIK_6, INPUTEVENT_KEY_6 },
    { DIK_7, INPUTEVENT_KEY_7 },
    { DIK_8, INPUTEVENT_KEY_8 },
    { DIK_9, INPUTEVENT_KEY_9 },
    { DIK_0, INPUTEVENT_KEY_0 },

    { DIK_TAB, INPUTEVENT_KEY_TAB },

    { DIK_A, INPUTEVENT_KEY_A },
    { DIK_B, INPUTEVENT_KEY_B },
    { DIK_C, INPUTEVENT_KEY_C },
    { DIK_D, INPUTEVENT_KEY_D },
    { DIK_E, INPUTEVENT_KEY_E },
    { DIK_F, INPUTEVENT_KEY_F },
    { DIK_G, INPUTEVENT_KEY_G },
    { DIK_H, INPUTEVENT_KEY_H },
    { DIK_I, INPUTEVENT_KEY_I },
    { DIK_J, INPUTEVENT_KEY_J },
    { DIK_K, INPUTEVENT_KEY_K },
    { DIK_L, INPUTEVENT_KEY_L },
    { DIK_M, INPUTEVENT_KEY_M },
    { DIK_N, INPUTEVENT_KEY_N },
    { DIK_O, INPUTEVENT_KEY_O },
    { DIK_P, INPUTEVENT_KEY_P },
    { DIK_Q, INPUTEVENT_KEY_Q },
    { DIK_R, INPUTEVENT_KEY_R },
    { DIK_S, INPUTEVENT_KEY_S },
    { DIK_T, INPUTEVENT_KEY_T },
    { DIK_U, INPUTEVENT_KEY_U },
    { DIK_W, INPUTEVENT_KEY_W },
    { DIK_V, INPUTEVENT_KEY_V },
    { DIK_X, INPUTEVENT_KEY_X },
    { DIK_Y, INPUTEVENT_KEY_Y },
    { DIK_Z, INPUTEVENT_KEY_Z },

    { DIK_CAPITAL, INPUTEVENT_KEY_CAPS_LOCK },

    { DIK_NUMPAD1, INPUTEVENT_KEY_NP_1 },
    { DIK_NUMPAD2, INPUTEVENT_KEY_NP_2 },
    { DIK_NUMPAD3, INPUTEVENT_KEY_NP_3 },
    { DIK_NUMPAD4, INPUTEVENT_KEY_NP_4 },
    { DIK_NUMPAD5, INPUTEVENT_KEY_NP_5 },
    { DIK_NUMPAD6, INPUTEVENT_KEY_NP_6 },
    { DIK_NUMPAD7, INPUTEVENT_KEY_NP_7 },
    { DIK_NUMPAD8, INPUTEVENT_KEY_NP_8 },
    { DIK_NUMPAD9, INPUTEVENT_KEY_NP_9 },
    { DIK_NUMPAD0, INPUTEVENT_KEY_NP_0 },
    { DIK_DECIMAL, INPUTEVENT_KEY_NP_PERIOD },
    { DIK_ADD, INPUTEVENT_KEY_NP_ADD },
    { DIK_SUBTRACT, INPUTEVENT_KEY_NP_SUB },
    { DIK_MULTIPLY, INPUTEVENT_KEY_NP_MUL },
    { DIK_DIVIDE, INPUTEVENT_KEY_NP_DIV },
    { DIK_NUMPADENTER, INPUTEVENT_KEY_ENTER },

    { DIK_MINUS, INPUTEVENT_KEY_SUB },
    { DIK_EQUALS, INPUTEVENT_KEY_EQUALS },
    { DIK_BACK, INPUTEVENT_KEY_BACKSPACE },
    { DIK_RETURN, INPUTEVENT_KEY_RETURN },
    { DIK_SPACE, INPUTEVENT_KEY_SPACE },

    { DIK_LSHIFT, INPUTEVENT_KEY_SHIFT_LEFT },
    { DIK_LCONTROL, INPUTEVENT_KEY_CTRL },
    { DIK_LWIN, INPUTEVENT_KEY_AMIGA_LEFT },
    { DIK_LMENU, INPUTEVENT_KEY_ALT_LEFT },
    { DIK_RMENU, INPUTEVENT_KEY_ALT_RIGHT },
    { DIK_RWIN, INPUTEVENT_KEY_AMIGA_RIGHT },
    { DIK_APPS, INPUTEVENT_KEY_AMIGA_RIGHT },
    { DIK_RCONTROL, INPUTEVENT_KEY_CTRL_RIGHT },
    { DIK_RSHIFT, INPUTEVENT_KEY_SHIFT_RIGHT },

    { DIK_UP, INPUTEVENT_KEY_CURSOR_UP },
    { DIK_DOWN, INPUTEVENT_KEY_CURSOR_DOWN },
    { DIK_LEFT, INPUTEVENT_KEY_CURSOR_LEFT },
    { DIK_RIGHT, INPUTEVENT_KEY_CURSOR_RIGHT },

    { DIK_INSERT, INPUTEVENT_KEY_AMIGA_LEFT },
    { DIK_DELETE, INPUTEVENT_KEY_DEL },
    { DIK_HOME, INPUTEVENT_KEY_AMIGA_RIGHT },
    { DIK_NEXT, INPUTEVENT_KEY_HELP },

    { DIK_LBRACKET, INPUTEVENT_KEY_LEFTBRACKET },
    { DIK_RBRACKET, INPUTEVENT_KEY_RIGHTBRACKET },
    { DIK_SEMICOLON, INPUTEVENT_KEY_SEMICOLON },
    { DIK_APOSTROPHE, INPUTEVENT_KEY_SINGLEQUOTE },
    { DIK_GRAVE, INPUTEVENT_KEY_BACKQUOTE },
    { DIK_BACKSLASH, INPUTEVENT_KEY_BACKSLASH },
    { DIK_COMMA, INPUTEVENT_KEY_COMMA },
    { DIK_PERIOD, INPUTEVENT_KEY_PERIOD },
    { DIK_SLASH, INPUTEVENT_KEY_DIV },
    { DIK_OEM_102, INPUTEVENT_KEY_30 },
    { -1, 0 }
};

extern int ispressed (int key);

static int endpressed (void)
{
    return ispressed(DIK_END);
}

static int shiftpressed (void)
{
    return ispressed (DIK_LSHIFT) || ispressed (DIK_RSHIFT);
}

static int altpressed (void)
{
    return ispressed (DIK_LMENU) || ispressed (DIK_RMENU);
}

static int ctrlpressed (void)
{
    return ispressed (DIK_LCONTROL) || ispressed (DIK_RCONTROL);
}

static int capslockstate;

int getcapslock (void)
{
    int newstate;

    BYTE keyState[256];
    GetKeyboardState (keyState);
    newstate = keyState[VK_CAPITAL] & 1;
    if (newstate != capslockstate)
        inputdevice_translatekeycode (0, DIK_CAPITAL, newstate);
    capslockstate = newstate;
    return capslockstate;
}

#ifdef CD32
extern int cd32_enabled;

static int handlecd32 (int scancode, int state)
{
    int e = 0;
    if (!cd32_enabled)
	return 0;
    switch (scancode)
    {
	case DIK_NUMPAD7:
	e = INPUTEVENT_JOY2_CD32_GREEN;
	break;
	case DIK_NUMPAD9:
	e = INPUTEVENT_JOY2_CD32_YELLOW;
	break;
	case DIK_NUMPAD1:
	e = INPUTEVENT_JOY2_CD32_RED;
	break;
	case DIK_NUMPAD3:
	e = INPUTEVENT_JOY2_CD32_BLUE;
	break;
	case DIK_DIVIDE:
	e = INPUTEVENT_JOY2_CD32_RWD;
	break;
	case DIK_MULTIPLY:
	e = INPUTEVENT_JOY2_CD32_PLAY;
	break;
	case DIK_SUBTRACT:
	e = INPUTEVENT_JOY2_CD32_FFW;
	break;
    }
    if (!e)
	return 0;
    handle_input_event (e, state, 1, 0);
    return 1;
}
#endif

void clearallkeys (void)
{
    inputdevice_updateconfig (&currprefs);
}

static int np[] = { DIK_NUMPAD0, 0, DIK_NUMPADPERIOD, 0, DIK_NUMPAD1, 1, DIK_NUMPAD2, 2,
    DIK_NUMPAD3, 3, DIK_NUMPAD4, 4, DIK_NUMPAD5, 5, DIK_NUMPAD6, 6, DIK_NUMPAD7, 7,
    DIK_NUMPAD8, 8, DIK_NUMPAD9, 9, -1 };

void my_kbd_handler (int keyboard, int scancode, int newstate)
{
//    write_log( "keyboard = %d scancode = 0x%02.2x state = %d\n", keyboard, scancode, newstate ); 
    if (newstate) {
	switch (scancode)
	{
	    case DIK_F12:
	    if (ctrlpressed ()) {
		fullscreentoggle();
	    } else if (shiftpressed () || endpressed ()) {
	        disablecapture ();
		activate_debugger ();
	    } else {
		WIN32GUI_DisplayGUI (-1);
	    }
	    return;
	    case DIK_F11:
	    if (currprefs.win32_ctrl_F11_is_quit) {
		if (ctrlpressed()) {
		    uae_quit();
		    return;
		}
	    }
	    break;
	    case DIK_F1:
	    case DIK_F2:
	    case DIK_F3:
	    case DIK_F4:
	    if (endpressed ()) {
		if (shiftpressed ())
		    disk_eject (scancode - DIK_F1);
		else
		    WIN32GUI_DisplayGUI (scancode - DIK_F1);
		return;
	    }
	    break;
	    case DIK_F5:
	    if (endpressed ()) {
		WIN32GUI_DisplayGUI (shiftpressed () ? 5 : 4);
		return;
	    }
	    break;
#if 0
	    case DIK_F6:
	    if (endpressed ()) {
		if (shiftpressed ()) {
		    if(savestate_filename && strlen (savestate_filename))
			savestate_state = STATE_DOSAVE;
		} else {		
		    if(savestate_filename && strlen (savestate_filename))
			savestate_state = STATE_DORESTORE;
		}
		return;
	    }
#endif
	    break;
	    case DIK_NUMPAD0:
	    case DIK_NUMPAD1:
	    case DIK_NUMPAD2:
	    case DIK_NUMPAD3:
	    case DIK_NUMPAD4:
	    case DIK_NUMPAD5:
	    case DIK_NUMPAD6:
	    case DIK_NUMPAD7:
	    case DIK_NUMPAD8:
	    case DIK_NUMPAD9:
	    case DIK_NUMPADPERIOD:
	    if (endpressed ()) {
		int i = 0, v = -1;
		while (np[i] >= 0) {
		    v = np[i + 1];
		    if (np[i] == scancode)
			break;
		    i+=2;
		}
		if (v >= 0)
		    savestate_quick (v, shiftpressed() || ctrlpressed ());
		return;
	    }
	    break;
	    case DIK_SYSRQ:
	    screenshot (endpressed() ? 1 : 0);
	    break;
	    case DIK_PAUSE:
	    if (endpressed ()) {
		turbo_emulation = turbo_emulation ? 0 : 1;
		if (turbo_emulation)
		    pause_sound ();
		else
		    resume_sound ();
		compute_vsynctime ();
	    } else {
	        pause_emulation = pause_emulation ? 0 : 1;
	    }
	    break;
	    case DIK_SCROLL:
	    toggle_inhibit_frame (IHF_SCROLLLOCK);
	    return;
	    case DIK_PRIOR:
#ifdef ACTION_REPLAY
	    action_replay_freeze ();
#endif
	    return;
	    case DIK_NEXT:
	    break;
	}
    }
    if (endpressed ())
	return;

    if (scancode == DIK_CAPITAL) {
	if (!newstate)
	    return;
	capslockstate ^= 1;
	newstate = capslockstate;
    }

    if (currprefs.input_selected_setting == 0) {
#ifdef CD32
        if (handlecd32 (scancode, newstate))
	    return;
#endif
    }
    inputdevice_translatekeycode (keyboard, scancode, newstate);
}

void keyboard_settrans (void)
{
    inputdevice_setkeytranslation (keytrans);
}

