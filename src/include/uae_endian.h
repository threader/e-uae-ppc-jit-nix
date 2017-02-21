/*
 * E-UAE - The portable Amiga Emulator
 *
 * Endian swapping
 *
 * (c) 2003-2007 Richard Drummond
 *
 * Based on code from UAE.
 * (c) 1995-2002 Bernd Schmidt
 */

#ifndef UAE_ENDIAN_H
#define UAE_ENDIAN_H

#include "byteorder.h"

# if !(defined(__PPC__) || defined(_ARCH_PPC))
/* Try to use system bswap_16/bswap_32 functions. */
#if defined HAVE_BSWAP_16 && defined HAVE_BSWAP_32
# ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
	#ifndef uae_swap16(x)
	#if __GNUC_PREREQ (4,8)
	#define		uae_swap16(x) __builtin_bswap16(x)
	#else
	#define		uae_swap16(x) bswap_16(x)
	#endif
	#endif

	#ifndef uae_swap32(x)
	#if __GNUC_PREREQ (4,3)
	#define		uae_swap32(x) __builtin_bswap32(x)
	#else
	#define		uae_swap32(x) bswap_32(x)
	#endif
	#endif
# endif
#elif defined UAE_SDL
/* Else, if using SDL, try SDL's endian functions. */
#  include <SDL/SDL_endian.h>
#  define uae_swap16(x) SDL_Swap16(x)
#  define uae_swap32(x) SDL_Swap32(x)
# else
/* Otherwise, we'll roll our own. */
#  define uae_swap16(x)( (((x) & 0x00ff) << 8) | ( (x) >> 8) )
#  define uae_swap32(x) \
    ( (((x) & 0xff000000u) >> 24) \
	| (((x) & 0x00ff0000u) >> 8) \
	| (((x) & 0x0000ff00u) << 8) \
	| (((x) & 0x000000ffu) << 24) ) 
# endif
#endif 

#if defined(__PPC__) || defined(_ARCH_PPC)
static __inline__ void memcpy_bswap32 (void *dst, void *src, int n)
{
  uae_u32 *q = (uae_u32 *)dst;
  uae_u32 *srcp = (uae_u32 *)src;
  uae_u32 i = (uae_u32)(n >>=2);

    while (i--) {
		ld_swap32(srcp+i,q[i]);
 		//st_swap32(srcp[i], q+i);
	};

}
#else 
STATIC_INLINE void memcpy_bswap32 (void *dst, void *src, int n)
{
    int i = n / 4;
    uae_u32 *dstp = (uae_u32 *)dst;
    uae_u32 *srcp = (uae_u32 *)src;
    for ( ; i; i--)
	*dstp++ = uae_swap32 (*srcp++);
}
#endif

#endif /* UAE_ENDIAN_H */
