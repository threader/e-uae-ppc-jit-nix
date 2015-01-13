 /*
  * E-UAE - The portable Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"

#ifdef JIT

#if defined(darwin0) 			/* Darwin 0 - 7 */
#include <cache.h>
#elif defined(darwin8)
#include "OSCacheControl.h"
#else					/* Darwin 9< */
#include <libkern/OSCacheControl.h>
#endif

/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
#ifndef darwin0
    sys_icache_invalidate(start, length);
#else
  uintptr_t stop = ((uintptr_t)start + length);

  flush_cache_range((uintptr_t)start, stop);
#endif
}
#endif
