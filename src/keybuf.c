 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Keyboard buffer. Not really needed for X, but for SVGAlib and possibly
  * Mac and DOS ports.
  *
  * Note: it's possible to have two threads in UAE, one reading keystrokes
  * and the other one writing them. Despite this, no synchronization effort
  * is needed. This code should be perfectly thread safe. At least if you
  * assume that integer store instructions are atomic.
  *
  * Copyright 1995, 1997 Bernd Schmidt
  */

#include "sysconfig.h"
#include "sysdeps.h"
#include <assert.h>

#include "config.h"
#include "options.h"
#include "keybuf.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "custom.h"

static int fakestate[3][6] = { { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };

static int *fs_np;
static int *fs_ck;
static int *fs_se;

/* Not static so the DOS code can mess with them */
int kpb_first, kpb_last;

static int keybuf[256];

int keys_available (void)
{
    int val;
    val = kpb_first != kpb_last;
    return val;
}

int get_next_key (void)
{
    int key;
    assert (kpb_first != kpb_last);

    key = keybuf[kpb_last];
    if (++kpb_last == 256)
	kpb_last = 0;
    return key;
}

static void do_fake (int nr)
{
    int *fake = fakestate[nr];

    nr += 2;
    setjoystickstate (nr, 0, fake[1] ? -100 : (fake[2] ? 100 : 0), 100);
    setjoystickstate (nr, 1, fake[0] ? -100 : (fake[3] ? 100 : 0), 100);
    setjoybuttonstate (nr, 0, fake[4]);
    setjoybuttonstate (nr, 1, fake[5]);
}

void record_key (int kc)
{
    int fs = 0;
    int kpb_next = kpb_first + 1;

    //write_log ("got kc %02.2X\n", ((kc << 7) | (kc >> 1)) & 0xff);
    if (kpb_next == 256)
	kpb_next = 0;
    if (kpb_next == kpb_last) {
	write_log ("Keyboard buffer overrun. Congratulations.\n");
	return;
    }
    if (fs_np != 0) {
	switch (kc >> 1) {
	case AK_NP8: fs = 1; fs_np[0] = !(kc & 1); break;
	case AK_NP4: fs = 1; fs_np[1] = !(kc & 1); break;
	case AK_NP6: fs = 1; fs_np[2] = !(kc & 1); break;
	case AK_NP2: fs = 1; fs_np[3] = !(kc & 1); break;
	case AK_NP0: case AK_NP5: fs = 1; fs_np[4] = !(kc & 1); break;
	case AK_NPDEL: case AK_NPDIV: case AK_ENT: fs = 1; fs_np[5] = !(kc & 1); break;
	}
    }
    if (fs_ck != 0) {
	switch (kc >> 1) {
	case AK_UP: fs = 1; fs_ck[0] = !(kc & 1); break;
	case AK_LF: fs = 1; fs_ck[1] = !(kc & 1); break;
	case AK_RT: fs = 1; fs_ck[2] = !(kc & 1); break;
	case AK_DN: fs = 1; fs_ck[3] = !(kc & 1); break;
	case AK_RCTRL: fs = 1; fs_ck[4] = !(kc & 1); break;
	case AK_RSH: fs = 1; fs_ck[5] = !(kc & 1); break;
	}
    }
    if (fs_se != 0) {
	switch (kc >> 1) {
	case AK_T: fs = 1; fs_se[0] = !(kc & 1); break;
	case AK_F: fs = 1; fs_se[1] = !(kc & 1); break;
	case AK_H: fs = 1; fs_se[2] = !(kc & 1); break;
	case AK_B: fs = 1; fs_se[3] = !(kc & 1); break;
	case AK_LALT: fs = 1; fs_se[4] = !(kc & 1); break;
	case AK_LSH: fs = 1; fs_se[5] = !(kc & 1); break;
	}
    }

    if (fs && currprefs.input_selected_setting == 0) {
	if (JSEM_ISNUMPAD (0, &currprefs) || JSEM_ISCURSOR (0, &currprefs) || JSEM_ISSOMEWHEREELSE (0, &currprefs))
	    do_fake (0);
	if (JSEM_ISNUMPAD (1, &currprefs) || JSEM_ISCURSOR (1, &currprefs) || JSEM_ISSOMEWHEREELSE (1, &currprefs))
	    do_fake (1);
	return;
    } else {
        if ((kc >> 1) == AK_RCTRL) {
	    kc ^= AK_RCTRL << 1;
	    kc ^= AK_CTRL << 1;
	}
    }

    keybuf[kpb_first] = kc;
    kpb_first = kpb_next;
}

void joystick_setting_changed (void)
{
    fs_np = fs_ck = fs_se = 0;

    if (JSEM_ISNUMPAD (0, &currprefs))
	fs_np = fakestate[0];
    else if (JSEM_ISNUMPAD (1, &currprefs))
	fs_np = fakestate[1];

    if (JSEM_ISCURSOR (0, &currprefs))
	fs_ck = fakestate[0];
    else if (JSEM_ISCURSOR (1, &currprefs))
	fs_ck = fakestate[1];

    if (JSEM_ISSOMEWHEREELSE (0, &currprefs))
	fs_se = fakestate[0];
    else if (JSEM_ISSOMEWHEREELSE (1, &currprefs))
	fs_se = fakestate[1];
}

void keybuf_init (void)
{
    kpb_first = kpb_last = 0;
    inputdevice_updateconfig (&currprefs);
}
