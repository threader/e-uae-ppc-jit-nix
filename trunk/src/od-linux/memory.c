 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"
#include <cacheinvalidate.h> 

#ifdef JIT

#if defined(ppclnx) /* Linux PPC */
void ppc_cacheflush(void* start, int length)
{
	unsigned long stop;
  	stop = ((unsigned long)start + length);

  inval_icache_range(start, stop);
}
#endif

#endif
