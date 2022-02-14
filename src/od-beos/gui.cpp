 /*
  * UAE - The Un*x Amiga Emulator
  *
  * BeOS UI - or the beginnings of one
  *
  * Copyright 2004 Richard Drummond
  */

extern "C" {
#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "gui.h"
#include "disk.h"
}

#include <AppKit.h>
#include <InterfaceKit.h>
#include <storage/FilePanel.h>
#include <storage/Path.h>

const uint32 MSG_FLOPPY_PANEL_DRIVE0 = 'flp0';
const uint32 MSG_FLOPPY_PANEL_DRIVE1 = 'flp1';
const uint32 MSG_FLOPPY_PANEL_DRIVE2 = 'flp2';
const uint32 MSG_FLOPPY_PANEL_DRIVE3 = 'flp3';

class floppyPanelHandler: public BHandler {
    void MessageReceived(BMessage *msg) {
	printf("got message: %08x\n", msg->what);
	switch (msg->what) {
	    case MSG_FLOPPY_PANEL_DRIVE0:
	    case MSG_FLOPPY_PANEL_DRIVE1:
	    case MSG_FLOPPY_PANEL_DRIVE2:
	    case MSG_FLOPPY_PANEL_DRIVE3: {
	    	int drive = msg->what - MSG_FLOPPY_PANEL_DRIVE0;
		entry_ref ref;
		BEntry entry;
		printf ("Insert in drive %d\n", drive);
		if (msg->FindRef ("refs", &ref) == B_NO_ERROR)
		    if (entry.SetTo (&ref) == B_NO_ERROR) {
			BPath path;
		  	entry.GetPath (&path); printf ("path: %s\n", path.Path());
		  	strcpy (changed_prefs.df[drive], path.Path ());
//		        disk_insert (drive, path.Path());
		    }
		break;
    	    }
    	    default:
                BHandler::MessageReceived (msg);
    	}
    }
};

static class floppyPanelHandler *floppy_handler;

static void do_insert_floppy (int drive)
{
    char title[80];
    BFilePanel *panel;
    BMessage msg = BMessage (MSG_FLOPPY_PANEL_DRIVE0 + drive);
    BEntry dir   = BEntry(currprefs.df[drive]);
    dir.GetParent (&dir);

    sprintf (title, "UAE: Select image to insert in drive DF%d:", drive);

    if (floppy_handler == NULL) {
	floppy_handler = new floppyPanelHandler ();
        be_app->Lock ();
        be_app->AddHandler (floppy_handler);
        be_app->Unlock ();
    }
    panel = new BFilePanel (B_OPEN_PANEL, new BMessenger (floppy_handler),
    			    NULL, 0, false, &msg, 0, true, true);

    panel->SetPanelDirectory (&dir);
    panel->Window ()->SetTitle (title);
    panel->Window ()->Show ();
}


void gui_changesettings (void)
{
}

int gui_init (void)
{
    return 0;
}

void gui_exit (void)
{
}

int gui_update (void)
{
    return 0;
}

void gui_filename (int num, const char *name)
{
}

void gui_handle_events (void)
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
   if (shortcut >=0 && shortcut < 4)
	do_insert_floppy (shortcut);
}

void gui_message (const char *format,...)
{
    char msg[2048];
    va_list parms;
    BAlert *alert;

    va_start (parms,format);
    vsprintf ( msg, format, parms);
    va_end (parms);
    
    write_log (msg);

    alert = new BAlert ("UAE Information", msg, "Okay", NULL, NULL,
	B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
    alert->Go(); 
}
