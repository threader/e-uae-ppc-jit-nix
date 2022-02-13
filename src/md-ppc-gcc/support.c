 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Miscellaneous machine dependent support functions and definitions
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2003-2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "events.h"
#include "machdep/m68k.h"
#include "sleep.h"

#ifndef HAVE_SYNC
# define sync()
#endif

struct flag_struct regflags;

static volatile frame_time_t last_time, best_time;
static volatile int loops_to_go;

void machdep_init (void)
{
    static int done = 0;

    if (!done) {
	rpt_available = 1;

	write_log ("Calibrating timebase: ");
	flush_log ();

	loops_to_go = 5;
	sync ();
	last_time = read_processor_time ();
	uae_msleep (loops_to_go * 1000);
	best_time = read_processor_time () - last_time;

	syncbase = best_time / loops_to_go;
	write_log ("%.6f MHz\n", (double) syncbase / 1000000);

	sleep_test();

	done = 1;
    }
}
