 /*
  * E-UAE - The portable Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"

#ifdef JIT
#include <libkern/OSCacheControl.h>
/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
    sys_icache_invalidate(start, length);
}
#endif
