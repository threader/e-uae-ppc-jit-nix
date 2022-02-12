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
extern uae_u16* comp_current_m68k_location(void);

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
 *    CR2-4 (NOTE: actually, CR2 is used by the interpretive)
 */

#define	PPCR_SPECTMP	0	// r0 - special temporary register, cannot be used for every operation
#define	PPCR_SP			1	// r1 - stack pointer
#define	PPCR_RTOC		2	// r2 - rtoc register, must be preserved

/* PowerPC function call parameter registers
 * Please note: the parameter registers are reused as temporary registers */
#define PPCR_PARAM1		3	// r3
#define PPCR_PARAM2		4	// r4
#define PPCR_PARAM3		5	// r5

/* Temporary registers, free to use, no need to preserve */
#define PPCR_TMP0	3	// r3 - do not change this mapping
#define PPCR_TMP1	4	// r4 - do not change this mapping
#define PPCR_TMP2	5	// r5 - do not change this mapping
#define PPCR_TMP3	6	// r6
#define PPCR_TMP4	7	// r7
#define PPCR_TMP5	8	// r8
#define PPCR_TMP6	9	// r9
#define PPCR_TMP7	10	// r10
#define PPCR_TMP8	11	// r11
#define PPCR_TMP9	12	// r12
#define PPCR_TMP10	13	// r13

/* Regs structure base pointer register
 * Note: A non-volatile register was chosen to avoid the reloading of
 * this register when an external function is called. */
#define PPCR_REGS_BASE	14	// r14

/* M68k flags register: while the flags are emulated those are stored
 * in this register, the layout is detailed in PPCR_FLAG_* constants.
 * IMPORTANT: the state of the other bits are not defined, at every usage of
 * the flags the other bits must be masked out. */
#define PPCR_FLAGS	15	// r15

#define PPCR_CR_TMP0	0	//CR0
#define PPCR_CR_TMP1	1	//CR1
#define PPCR_CR_TMP2	2	//CR2 - NOTE: this supposed to be preserved, but the interpretive emulator is using it already

/* Conditional branch BO and BI operands (together)
 * Please note: stupid old GCC does not allow binary constants. Pff. */
#define PPC_B_CR_TMP0_LT	0x0180	//0b0110000000
#define PPC_B_CR_TMP0_GT	0x0181	//0b0110000001
#define PPC_B_CR_TMP0_EQ	0x0182	//0b0110000010
#define PPC_B_CR_TMP0_NE	0x0080 	//0b0010000000

//These two constants for hinting the branch if it was more likely taken or not
#define PPC_B_TAKEN			0x0020	//0b0000100000
#define PPC_B_NONTAKEN		0x0000	//0b0000000000

/* Not used temporary register */
#define PPC_TMP_REG_NOTUSED -1
/* Allocated temporary register that is not mapped to an M68k register */
#define PPC_TMP_REG_ALLOCATED -2

/* Emulated M68k flag bit definitions for PPCR_FLAGS register:
 * We assume the PPC GCC environment, the flags are the same
 * except X flag which is stored in the same register but
 * shifted to the right by 16 bits (lower half word, same position). */
//N = 31
#define PPCR_FLAG_N	FLAGBIT_N
//Z = 29
#define PPCR_FLAG_Z	FLAGBIT_Z
//V = 22
#define PPCR_FLAG_V	FLAGBIT_V
//C = 21
#define PPCR_FLAG_C	FLAGBIT_C
//X = 5
#define PPCR_FLAG_X	(FLAGBIT_X - 16)

#define FLAGBIT_N	31
#define FLAGBIT_Z	29
#define FLAGBIT_V	22
#define FLAGBIT_C	21
#define FLAGBIT_X	21

/* Convert PPC register number to bit number in a longword
 * This macro can be used to mark registers for Prolog/Epilog compiling */
#define PPCR_REG_BIT(x) (1 << (x))

/* The used non-volatile registers in a bit masp for saving/restoring */
#define PPCR_REG_USED_NONVOLATILE	(PPCR_REG_BIT(PPCR_REGS_BASE) | PPCR_REG_BIT(PPCR_FLAGS))

/* The number of additional longwords in the stackframe that are allocated
 * for the temporary register saving.
 */
#define COMP_STACKFRAME_ALLOCATED_SLOTS 3

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

int comp_is_spec_memory_read_byte(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_read_word(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_read_long(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_byte(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_word(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_long(uae_u32 pc, int specmem_flags);

void comp_not_implemented(uae_u16 opcode);
uae_u8 comp_allocate_temp_register(int allocate_for);
void comp_free_temp_register(uae_u8 temp_reg);
uae_u8 comp_map_temp_register(uae_u8 reg_number, int needs_init, int needs_flush);
void comp_unmap_temp_register(uae_u8 reg_number);
void comp_flush_temp_registers(int supresswarning);
uae_u8 comp_get_gpr_for_temp_register(uae_u8 tmpreg);

/* PowerPC instruction compiler functions */
void comp_ppc_add(int regd, int rega, int regb, int updateflags);
void comp_ppc_addco(int regd, int rega, int regb, int updateflags);
void comp_ppc_addi(int regd, int rega, uae_u16 imm);
void comp_ppc_addis(int regd, int rega, uae_u16 imm);
void comp_ppc_and(int rega, int regs, int regb, int updateflags);
void comp_ppc_andi(int rega, int regs, uae_u16 imm);
void comp_ppc_andis(int rega, int regs, uae_u16 imm);
void comp_ppc_branch_target(void);
void comp_ppc_b(uae_u32 target);
void comp_ppc_bc(int bibo);
void comp_ppc_bl(uae_u32 target);
void comp_ppc_blr(void);
void comp_ppc_blrl(void);
void comp_ppc_cmplw(int regcrfd, int rega, int regb);
void comp_ppc_extsb(int rega, int regs, int updateflags);
void comp_ppc_extsh(int rega, int regs, int updateflags);
void comp_ppc_lbz(int regd, uae_u16 delta, int rega);
void comp_ppc_lha(int regd, uae_u16 delta, int rega);
void comp_ppc_lhz(int regd, uae_u16 delta, int rega);
void comp_ppc_li(int rega, uae_u16 imm);
void comp_ppc_lis(int rega, uae_u16 imm);
void comp_ppc_liw(int reg, uae_u32 value);
void comp_ppc_lwz(int regd, uae_u16 delta, int rega);
void comp_ppc_lwzx(int regd, int rega, int regb);
void comp_ppc_mcrxr(int crreg);
void comp_ppc_mfcr(int reg);
void comp_ppc_mflr(int reg);
void comp_ppc_mfxer(int reg);
void comp_ppc_mr(int rega, int regs, int updateflags);
void comp_ppc_mtlr(int reg);
void comp_ppc_nop(void);
void comp_ppc_or(int rega, int regs, int regb, int updateflags);
void comp_ppc_ori(int rega, int regs, uae_u16 imm);
void comp_ppc_oris(int rega, int regs, uae_u16 imm);
void comp_ppc_rlwimi(int rega, int regs, int shift, int maskb, int maske, int updateflags);
void comp_ppc_rlwinm(int rega, int regs, int shift, int maskb, int maske, int updateflags);
void comp_ppc_trap(void);
void comp_ppc_sth(int regs, uae_u16 delta, int rega);
void comp_ppc_stb(int regs, uae_u16 delta, int rega);
void comp_ppc_sthu(int regs, uae_u16 delta, int rega);
void comp_ppc_stw(int regs, uae_u16 delta, int rega);

void* comp_ppc_buffer_top(void);
void comp_ppc_emit_halfwords(uae_u16 halfword_high, uae_u16 halfword_low);
void comp_ppc_emit_word(uae_u32 word);
void comp_ppc_call(int reg, uae_uintptr addr);
void comp_ppc_call_reg(int addrreg);
void comp_ppc_jump(uae_uintptr addr);
void comp_ppc_prolog(uae_u32 save_regs);
void comp_ppc_epilog(uae_u32 restore_regs);
void comp_ppc_return_to_caller(uae_u32 restore_regs);
void comp_ppc_do_cycles(int totalcycles);
void comp_ppc_verify_pc(uae_u8* pc_addr_exp);
void comp_ppc_reload_pc_p(uae_u8* new_pc_p);
#endif

/* Some more function protos */
extern void flush_icache(int n);
extern void flush_icache_hard(const char* callsrc);
extern void set_cache_state(int enabled);

void execute_normal(void);
void exec_nostats(void);
void do_nothing(void);
void do_cycles_callback(unsigned int cycles_to_add);
