 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Support for mapping Quartz keycodes to UAE input events
  *
  * Copyright 2004 Richard Drummond
  */

#if defined __APPLE__

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "hotkeys.h"

#define RAWKEY_ESCAPE		53

#define RAWKEY_F1		122
#define RAWKEY_F2		120
#define RAWKEY_F3		99
#define RAWKEY_F4		118
#define RAWKEY_F5		96
#define RAWKEY_F6		97
#define RAWKEY_F7		98
#define RAWKEY_F8		100
#define RAWKEY_F9		101
#define RAWKEY_F10		109
#define RAWKEY_F11		103
#define RAWKEY_F12		111

#define RAWKEY_PRINTSCR		105
#define RAWKEY_SCROLL_LOCK      107
#define RAWKEY_PAUSE            113

#define RAWKEY_1		18
#define RAWKEY_2		19
#define RAWKEY_3		20
#define RAWKEY_4		21
#define RAWKEY_5		23
#define RAWKEY_6		22
#define RAWKEY_7		26
#define RAWKEY_8		28
#define RAWKEY_9		25
#define RAWKEY_0		29

#define RAWKEY_TAB		48
#define RAWKEY_ENTER		36
#define RAWKEY_BACKSPACE	51
#define RAWKEY_SPACE            49

#define RAWKEY_A		0
#define RAWKEY_B		11
#define RAWKEY_C		8
#define RAWKEY_D		2
#define RAWKEY_E		14
#define RAWKEY_F		3
#define RAWKEY_G		5
#define RAWKEY_H		4
#define RAWKEY_I		34
#define RAWKEY_J		38
#define RAWKEY_K		40
#define RAWKEY_L		37
#define RAWKEY_M		46
#define RAWKEY_N		45
#define RAWKEY_O		31
#define RAWKEY_P		35
#define RAWKEY_Q		12
#define RAWKEY_R		15
#define RAWKEY_S		1
#define RAWKEY_T		17
#define RAWKEY_U		32
#define RAWKEY_V		9
#define RAWKEY_W		13
#define RAWKEY_X		7
#define RAWKEY_Y		16
#define RAWKEY_Z		6

#define RAWKEY_MINUS		27
#define RAWKEY_EQUALS		24
#define RAWKEY_LEFTBRACKET	33
#define RAWKEY_RIGHTBRACKET	30
#define RAWKEY_BACKSLASH	42
#define RAWKEY_SEMICOLON	41
#define RAWKEY_SINGLEQUOTE	39
#define RAWKEY_COMMA		43
#define RAWKEY_PERIOD		47
#define RAWKEY_SLASH		44
#define RAWKEY_GRAVE		10
#define RAWKEY_LTGT		50

#define RAWKEY_NUMPAD_1		83
#define RAWKEY_NUMPAD_2		84
#define RAWKEY_NUMPAD_3		85
#define RAWKEY_NUMPAD_4		86
#define RAWKEY_NUMPAD_5		87
#define RAWKEY_NUMPAD_6		88
#define RAWKEY_NUMPAD_7		89
#define RAWKEY_NUMPAD_8		91
#define RAWKEY_NUMPAD_9		92
#define RAWKEY_NUMPAD_0		82
#define RAWKEY_NUMPAD_EQUALS	81
#define RAWKEY_NUMPAD_DIVIDE	75
#define RAWKEY_NUMPAD_MULTIPLY	67
#define RAWKEY_NUMPAD_MINUS	78
#define RAWKEY_NUMPAD_PLUS	69
#define RAWKEY_NUMPAD_PERIOD	65
#define RAWKEY_NUMPAD_ENTER	76
#define RAWKEY_NUMLOCK		71

#define RAWKEY_INSERT		114
#define RAWKEY_DELETE		117
#define RAWKEY_HOME		115
#define RAWKEY_END		119
#define RAWKEY_PAGEUP		116
#define RAWKEY_PAGEDOWN		121

#define RAWKEY_CURSOR_UP	126
#define RAWKEY_CURSOR_DOWN	123
#define RAWKEY_CURSOR_LEFT	125
#define RAWKEY_CURSOR_RIGHT	124

/*
 * MacOS doesn't report raw keycodes for modifier keys.
 * Solution: query modifiers separately and map
 * them to these fake keycodes.
 *
 * Also note that MacOS doesn't seem to distinguish
 * between left and right modfiers - i.e. left shift
 * and right shift produce the modifier event
 */
#define RAWKEY_LEFT_CTRL	128
#define RAWKEY_LEFT_SHIFT	129
#define RAWKEY_LEFT_ALT		130
#define RAWKEY_LEFT_SUPER	131
#define RAWKEY_RIGHT_SUPER	131
#define RAWKEY_RIGHT_ALT	130
//#define RAWKEY_MENU
#define RAWKEY_RIGHT_SHIFT	129
#define RAWKEY_RIGHT_CTRL	128
#define RAWKEY_CAPSLOCK		132

#define RAWKEY_POWER            127
//#define RAWKEY_SLEEP
//#define RAWKEY_WAKE


/* MacOS doesn't report modifier keycodes */
#define MODIFIER_HACK_NEEEDED

/* F12 seems to be broken on Mac keyboards - it doesn't
 * generate key-down events until the key is released
 * Use F11 for control sequences instead
 */
#define HOTKEY_MODIFIER		RAWKEY_F11


#include "quartz_rawkeys.h"
#include "keymap_common.h"
#include "hotkeys_common.h"

const struct uae_input_device_kbr_default keytrans_quartz[] =
{
    { RAWKEYS_COMMON },
    { RAWKEY_PRINTSCR,          INPUTEVENT_SPC_SCREENSHOT },
    { RAWKEY_SCROLL_LOCK,       INPUTEVENT_SPC_INHIBITSCREEN },
    { RAWKEY_PAUSE,             INPUTEVENT_SPC_PAUSE },
    { RAWKEYS_END }
};

const struct uae_hotkeyseq hotkeys_quartz[] =
{
     { DEFAULT_HOTKEYS },
     { HOTKEYS_END }
};

#endif
