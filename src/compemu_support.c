#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "events.h"
#include "include/memory.h"
#include "custom.h"
#include "newcpu.h"
#include "compemu.h"
#include "uae_endian.h"
#include "compemu_compiler.h"
#include "compemu_macroblocks.h"

/* Number of temporary registers */
#define PPC_TMP_REGS_COUNT 10

/* List of temporary registers
 * Note: do not use this array directly, get the register mapping by
 * calling function comp_get_gpr_for_temp_register(). */
const int PPC_TMP_REGS[PPC_TMP_REGS_COUNT] = {  PPCR_TMP0,
												PPCR_TMP1,
												PPCR_TMP2,
												PPCR_TMP3,
												PPCR_TMP4,
												PPCR_TMP5,
												PPCR_TMP6,
												PPCR_TMP7,
												PPCR_TMP8,
												PPCR_TMP9 };

/**
 *  List of temporary register usage,
 *  items can be either:
 *    PPC_TMP_REG_NOTUSED - temporary register is not mapped
 *    PPC_TMP_REG_ALLOCATED - temporary register is allocated for other purpose than M68k register emulation
 *    other - temporary register is allocated and mapped for a M68k register, the item contains the M68k register number */
int used_tmp_regs[PPC_TMP_REGS_COUNT];

/* Structure for the M68k register mapping to the temporary registers */
struct m68k_register {
	int tmpreg;			//Mapped temporary register number or PPC_TMP_REG_NOTUSED
	int needs_flush;	//0 - no need to flush this register, 1 - compile code for writing back the register out to the interpretive regs structure
};

/* M68k register mapping to the temp registers */
struct m68k_register comp_m68k_registers[16];

/**
 * Compiled code cache memory start address
 */
uae_u8* compiled_code = NULL;

/**
 * Compiled code cache memory end address
 */

uae_u8* compiled_code_top = NULL;

/**
 * Base register for the M68k interpretive registers array
 */
int comp_regs_base_reg = PPC_TMP_REG_NOTUSED;

/**
 * Current top of the compiling target
 */
uae_u8* current_compile_p = NULL;

/**
 * Cache enabled state
 */
int cache_enabled = 0;

/**
 * The highest start addess for code compiling when the compiling starts
 */
uae_u8* max_compile_start;

/**
 * Pointer to the currently compiled instruction
 */
uae_u16* current_block_pc_p;

/**
 * Pointer to the previously marked branch instruction that has to be completed when target is identified
 */
uae_u8* compiled_branch_instruction;

/**
 * Pointer to the target address for a branch instruction for compiling it later on
 */
uae_u8* compiled_branch_target;

/**
 * Reference to the kickstart memory start address
 */
extern uae_u8* kickmemory;

/**
 * Reference to picasso initialization flag
 */
extern int have_done_picasso;

/**
 * Reference to compile opcode table
 */
extern const struct comptbl compopcodetbl[];

/* The 68k only ever executes from even addresses. So right now, we
 waste half the entries in this array
 UPDATE: We now use those entries to store the start of the linked
 lists that we maintain for each hash result. */
cacheline cache_tags[TAGSIZE];

/* Preallocated blockinfo structures */
//TODO: do we need preallocated blockinfo?
blockinfo* hold_bi[MAX_HOLD_BI];


static void free_cache(void)
{
	if (compiled_code)
	{
		flush_icache_hard("free cache");
		cache_free(compiled_code);
		compiled_code = 0;

		write_log("JIT: Deallocated translation cache.\n");
	}
}

static void alloc_cache(void)
{
	write_log("JIT: Allocation of translation cache...\n");

	if (compiled_code != 0)
	{
		write_log("JIT: Translation cache is already allocated, leaving.\n");
	}

	write_log("JIT: Translation cache size in prefs: %d\n", currprefs.cachesize);
	if (currprefs.cachesize == 0)
	{
		write_log("JIT: translation cache is 0 in prefs, leaving.\n");
		return;
	}

	//Fail-back method for allocating the cache, halve the size on failure
	while (!compiled_code && currprefs.cachesize)
	{
		compiled_code = cache_alloc(currprefs.cachesize * 1024);
		if (!compiled_code) currprefs.cachesize /= 2;
	}

	//Initialize the pointers
	if (compiled_code)
	{
		compiled_code_top = compiled_code + currprefs.cachesize * 1024;
		max_compile_start = compiled_code_top - BYTES_PER_INST;
		current_compile_p = compiled_code;
	}

	write_log("JIT: Allocated %d KB translation cache.\n", currprefs.cachesize);
}

void set_cache_state(int enabled)
{
	write_log("JIT: Change cache emulation: %s\n", enabled ? "enabled" : "disabled");

	//If the enabled state changed then flush the cahe
	if (enabled != cache_enabled)
	{
		flush_icache_hard("cache state change");
	}

	cache_enabled = enabled;
}

/**
 * Preferences handling
 **/
void check_prefs_changed_comp(void)
{
	currprefs.comptrustbyte = changed_prefs.comptrustbyte;
	currprefs.comptrustword = changed_prefs.comptrustword;
	currprefs.comptrustlong = changed_prefs.comptrustlong;
	currprefs.comptrustnaddr = changed_prefs.comptrustnaddr;
	currprefs.compnf = changed_prefs.compnf;
	currprefs.comp_hardflush = changed_prefs.comp_hardflush;
	currprefs.comp_constjump = changed_prefs.comp_constjump;
	currprefs.comp_oldsegv = changed_prefs.comp_oldsegv;
	currprefs.compfpu = changed_prefs.compfpu;

	if (currprefs.cachesize != changed_prefs.cachesize)
	{
		currprefs.cachesize = changed_prefs.cachesize;
		free_cache();
		alloc_cache();
	}

	if ((!canbang || !currprefs.cachesize) && currprefs.comptrustbyte != 1)
	{
		// Set all of these to indirect when canbang == 0
		// Basically, set the  compforcesettings option...
		currprefs.comptrustbyte = 1;
		currprefs.comptrustword = 1;
		currprefs.comptrustlong = 1;
		currprefs.comptrustnaddr = 1;
		currprefs.compforcesettings = 1;

		changed_prefs.comptrustbyte = 1;
		changed_prefs.comptrustword = 1;
		changed_prefs.comptrustlong = 1;
		changed_prefs.comptrustnaddr = 1;
		changed_prefs.compforcesettings = 1;

		if (currprefs.cachesize) write_log(
				"JIT: Reverting to \"indirect\" access, because canbang is zero!\n");
	}
}

/**
 * Initialize block compiling
 */
void comp_init(void)
{
	int i;

	write_jit_log("Init compiling\n");

	/* Initialize branch compiling: both pointers must be zero */
	compiled_branch_instruction = compiled_branch_target = NULL;

	/* Initialize macroblock compiler */
	comp_compiler_init();
}

/**
 * Finish and clean up block compiling
 */
void comp_done(void)
{
	write_jit_log("Done compiling\n");

	if (compiled_branch_instruction != NULL)
	{
		write_log("Compiling error: branch instruction compiling was not completed, branch instruction address: 0x%08x\n", compiled_branch_instruction);
		abort();
	}

	if (compiled_branch_target != NULL)
	{
		write_log("Compiling error: branch instruction compiling was scheduled, but not completed, target address: 0x%08x\n", compiled_branch_target);
		abort();
	}

	//Flush all temp registers
	comp_flush_temp_registers();

	//Release regs base register if it was allocated
	comp_free_regs_base_register();

	comp_compiler_done();
}

STATIC_INLINE int isinrom(uae_uintptr addr)
{
	return (addr >= (uae_uintptr) kickmemory && addr < (uae_uintptr) kickmemory + 8 * 65536);
}

static void check_checksum(void)
{
	write_jit_log("Check checksum...\n");

	blockinfo* bi = get_blockinfo_addr(regs.pc_p);
	uae_u32 cl = cacheline(regs.pc_p);
	blockinfo* bi2 = get_blockinfo(cl);

	uae_u32 c1, c2;

	/* These are not the droids you are looking for...  */
	if (!bi)
	{
		/* We shouldn't be here... Anyway, let's execute the code normally. */
		execute_normal();
		return;
	}
	if (bi != bi2)
	{
		/* The block was hit accidentally, but it does exist. Cache miss */
		cache_miss();
		return;
	}

	if (bi->c1 || bi->c2)
	{
		calc_checksum(bi, &c1, &c2);
	}
	else
	{
		c1 = c2 = 1; /* Make sure it doesn't match */
	}
	if (c1 == bi->c1 && c2 == bi->c2)
	{
		/* This block is still OK. So we reactivate. Of course, that
		 means we have to move it into the needs-to-be-flushed list */
		bi->handler_to_use = bi->handler;

		raise_in_cl_list(bi);
	}
	else
	{
		/* This block actually changed. We need to invalidate it,
		 and set it up to be recompiled */
		raise_in_cl_list(bi);
		execute_normal();
	}
}

void compemu_reset(void)
{
	int i;
	write_log("JIT: Compiling reset\n");

	//Disable cache emulation
	set_cache_state(0);

	/* Clear used temporary registers list */
	for (i = 0; i < PPC_TMP_REGS_COUNT; i++)
		used_tmp_regs[i] = PPC_TMP_REG_NOTUSED;

	/* Clear M68k - PPC temp register mapping */
	for (i = 0; i < 16; i++)
		comp_m68k_registers[i].tmpreg = PPC_TMP_REG_NOTUSED;

	/* Clear the base register for the regs array */
	comp_regs_base_reg = PPC_TMP_REG_NOTUSED;
}

void build_comp(void)
{
	int i;
	unsigned long opcode;

	write_log("JIT: Building compiler function table.\n");

    for (opcode = 0; opcode < 65536; opcode++)
	{
		//TODO: remove instructions that are not supported by the processor

		if (table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > currprefs.cpu_level) continue;
	}

	//Reset cache lines
	for (i = 0; i < TAGSIZE; i += 2)
	{
		cache_tags[i].handler = execute_normal_callback;
		cache_tags[i + 1].bi = NULL;
	}

	/* Initialize state */
	alloc_cache();
}

void flush_icache_hard(const char* callsrc)
{
	uae_u32 i;
	blockinfo* bi;

	write_jit_log("Flush icache hard (%s/%x/%p)\n", callsrc, regs.pc, regs.pc_p);

	//!TODO: should we go trough the lists to free up cache lines? It might be faster
	//    bi=active;
	//    while(bi) {
	//	cache_tags[cacheline(bi->pc_p)].handler=(void*)popall_execute_normal;
	//	cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
	//	bi=bi->next;
	//    }
	//    bi=dormant;
	//    while(bi) {
	//	cache_tags[cacheline(bi->pc_p)].handler=(void*)popall_execute_normal;
	//	cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
	//	bi=bi->next;
	//    }

	//Reset cache tags
	for (i = 0; i < TAGSIZE; i += 2)
	{
		cache_tags[i].handler = execute_normal_callback;
		cache_tags[i + 1].bi = NULL;
	}

	reset_lists();
	if (!compiled_code) return;
	current_compile_p = compiled_code;
	set_special(&regs, 0); /* To get out of compiled code */
}

void flush_icache(int n)
{
	write_jit_log("Flush icache soft (%d/%x/%p)\n", n, regs.pc, regs.pc_p);

	//!TODO: is soft cache flush needed? It is retargeted to hard cache flush for now
	flush_icache_hard("soft cache flush");

	//	uae_u32 i;
	//	blockinfo* bi;
	//	blockinfo* bi2;
	//
	//	if (currprefs.comp_hardflush)
	//	{
	//		flush_icache_hard("soft cache flush");
	//		return;
	//	}
	//
	//	if (!active)
	//		return;
	//
	//	bi = active;
	//	while (bi)
	//	{
	//		uae_u32 cl = cacheline(bi->pc_p);
	//		if (!bi->handler)
	//		{
	//			/* invalidated block */
	//			if (bi == cache_tags[cl + 1].bi)
	//				cache_tags[cl].handler = execute_normal_callback;
	//			bi->handler_to_use = execute_normal_callback;
	//		}
	//		else
	//		{
	//			if (bi == cache_tags[cl + 1].bi)
	//				cache_tags[cl].handler = check_checksum_callback;
	//			bi->handler_to_use = check_checksum_callback;
	//		}
	//		bi2 = bi;
	//		bi = bi->next;
	//	}
	//	/* bi2 is now the last entry in the active list */
	//	bi2->next = dormant;
	//	if (dormant)
	//		dormant->prev_p = &(bi2->next);
	//
	//	dormant = active;
	//	active->prev_p = &dormant;
	//	active = NULL;
}

void compile_block(const cpu_history *pc_hist, int blocklen, int totcycles)
{
	write_jit_log("JIT: compile code, pc: %08x, block length: %d, total cycles: %d\n",
			pc_hist->pc, blocklen, totcycles);

	if (cache_enabled && compiled_code && currprefs.cpu_level >= 2)
	{
		write_jit_log("Compiling enabled, block length: %d\n", blocklen);

		int i;

		uae_u32 cl = cacheline(pc_hist[0].location);
		void* specflags = (void*) &regs.spcflags;
		blockinfo* bi = NULL;

		if (current_compile_p >= max_compile_start) flush_icache_hard("compiling - buffer is full");

		alloc_blockinfos();

		bi = get_blockinfo_addr_new(pc_hist[0].location, 0);

		if (bi->handler)
		{
			if (bi != get_blockinfo(cl))
			{
				write_log("Block error: count=%d, %p %p\n", bi->count, bi->handler_to_use,
						cache_tags[cl].handler);
				abort();
			}
		}

		//Do we still counting back on block execution?
		if (bi->count > -1)
		{
			// Still counting, skip compiling
			bi->count--;
			return;
		}

		//Add the pointer to the fist compiled instruction as the begging of the block
		bi->pc_p = (uae_u8*) pc_hist[0].location;

		//Initialize compiling
		comp_init();

		//Main loop of compiling
		uae_u8* compile_p_at_start = current_compile_p;

		write_jit_log("Compiled code start: %08x\n", current_compile_p);

		//Compile verification of 68k PC against the expected PC and call
		//cache miss function if these were not matching
		comp_ppc_verify_pc((uae_u8*) pc_hist[0].location, &regs.pc_p);

		//Compile prolog (stackframe preparing) to the buffer
		comp_ppc_prolog();

		//First block: load flags into the register
		comp_macroblock_push_load_flags();

		//Loop trough the previously collected instructions
		for (i = 0; i < blocklen && current_compile_p < max_compile_start; i++)
		{
			write_jit_log("Compile: %08x (%08x): ", pc_hist[i].pc, pc_hist[i].location);
			uaecptr nextpc;
			//TODO: implement JIT file handler for the disasm output
			//m68k_disasm(stdout, (uaecptr) pc_hist[i].pc, &nextpc, 1);

			//Recall special memory access from CPU history
			special_mem = pc_hist[i].specmem;

			uae_u16* location = pc_hist[i].location;
			uae_u16 opcode = do_get_mem_word(location);

			struct comptbl* props = &compprops[opcode];

			//Is this instruction supported? (handler is not NULL)
			if (props->instr_handler != NULL)
			{
				//Call addressing pre functions, if not null
				if (compsrc_pre_func[props->src_addr]) compsrc_pre_func[props->src_addr](location, props);
				if (compdest_pre_func[props->dest_addr]) compdest_pre_func[props->dest_addr](location, props);

				//Call instruction compiler
				props->instr_handler(location, props);

				//Call addressing post functions, if not null
				if (compsrc_post_func[props->src_addr]) compsrc_post_func[props->src_addr](location, props);
				if (compdest_post_func[props->dest_addr]) compdest_post_func[props->dest_addr](location, props);
			}
			else
			{
				//Not supported: compile direct call to the interpretive emulator
				write_jit_log("Unsupported opcode: 0x%04x\n", opcode);

				comp_opcode_unsupported(location, opcode);
			}
		}

		//Last block: save flags back to memory from register
		comp_macroblock_push_save_flags();

		//Optimize the collected macroblocks
		comp_compiler_optimize_macroblocks();

		//Generate the PPC code from the macroblocks
		comp_compiler_generate_code();

		//Compile calling the do_cycles function at the end of the block with the pre-calculated cycles
		comp_ppc_do_cycles(scaled_cycles(totcycles));

		//Return to the caller from the compiled block
		comp_ppc_return_to_caller();

		//PowerPC cache flush at the end of the compiling
		ppc_cacheflush(compile_p_at_start, current_compile_p - compile_p_at_start);

		//Lower and upper bound of the translated block
		uae_uintptr min_pcp = (uae_uintptr) pc_hist[0].location;
		uae_uintptr max_pcp = (uae_uintptr) pc_hist[blocklen - 1].location;

		//After translation calculate the real block length in 68k memory for the compiled block
		bi->len = max_pcp - min_pcp;

		//Calculate checksum
		if (isinrom(min_pcp) && isinrom(max_pcp))
		{
			bi->dormant = TRUE; /* No need to checksum it on cache flush. */
		}
		else
		{
			bi->dormant = FALSE;
			calc_checksum(bi, &(bi->c1), &(bi->c2));
		}

		//Block start from compiling
		bi->handler = bi->handler_to_use = (cpuop_func*) compile_p_at_start;

		//Raise block in cache list
		raise_in_cl_list(bi);

		//Finished compiling, cleanup
		comp_done();
	}
	else
	{
		write_jit_log("Compiling ignored\n");
	}
}

/**
 * Get the base register for the M68k regs array, allocate a temporary register
 * if it was not done yet and load the base address to that register.
 * Returns the temp register index that is mapped.
 */
uae_u8 comp_get_regs_base_register()
{
	if (comp_regs_base_reg == PPC_TMP_REG_NOTUSED)
	{
		//It is not mapped yet, we need to get a temp register
		comp_regs_base_reg = comp_allocate_temp_register(PPC_TMP_REG_BASEREG);

		//Compile code to load the base pointer to the regs array
		comp_macroblock_push_load_register_long(
				COMP_COMPILER_MACROBLOCK_REG_TMP(comp_regs_base_reg),
				comp_get_gpr_for_temp_register(comp_regs_base_reg),
				(uae_u32)&regs.regs[0]);
	}

	return comp_regs_base_reg;
}

/**
 * Release the temp register for the base register for the M68k regs array,
 * if there was one allocated.
 */
void comp_free_regs_base_register()
{
	if (comp_regs_base_reg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(comp_regs_base_reg);
		comp_regs_base_reg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Allocate a temporary register
 * Parameters:
 *   allocate_for - the M68k register number that was mapped to the temp
 *                  register, or one of the PPC_TMP_REG_* constants.
 * Returns the index of the PPC temporary register that was allocated
 */
uae_u8 comp_allocate_temp_register(int allocate_for)
{
	uae_u8 i;

	//Allocate the next free temporary register
	for(i = 0; i < PPC_TMP_REGS_COUNT; i++)
		if (used_tmp_regs[i] == PPC_TMP_REG_NOTUSED) break;

	if (i == PPC_TMP_REGS_COUNT)
	{
		//TODO: free up the oldest register
		write_log("Error: JIT compiler ran out of free temporary registers\n");
		abort();
	}

	//Set allocated state for the register
	used_tmp_regs[i] = allocate_for;

	return i;
}

/**
 * Free previously allocated temporary register
 * Parameters:
 *    temp_reg - index of the temporary register that needs to be free'd
 */
void comp_free_temp_register(uae_u8 temp_reg)
{
	if (used_tmp_regs[temp_reg] == PPC_TMP_REG_NOTUSED)
	{
		//Wasn't allocated
		write_jit_log("Warning: Temporary register %d was not allocated, but now it is free'd\n", temp_reg);
		return;
	}

	used_tmp_regs[temp_reg] = PPC_TMP_REG_NOTUSED;
}

/**
 * Returns the GPR register that is mapped to a temporary register index
 */
uae_u8 comp_get_gpr_for_temp_register(uae_u8 tmpreg)
{
	if (tmpreg >= PPC_TMP_REGS_COUNT)
	{
		write_log("ERROR: JIT temporary register index '%d' cannot be mapped to GPR\n", tmpreg);
	}

	if (used_tmp_regs[tmpreg] == PPC_TMP_REG_NOTUSED)
	{
		write_log("ERROR: JIT temporary register '%d' is not allocated, but mapping info is requested\n");
		abort();
	}

	return PPC_TMP_REGS[tmpreg];
}

/**
 * Flush all the allocated temporary registers (except base register),
 * reset allocation state.
 */
void comp_flush_temp_registers()
{
	int i;

	/* Flush temporary registers list */
	for (i = 0; i < PPC_TMP_REGS_COUNT; i++)
	{
		switch (used_tmp_regs[i])
		{
		case PPC_TMP_REG_NOTUSED:
		case PPC_TMP_REG_BASEREG:
			break;
		case PPC_TMP_REG_ALLOCATED:
			//This register is allocated for temporary operations, must be deallocated, but let it slip with a warning
			write_jit_log("Warning: Temporary register %d allocated but not free'd\n", i);
			used_tmp_regs[i] = PPC_TMP_REG_NOTUSED;
			break;
		default:
			//Temp register is mapped to a M68k register
			comp_unmap_temp_register(used_tmp_regs[i]);
			break;
		}
	}
}

/**
 * Maps a temporary register to a M68k register
 * Parameters:
 *   reg_number - number of the M68k register for the mapping
 *   needs_init - if 1 then the register must be initialized in the compiled code with the register from the regs array
 *   needs_flush - if 1 then the register must be written back to the regs array on releasing
 * Returns the mapped physical PPC register number.
 */
uae_u8 comp_map_temp_register(uae_u8 reg_number, int needs_init, int needs_flush)
{
	uae_u8 tmpreg;
	uae_u8 basereg;
	uae_u8 ppc_reg;
	struct m68k_register* reg = &comp_m68k_registers[reg_number];

	//Check for already mapped register
	if (reg->tmpreg != PPC_TMP_REG_NOTUSED)
	{
		//It is already mapped, but we need to make sure that if flush is
		//requested for this mapping and wasn't for the previous mapping then
		//still it will be done at the end
		reg->needs_flush |= needs_flush;
		ppc_reg = comp_get_gpr_for_temp_register(reg->tmpreg);
	} else {
		//Allocate a temp register for the mapping
		tmpreg = comp_allocate_temp_register(reg_number);

		//Map the temp register
		reg->tmpreg = tmpreg;
		reg->needs_flush = needs_flush;
		ppc_reg = comp_get_gpr_for_temp_register(tmpreg);

		if (needs_init)
		{
			//The register needs initialization from the interpretive M68k register array
			basereg = comp_get_regs_base_register();
			comp_macroblock_push_load_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_TMP(basereg),
					COMP_COMPILER_MACROBLOCK_REG_DX_OR_AX(reg_number),
					ppc_reg,
					comp_get_gpr_for_temp_register(basereg),
					reg_number * 4);
		}
	}

	return ppc_reg;
}

/**
 * Free up mapping of a temp register to a M68k register
 * Parameters:
 *    reg_number - M68k register number that is mapped
 */
void comp_unmap_temp_register(uae_u8 reg_number)
{
	struct m68k_register* reg = &comp_m68k_registers[reg_number];
	uae_u8 basereg;

	if (reg->tmpreg == PPC_TMP_REG_NOTUSED)
	{
		write_jit_log("Warning: Free'd M68k register %d is not mapped to a temp register\n", reg_number);
	}
	else
	{
		if (reg->needs_flush)
		{
			//Register must be written back to the regs array
			basereg = comp_get_regs_base_register();
			comp_macroblock_push_save_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_TMP(basereg) | COMP_COMPILER_MACROBLOCK_REG_DX_OR_AX(used_tmp_regs[reg_number]),
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					comp_get_gpr_for_temp_register(reg->tmpreg),
					comp_get_gpr_for_temp_register(basereg),
					reg_number * 4);
		}
		used_tmp_regs[reg->tmpreg] = PPC_TMP_REG_NOTUSED;
		reg->tmpreg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Callback functions for main code handling routines
 * Parameters are ignored
 */

unsigned long execute_normal_callback(uae_u32 ignored1, struct regstruct *ignored2)
{
	execute_normal();

	return 0;
}

unsigned long exec_nostats_callback(uae_u32 ignored1, struct regstruct *ignored2)
{
	exec_nostats();

	return 0;
}

void do_cycles_callback(unsigned int cycles_to_add)
{
	do_cycles(cycles_to_add);
}

/* Blockinfo list handling functions */
STATIC_INLINE blockinfo* get_blockinfo(uae_u32 cl)
{
	return cache_tags[cl + 1].bi;
}

STATIC_INLINE blockinfo* get_blockinfo_addr(void* addr)
{
	blockinfo* bi = get_blockinfo(cacheline(addr));

	while (bi)
	{
		if (bi->pc_p == addr) return bi;
		bi = bi->next_same_cl;
	}
	return NULL;
}

STATIC_INLINE void remove_from_cl_list(blockinfo* bi)
{
	uae_u32 cl = cacheline(bi->pc_p);

	if (bi->prev_same_cl_p) *(bi->prev_same_cl_p) = bi->next_same_cl;
	if (bi->next_same_cl) bi->next_same_cl->prev_same_cl_p = bi->prev_same_cl_p;
	if (cache_tags[cl + 1].bi) cache_tags[cl].handler = cache_tags[cl + 1].bi->handler_to_use;
	else cache_tags[cl].handler = execute_normal_callback;
}

STATIC_INLINE void add_to_cl_list(blockinfo* bi)
{
	uae_u32 cl = cacheline(bi->pc_p);

	if (cache_tags[cl + 1].bi) cache_tags[cl + 1].bi->prev_same_cl_p = &(bi->next_same_cl);
	bi->next_same_cl = cache_tags[cl + 1].bi;

	cache_tags[cl + 1].bi = bi;
	bi->prev_same_cl_p = &(cache_tags[cl + 1].bi);

	cache_tags[cl].handler = bi->handler_to_use;
}

STATIC_INLINE void raise_in_cl_list(blockinfo* bi)
{
	remove_from_cl_list(bi);
	add_to_cl_list(bi);
}

STATIC_INLINE void invalidate_block(blockinfo* bi)
{
	bi->count = currprefs.optcount[0] - 1;
	bi->handler = NULL;
	bi->handler_to_use = execute_normal_callback;
}

STATIC_INLINE blockinfo* get_blockinfo_addr_new(void* addr, int setstate)
{
	blockinfo* bi = get_blockinfo_addr(addr);
	int i;

	if (!bi)
	{
		for (i = 0; i < MAX_HOLD_BI && !bi; i++)
		{
			if (hold_bi[i])
			{
				bi = hold_bi[i];
				hold_bi[i] = NULL;
				bi->pc_p = addr;
				invalidate_block(bi);
				add_to_cl_list(bi);
			}
		}
	}
	if (!bi)
	{
		write_log("JIT: Looking for blockinfo, can't find free one\n");
		abort();
	}

	return bi;
}

STATIC_INLINE void alloc_blockinfos(void)
{
	int i;
	blockinfo* bi;

	for (i = 0; i < MAX_HOLD_BI; i++)
	{
		if (hold_bi[i])
		{
			return;
		}

		bi = hold_bi[i] = (blockinfo*) current_compile_p;
		current_compile_p += sizeof(blockinfo);
	}
}

STATIC_INLINE void reset_lists(void)
{
	int i;

	for (i = 0; i < MAX_HOLD_BI; i++)
		hold_bi[i] = NULL;
	//	active = NULL;
	//	dormant = NULL;
}

static void cache_miss(void)
{
	blockinfo* bi = get_blockinfo_addr(regs.pc_p);
	blockinfo* bi2 = get_blockinfo(cacheline(regs.pc_p));

	if (!bi)
	{
		execute_normal(); /* Compile this block now */
		return;
	}
	if (!bi2 || bi == bi2)
	{
		write_log("JIT: Unexplained cache miss %p %p\n", bi, bi2);
		abort();
	}
	raise_in_cl_list(bi);
	return;
}

static void calc_checksum(blockinfo* bi, uae_u32* c1, uae_u32* c2)
{
	uae_u32 k1 = 0;
	uae_u32 k2 = 0;
	uae_s32 len = bi->len;
	uae_uintptr tmp = (uae_uintptr) bi->pc_p;
	uae_u32* pos;

	len += (tmp & 3);
	tmp &= (~3);
	pos = (uae_u32*) tmp;

	if (len < 0 || len > MAX_CHECKSUM_LEN)
	{
		*c1 = 0;
		*c2 = 0;
	}
	else
	{
		while (len > 0)
		{
			k1 += *pos;
			k2 ^= *pos;
			pos++;
			len -= 4;
		}
		*c1 = k1;
		*c2 = k2;
	}
}

/** ------------------------------------------------------------------------------
 * Memory access compiling functions
 */

/* This version assumes that it is writing *real* memory, and *will* fail
 *  if that assumption is wrong! No branches, no second chances, just
 *  straight go-for-it attitude */
STATIC_INLINE void writemem_real(int address, int source, int offset, int size, int tmp)
{
	//TODO: writemem_real

//	int f = tmp;
//
//	mov_l_rr(f, address);
//	shrl_l_ri(f, 16); /* The index into the baseaddr table */
//	mov_l_rm_indexed(f, (uae_uintptr) baseaddr, f, 4);
//
//	if (address == source && size > 1)
//	{ /* IBrowse does this! */
//		add_l(f, address); /* f now has the final address */
//		switch (size)
//		{
//		case 2:
//			gen_bswap_16(source);
//			mov_w_Rr(f, source, 0);
//			gen_bswap_16(source);
//			break;
//		case 4:
//			gen_bswap_32(source);
//			mov_l_Rr(f, source, 0);
//			gen_bswap_32(source);
//			break;
//		}
//	}
//	else
//	{
//		/* f now holds the offset */
//		switch (size)
//		{
//		case 1:
//			mov_b_mrr_indexed(address, f, 1, source);
//			break;
//		case 2:
//			gen_bswap_16(source);
//			mov_w_mrr_indexed(address, f, 1, source);
//			gen_bswap_16(source);
//			break;
//		case 4:
//			gen_bswap_32(source);
//			mov_l_mrr_indexed(address, f, 1, source);
//			gen_bswap_32(source);
//			break;
//		}
//	}
}

STATIC_INLINE void writemem(int address, int source, int offset, int size, int tmp)
{
	//TODO: writemem trough banks

//    int f = tmp;
//
//	mov_l_rr(f, address);
//	shrl_l_ri(f, 16); /* The index into the mem bank table */
//	mov_l_rm_indexed(f, (uae_uintptr) mem_banks, f, 4);
//	/* Now f holds a pointer to the actual membank */
//	mov_l_rR(f, f, offset);
//	/* Now f holds the address of the b/w/lput function */
//	call_r_02(f, address, source, 4, size);
//	forget_about(tmp);
}

void writebyte(int address, int source, int tmp)
{
	int distrust;
	switch (currprefs.comptrustbyte)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_WRITE) || distrust)
	{
		writemem(address, source, 20, 1, tmp);
	}
	else
	{
		writemem_real(address, source, 20, 1, tmp);
	}
}

void writeword_general(int address, int source, int tmp)
{
	int distrust;
	switch (currprefs.comptrustword)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_WRITE) || distrust)
	{
		writemem(address, source, 16, 2, tmp);
	}
	else
	{
		writemem_real(address, source, 16, 2, tmp);
	}
}

void writelong_general(int address, int source, int tmp)
{
	int distrust;
	switch (currprefs.comptrustlong)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_WRITE) || distrust)
	{
		writemem(address, source, 12, 4, tmp);
	}
	else
	{
		writemem_real(address, source, 12, 4, tmp);
	}
}

/* This version assumes that it is reading *real* memory, and *will* fail
 *  if that assumption is wrong! No branches, no second chances, just
 *  straight go-for-it attitude */
STATIC_INLINE void readmem_real(int address, int dest, int offset, int size, int tmp)
{
	//TODO: readmem_real

//    int f = tmp;
//
//    if (size == 4 && address != dest)
//	f = dest;
//
//    mov_l_rr (f, address);
//    shrl_l_ri (f, 16);   /* The index into the baseaddr table */
//    mov_l_rm_indexed (f, (uae_uintptr) baseaddr, f, 4);
//    /* f now holds the offset */
//
//    switch(size) {
//     case 1: mov_b_rrm_indexed (dest, address, f, 1); break;
//     case 2: mov_w_rrm_indexed (dest, address, f, 1); gen_bswap_16 (dest); break;
//     case 4: mov_l_rrm_indexed (dest, address, f, 1); gen_bswap_32 (dest); break;
//    }
//    forget_about (tmp);
}

STATIC_INLINE void readmem(int address, int dest, int offset, int size, int tmp)
{
	//TODO: readmem trough banks

//	int f = tmp;
//
//    mov_l_rr (f,address);
//    shrl_l_ri (f,16);   /* The index into the mem bank table */
//    mov_l_rm_indexed (f, (uae_uintptr) mem_banks, f, 4);
//    /* Now f holds a pointer to the actual membank */
//    mov_l_rR (f, f, offset);
//    /* Now f holds the address of the b/w/lget function */
//    call_r_11 (dest, f, address, size, 4);
//    forget_about (tmp);
}

void readbyte(int address, int dest, int tmp)
{
	int distrust;
	switch (currprefs.comptrustbyte)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_READ) || distrust)
	{
		readmem(address, dest, 8, 1, tmp);
	}
	else
	{
		readmem_real(address, dest, 8, 1, tmp);
	}
}

void readword(int address, int dest, int tmp)
{
	int distrust;
	switch (currprefs.comptrustword)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_READ) || distrust)
	{
		readmem(address, dest, 4, 2, tmp);
	}
	else
	{
		readmem_real(address, dest, 4, 2, tmp);
	}
}

void readlong(int address, int dest, int tmp)
{
	int distrust;
	switch (currprefs.comptrustlong)
	{
	default:
	case 0:
		distrust = 0;
		break;
	case 1:
		distrust = 1;
		break;
	case 2:
		distrust = ((start_pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	if ((special_mem & SPECIAL_MEM_READ) || distrust)
	{
		readmem(address, dest, 0, 4, tmp);
	}
	else
	{
		readmem_real(address, dest, 0, 4, tmp);
	}
}

/**
 * Prototype for not implemented instruction aborting function
 */
void comp_not_implemented(uae_u16 opcode)
{
	write_log("Compiling error: instruction or addressing mode is not implemented, but marked as implemented: 0x%04x\n", opcode);
	abort();
}

/** ------------------------------------------------------------------------------
 * Compiling functions for native PowerPC code
 * Note: target of the compiling is always the top of the code cache, stored in
 * current_compile_p register. This register will also be updated with the size
 * of the compiled code.
 */

/* Pushes a word to the code cache and updates the pointer */
void comp_ppc_emit_word(uae_u32 word)
{
	*((uae_u32*) current_compile_p) = word;
	current_compile_p += 4;
}

/* Pushes two halfwords to the code cache and updates the pointer */
void comp_ppc_emit_halfwords(uae_u16 halfword_high, uae_u16 halfword_low)
{
	comp_ppc_emit_word((((uae_u32) halfword_high) << 16) | halfword_low);
}

/** ------------------------------------------------------------------------------
 * Instruction compilers
 */

/* Compiles addco instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_addco(int regd, int rega, int regb, int updateflags)
{
	// ## addco(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x0414 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles addi instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register
 * 		imm - immediate to be added
 */
void comp_ppc_addi(int regd, int rega, uae_u16 imm)
{
	//Parameter validation
	if (rega == 0)
	{
		write_log(
				"Compiling error: r0 register cannot be used for source register in addi instruction");
		abort();
	}

	// ## addi regd, rega, imm
	comp_ppc_emit_halfwords(0x3800 | (regd << 5) | rega, imm);
}

/* Compiles addis instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register
 * 		imm - immediate to be added
 */
void comp_ppc_addis(int regd, int rega, uae_u16 imm)
{
	//Parameter validation
	if (rega == 0)
	{
		write_log(
				"Compiling error: r0 register cannot be used for source register in addis instruction");
		abort();
	}

	// ## addis regd, rega, imm
	comp_ppc_emit_halfwords(0x3c00 | (regd << 5) | rega, imm);
}

/* Compiles and instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_and(int rega, int regs, int regb, int updateflags)
{
	// ## and(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regs) << 5) | rega,
			0x0038 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles andi. instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be and'ed to the register
 */
void comp_ppc_andi(int rega, int regs, uae_u16 imm)
{
	// ## andi. rega, regs, imm
	comp_ppc_emit_halfwords(0x7000 | ((regs) << 5) | rega, imm);
}

/* Compiles andis. instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be and'ed to the register
 */
void comp_ppc_andis(int rega, int regs, uae_u16 imm)
{
	// ## andis. rega, regs, imm
	comp_ppc_emit_halfwords(0x7400 | ((regs) << 5) | rega, imm);
}

/* Compiles b instruction
 * Parameters:
 * 		target - target address for the branch
 */
void comp_ppc_b(uae_u32 target)
{
	// ## b target
	comp_ppc_emit_word(0x48000000 | (target & 0x3fffffc));
}

/* Schedules a branch instruction target or compiles a conditional branch, if it was scheduled before.
 * See also:  comp_ppc_bc() function
 */
void comp_ppc_branch_target()
{
	//Is there an already scheduled target?
	if (compiled_branch_target != NULL)
	{
		//Yes, this must not happen
		write_log("Compiling error: branch target was already scheduled, scheduled branch target address: 0x%08x, new branch target address: 0x%08x\n", compiled_branch_instruction, current_compile_p);
		abort();
	}

	//Is there a previously scheduled branch instruction?
	if (compiled_branch_instruction != NULL)
	{
		//Calculate the offset to the target from the previously scheduled branch instruction
		uae_u32 offset = ((uae_u32) current_compile_p) - ((uae_u32) compiled_branch_instruction);

		//Is the offset to the target address less than 0x02000000 or more than 0xfdfffff?
		if ((offset < 0x02000000) || (offset > 0xfdfffff))
		{
			// complete already pre-compiled branch instruction in memory
			*((uae_u32*) compiled_branch_instruction) |= (offset & 0x3fffffc);

			//Branch instruction is finished, removing scheduled address
			compiled_branch_instruction = NULL;
		}
		else
		{
			//Target is too far a way, bailing out
			write_log(
					"Compiling error: at branch target scheduling target is too far from the instruction, distance: %08x\n",
					offset);
			abort();
		}
	}
	else
	{
		//There was no branch instruction scheduled, schedule target
		compiled_branch_target = current_compile_p;
	}
}

/* Schedules or compiles a conditional branch instruction.
 * Parameters:
 * 		bibo - the combined value for BI and BO instruction parts (conditional code)
 * See also: PPC_B_* defines and comp_ppc_branch_target() function
 */
void comp_ppc_bc(int bibo)
{
	//Is there a scheduled branch instruction already?
	if (compiled_branch_instruction != NULL)
	{
		//This must not happen, bailing out
		write_log("Compiling error: branch instruction was already scheduled\n");
		abort();
	}

	//Prepare instruction
	uae_u32 opcode = 0x40000000 | (bibo << 16);

	//Is there a target already available?
	if (compiled_branch_target != NULL)
	{
		//Calculate the offset to the target from the actual PC address
		uae_u32 offset = ((uae_u32) compiled_branch_target) - ((uae_u32) current_compile_p);

		//Is the offset to the target address less than 0x02000000 or more than 0xfdfffff?
		if ((offset < 0x02000000) || (offset > 0xfdfffff))
		{
			// ## bcc target
			comp_ppc_emit_word(opcode | (offset & 0x3fffffc));

			//Instruction is finished, remove target
			compiled_branch_target = NULL;
		}
		else
		{
			//Target is too far a way, bailing out
			write_log("Compiling error: at branch instruction compiling target is too far from the instruction, distance: %08x\n", offset);
			abort();
		}
	}
	else
	{
		//There was no target scheduled, schedule instruction
		compiled_branch_instruction = current_compile_p;

		//Emit the base instruction, target address will be filled in later on
		comp_ppc_emit_word(opcode);
	}
}

/* Compiles bl instruction
 * Parameters:
 * 		target - target address for the branch
 */
void comp_ppc_bl(uae_u32 target)
{
	// ## bl target
	comp_ppc_emit_word(0x48000001 | (target & 0x3fffffc));
}

/* Compiles blr instruction
 * Parameters:
 * 		none
 */
void comp_ppc_blr()
{
	// ## mtlr reg
	comp_ppc_emit_word(0x4e800020);
}

/* Compiles blrl instruction
 * Parameters:
 * 		none
 */
void comp_ppc_blrl()
{
	// ## blrl
	comp_ppc_emit_word(0x4e800021);
}

/* Compiles cmplw instruction
 * Parameters:
 * 		regcrfd - target CR register
 * 		rega - first register for comparing
 * 		regb - second register for comparing
 */
void comp_ppc_cmplw(int regcrfd, int rega, int regb)
{
	// ## cmpl regcrfd, 0, rega, regb
	comp_ppc_emit_halfwords(0x7C00 | regcrfd << 7 | rega, 0x0040 | regb << 11);
}

/* Compiles li instruction
 * Parameters:
 * 		rega - target register
 * 		imm - immediate to be loaded
 */
void comp_ppc_li(int rega, uae_u16 imm)
{
	// ## li rega, imm ==> addi reg, 0, imm
	comp_ppc_emit_halfwords(0x3800 | (rega << 5), imm);
}

/* Compiles lis instruction
 * Parameters:
 * 		rega - target register
 * 		imm - immediate to be added
 */
void comp_ppc_lis(int rega, uae_u16 imm)
{
	// ## lis rega, imm ==> addis rega, 0, imm
	comp_ppc_emit_halfwords(0x3c00 | (rega << 5), imm);
}

/* Compiles liw instruction
 * Parameters:
 * 		reg - target register
 * 		value - value to be loaded
 */
void comp_ppc_liw(int reg, uae_u32 value)
{
	//Value smaller than 0x00008000 or bigger than 0xffff7fff?
	if ((value < 0x00008000) || (value > 0xffff7fff))
	{
		//Yes - li (addi) instruction can be used,
		//upper halfword bits will be extended according to the highest bit of lower halfword
		comp_ppc_li(reg, value & 0xffff);
	}
	else
	{
		//No - liw (addis + ori) instructions must be used
		comp_ppc_lis(reg, value >> 16);

		uae_u16 lower = value & 0xffff;

		//Is the lower half 0?
		if (lower != 0)
		{
			//No - we have to load it, otherwise it can be skipped
			// ## ori reg, value & 0xffff
			comp_ppc_ori(reg, reg, lower);
		}
	}
}

/* Compiles lwz instruction
 * Parameters:
 * 		regd - target register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_lwz(int regd, uae_u16 delta, int rega)
{
	// ## lwz regd, delta(rega)
	comp_ppc_emit_halfwords(0x8000 | ((regd) << 5) | rega, delta);
}

/* Compiles mcrxr instruction
 * Parameters:
 * 		reg - target flag register
 */
void comp_ppc_mcrxr(int crreg)
{
	// ## mcrxr crreg
	comp_ppc_emit_word(0x7c000400 | (crreg << 23));
}

/* Compiles mfcr instruction
 * Parameters:
 * 		reg - target register
 */
void comp_ppc_mfcr(int reg)
{
	// ## mflr reg
	comp_ppc_emit_word(0x7c000026 | (reg << 21));
}

/* Compiles mflr instruction
 * Parameters:
 * 		reg - target register
 */
void comp_ppc_mflr(int reg)
{
	// ## mflr reg
	comp_ppc_emit_word(0x7c0802a6 | (reg << 21));
}

/* Compiles mfxer instruction
 * Parameters:
 * 		reg - target register
 */
void comp_ppc_mfxer(int reg)
{
	// ## mflr reg
	comp_ppc_emit_word(0x7c0102a6 | (reg << 21));
}

/* Compiles mtlr instruction
 * Parameters:
 * 		reg - source register
 */
void comp_ppc_mtlr(int reg)
{
	// ## mtlr reg
	comp_ppc_emit_word(0x7c0803a6 | (reg << 21));
}

/* Compiles mr instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_mr(int rega, int regs, int updateflags)
{
	// ## mr(x) rega, regs ==> or(x) rega, regs, regs
	comp_ppc_or(rega, regs, regs, updateflags);
}

/* Compiles nop instruction
 * Parameters:
 * 		none
 */
void comp_ppc_nop()
{
	// ## nop = ori r0,r0,0
	comp_ppc_ori(0, 0, 0);
}

/* Compiles or instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_or(int rega, int regs, int regb, int updateflags)
{
	// ## or(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regs) << 5) | rega,
			0x0378 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles ori instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be or'ed to the register
 */
void comp_ppc_ori(int rega, int regs, uae_u16 imm)
{
	// ## ori rega, regs, imm
	comp_ppc_emit_halfwords(0x6000 | ((regs) << 5) | rega, imm);
}

/* Compiles oris instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be or'ed to the register
 */
void comp_ppc_oris(int rega, int regs, uae_u16 imm)
{
	// ## oris rega, regs, imm
	comp_ppc_emit_halfwords(0x6400 | ((regs) << 5) | rega, imm);
}

/* Compiles rlwimi instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		shift - shift amount
 * 		maskb - mask beginning
 * 		maske - mask end
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_rlwimi(int rega, int regs, int shift, int maskb, int maske, int updateflags)
{
	// ## rlwimi(x) rega, regs, shift, maskb, maske
	comp_ppc_emit_halfwords(0x5000 | ((regs) << 5) | rega,
			(shift << 11) | (maskb << 6) | (maske << 1) | (updateflags ? 1 : 0));
}

/* Compiles rlwinm instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		shift - shift amount
 * 		maskb - mask beginning
 * 		maske - mask end
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_rlwinm(int rega, int regs, int shift, int maskb, int maske, int updateflags)
{
	// ## rlwinm(x) rega, regs, shift, maskb, maske
	comp_ppc_emit_halfwords(0x5400 | ((regs) << 5) | rega,
			(shift << 11) | (maskb << 6) | (maske << 1) | (updateflags ? 1 : 0));
}

/* Compiles sth instruction
 * Parameters:
 * 		regs - source register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_sth(int regs, uae_u16 delta, int rega)
{
	// ## sth regs, delta(rega)
	comp_ppc_emit_halfwords(0xb000 | ((regs) << 5) | rega, delta);
}

/* Compiles stw instruction
 * Parameters:
 * 		regs - source register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_stw(int regs, uae_u16 delta, int rega)
{
	// ## stw regs, delta(rega)
	comp_ppc_emit_halfwords(0x9000 | ((regs) << 5) | rega, delta);
}

 /* Compiles trap instruction
  * Parameters:
  * 		none
  */
void comp_ppc_trap()
 {
 	// ## trap
 	comp_ppc_emit_word(0x7fe00008);
 }

/** ------------------------------------------------------------------------------
 * More complex code chunks
 */

/* Compiles a subroutine call
 * Parameters:
 * 		reg - a temporary register (not preserved)
 * 		addr - target address for the subroutine
 */
void comp_ppc_call(int reg, uae_uintptr addr)
{
	//Calculate the offset to the target from the actual PC address
	uae_u32 offset = ((uae_u32) addr) - ((uae_u32) current_compile_p);

	//Is the offset to the target address less than 0x02000000 or more than 0xfdfffff?
	if ((offset < 0x02000000) || (offset > 0xfdfffff))
	{
		//Yes - we can use relative branch instruction
		comp_ppc_bl(offset);
	}
	else
	{
		comp_ppc_liw(reg, addr);
		comp_ppc_mtlr(reg);
		comp_ppc_blrl();
	}
}

/* Compiles a direct jump to an address
 * Parameters:
 * 		reg - a temporary register (not preserved)
 * 		addr - target address for the jump
 */
void comp_ppc_jump(int reg, uae_uintptr addr)
{
	//Calculate the offset to the target from the actual PC address
	uae_u32 offset = ((uae_u32) addr) - ((uae_u32) current_compile_p);

	//Is the offset to the target address less than 0x02000000 or more than 0xfdfffff?
	if ((offset < 0x02000000) || (offset > 0xfdfffff))
	{
		//Yes - we can use relative branch instruction
		comp_ppc_b(offset);
	}
	else
	{
		//No - offset is too large, indirect jump is used
		comp_ppc_liw(reg, addr);
		comp_ppc_mtlr(reg);
		comp_ppc_blr();
	}
}

/* Compiles prolog to the beginnig of the function call (stackframe preparing)
 * Note: emitted code uses PPCR_TMP0 register, it is not restored
 */
void comp_ppc_prolog()
{
	comp_ppc_mflr(PPCR_TMP0); //Read LR
	comp_ppc_stw(PPCR_TMP0, -4, PPCR_SP); //Store in stack frame

	comp_ppc_addi(PPCR_TMP0, PPCR_SP, -16); //Calculate new stack frame
	comp_ppc_rlwinm(PPCR_TMP0, PPCR_TMP0, 0, 0, 27, FALSE); //Align stack pointer
	comp_ppc_stw(PPCR_SP, 0, PPCR_TMP0); //Store backchain pointer
	comp_ppc_mr(PPCR_SP, PPCR_TMP0, FALSE); //Set real stack pointer
}

/* Compiles epilog to the end of the function call (stackframe freeing)
 * Note: emitted code uses PPCR_TMP0 and PPCR_SPECTMP registers, these are not restored
 */
void comp_ppc_epilog()
{
	comp_ppc_lwz(PPCR_TMP0, 0, PPCR_SP); //Read the pointer to the previous stack frame
	comp_ppc_lwz(PPCR_SPECTMP, 4, PPCR_TMP0); //Read LR from the stackframe
	comp_ppc_mtlr(PPCR_SPECTMP); //Restore LR
	comp_ppc_mr(PPCR_SP, PPCR_TMP0, FALSE); //Free up stack space by using the backchain pointer
}

/* Compiles a static do_cycles function call with the specified number of CPU cycles
 * Parameters:
 *      totalcycles - precalculated cycles for the executed chunk of compiled code
 */
void comp_ppc_do_cycles(int totalcycles)
{
	comp_ppc_liw(PPCR_PARAM1, totalcycles);
	comp_ppc_call(PPCR_SPECTMP, (uae_uintptr) do_cycles_callback);
}

/* Compiles "return from function" code to the code cache actual position */
void comp_ppc_return_to_caller()
{
	comp_ppc_epilog();
	comp_ppc_blr();
}

 /* Compile verification of 68k PC against the expected PC and call
  * cache miss function if these were not matching
  * Parameters:
  *      pc_addr_exp - expected 68k PC address (value)
  *      pc_addr_act - pointer to the memory address that contains the actual 58k PC address
  */
void comp_ppc_verify_pc(uae_u8* pc_addr_exp, uae_u8** pc_addr_act)
{
	//TODO: simplify access to the actual PC register by using regs variable pointer in a prepopulated native register
	comp_ppc_liw(PPCR_TMP0, (uae_u32) pc_addr_exp);
	comp_ppc_liw(PPCR_TMP1, (uae_u32) pc_addr_act);
	comp_ppc_lwz(PPCR_TMP1, 0, PPCR_TMP1);
	comp_ppc_cmplw(PPCR_CR_TMP0, PPCR_TMP0, PPCR_TMP1);

	//beq+ skip
	comp_ppc_bc(PPC_B_CR_TMP0_EQ | PPC_B_TAKEN);

	comp_ppc_liw(PPCR_PARAM1, (uae_u32) pc_addr_exp);

	//PC is not the same as the cached: continue on cache miss function
	comp_ppc_jump(PPCR_TMP0, (uae_u32) cache_miss);

	//skip:
	comp_ppc_branch_target();
}

/* Compiles a piece of code to reload the regs.pc_p pointer to the specified address. */
void comp_ppc_reload_pc_p(uae_u8* new_pc_p, uae_u8** regs_pc_p)
{
	//TODO: simplify access to the actual PC register by using regs variable pointer in a prepopulated native register

	comp_ppc_liw(PPCR_TMP0, (uae_u32) new_pc_p);
	comp_ppc_liw(PPCR_TMP1, (uae_u32) regs_pc_p);
	comp_ppc_stw(PPCR_TMP0, 0, PPCR_TMP1);
}
