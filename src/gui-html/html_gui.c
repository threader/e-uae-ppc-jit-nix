/*
 * UAE - the Un*x Amiga Emulator
 *
 * HTML user interface via Pepper used under Native Client.
 *
 * Messages from the HTML UI are dispatched via PostMessage to the Native
 * Client module.
 */

#include "gui.h"

#include <limits.h>
#include <stdlib.h>

#include "inputdevice.h"
#include "options.h"
#include "sounddep/sound.h"
#include "sysdeps.h"
#include "threaddep/thread.h"
#include "uae.h"
#include "writelog.h"

/* For mutual exclusion on pref settings. */
static uae_sem_t gui_sem;
/* For sending messages from the GUI to UAE. */
static smp_comm_pipe from_gui_pipe;

static char *gui_romname = 0;
static char *new_disk_string[4];
static unsigned int gui_pause_uae = 0;

static int gui_initialized = 0;

int using_restricted_cloanto_rom = 1;

/*
 * Supported messages. Sent from the GUI to UAE via from_gui_pipe.
 */
enum uae_commands {
    UAECMD_START,
    UAECMD_STOP,
    UAECMD_QUIT,
    UAECMD_RESET,
    UAECMD_PAUSE,
    UAECMD_RESUME,
    UAECMD_DEBUG,
    UAECMD_SAVE_CONFIG,
    UAECMD_EJECTDISK,
    UAECMD_INSERTDISK,
    UAECMD_SELECT_ROM,
    UAECMD_SAVESTATE_LOAD,
    UAECMD_SAVESTATE_SAVE,
    UAECMD_RESIZE
};

/* handle_message()
 *
 * This is called from the GUI when a GUI event happened. Specifically,
 * HandleMessage (PPP_Messaging) forwards dispatched UI action
 * messages posted from JavaScript to handle_message().
 */
int handle_message(const char* msg) {
    /* Grammar for messages from the UI:
     *
     * message ::= 'insert' drive fileURL
     *           | 'rom' fileURL
     *           | 'connect' port input
     *           | 'eject' drive
     *           | 'reset' | 'pause' | 'resume'
     *           | 'resize' <width> <height>
     * device  ::= 'kickstart' | drive
     * drive   ::= 'df0' | 'df1'
     * port    ::= 'port0' | 'port1'
     * input   ::= 'mouse' | 'joy0' | 'joy1' | 'kbd0' | 'kbd1'
     * fileURL ::= <a URL of the form blob://>
     */

    DEBUG_LOG("%s\n", msg);
    if (!gui_initialized) {
        DEBUG_LOG("GUI message refused; not yet initialized.\n");
        return -1;
    }

    /* TODO(cstefansen): scan the string instead of these shenanigans. */

    /* Copy to non-const buffer. */
    char buf[1024];
    (void) strncpy(buf, msg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0'; /* Ensure NUL termination. */

    /* Tokenize message up to 3 tokens (max given the grammar). */
    int i = 0;
    char *t[3], *token, *rest = NULL, *sep = " ";

    for (token = strtok_r(buf, sep, &rest);
         token != NULL && i <= 3;
         token = strtok_r(NULL, sep, &rest), ++i) {
        t[i] = token;
    }

    /* Pipe message to UAE main thread. */
    if (i == 1 && !strcmp(t[0], "reset")) {
        write_comm_pipe_int(&from_gui_pipe, UAECMD_RESET, 1);
    } else if (i == 1 && !strcmp(t[0], "pause")) {
        /* It would be cleaner to call pause_sound and resume_sound in
         * gui_handle_events below, i.e, on the emulator thread. However,
         * if we're pausing because the tab is no longer in the foreground,
         * no graphics flush calls will unblock and no graphics callbacks will
         * be delivered until the tab is back in front. This means that the
         * emulator is probably already stuck in some call and won't get to
         * our UI request to pause the sound. */
        /* TODO(cstefansen)People are reporting pausing/resuming problems; let's
           not do this until investigated. */
        /* pause_sound(); */
        write_comm_pipe_int(&from_gui_pipe, UAECMD_PAUSE, 1);
    } else if (i == 1 && !strcmp(t[0], "resume")) {
        /* resume_sound(); */
        write_comm_pipe_int(&from_gui_pipe, UAECMD_RESUME, 1);
    } else if (i == 2 && !strcmp(t[0], "eject")) {
        int drive_num;
        if (!strcmp(t[1], "df0")) {
            drive_num = 0;
        } else if (!strcmp(t[1], "df1")) {
            drive_num = 1;
        } else {
            return -1;
        }
        write_comm_pipe_int(&from_gui_pipe, UAECMD_EJECTDISK, 0);
        write_comm_pipe_int(&from_gui_pipe, drive_num, 1);
    } else if (i == 3 && !strcmp(t[0], "resize")) {
        long width = strtol(t[1], NULL, 10);
        long height = strtol(t[2], NULL, 10);
        if (width > INT_MAX || height > INT_MAX || errno == ERANGE
            || width <= 0  || height <= 0) {
            write_log("Could not parse width/height in message: %s\n", msg);
            return -1;
        }
        write_comm_pipe_int(&from_gui_pipe, UAECMD_RESIZE, 0);
        write_comm_pipe_int(&from_gui_pipe, (int) width, 0);
        write_comm_pipe_int(&from_gui_pipe, (int) height, 1);
    } else if (i == 3 && !strcmp(t[0], "insert")) {
        int drive_num;
        if (!strcmp(t[1], "df0")) {
            drive_num = 0;
        } else if (!strcmp(t[1], "df1")) {
            drive_num = 1;
        } else {
            return -1;
        }
        uae_sem_wait(&gui_sem);
        if (new_disk_string[drive_num] != 0)
            free (new_disk_string[drive_num]);
        new_disk_string[drive_num] = strdup(t[2]);
        uae_sem_post(&gui_sem);
        write_comm_pipe_int (&from_gui_pipe, UAECMD_INSERTDISK, 0);
        write_comm_pipe_int (&from_gui_pipe, drive_num, 1);
    } else if (i == 2 && !strcmp(t[0], "rom")) {
        uae_sem_wait(&gui_sem);
        if (gui_romname != 0)
            free (gui_romname);
        gui_romname = strdup(t[1]);
        uae_sem_post(&gui_sem);
        write_comm_pipe_int(&from_gui_pipe, UAECMD_SELECT_ROM, 1);
    } else if (i == 3 && !strcmp(t[0], "connect")) {
        int port_num;
        if (!strcmp(t[1], "port0")) {
            port_num = 0;
        } else if (!strcmp(t[1], "port1")) {
            port_num = 1;
        } else {
            return -1;
        }

        int input_device =
                !strcmp(t[2], "mouse") ? JSEM_MICE :
                !strcmp(t[2], "joy0") ? JSEM_JOYS :
                !strcmp(t[2], "joy1") ? JSEM_JOYS + 1 :
                !strcmp(t[2], "kbd0") ? JSEM_KBDLAYOUT + 1 :
                !strcmp(t[2], "kbd1") ? JSEM_KBDLAYOUT + 2 :
                JSEM_END;

        changed_prefs.jports[port_num].id = input_device;
        if (changed_prefs.jports[port_num].id !=
            currprefs.jports[port_num].id) {
            /* It's a little fishy that the typical way to update input
             * devices doesn't use the comm pipe.
             */
            inputdevice_updateconfig (&changed_prefs);
            inputdevice_config_change();
        }
    } else {
        return -1;
    }
    return 0;
}

/* TODO(cstefansen): Factor out general descriptions like the following to
 * gui.h.
 */
/*
 * gui_init()
 *
 * This is called from the main UAE thread to tell the GUI to initialize.
 * To indicate failure to initialize, return -1.
 */
int gui_init (void)
{
    init_comm_pipe (&from_gui_pipe, 8192 /* size */, 1 /* chunks */);
    uae_sem_init (&gui_sem, 0, 1);
    gui_initialized = 1;
    return 0;
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
    DEBUG_LOG("gui_update() unimplemented for HTML GUI.\n");
    return 0;
}

/*
 * gui_exit()
 *
 * This called from the main UAE thread to tell the GUI to quit gracefully.
 */
void gui_exit (void) {}

/*
 * gui_led()
 *
 * Called from the main UAE thread to inform the GUI
 * of disk activity so that indicator LEDs may be refreshed.
 */
void gui_led (int num, int on) {}

/*
 * gui_handle_events()
 *
 * This is called from the main UAE thread to process events sent from
 * the GUI thread.
 *
 * If the UAE emulation proper is not running yet or is paused,
 * this loops continuously waiting for and responding to events
 * until the emulation is started or resumed, respectively. When
 * the emulation is running, this is called periodically from
 * the main UAE event loop.
 */
void gui_handle_events (void)
{
    /* Read GUI command if any. */

    /* Process it, e.g., call uae_reset(). */
    while (comm_pipe_has_data (&from_gui_pipe) || gui_pause_uae) {
        DEBUG_LOG("gui_handle_events: trying to read...\n");
        int cmd = read_comm_pipe_int_blocking (&from_gui_pipe);
        DEBUG_LOG("gui_handle_events: %i\n", cmd);

        switch (cmd) {
        case UAECMD_EJECTDISK: {
            int n = read_comm_pipe_int_blocking (&from_gui_pipe);
            uae_sem_wait(&gui_sem);
            changed_prefs.floppyslots[n].df[0] = '\0';
            uae_sem_post(&gui_sem);
            continue;
        }
        case UAECMD_INSERTDISK: {
            int n = read_comm_pipe_int_blocking (&from_gui_pipe);
            if (using_restricted_cloanto_rom) {
                write_log("Loading other disks is not permitted under the "
                          "license for the built-in Cloanto Kickstart "
                          "ROM.\n");
                continue;
            }
            uae_sem_wait(&gui_sem);
            strncpy (changed_prefs.floppyslots[n].df, new_disk_string[n], 255);
            free (new_disk_string[n]);
            new_disk_string[n] = 0;
            changed_prefs.floppyslots[n].df[255] = '\0';
            uae_sem_post(&gui_sem);
            continue;
        }
        case UAECMD_RESET:
            uae_reset(1);
            break; // Stop GUI command processing until UAE is ready again.
        case UAECMD_PAUSE:
            gui_pause_uae = 1;
            continue;
        case UAECMD_RESUME:
            gui_pause_uae = 0;
            continue;
        case UAECMD_SELECT_ROM:
            uae_sem_wait(&gui_sem);
            strncpy(changed_prefs.romfile, gui_romname, 255);
            changed_prefs.romfile[255] = '\0';
            free(gui_romname);
            gui_romname = 0;

            /* Switching to non-restricted ROM; rebooting. */
            using_restricted_cloanto_rom = 0;
            uae_reset(1);

            uae_sem_post(&gui_sem);
            continue;
        case UAECMD_RESIZE: {
            int width = read_comm_pipe_int_blocking(&from_gui_pipe);
            int height = read_comm_pipe_int_blocking(&from_gui_pipe);
            screen_size_changed(width, height);
            continue;
        }
        default:
            DEBUG_LOG("Unknown command %d received from GUI.\n", cmd);
            continue;
        }
    }
}

/*
 * gui_filename()
 *
 * This is called from the main UAE thread to inform
 * the GUI that a floppy disk has been inserted or ejected.
 */
void gui_filename (int num, const char *name) {}

/* gui_fps()
 *
 * This is called from the main UAE thread to provide the GUI with
 * the most recent FPS and idle numbers.
 */
void gui_fps (int fps, int idle) {
    gui_data.fps  = fps;
    gui_data.idle = idle;
}

/* gui_lock() */
void gui_lock (void) {}

/* gui_unlock() */
void gui_unlock (void) {}

/* gui_flicker_led()
 *
 * This is called from the main UAE thread to tell the GUI that a particular
 * drive LED should flicker to indicate I/O activity.
 */
void gui_flicker_led (int led, int unitnum, int status) {}

/* gui_disk_image_change() */
void gui_disk_image_change (int unitnum, const TCHAR *name, bool writeprotected) {}

/* gui_display() */
void gui_display (int shortcut) {}

/* gui_gameport_button_change() */
void gui_gameport_button_change (int port, int button, int onoff) {}

/* gui_gameport_axis_change */
void gui_gameport_axis_change (int port, int axis, int state, int max) {}

/* gui_message() */
void gui_message (const char *format,...) {}
