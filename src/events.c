 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Events
  * These are best for low-frequency events. Having too many of them,
  * or using them for events that occur too frequently, can cause massive
  * slowdown.
  *
  * Copyright 1995-1998 Bernd Schmidt
  * Copyright 2004      Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "events.h"
#include "custom.h"
#include "cia.h"
#include "blitter.h"
#include "disk.h"
#include "audio.h"

/* Current time in cycles */
unsigned int currcycle;

/* Cycles to next event pending */
static unsigned int nextevent;

#ifdef JIT
/* For faster cycles handling */
signed int pissoff = 0;
#endif

struct ev eventtab[ev_max];


void init_eventtab (void)
{
    int i;

    nextevent = 0;
    set_cycles (0);

    for (i = 0; i < ev_max; i++) {
	eventtab[i].active = 0;
	eventtab[i].oldcycles = 0;
    }

    eventtab[ev_cia].handler     = CIA_handler;
    eventtab[ev_hsync].handler   = hsync_handler;
    eventtab[ev_hsync].evtime    = get_cycles () + HSYNCTIME;
    eventtab[ev_hsync].active    = 1;
    eventtab[ev_copper].handler  = copper_handler;
    eventtab[ev_copper].active   = 0;
    eventtab[ev_blitter].handler = blitter_handler;
    eventtab[ev_blitter].active  = 0;
    eventtab[ev_disk].handler    = DISK_handler;
    eventtab[ev_disk].active     = 0;
    eventtab[ev_audio].handler   = audio_evhandler;
    eventtab[ev_audio].active    = 0;

    events_schedule ();
}

/*
 * Determine next event pending
 */
void events_schedule (void)
{
    int i;

    unsigned long int mintime = ~0L;
    for (i = 0; i < ev_max; i++) {
	if (eventtab[i].active) {
	    unsigned long int eventtime = eventtab[i].evtime - currcycle;
	    if (eventtime < mintime)
		mintime = eventtime;
	}
    }
    nextevent = currcycle + mintime;
}

/*
 * Handle all events pending within the next cycles_to_add cycles
 */
void do_cycles_slow (unsigned int cycles_to_add)
{
#ifdef JIT
    if ((pissoff -= cycles_to_add) >= 0)
	return;

    cycles_to_add = -pissoff;
    pissoff = 0;
#endif

    if (is_lastline && eventtab[ev_hsync].evtime - currcycle <= cycles_to_add) {
	int rpt = read_processor_time ();
	int v   = rpt - vsyncmintime;
	if (v > (int)syncbase || v < -((int)syncbase))
	    vsyncmintime = rpt;
	if (v < 0) {
#ifdef JIT
            pissoff = 3000 * CYCLE_UNIT;
#endif
	    return;
	}
    }

    while ((nextevent - currcycle) <= cycles_to_add) {
	int i;
	cycles_to_add -= (nextevent - currcycle);
	currcycle = nextevent;

	for (i = 0; i < ev_max; i++) {
	    if (eventtab[i].active && eventtab[i].evtime == currcycle) {
		(*eventtab[i].handler)();
	    }
	}
	events_schedule ();
    }
    currcycle += cycles_to_add;
}

/*
 * Handle all events due at current time
 */
void handle_active_events (void)
{
    int i;
    for (i = 0; i < ev_max; i++) {
	if (eventtab[i].active && eventtab[i].evtime == currcycle) {
	    (*eventtab[i].handler)();
	}
    }
}
