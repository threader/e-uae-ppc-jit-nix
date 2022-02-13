/*
 *  UAE - The Un*x Amiga Emulator
 *
 *  BeOS joystick driver
 *
 *  (c) 1996-1998 Christian Bauer
 *  (c) 1996 Patrick Hanevold
 *  (c) 2003-2004 Richard Drummond
 */

extern "C" {
#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "inputdevice.h"
}

#include <device/Joystick.h>

extern "C" {
static int init_joysticks(void);
static void close_joysticks(void);
static int acquire_joy (int num, int flags);
static void unacquire_joy (int num);
static void read_joysticks (void);
static int get_joystick_num (void);
static char *get_joystick_name (int nr);
static int get_joystick_widget_num (int nr);
static int get_joystick_widget_type (int nr, int num, char *name);
static int get_joystick_widget_first (int nr, int type);
};

#define MAX_JOYSTICKS 4

static int nr_joysticks;

static BJoystick *joy;

static int nr_axes[MAX_JOYSTICKS];
static int nr_buttons[MAX_JOYSTICKS];

/* Hard code these for just now */
#define MAX_BUTTONS  2
#define MAX_AXLES    2
#define FIRST_AXLE   0
#define FIRST_BUTTON 2


static void read_joy (int nr)
{
    if (nr >= nr_joysticks)
	return;

    if (joy->Update () != B_ERROR) {
	int16 values[nr_axes[nr]];
	int32 buttons;

	joy->GetAxisValues (values, nr);
	setjoystickstate (nr, 0, values[0], 32767);
	setjoystickstate (nr, 1, values[1], 32767);

	buttons = joy->ButtonValues ();
	setjoybuttonstate (nr, 0, buttons & 1);
	setjoybuttonstate (nr, 1, buttons & 2);
    }
}


static int init_joysticks (void)
{
    char port_name[B_OS_NAME_LENGTH];

    joy = new BJoystick ();

    /* Use only first joystick port for now */
    joy->GetDeviceName (0, port_name);

    if (joy->Open (port_name, true)) {
	nr_joysticks = joy->CountSticks ();

	/* Hard code one joystick for now */
	if (nr_joysticks > 1)
	    nr_joysticks = 1;
	nr_axes[0] = MAX_AXLES;

	write_log ("Found %d joystick(s) on port %s\n", nr_joysticks, port_name);
    } else {
	write_log ("Failed to open joystick port %s\n", port_name);
	nr_joysticks = 0;
    }

    return 1;
}


static void close_joysticks (void)
{
    joy->Close();
    delete joy;
}


static int acquire_joy (int num, int flags)
{
    return 1;
}


static void unacquire_joy (int num)
{
}


static void read_joysticks (void)
{
    int i;
    for (i = 0; i < get_joystick_num(); i++)
	read_joy (i);
}


static int get_joystick_num (void)
{
    return nr_joysticks;
}


static char *get_joystick_name (int nr)
{
    static char name[B_OS_NAME_LENGTH];
    joy->GetDeviceName (nr, name, B_OS_NAME_LENGTH);
    return name;
}


static int get_joystick_widget_num (int nr)
{
    return MAX_AXLES + MAX_BUTTONS;
}


static int get_joystick_widget_type (int nr, int num, char *name, uae_u32 *what)
{
    if (num >= MAX_AXLES && num < MAX_AXLES+MAX_BUTTONS) {
	if (name)
	    sprintf (name, "Button %d", num + 1 - MAX_AXLES);
	return IDEV_WIDGET_BUTTON;
    } else if (num < MAX_AXLES) {
	if (name)
	    sprintf (name, "Axis %d", num + 1);
	return IDEV_WIDGET_AXIS;
    }
    return IDEV_WIDGET_NONE;
}


static int get_joystick_widget_first (int nr, int type)
{
    switch (type) {
	case IDEV_WIDGET_BUTTON:
	    return FIRST_BUTTON;
	case IDEV_WIDGET_AXIS:
	    return FIRST_AXLE;
    }

    return -1;
}


struct inputdevice_functions inputdevicefunc_joystick = {
    init_joysticks, close_joysticks, acquire_joy, unacquire_joy,
    read_joysticks, get_joystick_num, get_joystick_name,
    get_joystick_widget_num, get_joystick_widget_type,
    get_joystick_widget_first
};

/*
 * Set default inputdevice config for joysticks
 */
void input_get_default_joystick (struct uae_input_device *uid)
{
    int i, port;

    for (i = 0; i < nr_joysticks; i++) {
        port = i & 1;
        uid[i].eventid[ID_AXIS_OFFSET + 0][0]   = port ? INPUTEVENT_JOY2_HORIZ : INPUTEVENT_JOY1_HORIZ;
        uid[i].eventid[ID_AXIS_OFFSET + 1][0]   = port ? INPUTEVENT_JOY2_VERT  : INPUTEVENT_JOY1_VERT;
        uid[i].eventid[ID_BUTTON_OFFSET + 0][0] = port ? INPUTEVENT_JOY2_FIRE_BUTTON : INPUTEVENT_JOY1_FIRE_BUTTON;
        uid[i].eventid[ID_BUTTON_OFFSET + 1][0] = port ? INPUTEVENT_JOY2_2ND_BUTTON  : INPUTEVENT_JOY1_2ND_BUTTON;
        uid[i].eventid[ID_BUTTON_OFFSET + 2][0] = port ? INPUTEVENT_JOY2_3RD_BUTTON  : INPUTEVENT_JOY1_3RD_BUTTON;
    }
    uid[0].enabled = 1;
}
