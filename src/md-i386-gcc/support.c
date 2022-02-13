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
#include "machdep/m68k.h"
#include "events.h"
#include "custom.h"
#include "sleep.h"

#ifndef USE_UNDERSCORE
#define LARGE_ALIGNMENT ".align 16\n"
#else
#define LARGE_ALIGNMENT ".align 4,0x90\n"
#endif

struct flag_struct regflags;

/* All the Win32 configurations handle this in od-win32/win32.c */
#ifndef _WIN32

#include <signal.h>

static volatile frame_time_t last_time, best_time;
static volatile int loops_to_go;

#if defined HAVE_SETITIMER || defined HAVE_ALARM
# define USE_ALARM
# ifndef HAVE_SETITIMER
#  define TIME_UNIT 1000000
# else
#  define TIME_UNIT 100000
# endif
#else
# define TIME_DELAY 200
# define TIME_UNIT  (TIME_DELAY*1000)
#endif

#ifndef HAVE_SYNC
# define sync()
#endif

#ifdef USE_ALARM
static void set_the_alarm (void)
{
# ifndef HAVE_SETITIMER
    alarm (1);
# else
    struct itimerval t;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = TIME_UNIT;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = TIME_UNIT;
    setitimer (ITIMER_REAL, &t, NULL);
# endif
}

static int first_loop = 1;

#ifdef __cplusplus
static RETSIGTYPE alarmhandler(...)
#else
static RETSIGTYPE alarmhandler(int foo)
#endif
{
    frame_time_t bar;
    bar = read_processor_time ();
    if (! first_loop && bar - last_time < best_time)
	best_time = bar - last_time;
    first_loop = 0;
    if (--loops_to_go > 0) {
	signal (SIGALRM, alarmhandler);
	last_time = read_processor_time();
	set_the_alarm ();
    } else {
	alarm (0);
	signal (SIGALRM, SIG_IGN);
    }
}
#endif

#include <setjmp.h>
static jmp_buf catch_test;

#ifdef __cplusplus
static RETSIGTYPE illhandler (...)
#else
static RETSIGTYPE illhandler (int foo)
#endif
{
    rpt_available = 0;
    longjmp (catch_test, 1);
}

void machdep_init (void)
{
    static int done = 0;

    if (!done) {
	rpt_available = 1;

	write_log ("Testing the RDTSC instruction ... ");
	signal (SIGILL, illhandler);
	if (setjmp (catch_test) == 0)
	    read_processor_time ();
	signal (SIGILL, SIG_DFL);
	write_log ("done.\n");

	if (! rpt_available) {
	    write_log ("Your processor does not support the RDTSC instruction.\n");
	    return;
	}

	write_log ("Calibrating delay loop.. ");
	flush_log ();

	best_time = (frame_time_t)-1;
	loops_to_go = 5;

#ifdef USE_ALARM
	signal (SIGALRM, alarmhandler);
#endif

	/* We want exact values... */
	sync (); sync (); sync ();

#ifdef USE_ALARM
	last_time = read_processor_time ();
	set_the_alarm ();

	while (loops_to_go != 0)
	    uae_msleep (10);
#else
	{
	    int i = loops_to_go;
	    frame_time_t bar;

	    while (i-- > 0) {
		last_time = read_processor_time ();
		uae_msleep (TIME_DELAY);
		bar = read_processor_time ();
		if (i != loops_to_go && bar - last_time < best_time)
		    best_time = bar - last_time;
	    }
	}
#endif

	syncbase = best_time * (1000000 / TIME_UNIT);

	write_log ("ok - %.2f BogoMIPS\n", ((double)RPT_SCALE_FACTOR * best_time / TIME_UNIT));

	sleep_test ();

	done = 1;
     }
}
#endif
