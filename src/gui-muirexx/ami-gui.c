 /*
  * UAE - The Un*x Amiga Emulator
  *
  * GUI interface (to be done).
  * Calls AREXX interface.
  *
  * Copyright 1996 Bernd Schmidt, Samuel Devulder
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "gui.h"
#include "disk.h"

#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <dos/dosextens.h>

/****************************************************************************/

extern int  rexx_init (void);
extern void rexx_exit (void);
extern void rexx_led (int led, int on);          /* ami-rexx.c */
extern void rexx_filename (int num, const char *name);
extern void rexx_handle_events (void);
extern void main_window_led (int led, int on);   /* ami-win.c */

/****************************************************************************/

extern struct AslIFace *IAsl;

static void do_disk_insert (int drive)
{
    struct FileRequester *FileRequest;
    unsigned char	  buff[80];
    unsigned char	  path[512];
    static unsigned char *last_dir = 0;
    struct Window        *win;

#ifdef __amigaos4__
    int release_asl = 0;
#endif

    if (!AslBase) {
	AslBase = OpenLibrary ("asl.library", 36);
	if (!AslBase) {
	    write_log ("Can't open asl.library v36.\n");
	    return;
	} else {
#ifdef __amigaos4__
	    IAsl = (struct AslIFace *) GetInterface ((struct Library *)AslBase, "main", 1, NULL);
	    if (!IAsl) {
		CloseLibrary (AslBase);
		AslBase = 0;
		write_log ("Can't get asl.library interface\n");
	    }
#endif
	}
#ifdef __amigaos4__
    } else {
        IAsl->Obtain ();
        release_asl = 1;
#endif
    }

    FileRequest = AllocAslRequest (ASL_FileRequest, NULL);
    if (!FileRequest) {
	write_log ("Unable to allocate file requester.\n");
	return;
    }

    /* Find this task's default window */
    win = ((struct Process *) FindTask (NULL))->pr_WindowPtr;
    if (win == (struct Window *)-1)
	win = 0;

    sprintf (buff, "Select image to insert in drive DF%d:", drive);
    if (AslRequestTags (FileRequest,
			ASLFR_TitleText,      (ULONG) buff,
			ASLFR_InitialDrawer,  (ULONG) last_dir,
			ASLFR_InitialPattern, (ULONG) "(#?.(ad(f|z)|dms|ipf|zip)#?|df?|?)",
			ASLFR_DoPatterns,     TRUE,
			ASLFR_RejectIcons,    TRUE,
			ASLFR_Window,         (ULONG) win,
			TAG_DONE)) {

	/* Remember directory */
	if (last_dir) {
	    free (last_dir);
	    last_dir = 0;
	}
	if (FileRequest->fr_Drawer && strlen (FileRequest->fr_Drawer))
	    last_dir = malloc (strlen (FileRequest->fr_Drawer));
	if (last_dir)
	    strcpy (last_dir, FileRequest->fr_Drawer);

	/* Construct file path to selected image */
	strcpy (path, FileRequest->fr_Drawer);
	if (strlen(path) && !(path[strlen (path) - 1] == ':' || path[strlen (path) - 1] == '/'))
	    strcat (path, "/");
	strcat (path, FileRequest->fr_File);

	/* Insert it */
	strcpy (changed_prefs.df[drive], path);
    }
    FreeAslRequest (FileRequest);

#ifdef __amigaos4__
    if (release_asl)
        IAsl->Release ();
#endif

    return;
}

/****************************************************************************/

static int have_rexx = 0;

int gui_init (void)
{
    if (!have_rexx) {
	have_rexx = rexx_init ();

	if (have_rexx)
	   atexit (rexx_exit);
    }
    return -1;
}

/****************************************************************************/

void gui_exit (void)
{
}

/****************************************************************************/

int gui_update (void)
{
    return 0;
}

/****************************************************************************/

void gui_led (int led, int on)
{
//    main_window_led (led, on);

    if (have_rexx)
        rexx_led (led, on);
}

/****************************************************************************/

void gui_filename (int num, const char *name)
{
    if (have_rexx)
        rexx_filename (num, name);
}

/****************************************************************************/

void gui_handle_events (void)
{
    if (have_rexx)
        rexx_handle_events();
}

/****************************************************************************/

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

/****************************************************************************/

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

/****************************************************************************/

void gui_fps (int fps, int idle)
{
    gui_data.fps  = fps;
    gui_data.idle = idle;
}

/****************************************************************************/

void gui_lock (void)
{
}

/****************************************************************************/

void gui_unlock (void)
{
}

/****************************************************************************/

void gui_display (int shortcut)
{
    if (shortcut >= 0 && shortcut < 4)
	do_disk_insert (shortcut);
}

/****************************************************************************/

void gui_message (const char *format,...)
{
    char msg[2048];
    va_list parms;
    struct EasyStruct req;
    struct Window *win;

    va_start (parms,format);
    vsprintf ( msg, format, parms);
    va_end (parms);

    /* Find this task's default window */
    win = ((struct Process *) FindTask (NULL))->pr_WindowPtr;
    if (win == (struct Window *)-1)
	win = 0;

    req.es_StructSize   = sizeof req;
    req.es_Flags        = 0;
    req.es_Title        = PACKAGE_NAME " Information";
    req.es_TextFormat   = msg;
    req.es_GadgetFormat = "Okay";
    EasyRequest (win, &req, NULL, NULL);

    write_log (msg);
}

/****************************************************************************/

void gui_update_gfx (void)
{
}
