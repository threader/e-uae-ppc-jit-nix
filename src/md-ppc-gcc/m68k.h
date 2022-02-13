 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation - machine dependent bits
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2004 Richard Drummond
  */

 /*
  * Machine dependent structure for holding the 68k CCR flags
  */
struct flag_struct {
    unsigned int cznv;
    unsigned int x;
};

extern struct flag_struct regflags;

/*
 * The bits in the cznv field in the above structure are assigned to
 * allow the easy mirroring of the PPC condition flags.
 *
 * The 68k CZNV flags are thus assinged in cznv as:
 *
 *      <cr0> <cr1> <cr2> <cr3> <cr4> <cr5> <cr6> <cr7>
 * bit:  00    04    08    0C    10    14    18    1C
 *       |     |     |     |     |     |     |     |
 * flag: N-Z-  ----  -VC-  ----  ----  ----  ----  ----
 *
 * Note: The PPC convention is that the MSB is bit 0. Don't get confused.
 *
 * Note: The PPC Carry flags has the the opposite sense of the 68k
 * Carry flag for substractions. Thus, following a substraction, the
 * C bit in the above needs to be flipped.
 */

#define FLAGBIT_N	31
#define FLAGBIT_Z	29
#define FLAGBIT_V	22
#define FLAGBIT_C	21

#define FLAGVAL_N	(1 << FLAGBIT_N)
#define FLAGVAL_Z 	(1 << FLAGBIT_Z)
#define FLAGVAL_C	(1 << FLAGBIT_C)
#define FLAGVAL_V	(1 << FLAGBIT_V)

#define SET_ZFLG(y)	(regflags.cznv = (regflags.cznv & ~FLAGVAL_Z) | (((y) & 1) << FLAGBIT_Z))
#define SET_CFLG(y)	(regflags.cznv = (regflags.cznv & ~FLAGVAL_C) | (((y) & 1) << FLAGBIT_C))
#define SET_VFLG(y)	(regflags.cznv = (regflags.cznv & ~FLAGVAL_V) | (((y) & 1) << FLAGBIT_V))
#define SET_NFLG(y)	(regflags.cznv = (regflags.cznv & ~FLAGVAL_N) | (((y) & 1) << FLAGBIT_N))
#define SET_XFLG(y)	(regflags.x = (y))

#define GET_ZFLG	((regflags.cznv >> FLAGBIT_Z) & 1)
#define GET_CFLG	((regflags.cznv >> FLAGBIT_C) & 1)
#define GET_VFLG	((regflags.cznv >> FLAGBIT_V) & 1)
#define GET_NFLG	((regflags.cznv >> FLAGBIT_N) & 1)
#define GET_XFLG	(regflags.x & 1)

#define CLEAR_CZNV	(regflags.cznv = 0)
#define GET_CZNV	(regflags.cznv)
#define IOR_CZNV(X)	(regflags.cznv |= (X))
#define SET_CZNV(X)	(regflags.cznv = (X))

#define COPY_CARRY	(regflags.x = (regflags.cznv) >> FLAGBIT_C)

/*
 * Test CCR condition
 */
STATIC_INLINE int cctrue (int cc)
{
    uae_u32 cznv = regflags.cznv;

    switch (cc) {
	case 0:  return 1;								/*				T  */
	case 1:  return 0;								/*				F  */
	case 2:  return (cznv & (FLAGVAL_C | FLAGVAL_Z)) == 0;				/* !CFLG && !ZFLG		HI */
	case 3:  return (cznv & (FLAGVAL_C | FLAGVAL_Z)) != 0;				/*  CFLG || ZFLG		LS */
	case 4:  return (cznv & FLAGVAL_C) == 0;					/* !CFLG			CC */
	case 5:  return (cznv & FLAGVAL_C) != 0;					/*  CFLG			CS */
	case 6:  return (cznv & FLAGVAL_Z) == 0;					/* !ZFLG			NE */
	case 7:  return (cznv & FLAGVAL_Z) != 0;					/*  ZFLG			EQ */
	case 8:  return (cznv & FLAGVAL_V) == 0;					/* !VFLG			VC */
	case 9:  return (cznv & FLAGVAL_V) != 0;					/*  VFLG			VS */
	case 10: return (cznv & FLAGVAL_N) == 0;					/* !NFLG			PL */
	case 11: return (cznv & FLAGVAL_N) != 0;					/*  NFLG			MI */
	case 12: return (((cznv << (FLAGBIT_N - FLAGBIT_V)) ^ cznv) & FLAGVAL_N) == 0;	/*  NFLG == VFLG		GE */
	case 13: return (((cznv << (FLAGBIT_N - FLAGBIT_V)) ^ cznv) & FLAGVAL_N) != 0;	/*  NFLG != VFLG		LT */
	case 14: cznv &= (FLAGVAL_N | FLAGVAL_Z | FLAGVAL_V);				/* ZFLG && (NFLG == VFLG)	GT */
		 return (((cznv << (FLAGBIT_N - FLAGBIT_V)) ^ cznv) & (FLAGVAL_N | FLAGVAL_Z)) == 0;
	case 15: cznv &= (FLAGVAL_N | FLAGVAL_Z | FLAGVAL_V);				/* ZFLG && (NFLG != VFLG)	LE */
		 return (((cznv << (FLAGBIT_N - FLAGBIT_V)) ^ cznv) & (FLAGVAL_N | FLAGVAL_Z)) != 0;
    }
    abort ();
    return 0;
}

/*
 * Optimized code which uses the host CPU's condition flags to evaluate
 * 68K CCR flags for certain operations.
 *
 * These are used by various opcode handlers when
 * gencpu has been built with OPTIMIZED_FLAGS defined
 */

/* GCC 2.95 doesn't understand the XER register */
#if __GNUC__ - 1 > 1
# define DEP_XER ,"xer"
#else
# define DEP_XER
#endif

/*
 * Test operations
 *
 * Evaluate operand and set Z and N flags. Always clear C and V.
 */
#define optflag_testl(v) 				\
    do { 						\
	uae_u32 tmp; 					\
	asm (						\
		"cmpi cr0, %2, 0	\n\t" 		\
		"mfcr %1		\n\t" 		\
		"rlwinm %0, %1, 0, 0, 3 \n\t" 		\
							\
		: "=r" (regflags.cznv),			\
		  "=r" (tmp)				\
		:  "r" (v)				\
		: "cr0"					\
	);						\
    } while (0)

#define optflag_testw(v) optflag_testl((uae_s32)(v))
#define optflag_testb(v) optflag_testl((uae_s32)(v))

/*
 * Add operations
 *
 * Perform v = s + d and set ZNCV accordingly
 */
#define optflag_addl(v, s, d)				\
    do {						\
	asm (						\
		"addco. %1, %2, %3	\n\t"		\
		"mcrxr  cr2		\n\t"		\
		"mfcr   %0		\n\t"		\
							\
		: "=r" (regflags.cznv), "=r" (v)	\
		: "r" (s), "r" (d)			\
		: "cr0", "cr2"  DEP_XER			\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_addw(v, s, d) do { optflag_addl((v), (s) << 16, (d) << 16); v = v >> 16; } while (0)
#define optflag_addb(v, s, d) do { optflag_addl((v), (s) << 24, (d) << 24); v = v >> 24; } while (0)

/*
 * Subtraction operations
 *
 * Perform v = d - s and set ZNCV accordingly
 */
#define optflag_subl(v, s, d)				\
    do {						\
	asm (						\
		"subfco. %1, %2, %3	\n\t"		\
		"mcrxr  cr2		\n\t"		\
		"mfcr   %0		\n\t"		\
		"xoris  %0,%0,32	\n\t"		\
							\
		: "=r" (regflags.cznv),			\
		  "=r" (v)				\
		:  "r" (s),				\
		   "r" (d)				\
		: "cr0", "cr2"  DEP_XER			\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_subw(v, s, d) do { optflag_subl(v, (s) << 16, (d) << 16); v = v >> 16; } while (0)
#define optflag_subb(v, s, d) do { optflag_subl(v, (s) << 24, (d) << 24); v = v >> 24; } while (0)

#define optflag_cmpl(s, d) 				\
    do { 						\
	uae_s32 tmp; 					\
	asm (						\
		"subfco. %1, %2, %3	\n\t"		\
		"mcrxr  cr2		\n\t"		\
		"mfcr   %0		\n\t"		\
		"xoris  %0,%0,32	\n\t"		\
							\
		: "=r" (regflags.cznv),			\
		  "=r" (tmp)				\
		:  "r" (s),				\
		   "r" (d) 				\
		: "cr0", "cr2"  DEP_XER			\
	);						\
    } while (0)

#define optflag_cmpw(s, d) optflag_cmpl((s) << 16, (d) << 16)
#define optflag_cmpb(s, d) optflag_cmpl((s) << 24, (d) << 24)
