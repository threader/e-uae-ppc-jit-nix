/*
 * General implementation of invalidate icache range
 */

#include <stdint.h>

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
    .dcache_bsize = 32,
    .icache_bsize = 32
};


static inline void inval_dcache_range(unsigned long start, unsigned long stop)
{
            unsigned long p;
            unsigned long dsize = cache_conf.dcache_bsize;

    for (p = start; p < stop; p += dsize) {
        asm volatile ("dcbst 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
}

static inline void inval_icache_range(unsigned long start, unsigned long stop)
{
            unsigned long p;
            unsigned long isize = cache_conf.icache_bsize;

    for (p = start; p < stop; p += isize) {
        asm volatile ("icbi 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    asm volatile ("isync" : : : "memory");
}
