 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef JIT

#ifdef __MORPHOS__
//We need the memory handling functions for MorphOS
#include <proto/exec.h>
#include <exec/system.h>
#endif

/*
 * Allocate executable memory for JIT cache
 */
void *cache_alloc (int size)
{
   return malloc (size);
}

void cache_free (void *cache)
{
    free (cache);
}

#endif
