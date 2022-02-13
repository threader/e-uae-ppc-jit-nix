/*
 * UAE - The Un*x Amiga Emulator
 *
 * Copyright 2004 Richard Drummond
 *
 * Start-up and support functions for Amiga target
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "uae.h"
#include "xwin.h"
#include "debug.h"

#include "signal.h"

#include <proto/exec.h>

#ifdef USE_SDL
# include <SDL.h>
#endif

/* Get compiler/libc to enlarge stack to this size - if possible */
#if defined __PPC__ || defined __ppc__ || defined POWERPC || defined __POWERPC__
# define MIN_STACK_SIZE  (64 * 1024)
#else
# define MIN_STACK_SIZE  (32 * 1024)
#endif

#if defined __libnix__ || defined __ixemul__
/* libnix requires that we link against the swapstack.o module */
unsigned int __stack = MIN_STACK_SIZE;
#else
# ifdef __amigaos4__
// This breaks for some reason...
//unsigned int __stack_size = MIN_STACK_SIZE;
# endif
#endif

static int fromWB;

/*
 * Amiga-specific main entry
 */
int main (int argc, char *argv[])
{
    fromWB = argc == 0;

#ifdef HAVE_OSDEP_RPT
    /* On 68k machines, open timer.device so that
     * we can use ReadEClock() for timing */
    osdep_open_timer ();
#endif
#ifdef USE_SDL
    init_sdl();
#endif

    real_main (argc, argv);
    return 0;
}

/*
 * Handle CTRL-C signals
 */
static RETSIGTYPE sigbrkhandler(int foo)
{
    activate_debugger ();
}

void setup_brkhandler (void)
{
#ifdef HAVE_SIGACTION
    struct sigaction sa;
    sa.sa_handler = (void*)sigbrkhandler;
    sa.sa_flags = 0;
    sa.sa_flags = SA_RESTART;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT, &sa, NULL);
#else
    signal (SIGINT,sigbrkhandler);
#endif
}

void write_log_amigaos (const char *format, ...)
{
    if (!fromWB) {
	va_list parms;

	va_start (parms,format);
	vfprintf (stderr, format, parms);
	va_end (parms);
    }
}

void flush_log_amigaos (void)
{
    if (!fromWB)
	fflush (stderr);
}

/*
 * Handle target-specific cfgfile options
 */
void target_save_options (FILE *f, struct uae_prefs *p)
{
}

int target_parse_option (struct uae_prefs *p, char *option, char *value)
{
    return 0;
}

void target_default_options (struct uae_prefs *p)
{
}
