/*
 * General implementation of invalidate icache range
 */

#include <stdint.h>

#ifndef DCACHE_SIZE
#warning "DCACHE_SIZE not defined! Defaulting to 32! Only normal with --disable-optimization"
#define DCACHE_SIZE 32
#endif

#ifndef ICACHE_SIZE
#warning "ICACHE_SIZE not defined! Defaulting to 32! Only normal with --disable-optimization"
#define ICACHE_SIZE 32
#endif

/*
 * PowerPC instruction cache flush function
 */
 struct cache_conf {
    unsigned long dcache_bsize;
    unsigned long icache_bsize;
};

/*
 *  Cache line sizes, G2-G4 = 32, G5 = 64.
 */
 struct cache_conf cache_conf = {
    .dcache_bsize = DCACHE_SIZE,
    .icache_bsize = ICACHE_SIZE
};


static inline void inval_dcache_range(unsigned long start, unsigned long stop)
{
            unsigned long p;
            unsigned long dsize = cache_conf.dcache_bsize;

    for (p = start; p < stop; p += dsize) {
        	__asm__ __volatile__ ("dcbst 0,%0" : : "r"(p) : "memory");
    }
    	__asm__ __volatile__ ("sync" : : : "memory");
}

static inline void inval_icache_range(unsigned long start, unsigned long stop)
{
            unsigned long p;
            unsigned long isize = cache_conf.icache_bsize;

    for (p = start; p < stop; p += isize) {
#ifdef AMCC_CPU
            __asm__ __volatile__ ("iccci 0,0" : : : "memory");
#else
        	__asm__ __volatile__ ("icbi 0,%0" : : "r"(p) : "memory");
#endif
    }
    	__asm__ __volatile__ ("sync" : : : "memory");
    	__asm__ __volatile__ ("isync" : : : "memory");
}
