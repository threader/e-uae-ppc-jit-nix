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

/****************************************************************************/

extern void rexx_led(int led, int on);          /* ami-rexx.c */
extern void rexx_filename(int num, const char *name);
extern void rexx_handle_events(void);
extern void main_window_led(int led, int on);   /* ami-win.c */

/****************************************************************************/
char *to_unix_path (char *s);
char *from_unix_path (char *s);
void split_dir_file (char *src, char **dir, char **file);

static void do_disk_insert (int drive)
{
    struct FileRequester *FileRequest;
    char buff[80];
    char *last_file,*last_dir,*s;

    last_file = currprefs.df[drive];

    split_dir_file (from_unix_path(last_file), &last_dir, &last_file);
    if (!last_file) return;
    if (!last_dir)  return;

    if (!AslBase) AslBase = OpenLibrary ("asl.library", 36);
    if (!AslBase) {
	write_log ("Can't open asl.library v36 !");
	return;
    }

    FileRequest = AllocAslRequest (ASL_FileRequest, NULL);
    if (!FileRequest) {
	write_log ("Unable to allocate file requester.\n");
	return;
    }

    sprintf (buff, "Select file to use for drive DF%d:", drive);
    if (AslRequestTags (FileRequest,
//			use_graffiti?TAG_IGNORE:
//                      ASLFR_Window,         (ULONG)W,
			ASLFR_TitleText,      (ULONG)buff,
			ASLFR_InitialDrawer,  (ULONG)last_dir,
			ASLFR_InitialFile,    (ULONG)last_file,
			ASLFR_InitialPattern, (ULONG)"(#?.(ad(f|z)|dms|zip)#?|df?|?)",
			ASLFR_DoPatterns,     TRUE,
			ASLFR_RejectIcons,    TRUE,
			TAG_DONE)) {
	free (last_file);
	last_file = malloc (3 + strlen (FileRequest->fr_Drawer) +
					strlen (FileRequest->fr_File));
	if ((last_file)) {
	    s = last_file;
	    strcpy (s, FileRequest->fr_Drawer);
	    if (*s && !(s[strlen (s) - 1] == ':' || s[strlen (s) - 1] == '/'))
		strcat (s,"/");
	    strcat (s, FileRequest->fr_File);
	    last_file = to_unix_path (s); free (s);
        }
    } else {
        free (last_file);
        last_file = NULL;
    }
    FreeAslRequest (FileRequest);
    free (last_dir);

    if (last_file) {
        disk_insert (drive, last_file);
        free (last_file);
    }
    return;
}

/****************************************************************************/

int gui_init (void)
{
    return 0;
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
    main_window_led(led, on);
    rexx_led(led, on);
}

/****************************************************************************/

void gui_filename (int num, const char *name)
{
    rexx_filename(num, name);
}

/****************************************************************************/

void gui_handle_events (void)
{
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

    va_start (parms,format);
    vsprintf ( msg, format, parms);
    va_end (parms);

    req.es_StructSize   = sizeof req;
    req.es_Flags        = 0;
    req.es_Title        = "UAE Information";
    req.es_TextFormat   = msg;
    req.es_GadgetFormat = "Okay";
    EasyRequest (NULL /*window*/, &req, NULL, NULL);

    write_log (msg);
}
