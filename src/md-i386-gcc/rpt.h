/*
  * UAE - The Un*x Amiga Emulator
  *
  * Definitions for accessing cycle counters on a given machine, if possible.
  *
  * Copyright 1997, 1998 Bernd Schmidt
  * Copyright 2003 Richard Drummond
  */

/* RPT_SCALE_FACTOR scales down time values read from the processor. We need this
 * so that the number of ticks per second will fit in an unsigned int even on
 * fast processors (>2GHz). The rest of UAE doesn't need to know that the clock is
 * being scaled down - except where we print the BogoMIPS score in support.c.
 *
 * Don't worry about any loss of precision implied. We don't need nanosecond
 * resolution.
 */

#ifndef RPT_SCALE_FACTOR
#define RPT_SCALE_FACTOR 16
#endif

typedef unsigned long frame_time_t;

STATIC_INLINE uae_u64 read_processor_time (void)
{
    uae_u64 foo;
    /* Don't assume the assembler knows rdtsc */
    __asm__ __volatile__ (".byte 0x0f,0x31" : "=A" (foo) :);
    return foo / RPT_SCALE_FACTOR;
}
