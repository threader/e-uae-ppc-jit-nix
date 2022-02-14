 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Sleeping for *nix systems
  *
  * Copyright 2003 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "uae.h"
#include "custom.h"
#include "events.h"

void sleep_millis (int ms)
{
    if (rpt_available && ms < 10) {
	/* Typical sleep routines can't sleep for less than 10ms. If we want
	 * to sleep for a period shorter than this, we'll have to busy wait . . .
	 */
	frame_time_t start = read_processor_time();
	frame_time_t end   = ((frame_time_t)ms) * syncbase / 1000;
	while ((read_processor_time() - start) < end) {
	     ;
	}
    } else
	my_usleep (ms*1000);
}

void sleep_millis_busy (int ms)
{
    /* Only sleep if we don't have to busy wait */
    if (ms >= 10)
	sleep_millis( ms );
}
