#ifdef JIT

/* Margin for one instruction compiled code */
#define BYTES_PER_INST 10240

/**
 * The maximum size we calculate checksums for.
 * Anything larger will be flushed unconditionally
 */
#define MAX_CHECKSUM_LEN 2048

/* Number of prepared blockinfo for a compiled block */
#define MAX_HOLD_BI 3  /* One for the current block, and up to two
			  for jump targets */

/* REMARK: No idea why we need this magic with the cycles scaling... */
#define SCALE 2
#define MAXCYCLES (1000 * CYCLE_UNIT)
#define scaled_cycles(x) (currprefs.m68k_speed==-1?(((x)/SCALE)?(((x)/SCALE<MAXCYCLES?((x)/SCALE):MAXCYCLES)):1):(x))


/* Functions exposed to newcpu, or to what was moved from newcpu.c to
 * compemu_support.c */
extern void comp_init(void);
extern void comp_done(void);
extern void build_comp(void);
extern void compile_block(const cpu_history *pc_hist, int blocklen, int totcyles);

extern cacheline cache_tags[];

/* Preferences handling */
void check_prefs_changed_comp(void);

struct blockinfo_t;

typedef struct blockinfo_t
{
	uae_s32 count;				/* Block usage counter */
	cpuop_func* handler;		/* Pointer to the compiled code */
	cpuop_func* handler_to_use;	/* Pointer to the execution/compilation handler */
	uae_u8* pc_p;				/* Pointer to the first instruction */

	uae_u32 len;				/* Block length (in 68k memory) */
	uae_u32 c1;					/* Code checksum 1 */
	uae_u32 c2;					/* Code checksum 2 */

	int dormant;				/* Dormant block if true (no need to calculate checksum) */

	struct blockinfo_t* next_same_cl;		/* Cache line pointer */
	struct blockinfo_t** prev_same_cl_p;	/* Cache line pointer */
} blockinfo;

/* PowerPC register layout
 * SysV ABI assumed, might change for other ABIs
 * See documentation:
 * SVR4 ABI for the Power PC
 * at
 * http://www.nondot.org/sabre/os/articles/ProcessorArchitecture
 *
 * Volatile registers:
 *    r0, r3-r12, r13 (we don't use small data),
 *    f0-f13
 *    XER, CTR, CR0-1, CR5-7, LR
 * Non-volatile registers:
 *    r1 (stack),
 *    r14-r30,
 *    f14-f31, FPSCR (let's preserve it)
 * Non-usable registers:
 *    r2 (rtoc),
 *    r31 (reserved for possible OS use),
 *    CR2-4
 */

#define	PPCR_SPECTMP	0	// r0 - special temporary register, cannot be used for every operation
#define	PPCR_SP			1	// r1 - stack pointer
#define	PPCR_RTOC		2	// r2 - rtoc register, must be preserved

/* PowerPC function call parameter registers */
#define PPCR_PARAM1		3	// r3
#define PPCR_PARAM2		4	// r4
#define PPCR_PARAM3		5	// r5


/* Compiled function parameters */
#define PPCR_CONTEXT	PPCR_PARAM1	// r3 - helper context pointer
#define M68KR_PC		PPCR_PARAM2	// r4 - 68k program counter
#define M68KR_FLAGS		PPCR_PARAM3	// r5 - 68k flags, flag layout: XNZVC (X is at bit 4, C is at bit 0)

/* Temporary registers, free to use, no need to preserve */
#define PPCR_TMP0	6	// r6
#define PPCR_TMP1	7	// r7
#define PPCR_TMP2	8	// r8
#define PPCR_TMP3	9	// r9
#define PPCR_TMP4	10	// r10
#define PPCR_TMP5	11	// r11
#define PPCR_TMP6	12	// r12
#define PPCR_TMP7	13	// r13

#define PPCR_CR_TMP0	0	//CR0
#define PPCR_CR_TMP1	1	//CR1

/* Conditional branch BO and BI operands (together)
 * Please note: stupid old GCC does not allow binary constants. Pff. */
#define PPC_B_CR_TMP0_LT	0x0180	//0b0110000000
#define PPC_B_CR_TMP0_GT	0x0181	//0b0110000001
#define PPC_B_CR_TMP0_EQ	0x0182	//0b0110000010
#define PPC_B_CR_TMP0_NE	0x0080 	//0b0010000000

//These two constants for hinting the branch if it was more likely taken or not
#define PPC_B_TAKEN			0x0020	//0b0000100000
#define PPC_B_NONTAKEN		0x0000	//0b0000000000

/* Some function protos */
STATIC_INLINE blockinfo* get_blockinfo(uae_u32 cl);
STATIC_INLINE blockinfo* get_blockinfo_addr_new(void* addr, int setstate);
STATIC_INLINE void alloc_blockinfos(void);
STATIC_INLINE blockinfo* get_blockinfo_addr(void* addr);
STATIC_INLINE void raise_in_cl_list(blockinfo* bi);
STATIC_INLINE void compile_return_block(void);
STATIC_INLINE void reset_lists(void);
static void calc_checksum(blockinfo* bi, uae_u32* c1, uae_u32* c2);
static void cache_miss(void);

unsigned long execute_normal_callback(uae_u32 ignored1, struct regstruct *ignored2);
unsigned long exec_nostats_callback(uae_u32 ignored1, struct regstruct *ignored2);
unsigned long do_nothing_callback(uae_u32 ignored1, struct regstruct *ignored2);

void writebyte(int address, int source, int tmp);
void writeword_general(int address, int source, int tmp);
void writelong_general(int address, int source, int tmp);
void readbyte(int address, int dest, int tmp);
void readword(int address, int dest, int tmp);
void readlong(int address, int dest, int tmp);

void comp_not_implemented(uae_u16 opcode);


/* PowerPC instruction compiler functions */
STATIC_INLINE void comp_ppc_branch_target(void);
STATIC_INLINE void comp_ppc_bc(int bibo);
STATIC_INLINE void comp_ppc_blr(void);
STATIC_INLINE void comp_ppc_blrl(void);
STATIC_INLINE void comp_ppc_cmplw(int regcrfd, int rega, int regb);
STATIC_INLINE void comp_ppc_liw(int reg, uae_u32 value);
STATIC_INLINE void comp_ppc_or(int rega, int regs, int regb, BOOL updateflags);
STATIC_INLINE void comp_ppc_ori(int rega, int regs, uae_u16 imm);
STATIC_INLINE void comp_ppc_trap(void);

STATIC_INLINE void comp_ppc_call(int reg, uae_uintptr addr);
STATIC_INLINE void comp_ppc_prolog(void);
STATIC_INLINE void comp_ppc_epilog(void);
STATIC_INLINE void comp_ppc_return_to_caller(void);
STATIC_INLINE void comp_ppc_do_cycles(int totalcycles);
STATIC_INLINE void comp_ppc_verify_pc(uae_u8* pc_addr_exp, uae_u8** pc_addr_act);
STATIC_INLINE void comp_ppc_reload_pc_p(uae_u8* new_pc_p, uae_u8** regs_pc_p);
#endif

/* Some more function protos */
extern void flush_icache(int n);
extern void flush_icache_hard(const char* callsrc);
extern void set_cache_state(int enabled);

void execute_normal(void);
void exec_nostats(void);
void do_nothing(void);
void do_cycles_callback(unsigned int cycles_to_add);
