/*
  * UAE - The Un*x Amiga Emulator
  *
  * Definitions for accessing cycle counters on a given machine, if possible.
  *
  * Copyright 1998 Bernd Schmidt
  * Copyright 2004 Richard Drummond
  */

typedef unsigned long frame_time_t;

/* Since version 0.8.23, read_processor_time()
 * must work on all systesm.
 *
 * Fake it - using gettimeofday()
 */
static __inline__ frame_time_t read_processor_time (void)
{
    struct timeval t;
    gettimeofday (&t, 0);
    return t.tv_sec * 1000000 + t.tv_usec;
}
