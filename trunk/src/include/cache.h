/*
 * General implementation of PowerPC cache flush functions
 *
 * Cache line sizes:
 * G2-G4(PPC32) = 32, PPC64 generally = 64, 970*(G5`s) = 128.
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
#ifdef AMCC_CPU
        __asm__ __volatile__ ("msync; isync");
#else
        __asm__ __volatile__ ("sync; isync");
#endif
}

static inline void presync(void)
{
#ifdef AMCC_CPU
        __asm__ __volatile__ ("msync");
#else
        __asm__ __volatile__ ("eieio");
#endif
}

static inline void cflush(uintptr_t p)
{
	__asm__ __volatile__ ("dcbf 0,%0" : : "r" (p));
}

static inline void cinval(uintptr_t p)
{
        __asm__ __volatile__ ("icbi 0,%0" : : "r" (p));
}

static inline void flush_cache_range(uintptr_t start, uintptr_t stop)
{
	uintptr_t p, start1, stop1;
	size_t isize = icache_bsize;
    size_t dsize = dcache_bsize;

    start1 = start & ~(isize - 1);
    stop1 = (stop + isize - 1) & ~(isize - 1);

	presync();

    for (p = start1; p < stop1; p += isize) {
	cflush(p);
    }

    start1 = start & ~(dsize - 1);
    stop1 = (stop + dsize - 1) & ~(dsize - 1);

    for (p = start1; p < stop1; p += dsize) {
        cinval(p);
    }

isync();
}
