#ifndef EVENTS_H
#define EVENTS_H

 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Events
  * These are best for low-frequency events. Having too many of them,
  * or using them for events that occur too frequently, can cause massive
  * slowdown.
  *
  * Copyright 1995-1998 Bernd Schmidt
  */

#undef EVENT_DEBUG

#include "machdep/rpt.h"

extern volatile frame_time_t vsynctime, vsyncmintime;
extern void reset_frame_rate_hack (void);
extern int rpt_available;
extern unsigned long syncbase;

extern void compute_vsynctime (void);
extern void do_cycles_ce (long cycles);


extern unsigned int currcycle;
extern unsigned int is_lastline;

typedef void (*evfunc)(void);

struct ev
{
    int active;
    unsigned long int evtime, oldcycles;
    evfunc handler;
};

enum {
    ev_hsync, ev_copper, ev_audio, ev_cia, ev_blitter, ev_disk,
    ev_max
};

extern struct ev eventtab[ev_max];

extern void init_eventtab (void);
extern void events_schedule (void);
extern void handle_active_events (void);
extern void do_cycles_slow (unsigned int cycles_to_add);

#define do_cycles do_cycles_slow

STATIC_INLINE unsigned int get_cycles (void)
{
    return currcycle;
}

STATIC_INLINE void set_cycles (unsigned int x)
{
#ifdef JIT
    currcycle = x;
#endif
}

#ifdef JIT
/* For faster cycles handling */
extern signed int pissoff;
#endif

STATIC_INLINE void cycles_do_special (void)
{
#ifdef JIT
    if (pissoff >= 0)
        pissoff = -1;
#endif
}

STATIC_INLINE void do_extra_cycles (unsigned long cycles_to_add)
{
#ifdef JIT
    pissoff -= cycles_to_add;
#endif
}

#define countdown pissoff

#endif
