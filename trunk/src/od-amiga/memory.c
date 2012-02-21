 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"
#include "include/memory.h"

#ifdef JIT
#if defined __amigaos4__
//We need the memory handling functions and flags for AmigaOS4
#include <proto/exec.h>
#include <exec/exec.h>
#endif

/*
 * Allocate executable memory for JIT cache
 */
void * cache_alloc (int size)
{
#if defined __amigaos4__
	//For AmigaOS4 we must allocate the memory with executable flag
	//also we must align to the size of the cache line
	return AllocVec(size, MEMF_EXECUTABLE|MEMF_HWALIGNED);
#else
   return malloc (size);
#endif
}

#if defined __amigaos4__
//PowerPC instruction cache flush function for AmigaOS4
void ppc_cacheflush(void* start, int length)
{
	CacheClearE(start, length, CACRF_ClearI|CACRF_ClearD);
}
#endif

void cache_free (void *cache)
{
#if defined __amigaos4__
	FreeVec(cache);
#else
	free (cache);
#endif
}

#endif
