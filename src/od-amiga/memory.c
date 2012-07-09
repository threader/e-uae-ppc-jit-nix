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
#ifdef __amigaos4__
//We need the memory handling functions and flags for AmigaOS4
#include <proto/exec.h>
#include <exec/exec.h>
#endif

#ifdef __MORPHOS__
//We need the memory handling functions for MorphOS
#include <proto/exec.h>
#include <exec/system.h>
#endif

#ifdef __MORPHOS__
//We need the memory handling functions for MorphOS
#include <proto/exec.h>
#include <exec/system.h>
#endif

/*
 * Allocate executable memory for JIT cache
 */
void * cache_alloc (int size)
{
#ifdef __amigaos4__
	//For AmigaOS4 we must allocate the memory with executable flag
	//also we must align to the size of the cache line
	return AllocVec(size, MEMF_EXECUTABLE|MEMF_HWALIGNED);
#else
#ifdef __MORPHOS__
	//For MorphOS we will try to figure out the cache line size for the aligned memory allocation
	ULONG cachelinesize;
	if (!NewGetSystemAttrsA(&cachelinesize, sizeof(cachelinesize), SYSTEMINFOTYPE_PPC_ICACHEL1LINESIZE, NULL))
	{
		//Failed: let's send a warning and use 32 byte alignment
		write_log("Warning: failed to read cache alignment requirement, 32 bytes alignment is used");
		cachelinesize = 32;
	}

	//Allocate memory
	return AllocVecAligned(size, MEMF_ANY, cachelinesize, 0);
#else
	//Unknown system: use generic memory allocation
   return malloc (size);
#endif
#endif
}

/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
#ifdef __amigaos4__
	//For AmigaOS4
	CacheClearE(start, length, CACRF_ClearI|CACRF_ClearD);
#else
#ifdef __MORPHOS__
	//For MorphOS
	CacheFlushDataInstArea(start, length);
#else
	write_log("Error: processor cache flush function is not implemented for your system");
#endif
#endif
}

/**
 * Free JIT cache
 */
void cache_free (void *cache)
{
#if defined __amigaos4__ || defined __MORPHOS__
	//For AmigaOS4 and MorphOS
	FreeVec(cache);
#else
	//For unknown system
	free (cache);
#endif
}

#endif
