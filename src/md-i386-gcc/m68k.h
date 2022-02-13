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

extern struct flag_struct regflags __asm__ ("regflags");

/*
 * The bits in the cznv field in the above structure are assigned to
 * allow the easy mirroring of the x86 condition flags. (For example,
 * from the AX register - the x86 overflow flag can be copied to AL
 * with a setto %AL instr and the other flags copied to AH with an
 * lahf instr).
 *
 * The 68k CZNV flags are thus assinged in cznv as:
 *
 * <--AL-->  <--AH-->
 * 76543210  FEDCBA98 --------- ---------
 * xxxxxxxV  NZxxxxxC xxxxxxxxx xxxxxxxxx
 */

#define FLAGBIT_N	15
#define FLAGBIT_Z	14
#define FLAGBIT_C	8
#define FLAGBIT_V	0

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

/*
 * Test operations
 *
 * Evaluate operand and set Z and N flags. Always clear C and V.
 */
#define optflag_testl(v)				\
	asm (						\
		"andl %1,%1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "r" (v)				\
		: "eax", "cc"				\
	)

#define optflag_testw(v)				\
	asm (						\
		"andw %w1,%w1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "r" (v)				\
		: "eax", "cc"				\
	);

#define optflag_testb(v)				\
	asm ( 						\
		"andb %b1,%b1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "q" (v)				\
		: "eax", "cc"				\
	);


/*
 * Add operations
 *
 * Perform v = s + d and set ZNCV accordingly
 */
#define optflag_addl(v, s, d)				\
    do {						\
	asm (						\
		"addl %k2,%k1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=r" (v)	\
		: "rmi" (s), "1" (d)			\
		: "cc", "eax"				\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_addw(v, s, d) \
    do { \
	asm ( \
		"addw %w2,%w1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=r" (v)	\
		: "rmi" (s), "1" (d)			\
		: "cc", "eax"				\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_addb(v, s, d) \
    do { \
	asm ( \
		"addb %b2,%b1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=q" (v)	\
		: "qmi" (s), "1" (d)			\
		: "cc", "eax"				\
	);						\
	COPY_CARRY;					\
    } while (0)

/*
 * Add operations
 *
 * Perform v = d - s and set ZNCV accordingly
 */
#define optflag_subl(v, s, d)				\
    do {						\
	asm (						\
		"subl %k2,%k1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=r" (v)	\
		: "rmi" (s), "1" (d)			\
		: "%eax","cc"				\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_subw(v, s, d)				\
    do {						\
	asm (						\
		"subw %w2,%w1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=r" (v)	\
		: "rmi" (s), "1" (d)			\
		: "%eax","cc"				\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_subb(v, s, d)				\
    do {						\
	asm (						\
		"subb %b2,%b1		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv), "=q" (v)	\
		: "qmi" (s), "1" (d)			\
		: "%eax","cc"				\
	);						\
	COPY_CARRY;					\
    } while (0)

/*
 * Add operations
 *
 * Evaluate d - s and set ZNCV accordingly
 */
#define optflag_cmpl(s, d)				\
	asm (						\
		"cmpl %k1,%k2		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "rmi" (s), "r" (d)			\
		: "eax", "cc"				\
	)

#define optflag_cmpw(s, d)				\
	asm (						\
		"cmpw %w1,%w2		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "rmi" (s), "r" (d)			\
		: "eax", "cc"				\
	)

#define optflag_cmpb(s, d)				\
	asm (						\
		"cmpb %b1,%b2		\n\t"		\
		"lahf			\n\t"		\
		"seto %%al		\n\t"		\
		"movb %%ah, regflags+1  \n\t"		\
		"movb %%al, regflags    \n\t"		\
							\
		: "=m" (regflags.cznv)			\
		: "qmi" (s), "q" (d)			\
		: "eax", "cc"				\
	)
