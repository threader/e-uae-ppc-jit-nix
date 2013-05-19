 /*
  * E-UAE - The portable Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"

#ifdef JIT
#if defined(tenfour) /* != 10.5 > */
#include "OSCacheControl.h"
#elif defined(allelsefailed)
#include <cacheinvalidate.h>
#else
#include <libkern/OSCacheControl.h>
#endif

/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
#ifndef allelsefailed
    sys_icache_invalidate(start, length);
#else
unsigned long stop;
stop = (start + length); 
inval_icache_range(start, stop);
#endif
}
#endif
