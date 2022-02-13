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

#ifdef USE_SDL
# include <SDL.h>
#endif

/* When built with libnix and linked against the swapstack
 * module, this will ensure that the stack is at least
 * MIN_STACKS_SIZE (bytes) on start-up.
 */
#define MIN_STACK_SIZE  32768

unsigned long __stack = MIN_STACK_SIZE;



/*
 * Amiga-specific main entry
 */
int main (int argc, char *argv[])
{
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
