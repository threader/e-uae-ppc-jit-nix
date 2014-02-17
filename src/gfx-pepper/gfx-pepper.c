 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Pepper graphics to be used for Native Client builds.
  *
  * Copyright 1997 Bernd Schmidt
  * Copyright 2003 Richard Drummond
  * Copyright 2013 Christian Stefansen
  *
  */

#include "ppapi/c/pp_point.h"
#include "ppapi/c/ppb_input_event.h"

#include "hotkeys.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "keybuf.h"
#include "keymap/keymap.h"
#include "options.h"
#ifdef PICASSO96
#include "picasso96.h"
#endif /* PICASSO96 */
#include "writelog.h"
#include "xwin.h"

/* guidep == gui-html is currently the only way to build with Pepper. */
#include "guidep/ppapi.h"

/* External function declarations. */
/* From misc.c. */
extern void my_kbd_handler (int keyboard, int scancode, int newstate);
/* From drawing.h. */
extern void reset_drawing();
/* TODO(cstefansen): Fix drawing.h so it can be included in isolation without
 * tons of unsatisfied dependencies.
 */

/* Forward declaration. */
int push_event(PP_Resource event);

/* graphics_2d_subinit is defined in gfx-pepper-2d.c. */
int graphics_2d_subinit(uint32_t *Rmask, uint32_t *Gmask, uint32_t *Bmask,
                        uint32_t *Amask);
void screen_size_changed_2d(int32_t width, int32_t height);

/* graphics_3d_subinit is defined in gfx-pepper-3d.c. */
int graphics_3d_subinit(uint32_t *Rmask, uint32_t *Gmask, uint32_t *Bmask,
                        uint32_t *Amask);
void screen_size_changed_3d(int32_t width, int32_t height);

static int using_3d;
static uint32_t Rmask, Gmask, Bmask, Amask;

void screen_size_changed(unsigned int width, unsigned int height) {
    if (using_3d) {
        screen_size_changed_3d(width, height);
    } else {
        screen_size_changed_2d(width, height);
    }
}

STATIC_INLINE int pepper_lockscr(struct vidbuf_description *gfxinfo) {
    return 1;
}
STATIC_INLINE void pepper_unlockscr(struct vidbuf_description *gfxinfo) {}
STATIC_INLINE void pepper_flush_line(struct vidbuf_description *gfxinfo,
                                     int line_no) {}

STATIC_INLINE void pepper_flush_block(struct vidbuf_description *gfxinfo,
        int first_line, int last_line) {}
STATIC_INLINE void pepper_flush_clear_screen(
        struct vidbuf_description *gfxinfo) {}

int graphics_setup() {
    return 1;
}

STATIC_INLINE unsigned long bitsInMask (unsigned long mask)
{
    /* count bits in mask */
    unsigned long n = 0;
    while (mask) {
        n += mask & 1;
        mask >>= 1;
    }
    return n;
}

STATIC_INLINE unsigned long maskShift (unsigned long mask)
{
    /* determine how far mask is shifted */
    unsigned long n = 0;
    while (!(mask & 1)) {
        n++;
        mask >>= 1;
    }
    return n;
}

static int init_colors (void)
{
    if (gfxvidinfo.pixbytes > 1) {
        int red_bits    = bitsInMask(Rmask);
        int green_bits  = bitsInMask(Gmask);
        int blue_bits   = bitsInMask(Bmask);
        int red_shift   = maskShift(Rmask);
        int green_shift = maskShift(Gmask);
        int blue_shift  = maskShift(Bmask);
        if (Amask == 0) {
            alloc_colors64k (red_bits, green_bits, blue_bits,
                    red_shift, green_shift, blue_shift,
                    0, 0, 0, 0);
        } else {
            int alpha_shift = maskShift(Amask);
            int alpha_bits  = bitsInMask(Amask);
            alloc_colors64k (red_bits, green_bits, blue_bits,
                    red_shift, green_shift, blue_shift,
                    alpha_bits, alpha_shift, /* alpha = */ 0xffffffff, 0);
        }
    } else {
        DEBUG_LOG ("init_colors: only 256 colors\n");
        return 0;
    }
    return 1;
}


/* A key thing here is to populate the struct gfxvidinfo; most of it is done
 * in the subinit functions.
 *
 * We try hardware accelerated graphics via Graphics3D, If Chrome doesn't
 * support it on the given adapter/driver/OS combination, we fall back to
 * Graphics2D, which is generally speaking slower.
 */
int graphics_init(void) {
    /* Set up the common parts of the gfxvidinfo struct here. The subinit
     * functions for 2D and 3D initialize the remaining values appropriately.
     */
    /* TODO(cstefansen): Generalize resolution. For now we force 720x568. */
    gfxvidinfo.width = currprefs.gfx_size_fs.width = 720;
    gfxvidinfo.height = currprefs.gfx_size_fs.height = 568;
    gfxvidinfo.emergmem = 0;
    gfxvidinfo.linemem = 0;
    gfxvidinfo.maxblocklines = MAXBLOCKLINES_MAX;
    gfxvidinfo.lockscr = pepper_lockscr;
    gfxvidinfo.unlockscr = pepper_unlockscr;
    gfxvidinfo.flush_line = pepper_flush_line;
    gfxvidinfo.flush_block = pepper_flush_block;
    gfxvidinfo.flush_clear_screen = pepper_flush_clear_screen;

    /* Try 3D graphics. */
    using_3d = 1;
    if (!graphics_3d_subinit(&Rmask, &Gmask, &Bmask, &Amask)) {
        DEBUG_LOG("Could not initialize hardware accelerated graphics "
                  "(Graphics3D).\n");
        using_3d = 0;
        /* Try 2D graphics. */
        if (!graphics_2d_subinit(&Rmask, &Gmask, &Bmask, &Amask)) {
            DEBUG_LOG("Could not initialize 2D graphics (Graphics2D).\n");
            return 0;
        }
    }

    reset_drawing();
    init_colors();

    DEBUG_LOG("Screen height    : %d\n", gfxvidinfo.height);
    DEBUG_LOG("Screen width     : %d\n", gfxvidinfo.width);
    DEBUG_LOG("Using Graphics3D?: %d\n", using_3d);
    DEBUG_LOG("Bytes per pixel  : %d\n", gfxvidinfo.pixbytes);
    DEBUG_LOG("Bytes per line   : %d\n", gfxvidinfo.rowbytes);
    DEBUG_LOG("Buffer address   : %p\n", gfxvidinfo.bufmem);

    return 1;
}

void setmaintitle(void) {}

int gfx_parse_option(struct uae_prefs *p, const char *option,
                     const char *value) {
    return 0;
}

void gfx_default_options(struct uae_prefs *p) {}

int check_prefs_changed_gfx(void) {
    return 0;
}

void graphics_leave(void) {}

int debuggable(void) {
    return 1;
}


/*
 * Input device functions.
 */
static PPB_InputEvent *ppb_input_event_interface;
static PPB_KeyboardInputEvent *ppb_keyboard_event_interface;
static PPB_MouseInputEvent *ppb_mouse_event_interface;

/*
 * Mouse input device functions.
 */

#define MAX_BUTTONS 3
#define MAX_AXES    3
#define FIRST_AXIS  0
#define FIRST_BUTTON    MAX_AXES

static int init_mouse (void)
{
    if (!ppb_input_event_interface) {
        ppb_input_event_interface =
            (PPB_InputEvent *) NaCl_GetInterface(PPB_INPUT_EVENT_INTERFACE);
    }
    ppb_mouse_event_interface =
            (PPB_MouseInputEvent *) NaCl_GetInterface(
                    PPB_MOUSE_INPUT_EVENT_INTERFACE);

    if (!ppb_input_event_interface) {
        DEBUG_LOG("Could not acquire PPB_InputEvent interface.\n");
        return 0;
    }
    if (!ppb_mouse_event_interface) {
        DEBUG_LOG("Could not acquire PPB_MouseInputEvent interface.\n");
        return 0;
    }
    return 1;
}

static void close_mouse(void)
{
    return;
}

static int acquire_mouse(int num, int flags)
{
    return 1;
}

static void unacquire_mouse (int num)
{
    return;
}

static void read_mouse(void)
{
}

static int get_mouse_num (void)
{
    return 1;
}

static TCHAR *get_mouse_friendlyname (int mouse)
{
    return "Default mouse";
}
static TCHAR *get_mouse_uniquename (int mouse)
{
    return "DEFMOUSE1";
}

static int get_mouse_widget_num (int mouse)
{
    return MAX_AXES + MAX_BUTTONS;
}

static int get_mouse_widget_type (int mouse, int num, TCHAR *name,
                                  uae_u32 *code)
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

static int get_mouse_flags (int num)
{
    return 1;
}

struct inputdevice_functions inputdevicefunc_mouse = {
    init_mouse,
    close_mouse,
    acquire_mouse,
    unacquire_mouse,
    read_mouse,
    get_mouse_num,
    get_mouse_friendlyname,
    get_mouse_uniquename,
    get_mouse_widget_num,
    get_mouse_widget_type,
    get_mouse_widget_first,
    get_mouse_flags
};



/*
 * Keyboard inputdevice functions
 */

static int init_kb(void)
{
    if (!ppb_input_event_interface) {
        ppb_input_event_interface =
                (PPB_InputEvent *) NaCl_GetInterface(PPB_INPUT_EVENT_INTERFACE);
    }
    ppb_keyboard_event_interface =
            (PPB_KeyboardInputEvent *) NaCl_GetInterface(
                    PPB_KEYBOARD_INPUT_EVENT_INTERFACE);

    if (!ppb_input_event_interface) {
        DEBUG_LOG("Could not acquire PPB_InputEvent interface.\n");
        return 0;
    }
    if (!ppb_keyboard_event_interface) {
        DEBUG_LOG("Could not acquire PPB_KeyboardInputEvent interface.\n");
        return 0;
    }

    inputdevice_release_all_keys ();
    reset_hotkeys ();

    return 1;
}

static void close_kb(void)
{
}

static int acquire_kb(int num, int flags)
{
    return 1;
}

static void unacquire_kb(int num)
{
}

static void read_kb(void)
{
}

static int get_kb_num(void)
{
    return 1;
}

static TCHAR *get_kb_friendlyname(int kb)
{
    return "Default keyboard";
}
static TCHAR *get_kb_uniquename(int kb)
{
    return "DEFKEYB1";
}

static int get_kb_widget_num(int kb)
{
    return 255;
}

static int get_kb_widget_first(int kb, int type)
{
    return 0;
}

static int get_kb_widget_type(int kb, int num, TCHAR *name, uae_u32 *code)
{
    *code = num;
    return IDEV_WIDGET_KEY;
}

static int get_kb_flags(int num)
{
    return 0;
}

struct inputdevice_functions inputdevicefunc_keyboard =
{
    init_kb,
    close_kb,
    acquire_kb,
    unacquire_kb,
    read_kb,
    get_kb_num,
    get_kb_friendlyname,
    get_kb_uniquename,
    get_kb_widget_num,
    get_kb_widget_type,
    get_kb_widget_first,
    get_kb_flags
};


/* TODO(cstefansen): Implement caps lock, num lock, and scroll lock. */
int getcapslockstate (void) { return 0; }
void setcapslockstate (int state) {}
int target_checkcapslock (int scancode, int *state) { return 0; }

/* TODO(cstefansen): Update commented-out keys if they get added to DOM
 * KeyboardEvent Keycodes. Currently, the codes for alt, shift, and control
 * are the same for left and right. Similarly for =/enter and =/enter on the
 * numeric keypad.
 * https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent */

static int ppapi_keycode_to_dik(uint32_t key) {
    switch (key) {
    case 27:         return DIK_ESCAPE;

    case '1':        return DIK_1;
    case '2':        return DIK_2;
    case '3':        return DIK_3;
    case '4':        return DIK_4;
    case '5':        return DIK_5;
    case '6':        return DIK_6;
    case '7':        return DIK_7;
    case '8':        return DIK_8;
    case '9':        return DIK_9;
    case '0':        return DIK_0;
    case 8:          return DIK_BACK;

    case 9:          return DIK_TAB;
    case 'Q':        return DIK_Q;
    case 'W':        return DIK_W;
    case 'E':        return DIK_E;
    case 'R':        return DIK_R;
    case 'T':        return DIK_T;
    case 'Y':        return DIK_Y;
    case 'U':        return DIK_U;
    case 'I':        return DIK_I;
    case 'O':        return DIK_O;
    case 'P':        return DIK_P;
    case 13:         return DIK_RETURN;
    case 17:         return DIK_LCONTROL;

    case 'A':        return DIK_A;
    case 'S':        return DIK_S;
    case 'D':        return DIK_D;
    case 'F':        return DIK_F;
    case 'G':        return DIK_G;
    case 'H':        return DIK_H;
    case 'J':        return DIK_J;
    case 'K':        return DIK_K;
    case 'L':        return DIK_L;
    case 16:         return DIK_LSHIFT;

    case 'Z':        return DIK_Z;
    case 'X':        return DIK_X;
    case 'C':        return DIK_C;
    case 'V':        return DIK_V;
    case 'B':        return DIK_B;
    case 'N':        return DIK_N;
    case 'M':        return DIK_M;
    /* case 16:         return DIK_RSHIFT; */
    case ' ':        return DIK_SPACE;

    case 112:        return DIK_F1;
    case 113:        return DIK_F2;
    case 114:        return DIK_F3;
    case 115:        return DIK_F4;
    case 116:        return DIK_F5;
    case 117:        return DIK_F6;
    case 118:        return DIK_F7;
    case 119:        return DIK_F8;
    case 120:        return DIK_F9;
    case 121:        return DIK_F10;
    case 122:        return DIK_F11;
    case 123:        return DIK_F12;
    case 124:        return DIK_F13;
    case 125:        return DIK_F14;
    case 126:        return DIK_F15;
    case 144:        return DIK_NUMLOCK;
    case 20:         return DIK_CAPITAL;
    case 145:        return DIK_SCROLL;

    /* TODO(cstefansen): +, *, and / don't work on the numeric pad - why? */
    case 103:        return DIK_NUMPAD7;
    case 104:        return DIK_NUMPAD8;
    case 105:        return DIK_NUMPAD9;
    case 109:        return DIK_SUBTRACT;
    case 100:        return DIK_NUMPAD4;
    case 101:        return DIK_NUMPAD5;
    case 102:        return DIK_NUMPAD6;
    case 107:        return DIK_ADD;
    case 97:         return DIK_NUMPAD1;
    case 98:         return DIK_NUMPAD2;
    case 99:         return DIK_NUMPAD3;
    case 96:         return DIK_NUMPAD0;
    case 110:        return DIK_DECIMAL;
    /* case 13:         return DIK_NUMPADENTER; */
    case 111:        return DIK_DIVIDE;
    case 106:        return DIK_MULTIPLY;
    /* case 187:        return DIK_NUMPADEQUALS; */

    case 46:         return DIK_DELETE;
    /* case 17:         return DIK_RCONTROL; */
    case 18:         return DIK_LMENU;
    /* case 18:         return DIK_RMENU; */

    case 45:         return DIK_INSERT;
    case 36:         return DIK_HOME;
    case 35:         return DIK_END;

    case 186:        return DIK_SEMICOLON;
    case 187:        return DIK_EQUALS;
    case 188:        return DIK_COMMA;
    case 189:        return DIK_MINUS;
    case 190:        return DIK_PERIOD;
    case 191:        return DIK_SLASH;
    case 192:        return DIK_GRAVE;
    case 219:        return DIK_LBRACKET;
    case 220:        return DIK_BACKSLASH;
    case 221:        return DIK_RBRACKET;
    case 222:        return DIK_APOSTROPHE;
    case 226:        return DIK_OEM_102;

    case 38:         return DIK_UP;
    case 33:         return DIK_PRIOR;
    case 37:         return DIK_LEFT;
    case 39:         return DIK_RIGHT;
    case 40:         return DIK_DOWN;
    case 34:         return DIK_NEXT;

    case 19:         return DIK_PAUSE;
    case 91:         return DIK_LWIN;
    case 93:         return DIK_RWIN;

    default: return -1;
    }
}

int input_get_default_mouse (struct uae_input_device *uid, int num, int port,
                             int af, bool joymouseswap) {
    /* Supports only one mouse */
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
    uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
    uid[0].enabled = 1;
    return 0;
}

void toggle_fullscreen (int mode) {}
void toggle_mousegrab (void) {}
void screenshot (int mode, int doprepare) {}

/* Returns 0 if the queue is full, the event cannot be parsed, or the event
 * was not handled. */
int push_event(PP_Resource event) {
    if (!ppb_input_event_interface || !ppb_mouse_event_interface ||
        !ppb_keyboard_event_interface) {
        DEBUG_LOG("Refusing to process pre-initialization input event.\n");
        return 0;
    }
    PP_InputEvent_Type type = ppb_input_event_interface->GetType(event);
    switch (type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP: {
        int buttonno = -1;
        switch (ppb_mouse_event_interface->GetButton(event)) {
        case PP_INPUTEVENT_MOUSEBUTTON_NONE:   return 0;
        case PP_INPUTEVENT_MOUSEBUTTON_LEFT:   buttonno = 0; break;
        case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE: buttonno = 2; break;
        case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:  buttonno = 1; break;
        }
        setmousebuttonstate(0, buttonno, type == PP_INPUTEVENT_TYPE_MOUSEDOWN ?
                            1 : 0);
        break;
    }
    case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
        struct PP_Point delta = ppb_mouse_event_interface->GetMovement(event);
        setmousestate (0, 0, delta.x, 0);
        setmousestate (0, 1, delta.y, 0);
        break;
    }
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
        int keycode = ppb_keyboard_event_interface->GetKeyCode(event);
        my_kbd_handler(0, ppapi_keycode_to_dik(keycode),
                type == PP_INPUTEVENT_TYPE_KEYDOWN ? 1 : 0);
        break;
    }
    default:
        return 0;
    }
    return 1;
}

void handle_events(void) {}

#ifdef PICASSO96
int picasso_palette (void) { return 0; }
void gfx_set_picasso_modeinfo (uae_u32 w, uae_u32 h, uae_u32 depth,
                               RGBFTYPE rgbfmt) {}
void gfx_set_picasso_colors (RGBFTYPE rgbfmt) {}
void gfx_set_picasso_state (int on) {}
int WIN32GFX_IsPicassoScreen (void) { return 0; }
void DX_Invalidate (int first, int last) {}
int DX_Fill (int dstx, int dsty, int width, int height, uae_u32 color,
             RGBFTYPE rgbtype) { return 0; }
int DX_FillResolutions (uae_u16 *ppixel_format) { return 0; }
void gfx_unlock_picasso (void) {}
uae_u8 *gfx_lock_picasso (int fullupdate) {
    DEBUG_LOG("Function 'gfx_lock_picasso' not implemented.\n");
    return 0;
}
#endif /* PICASSO96 */
