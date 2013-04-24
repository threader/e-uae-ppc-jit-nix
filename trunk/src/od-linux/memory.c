 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "od-generic/memory.c"

#ifdef JIT
#if defined(ppclnx) /* Linux PPC */
/*
 * PowerPC instruction cache flush function
 */
 struct cache_conf {
    unsigned long dcache_bsize;
    unsigned long icache_bsize;
};

 struct cache_conf cache_conf = {
    .dcache_bsize = 16,
    .icache_bsize = 16
};

#define AT_NULL        0
#define AT_DCACHEBSIZE 19
#define AT_ICACHEBSIZE 20

static void ppc_init_cacheline_sizes(char **envp)
{
    unsigned long *auxv;

    while (*envp++);

    for (auxv = (unsigned long *) envp; *auxv != AT_NULL; auxv += 2) {
        switch (*auxv) {
        case AT_DCACHEBSIZE: cache_conf.dcache_bsize = auxv[1]; break;
        case AT_ICACHEBSIZE: cache_conf.icache_bsize = auxv[1]; break;
        default: break;
        }
    }
}

 void cache_utils_init(char **envp)
{
    ppc_init_cacheline_sizes(envp);
}

void ppc_cacheflush(void* start, int length)
{
	    unsigned long p, start1, stop1;
	    unsigned long dsize = cache_conf.dcache_bsize;
        unsigned long isize = cache_conf.icache_bsize;
	    stop1 = (start + length); 
	    start1 = start; 
	    
	start1 = start1 & ~(dsize - 1);
    stop1 = (stop1 + dsize - 1) & ~(dsize - 1);
    for (p = start1; p < stop1;  p += dsize ) {
        asm volatile ("dcbst 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    
    start1 &= start1 & ~(isize - 1);
    stop1 = (stop1 + isize - 1) & ~(isize - 1);
	    for (p = start1; p < stop1; p += isize ) {
        asm volatile ("icbi 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    asm volatile ("isync" : : : "memory");

}

#endif
#endif
