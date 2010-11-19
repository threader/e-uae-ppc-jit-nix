#ifndef PUAE_MAINWINDOW_H
#define PUAE_MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
    class puae_MainWindow;
}

class puae_MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    QString GetPath(QWidget *who, QString what, QString where);
    QString GetFile(QWidget *who, QString what, QString extensions);
    explicit puae_MainWindow(QWidget *parent = 0);
    ~puae_MainWindow();

private:
    Ui::puae_MainWindow *ui;

private slots:
    void on_IDC_MBMEM2_valueChanged(int value);
    void on_IDC_FLOPPYSPD_valueChanged(int value);
    void on_IDC_MBMEM1_valueChanged(int value);
    void on_IDC_Z3FASTMEM_valueChanged(int value);
    void on_IDC_SLOWMEM_valueChanged(int value);
    void on_IDC_FASTMEM_valueChanged(int value);
    void on_IDC_CHIPMEM_valueChanged(int value);
    void on_IDC_CS_DENISE_toggled(bool checked);
    void on_IDC_CS_AGNUS_toggled(bool checked);
    void on_IDC_CS_FATGARY_toggled(bool checked);
    void on_IDC_CS_RAMSEY_toggled(bool checked);
    void on_IDC_CS_CDTVSCSI_toggled(bool checked);
    void on_IDC_CS_DMAC2_toggled(bool checked);
    void on_IDC_CS_A4091_toggled(bool checked);
    void on_IDC_CS_DMAC_toggled(bool checked);
    void on_IDC_CS_A2091_toggled(bool checked);
    void on_IDC_CS_DIPAGNUS_toggled(bool checked);
    void on_IDC_CS_NOEHB_toggled(bool checked);
    void on_IDC_CS_RESETWARNING_toggled(bool checked);
    void on_IDC_CS_PCMCIA_toggled(bool checked);
    void on_IDC_CS_CDTVRAMEXP_toggled(bool checked);
    void on_IDC_CS_DF0IDHW_toggled(bool checked);
    void on_IDC_CS_A1000RAM_toggled(bool checked);
    void on_IDC_CS_SLOWISFAST_toggled(bool checked);
    void on_IDC_CS_KSMIRROR_A8_toggled(bool checked);
    void on_IDC_CS_KSMIRROR_E0_toggled(bool checked);
    void on_IDC_CS_CIAA_TOD3_clicked();
    void on_IDC_CS_CIAA_TOD2_clicked();
    void on_IDC_CS_RTC3_clicked();
    void on_IDC_CS_RTC2_clicked();
    void on_IDC_CS_SOUND2_clicked();
    void on_IDC_CS_SOUND1_clicked();
    void on_IDC_COLLISION3_clicked();
    void on_IDC_COLLISION2_clicked();
    void on_IDC_COLLISION1_clicked();
    void on_IDC_FPU2_clicked();
    void on_IDC_FPU3_clicked();
    void on_IDC_FPU1_clicked();
    void on_IDC_CPU5_clicked();
    void on_IDC_CPU3_clicked();
    void on_IDC_CPU4_clicked();
    void on_IDC_CPU2_clicked();
    void on_IDC_CS_CDTVRAM_toggled(bool checked);
    void on_IDC_CS_CD32NVRAM_toggled(bool checked);
    void on_IDC_CS_CD32C2P_toggled(bool checked);
    void on_IDC_CS_IDE2_toggled(bool checked);
    void on_IDC_CS_IDE1_toggled(bool checked);
    void on_IDC_CS_CDTVCD_toggled(bool checked);
    void on_IDC_CS_CD32CD_toggled(bool checked);
    void on_IDC_CS_CIAOVERLAY_toggled(bool checked);
    void on_IDC_CS_CIAA_TOD1_clicked();
    void on_IDC_CS_RTC1_clicked();
    void on_IDC_CS_COMPATIBLE_toggled(bool checked);
    void on_IDC_CS_SOUND0_clicked();
    void on_IDC_COLLISION0_clicked();
    void on_IDC_NTSC_toggled(bool checked);
    void on_IDC_GENLOCK_toggled(bool checked);
    void on_IDC_CYCLEEXACT_toggled(bool checked);
    void on_IDC_BLITIMM_toggled(bool checked);
    void on_IDC_CS_EXT_currentIndexChanged(int index);
    void on_IDC_AGA_clicked();
    void on_IDC_ECS_clicked();
    void on_IDC_ECS_DENISE_clicked();
    void on_IDC_ECS_AGNUS_clicked();
    void on_IDC_OCS_clicked();
    void on_IDC_CPU_FREQUENCY_currentIndexChanged(int index);
    void on_IDC_CPUIDLE_sliderMoved(int position);
    void on_IDC_SPEED_valueChanged(int value);
    void on_IDC_CS_ADJUSTABLE_clicked();
    void on_IDC_CS_68000_clicked();
    void on_IDC_CS_HOST_clicked();
    void on_IDC_COMPATIBLE_FPU_toggled(bool checked);
    void on_IDC_FPU0_clicked();
    void on_IDC_MMUENABLE_toggled(bool checked);
    void on_IDC_JITENABLE_toggled(bool checked);
    void on_IDC_COMPATIBLE_toggled(bool checked);
    void on_IDC_COMPATIBLE24_toggled(bool checked);
    void on_IDC_CPU1_clicked();
    void on_IDC_CPU0_clicked();
    void on_IDC_KICKSHIFTER_toggled(bool checked);
    void on_IDC_MAPROM_toggled(bool checked);
    void on_IDC_FLASHCHOOSER_clicked();
    void on_IDC_CARTCHOOSER_clicked();
    void on_IDC_ROMCHOOSER2_clicked();
    void on_IDC_PATHS_RIPS_clicked();
    void on_IDC_PATHS_SAVEIMAGES_clicked();
    void on_IDC_PATHS_AVIOUTPUTS_clicked();
    void on_IDC_PATHS_SAVESTATES_clicked();
    void on_IDC_KICKCHOOSER_clicked();
    void on_IDC_PATHS_SCREENSHOTS_clicked();
    void on_IDC_PATHS_CONFIGS_clicked();
    void on_IDC_PATHS_ROMS_clicked();
//macros
    void values_to_memorydlg();
    void out_floppyspeed();
    void fix_values_memorydlg();
    void updatez3 (unsigned int *size1p, unsigned int *size2p);
    void enable_for_memorydlg ();
};

// ************************************************
// for now
// REMOVEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
// ************************************************

struct uae_prefs {

char description[256];
char info[256];
int config_version;
char config_hardware_path[100];
char config_host_path[100];

bool illegal_mem;
bool use_serial;
bool serial_demand;
bool serial_hwctsrts;
bool serial_direct;
bool parallel_demand;
int parallel_matrix_emulation;
bool parallel_postscript_emulation;
bool parallel_postscript_detection;
int parallel_autoflush_time;
char ghostscript_parameters[256];
bool use_gfxlib;
bool socket_emu;

#ifdef DEBUGGER
bool start_debugger;
#endif
bool start_gui;

int produce_sound;
int sound_stereo;
int sound_stereo_separation;
int sound_mixed_stereo_delay;
int sound_freq;
int sound_maxbsiz;
int sound_latency;
int sound_interpol;
int sound_filter;
int sound_filter_type;
int sound_volume;
bool sound_stereo_swap_paula;
bool sound_stereo_swap_ahi;
bool sound_auto;

#ifdef JIT
int comptrustbyte;
int comptrustword;
int comptrustlong;
int comptrustnaddr;
bool compnf;
bool compfpu;
bool comp_midopt;
bool comp_lowopt;
bool fpu_strict;

bool comp_hardflush;
bool comp_constjump;
bool comp_oldsegv;

int optcount[10];
#endif
int cachesize;
bool avoid_cmov;

int gfx_display;
char gfx_display_name[256];
int gfx_framerate, gfx_autoframerate;

bool gfx_autoresolution;
bool gfx_scandoubler;
int gfx_refreshrate;
int gfx_avsync, gfx_pvsync;
int gfx_resolution;
int gfx_vresolution;
int gfx_lores_mode;
int gfx_scanlines;
int gfx_afullscreen, gfx_pfullscreen;
int gfx_xcenter, gfx_ycenter;
int gfx_xcenter_pos, gfx_ycenter_pos;
int gfx_xcenter_size, gfx_ycenter_size;
int gfx_max_horizontal, gfx_max_vertical;
int gfx_saturation, gfx_luminance, gfx_contrast, gfx_gamma;
bool gfx_blackerthanblack;
int gfx_backbuffers;
int gfx_api;
int color_mode;
int gfx_gl_x_offset; //koko
int gfx_gl_y_offset; //koko
int gfx_gl_smoothing; //koko
int gfx_gl_panscan; //koko

int gfx_filter;

int gfx_filteroverlay_overscan;
int gfx_filter_scanlines;
int gfx_filter_scanlineratio;
int gfx_filter_scanlinelevel;
int gfx_filter_horiz_zoom, gfx_filter_vert_zoom;
int gfx_filter_horiz_zoom_mult, gfx_filter_vert_zoom_mult;
int gfx_filter_horiz_offset, gfx_filter_vert_offset;
int gfx_filter_filtermode;
int gfx_filter_bilinear;
int gfx_filter_noise, gfx_filter_blur;
int gfx_filter_saturation, gfx_filter_luminance, gfx_filter_contrast, gfx_filter_gamma;
int gfx_filter_keep_aspect, gfx_filter_aspect;
int gfx_filter_autoscale;

bool immediate_blits;
unsigned int chipset_mask;
bool ntscmode;
bool genlock;
int chipset_refreshrate;
int collision_level;
int leds_on_screen;
int keyboard_leds[3];
bool keyboard_leds_in_use;
int scsi;
bool sana2;
bool uaeserial;
int catweasel;
int catweasel_io;
int cpu_idle;
bool cpu_cycle_exact;
int cpu_clock_multiplier;
int cpu_frequency;
bool blitter_cycle_exact;
int floppy_speed;
int floppy_write_length;
int floppy_random_bits_min;
int floppy_random_bits_max;
bool tod_hack;
unsigned long maprom;
int turbo_emulation;
bool headless;

int cs_compatible;
int cs_ciaatod;
int cs_rtc;
int cs_rtc_adjust;
int cs_rtc_adjust_mode;
bool cs_ksmirror_e0;
bool cs_ksmirror_a8;
bool cs_ciaoverlay;
bool cs_cd32cd;
bool cs_cd32c2p;
bool cs_cd32nvram;
bool cs_cdtvcd;
bool cs_cdtvram;
int cs_cdtvcard;
int cs_ide;
bool cs_pcmcia;
bool cs_a1000ram;
int cs_fatgaryrev;
int cs_ramseyrev;
int cs_agnusrev;
int cs_deniserev;
int cs_mbdmac;
bool cs_cdtvscsi;
bool cs_a2091, cs_a4091;
bool cs_df0idhw;
bool cs_slowmemisfast;
bool cs_resetwarning;
bool cs_denisenoehb;
bool cs_dipagnus;
bool cs_agnusbltbusybug;

char romfile[100];
char romident[256];
char romextfile[100];
char romextident[256];
char keyfile[256];
char flashfile[100];
#ifdef ACTION_REPLAY
char cartfile[100];
char cartident[256];
int cart_internal;
#endif
char pci_devices[256];
char prtname[256];
char sername[256];
char amaxromfile[100];
char a2065name[100];

char quitstatefile[100];
char statefile[100];
#ifndef WIN32
char scsi_device[256];
#endif

char path_floppy[256];
char path_hardfile[256];
char path_rom[256];
char path_savestate[256];

int m68k_speed;
int cpu_model;
int mmu_model;
int cpu060_revision;
int fpu_model;
int fpu_revision;
bool cpu_compatible;
bool address_space_24;
bool picasso96_nocustom;
int picasso96_modeflags;

#ifdef HAVE_MACHDEP_TIMER
    int use_processor_clock;
#endif

unsigned long z3fastmem_size, z3fastmem2_size;
unsigned long z3fastmem_start;
unsigned long z3chipmem_size;
unsigned long z3chipmem_start;
unsigned long fastmem_size;
unsigned long chipmem_size;
unsigned long bogomem_size;
unsigned long mbresmem_low_size;
unsigned long mbresmem_high_size;
unsigned long gfxmem_size;
unsigned long custom_memory_addrs[10];
unsigned long custom_memory_sizes[10];

bool kickshifter;
bool filesys_no_uaefsdb;
bool filesys_custom_uaefsdb;
bool mmkeyboard;
int uae_hide;

int nr_floppies;

char dfxlist[10][100];
#ifdef DRIVESOUND
int dfxclickvolume;
int dfxclickchannelmask;
#endif

int hide_cursor; /* Whether to hide host WM cursor or not */

/* Target specific options */
#ifdef USE_X11_GFX
int x11_use_low_bandwidth;
int x11_use_mitshm;
int x11_use_dgamode;
int x11_hide_cursor;
#endif

#ifdef USE_SVGALIB_GFX
int svga_no_linear;
#endif

#ifdef _WIN32
bool win32_middle_mouse;
bool win32_logfile;
bool win32_notaskbarbutton;
bool win32_alwaysontop;
bool win32_powersavedisabled;
bool win32_minimize_inactive;
int win32_statusbar;

int win32_active_priority;
int win32_inactive_priority;
bool win32_inactive_pause;
bool win32_inactive_nosound;
int win32_iconified_priority;
bool win32_iconified_pause;
bool win32_iconified_nosound;

bool win32_rtgmatchdepth;
bool win32_rtgscaleifsmall;
bool win32_rtgallowscaling;
int win32_rtgscaleaspectratio;
bool win32_borderless;
bool win32_ctrl_F11_is_quit;
bool win32_automount_removable;
bool win32_automount_drives;
bool win32_automount_cddrives;
bool win32_automount_netdrives;
bool win32_automount_removabledrives;
int win32_midioutdev;
int win32_midiindev;
int win32_uaescsimode;
int win32_soundcard;
int win32_samplersoundcard;
bool win32_soundexclusive;
bool win32_norecyclebin;
int win32_specialkey;
int win32_guikey;
int win32_kbledmode;
char win32_commandpathstart[100];
char win32_commandpathend[100];
char win32_parjoyport0[100];
char win32_parjoyport1[100];
#endif
int win32_rtgvblankrate;

#ifdef USE_CURSES_GFX
int curses_reverse_video;
#endif

#if defined USE_SDL_GFX || defined USE_X11_GFX
int map_raw_keys;
#endif
int use_gl;

#ifdef USE_AMIGA_GFX
int amiga_screen_type;
char amiga_publicscreen[256];
int amiga_use_grey;
int amiga_use_dither;
#endif

#ifdef SAVESTATE
bool statecapture;
int statecapturerate, statecapturebuffersize;
#endif

/* input */

int input_selected_setting;
int input_joymouse_multiplier;
int input_joymouse_deadzone;
int input_joystick_deadzone;
int input_joymouse_speed;
int input_analog_joystick_mult;
int input_analog_joystick_offset;
int input_autofire_linecnt;
int input_mouse_speed;
int input_tablet;
bool input_magic_mouse;
int input_magic_mouse_cursor;

int dongle;
};


#endif // PUAE_MAINWINDOW_H
