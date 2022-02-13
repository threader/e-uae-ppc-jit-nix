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

/* Busy sleep threshhold in ms. If busy-waiting is enabled, sleeps for periods
 * shorter than this will always be done with a busy loop
 *
 * We should probably determine the threshhold at run-time, but a constant value
 * works well enough
 */
#define SLEEP_BUSY_THRESHOLD	 10

/*
 * Sleep for ms milliseconds either using an appropriate sleep routine on the
 * target system or, if the target's routine has too much latency to
 * accurately sleep for that period, then busy wait.
 *
 * Busy-waiting requires a high-resolution timer for accuracy. We use
 * the machine-dependent read_processor_time ()
 */
void sleep_millis (int ms)
{
    frame_time_t start = read_processor_time ();
    int sleep_time;

#ifndef SLEEP_DONT_BUSY_WAIT
    if (!currprefs.dont_busy_wait && ms < SLEEP_BUSY_THRESHOLD) {
	/* Typical sleep routines can't sleep for less than 10ms. If we want
	 * to sleep for a period shorter than the threshold, we'll have to busy wait . . .
	 */
	int end = start + ms * syncbase / 1000;
	int v;

        do {
	    v = (int)read_processor_time ();
	} while (v < end && v > -end);
    } else
#endif
	uae_msleep (ms);

    sleep_time = read_processor_time () - start;
    if (sleep_time < 0)
	sleep_time = -sleep_time;

    idletime += sleep_time;
}

/*
 * Sleep for ms milliseconds if and only busy-waiting would not be required
 * to do so.
 */
void sleep_millis_busy (int ms)
{
#ifndef SLEEP_DONT_BUSY_WAIT
    if (currprefs.dont_busy_wait || ms >= SLEEP_BUSY_THRESHOLD)
#endif
	sleep_millis (ms);
}

/*
 * Measure how long it takes to do a ms millisecond sleep. Timing is performed
 * with a machine-specific high-resolution timer.
 */
static int do_sleep_test (int ms)
{
    int t;
    int t2;

    t = read_processor_time ();
    uae_msleep (ms);
    t2 = read_processor_time () - t;

    if (t2 < 0)
	t2 = -t2;

    return t2;
}

/*
 * Test the target system's sleep routine to decide whether we should busy wait
 * by default for sleeps shorter in duration than SLEEP_BUSY_THRESHOLD
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

	write_log ("Testing system sleep function"); flush_log ();

	/* Do a few tests to get a rough idea how fast we can do it */
	num_tests = 16;

	for (i=0; i < num_tests; i++)
	    total += do_sleep_test (1);

	/* How many for 2 seconds worth of tests . . . */
	num_tests = 2 * syncbase * num_tests / total;
	total = 0;

	/* Now the test proper */
	for (i = 0; i < num_tests; i++) {
	    total += do_sleep_test (1);

	    if (i - (i % 100) == i) {
		write_log (".");
		flush_log ();
	    }
	}

	result = (1000 * total / syncbase) / num_tests;
	write_log ("\nAverage duration of a 1ms sleep: %d ms\n", result);

	if (result > SLEEP_BUSY_THRESHOLD) {
	    currprefs.dont_busy_wait = 0;
	    write_log ("Enabling busy-waiting for sub-%dms sleeps\n", SLEEP_BUSY_THRESHOLD);
	}
    }

#endif
}

#endif /* !_WIN32 */
