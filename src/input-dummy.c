/*
 * UAE - The Un*x Amiga Emulator
 *
 * Stubs for inputdevice interface
 * Copyrigt Richard Drummond 2003
 *
 * Based on code copyright 2002 Toni Wilen
 *
 * Eventually all this will go away, as the various gfx targets
 * adopt this interface.
 */

#include "config.h"
#include "sysconfig.h"

#include "sysdeps.h"
#include "options.h"
#include "inputdevice.h"
#define MAX_MAPPINGS 256


static int get_mouse_num (void)
{
    return 0;
}

static char *get_mouse_name (int mouse)
{
    return 0;
}

static int get_mouse_widget_num (int mouse)
{
    return 4;
}

static int get_mouse_widget_first (int mouse, int type)
{
    switch (type) {
	case IDEV_WIDGET_BUTTON:
            return 0;
	case IDEV_WIDGET_AXIS:
            return 0;
    }
    return -1;
}

static int get_mouse_widget_type (int mouse, int num, char *name, uae_u32 *code)
{
    return IDEV_WIDGET_NONE;
}

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

static void read_mouse (void)
{
}

struct inputdevice_functions inputdevicefunc_mouse = {
    init_mouse, close_mouse, acquire_mouse, unacquire_mouse, read_mouse,
    get_mouse_num, get_mouse_name,
    get_mouse_widget_num, get_mouse_widget_type,
    get_mouse_widget_first
};


static int get_kb_num (void)
{
    return 0;
}

static char *get_kb_name (int kb)
{
    return 0;
}

static int get_kb_widget_num (int kb)
{
    return 0;
}

static int get_kb_widget_first (int kb, int type)
{
    return 0;
}

static int get_kb_widget_type (int kb, int num, char *name, uae_u32 *code)
{
    return IDEV_WIDGET_KEY;
}

static int keyboard_german;

static int init_kb (void)
{
    return 1;
}

static void close_kb (void)
{
}

uae_u8 di_keycodes[256];
static int kb_do_refresh;


static int keyhack (int scancode,int pressed)
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

struct inputdevice_functions inputdevicefunc_keyboard = {
    init_kb, close_kb, acquire_kb, unacquire_kb, read_kb,
    get_kb_num, get_kb_name,
    get_kb_widget_num, get_kb_widget_type,
    get_kb_widget_first
};
