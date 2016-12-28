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
  intptr_t stop = ((intptr_t)start + (size_t)length);

  flush_cache_range((intptr_t)start, stop);
}
#endif

#endif
