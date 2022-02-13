 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation - machine dependent bits
  *
  * Copyright 1996 Bernd Schmidt, Samuel Devulder
  * Copyright 2004 Richard Drummond
  */

 /*
  * Machine dependent structure for holding the 68k CCR flags
  */
struct flag_struct {
    unsigned short int cznv;
    unsigned short int x;
};

extern struct flag_struct regflags;

/*
 * The bits in the cznv field are assigned in correspondence to the
 * CZNV flags in the 68k CCR register
 */
#define FLAGBIT_N	3
#define FLAGBIT_Z	2
#define FLAGBIT_V	1
#define FLAGBIT_C	0

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

/* sam: MIT or MOTOROLA syntax ? */
#ifdef __linux__
# define MITMOT(mit,mot) mot
# define MIT(x)
# define MOT(x) x
#else /* Amiga os */
# define MITMOT(mit,mot) mit
# define MIT(x) x
# define MOT(x)
#endif

/*
 * Test operations
 *
 * Evaluate operand and set Z and N flags. Always clear C and V.
 */
#define m68k_flag_tst(l, v)				\
    asm (						\
	MIT(	"tst"#l" %1		\n\t"		\
		"movew ccr, %0          \n\t"		\
	)						\
	MOT(	"tst."#l" %1		\n\t"		\
		"move.w %%ccr, %0	\n\t"		\
	)						\
        : "=m"  (regflags.cznv)				\
	: "dmi" (v) 					\
	: "cc"						\
    )

#define optflag_testl(v)	m68k_flag_tst(l, v)
#define optflag_testw(v)	m68k_flag_tst(w, v)
#define optflag_testb(v)	m68k_flag_tst(b, v)

/*
 * Add operations
 *
 * Perform v = s + d and set ZNCV accordingly
 */
#define m68k_flag_add(l, v, s, d)			\
    do {						\
	asm (						\
	    MIT("add"#l" %3,%1		\n\t"		\
		"movew ccr,%0		\n\t"		\
	    )						\
	    MOT("add."#l" %3,%1	\n\t"			\
		"move.w %%ccr,%0	\n\t"		\
	    )						\
	    : "=dm" (regflags.cznv),			\
	      "=&d" (v)					\
	    : "1"   (s),				\
	      "dmi" (d)					\
	    : "cc"					\
	);						\
	COPY_CARRY;					\
    } while (0)

#define optflag_addl(v, s, d)	m68k_flag_add(l, v, s, d)
#define optflag_addw(v, s, d)	m68k_flag_add(w, v, s, d)
#define optflag_addb(v, s, d)	m68k_flag_add(b, v, s, d)

/*
 * Subtraction operations
 *
 * Perform v = d - s and set ZNCV accordingly
 */
#define m68k_flag_sub(l, v, s, d)			\
    do {						\
	asm (						\
	    MIT("sub"#l" %2,%1		\n\t"		\
		"movew ccr,%0		\n\t"		\
	    )						\
	    MOT("sub."#l" %2,%1		\n\t"		\
		"move.w %%ccr,%0	\n\t"		\
	    )						\
	    : "=dm" (regflags.cznv),			\
	      "=&d" (v)					\
	    : "dmi" (s),				\
	      "1"   (d)					\
	    : "cc"					\
	 );						\
	COPY_CARRY;					\
    } while (0)

#define optflag_subl(v, s, d)	m68k_flag_sub(l, v, s, d)
#define optflag_subw(v, s, d)	m68k_flag_sub(w, v, s, d)
#define optflag_subb(v, s, d)	m68k_flag_sub(b, v, s, d)

/*
 * Compare operations
 *
 * Perform d - s and set ZNCV accordingly
 */
#define m68k_flag_cmp(l, s, d)				\
    asm (						\
	MIT(	"cmp"#l" %1,%2		\n\t"		\
		"movew ccr, %0          \n\t"		\
	)						\
	MOT(	"cmp."#l" %0,%1		\n\t"		\
		"move.w %%ccr, %0	\n\t"		\
	)						\
        : "=m"  (regflags.cznv)				\
	: "dmi" (s), 					\
	  "d"   (d)					\
	: "cc"						\
    )

#define optflag_cmpl(s, d)	m68k_flag_cmp(l, s, d)
#define optflag_cmpw(s, d)	m68k_flag_cmp(w, s, d)
#define optflag_cmpb(s, d)	m68k_flag_cmp(b, s, d)
