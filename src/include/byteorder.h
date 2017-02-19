#ifndef HAVE_BSWAP_32
#define bswap32(x) \
( (((x) & 0xff000000u) >> 24) \
| (((x) & 0x00ff0000u) >> 8) \
| (((x) & 0x0000ff00u) << 8) \
| (((x) & 0x000000ffu) << 24) ) 
#endif

/*
 * Byte swapping macros for big endian architectures and compilers,
 * add as appropriate for other architectures and/or compilers.
 *
 *     ld_swap64(src,dst) : uint64_t dst = *(src)
 *     st_swap64(src,dst) : *(dst)       = uint64_t src
 */

#if defined(__PPC__) || defined(_ARCH_PPC)

#if defined(__64BIT__)
#if defined(_ARCH_PWR7)
#define aix_ld_swap64(s64,d64)\
	__asm__ ("ldbrx %0,0,%1" : "=r"(d64) : "r"(s64))
#define aix_st_swap64(s64,d64)\
	__asm__ volatile ("stdbrx %1,0,%0" : : "r"(d64), "r"(s64))
#else
#define aix_ld_swap64(s64,d64)\
{\
	uint64_t *s4, h;\
\
	__asm__ ("addi %0,%3,4;lwbrx %1,0,%3;lwbrx %2,0,%0;rldimi %1,%2,32,0"\
		: "+r"(s4), "=r"(d64), "=r"(h) : "b"(s64));\
}

#define aix_st_swap64(s64,d64)\
{\
	uint64_t *s4, h;\
\
	h = (s64) >> 32;\
\
	__asm__ volatile ("addi %0,%3,4;stwbrx %1,0,%3;stwbrx %2,0,%0"\
		: "+r"(s4) : "r"(s64), "r"(h), "b"(d64));\
}
#endif /*64BIT && PWR7*/
#else
#define aix_ld_swap64(s64,d64)\
{\
	uint32_t *s4, h, l;\
\
	__asm__ ("addi %0,%3,4;lwbrx %1,0,%3;lwbrx %2,0,%0"\
		: "+r"(s4), "=r"(l), "=r"(h) : "b"(s64));\
\
	d64 = ((uint64_t)h<<32) | l;\
}

#define aix_st_swap64(s64,d64)\
{\
	uint32_t *s4, h, l;\
\
	l = (s64) & 0xfffffffful, h = (s64) >> 32;\
\
	__asm__ volatile ("addi %0,%3,4;stwbrx %1,0,%3;stwbrx %2,0,%0"\
		: "+r"(s4) : "r"(l), "r"(h), "b"(d64));\
}
#endif /*__64BIT__*/
#define aix_ld_swap32(s32,d32)\
__asm__ ("lwbrx %0,0,%1" : "=r"(d32) : "r"(s32))
#define aix_st_swap32(s32,d32)\
	__asm__ volatile ("stwbrx %1,0,%0" : : "r"(d32), "r"(s32))
#define ld_swap32(s,d) aix_ld_swap32(s,d)
#define st_swap32(s,d) aix_st_swap32(s,d)
#define ld_swap64(s,d) aix_ld_swap64(s,d)
#define st_swap64(s,d) aix_st_swap64(s,d)
#endif /*__PPC__ || _ARCH_PPC*/

