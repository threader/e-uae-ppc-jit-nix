 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Sleeping for *nix systems
  *
  * Copyright 2003-2004 Richard Drummond
  */

#ifndef _WIN32

#include "sysconfig.h"
#include "sysdeps.h"

#include "uae.h"
#include "custom.h"
#include "events.h"
#include "options.h"
#include "sleep.h"


/* Define me to always sleep no matter what the latency 
 * of the sleep function */
//#define SLEEP_DONT_BUSY_WAIT

/* Busy sleep threshhold in ms */
#define SLEEP_BUSY_THRESHOLD	 10


void sleep_millis (int ms)
{
    uae_u64 start = read_processor_time();

#ifndef SLEEP_DONT_BUSY_WAIT
    if (!currprefs.dont_busy_wait && ms < SLEEP_BUSY_THRESHOLD) {
	/* Typical sleep routines can't sleep for less than 10ms. If we want
	 * to sleep for a period shorter than this, we'll have to busy wait . . .
	 */
	frame_time_t delay = ((frame_time_t)ms) * syncbase / 1000;

	while ((read_processor_time() - start) < delay)
	     ;
    } else
#endif
	my_usleep (ms * 1000);

    idletime += read_processor_time() - start;
}

void sleep_millis_busy (int ms)
{
    /* Only sleep if we don't have to busy wait */
#ifndef SLEEP_DONT_BUSY_WAIT
    if (currprefs.dont_busy_wait || ms >= SLEEP_BUSY_THRESHOLD)
#endif
	sleep_millis( ms );
}


uae_u32 do_sleep_test (int ms)
{
    uae_u64 t;
    uae_u32 t2;
   
    t = read_processor_time();
    my_usleep (ms*1000);
    t2 = read_processor_time() - t;

    return t2;
}


/*
 * Test the system sleep routine to decide whether we should
 * busy wait by default for 1 ms sleeps 
 */
void sleep_test (void)
{
    int result;

    currprefs.dont_busy_wait = 1;
    
#ifndef SLEEP_DONT_BUSY_WAIT   

    if (rpt_available) {
	uae_u64 total = 0;
	int result;
	int num_tests;
	int i;

	write_log ("Testing system sleep function"); fflush(stderr);

	/* Do a few tests to get a rough idea how fast we can do it */
	num_tests = 5;

	for (i=0; i<num_tests; i++)
	    total += do_sleep_test (1);

	/* How many for 2 seconds worth of tests . . . */
	num_tests = 2 * syncbase * num_tests / total;
	total = 0;

	/* Now the test proper */
	for (i=0; i < num_tests; i++) {
	    total += do_sleep_test (1);

	    if (i - (i % 100) == i) {
		write_log(".");
		fflush(stderr);
	    }
	}

	result = (1000 * total / syncbase) / num_tests;
	write_log ("\nAverage duration of a 1ms sleep: %d ms\n", result);

	if (result > 10) {
	    currprefs.dont_busy_wait = 0;
	    write_log ("Enabling busy-waiting for sub-10ms sleeps\n");
	}
    }

#endif
}

#endif /* !_WIN32 */
