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
  uintptr_t stop = ((uintptr_t)start + length);

  flush_cache_range((uintptr_t)start, stop);
}
#endif

#endif
