#ifdef JIT

/* If BOOL is not defined then define it here using int */
#ifndef BOOL
#define BOOL int
#endif

/* Margin for one block of compiled code */
#define BYTES_PER_BLOCK 10240

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
extern void comp_compile_error(void);
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

	struct blockinfo_t* next_same_cl;		/* Cache line pointer */
	struct blockinfo_t** prev_same_cl_p;	/* Cache line pointer */

    struct blockinfo_t* next;		/* Pointer to the next block in active/dormant list */
    struct blockinfo_t* prev;		/* Pointer to the previous block in active/dormant list */
} blockinfo;

/* Mapped PowerPC register type
 * We have to use struct for a custom data type, otherwise it will be automatically
 * converted to the required type. tsk... tsk... */
typedef struct comp_ppc_reg_t
{
	uae_u8 r;	//The number of the mapped PowerPC register as integer
} comp_ppc_reg;

/* Temporary register mapping descriptor structure */
typedef struct comp_tmp_reg_t
{
	uae_u64 reg_usage_mapping;		/* Temporary register usage mapping, see register mapping for macroblocks in compemu_compiler.h */
	struct m68k_register* allocated_for;			/* Pointer to the emulated 68K register which is linked to this temporary register, or null if there is no M68k register linked. */
	BOOL allocated;					/* If TRUE then the temporary register is allocated at the moment */
	comp_ppc_reg mapped_reg_num;	/* Mapped physical register for the temporary register */
} comp_tmp_reg;

/* Data structure for Exception triggering */
typedef struct comp_exception_data_t
{
	uae_u32 next_address;			//Next instruction address
	uae_u32 next_location;			//Pointer to next instruction in memory
	uae_s8 mapped_regs[16];			//List of mapped registers to be flushed
} comp_exception_data;

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

#ifdef __APPLE__
#warning Assuming Darwin PowerPC ABI
#else
#warning Assuming SysV PowerPC ABI
#endif

/* Convert a PowerPC register number to mapped register typed data */
#define PPCR_MAPPED_REG(x) ((comp_ppc_reg){.r = (x)})

#define	PPCR_SPECTMP	0	// r0 - special temporary register, cannot be used for every operation
#define	PPCR_SP			1	// r1 - stack pointer

//Mapped registers
#define PPCR_SPECTMP_MAPPED PPCR_MAPPED_REG(PPCR_SPECTMP)
#define	PPCR_SP_MAPPED PPCR_MAPPED_REG(PPCR_SP)

#ifndef __APPLE__
#define	PPCR_RTOC		2	// r2 - rtoc register, must be preserved
#endif

/* PowerPC function call parameter registers
 * Please note: the parameter registers are reused as temporary registers */
#define PPCR_PARAM1		3	// r3
#define PPCR_PARAM2		4	// r4
#define PPCR_PARAM3		5	// r5

//Mapped registers
#define PPCR_PARAM1_MAPPED PPCR_MAPPED_REG(PPCR_PARAM1)
#define PPCR_PARAM2_MAPPED PPCR_MAPPED_REG(PPCR_PARAM2)
#define PPCR_PARAM3_MAPPED PPCR_MAPPED_REG(PPCR_PARAM3)

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
#ifndef __APPLE__
#define PPCR_TMP10	13	// r13
#else
#define PPCR_TMP10	2	// r2
#endif

//Mapped registers
#define PPCR_TMP0_MAPPED PPCR_MAPPED_REG(PPCR_TMP0)
#define PPCR_TMP1_MAPPED PPCR_MAPPED_REG(PPCR_TMP1)
#define PPCR_TMP2_MAPPED PPCR_MAPPED_REG(PPCR_TMP2)
#define PPCR_TMP3_MAPPED PPCR_MAPPED_REG(PPCR_TMP3)
#define PPCR_TMP4_MAPPED PPCR_MAPPED_REG(PPCR_TMP4)
#define PPCR_TMP5_MAPPED PPCR_MAPPED_REG(PPCR_TMP5)
#define PPCR_TMP6_MAPPED PPCR_MAPPED_REG(PPCR_TMP6)
#define PPCR_TMP7_MAPPED PPCR_MAPPED_REG(PPCR_TMP7)
#define PPCR_TMP8_MAPPED PPCR_MAPPED_REG(PPCR_TMP8)
#define PPCR_TMP9_MAPPED PPCR_MAPPED_REG(PPCR_TMP9)
#define PPCR_TMP10_MAPPED PPCR_MAPPED_REG(PPCR_TMP10)

/* Regs structure base pointer register
 * Note: A non-volatile register was chosen to avoid the reloading of
 * this register when an external function is called. */
#ifndef __APPLE__
#define PPCR_REGS_BASE	14	// r14
#else
#define PPCR_REGS_BASE	13	// r13
#endif

//Mapped registers
#define PPCR_REGS_BASE_MAPPED PPCR_MAPPED_REG(PPCR_REGS_BASE)

/* M68k flags register: while the flags are emulated those are stored
 * in this register, the layout is detailed in PPCR_FLAG_* constants.
 * IMPORTANT: the state of the other bits are not defined, at every usage of
 * the flags the other bits must be masked out. */
#ifndef __APPLE__
#define PPCR_FLAGS	15	// r15
#else
#define PPCR_FLAGS	14	// r14
#endif

//Mapped registers
#define PPCR_FLAGS_MAPPED PPCR_MAPPED_REG(PPCR_FLAGS)

/* Non-volatile registers, values are preserved while the execution leaves
 * the translated code.
 * IMPORTANT: these registers are not saved automatically in the prolog/epilog
 * functions for the translated code chunk. Save these registers using
 * comp_macroblock_push_save_register_to_context() function before use and restore
 * by calling comp_macroblock_push_restore_register_from_context() before the execution leaves
 * the compiled block.
 */
#ifndef __APPLE__
#define PPCR_TMP_NONVOL0	16	// r16
#define PPCR_TMP_NONVOL1	17	// r17
#else
#define PPCR_TMP_NONVOL0	15	// r15
#define PPCR_TMP_NONVOL1	16	// r16
#endif

//Mapped registers
#define PPCR_TMP_NONVOL0_MAPPED PPCR_MAPPED_REG(PPCR_TMP_NONVOL0)
#define PPCR_TMP_NONVOL1_MAPPED PPCR_MAPPED_REG(PPCR_TMP_NONVOL1)

/* Temporary CR registers
 */
#define PPCR_CR_TMP0	0	//CR0
#define PPCR_CR_TMP1	1	//CR1
#define PPCR_CR_TMP2	2	//CR2 - NOTE: this supposed to be preserved, but the interpretive emulator is using it already

/* Conditional branch BO and BI operands (together)
 * Please note: stupid old GCC does not allow binary constants. Pff. */
#define PPC_B_CR_TMP0_LT	0x0180	//0b0110000000
#define PPC_B_CR_TMP1_LT	0x0184	//0b0110000100
#define PPC_B_CR_TMP0_GT	0x0181	//0b0110000001
#define PPC_B_CR_TMP1_GT	0x0185	//0b0110000101
#define PPC_B_CR_TMP0_EQ	0x0182	//0b0110000010
#define PPC_B_CR_TMP1_EQ	0x0186	//0b0110000110
#define PPC_B_CR_TMP0_NE	0x0082 	//0b0010000010
#define PPC_B_CR_TMP1_NE	0x0086 	//0b0010000110

/**
 * Not used temporary register
 * Note: must be a negative number
 */
#define PPC_TMP_REG_NOTUSED -1
#define PPC_TMP_REG_NOTUSED_MAPPED PPCR_MAPPED_REG(PPC_TMP_REG_NOTUSED)

/**
 * Allocated temporary register that is not mapped to an M68k register
 * Note: must be a negative number
 */
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

//Bit numbers for the flags in the PPCR_FLAGS register (reversed for PPC)
#define PPC_FLAGBIT_N	(31 - FLAGBIT_N)
#define PPC_FLAGBIT_Z	(31 - FLAGBIT_Z)
#define PPC_FLAGBIT_V	(31 - FLAGBIT_V)
#define PPC_FLAGBIT_C	(31 - FLAGBIT_C)
#define PPC_FLAGBIT_X	(31 - (FLAGBIT_X - 16))

/* Convert PPC register number to bit number in a longword
 * This macro can be used to mark registers for Prolog/Epilog compiling */
#define PPCR_REG_BIT(x) (1 << (x))

/* The used non-volatile registers in a bit masp for saving/restoring */
#define PPCR_REG_USED_NONVOLATILE	(PPCR_REG_BIT(PPCR_REGS_BASE) | PPCR_REG_BIT(PPCR_FLAGS))

/* The number of used non-volatile registers for saving/restoring */
#define PPCR_REG_USED_NONVOLATILE_NUM 2

/* Some function protos */
STATIC_INLINE blockinfo* get_blockinfo(uae_u32 cl);
STATIC_INLINE blockinfo* get_blockinfo_addr_new(void* addr, int setstate);
STATIC_INLINE void alloc_blockinfos(void);
STATIC_INLINE blockinfo* get_blockinfo_addr(void* addr);
STATIC_INLINE void raise_in_cl_list(blockinfo* bi);
STATIC_INLINE void add_to_active(blockinfo* bi);
STATIC_INLINE void add_to_dormant(blockinfo* bi);
STATIC_INLINE void add_blockinfo_to_list(blockinfo* list, blockinfo* bi);
STATIC_INLINE void remove_blockinfo_from_list(blockinfo* bi);
STATIC_INLINE void compile_return_block(void);
STATIC_INLINE void reset_lists(void);
static void calc_checksum(blockinfo* bi, uae_u32* c1, uae_u32* c2);
static void cache_miss(void);
int check_for_cache_miss(void);

unsigned long execute_normal_callback(uae_u32 ignored1, struct regstruct *ignored2);
unsigned long exec_nostats_callback(uae_u32 ignored1, struct regstruct *ignored2);
unsigned long check_checksum_callback(uae_u32 ignored1, struct regstruct *ignored2);
unsigned long do_nothing_callback(uae_u32 ignored1, struct regstruct *ignored2);

int comp_is_spec_memory_read_byte(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_read_word(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_read_long(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_byte(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_word(uae_u32 pc, int specmem_flags);
int comp_is_spec_memory_write_long(uae_u32 pc, int specmem_flags);

void comp_not_implemented(uae_u16 opcode);
comp_tmp_reg* comp_allocate_temp_register(struct m68k_register* allocate_for, comp_ppc_reg preferred);
void comp_free_temp_register(comp_tmp_reg* temp_reg);
comp_tmp_reg* comp_map_temp_register(uae_u8 reg_number, int needs_init, int needs_flush);
void comp_unmap_temp_register(struct m68k_register* reg);
void comp_swap_temp_register_mapping(comp_tmp_reg* tmpreg1, comp_tmp_reg* tmpreg2);
comp_tmp_reg* comp_get_mapped_temp_register(uae_u8 reg_number);
void comp_reset_temp_registers(void);
void comp_flush_temp_registers(int supresswarning);
void comp_get_changed_mapped_regs_list(uae_s8* mapped_regs);
void comp_unlock_all_temp_registers(void);
int comp_next_free_register_slot(void);
int comp_last_register_slot(void);
void comp_dump_reg_usage(uae_u64 regs, char* str, char dump_control);
int comp_unsigned_divide_64_bit(uae_u32 divisor, uae_u32 diviend_high_regnum, uae_u32 dividend_low_regnum);
int comp_signed_divide_64_bit(uae_s32 divisor,  uae_u32 diviend_high_regnum, uae_u32 dividend_low_regnum);

/* PowerPC instruction compiler functions */
void comp_ppc_add(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_addc(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_addco(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_addeo(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_addi(comp_ppc_reg regd, comp_ppc_reg rega, uae_u16 imm);
void comp_ppc_addis(comp_ppc_reg regd, comp_ppc_reg rega, uae_u16 imm);
void comp_ppc_and(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_andc(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_andi(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_andis(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_branch_target(int reference);
void comp_ppc_b(uae_u32 target, int reference);
void comp_ppc_bc(int bibo, int reference);
void comp_ppc_bctr(void);
void comp_ppc_bl(uae_u32 target);
void comp_ppc_blr(void);
void comp_ppc_blrl(void);
void comp_ppc_cmplw(int regcrfd, comp_ppc_reg rega, comp_ppc_reg regb);
void comp_ppc_cmplwi(int regcrfd, comp_ppc_reg rega, uae_u16 imm);
void comp_ppc_cntlzw(comp_ppc_reg rega, comp_ppc_reg regs, BOOL updateflags);
void comp_ppc_divwo(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_divwuo(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_extsb(comp_ppc_reg rega, comp_ppc_reg regs, BOOL updateflags);
void comp_ppc_extsh(comp_ppc_reg rega, comp_ppc_reg regs, BOOL updateflags);
void comp_ppc_lbz(comp_ppc_reg regd, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_lha(comp_ppc_reg regd, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_lhz(comp_ppc_reg regd, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_li(comp_ppc_reg rega, uae_u16 imm);
void comp_ppc_lis(comp_ppc_reg rega, uae_u16 imm);
void comp_ppc_liw(comp_ppc_reg reg, uae_u32 value);
void comp_ppc_lwz(comp_ppc_reg regd, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_lwzx(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb);
void comp_ppc_mcrxr(int crreg);
void comp_ppc_mfcr(comp_ppc_reg reg);
#ifdef _ARCH_PWR4
void comp_ppc_mfocrf(int crreg, comp_ppc_reg reg);
#endif
void comp_ppc_mflr(comp_ppc_reg reg);
void comp_ppc_mfxer(comp_ppc_reg reg);
void comp_ppc_mr(comp_ppc_reg rega, comp_ppc_reg regs, BOOL updateflags);
void comp_ppc_mtcrf(int crreg, comp_ppc_reg regf);
void comp_ppc_mtctr(comp_ppc_reg reg);
void comp_ppc_mtlr(comp_ppc_reg reg);
void comp_ppc_mtxer(comp_ppc_reg reg);
void comp_ppc_mulhw(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_mulhwu(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_mullw(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_mullwo(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_neg(comp_ppc_reg regd, comp_ppc_reg rega, BOOL updateflags);
void comp_ppc_nego(comp_ppc_reg regd, comp_ppc_reg rega, BOOL updateflags);
void comp_ppc_nop(void);
void comp_ppc_nor(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_or(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_orc(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_ori(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_oris(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_rlwimi(comp_ppc_reg rega, comp_ppc_reg regs, int shift, int maskb, int maske, BOOL updateflags);
void comp_ppc_rlwinm(comp_ppc_reg rega, comp_ppc_reg regs, int shift, int maskb, int maske, BOOL updateflags);
void comp_ppc_rlwnm(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, int maskb, int maske, BOOL updateflags);
void comp_ppc_slw(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_sraw(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_srawi(comp_ppc_reg rega, comp_ppc_reg regs, int shift, BOOL updateflags);
void comp_ppc_srw(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_stb(comp_ppc_reg regs, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_sth(comp_ppc_reg regs, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_sthu(comp_ppc_reg regs, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_stw(comp_ppc_reg regs, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_stwu(comp_ppc_reg regs, uae_u16 delta, comp_ppc_reg rega);
void comp_ppc_subf(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_subfco(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_subfc(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_subfeo(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_subfe(comp_ppc_reg regd, comp_ppc_reg rega, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_subfic(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_trap(void);
void comp_ppc_xor(comp_ppc_reg rega, comp_ppc_reg regs, comp_ppc_reg regb, BOOL updateflags);
void comp_ppc_xori(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);
void comp_ppc_xoris(comp_ppc_reg rega, comp_ppc_reg regs, uae_u16 imm);

void* comp_ppc_buffer_top(void);
void comp_ppc_emit_halfwords(uae_u16 halfword_high, uae_u16 halfword_low);
void comp_ppc_emit_word(uae_u32 word);
int comp_ppc_check_top(void);
void comp_ppc_call(comp_ppc_reg reg, uae_uintptr addr);
void comp_ppc_call_reg(comp_ppc_reg addrreg);
void comp_ppc_jump(uae_uintptr addr);
void comp_ppc_prolog(uae_u32 save_regs);
void comp_ppc_epilog(uae_u32 restore_regs);
void comp_ppc_return_to_caller(uae_u32 restore_regs);
void comp_ppc_do_cycles(int totalcycles);
void comp_ppc_verify_pc(uae_u8* pc_addr_exp);
void comp_ppc_reload_pc_p(uae_u8* new_pc_p);
uae_u32 comp_ppc_save_temp_regs(uae_u32 exceptions);
void comp_ppc_restore_temp_regs(uae_u32 saved_regs);
void comp_ppc_return_from_block(int cycles);
void comp_ppc_exception(uae_u8 level, comp_exception_data* exception_data);
void comp_ppc_save_mapped_registers_from_list(uae_s8* mapped_regs);
void comp_ppc_save_flags(void);
void comp_ppc_load_pc(uae_u32 pc_address, uae_u32 location);
#endif

/* I wonder why we don't have these defined globally. */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* Some more function protos */
extern void flush_icache(int n);
extern void flush_icache_hard(const char* callsrc);
extern void set_cache_state(int enabled);

void execute_normal(void);
void exec_nostats(void);
void do_nothing(void);
void do_cycles_callback(unsigned int cycles_to_add);
