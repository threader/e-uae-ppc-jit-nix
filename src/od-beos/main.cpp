/*
 * UAE - The Un*x Amiga Emulator
 *
 * Copyright 2004 Richard Drummond
 *
 * Start-up and support functions for BeOS target
 */

extern "C" {
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
}


/*
 * BeOS-specific main entry
 */
int main (int argc, char *argv[])
{
    real_main (argc, argv);
    return 0;
}

/*
 * Handle CTRL-C signals
 */
static void sigbrkhandler (int foo)
//static RETSIGTYPE sigbrkhandler(int foo)
{
    activate_debugger ();
}

void setup_brkhandler (void)
{
    struct sigaction sa;
    sa.sa_handler = sigbrkhandler;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT, &sa, NULL);
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
