 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Interface to the Cocoa Mac OS X GUI
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2004 Steven J. Saunders
  */
#include <stdarg.h>

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "gui.h"
#include "inputdevice.h"

/* These prototypes aren't declared in the sdlgfx header for some reason */
extern void toggle_fullscreen (void);
extern int is_fullscreen (void);

#import <Cocoa/Cocoa.h>

/* Prototypes */
int ensureNotFullscreen (void);

/*
 * Revert to windowed mode if in fullscreen mode. Returns 1 if the
 * mode was initially fullscreen and was successfully changed. 0 otherwise.
 */
int ensureNotFullscreen (void)
{
    if (is_fullscreen ()) {
	toggle_fullscreen ();
	if (is_fullscreen ()) {
	    write_log ("Cannot activate GUI in full-screen mode\n");
	    return 0;
	} else return 1;
    } else return 0;
}

static void sigchldhandler (int foo)
{
}

int gui_init (void)
{
    return -1;
}

int gui_update (void)
{
    return 0;
}

void gui_exit (void)
{
}

void gui_fps (int fps, int idle)
{
    gui_data.fps  = fps;
    gui_data.idle = idle;
}

void gui_led (int led, int on)
{
}

void gui_hd_led (int led)
{
    static int resetcounter;

    int old = gui_data.hd;

    if (led == 0) {
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
    static int resetcounter;

    int old = gui_data.cd;
    if (led == 0) {
	resetcounter--;
	if (resetcounter > 0)
	    return;
    }

    gui_data.cd = led;
    resetcounter = 6;
    if (old != gui_data.cd)
	gui_led (6, gui_data.cd);
}

void gui_filename (int num, const char *name)
{
}

static void getline (char *p)
{
}

void gui_handle_events (void)
{
}

void gui_changesettings (void)
{
}

void gui_update_gfx (void)
{
}

void gui_lock (void)
{
}

void gui_unlock (void)
{
}

void gui_display (int shortcut)
{
    int result;
    int originallyFullscreen = ensureNotFullscreen ();

    if ((shortcut >= 0) && (shortcut < 4)) {
	NSArray *fileTypes = [NSArray arrayWithObjects:@"adf", @"adz",
	    @"zip", @"dms", @"fdi", nil];
	NSOpenPanel *oPanel = [NSOpenPanel openPanel];
	[oPanel setTitle:[NSString stringWithFormat:
	    @"Select a disk image file for DF%d", shortcut]];

	result = [oPanel runModalForDirectory:nil file:nil
	    types:fileTypes];

	if (result == NSOKButton) {
	    NSArray *files = [oPanel filenames];
	    NSString *file = [files objectAtIndex:0];
	    strncpy (changed_prefs.df[shortcut], [file lossyCString], 255);
	    changed_prefs.df[shortcut][255] = '\0';
	}
    }

    if (originallyFullscreen)
	toggle_fullscreen ();
}

void gui_message (const char *format,...)
{
    char msg[2048];
    va_list parms;

    int originallyFullscreen = ensureNotFullscreen ();

    va_start (parms,format);
    vsprintf (msg, format, parms);
    va_end (parms);

    NSRunAlertPanel(nil, [NSString stringWithCString:msg],
	@"OK", NULL, NULL);

    write_log (msg);
    if (originallyFullscreen)
	toggle_fullscreen ();
}
