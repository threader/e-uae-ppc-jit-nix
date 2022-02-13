 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Miscellaneous machine dependent support functions and definitions
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "machdep/m68k.h"
#include "custom.h"
#include "events.h"

struct flag_struct regflags;

void machdep_init (void)
{
    /* As of version 0.8.23, all frame-time calculations are done
     * using read_processor_time() rather than gettimeofday().
     * Thus we need a working read_processor_time() on all systems
     *
     * If nothing else is available, we have to fallback on
     * gettimeofday() for this.
     */
    syncbase = 1000000; /* 1 MHz */
    rpt_available = 1;

    write_log ("Warning: UAE has no high-resolution timer support on your system.\n");
}
