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
#else
#include <libkern/OSCacheControl.h>
#endif

/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
    sys_icache_invalidate(start, length);
}
#endif
