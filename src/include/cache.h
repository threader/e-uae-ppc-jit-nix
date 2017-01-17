/*
 * General implementation of PowerPC cache flush functions
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

static size_t dcache_bsize = DCACHE_SIZE;
static size_t icache_bsize = ICACHE_SIZE;

static inline void isync(void)
{
#if defined(_ARCH_PWR4) && !defined(E500_CPU) && !defined(AMCC_CPU)
 		__asm__ __volatile__ ("lwsync");
#elif defined(AMCC_CPU) || defined(E500_CPU) || defined(PPC_ISA_203)
        __asm__ __volatile__ ("msync; isync");
#else
        __asm__ __volatile__ ("sync; isync");
#endif
}

static inline void dsync(void)
{
#if defined(AMCC_CPU) || defined(E500_CPU) || defined(PPC_ISA_203)
        __asm__ __volatile__ ("msync");
#else
        __asm__ __volatile__ ("sync");
#endif
}

static inline void cflush(intptr_t p)
{
	    __asm__ __volatile__ ("dcbf 0,%0" : : "r" (p));
}

static inline void cinval(intptr_t p)
{
        __asm__ __volatile__ ("icbi 0,%0" : : "r" (p));
}

static inline void flush_cache_range(intptr_t start, intptr_t stop)
{
    intptr_t p, start1, stop1, i_mask, d_mask;
    const size_t isize = icache_bsize;
    const size_t dsize = dcache_bsize;

    i_mask = ~(isize - 1);
    start1 = ((intptr_t)start) & i_mask;
    stop1 = ((intptr_t)stop + isize - 1) & i_mask;

	
    for (intptr_t p = start1; p < stop1; p += isize) {
	cflush(p);
    }

dsync();

    d_mask = ~(dsize - 1);
    start1 = ((intptr_t)start) & d_mask;
    stop1 = ((intptr_t)stop + dsize - 1) & d_mask;

    for (intptr_t p = start1; p < stop1; p += dsize) {
        cinval(p);
    }

isync();
}
