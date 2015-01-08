 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"
#include <cache.h>

#ifdef JIT

#if defined(PPC_CPU)
void ppc_cacheflush(void* start, int length)
{
  unsigned long stop = ((unsigned long)start + length);

  flush_cache_range((unsigned long)start, stop);
}
#endif

#endif
