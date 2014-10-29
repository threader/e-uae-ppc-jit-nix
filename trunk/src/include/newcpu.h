 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation
  *
  * Copyright 1995 Bernd Schmidt
  */

#include "readcpu.h"
#include "machdep/m68k.h"
#include "events.h"

#ifndef SET_CFLG

#define SET_CFLG(regs, x) (CFLG(regs) = (x))
#define SET_NFLG(regs, x) (NFLG(regs) = (x))
#define SET_VFLG(regs, x) (VFLG(regs) = (x))
#define SET_ZFLG(regs, x) (ZFLG(regs) = (x))
#define SET_XFLG(regs, x) (XFLG(regs) = (x))

#define GET_CFLG(regs) CFLG(regs)
#define GET_NFLG(regs) NFLG(regs)
#define GET_VFLG(regs) VFLG(regs)
#define GET_ZFLG(regs) ZFLG(regs)
#define GET_XFLG(regs) XFLG(regs)

#define CLEAR_CZNV(regs) do { \
 SET_CFLG (regs, 0); \
 SET_ZFLG (regs, 0); \
 SET_NFLG (regs, 0); \
 SET_VFLG (regs, 0); \
} while (0)

#define COPY_CARRY(regs) (SET_XFLG (regs, GET_CFLG (regs)))
#endif

extern const int areg_byteinc[];
extern const int imm8_table[];

extern int movem_index1[256];
extern int movem_index2[256];
extern int movem_next[256];

#ifdef FPUEMU
extern int fpp_movem_index1[256];
extern int fpp_movem_index2[256];
extern int fpp_movem_next[256];
#endif

struct regstruct;

typedef unsigned long cpuop_func (uae_u32, struct regstruct *regs) REGPARAM;
typedef  void cpuop_func_ce (uae_u32, struct regstruct *regs) REGPARAM;

struct cputbl {
    cpuop_func *handler;
    uae_u16 opcode;
};

#ifdef JIT

//Prototype for the comptbl structure to allow us using it in the function type definition
struct comptbl;

//CPU history structure for helping the JIT by using the collected
//information from the interpretive emulation cycles
typedef struct {
  uae_u16* location;
  uaecptr pc;
  uae_u8  cycles;
  uae_u8  specmem;
} cpu_history;

//Compile function for JIT, can be either an address or an instruction compiler
//First parameter is a pointer to the instruction in memory,
//second parameter is pointer to the compiling properties for the instruction (struct comptbl)
typedef void compop_func (const cpu_history*, struct comptbl*) REGPARAM;

//Structure for instruction compiling function parameters
struct comptbl {
    compop_func* instr_handler;	//Instruction compiler handler
    uae_u8 extension;			//Number of extension words
    uae_u8 size;				//Operation size
    uae_u8 srcreg;				//Source register number (or immediate data)
    uae_u8 destreg;				//Destination register number
    uae_u8 src_addr;			//Source addressing mode index for the jump table
    uae_u8 dest_addr;			//Destination addressing mode index for the jump table
    uae_u8 specific;			//Instruction specific additional flags (see COMPTBL_SPEC_* constants)
};

//Instruction description specific flag: not implemented (specific value is 0)
#define COMPTBL_SPEC_ISNOTIMPLEMENTED 0
//Instruction description specific flag: implemented
#define COMPTBL_SPEC_ISIMPLEMENTED 1
//Instruction description specific flag: it is a jump instruction
#define COMPTBL_SPEC_ISJUMP 2
//Instruction description specific flag: it is a constant jump instruction (not conditional)
#define COMPTBL_SPEC_ISCONSTJUMP 4

extern struct comptbl compprops[65536];

extern compop_func* compsrc_pre_func[];
extern compop_func* compdest_pre_func[];
extern compop_func* compsrc_post_func[];
extern compop_func* compdest_post_func[];

STATIC_INLINE int end_block(uae_u16 opcode)
{
	uae_u8 specific = compprops[opcode].specific;
	return (specific & COMPTBL_SPEC_ISJUMP) || ((specific & COMPTBL_SPEC_ISCONSTJUMP) && !currprefs.comp_constjump);
}

/**
 * The number of longwords in the Regs structure that are allocated
 * for the temporary register saving.
 */
#define COMP_REGS_ALLOCATED_SLOTS 4
#endif

extern unsigned long op_illg (uae_u32, struct regstruct *regs) REGPARAM;

typedef char flagtype;

#ifdef FPUEMU
/* You can set this to long double to be more accurate. However, the
   resulting alignment issues will cost a lot of performance in some
   apps */
#define USE_LONG_DOUBLE 0

#if USE_LONG_DOUBLE
typedef long double fptype;
#else
typedef double fptype;
#endif
#endif

extern struct regstruct
{
    uae_u32 regs[16];
    struct flag_struct ccrflags;

    uae_u32 pc;
    uae_u8 *pc_p;
    uae_u8 *pc_oldp;

    uae_u16 irc;
    uae_u16 ir;

    uae_u32 spcflags;

    uaecptr  usp,isp,msp;
    uae_u16 sr;
    flagtype t1;
    flagtype t0;
    flagtype s;
    flagtype m;
    flagtype x;
    flagtype stopped;
    unsigned int intmask;

    uae_u32 vbr;

#ifdef FPUEMU
    fptype fp_result;

    fptype fp[8];

    uae_u32 fpcr,fpsr,fpiar;
    uae_u32 fpsr_highbyte;
#endif

    uae_u32 sfc, dfc;

    uae_u32 kick_mask;
    uae_u32 address_space_mask;

    uae_u8 panic;
    uae_u32 panic_pc, panic_addr;

#ifdef JIT
    uae_u32 jit_cache_inconsistent;
    uae_u32 regslots[COMP_REGS_ALLOCATED_SLOTS];
#endif
} regs, lastint_regs;

struct blockinfo_t;

typedef union {
    cpuop_func* handler;
    struct blockinfo_t* bi;
} cacheline;


STATIC_INLINE uae_u32 munge24 (uae_u32 x)
{
    return x & regs.address_space_mask;
}

STATIC_INLINE void set_special (struct regstruct *regs, uae_u32 x)
{
    regs->spcflags |= x;
    cycles_do_special ();
}

STATIC_INLINE void unset_special (struct regstruct *regs, uae_u32 x)
{
    regs->spcflags &= ~x;
}

#define m68k_dreg(r,num) ((r)->regs[(num)])
#define m68k_areg(r,num) (((r)->regs + 8)[(num)])

STATIC_INLINE void m68k_setpc (struct regstruct *regs, uaecptr newpc)
{
    regs->pc_p = regs->pc_oldp = get_real_address (newpc);
    regs->pc   = newpc;
}

STATIC_INLINE uaecptr m68k_getpc (struct regstruct *regs)
{
    return regs->pc + ((char *)regs->pc_p - (char *)regs->pc_oldp);
}

STATIC_INLINE uaecptr m68k_getpc_p (struct regstruct *regs, uae_u8 *p)
{
    return regs->pc + ((char *)p - (char *)regs->pc_oldp);
}

#define m68k_incpc(regs, o) ((regs)->pc_p += (o))

STATIC_INLINE void m68k_do_rts (struct regstruct *regs)
{
    m68k_setpc (regs, get_long (m68k_areg (regs, 7)));
    m68k_areg (regs, 7) += 4;
}

STATIC_INLINE void m68k_do_bsr (struct regstruct *regs, uaecptr oldpc, uae_s32 offset)
{
    m68k_areg (regs, 7) -= 4;
    put_long (m68k_areg (regs, 7), oldpc);
    m68k_incpc (regs, offset);
}

STATIC_INLINE void m68k_do_jsr (struct regstruct *regs, uaecptr oldpc, uaecptr dest)
{
    m68k_areg (regs, 7) -= 4;
    put_long (m68k_areg (regs, 7), oldpc);
    m68k_setpc (regs, dest);
}

#define get_ibyte(regs, o) do_get_mem_byte((uae_u8 *) ((regs)->pc_p + (o) + 1))
#define get_iword(regs, o) do_get_mem_word((uae_u16 *)((regs)->pc_p + (o)))
#define get_ilong(regs, o) do_get_mem_long((uae_u32 *)((regs)->pc_p + (o)))

/* These are only used by the 68020/68881 code, and therefore don't
 * need to handle prefetch.  */
STATIC_INLINE uae_u32 next_ibyte (struct regstruct *regs)
{
    uae_u32 r = get_ibyte (regs, 0);
    m68k_incpc (regs, 2);
    return r;
}

STATIC_INLINE uae_u32 next_iword (struct regstruct *regs)
{
    uae_u32 r = get_iword (regs, 0);
    m68k_incpc (regs, 2);
    return r;
}

STATIC_INLINE uae_u32 next_ilong (struct regstruct *regs)
{
    uae_u32 r = get_ilong (regs, 0);
    m68k_incpc (regs, 4);
    return r;
}


STATIC_INLINE void m68k_setstopped (struct regstruct *regs, int stop)
{
    regs->stopped = stop;
    /* A traced STOP instruction drops through immediately without
       actually stopping.  */
    if (stop && (regs->spcflags & SPCFLAG_DOTRACE) == 0)
	set_special (regs, SPCFLAG_STOP);
}

extern uae_u32 get_disp_ea_020 (struct regstruct *regs, uae_u32 base, uae_u32 dp) REGPARAM;
extern uae_u32 get_disp_ea_000 (struct regstruct *regs, uae_u32 base, uae_u32 dp) REGPARAM;

/* Hack to stop conflict with AROS Exception function */
#ifdef __AROS__
# undef Exception
#endif

extern void MakeSR (struct regstruct *regs) REGPARAM;
extern void MakeFromSR (struct regstruct *regs) REGPARAM;
extern void Exception (int, struct regstruct *regs, uaecptr) REGPARAM;
extern void Interrupt (unsigned int level);
extern void dump_counts (void);
extern int m68k_move2c (int, uae_u32 *);
extern int m68k_movec2 (int, uae_u32 *);
extern void m68k_divl (uae_u32, uae_u32, uae_u16, uaecptr);
extern void m68k_mull (uae_u32, uae_u32, uae_u16);
extern void init_m68k (void);
extern void init_m68k_full (void);
extern void m68k_go (int);
extern void m68k_dumpstate (void *, uaecptr *);
extern void m68k_disasm (void *, uaecptr, uaecptr *, int);
extern void m68k_disasm_ea (void *f, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr);
void m68k_disasm_str (char* str, uaecptr addr, uaecptr *nextpc, int cnt);
extern void sm68k_disasm(char *, char *, uaecptr addr, uaecptr *nextpc);
extern void m68k_reset (void);
extern int getDivu68kCycles(uae_u32 dividend, uae_u16 divisor);
extern int getDivs68kCycles(uae_s32 dividend, uae_s16 divisor);

extern void mmu_op       (uae_u32, struct regstruct *regs, uae_u16);

extern void fpp_opp      (uae_u32, struct regstruct *regs, uae_u16);
extern void fdbcc_opp    (uae_u32, struct regstruct *regs, uae_u16);
extern void fscc_opp     (uae_u32, struct regstruct *regs, uae_u16);
extern void ftrapcc_opp  (uae_u32, struct regstruct *regs, uaecptr);
extern void fbcc_opp     (uae_u32, struct regstruct *regs, uaecptr, uae_u32);
extern void fsave_opp    (uae_u32, struct regstruct *regs);
extern void frestore_opp (uae_u32, struct regstruct *regs);
extern uae_u32 fpp_get_fpsr (const struct regstruct *regs);

extern void exception3 (uae_u32 opcode, uaecptr addr, uaecptr fault);
extern void exception3i (uae_u32 opcode, uaecptr addr, uaecptr fault);
extern void exception2 (uaecptr addr, uaecptr fault);
extern void cpureset (void);

extern void fill_prefetch_slow (struct regstruct *regs);

STATIC_INLINE int notinrom (void)
{
    if (munge24 (m68k_getpc (&regs)) < 0xe0000)
	return 1;
    return 0;
}

#define CPU_OP_NAME(a) op ## a

/* 68040 */
extern const struct cputbl op_smalltbl_0_ff[];
/* 68020 + 68881 */
extern const struct cputbl op_smalltbl_1_ff[];
/* 68020 */
extern const struct cputbl op_smalltbl_2_ff[];
/* 68010 */
extern const struct cputbl op_smalltbl_3_ff[];
/* 68000 */
extern const struct cputbl op_smalltbl_4_ff[];
/* 68000 slow but compatible.  */
extern const struct cputbl op_smalltbl_5_ff[];
/* 68000 slow but compatible and cycle-exact.  */
extern const struct cputbl op_smalltbl_6_ff[];

extern cpuop_func *cpufunctbl[65536] ASM_SYM_FOR_FUNC ("cpufunctbl");


/* Flags for Bernie during development/debugging. Should go away eventually */
#define DISTRUST_CONSISTENT_MEM 0
#define TAGMASK 0x000fffff
#define TAGSIZE (TAGMASK+1)
#define MAXRUN 1024

extern uae_u8* start_pc_p;
extern uae_u32 start_pc;

#define cacheline(x) (((uae_uintptr) x) & TAGMASK)

void newcpu_showstate (void);

#ifdef JIT
extern void flush_icache (int n);
extern void compemu_reset (void);
extern void compemu_cleanup (void);
#else
#define flush_icache(X) do {} while (0)
#endif
