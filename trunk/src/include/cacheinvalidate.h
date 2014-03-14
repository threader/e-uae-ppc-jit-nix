/* 
 * General implementation of invalidate icache range
 * 
 */

#include <stdint.h>

static inline void inval_icache_range(unsigned long start, unsigned long stop)
{
	unsigned long p;
	    
    for (p = start; p < stop;  p++) {
        asm volatile ("dcbst 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    
	for (p = start; p < stop; p++) {
        asm volatile ("icbi 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    asm volatile ("isync" : : : "memory");
}