 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Joystick support via the Pepper Gamepad API
  *
  * Copyright 1997 Bernd Schmidt
  * Copyright 2003-2005 Richard Drummond
  * Copyright 2013 Christian Stefansen
  */

#include <ppapi/c/ppb_gamepad.h>

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "custom.h"
#include "inputdevice.h"
#include "writelog.h"

/* guidep == gui-html is currently the only way to build with Pepper. */
#include "guidep/ppapi.h"

static PPB_Gamepad *ppb_gamepad_interface;
static PP_Instance pp_instance;

static int get_joystick_num(void);

static int init_joysticks (void) {
    ppb_gamepad_interface = (PPB_Gamepad *)
        NaCl_GetInterface(PPB_GAMEPAD_INTERFACE);
    pp_instance = NaCl_GetInstance();

    if (!ppb_gamepad_interface) {
        DEBUG_LOG("Could not acquire PPB_Gamepad interface.\n");
        return 0;
    }
    if (!pp_instance) {
        DEBUG_LOG("Could not find current Pepper instance.\n");
        return 0;
    }
    return 0;
}

static void close_joysticks (void) {
}

static int acquire_joystick (int num, int flags) {
    return num < get_joystick_num();
}

static void unacquire_joystick (int num) {
}

/* TODO(cstefansen): Figure out a proper autofire (see use of
   lastState below). */
/* static lastState = 0; */
static void read_joysticks (void)
{
    /* TODO(cstefansen): Support third joystick button (what games use
       this?) */

    /* Get current gamepad data. */
    struct PP_GamepadsSampleData gamepad_data;
    ppb_gamepad_interface->Sample(pp_instance, &gamepad_data);

    /* Update state for each connected gamepad. */
    size_t p = 0;
    for (; p < gamepad_data.length; ++p) {
      struct PP_GamepadSampleData pad = gamepad_data.items[p];

      if (!pad.connected)
        continue;

      /* Update axes. */
      size_t i = 0;
      int axisState[2] = {0, 0};
      for (; i < pad.axes_length; ++i) {
          axisState[i % 2] += pad.axes[i] < 0.1f && pad.axes[i] > -0.1f ?
              0 : (int) (pad.axes[i] * 32767);
      }
      /* Buttons 12-15 are up/down/left/right of a small digital
         joystick on the typical gamepad. */
      if (pad.buttons_length >= 15) {
          axisState[0] += (int) ((-pad.buttons[14] + pad.buttons[15]) * 32767);
          axisState[1] += (int) ((-pad.buttons[12] + pad.buttons[13]) * 32767);
      }
      setjoystickstate(p, 0, axisState[0] > 32767 ? 32767 :
                       axisState[0] < -32767 ? -32767 : axisState[0], 32767);
      setjoystickstate(p, 1, axisState[1] > 32767 ? 32767 :
                       axisState[1] < -32767 ? -32767 : axisState[1], 32767);

      /* Update buttons. */
      static const int buttons[16] =
          {0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1};
      size_t j = 0;
      int buttonState[2] = {0, 0};
      /* Buttons 12-15 are considered a joystick (i.e., for movement)
         and 16 and above are ignored. */
      for (; j < pad.buttons_length && j < 12; ++j) {
          buttonState[buttons[j]] |= pad.buttons[j] < 0.1f ? 0 : 1;
      }
      /* TODO(cstefansen): Figure out a proper autofire. */
      /*
      lastState = (lastState == 1 && buttonState[0] == 1) ? 0 : buttonState[0];
      setjoybuttonstate(p, 0, lastState);
      */
      setjoybuttonstate(p, 0, buttonState[0]);
      setjoybuttonstate(p, 1, buttonState[1]);
    }
}

static int get_joystick_num (void) {
    return 2;
    /* TODO(cstefansen): Avoid hardcoding to two joysticks. */
    /*
    struct PP_GamepadsSampleData gamepads_data;
    ppb_gamepad_interface->Sample(pp_instance, &gamepads_data);
    return gamepads_data.length;
    */
}

static char *get_joystick_friendlyname (int joy) {
    switch (joy) {
    case 0: return "Joystick 0";
    case 1: return "Joystick 1";
    default: return 0;
    }
}

static char *get_joystick_uniquename (int joy) {
    switch (joy) {
    case 0: return "JOY0";
    case 1: return "JOY1";
    default: return 0;
    }
}

static int get_joystick_widget_num (int joy) {
    /* Just make all joysticks 2 axes and 2 buttons. */
    return 2 + 2;
}

static int get_joystick_widget_type (int joy, int num, char *name,
                                     uae_u32 *code) {
    if (num < 2) return IDEV_WIDGET_BUTTON;
    if (num < 4) return IDEV_WIDGET_AXIS;
    return IDEV_WIDGET_NONE;
}

static int get_joystick_widget_first (int joy, int type) {
    switch (type) {
    case IDEV_WIDGET_BUTTON:
        return 2;
    case IDEV_WIDGET_AXIS:
        return 0;
    }
    return -1;
}

static int get_joystick_flags (int num) {
	return 0;
}

struct inputdevice_functions inputdevicefunc_joystick = {
    init_joysticks,
    close_joysticks,
    acquire_joystick,
    unacquire_joystick,
    read_joysticks,
    get_joystick_num,
    get_joystick_friendlyname,
    get_joystick_uniquename,
    get_joystick_widget_num,
    get_joystick_widget_type,
    get_joystick_widget_first,
	get_joystick_flags
};

/* Set default inputdevice config for Pepper joysticks. */
int input_get_default_joystick (struct uae_input_device *uid, int i,
                                int port, int af, int mode, bool joymouseswap) {
    if (i >= 2)
        return 0;

    setid (uid, i, ID_AXIS_OFFSET + 0, 0, port,
           port ? INPUTEVENT_JOY2_HORIZ : INPUTEVENT_JOY1_HORIZ);
    setid (uid, i, ID_AXIS_OFFSET + 1, 0, port,
           port ? INPUTEVENT_JOY2_VERT  : INPUTEVENT_JOY1_VERT);
    setid_af (uid, i, ID_BUTTON_OFFSET + 0, 0, port,
              port ? INPUTEVENT_JOY2_FIRE_BUTTON : INPUTEVENT_JOY1_FIRE_BUTTON,
              af);
    setid (uid, i, ID_BUTTON_OFFSET + 1, 0, port,
           port ? INPUTEVENT_JOY2_2ND_BUTTON : INPUTEVENT_JOY1_2ND_BUTTON);

    return 1;

    if (i == 0) return 1;
    return 0;
}
