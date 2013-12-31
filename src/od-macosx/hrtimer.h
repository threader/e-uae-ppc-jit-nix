/*
 * E-UAE - The portable Amiga Emulator
 *
 * OS X high-resolution timer support
 *
 * (c) 2005-2008 Richard Drummond
 */

#if defined(__i386__) || defined(__x86_64__)

/*
 * On Intel macs, we use mach_absolute_time
 *
 * This is preferred to using the TSC, since on dual-core machines
 * the TSC on each CPU will be out of sync.
 */

# include <mach/mach_time.h>
# include "machdep/rpt.h"

STATIC_INLINE frame_time_t osdep_gethrtime (void)
{
    return (frame_time_t) mach_absolute_time();
}

STATIC_INLINE frame_time_t osdep_gethrtimebase (void)
{
    struct mach_timebase_info tbinfo;

    mach_timebase_info(&tbinfo);
    return (frame_time_t)1e9 *tbinfo.denom / tbinfo.numer;
}

STATIC_INLINE void osdep_inithrtimer (void)
{
}

#else

/*
 * On PPC Macs, mach_absolute_time reads the CPU's timebase counter,
 * which we do in the machdep part. Our alternative to the TBC
 * (should one be needed, which it shouldn't) is to fallback on the
 * default generic behaviour, which is gettimeofday(). This isn't such
 * as prohibitively expensive call on OS X as it is on many other Unices.
 */

# ifndef EUAE_OSDEP_SYNC_H
#  include "od-generic/hrtimer.h"
# endif

#endif
