 /*
  * UAE - The Un*x Amiga Emulator
  *
  * PPC-SYSV/GCC stack magic definitions for autoconf.c
  *
  * Copyright 2004 Richard Drummond
  */

#include <setjmp.h>

#undef CAN_DO_STACK_MAGIC

#if (__GNUC__ > 2 || __GNUC_MINOR__ >= 7)

static inline void transfer_control(void *, int, void *, void *, int) __attribute__((noreturn));

#define CAN_DO_STACK_MAGIC
#define USE_EXECLIB

#ifdef __APPLE__
# define R1 "r1"
# define R2 "r2"
# define R3 "r3"
# define R4 "r4"
# define R5 "r5"
#else
# define R1  "1"
# define R2  "2"
# define R3  "3"
# define R4  "4"
# define R5  "5"
#endif

static inline void transfer_control (void *s, int size, void *pc, void *f, int has_retval)
{
    unsigned long *stacktop = (unsigned long *)((char *)s + size - 20);
    stacktop[4] = 0xC0DEDBAD;                   /* End back-chain */
    stacktop[3] = 0;                            /* Local variable: retval */
    stacktop[2] = has_retval;                   /* Local variable: has_retval */
    stacktop[1] = 0;                            /* LR save word */
    stacktop[0] = (unsigned long) &stacktop[4]; /* Back-chain */

    __asm__ __volatile__ ("\
	mtctr %0        \n\
	mr    "R1",%1   \n\
	mr    "R3",%2   \n\
	mr    "R4",%3   \n\
	mr    "R5",%4   \n\
	bctr"
	:
	: "r" (pc),
	  "r" (stacktop),
	  "r" (s),
	  "r" (f),
	  "r" (&stacktop[3])
	: "memory");

    /* Not reached. */
    abort ();
}

static inline uae_u32 get_retval_from_stack (void *s, int size)
{
    return *(uae_u32 *)((char *)s + size - 8);
}

static inline int stack_has_retval (void *s, int size)
{
    return *(int *)((char *)s + size - 12);
}

#endif
