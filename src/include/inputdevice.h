 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Joystick, mouse and keyboard emulation prototypes and definitions
  *
  * Copyright 1995 Bernd Schmidt
  * Copyright 2001-2002 Toni Wilen
  */


#define IDTYPE_JOYSTICK 0
#define IDTYPE_MOUSE 1
#define IDTYPE_KEYBOARD 2

struct inputdevice_functions {
    int           (*init)             (void);
    void          (*close)            (void);
    int (*acquire)(int,int);
    void (*unacquire)(int);
    void          (*read)             (void);
    int (*get_num)(void);
    TCHAR* (*get_friendlyname)(int);
    TCHAR* (*get_uniquename)(int);
    int (*get_widget_num)(int);
    int (*get_widget_type)(int,int,TCHAR*,uae_u32*);
    int (*get_widget_first)(int,int);
    int (*get_flags)(int);
};
extern struct inputdevice_functions idev[3];
extern struct inputdevice_functions inputdevicefunc_joystick;
extern struct inputdevice_functions inputdevicefunc_mouse;
extern struct inputdevice_functions inputdevicefunc_keyboard;
extern int pause_emulation;

struct uae_input_device_kbr_default {
    int scancode;
    int evt;
	int flags;
};

struct inputevent {
	const TCHAR *confname;
	const TCHAR *name;
	int allow_mask;
	int type;
	int unit;
	int data;
};

/* event flags */
#define ID_FLAG_AUTOFIRE 1
#define ID_FLAG_TOGGLE 2
#define ID_FLAG_SAVE_MASK 0xff
#define ID_FLAG_TOGGLED 0x100

#define IDEV_WIDGET_NONE 0
#define IDEV_WIDGET_BUTTON 1
#define IDEV_WIDGET_AXIS 2
#define IDEV_WIDGET_BUTTONAXIS 3
#define IDEV_WIDGET_KEY 4

#define IDEV_MAPPED_AUTOFIRE_POSSIBLE 1
#define IDEV_MAPPED_AUTOFIRE_SET 2
#define IDEV_MAPPED_TOGGLE 4

#define ID_BUTTON_OFFSET 0
#define ID_BUTTON_TOTAL 32
#define ID_AXIS_OFFSET 32
#define ID_AXIS_TOTAL 32

extern int inputdevice_iterate (int devnum, int num, TCHAR *name, int *af);
extern int inputdevice_set_mapping (int devnum, int num, TCHAR *name, TCHAR *custom, int flags, int sub);
extern int inputdevice_get_mapped_name (int devnum, int num, int *pflags, TCHAR *name, TCHAR *custom, int sub);
extern void inputdevice_copyconfig (const struct uae_prefs *src, struct uae_prefs *dst);
extern void inputdevice_copy_single_config (struct uae_prefs *p, int src, int dst, int devnum);
extern void inputdevice_swap_ports (struct uae_prefs *p, int devnum);
extern void inputdevice_swap_compa_ports (struct uae_prefs *p, int portswap);
extern void inputdevice_config_change (void);
extern int inputdevice_config_change_test (void);
extern int inputdevice_get_device_index (int devnum);
extern TCHAR *inputdevice_get_device_name (int type, int devnum);
extern TCHAR *inputdevice_get_device_name2 (int devnum);
extern TCHAR *inputdevice_get_device_unique_name (int type, int devnum);
extern int inputdevice_get_device_status (int devnum);
extern void inputdevice_set_device_status (int devnum, int enabled);
extern int inputdevice_get_device_total (int type);
extern int inputdevice_get_widget_num (int devnum);
extern int inputdevice_get_widget_type (int devnum, int num, TCHAR *name);

extern int input_get_default_mouse (struct uae_input_device *uid, int num, int port);
extern int input_get_default_lightpen (struct uae_input_device *uid, int num, int port);
extern int input_get_default_joystick (struct uae_input_device *uid, int num, int port, int mode);
extern int input_get_default_joystick_analog (struct uae_input_device *uid, int num, int port);
extern int input_get_default_keyboard (int num);

#define DEFEVENT(A, B, C, D, E, F) INPUTEVENT_ ## A,
enum inputevents {
INPUTEVENT_ZERO,
#include "inputevents.def"
INPUTEVENT_END
};
#undef DEFEVENT

extern void handle_cd32_joystick_cia (uae_u8, uae_u8);
extern uae_u8 handle_parport_joystick (int port, uae_u8 pra, uae_u8 dra);
extern uae_u8 handle_joystick_buttons (uae_u8);
extern int getbuttonstate (int joy, int button);
extern int getjoystate (int joy);

#define MAGICMOUSE_BOTH 0
#define MAGICMOUSE_NATIVE_ONLY 1
#define MAGICMOUSE_HOST_ONLY 2

extern int magicmouse_alive (void);
extern int is_tablet (void);
extern int inputdevice_is_tablet (void);
extern void input_mousehack_status (int mode, uaecptr diminfo, uaecptr dispinfo, uaecptr vp, uae_u32 moffset);
extern void input_mousehack_mouseoffset (uaecptr pointerprefs);
extern int mousehack_alive (void);
extern void setmouseactive (int);

extern void setmousebuttonstateall (int mouse, uae_u32 buttonbits, uae_u32 buttonmask);
extern void setjoybuttonstateall (int joy, uae_u32 buttonbits, uae_u32 buttonmask);
extern void setjoybuttonstate (int joy, int button, int state);
extern void setmousebuttonstate (int mouse, int button, int state);
extern void setjoystickstate (int joy, int axle, int state, int max);
extern int getjoystickstate (int mouse);
void setmousestate (int mouse, int axis, int data, int isabs);
extern int getmousestate (int mouse);
extern void inputdevice_updateconfig (struct uae_prefs *prefs);
extern void inputdevice_mergeconfig (struct uae_prefs *prefs);
extern void inputdevice_devicechange (struct uae_prefs *prefs);

extern int inputdevice_translatekeycode (int keyboard, int scancode, int state);
extern void inputdevice_setkeytranslation (struct uae_input_device_kbr_default *trans, int **kbmaps);
extern int handle_input_event (int nr, int state, int max, int autofire);
extern void inputdevice_do_keyboard (int code, int state);
extern int inputdevice_iskeymapped (int keyboard, int scancode);
extern int inputdevice_synccapslock (int, int*);
extern void inputdevice_testrecord (int type, int num, int wtype, int wnum, int state);
extern int inputdevice_get_compatibility_input (struct uae_prefs*, int, int*, int**, int**);
extern struct inputevent *inputdevice_get_eventinfo (int evt);
extern void inputdevice_get_eventname (const struct inputevent *ie, TCHAR *out);
extern void inputdevice_compa_prepare_custom (struct uae_prefs *prefs, int index);
extern int intputdevice_compa_get_eventtype (int evt, int **axistable);


extern uae_u16 potgo_value;
extern uae_u16 POTGOR (void);
extern void POTGO (uae_u16 v);
extern uae_u16 POT0DAT (void);
extern uae_u16 POT1DAT (void);
extern void JOYTEST (uae_u16 v);
extern uae_u16 JOY0DAT (void);
extern uae_u16 JOY1DAT (void);

extern void inputdevice_vsync (void);
extern void inputdevice_hsync (void);
extern void inputdevice_reset (void);

extern void write_inputdevice_config (struct uae_prefs *p, FILE *f);
extern void read_inputdevice_config (struct uae_prefs *p, TCHAR *option, TCHAR *value);
extern void reset_inputdevice_config (struct uae_prefs *pr);
extern int inputdevice_joyport_config (struct uae_prefs *p, TCHAR *value, int portnum, int mode, int type);
extern int inputdevice_getjoyportdevice (int port, int val);

extern void inputdevice_init (void);
extern void inputdevice_close (void);
extern void inputdevice_default_prefs (struct uae_prefs *p);

extern void inputdevice_acquire (int allmode);
extern void inputdevice_unacquire (void);

extern void indicator_leds (int num, int state);

extern void warpmode (int mode);
extern void pausemode (int mode);

extern void inputdevice_add_inputcode (int code, int state);
extern void inputdevice_handle_inputcode (void);

extern void inputdevice_tablet (int x, int y, int z,
	      int pressure, uae_u32 buttonbits, int inproximity,
	      int ax, int ay, int az);
extern void inputdevice_tablet_info (int maxx, int maxy, int maxz, int maxax, int maxay, int maxaz, int xres, int yres);
extern void inputdevice_tablet_strobe (void);


#define JSEM_MODE_DEFAULT 0
#define JSEM_MODE_MOUSE 1
#define JSEM_MODE_JOYSTICK 2
#define JSEM_MODE_JOYSTICK_ANALOG 3
#define JSEM_MODE_MOUSE_CDTV 4
#define JSEM_MODE_JOYSTICK_CD32 5
#define JSEM_MODE_LIGHTPEN 6

#define JSEM_KBDLAYOUT 0
#define JSEM_JOYS      100
#define JSEM_MICE      200
#define JSEM_END       300
#define JSEM_XARCADE1LAYOUT (JSEM_KBDLAYOUT + 3)
#define JSEM_XARCADE2LAYOUT (JSEM_KBDLAYOUT + 4)
#define JSEM_DECODEVAL(port,p) ((p)->jports[port].id)
#define JSEM_ISNUMPAD(port,p)        (jsem_iskbdjoy (port,p) == JSEM_KBDLAYOUT)
#define JSEM_ISCURSOR(port,p)        (jsem_iskbdjoy (port,p) == JSEM_KBDLAYOUT + 1)
#define JSEM_ISSOMEWHEREELSE(port,p) (jsem_iskbdjoy (port,p) == JSEM_KBDLAYOUT + 2)
#define JSEM_ISXARCADE1(port,p) (jsem_iskbdjoy(port,p) == JSEM_XARCADE1LAYOUT)
#define JSEM_ISXARCADE2(port,p) (jsem_iskbdjoy(port,p) == JSEM_XARCADE2LAYOUT)
#define JSEM_LASTKBD 5
#define JSEM_ISANYKBD(port,p)        (jsem_iskbdjoy (port,p) >= JSEM_KBDLAYOUT && jsem_iskbdjoy(port,p) < JSEM_KBDLAYOUT + JSEM_LASTKBD)

extern int jsem_isjoy    (int port, const struct uae_prefs *p);
extern int jsem_ismouse  (int port, const struct uae_prefs *p);
extern int jsem_iskbdjoy (int port, const struct uae_prefs *p);

extern int inputdevice_uaelib (TCHAR *, TCHAR *);

#define INPREC_JOYPORT 1
#define INPREC_JOYBUTTON 2
#define INPREC_KEY 3
#define INPREC_DISKINSERT 4
#define INPREC_DISKREMOVE 5
#define INPREC_VSYNC 6
#define INPREC_CIAVSYNC 7
#define INPREC_END 0xff
#define INPREC_QUIT 0xfe

extern int input_recording;
extern void inprec_close (void);
extern int inprec_open (TCHAR*, int);
extern void inprec_rend (void);
extern void inprec_rstart (uae_u8);
extern void inprec_ru8 (uae_u8);
extern void inprec_ru16 (uae_u16);
extern void inprec_ru32 (uae_u32);
extern void inprec_rstr (const TCHAR*);
extern int inprec_pstart (uae_u8);
extern void inprec_pend (void);
extern uae_u8 inprec_pu8 (void);
extern uae_u16 inprec_pu16 (void);
extern uae_u32 inprec_pu32 (void);
extern int inprec_pstr (TCHAR*);

extern int inputdevice_testread (int*, int*, int*);
extern int inputdevice_istest (void);
extern void inputdevice_settest (int);
extern int inputdevice_testread_count (void);

//FIXME:
    typedef enum {
        DIK_0,
        DIK_1,
        DIK_2,
        DIK_3,
        DIK_4,
        DIK_5,
        DIK_6,
        DIK_7,
        DIK_8,
        DIK_9,
        DIK_A,
        DIK_ABNT_C1,
        DIK_ABNT_C2,
        DIK_ADD,
        DIK_APOSTROPHE,
        DIK_APPS,
        DIK_AT,
        DIK_AX,
        DIK_B,
        DIK_BACK,
        DIK_BACKSLASH,
        DIK_C,
        DIK_CALCULATOR,
        DIK_CAPITAL,
        DIK_COLON,
        DIK_COMMA,
        DIK_CONVERT,
        DIK_D,
        DIK_DECIMAL,
        DIK_DELETE,
        DIK_DIVIDE,
        DIK_DOWN,
        DIK_E,
        DIK_END,
        DIK_EQUALS,
        DIK_ESCAPE,
        DIK_F,
        DIK_F1,
        DIK_F2,
        DIK_F3,
        DIK_F4,
        DIK_F5,
        DIK_F6,
        DIK_F7,
        DIK_F8,
        DIK_F9,
        DIK_F10,
        DIK_F11,
        DIK_F12,
        DIK_F13,
        DIK_F14,
        DIK_F15,
        DIK_G,
        DIK_GRAVE,
        DIK_H,
        DIK_HOME,
        DIK_I,
        DIK_INSERT,
        DIK_J,
        DIK_K,
        DIK_KANA,
        DIK_KANJI,
        DIK_L,
        DIK_LBRACKET,
        DIK_LCONTROL,
        DIK_LEFT,
        DIK_LMENU,
        DIK_LSHIFT,
        DIK_LWIN,
        DIK_M,
        DIK_MAIL,
        DIK_MEDIASELECT,
        DIK_MEDIASTOP,
        DIK_MINUS,
        DIK_MULTIPLY,
        DIK_MUTE,
        DIK_MYCOMPUTER,
        DIK_N,
        DIK_NEXT,
        DIK_NEXTTRACK,
        DIK_NOCONVERT,
        DIK_NUMLOCK,
        DIK_NUMPAD0,
        DIK_NUMPAD1,
        DIK_NUMPAD2,
        DIK_NUMPAD3,
        DIK_NUMPAD4,
        DIK_NUMPAD5,
        DIK_NUMPAD6,
        DIK_NUMPAD7,
        DIK_NUMPAD8,
        DIK_NUMPAD9,
        DIK_NUMPADCOMMA,
        DIK_NUMPADENTER,
        DIK_NUMPADEQUALS,
        DIK_O,
        DIK_OEM_102,
        DIK_P,
        DIK_PAUSE,
        DIK_PERIOD,
        DIK_PLAYPAUSE,
        DIK_POWER,
        DIK_PREVTRACK,
        DIK_PRIOR,
        DIK_Q,
        DIK_R,
        DIK_RBRACKET,
        DIK_RCONTROL,
        DIK_RETURN,
        DIK_RIGHT,
        DIK_RMENU,
        DIK_RSHIFT,
        DIK_RWIN,
        DIK_S,
        DIK_SCROLL,
        DIK_SEMICOLON,
        DIK_SLASH,
        DIK_SLEEP,
        DIK_SPACE,
        DIK_STOP,
        DIK_SUBTRACT,
        DIK_SYSRQ,
        DIK_T,
        DIK_TAB,
        DIK_U,
        DIK_UNDERLINE,
        DIK_UNLABELED,
        DIK_UP,
        DIK_V,
        DIK_VOLUMEDOWN,
        DIK_VOLUMEUP,
        DIK_W,
        DIK_WAKE,
        DIK_WEBBACK,
        DIK_WEBFAVORITES,
        DIK_WEBFORWARD,
        DIK_WEBHOME,
        DIK_WEBREFRESH,
        DIK_WEBSEARCH,
        DIK_WEBSTOP,
        DIK_X,
        DIK_Y,
        DIK_YEN,
        DIK_Z
    };

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

	{ DIK_CAPITAL, INPUTEVENT_KEY_CAPS_LOCK, ID_FLAG_TOGGLE },

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

	{ DIK_VOLUMEDOWN, INPUTEVENT_SPC_MASTER_VOLUME_DOWN },
	{ DIK_VOLUMEUP, INPUTEVENT_SPC_MASTER_VOLUME_UP },
	{ DIK_MUTE, INPUTEVENT_SPC_MASTER_VOLUME_MUTE },

	{ DIK_HOME, INPUTEVENT_KEY_70 },
	{ DIK_END, INPUTEVENT_KEY_71 },
	//    { DIK_SYSRQ, INPUTEVENT_KEY_6E },
	//    { DIK_F12, INPUTEVENT_KEY_6F },
	{ DIK_INSERT, INPUTEVENT_KEY_47 },
	//    { DIK_PRIOR, INPUTEVENT_KEY_48 },
	{ DIK_PRIOR, INPUTEVENT_SPC_FREEZEBUTTON },
	{ DIK_NEXT, INPUTEVENT_KEY_49 },
	{ DIK_F11, INPUTEVENT_KEY_4B },

	{ DIK_MEDIASTOP, INPUTEVENT_KEY_CDTV_STOP },
	{ DIK_PLAYPAUSE, INPUTEVENT_KEY_CDTV_PLAYPAUSE },
	{ DIK_PREVTRACK, INPUTEVENT_KEY_CDTV_PREV },
	{ DIK_NEXTTRACK, INPUTEVENT_KEY_CDTV_NEXT },

	{ -1, 0 }
};

static int kb_np[] = { DIK_NUMPAD4, -1, DIK_NUMPAD6, -1, DIK_NUMPAD8, -1, DIK_NUMPAD2, -1, DIK_NUMPAD0, DIK_NUMPAD5, -1, DIK_DECIMAL, DIK_DIVIDE, DIK_NUMPADENTER, -1, -1 };
static int kb_ck[] = { DIK_LEFT, -1, DIK_RIGHT, -1, DIK_UP, -1, DIK_DOWN, -1, DIK_RCONTROL, DIK_RMENU, -1, DIK_RSHIFT, -1, -1 };
static int kb_se[] = { DIK_A, -1, DIK_D, -1, DIK_W, -1, DIK_S, -1, DIK_LMENU, -1, DIK_LSHIFT, -1, -1 };
static int kb_cd32_np[] = { DIK_NUMPAD4, -1, DIK_NUMPAD6, -1, DIK_NUMPAD8, -1, DIK_NUMPAD2, -1, DIK_NUMPAD1, -1, DIK_NUMPAD3, -1, DIK_NUMPAD7, -1, DIK_NUMPAD9, -1, DIK_DIVIDE, -1, DIK_SUBTRACT, -1, DIK_MULTIPLY, -1, -1 };
static int kb_cd32_ck[] = { DIK_LEFT, -1, DIK_RIGHT, -1, DIK_UP, -1, DIK_DOWN, -1, DIK_NUMPAD1, -1, DIK_NUMPAD3, -1, DIK_NUMPAD7, -1, DIK_NUMPAD9, -1, DIK_DIVIDE, -1, DIK_SUBTRACT, -1, DIK_MULTIPLY, -1, -1 };
static int kb_cd32_se[] = { DIK_A, -1, DIK_D, -1, DIK_W, -1, DIK_S, -1, DIK_NUMPAD1, -1, DIK_NUMPAD3, -1, DIK_NUMPAD7, -1, DIK_NUMPAD9, -1, DIK_DIVIDE, -1, DIK_SUBTRACT, -1, DIK_MULTIPLY, -1, -1 };
static int kb_xa1[] = { DIK_NUMPAD4, -1, DIK_NUMPAD6, -1, DIK_NUMPAD8, -1, DIK_NUMPAD2, DIK_NUMPAD5, -1, DIK_LCONTROL, -1, DIK_LMENU, -1, DIK_SPACE, -1, -1 };
static int kb_xa2[] = { DIK_D, -1, DIK_G, -1, DIK_R, -1, DIK_F, -1, DIK_A, -1, DIK_S, -1, DIK_Q, -1 };
static int kb_arcadia[] = { DIK_F2, -1, DIK_1, -1, DIK_2, -1, DIK_5, -1, DIK_6, -1, -1 };
static int kb_arcadiaxa[] = { DIK_1, -1, DIK_2, -1, DIK_3, -1, DIK_4, -1, DIK_6, -1, DIK_LBRACKET, DIK_LSHIFT, -1, DIK_RBRACKET, -1, DIK_C, -1, DIK_5, -1, DIK_Z, -1, DIK_X, -1, -1 };
static int *kbmaps[] = { kb_np, kb_ck, kb_se, kb_cd32_np, kb_cd32_ck, kb_cd32_se, kb_xa1, kb_xa2, kb_arcadia, kb_arcadiaxa };
