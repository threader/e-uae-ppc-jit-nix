/*
 * UAE - the Un*x Amiga Emulator
 *
 * Yet Another User Interface for the X11 version
 *
 * Copyright 1997, 1998 Bernd Schmidt
 * Copyright 1998 Michael Krause
 * 
 * The Tk GUI doesn't work.
 * The X Forms Library isn't available as source, and there aren't any
 * binaries compiled against glibc
 *
 * So let's try this...
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "gui.h"
#include "newcpu.h"
#include "autoconf.h"
#include "threaddep/thread.h"
#include "sounddep/sound.h"
#include "savestate.h"
#include "compemu.h"
#include "debug.h"
#include "inputdevice.h"

//#define GUI_DEBUG
#ifdef  GUI_DEBUG
#define DEBUG_LOG(...) write_log(__FUNCTION__": " __VA_ARGS__)
#else
#define DEBUG_LOG(...) do ; while(0)
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

/* One of the 1.1.6 "features" is a gratuitous name change */
#ifndef HAVE_GTK_FEATURES_1_1_6
#define gtk_container_set_border_width gtk_container_border_width
#endif
/* Likewise for 1.1.8.  */
#ifndef HAVE_GTK_FEATURES_1_1_8
#define gtk_label_set_text gtk_label_set
#endif
/* This is beginning to suck... */
#ifndef HAVE_GTK_FEATURES_1_1_13
#define gtk_toggle_button_set_active gtk_toggle_button_set_state
#endif

static int gui_active;

static GtkWidget *gui_window;

static GtkWidget *pause_uae_widget, *snap_save_widget, *snap_load_widget;

static GtkWidget *chipsize_widget[5];
static GtkWidget *bogosize_widget[4];
static GtkWidget *fastsize_widget[5];
static GtkWidget *z3size_widget[10];
static GtkWidget *p96size_widget[7];
static GtkWidget *rom_text_widget, *key_text_widget;
static GtkWidget *rom_change_widget, *key_change_widget;

static GtkWidget *disk_insert_widget[4], *disk_eject_widget[4], *disk_text_widget[4];
static char *new_disk_string[4];

static GtkAdjustment *cpuspeed_adj;
static GtkWidget *cpuspeed_widgets[4], *cpuspeed_scale;
static GtkWidget *cpu_widget[5], *a24m_widget, *ccpu_widget;
static GtkWidget *sound_widget[4], *sound_bits_widget[2], *sound_freq_widget[3], *sound_ch_widget[3];

static GtkWidget *coll_widget[4], *cslevel_widget[4];
static GtkWidget *fcop_widget;

static GtkAdjustment *framerate_adj;
static GtkWidget *bimm_widget, *b32_widget, *afscr_widget, *pfscr_widget;

#ifdef JIT
static GtkWidget *compbyte_widget[4], *compword_widget[4], *complong_widget[4];
static GtkWidget *compaddr_widget[4], *compnf_widget[2], *comp_midopt_widget[2];
static GtkWidget *comp_lowopt_widget[2], *compfpu_widget[2], *comp_hardflush_widget[2];
static GtkWidget *comp_constjump_widget[2];
static GtkAdjustment *cachesize_adj;
#endif

static GtkWidget *joy_widget[2][6];

static GtkWidget *led_widgets[5];
static GdkColor led_on[5], led_off[5];
static unsigned int prevledstate;

static GtkWidget *hdlist_widget;
static int selected_hd_row;
static GtkWidget *hdchange_button, *hddel_button;
static GtkWidget *devname_entry, *volname_entry, *path_entry;
static GtkWidget *readonly_widget, *bootpri_widget;
static GtkWidget *dirdlg;
static GtkWidget *dirdlg_ok;
static char dirdlg_devname[256], dirdlg_volname[256], dirdlg_path[256];

enum hdlist_cols {
    HDLIST_DEVICE, 
    HDLIST_VOLUME,
    HDLIST_PATH,
    HDLIST_READONLY,
    HDLIST_HEADS,
    HDLIST_CYLS,
    HDLIST_SECS,
    HDLIST_RSRVD,
    HDLIST_SIZE,
    HDLIST_BLKSIZE,             
    HDLIST_BOOTPRI,
//    HDLIST_FILESYSDIR, 
    HDLIST_MAX_COLS
};

static const char *hdlist_col_titles[] = {
     "Device",
     "Volume",
     "File/Directory",
     "R/O",
     "Heads",
     "Cyl.",
     "Sec.",
     "Rsrvd",
     "Size",
     "Blksize",
     "Boot pri",
//    "Filesysdir?"     
     NULL
};


static smp_comm_pipe to_gui_pipe;   // For sending messages to the GUI from UAE
static smp_comm_pipe from_gui_pipe; // For sending messages from the GUI to UAE

/*
 * Messages sent to GUI from UAE via to_gui_pipe
 */
enum gui_commands {
    GUICMD_UPDATE,       // Refresh your state from changed preferences
    GUICMD_DISKCHANGE,   // Hey! A disk has been changed. Do something!
    GUICMD_MSGBOX,       // Display a message box for me, please
    GUICMD_PAUSE,        // We're now paused, in case you didn't notice
    GUICMD_UNPAUSE       // We're now running.
};

static uae_sem_t gui_sem;        // For mutual exclusion on various prefs settings
static uae_sem_t gui_update_sem; // For synchronization between gui_update() and the GUI thread
static uae_sem_t gui_init_sem;   // For the GUI thread to tell UAE that it's ready.
static uae_sem_t gui_quit_sem;   // For the GUI thread to tell UAE that it's quitting.

static volatile int quit_gui = 0, quitted_gui = 0;



void gui_set_paused (int state);

static void do_message_box( const guchar *title, const guchar *message, gboolean modal, gboolean wait );
static void handle_message_box_request (smp_comm_pipe *msg_pipe);
static GtkWidget *make_message_box( const guchar *title, const guchar *message, int modal, uae_sem_t *sem );
void on_message_box_quit (GtkWidget *w, gpointer user_data);


/*
 * The variable uae_paused determines whether UAE is paused. If this is set to TRUE
 * gui_handle_events() won't return when called, and, since this is invoked from UAE's
 * main event loop, UAE is thus paused. Not a terribly nice way of doing
 * things, but it's what we're stuck with just now . . .
 *
 * Previously only gui_handle_events() cared whether UAE was paused or not. I've
 * made this global to the GUI code because various other routines need to know whether
 * UAE is paused to be able to work properly. Ideally main.c should be revised to handle
 * pausing of UAE itself. Then all the yuckiness with the handling of pausing can go away.
 */
static int uae_paused;

static int is_uae_paused (void)
{
    return uae_paused;
}

static void set_uae_paused (int state)
{
    uae_paused = state;
}



static void save_config (void)
{
    FILE *f;
    char tmp[257];

    /* Backup the options file.  */
    strcpy (tmp, optionsfile);
    strcat (tmp, "~");
    rename (optionsfile, tmp);

    f = fopen (optionsfile, "w");
    if (f == NULL) {
	gui_message ("Error saving options file!\n");
	return;
    }
  
    if( is_uae_paused() )
        save_options (f, &changed_prefs);
    else
        save_options (f, &currprefs);
    fclose (f);
}

static int nr_for_led (GtkWidget *led)
{
    int i;
    i = 0;
    while (led_widgets[i] != led)
	i++;
    return i;
}

static void enable_disk_buttons (int enable)
{
    int i;
    for (i = 0; i < 4; i++) {
	gtk_widget_set_sensitive (disk_insert_widget[i], enable);
	gtk_widget_set_sensitive (disk_eject_widget[i], enable);
    }
}

static void enable_snap_buttons (int enable)
{
// temporarily disable these until snapshots are working - Rich
//    gtk_widget_set_sensitive (snap_save_widget, enable);
//    gtk_widget_set_sensitive (snap_load_widget, enable);
}

static void set_cpu_state (void)
{
    int i;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (a24m_widget), changed_prefs.address_space_24 != 0);
    gtk_widget_set_sensitive (a24m_widget, changed_prefs.cpu_level > 1 && changed_prefs.cpu_level < 4);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ccpu_widget), changed_prefs.cpu_compatible != 0);
    gtk_widget_set_sensitive (ccpu_widget, changed_prefs.cpu_level == 0);
    gtk_widget_set_sensitive (cpuspeed_scale, changed_prefs.m68k_speed > 0);
    for (i = 0; i < 10; i++)
	gtk_widget_set_sensitive (z3size_widget[i],
				  changed_prefs.cpu_level >= 2 && ! changed_prefs.address_space_24);
}

static void set_cpu_widget (void)
{
    int nr = changed_prefs.cpu_level;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cpu_widget[nr]), TRUE);
    nr = currprefs.m68k_speed + 1 < 3 ? currprefs.m68k_speed + 1 : 2;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cpuspeed_widgets[nr]), TRUE);

}

static void set_gfx_state (void)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bimm_widget), currprefs.immediate_blits != 0);
#if 0
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b32_widget), currprefs.blits_32bit_enabled != 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (afscr_widget), currprefs.gfx_afullscreen != 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pfscr_widget), currprefs.gfx_pfullscreen != 0);
#endif
}

static void set_chipset_state (void)
{
    int t0 = 0;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (coll_widget[currprefs.collision_level]), TRUE);
    if (currprefs.chipset_mask & CSMASK_ECS_DENISE)
	t0 = 2;
    if (currprefs.chipset_mask & CSMASK_ECS_AGNUS)
	t0 = 1;
#ifdef AGA   
    if (currprefs.chipset_mask & CSMASK_AGA)
	t0 = 3;
#endif   
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cslevel_widget[t0]), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fcop_widget), currprefs.fast_copper != 0);
}

static void set_sound_state (void)
{
    int stereo = currprefs.stereo + currprefs.mixed_stereo;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sound_widget[currprefs.produce_sound]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sound_ch_widget[stereo]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sound_bits_widget[currprefs.sound_bits == 16]), 1);

}

static void set_mem_state (void)
{
    int t, t2;

    t = 0;
    t2 = currprefs.chipmem_size;
    while (t < 4 && t2 > 0x80000)
	t++, t2 >>= 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chipsize_widget[t]), 1);

    t = 0;
    t2 = currprefs.bogomem_size;
    while (t < 3 && t2 >= 0x80000)
	t++, t2 >>= 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bogosize_widget[t]), 1);

    t = 0;
    t2 = currprefs.fastmem_size;
    while (t < 4 && t2 >= 0x100000)
	t++, t2 >>= 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fastsize_widget[t]), 1);

    t = 0;
    t2 = currprefs.z3fastmem_size;
    while (t < 9 && t2 >= 0x100000)
	t++, t2 >>= 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (z3size_widget[t]), 1);

    t = 0;
    t2 = currprefs.gfxmem_size;
    while (t < 6 && t2 >= 0x100000)
	t++, t2 >>= 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p96size_widget[t]), 1);

    gtk_label_set_text (GTK_LABEL (rom_text_widget), currprefs.romfile);
    gtk_label_set_text (GTK_LABEL (key_text_widget), currprefs.keyfile);
}

#ifdef JIT
static void set_comp_state (void)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (compbyte_widget[currprefs.comptrustbyte]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (compword_widget[currprefs.comptrustword]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (complong_widget[currprefs.comptrustlong]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (compaddr_widget[currprefs.comptrustnaddr]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (compnf_widget[currprefs.compnf]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_hardflush_widget[currprefs.comp_hardflush]), 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_constjump_widget[currprefs.comp_constjump]), 1);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (compfpu_widget[currprefs.compfpu]), 1);
#if USE_OPTIMIZER
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_midopt_widget[currprefs.comp_midopt]), 1);
#endif
#if USE_LOW_OPTIMIZER
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_lowopt_widget[currprefs.comp_lowopt]), 1);
#endif
}
#endif

static void set_joy_state (void)
{
    int j0t = changed_prefs.jport0;
    int j1t = changed_prefs.jport1;
    int i;

    if (j0t == j1t) {
	/* Can't happen */
	j0t++;
	j0t %= 6;
    }
    for (i = 0; i < 6; i++) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (joy_widget[0][i]), j0t == i);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (joy_widget[1][i]), j1t == i);
	gtk_widget_set_sensitive (joy_widget[0][i], j1t != i);
	gtk_widget_set_sensitive (joy_widget[1][i], j0t != i);
    }
}

static void set_hd_state (void)
{
    char  texts[HDLIST_MAX_COLS][256];
    char *tptrs[HDLIST_MAX_COLS];
    int nr = nr_units (currprefs.mountinfo);
    int i;

    for (i=0; i<HDLIST_MAX_COLS; i++)
        tptrs[i] = texts[i];
   
    gtk_clist_freeze (GTK_CLIST (hdlist_widget));
    gtk_clist_clear (GTK_CLIST (hdlist_widget));

    for (i = 0; i < nr; i++) {
        int     secspertrack, surfaces, reserved, blocksize, bootpri;
	uae_u64 size;
	int     cylinders, readonly;
	char   *devname, *volname, *rootdir, *filesysdir;
	char   *failure;

	/* We always use currprefs.mountinfo for the GUI.  The filesystem
	   code makes a private copy which is updated every reset.  */
	failure = get_filesys_unit (currprefs.mountinfo, i,
				    &devname, &volname, &rootdir, &readonly,
				    &secspertrack, &surfaces, &reserved,
				    &cylinders, &size, &blocksize, &bootpri, &filesysdir );
	    
	if (is_hardfile (currprefs.mountinfo, i)) {
	    strncpy (texts[HDLIST_DEVICE],  devname, 255);
	    sprintf (texts[HDLIST_VOLUME],  "DH%d", i );
	    sprintf (texts[HDLIST_HEADS],   "%d", surfaces);
	    sprintf (texts[HDLIST_CYLS],    "%d", cylinders);
	    sprintf (texts[HDLIST_SECS],    "%d", secspertrack);
	    sprintf (texts[HDLIST_RSRVD],   "%d", reserved);
	    sprintf (texts[HDLIST_SIZE],    "%d", size);
	    sprintf (texts[HDLIST_BLKSIZE], "%d", blocksize);
	} else {
	    strncpy (texts[HDLIST_DEVICE], devname, 255);
	    strcpy (texts[HDLIST_VOLUME],  volname);
	    strcpy (texts[HDLIST_HEADS],   "N/A");
	    strcpy (texts[HDLIST_CYLS],    "N/A");
	    strcpy (texts[HDLIST_SECS],    "N/A");
	    strcpy (texts[HDLIST_RSRVD],   "N/A");
	    strcpy (texts[HDLIST_SIZE],    "N/A");
	    strcpy (texts[HDLIST_BLKSIZE], "N/A");
	}
	strcpy  (texts[HDLIST_PATH],     rootdir);
	strcpy  (texts[HDLIST_READONLY], readonly ? "Y" : "N");
        sprintf (texts[HDLIST_BOOTPRI], "%d", bootpri);
	gtk_clist_append (GTK_CLIST (hdlist_widget), tptrs);
    }
    gtk_clist_thaw (GTK_CLIST (hdlist_widget));
    gtk_widget_set_sensitive (hdchange_button, FALSE);
    gtk_widget_set_sensitive (hddel_button, FALSE);
}

static void set_floppy_state( void )
{
    gtk_label_set_text (GTK_LABEL (disk_text_widget[0]), currprefs.df[0]);
}

	

static void draw_led (int nr)
{
    if (nr<5 && led_widgets[nr]) {
        GtkWidget *thing  = led_widgets[nr];
        GdkWindow *window = thing->window;
         
        if (window) {
	    GdkGC    *gc = gdk_gc_new (window);
	    GdkColor *col;
	   
            if (gui_ledstate & (1 << nr))
	        col = led_on + nr;
            else
	        col = led_off + nr;
	   
            gdk_gc_set_foreground (gc, col);
            gdk_draw_rectangle (window, gc, 1, 0, 0, -1, -1);
            gdk_gc_destroy (gc);
	}
    }
}


/*
 * my_idle()
 *
 * This function is added as a callback to the GTK+ mainloop
 * and is run every 1000ms. It handles messages sent from UAE and
 * updates the floppy drive LEDs.
 * 
 * TODO: the floppy drives LEDS should be a separate call back. Then
 * we can call this more frequently without wasting too much CPU time.
 * 1000ms is too slow for responding to GUI events.
 */
static int my_idle (void)
{
    unsigned int leds = gui_ledstate;
    int i;

    if (quit_gui) {
	gtk_main_quit ();
	goto out;
    }
    while (comm_pipe_has_data (&to_gui_pipe)) {
	int cmd = read_comm_pipe_int_blocking (&to_gui_pipe);
	int n;
	switch (cmd) {
	 case GUICMD_DISKCHANGE:
	    n = read_comm_pipe_int_blocking (&to_gui_pipe);
	    gtk_label_set_text (GTK_LABEL (disk_text_widget[n]),
			       is_uae_paused() ? changed_prefs.df[n] : currprefs.df[n]);
	    break;
	 case GUICMD_UPDATE:
	    set_cpu_widget ();
	    set_cpu_state ();
	    set_gfx_state ();
	    set_joy_state ();
	    set_sound_state ();
#ifdef JIT	   
	    set_comp_state ();
#endif	   
	    set_mem_state ();
	    set_floppy_state (); 
	    set_hd_state ();
	    set_chipset_state ();

	    gtk_widget_show (gui_window);  // Should find a better place to do this, surely? - Rich
	    uae_sem_post (&gui_update_sem);
	    gui_active = 1;
	    break;
	 case GUICMD_MSGBOX:
	    handle_message_box_request(&to_gui_pipe);
            break;
	 case GUICMD_PAUSE:
	 case GUICMD_UNPAUSE:
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pause_uae_widget),
					  cmd==GUICMD_PAUSE ? TRUE : FALSE);
	    set_uae_paused (cmd==GUICMD_PAUSE);
	    break;
	}
    }

    for (i = 0; i < 5; i++) {
	unsigned int mask = 1 << i;
	unsigned int on = leds & mask;

	if (on == (prevledstate & mask))
	    continue;

/*	printf(": %d %d\n", i, on);*/
	draw_led (i);
    }
    prevledstate = leds;
out:
    return 1;
}

static int find_current_toggle (GtkWidget **widgets, int count)
{
    int i;
    for (i = 0; i < count; i++)
	if (GTK_TOGGLE_BUTTON (*widgets++)->active)
	    return i;
    write_log ("GTKUI: Can't happen!\n");
    return -1;
}

static void joy_changed (void)
{
    if (! gui_active)
	return;
    changed_prefs.jport0 = find_current_toggle (joy_widget[0], 6);
    changed_prefs.jport1 = find_current_toggle (joy_widget[1], 6);
    
    if( changed_prefs.jport0 != currprefs.jport0 || changed_prefs.jport1 != currprefs.jport1 )
        inputdevice_config_change();

    set_joy_state ();
}

static void coll_changed (void)
{
    changed_prefs.collision_level = find_current_toggle (coll_widget, 4);
}

static void cslevel_changed (void)
{
    int t = find_current_toggle (cslevel_widget, 4);
    int t1 = 0;
    if (t > 0)
	t1 |= CSMASK_ECS_AGNUS;
    if (t > 1)
	t1 |= CSMASK_ECS_DENISE;
#ifdef AGA   
    if (t > 2)
	t1 |= CSMASK_AGA;
#endif   
    changed_prefs.chipset_mask = t1;
}

static void custom_changed (void)
{
    changed_prefs.gfx_framerate = framerate_adj->value;
    changed_prefs.immediate_blits = GTK_TOGGLE_BUTTON (bimm_widget)->active;
    changed_prefs.fast_copper = GTK_TOGGLE_BUTTON (fcop_widget)->active;
#if 0
    changed_prefs.blits_32bit_enabled = GTK_TOGGLE_BUTTON (b32_widget)->active;
    changed_prefs.gfx_afullscreen = GTK_TOGGLE_BUTTON (afscr_widget)->active;
    changed_prefs.gfx_pfullscreen = GTK_TOGGLE_BUTTON (pfscr_widget)->active;
#endif
}

static void cpuspeed_changed (void)
{
    int which = find_current_toggle (cpuspeed_widgets, 3);
    changed_prefs.m68k_speed = (which == 0 ? -1
				: which == 1 ? 0
				: cpuspeed_adj->value);
    set_cpu_state ();
}

static void cputype_changed (void)
{
    int i, oldcl;
    if (! gui_active)
	return;

    oldcl = changed_prefs.cpu_level;

    changed_prefs.cpu_level = find_current_toggle (cpu_widget, 5);
    changed_prefs.cpu_compatible = GTK_TOGGLE_BUTTON (ccpu_widget)->active;
    changed_prefs.address_space_24 = GTK_TOGGLE_BUTTON (a24m_widget)->active;

    if (changed_prefs.cpu_level != 0)
	changed_prefs.cpu_compatible = 0;
    /* 68000/68010 always have a 24 bit address space.  */
    if (changed_prefs.cpu_level < 2)
	changed_prefs.address_space_24 = 1;
    /* Changing from 68000/68010 to 68020 should set a sane default.  */
    else if (oldcl < 2)
	changed_prefs.address_space_24 = 0;

    set_cpu_state ();
}

static void chipsize_changed (void)
{
    int t = find_current_toggle (chipsize_widget, 5);
    changed_prefs.chipmem_size = 0x80000 << t;
    for (t = 0; t < 5; t++)
	gtk_widget_set_sensitive (fastsize_widget[t], changed_prefs.chipmem_size <= 0x200000);
    if (changed_prefs.chipmem_size > 0x200000) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fastsize_widget[0]), 1);
	changed_prefs.fastmem_size = 0;
    }
}

static void bogosize_changed (void)
{
    int t = find_current_toggle (bogosize_widget, 4);
    changed_prefs.bogomem_size = (0x40000 << t) & ~0x40000;
}

static void fastsize_changed (void)
{
    int t = find_current_toggle (fastsize_widget, 5);
    changed_prefs.fastmem_size = (0x80000 << t) & ~0x80000;
}

static void z3size_changed (void)
{
    int t = find_current_toggle (z3size_widget, 10);
    changed_prefs.z3fastmem_size = (0x80000 << t) & ~0x80000;
}

static void p96size_changed (void)
{
    int t = find_current_toggle (p96size_widget, 7);
    changed_prefs.gfxmem_size = (0x80000 << t) & ~0x80000;
}

static void sound_changed (void)
{
    changed_prefs.produce_sound = find_current_toggle (sound_widget, 4);
    changed_prefs.stereo = find_current_toggle (sound_ch_widget, 3);
    changed_prefs.mixed_stereo = 0;
    if (changed_prefs.stereo == 2)
	changed_prefs.mixed_stereo = changed_prefs.stereo = 1;
    changed_prefs.sound_bits = (find_current_toggle (sound_bits_widget, 2) + 1) * 8;
}

#ifdef JIT
static void comp_changed (void)
{
  changed_prefs.cachesize=cachesize_adj->value;
  changed_prefs.comptrustbyte = find_current_toggle (compbyte_widget, 4);
  changed_prefs.comptrustword = find_current_toggle (compword_widget, 4);
  changed_prefs.comptrustlong = find_current_toggle (complong_widget, 4);
  changed_prefs.comptrustnaddr = find_current_toggle (compaddr_widget, 4);
  changed_prefs.compnf = find_current_toggle (compnf_widget, 2);
  changed_prefs.comp_hardflush = find_current_toggle (comp_hardflush_widget, 2);
  changed_prefs.comp_constjump = find_current_toggle (comp_constjump_widget, 2);
  changed_prefs.compfpu= find_current_toggle (compfpu_widget, 2);
#if USE_OPTIMIZER
  changed_prefs.comp_midopt = find_current_toggle (comp_midopt_widget, 2);
#endif
#if USE_LOW_OPTIMIZER
  changed_prefs.comp_lowopt = find_current_toggle (comp_lowopt_widget, 2);
#endif
}
#endif

static void did_reset (void)
{
    DEBUG_LOG ("Called\n");

    if (!quit_gui)
        write_comm_pipe_int (&from_gui_pipe, 2, 1);
}

static void did_debug (void)
{
    DEBUG_LOG ("Called\n");

    if (!quit_gui)
        write_comm_pipe_int (&from_gui_pipe, 3, 1);
}

static void did_quit (void)
{
    DEBUG_LOG ("Called\n");

    if (!quit_gui)
        write_comm_pipe_int (&from_gui_pipe, 4, 1);
}

static void did_eject (GtkWidget *w, gpointer data)
{
    DEBUG_LOG ("Called with %d\n", (int)data);
				  
    if (!quit_gui) {
        write_comm_pipe_int (&from_gui_pipe, 0, 0);
        write_comm_pipe_int (&from_gui_pipe, (int)data, 1);
    }
}

static void pause_uae (GtkWidget *widget, gpointer data)
{
    DEBUG_LOG ( "Called with %d\n", GTK_TOGGLE_BUTTON (widget)->active == TRUE );
   
    if (!quit_gui) {
//        set_uae_paused (GTK_TOGGLE_BUTTON (widget)->active == TRUE ? TRUE : FALSE);
        write_comm_pipe_int (&from_gui_pipe, GTK_TOGGLE_BUTTON (widget)->active ? 5 : 6, 1);
    }
}

//static void end_pause_uae (void)
//{
//    
//    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pause_uae_widget), FALSE);
//}

static int filesel_active = -1;
static GtkWidget *disk_selector;

static int snapsel_active = -1;
static char *gui_snapname, *gui_romname, *gui_keyname;

static void did_close_insert (gpointer data)
{
    filesel_active = -1;
    enable_disk_buttons (1);
}

static void did_insert_select (GtkObject *o)
{
    char *s = gtk_file_selection_get_filename (GTK_FILE_SELECTION (disk_selector));
//    printf ("%d %s\n", filesel_active, s);
    if (quit_gui)
	return;

    uae_sem_wait (&gui_sem);
    if (new_disk_string[filesel_active] != 0)
	free (new_disk_string[filesel_active]);
    new_disk_string[filesel_active] = strdup (s);
    uae_sem_post (&gui_sem);
    write_comm_pipe_int (&from_gui_pipe, 1, 0);
    write_comm_pipe_int (&from_gui_pipe, filesel_active, 1);
    filesel_active = -1;
    enable_disk_buttons (1);
    gtk_widget_destroy (disk_selector);
}

static char fsbuffer[100];

static GtkWidget *make_file_selector (const char *title,
				      void (*insertfunc)(GtkObject *),
				      void (*closefunc)(gpointer))
{
    GtkWidget *p = gtk_file_selection_new (title);
    gtk_signal_connect (GTK_OBJECT (p), "destroy", (GtkSignalFunc) closefunc, p);

    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (p)->ok_button),
			       "clicked", (GtkSignalFunc) insertfunc,
			       GTK_OBJECT (p));
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (p)->cancel_button),
			       "clicked", (GtkSignalFunc) gtk_widget_destroy,
			       GTK_OBJECT (p));

#if 0
    gtk_window_set_title (GTK_WINDOW (p), title);
#endif

    gtk_widget_show (p);
    return p;
}

static void filesel_set_path (GtkWidget *p, const char *path)
{
    size_t len = strlen (path);
    if (len > 0 && ! access (path, R_OK)) {
	char *tmp = xmalloc (len + 2);
	strcpy (tmp, path);
	strcat (tmp, "/");
	gtk_file_selection_set_filename (GTK_FILE_SELECTION (p),
					 tmp);
    }
}

static void did_insert (GtkWidget *w, gpointer data)
{
    int n = (int)data;
    if (filesel_active != -1)
	return;
    filesel_active = n;
    enable_disk_buttons (0);

    sprintf (fsbuffer, "Select a disk image file for DF%d", n);
    disk_selector = make_file_selector (fsbuffer, did_insert_select, did_close_insert);
    filesel_set_path (disk_selector, currprefs.path_floppy);
}

static gint driveled_event (GtkWidget *thing, GdkEvent *event)
{
    int lednr = nr_for_led (thing);

    switch (event->type) {
     case GDK_MAP:
	draw_led (lednr);
	break;
     case GDK_EXPOSE:
	draw_led (lednr);
	break;
     default:
	break;
    }

  return 0;
}

static GtkWidget *snap_selector;

static void did_close_snap (gpointer gdata)
{
    snapsel_active = -1;
    enable_snap_buttons (1);
}

static void did_snap_select (GtkObject *o)
{
    char *s = gtk_file_selection_get_filename (GTK_FILE_SELECTION (snap_selector));

    if (quit_gui)
	return;

    uae_sem_wait (&gui_sem);
    gui_snapname = strdup (s);
    uae_sem_post (&gui_sem);
    write_comm_pipe_int (&from_gui_pipe, 7, 0);
    write_comm_pipe_int (&from_gui_pipe, snapsel_active, 1);
    snapsel_active = -1;
    enable_snap_buttons (1);
    gtk_widget_destroy (snap_selector);
}

static void did_loadstate (void)
{
    if (snapsel_active != -1)
	return;
    snapsel_active = STATE_DORESTORE;
    enable_snap_buttons (0);

    snap_selector = make_file_selector ("Select a state file to restore",
					did_snap_select, did_close_snap);
}

static void did_savestate (void)
{
    if (snapsel_active != -1)
	return;
    snapsel_active = STATE_DOSAVE;
    enable_snap_buttons (0);

    snap_selector = make_file_selector ("Select a filename for the state file",
					did_snap_select, did_close_snap);
}

static GtkWidget *rom_selector;

static void did_close_rom (gpointer gdata)
{
    gtk_widget_set_sensitive (rom_change_widget, 1);
}

static void did_rom_select (GtkObject *o)
{
    char *s = gtk_file_selection_get_filename (GTK_FILE_SELECTION (rom_selector));

    if (quit_gui)
	return;

    gtk_widget_set_sensitive (rom_change_widget, 1);

    uae_sem_wait (&gui_sem);
    gui_romname = strdup (s);
    uae_sem_post (&gui_sem);
    write_comm_pipe_int (&from_gui_pipe, 8, 0);
    gtk_label_set_text (GTK_LABEL (rom_text_widget), gui_romname);
    gtk_widget_destroy (rom_selector);
}

static void did_romchange (GtkWidget *w, gpointer data)
{
    gtk_widget_set_sensitive (rom_change_widget, 0);

    rom_selector = make_file_selector ("Select a ROM file",
				       did_rom_select, did_close_rom);
    filesel_set_path (rom_selector, currprefs.path_rom);
}

static GtkWidget *key_selector;

static void did_close_key (gpointer gdata)
{
    gtk_widget_set_sensitive (key_change_widget, 1);
}

static void did_key_select (GtkObject *o)
{
    char *s = gtk_file_selection_get_filename (GTK_FILE_SELECTION (key_selector));

    if (quit_gui)
	return;

    gtk_widget_set_sensitive (key_change_widget, 1);

    uae_sem_wait (&gui_sem);
    gui_keyname = strdup (s);
    uae_sem_post (&gui_sem);
    write_comm_pipe_int (&from_gui_pipe, 9, 0);
    gtk_label_set_text (GTK_LABEL (key_text_widget), gui_keyname);
    gtk_widget_destroy (key_selector);
}

static void did_keychange (GtkWidget *w, gpointer data)
{
    gtk_widget_set_sensitive (key_change_widget, 0);

    key_selector = make_file_selector ("Select a Kickstart key file",
				       did_key_select, did_close_key);
    filesel_set_path (key_selector, currprefs.path_rom);
}

static void add_empty_vbox (GtkWidget *tobox)
{
    GtkWidget *thing = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (tobox), thing, TRUE, TRUE, 0);
}

static void add_empty_hbox (GtkWidget *tobox)
{
    GtkWidget *thing = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (tobox), thing, TRUE, TRUE, 0);
}

static void add_centered_to_vbox (GtkWidget *vbox, GtkWidget *w)
{
    GtkWidget *hbox = gtk_hbox_new (TRUE, 0);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
}

static GtkWidget *make_labelled_widget (const char *str, GtkWidget *thing)
{
    GtkWidget *label = gtk_label_new (str);
    GtkWidget *hbox2 = gtk_hbox_new (FALSE, 4);

    gtk_widget_show (label);
    gtk_widget_show (thing);

    gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), thing, FALSE, TRUE, 0);

    return hbox2;
}

static GtkWidget *add_labelled_widget_centered (const char *str, GtkWidget *thing, GtkWidget *vbox)
{
    GtkWidget *w = make_labelled_widget (str, thing);
    gtk_widget_show (w);
    add_centered_to_vbox (vbox, w);
    return w;
}

static int make_radio_group (const char **labels, GtkWidget *tobox,
			      GtkWidget **saveptr, gint t1, gint t2,
			      void (*sigfunc) (void), int count, GSList *group)
{
    int t = 0;

    while (*labels && (count == -1 || count-- > 0)) {
	GtkWidget *thing = gtk_radio_button_new_with_label (group, *labels++);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (thing));

	*saveptr++ = thing;
	gtk_widget_show (thing);
	gtk_box_pack_start (GTK_BOX (tobox), thing, t1, t2, 0);
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) sigfunc, NULL);
	t++;
    }
    return t;
}

static GtkWidget *make_radio_group_box (const char *title, const char **labels,
					GtkWidget **saveptr, int horiz,
					void (*sigfunc) (void))
{
    GtkWidget *frame, *newbox;

    frame = gtk_frame_new (title);
    newbox = (horiz ? gtk_hbox_new : gtk_vbox_new) (FALSE, 4);
    gtk_widget_show (newbox);
    gtk_container_set_border_width (GTK_CONTAINER (newbox), 4);
    gtk_container_add (GTK_CONTAINER (frame), newbox);
    make_radio_group (labels, newbox, saveptr, horiz, !horiz, sigfunc, -1, NULL);
    return frame;
}

static GtkWidget *make_radio_group_box_1 (const char *title, const char **labels,
					  GtkWidget **saveptr, int horiz,
					  void (*sigfunc) (void), int elts_per_column)
{
    GtkWidget *frame, *newbox;
    GtkWidget *column;
    GSList *group = 0;

    frame = gtk_frame_new (title);
    column = (horiz ? gtk_vbox_new : gtk_hbox_new) (FALSE, 4);
    gtk_container_add (GTK_CONTAINER (frame), column);
    gtk_widget_show (column);

    while (*labels) {
	int count;
	newbox = (horiz ? gtk_hbox_new : gtk_vbox_new) (FALSE, 4);
	gtk_widget_show (newbox);
	gtk_container_set_border_width (GTK_CONTAINER (newbox), 4);
	gtk_container_add (GTK_CONTAINER (column), newbox);
	count = make_radio_group (labels, newbox, saveptr, horiz, !horiz, sigfunc, elts_per_column, group);
	labels += count;
	saveptr += count;
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (saveptr[-1]));
    }
    return frame;
}

static GtkWidget *make_led (int nr)
{
    GtkWidget *subframe, *the_led, *thing;
    GdkColormap *colormap;

    the_led = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (the_led);

    thing = gtk_preview_new (GTK_PREVIEW_COLOR);
    gtk_box_pack_start (GTK_BOX (the_led), thing, TRUE, TRUE, 0);
    gtk_widget_show (thing);

    subframe = gtk_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (the_led), subframe, TRUE, TRUE, 0);
    gtk_widget_show (subframe);

    thing = gtk_drawing_area_new ();
    gtk_drawing_area_size (GTK_DRAWING_AREA (thing), 20, 5);
    gtk_widget_set_events (thing, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER (subframe), thing);
    colormap = gtk_widget_get_colormap (thing);
    led_on[nr].red = nr == 0 ? 0xEEEE : 0xCCCC;
    led_on[nr].green = nr == 0 ? 0: 0xFFFF;
    led_on[nr].blue = 0;
    led_on[nr].pixel = 0;
    led_off[nr].red = 0;
    led_off[nr].green = 0;
    led_off[nr].blue = 0;
    led_off[nr].pixel = 0;
    gdk_color_alloc (colormap, led_on + nr);
    gdk_color_alloc (colormap, led_off + nr);
    led_widgets[nr] = thing;
    gtk_signal_connect (GTK_OBJECT (thing), "event",
			(GtkSignalFunc) driveled_event, (gpointer) thing);
    gtk_widget_show (thing);

    thing = gtk_preview_new (GTK_PREVIEW_COLOR);
    gtk_box_pack_start (GTK_BOX (the_led), thing, TRUE, TRUE, 0);
    gtk_widget_show (thing);
    
    return the_led;
}

static GtkWidget *make_file_container (const char *title, GtkWidget *vbox)
{
    GtkWidget *thing = gtk_frame_new (title);
    GtkWidget *buttonbox = gtk_hbox_new (FALSE, 4);

    gtk_container_set_border_width (GTK_CONTAINER (buttonbox), 4);
    gtk_container_add (GTK_CONTAINER (thing), buttonbox);
    gtk_box_pack_start (GTK_BOX (vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show (buttonbox);
    gtk_widget_show (thing);

    return buttonbox;
}

static GtkWidget *make_file_widget (GtkWidget *buttonbox)
{
    GtkWidget *thing, *subthing;
    GtkWidget *subframe = gtk_frame_new (NULL);

    gtk_frame_set_shadow_type (GTK_FRAME (subframe), GTK_SHADOW_ETCHED_OUT);
    gtk_box_pack_start (GTK_BOX (buttonbox), subframe, TRUE, TRUE, 0);
    gtk_widget_show (subframe);
    subthing = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (subthing);
    gtk_container_add (GTK_CONTAINER (subframe), subthing);
    thing = gtk_label_new ("");
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (subthing), thing, TRUE, TRUE, 0);

    return thing;
}

static void make_floppy_disks (GtkWidget *vbox)
{
    GtkWidget *thing, *subthing, *subframe, *buttonbox;
    char buf[5];
    int i;

    add_empty_vbox (vbox);

    for (i = 0; i < 4; i++) {
	/* Frame with an hbox and the "DFx:" title */
	sprintf (buf, "DF%d:", i);
	buttonbox = make_file_container (buf, vbox);

	/* LED */
	subthing = make_led (i + 1);
	gtk_box_pack_start (GTK_BOX (buttonbox), subthing, FALSE, TRUE, 0);

	/* Current file display */
	disk_text_widget[i] = make_file_widget (buttonbox);

	/* Now, the buttons.  */
	thing = gtk_button_new_with_label ("Eject");
	gtk_box_pack_start (GTK_BOX (buttonbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);
	disk_eject_widget[i] = thing;
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) did_eject, (gpointer) i);

	thing = gtk_button_new_with_label ("Insert");
	gtk_box_pack_start (GTK_BOX (buttonbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);
	disk_insert_widget[i] = thing;
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) did_insert, (gpointer) i);
    }

    add_empty_vbox (vbox);
}

static GtkWidget *make_cpu_speed_sel (void)
{
    int t;
    static const char *labels[] = {
	"Optimize for host CPU speed","Approximate 68000/7MHz speed", "Adjustable",
	NULL
    };
    GtkWidget *frame, *newbox;

    frame = gtk_frame_new ("CPU speed");
    newbox = gtk_vbox_new (FALSE, 4);
    gtk_widget_show (newbox);
    gtk_container_set_border_width (GTK_CONTAINER (newbox), 4);
    gtk_container_add (GTK_CONTAINER (frame), newbox);
    make_radio_group (labels, newbox, cpuspeed_widgets, 0, 1, cpuspeed_changed, -1, NULL);

    t = currprefs.m68k_speed > 0 ? currprefs.m68k_speed : 4 * CYCLE_UNIT;
    cpuspeed_adj = GTK_ADJUSTMENT (gtk_adjustment_new (t, 1.0, 5120.0, 1.0, 1.0, 1.0));
    gtk_signal_connect (GTK_OBJECT (cpuspeed_adj), "value_changed",
			GTK_SIGNAL_FUNC (cpuspeed_changed), NULL);

    cpuspeed_scale = gtk_hscale_new (cpuspeed_adj);
    gtk_range_set_update_policy (GTK_RANGE (cpuspeed_scale), GTK_UPDATE_DELAYED);
    gtk_scale_set_digits (GTK_SCALE (cpuspeed_scale), 0);
    gtk_scale_set_value_pos (GTK_SCALE (cpuspeed_scale), GTK_POS_RIGHT);
    cpuspeed_scale = add_labelled_widget_centered ("Cycles per instruction:", cpuspeed_scale, newbox);

    return frame;
}

static void make_cpu_widgets (GtkWidget *vbox)
{
    int i;
    GtkWidget *newbox, *hbox, *frame;
    GtkWidget *thing;
    static const char *radiolabels[] = {
	"68000", "68010", "68020", "68020+68881", "68040",
	NULL
    };

    add_empty_vbox (vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    add_empty_vbox (hbox);

    newbox = make_radio_group_box ("CPU type", radiolabels, cpu_widget, 0, cputype_changed);
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, FALSE, 0);

    newbox = make_cpu_speed_sel ();
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, FALSE, 0);

    add_empty_vbox (hbox);
    gtk_widget_show (hbox); 
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    frame = gtk_frame_new ("CPU flags");
    add_centered_to_vbox (vbox, frame);
    gtk_widget_show (frame);
    newbox = gtk_vbox_new (FALSE, 4);
    gtk_widget_show (newbox);
    gtk_container_set_border_width (GTK_CONTAINER (newbox), 4);
    gtk_container_add (GTK_CONTAINER (frame), newbox);

    a24m_widget = gtk_check_button_new_with_label ("24 bit address space");
    add_centered_to_vbox (newbox, a24m_widget);
    gtk_widget_show (a24m_widget);
    ccpu_widget = gtk_check_button_new_with_label ("Slow but compatible");
    add_centered_to_vbox (newbox, ccpu_widget);
    gtk_widget_show (ccpu_widget);

    add_empty_vbox (vbox);

    gtk_signal_connect (GTK_OBJECT (ccpu_widget), "clicked",
			(GtkSignalFunc) cputype_changed, NULL);
    gtk_signal_connect (GTK_OBJECT (a24m_widget), "clicked",
			(GtkSignalFunc) cputype_changed, NULL);    
}

static void make_gfx_widgets (GtkWidget *vbox)
{
    GtkWidget *thing, *frame, *newbox, *hbox;
    static const char *p96labels[] = {
	"None", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB", "32 MB", NULL
    };

    add_empty_vbox (vbox);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_widget_show (hbox);
    add_centered_to_vbox (vbox, hbox);

    frame = make_radio_group_box_1 ("P96 RAM", p96labels, p96size_widget, 0, p96size_changed, 4);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

    frame = gtk_frame_new ("Miscellaneous");
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);

    gtk_widget_show (frame);
    newbox = gtk_vbox_new (FALSE, 4);
    gtk_widget_show (newbox);
    gtk_container_set_border_width (GTK_CONTAINER (newbox), 4);
    gtk_container_add (GTK_CONTAINER (frame), newbox);

    framerate_adj = GTK_ADJUSTMENT (gtk_adjustment_new (currprefs.gfx_framerate, 1.0, 21.0, 1.0, 1.0, 1.0));
    gtk_signal_connect (GTK_OBJECT (framerate_adj), "value_changed",
			GTK_SIGNAL_FUNC (custom_changed), NULL);

    thing = gtk_hscale_new (framerate_adj);
    gtk_range_set_update_policy (GTK_RANGE (thing), GTK_UPDATE_DELAYED);
    gtk_scale_set_digits (GTK_SCALE (thing), 0);
    gtk_scale_set_value_pos (GTK_SCALE (thing), GTK_POS_RIGHT);
    add_labelled_widget_centered ("Framerate:", thing, newbox);

    b32_widget = gtk_check_button_new_with_label ("32 bit blitter");
    add_centered_to_vbox (newbox, b32_widget);
#if 0
    gtk_widget_show (b32_widget);
#endif
    bimm_widget = gtk_check_button_new_with_label ("Immediate blits");
    add_centered_to_vbox (newbox, bimm_widget);
    gtk_widget_show (bimm_widget);

    afscr_widget = gtk_check_button_new_with_label ("Amiga modes fullscreen");
    add_centered_to_vbox (newbox, afscr_widget);
#if 0
    gtk_widget_show (afscr_widget);
#endif
    pfscr_widget = gtk_check_button_new_with_label ("Picasso modes fullscreen");
    add_centered_to_vbox (newbox, pfscr_widget);
#if 0
    gtk_widget_show (pfscr_widget);
#endif
    add_empty_vbox (vbox);

    gtk_signal_connect (GTK_OBJECT (bimm_widget), "clicked",
			(GtkSignalFunc) custom_changed, NULL);
#if 0
    gtk_signal_connect (GTK_OBJECT (b32_widget), "clicked",
			(GtkSignalFunc) custom_changed, NULL);
    gtk_signal_connect (GTK_OBJECT (afscr_widget), "clicked",
			(GtkSignalFunc) custom_changed, NULL);
    gtk_signal_connect (GTK_OBJECT (pfscr_widget), "clicked",
			(GtkSignalFunc) custom_changed, NULL);
#endif
}

static void make_chipset_widgets (GtkWidget *vbox)
{
    GtkWidget *frame, *newbox, *hbox;
    static const char *colllabels[] = {
	"None (fastest)", "Sprites only", "Sprites & playfields", "Full (very slow)",
	NULL
    };
    static const char *cslevellabels[] = {
	"OCS", "ECS Agnus", "Full ECS",
#ifdef AGA	 
	 "AGA",
#endif	 
	 NULL
    };

    add_empty_vbox (vbox);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_widget_show (hbox);
    add_centered_to_vbox (vbox, hbox);

    newbox = make_radio_group_box ("Sprite collisions", colllabels, coll_widget, 0, coll_changed);
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, TRUE, 0);

    newbox = make_radio_group_box ("Chipset", cslevellabels, cslevel_widget, 0, cslevel_changed);
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, TRUE, 0);

    fcop_widget = gtk_check_button_new_with_label ("Enable copper speedup code");
    add_centered_to_vbox (vbox, fcop_widget);
    gtk_widget_show (fcop_widget);

    gtk_signal_connect (GTK_OBJECT (fcop_widget), "clicked",
			(GtkSignalFunc) custom_changed, NULL);

    add_empty_vbox (vbox);
}

static void make_sound_widgets (GtkWidget *vbox)
{
    GtkWidget *frame, *newbox;
    int i;
    GtkWidget *hbox;
    static const char *soundlabels1[] = {
	"None", "No output", "Normal", "Accurate",
	NULL
    }, *soundlabels2[] = {
	"8 bit", "16 bit",
	NULL
    }, *soundlabels3[] = {
	"Mono", "Stereo", "Mixed",
	NULL
    };

    add_empty_vbox (vbox);

    newbox = make_radio_group_box ("Mode", soundlabels1, sound_widget, 1, sound_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_widget_show (hbox);
    add_centered_to_vbox (vbox, hbox);
    newbox = make_radio_group_box ("Channels", soundlabels3, sound_ch_widget, 1, sound_changed);
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, TRUE, 0);
    newbox = make_radio_group_box ("Resolution", soundlabels2, sound_bits_widget, 1, sound_changed);
    gtk_widget_show (newbox);
    gtk_box_pack_start (GTK_BOX (hbox), newbox, FALSE, TRUE, 0);

    add_empty_vbox (vbox);
}

static void make_mem_widgets (GtkWidget *vbox)
{
    GtkWidget *hbox = gtk_hbox_new (FALSE, 10);
    GtkWidget *label, *frame;

    static const char *chiplabels[] = {
	"512 KB", "1 MB", "2 MB", "4 MB", "8 MB", NULL
    };
    static const char *bogolabels[] = {
	"None", "512 KB", "1 MB", "1.8 MB", NULL
    };
    static const char *fastlabels[] = {
	"None", "1 MB", "2 MB", "4 MB", "8 MB", NULL
    };
    static const char *z3labels[] = {
	"None", "1 MB", "2 MB", "4 MB", "8 MB",
	"16 MB", "32 MB", "64 MB", "128 MB", "256 MB",
	NULL
    };

    add_empty_vbox (vbox);

    {
	GtkWidget *buttonbox = make_file_container ("Kickstart ROM file:", vbox);
	GtkWidget *thing = gtk_button_new_with_label ("Change");

	/* Current file display */
	rom_text_widget = make_file_widget (buttonbox);

	gtk_box_pack_start (GTK_BOX (buttonbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);
	rom_change_widget = thing;
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) did_romchange, 0);
    }

    {
	GtkWidget *buttonbox = make_file_container ("ROM key file for Cloanto Amiga Forever:", vbox);
	GtkWidget *thing = gtk_button_new_with_label ("Change");

	/* Current file display */
	key_text_widget = make_file_widget (buttonbox);

	gtk_box_pack_start (GTK_BOX (buttonbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);
	key_change_widget = thing;
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) did_keychange, 0);
    }

    gtk_widget_show (hbox);
    add_centered_to_vbox (vbox, hbox);

    add_empty_vbox (vbox);

    label = gtk_label_new ("These settings take effect after the next reset.");
    gtk_widget_show (label);
    add_centered_to_vbox (vbox, label);

    frame = make_radio_group_box ("Chip Mem", chiplabels, chipsize_widget, 0, chipsize_changed);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

    frame = make_radio_group_box ("Slow Mem", bogolabels, bogosize_widget, 0, bogosize_changed);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

    frame = make_radio_group_box ("Fast Mem", fastlabels, fastsize_widget, 0, fastsize_changed);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

    frame = make_radio_group_box_1 ("Z3 Mem", z3labels, z3size_widget, 0, z3size_changed, 5);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);
}

#ifdef JIT
static void make_comp_widgets (GtkWidget *vbox)
{
    GtkWidget *frame, *newbox;
    int i;
    GtkWidget *hbox;
    static const char *complabels1[] = {
	"Direct", "Indirect", "Indirect for KS", "Direct after Picasso",
	NULL
    },*complabels2[] = {
	"Direct", "Indirect", "Indirect for KS", "Direct after Picasso", 
	NULL
    },*complabels3[] = {
	"Direct", "Indirect", "Indirect for KS", "Direct after Picasso", 
	NULL
    },*complabels3a[] = {
	"Direct", "Indirect", "Indirect for KS", "Direct after Picasso", 
	NULL
    }, *complabels4[] = {
      "Always generate", "Only generate when needed",
	NULL
    }, *complabels5[] = {
      "Disable", "Enable",
	NULL
    }, *complabels6[] = {
      "Disable", "Enable",
	NULL
    }, *complabels7[] = {
      "Disable", "Enable",
	NULL
    }, *complabels8[] = {
      "Soft", "Hard",
	NULL
    }, *complabels9[] = {
      "Disable", "Enable", 
	NULL
    };
    GtkWidget *thing;

    add_empty_vbox (vbox);

    newbox = make_radio_group_box ("Byte access", complabels1, compbyte_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);
    newbox = make_radio_group_box ("Word access", complabels2, compword_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);
    newbox = make_radio_group_box ("Long access", complabels3, complong_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);
    newbox = make_radio_group_box ("Address lookup", complabels3a, compaddr_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

    newbox = make_radio_group_box ("Flags", complabels4, compnf_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

    newbox = make_radio_group_box ("Icache flushes", complabels8, comp_hardflush_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

    newbox = make_radio_group_box ("Compile through uncond branch", complabels9, comp_constjump_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

    newbox = make_radio_group_box ("JIT FPU compiler", complabels7, compfpu_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);

#if USE_OPTIMIZER
    newbox = make_radio_group_box ("Mid Level Optimizer", complabels5, comp_midopt_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);
#endif

#if USE_LOW_OPTIMIZER
    newbox = make_radio_group_box ("Low Level Optimizer", complabels6, comp_lowopt_widget, 1, comp_changed);
    gtk_widget_show (newbox);
    add_centered_to_vbox (vbox, newbox);
#endif

    cachesize_adj = GTK_ADJUSTMENT (gtk_adjustment_new (currprefs.cachesize, 0.0, 16384.0, 1.0, 1.0, 1.0));
    gtk_signal_connect (GTK_OBJECT (cachesize_adj), "value_changed",
			GTK_SIGNAL_FUNC (comp_changed), NULL);

    thing = gtk_hscale_new (cachesize_adj);
    gtk_range_set_update_policy (GTK_RANGE (thing), GTK_UPDATE_DELAYED);
    gtk_scale_set_digits (GTK_SCALE (thing), 0);
    gtk_scale_set_value_pos (GTK_SCALE (thing), GTK_POS_RIGHT);
    add_labelled_widget_centered ("Translation buffer(kB):", thing, vbox);

    add_empty_vbox (vbox);
}
#endif

static void make_joy_widgets (GtkWidget *dvbox)
{
    int i;
    GtkWidget *hbox = gtk_hbox_new (FALSE, 10);
    static const char *joylabels[] = {
	"Joystick 0", "Joystick 1", "Mouse", "Numeric pad",
	"Cursor keys/Right Ctrl", "T/F/H/B/Left Alt",
	NULL
    };

    add_empty_vbox (dvbox);
    gtk_widget_show (hbox);
    add_centered_to_vbox (dvbox, hbox);

    for (i = 0; i < 2; i++) {
	GtkWidget *vbox, *frame;
	GtkWidget *thing;
	char buffer[20];
	int j;

	sprintf (buffer, "Port %d", i);
	frame = make_radio_group_box (buffer, joylabels, joy_widget[i], 0, joy_changed);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);
    }

    add_empty_vbox (dvbox);
}

static int hd_change_mode;

static void newdir_ok (void)
{
    int n;
    int readonly = GTK_TOGGLE_BUTTON (readonly_widget)->active;
    int bootpri  = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (bootpri_widget));
    strcpy (dirdlg_devname, gtk_entry_get_text (GTK_ENTRY (devname_entry)));
    strcpy (dirdlg_volname, gtk_entry_get_text (GTK_ENTRY (volname_entry)));
    strcpy (dirdlg_path, gtk_entry_get_text (GTK_ENTRY (path_entry)));

    n = strlen (dirdlg_volname);
    /* Strip colons from the end.  */
    if (n > 0) {
	if (dirdlg_volname[n - 1] == ':')
	    dirdlg_volname[n - 1] = '\0';
    }
    /* Do device name too */
    n = strlen (dirdlg_devname);
    if (n > 0) {
        if (dirdlg_devname[n - 1] == ':')
	    dirdlg_devname[n - 1] = '\0';
    }
    if (strlen (dirdlg_volname) == 0 || strlen (dirdlg_path) == 0) {
	/* Uh, no messageboxes in gtk?  */
    } else if (hd_change_mode) {
	set_filesys_unit (currprefs.mountinfo, selected_hd_row, dirdlg_devname, dirdlg_volname, dirdlg_path,
			  readonly, 0, 0, 0, 0, bootpri, 0);
	set_hd_state ();
    } else {
	add_filesys_unit (currprefs.mountinfo, dirdlg_devname, dirdlg_volname, dirdlg_path,
			  readonly, 0, 0, 0, 0, 0, 0);
	set_hd_state ();
    }
    gtk_widget_destroy (dirdlg);
}


GtkWidget *path_selector;

static void did_dirdlg_done_select (GtkObject *o, gpointer entry )
{
    assert (GTK_IS_ENTRY (entry));

    gtk_entry_set_text (GTK_ENTRY (entry), gtk_file_selection_get_filename (GTK_FILE_SELECTION (path_selector)));
}

static void did_dirdlg_select (GtkObject *o, gpointer entry )
{
    assert( GTK_IS_ENTRY(entry) );
    path_selector = gtk_file_selection_new("Select a folder to mount");
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (path_selector), gtk_entry_get_text (GTK_ENTRY (entry)));
    gtk_window_set_modal (GTK_WINDOW (path_selector), TRUE);

    gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(path_selector)->ok_button),
                                          "clicked", GTK_SIGNAL_FUNC (did_dirdlg_done_select),
                                          (gpointer) entry);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(path_selector)->ok_button),
                                          "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                          (gpointer) path_selector);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(path_selector)->cancel_button),
                                          "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                          (gpointer) path_selector);

    /* Gtk1.2 doesn't have a directory chooser widget, so we fake one from the
     * file dialog, but hiding the widgets related to file selection */
    gtk_widget_hide ((GTK_FILE_SELECTION(path_selector)->file_list)->parent);
    gtk_widget_hide (GTK_FILE_SELECTION(path_selector)->fileop_del_file);
    gtk_widget_hide (GTK_FILE_SELECTION(path_selector)->fileop_ren_file);
    gtk_widget_hide (GTK_FILE_SELECTION(path_selector)->selection_entry);
    gtk_entry_set_text (GTK_ENTRY (GTK_FILE_SELECTION(path_selector)->selection_entry), "" );

    gtk_widget_show (path_selector);
}

void dirdlg_on_change (GtkObject *o, gpointer data)
{
  int can_complete = (strlen (gtk_entry_get_text (GTK_ENTRY(path_entry))) !=0)
                  && (strlen (gtk_entry_get_text (GTK_ENTRY(volname_entry))) != 0)
	          && (strlen (gtk_entry_get_text (GTK_ENTRY(devname_entry))) != 0);

  gtk_widget_set_sensitive (dirdlg_ok, can_complete);
}

void create_dirdlg (const char *title)
{
    GtkWidget *dialog_vbox, *dialog_hbox, *vbox, *frame, *table, *hbox, *thing, *label, *button;

    dirdlg = gtk_dialog_new ();

    gtk_window_set_title (GTK_WINDOW (dirdlg), title);
    gtk_window_set_position (GTK_WINDOW (dirdlg), GTK_WIN_POS_MOUSE);
    gtk_window_set_modal (GTK_WINDOW (dirdlg), TRUE);
    gtk_widget_show (dirdlg);

    dialog_vbox = GTK_DIALOG (dirdlg)->vbox;
    gtk_widget_show (dialog_vbox);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, FALSE, 0);

    frame = gtk_frame_new ("Mount host folder");
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

    hbox = gtk_hbox_new (FALSE, 4);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);

    label  = gtk_label_new ("Path");
//    gtk_label_set_pattern (GTK_LABEL (label), "_");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    thing = gtk_entry_new_with_max_length (255);
    gtk_signal_connect (GTK_OBJECT (thing), "changed", (GtkSignalFunc) dirdlg_on_change, (gpointer) NULL);
    gtk_box_pack_start (GTK_BOX (hbox), thing, TRUE, TRUE, 0);
    gtk_widget_show (thing);
    path_entry = thing;

    button = gtk_button_new_with_label ("Select...");
    gtk_signal_connect (GTK_OBJECT (button), "clicked", (GtkSignalFunc) did_dirdlg_select, (gpointer) path_entry);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    frame = gtk_frame_new ("As Amiga disk");
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

    table = gtk_table_new (3, 4, FALSE);
    gtk_widget_show (table);
    gtk_container_add (GTK_CONTAINER (frame), table);
    gtk_container_set_border_width (GTK_CONTAINER (table), 8);
    gtk_table_set_row_spacings (GTK_TABLE (table), 4);
    gtk_table_set_col_spacings (GTK_TABLE (table), 4);

        label = gtk_label_new ("Device name");
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_show (label);
        thing = gtk_entry_new_with_max_length (255);
	gtk_signal_connect (GTK_OBJECT (thing), "changed", (GtkSignalFunc) dirdlg_on_change, (gpointer) NULL);
        gtk_widget_show (thing);
        gtk_table_attach (GTK_TABLE (table), thing, 1, 2, 0, 1,
                        (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_set_usize (thing, 200, -1);
        devname_entry = thing;

        label = gtk_label_new ("Volume name");
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_show (label);
        thing = gtk_entry_new_with_max_length (255);
	gtk_signal_connect (GTK_OBJECT (thing), "changed", (GtkSignalFunc) dirdlg_on_change, (gpointer) NULL);
        gtk_table_attach (GTK_TABLE (table), thing, 1, 2, 1, 2,
                        (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_show (thing);
        gtk_widget_set_usize (thing, 200, -1);
        volname_entry = thing;

        label = gtk_label_new ("Boot priority");
        gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 2,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_show (label);
        thing = gtk_spin_button_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, -128, 127, 1, 5, 5)), 1, 0);
        gtk_table_attach (GTK_TABLE (table), thing, 3, 4, 0, 2,
                        (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                        (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
        gtk_widget_show (thing);
        bootpri_widget = thing;

        readonly_widget = gtk_check_button_new_with_label ("Read only");
        gtk_table_attach (GTK_TABLE (table), readonly_widget, 0, 4, 2, 3,
                        (GtkAttachOptions) (GTK_EXPAND),
                        (GtkAttachOptions) (0), 0, 0);
        gtk_widget_show (readonly_widget);

    dialog_hbox = GTK_DIALOG (dirdlg)->action_area;

    hbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (dialog_hbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show (hbox);

    button = gtk_button_new_with_label ("OK");
    gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC(newdir_ok), NULL);
//    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
//    gtk_widget_grab_default (button);
    gtk_widget_show (button);
    dirdlg_ok = button;

    button = gtk_button_new_with_label ("Cancel");
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			       GTK_SIGNAL_FUNC (gtk_widget_destroy),
			       GTK_OBJECT (dirdlg));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);
}

static void did_newdir (void)
{
    hd_change_mode = 0;
    create_dirdlg ("Add a hard disk");
}
static void did_newhdf (void)
{
    hd_change_mode = 0;
}

static void did_hdchange (void)
{
    int secspertrack, surfaces, reserved, blocksize, bootpri;
    uae_u64 size;
    int cylinders, readonly;
    char *devname, *volname, *rootdir, *filesysdir;
    char *failure;

    failure = get_filesys_unit (currprefs.mountinfo, selected_hd_row,
				&devname, &volname, &rootdir, &readonly,
				&secspertrack, &surfaces, &reserved,
				&cylinders, &size, &blocksize, &bootpri, &filesysdir);

    hd_change_mode = 1;
    if (is_hardfile (currprefs.mountinfo, selected_hd_row)) {
    } else {
	create_dirdlg ("Hard disk properties");
        gtk_entry_set_text (GTK_ENTRY (devname_entry), devname);
	gtk_entry_set_text (GTK_ENTRY (volname_entry), volname);
	gtk_entry_set_text (GTK_ENTRY (path_entry), rootdir);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (readonly_widget), readonly);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (bootpri_widget), bootpri);
   }
}
static void did_hddel (void)
{
    kill_filesys_unit (currprefs.mountinfo, selected_hd_row);
    set_hd_state ();
}

static void hdselect (GtkWidget *widget, gint row, gint column, GdkEventButton *bevent,
		      gpointer user_data)
{
    selected_hd_row = row;
    gtk_widget_set_sensitive (hdchange_button, TRUE);
    gtk_widget_set_sensitive (hddel_button, TRUE);
}

static void hdunselect (GtkWidget *widget, gint row, gint column, GdkEventButton *bevent,
			gpointer user_data)
{
    gtk_widget_set_sensitive (hdchange_button, FALSE);
    gtk_widget_set_sensitive (hddel_button, FALSE);
}


static GtkWidget *make_buttons (const char *label, GtkWidget *box, void (*sigfunc) (void), GtkWidget *(*create)(const char *label))
{
    GtkWidget *thing = create (label);
    gtk_widget_show (thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) sigfunc, NULL);
    gtk_box_pack_start (GTK_BOX (box), thing, TRUE, TRUE, 0);

    return thing;
}
#define make_button(label, box, sigfunc) make_buttons(label, box, sigfunc, gtk_button_new_with_label)

static void make_hd_widgets (GtkWidget *dvbox)
{
    GtkWidget *frame, *vbox, *scrollbox, *thing, *buttonbox, *hbox;
//    char *titles [] = {
//	"Volume", "File/Directory", "R/O", "Heads", "Cyl.", "Sec.", "Rsrvd", "Size", "Blksize"
//    };

    frame = gtk_frame_new (NULL);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (dvbox), frame, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    scrollbox = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (scrollbox);
    gtk_box_pack_start (GTK_BOX (vbox), scrollbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scrollbox), 8);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollbox), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    thing = gtk_clist_new_with_titles (HDLIST_MAX_COLS, hdlist_col_titles );
    gtk_clist_set_selection_mode (GTK_CLIST (thing), GTK_SELECTION_SINGLE);
    gtk_signal_connect (GTK_OBJECT (thing), "select_row", (GtkSignalFunc) hdselect, NULL);
    gtk_signal_connect (GTK_OBJECT (thing), "unselect_row", (GtkSignalFunc) hdunselect, NULL);
    hdlist_widget = thing;
    gtk_widget_set_usize (thing, -1, 200);
    gtk_widget_show (thing);
    gtk_container_add (GTK_CONTAINER (scrollbox), thing);

    /* The buttons */
    buttonbox = gtk_hbutton_box_new ();
    gtk_widget_show (buttonbox);
    gtk_box_pack_start (GTK_BOX (vbox), buttonbox, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (buttonbox), 8);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox), GTK_BUTTONBOX_SPREAD);

    make_button ("Add...", buttonbox, did_newdir);
#if 0 /* later... */
    make_button ("New hardfile...", buttonbox, did_newhdf);
#endif
    hdchange_button = make_button ("Properties...", buttonbox, did_hdchange);
    hddel_button = make_button ("Remove", buttonbox, did_hddel);

    thing = gtk_label_new ("These settings take effect after the next reset.");
    gtk_widget_show (thing);
    add_centered_to_vbox (vbox, thing);
}
 
static void make_about_widgets (GtkWidget *dvbox)
{
    GtkWidget *thing;
    GtkStyle *style;
    GdkFont *font;
    char t[20];

    add_empty_vbox (dvbox);

    sprintf (t, "UAE %d.%d.%d", UAEMAJOR, UAEMINOR, UAESUBREV);
    thing = gtk_label_new (t);
    gtk_widget_show (thing);
    add_centered_to_vbox (dvbox, thing);

    font = gdk_font_load ("-*-helvetica-medium-r-normal--*-240-*-*-*-*-*-*");
    if (font) {
	style = gtk_style_copy (GTK_WIDGET (thing)->style);
	gdk_font_unref (style->font);
	style->font = font;
	gdk_font_ref (style->font);
	/* gtk_widget_push_style (style); Don't need this - Rich */
	gtk_widget_set_style (thing, style);
    }
    thing = gtk_label_new ("Choose your settings, then deselect the Pause button to start!");
    gtk_widget_show (thing);
    add_centered_to_vbox (dvbox, thing);

    add_empty_vbox (dvbox); 
}

static gint did_guidlg_delete (GtkWidget* window, GdkEventAny* e, gpointer data)
{
    if (!no_gui && !quit_gui) 
        write_comm_pipe_int (&from_gui_pipe, 4, 1);
    return TRUE;
}

static void create_guidlg (void)
{
    GtkWidget *window, *notebook;
    GtkWidget *buttonbox, *vbox, *hbox;
    GtkWidget *thing;
    unsigned int i;
//    int argc = 1;
//    char *a[] = {"UAE"};
//    char **argv = a;
    static const struct _pages {
	const char *title;
	void (*createfunc)(GtkWidget *);
    } pages[] = {
	{ "Floppy disks", make_floppy_disks },
	{ "Memory", make_mem_widgets },
	{ "CPU emulation", make_cpu_widgets },
	{ "Graphics", make_gfx_widgets },
	{ "Chipset", make_chipset_widgets },
	{ "Sound", make_sound_widgets },
#ifdef JIT       
 	{ "JIT", make_comp_widgets },
#endif       
	{ "Game ports", make_joy_widgets },
	{ "Hard disks", make_hd_widgets },
	{ "About", make_about_widgets }
    };

    DEBUG_LOG ("Entered\n");
//    gtk_init (&argc, &argv);
//    gtk_rc_parse ("uaegtkrc");

    gui_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (gui_window), "UAE control");
    gtk_signal_connect (GTK_OBJECT(gui_window), "delete_event", GTK_SIGNAL_FUNC(did_guidlg_delete), NULL);
   
    vbox = gtk_vbox_new (FALSE, 4);
    gtk_container_add (GTK_CONTAINER (gui_window), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (gui_window), 10);

    /* First line - buttons and power LED */
    hbox = gtk_hbox_new (FALSE, 10);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    /* The buttons */
    buttonbox = gtk_hbox_new (TRUE, 4);
    gtk_widget_show (buttonbox);
    gtk_box_pack_start (GTK_BOX (hbox), buttonbox, TRUE, TRUE, 0);
    make_button ("Reset", buttonbox, did_reset);
    make_button ("Debug", buttonbox, did_debug);
    make_button ("Quit", buttonbox, did_quit);
    make_button ("Save config", buttonbox, save_config);
    pause_uae_widget = make_buttons ("Pause", buttonbox, (void (*) (void))pause_uae, gtk_toggle_button_new_with_label);

    /* The LED */
    thing = make_led (0);
    thing = make_labelled_widget ("Power:", thing);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, TRUE, 0);

    /* More buttons */
    buttonbox = gtk_hbox_new (TRUE, 4);
    gtk_widget_show (buttonbox);
    gtk_box_pack_start (GTK_BOX (vbox), buttonbox, FALSE, FALSE, 0);
    snap_save_widget = make_button ("Save state", buttonbox, did_savestate);
    snap_load_widget = make_button ("Load state", buttonbox, did_loadstate);
    gtk_widget_set_sensitive (snap_save_widget, FALSE); // temporarily disable these, because
    gtk_widget_set_sensitive (snap_load_widget, FALSE); // snapshots aren't working yet - Rich

    /* Place a separator below those buttons.  */
    thing = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show (thing);

    /* Now the notebook */
    notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
    gtk_widget_show (notebook);

    for (i = 0; i < sizeof pages / sizeof (struct _pages); i++) {
	thing = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (thing);
	gtk_container_set_border_width (GTK_CONTAINER (thing), 10);
	pages[i].createfunc (thing);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), thing, gtk_label_new (pages[i].title));
    }

    /* Put "about" screen first.  */
    gtk_notebook_set_page (GTK_NOTEBOOK (notebook), i - 1);
    enable_disk_buttons (1);
    enable_snap_buttons (1);

    gtk_widget_show (vbox);

    filesel_active = -1;
    snapsel_active = -1;

//    gtk_timeout_add (1000, (GtkFunction)my_idle, 0);
}

/*
 * gtk_gui_thread()
 * 
 * This is launched as a separate thread to the main UAE thread
 * to create and handle the GUI. After the GUI has been set up, 
 * this calls the standard GTK+ event processing loop. 
 *
 */
static void *gtk_gui_thread (void *dummy)
{
    /* fake args for gtk_init() */
    int argc = 1;
    char *a[] = {"UAE"};
    char **argv = a;
   
    DEBUG_LOG ("Started\n");

    gui_active = 0;
   
    gtk_init (&argc, &argv);
    DEBUG_LOG ("gtk_init() called\n");
    gtk_rc_parse ("uaegtkrc");
   
    create_guidlg ();
    DEBUG_LOG ("GUI created\n") ;

    /* Add callback to GTK+ mainloop to handle messages from UAE */
    gtk_timeout_add (1000, (GtkFunction)my_idle, 0);   

    /* We're ready - tell the world */
    uae_sem_post (&gui_init_sem);
//    gui_active = 1; doesn't work here. Need to wait for gui_update()
   
    /* Enter GTK+ main loop */
    DEBUG_LOG ("Entering GTK+ main loop\n");
    gtk_main ();

    /* Main loop has exited, so the GUI will quit */
    quitted_gui = 1;
    uae_sem_post (&gui_quit_sem);
    DEBUG_LOG ("Exiting\n");
    return 0;
}

void gui_changesettings(void)
{
    
}

void gui_fps (int x)
{
}

/*
 * gui_led()
 * 
 * Called from the main UAE thread to inform the GUI
 * of disk activity so that indicator LEDs may be refreshed.
 *
 * We don't respond to this, since our LEDs are updated
 * periodically by my_idle()
 */
void gui_led (int num, int on)
{
/*    if (no_gui)
	return;

    if (num < 1 || num > 4)
	return;
    printf("LED %d %d\n", num, on);
    write_comm_pipe_int (&to_gui_pipe, 1, 0);
    write_comm_pipe_int (&to_gui_pipe, num == 0 ? 4 : num - 1, 0);
    write_comm_pipe_int (&to_gui_pipe, on, 1);
    printf("#LED %d %d\n", num, on);*/
}


/*
 * gui_filename() 
 * 
 * This is called from the main UAE thread to inform
 * the GUI that a floppy disk has been inserted or ejected.
 */
void gui_filename (int num, const char *name)
{
    DEBUG_LOG ("Entered with drive:%d name:%s\n", num, name);

    if (!no_gui) {
        write_comm_pipe_int (&to_gui_pipe, GUICMD_DISKCHANGE, 0);
        write_comm_pipe_int (&to_gui_pipe, num, 1);
    }
    return;    
}


/*
 * gui_handle_events()
 * 
 * This is called from the main UAE thread to handle the
 * processing of GUI-related events sent from the GUI thread.
 * 
 * If the UAE emulation proper is not running yet or is paused,
 * this loops continuously waiting for and responding to events
 * until the emulation is started or resumed, respectively. When
 * the emulation is running, this is called periodically from
 * the main UAE event loop.
 */
void gui_handle_events (void)
{
    if (no_gui)
	return;

    do {
        while (is_uae_paused() || comm_pipe_has_data (&from_gui_pipe)) {
            int cmd = read_comm_pipe_int_blocking (&from_gui_pipe);
            int n;
            DEBUG_LOG ("Got event %d from GUI\n", cmd);
            switch (cmd) {
	    case 0:
		n = read_comm_pipe_int_blocking (&from_gui_pipe);
	        uae_sem_wait (&gui_sem);
		changed_prefs.df[n][0] = '\0';
	        uae_sem_post (&gui_sem);
	        if (is_uae_paused()) {
		    /* When UAE is running it will notify the GUI when a disk has been inserted
		     * or removed itself. When UAE is paused, however, we need to do this ourselves
		     * or the change won't be realized in the GUI until UAE is resumed */
                    write_comm_pipe_int (&to_gui_pipe, GUICMD_DISKCHANGE, 0);
	            write_comm_pipe_int (&to_gui_pipe, n, 1);
		}
		break;
	    case 1:
		n = read_comm_pipe_int_blocking (&from_gui_pipe);
		uae_sem_wait (&gui_sem);
		strncpy (changed_prefs.df[n], new_disk_string[n], 255);
		free (new_disk_string[n]);
		new_disk_string[n] = 0;
		changed_prefs.df[n][255] = '\0';	        
		uae_sem_post (&gui_sem);
	        if (is_uae_paused()) {
		    /* When UAE is running it will notify the GUI when a disk has been inserted
		     * or removed itself. When UAE is paused, however, we need to do this ourselves
		     * or the change won't be realized in the GUI until UAE is resumed */
                    write_comm_pipe_int (&to_gui_pipe, GUICMD_DISKCHANGE, 0);
	            write_comm_pipe_int (&to_gui_pipe, n, 1);
		}
		break;
	    case 2:
	        uae_reset (0);
	        gui_set_paused( FALSE );
		break;
	    case 3:
		activate_debugger ();
	        gui_set_paused( FALSE );
		break;
	    case 4:
		uae_quit ();
	        gui_set_paused( FALSE );
		break;
	    case 5:
		set_uae_paused( TRUE );
		break;
	    case 6:
		set_uae_paused( FALSE );
		break;
	    case 7:
		printf ("STATESAVE\n");
		savestate_state = read_comm_pipe_int_blocking (&from_gui_pipe);
		uae_sem_wait (&gui_sem);
//		savestate_filename = gui_snapname;
		uae_sem_post (&gui_sem);
		break;
	    case 8:
		uae_sem_wait (&gui_sem);
		strncpy (changed_prefs.romfile, gui_romname, 255);
		changed_prefs.romfile[255] = '\0';
		free (gui_romname);
		uae_sem_post (&gui_sem);
		break;
	    case 9:
		uae_sem_wait (&gui_sem);
		strncpy (changed_prefs.keyfile, gui_keyname, 255);
		changed_prefs.keyfile[255] = '\0';
		free (gui_keyname);
		uae_sem_post (&gui_sem);
		break;
            }
        }
    } while (is_uae_paused());
}

void gui_update_gfx (void)
{
#if 0 /* This doesn't work... */
    set_gfx_state ();
#endif
}

/*
 * gui_init()
 * 
 * This is call from the main UAE thread to initialize the GUI.
 * It spawns the gtk_gui_thread to handle GUI creation and the
 * GTK+ main loop.
 */
int gui_init (void)
{
    uae_thread_id tid;

    DEBUG_LOG( "Entered\n" );
   
    init_comm_pipe (&to_gui_pipe, 20, 1);
    init_comm_pipe (&from_gui_pipe, 20, 1);
    uae_sem_init (&gui_sem, 0, 1);          // Unlock mutex on prefs settings
    uae_sem_init (&gui_update_sem, 0, 0);
    uae_sem_init (&gui_init_sem, 0, 0);
    uae_sem_init (&gui_quit_sem, 0, 0);

    /* Start GUI thread to construct GUI */
    uae_start_thread (gtk_gui_thread, NULL, &tid);

    /* Wait till GUI is ready */
    DEBUG_LOG ("Waiting for GUI\n");
    uae_sem_wait (&gui_init_sem);

    /* Tell it to refresh with current prefs settings */
    gui_update ();
   
    if (currprefs.start_gui == 1) {
        gui_set_paused( TRUE );
	/* Handle events until Pause is unchecked.  */
	gui_handle_events ();
	/* Quit requested?  */
	if (quit_program == -1) {
	    gui_exit ();
	    return -2;
	}
    }

    return 1;
}


/*
 * gui_update()
 * 
 * This is called from the main UAE thread to tell the GUI to update itself
 * using the current state of currprefs. This function will block
 * until it receives a message from the GUI telling it that the update
 * is complete.
 */
int gui_update (void)
{
    DEBUG_LOG( "Entered\n" );

    if (!no_gui) {
        write_comm_pipe_int (&to_gui_pipe, GUICMD_UPDATE, 1);
        uae_sem_wait (&gui_update_sem);
    }
    return 0;
}


/*
 * gui_exit()
 * 
 * This called from the main UAE thread to tell the GUI to gracefully
 * quit. It does this via the global variable guit_gui, which is checked and
 * responded to in the GTK main loop courtesy of our callback my_idle().
 * This function waits for a response from the GUI telling it that it
 * is acutally quitting.
 * 
 * TODO: Should change communication via a global, to sending a GUICMD_QUIT
 * message to the GUI?
 */
void gui_exit (void)
{
    DEBUG_LOG( "Entered\n" );

    if (!no_gui && !quit_gui) {
        quit_gui = 1;
        DEBUG_LOG( "Waiting for GUI thread to quit.\n" );
        uae_sem_wait (&gui_quit_sem); 
    }
}

void gui_lock (void)
{
    uae_sem_wait (&gui_sem);
}

void gui_unlock (void)
{
    uae_sem_post (&gui_sem);
}

void gui_hd_led (int led)
{
   
       static int resetcounter;
   
       int old = gui_data.hd;
       if (led == 0) 
     {
	
	        resetcounter--;
	        if (resetcounter > 0)
	              return;
     }
   
       gui_data.hd = led;
       resetcounter = 6;
       if (old != gui_data.hd)
             gui_led (5, gui_data.hd);
}

void gui_cd_led (int led)
{
}

/* An ugly stop-gap - needs proper message box implementation - Rich */
void gui_message (const char *format,...)
{
    char msg[2048];
    va_list parms;
    
    va_start (parms,format);
    vsprintf ( msg, format, parms);
    va_end (parms);
    
    if (!no_gui)
        do_message_box (NULL, msg, TRUE, TRUE);

    write_log (msg);
}


void gui_set_paused( int state )
{
   if (no_gui)
       return;

   write_comm_pipe_int (&to_gui_pipe, state==TRUE ? GUICMD_PAUSE : GUICMD_UNPAUSE, 1);
   set_uae_paused (state);
   return;
}



/*
 * do_message_box()
 * 
 * This makes up for GTK's lack of a function for creating simple message dialogs.
 * It can be called from any context. gui_init() must have been called at some point
 * previously.
 * 
 * title   - will be displayed in the dialog's titlebar (or NULL for default)
 * message - the message itself
 * modal   - should the dialog block input to the rest of the GUI
 * wait    - should the dialog wait until the user has acknowledged it
 */
static void do_message_box( const guchar *title, const guchar *message, gboolean modal, gboolean wait )
{
    uae_sem_t msg_quit_sem;

    // If we a need reply, then this semaphore which will be used
    // to signal us when the dialog has been exited.
    uae_sem_init (&msg_quit_sem, 0, 0);
   
    write_comm_pipe_int (&to_gui_pipe, GUICMD_MSGBOX, 0);
    write_comm_pipe_int (&to_gui_pipe, (int) title, 0);
    write_comm_pipe_int (&to_gui_pipe, (int) message, 0);
    write_comm_pipe_int (&to_gui_pipe, (int) modal, 0);    
    write_comm_pipe_int (&to_gui_pipe, (int) wait?&msg_quit_sem:NULL, 1);
   
    if (wait)
        uae_sem_wait (&msg_quit_sem);
   
    return;
}

/*
 * handle_message_box_request()
 * 
 * This is called from the GUI's context in repsonse to do_message_box() 
 * to actually create the dialog box
 */
static void handle_message_box_request (smp_comm_pipe *msg_pipe) 
{
    const guchar *title     = (const guchar *) read_comm_pipe_int_blocking (msg_pipe);
    const guchar *msg       = (const guchar *) read_comm_pipe_int_blocking (msg_pipe);
    int modal               =                  read_comm_pipe_int_blocking (msg_pipe);
    uae_sem_t *msg_quit_sem = (uae_sem_t *)    read_comm_pipe_int_blocking (msg_pipe);

    GtkWidget *dialog = make_message_box (title, msg, modal, msg_quit_sem);
}

/*
 * on_message_box_quit()
 * 
 * Handler called when message box is exited. Signals anybody that cares
 * via the semaphore it is supplied.
 */
void on_message_box_quit (GtkWidget *w, gpointer user_data)
{
     uae_sem_post ((uae_sem_t *)user_data);   
}

/*
 * make_message_box()
 * 
 * This does the actual work of constructing the message dialog.
 * 
 * title   - displayed in the dialog's titlebar
 * message - the message itself
 * modal   - whether the dialog should block input to the rest of the GUI
 * sem     - semaphore used for signalling that the dialog's finished
 * 
 * TODO: Make that semaphore go away. We shouldn't need to know about it here.
 */
static GtkWidget *make_message_box( const guchar *title, const guchar *message, int modal, uae_sem_t *sem )
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *hseparator;
    GtkWidget *hbuttonbox;
    GtkWidget *button;
    guint      key;
    GtkAccelGroup *accel_group;
   
    accel_group = gtk_accel_group_new ();
   
    dialog = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);   
    if (title==NULL || (title!=NULL && strlen(title)==0))
        title = "UAE information";   
    gtk_window_set_title (GTK_WINDOW (dialog), title);
    gtk_window_set_modal (GTK_WINDOW (dialog), modal);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
   
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_container_add (GTK_CONTAINER (dialog), vbox);
   
    label = gtk_label_new (message); 
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
   
    hseparator = gtk_hseparator_new ();
    gtk_widget_show (hseparator);
    gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 8);
   
    hbuttonbox = gtk_hbutton_box_new ();
    gtk_widget_show (hbuttonbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox), 4);

    button = gtk_button_new_with_label (NULL);
    key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (button)->child), "_Okay");
    gtk_widget_add_accelerator (button, "clicked", accel_group,key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (hbuttonbox), button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

    if (sem)			                            
        gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (on_message_box_quit), sem);   
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			                           GTK_SIGNAL_FUNC (gtk_widget_destroy),
			                           GTK_OBJECT (dialog));
   
    gtk_widget_grab_default (button);
    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);
    gtk_widget_show( dialog );

    return dialog;
}
