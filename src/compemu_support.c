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
#define PPC_TMP_REGS_COUNT 11

/* List of temporary registers
 * Note: do not use this array directly, get the register mapping by
 * calling function comp_get_gpr_for_temp_register().
 * Make sure that the first register is always R3, this allows
 * some internal optimizations on returned values from GCC function calls. */
const int PPC_TMP_REGS[PPC_TMP_REGS_COUNT] = {  PPCR_TMP0,
												PPCR_TMP1,
												PPCR_TMP2,
												PPCR_TMP3,
												PPCR_TMP4,
												PPCR_TMP5,
												PPCR_TMP6,
												PPCR_TMP7,
												PPCR_TMP8,
												PPCR_TMP9,
												PPCR_TMP10 };

/**
 *  List of temporary register usage,
 *  items can be either:
 *    PPC_TMP_REG_NOTUSED - temporary register is not mapped
 *    PPC_TMP_REG_ALLOCATED - temporary register is allocated for other purpose than M68k register emulation
 *    other - temporary register is allocated and mapped for a M68k register, the item contains the M68k register number */
int used_tmp_regs[PPC_TMP_REGS_COUNT];

/* Structure for the M68k register mapping to the temporary registers */
struct m68k_register {
	int tmpreg;		//Mapped temporary register number or PPC_TMP_REG_NOTUSED
	char needs_flush;	//FALSE - no need to flush this register, TRUE - compile code for writing back the register out to the interpretive regs structure
	char locked;		//FALSE - this register is mapped but not used in the recent instruction, TRUE - this register is locked, cannot be flushed automatically
};

/* M68k register mapping to the temp registers */
struct m68k_register comp_m68k_registers[16];

/* Last automatically unmapped temporary register pointer.
 * To avoid unmapping the same register every time we keep track of the
 * last unmapped register and continue searching for an available register
 * when the next one is needed.
 */
int last_unmapped_register;

/* The start offset for the non-volatile registers in the stack frame.
 * Initialized by the prolog stack frame preparing function.
 * From this offset the non-volatile registers can be stored.
 */
int nonvolatile_regs_in_stackframe;

/**
 * Compiled code cache memory start address
 */
uae_u8* compiled_code = NULL;

/**
 * Compiled code cache memory end address
 */

uae_u8* compiled_code_top = NULL;

/**
 * Current top of the compiling target
 */
uae_u8* current_compile_p = NULL;

/**
 * Cache enabled state
 */
int cache_enabled = FALSE;

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
 * Actually compiled m68k instruction location
 */
uae_u16* compiled_m68k_location;

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
		compiled_code = NULL;

		write_log("JIT: Deallocated translation cache.\n");
	}
}

static void alloc_cache(void)
{
	write_log("JIT: Allocation of translation cache...\n");

	if (compiled_code != NULL)
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
	currprefs.compoptim = changed_prefs.compoptim;
	currprefs.complog = changed_prefs.complog;
	currprefs.complogcompiled = changed_prefs.complogcompiled;
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

	//TODO: do we really need the canbang setting? we figure out the memory access on our own and there are the comp_trust settings too
//	if ((!canbang || !currprefs.cachesize) && currprefs.comptrustbyte != 1)
//	{
//		// Set all of these to indirect when canbang == 0
//		// Basically, set the  compforcesettings option...
//		currprefs.comptrustbyte = 1;
//		currprefs.comptrustword = 1;
//		currprefs.comptrustlong = 1;
//		currprefs.comptrustnaddr = 1;
//		currprefs.compforcesettings = 1;
//
//		changed_prefs.comptrustbyte = 1;
//		changed_prefs.comptrustword = 1;
//		changed_prefs.comptrustlong = 1;
//		changed_prefs.comptrustnaddr = 1;
//		changed_prefs.compforcesettings = 1;
//
//		if (currprefs.cachesize) write_log(
//				"JIT: Reverting to \"indirect\" access, because canbang is zero!\n");
//	}
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

	comp_compiler_done();
}

/**
 * Leave the compiling gracefully: release all allocated resources
 */
void compemu_cleanup(void)
{
	free_cache();
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
	set_cache_state(FALSE);

	/* Clear used temporary registers list */
	for (i = 0; i < PPC_TMP_REGS_COUNT; i++)
		used_tmp_regs[i] = PPC_TMP_REG_NOTUSED;

	/* Clear M68k - PPC temp register mapping */
	for (i = 0; i < 16; i++)
	{
		comp_m68k_registers[i].tmpreg = PPC_TMP_REG_NOTUSED;
		comp_m68k_registers[i].locked = FALSE;
	}

	//Reset the unmapped register round-robin counter
	last_unmapped_register = -1;

	//Reset actually compiled M68k instruction pointer
	compiled_m68k_location = NULL;
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

/**
 * Returns a pointer to the address of the currently compiled M68k instruction
 */
uae_u16* comp_current_m68k_location()
{
	return compiled_m68k_location;
}

void compile_block(const cpu_history *pc_hist, int blocklen, int totcycles)
{
	const cpu_history * inst_history;
	char str[200];

	//This flag signs the unsupported opcodes in a row, some initialization/cleanup is skipped if there was multiple unsupported opcode
	int unsupported_in_a_row = TRUE;

	write_jit_log("JIT: compile code, pc: %08x, block length: %d, total cycles: %d\n",
			pc_hist->pc, blocklen, totcycles);

	if (cache_enabled && compiled_code && currprefs.cpu_level >= 2)
	{
		write_jit_log("Compiling enabled, block length: %d\n", blocklen);

		int i;

		uae_u32 cl = cacheline(pc_hist[0].location);
		void* specflags = (void*) &regs.spcflags;
		blockinfo* bi = NULL;

		if (comp_ppc_check_top()) flush_icache_hard("compiling - buffer is full");

		alloc_blockinfos();

		bi = get_blockinfo_addr_new(pc_hist[0].location, FALSE);

		if (bi->handler)
		{
			if (bi != get_blockinfo(cl))
			{
				write_log("Block error: count=%d, %p %p\n", bi->count, bi->handler_to_use,
						cache_tags[cl].handler);
				abort();
			}
		}

		//Is the block long enough (more than 3 instructions)?
		if (blocklen <= 3)
		{
			//No: not worth to compile it, hardwire to interpretiv execution
			bi->handler = exec_nostats_callback;
			return;
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

		//Compile prolog (stackframe preparing) to the buffer, saving non-volatile registers
		comp_ppc_prolog(PPCR_REG_USED_NONVOLATILE);

		//Set up Regs pointer register
		comp_ppc_liw(PPCR_REGS_BASE, (uae_u32)&regs);

		//Compile verification of 68k PC against the expected PC and call
		//cache miss function if these were not matching
		comp_ppc_verify_pc((uae_u8*) pc_hist[0].location);

		//Loop trough the previously collected instructions
		for (i = 0; i < blocklen; i++)
		{
			uaecptr nextpc;
			m68k_disasm_str(str, (uaecptr) pc_hist[i].pc, &nextpc, 1);
			write_jit_log("Comp: %s", str);

			//Set actually compiled M68k instruction pointer
			compiled_m68k_location = (uae_u16*) pc_hist[i].pc;

			uae_u16 opcode = do_get_mem_word(pc_hist[i].location);

			struct comptbl* props = &compprops[opcode];

			//Get the actual pc history, each handler needs it
			inst_history = &pc_hist[i];

			//Is this instruction supported? (handler is not NULL)
			if (props->instr_handler != NULL)
			{
				if (unsupported_in_a_row)
				{
					//Previous instructions were unsupported:
					//we have to load the flags into the register
					comp_macroblock_push_load_flags();
				}

				unsupported_in_a_row = FALSE;

				//Init opcode compiling
				comp_opcode_init(inst_history);

				//Call addressing pre functions, if not null
				if (compsrc_pre_func[props->src_addr]) compsrc_pre_func[props->src_addr](inst_history, props);
				if (compdest_pre_func[props->dest_addr]) compdest_pre_func[props->dest_addr](inst_history, props);

				//Call instruction compiler
				props->instr_handler(inst_history, props);

				//Call addressing post functions, if not null
				if (compsrc_post_func[props->src_addr]) compsrc_post_func[props->src_addr](inst_history, props);
				if (compdest_post_func[props->dest_addr]) compdest_post_func[props->dest_addr](inst_history, props);

				//Unlock all the mapped temporary registers
				comp_unlock_all_temp_registers();
			}
			else
			{
				//Not supported: compile direct call to the interpretive emulator
				write_jit_log("Unsupported opcode: 0x%04x\n", opcode);

				if (!unsupported_in_a_row)
				{
					//Previous instruction was a supported one

					//Save flags
					comp_macroblock_push_save_flags();

					//Update M68k PC
					comp_macroblock_push_load_pc(inst_history);
				}

				unsupported_in_a_row = TRUE;

				comp_opcode_unsupported(opcode);
			}
		}

		//Reset actually compiled M68k instruction pointer
		compiled_m68k_location = NULL;

		//Flush all temp registers
		comp_flush_temp_registers(FALSE);

		//Last block: save flags/changed registers back to memory from register, if it was loaded before
		if (!unsupported_in_a_row)
		{
			//Save back flags to the regs structure
			comp_macroblock_push_save_flags();

			//Reload the PC at the end of the block from the additional virtual history item at the end
			comp_macroblock_push_load_pc(&pc_hist[blocklen]);
		}

		//Optimize the collected macroblocks
		comp_compiler_optimize_macroblocks();

		//Generate the PPC code from the macroblocks
		comp_compiler_generate_code();

		//Dump compiled code to the console
		comp_compiler_debug_dump_compiled();

		//Compile calling the do_cycles function at the end of the block with the pre-calculated cycles
		comp_ppc_do_cycles(scaled_cycles(totcycles));

		//Return to the caller from the compiled block, restore non-volatile registers
		comp_ppc_return_to_caller(PPCR_REG_USED_NONVOLATILE);

		//Check whether we ran out of the compiling buffer
		if (current_compile_p >= max_compile_start)
		{
			//Ooops, let's leave the party early and reset the buffer
			comp_done();
			flush_icache_hard("compiling - buffer is full");
			return;
		}

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
//		write_jit_log("Compiling ignored\n");
	}
}

/**
 * Find an unlocked temp register for unmapping.
 * The best choice for the to be unmapped register is one that needs
 * no flushing. If that was not available then one that needs flushing.
 *
 * Although it is theoretically impossible, but if there is no unlocked
 * register then the emulation stops with an error.
 */
STATIC_INLINE int comp_find_unlocked_temp_register(void)
{
	int i;
	struct m68k_register* reg;
	int found = -1;

	//Round-robin searching of the next unmappable register
	for(i = last_unmapped_register + 1; i != last_unmapped_register; i++)
	{
		//Round-robin: turns around at the end of the array
		if (i == PPC_TMP_REGS_COUNT) i = 0;

		//If the register mapping is not negative then it is mapped to an emulated register
		if (used_tmp_regs[i] > -1)
		{
			reg = &comp_m68k_registers[used_tmp_regs[i]];
			if (!reg->locked)
			{
				if (!reg->needs_flush)
				{
					//Needs no flushing - best choice
					found = i;
					break;
				}

				//It needs flushing, let's remember that we can use this
				if (found == -1) found = i;
			}
		}
	}

	if (found == -1)
	{
		//This must not happen: all temporary registers are mapped and locked
		write_log("Error: JIT compiler ran out of temporary registers\n");
		abort();
	}

	last_unmapped_register = found;

	return found;
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
		//All registers are allocated: find an unlocked register for unmapping
		i = comp_find_unlocked_temp_register();

		//Unmap the register and reuse the temporary register
		comp_unmap_temp_register(used_tmp_regs[i]);
	}

	//Set allocated state for the register
	used_tmp_regs[i] = allocate_for;

//	write_jit_log("Temp register allocated: %d\n", (int)i);

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

//	write_jit_log("Temp register free'd: %d\n", (int)temp_reg);
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
void comp_flush_temp_registers(int supresswarning)
{
	int i;

	/* Flush temporary registers list */
	for (i = 0; i < PPC_TMP_REGS_COUNT; i++)
	{
		switch (used_tmp_regs[i])
		{
		case PPC_TMP_REG_NOTUSED:
			break;
		case PPC_TMP_REG_ALLOCATED:
			//This register is allocated for temporary operations, must be deallocated, but let it slip with a warning
			if (!supresswarning)
			{
				write_jit_log("Warning: Temporary register %d allocated but not free'd\n", i);
			}
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
 *   needs_init - if TRUE then the register must be initialized in the compiled code with the register from the regs array
 *   needs_flush - if TRUE then the register must be written back to the regs array on releasing
 * Returns the mapped physical PPC register number.
 */
uae_u8 comp_map_temp_register(uae_u8 reg_number, int needs_init, int needs_flush)
{
	uae_u8 tmpreg;
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
			comp_macroblock_push_load_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_DX_OR_AX(reg_number),
					ppc_reg,
					PPCR_REGS_BASE,
					reg_number * 4);
		}
	}

	//Lock the register for this instruction
	reg->locked = TRUE;

	return ppc_reg;
}

/**
 * Returns the mapped temporary register for a M68k register if available
 * Parameters:
 *   reg_number - number of the M68k register for the mapping
 * Returns the mapped physical PPC register number, or PPC_TMP_REG_NOTUSED if it was not mapped yet.
 */
int comp_get_mapped_temp_register(uae_u8 reg_number)
{
	struct m68k_register* reg = &comp_m68k_registers[reg_number];

	if (reg->tmpreg == PPC_TMP_REG_NOTUSED)
	{
		return PPC_TMP_REG_NOTUSED;
	} else {
		return comp_get_gpr_for_temp_register(reg->tmpreg);
	}
}

/**
 * Free up mapping of a temp register to a M68k register
 * Parameters:
 *    reg_number - M68k register number that is mapped
 */
void comp_unmap_temp_register(uae_u8 reg_number)
{
	struct m68k_register* reg = &comp_m68k_registers[reg_number];

	if (reg->tmpreg == PPC_TMP_REG_NOTUSED)
	{
		write_jit_log("Warning: Free'd M68k register %d is not mapped to a temp register\n", reg_number);
	}
	else
	{
		if (reg->needs_flush)
		{
			//Register must be written back to the regs array
			comp_macroblock_push_save_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_DX_OR_AX(reg_number),
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					comp_get_gpr_for_temp_register(reg->tmpreg),
					PPCR_REGS_BASE,
					reg_number * 4);
		}
		used_tmp_regs[reg->tmpreg] = PPC_TMP_REG_NOTUSED;
		reg->tmpreg = PPC_TMP_REG_NOTUSED;
		reg->locked = FALSE;
	}
}

/**
 * Unlocks all mapped temporary registers.
 * This function is called at the end of an instruction to unlock all
 * the locked mapped temporary registers and let the system unmap one
 * when it runs out of free temp registers.
 */
void comp_unlock_all_temp_registers()
{
	int i;

	for(i = 0; i < 16; i++)
		comp_m68k_registers[i].locked = FALSE;
}

/**
 * Dumps the register usage flags from a 64 bit integer into a string buffer.
 * Parameters:
 *    regs - input, output or carry flag collection for the register usage
 *    str - string buffer for the output
 *    dump_control - enable/disable dumping the control flags in front of the register flags
 */
void comp_dump_reg_usage(uae_u64 regs, char* str, char dump_control)
{
	int i, j = 0;

	//Dump control flags if enabled
	if (dump_control)
	{
		for(i = 63; i > COMP_COMPILER_MACROBLOCK_CONTROL_FLAGS_START - 1; i--) str[j++] = regs & (1UL << i) ? '1' : '0';
		str[j++] = ' ';
	}

	//Dump temporary regs
	for(i = COMP_COMPILER_MACROBLOCK_TMP_REGS_START + PPC_TMP_REGS_COUNT; i > COMP_COMPILER_MACROBLOCK_TMP_REGS_START - 1; i--) str[j++] = regs & (1UL << i) ? '1' : '0';
	str[j++] = ' ';

	//Dump normal regs
	for(i = COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGS_START - 1; i > COMP_COMPILER_MACROBLOCK_ADDR_REGS_START - 1; i--) str[j++] = regs & (1UL << i) ? '1' : '0';
	str[j++] = ' ';
	for(i = COMP_COMPILER_MACROBLOCK_ADDR_REGS_START - 1; i > COMP_COMPILER_MACROBLOCK_REGS_START - 1; i--) str[j++] = regs & (1UL << i) ? '1' : '0';
	str[j++] = ' ';

	//Dump flags
	for(i = COMP_COMPILER_MACROBLOCK_REGS_START - 1; i > - 1; i--) str[j++] = regs & (1UL << i) ? '1' : '0';

	str[j] = '\0';
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

int check_for_cache_miss(void)
{
    blockinfo* bi=get_blockinfo_addr(regs.pc_p);

    if (bi) {
	int cl=cacheline(regs.pc_p);
	if (bi!=cache_tags[cl+1].bi) {
	    raise_in_cl_list(bi);
	    return 1;
	}
    }
    return 0;
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

/**
 * Figuring out if this read from the address a was special read or a normal one.
 * Parameters:
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 *   trust - memory access trust configuration
 */
STATIC_INLINE int comp_is_spec_memory_read(uae_u32 pc, int specmem_flags, int trust)
{
	int distrust;
	switch (trust)
	{
	default:
	case 0:
		distrust = FALSE;
		break;
	case 1:
		distrust = TRUE;
		break;
	case 2:
		distrust = ((pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	return (specmem_flags & SPECIAL_MEM_READ) || distrust;
}

/**
 * Figuring out if this byte read from the address a was special read or a normal one.
 * Parameters:
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_read_byte(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_read(pc, specmem_flags, currprefs.comptrustbyte);
}

/**
 * Figuring out if this word read from the address a was special read or a normal one.
 * Parameters:
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_read_word(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_read(pc, specmem_flags, currprefs.comptrustword);
}

/**
 * Figuring out if this long read from the address was special read or normal one.
 * Parameters:
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_read_long(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_read(pc, specmem_flags, currprefs.comptrustlong);
}

/**
 * Figuring out if this write to the address was special write or normal one.
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 *   trust - memory access trust configuration
 */
STATIC_INLINE int comp_is_spec_memory_write(uae_u32 pc, int specmem_flags, int trust)
{
	int distrust;
	switch (trust)
	{
	default:
	case 0:
		distrust = FALSE;
		break;
	case 1:
		distrust = TRUE;
		break;
	case 2:
		distrust = ((pc & 0xF80000) == 0xF80000);
		break;
	case 3:
		distrust = !have_done_picasso;
		break;
	}

	return (specmem_flags & SPECIAL_MEM_WRITE) || distrust;
}

/**
 * Figuring out if this byte write to the address was special write or normal one.
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_write_byte(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_write(pc, specmem_flags, currprefs.comptrustbyte);
}

/**
 * Figuring out if this word write to the address was special write or normal one.
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_write_word(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_write(pc, specmem_flags, currprefs.comptrustword);
}

/**
 * Figuring out if this long write to the address was special write or normal one.
 *   pc - M68k emulated instruction pointer address
 *   specmem_flags - memory access flags from the previous interpretive emulation iteration
 */
int comp_is_spec_memory_write_long(uae_u32 pc, int specmem_flags)
{
	return comp_is_spec_memory_write(pc, specmem_flags, currprefs.comptrustlong);
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

/* Returns the actual top address of the compiled code buffer */
void* comp_ppc_buffer_top()
{
	return current_compile_p;
}

/* Pushes a word to the code cache and updates the pointer */
void comp_ppc_emit_word(uae_u32 word)
{
	*((uae_u32*) current_compile_p) = word;
	current_compile_p += 4;
}

/* Returns true if the compiling reached the top of the compiled code buffer */
int comp_ppc_check_top(void)
{
	 return (current_compile_p >= max_compile_start);
}

/* Pushes two halfwords to the code cache and updates the pointer */
void comp_ppc_emit_halfwords(uae_u16 halfword_high, uae_u16 halfword_low)
{
	comp_ppc_emit_word((((uae_u32) halfword_high) << 16) | halfword_low);
}

/** ------------------------------------------------------------------------------
 * Instruction compilers
 */

/* Compiles add instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_add(int regd, int rega, int regb, int updateflags)
{
	// ## add(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x0214 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles addc instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_addc(int regd, int rega, int regb, int updateflags)
{
	// ## addc(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x0014 | (regb << 11) | (updateflags ? 1 : 0));
}

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
				"JIT compiling error: r0 register cannot be used for source register in addi instruction");
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

		//Is the offset to the target address less than 0x02000000 or more than 0xfdffffff?
		if ((offset < 0x02000000) || (offset > 0xfdffffff))
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

		//Is the offset to the target address less than 0x02000000 or more than 0xfdffffff?
		if ((offset < 0x02000000) || (offset > 0xfdffffff))
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

/* Compiles cntlwz instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_cntlwz(int rega, int regs, int updateflags)
{
	// ## cntlwz(x) rega, regs
	comp_ppc_emit_halfwords(0x7C00 | regs << 5 | rega, 0x0034 | (updateflags ? 1 : 0));
}

/* Compiles extsb instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_extsb(int rega, int regs, int updateflags)
{
	// ## extsb(x) rega, regs
	comp_ppc_emit_halfwords(0x7C00 | regs << 5 | rega, 0x774 | (updateflags ? 1 : 0));
}

/* Compiles extsh instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_extsh(int rega, int regs, int updateflags)
{
	// ## extsh(x) rega, regs
	comp_ppc_emit_halfwords(0x7C00 | regs << 5 | rega, 0x734 | (updateflags ? 1 : 0));
}

/* Compiles lbz instruction
 * Parameters:
 * 		regd - target register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_lbz(int regd, uae_u16 delta, int rega)
{
	// ## lbz regd, delta(rega)
	comp_ppc_emit_halfwords(0x8800 | ((regd) << 5) | rega, delta);
}

/* Compiles lha instruction
 * Parameters:
 * 		regd - target register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_lha(int regd, uae_u16 delta, int rega)
{
	// ## lha regd, delta(rega)
	comp_ppc_emit_halfwords(0xA800 | ((regd) << 5) | rega, delta);
}

/* Compiles lhz instruction
 * Parameters:
 * 		regd - target register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_lhz(int regd, uae_u16 delta, int rega)
{
	// ## lhz regd, delta(rega)
	comp_ppc_emit_halfwords(0xA000 | ((regd) << 5) | rega, delta);
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

/* Compiles lwzx instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register
 * 		regb - index register
 */
void comp_ppc_lwzx(int regd, int rega, int regb)
{
	// ## lwzx regd, rega, regb
	comp_ppc_emit_halfwords(0x7c00 | (regd << 5) | rega, 0x002e | (regb << 11));
}

/* Compiles mcrxr instruction
 * Parameters:
 * 		crreg - target flag register
 */
void comp_ppc_mcrxr(int crreg)
{
	// ## mcrxr crreg
#ifdef _ARCH_PWR4
	// Rotate the XER bits into the right slot in the temp register.
	comp_ppc_mfxer(PPCR_SPECTMP);
	comp_ppc_rlwinm(PPCR_SPECTMP, PPCR_SPECTMP, (4*(8-crreg)), 0, 31, FALSE);
	// Copy to that CR field.
	// Our mt(o)crf mask is based on the condreg we're after. 128 = cr0
	comp_ppc_mtcrf(crreg, PPCR_SPECTMP);
	// Now rotate back and clear out the XER fields we need to erase.
	comp_ppc_rlwinm(PPCR_SPECTMP, PPCR_SPECTMP, (32-(4*(8-crreg))), (31-(4*crreg)), (31-(4*(7-crreg))), FALSE);
	// And do the write back to the XER
	comp_ppc_mtxer(PPCR_SPECTMP);
#else
	comp_ppc_emit_word(0x7c000400 | (crreg << 23));
#endif
}

/* Compiles mfcr instruction
 * Parameters:
 * 		reg - target register
 */
void comp_ppc_mfcr(int reg)
{
	// ## mfcr reg
	comp_ppc_emit_word(0x7c000026 | (reg << 21));
}

#ifdef _ARCH_PWR4
/* Compiles mf(o)crf instruction
 * Parameters:
 * 		crreg - source flag register
 * 		reg - target register
 */
void comp_ppc_mfocrf(int crreg, int reg)
{
	// ## mf(o)crf reg
	comp_ppc_emit_word(0x7c000826 | (reg << 21) | (1 << (crreg + 12)));
}
#endif

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
	// ## mfxer reg
	comp_ppc_emit_word(0x7c0102a6 | (reg << 21));
}

/* Compiles mt(o)crf instruction
 * Parameters:
 * 		crreg - target flag register
 * 		regf - register containing the flags
 */
void comp_ppc_mtcrf(int crreg, int regf)
{
	// ## mtcrf reg
#ifdef _ARCH_PWR4
	comp_ppc_emit_word(0x7c000920 | (regf << 21) | (1 << (crreg + 12)));
#else
	comp_ppc_emit_word(0x7c000120 | (regf << 21) | (1 << (crreg + 12)));
#endif
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

/* Compiles mtxer instruction
 * Parameters:
 * 		reg - source register
 */
void comp_ppc_mtxer(int reg)
{
	// ## mtxer reg
	comp_ppc_emit_word(0x7c0103a6 | (reg << 21));
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

/* Compiles mullwo instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_mullwo(int regd, int rega, int regb, int updateflags)
{
	// ## mullwo(x) regd, rega, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x05d6 | (regb << 11) | (updateflags ? 1 : 0));
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

/* Compiles nor instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_nor(int rega, int regs, int regb, int updateflags)
{
	// ## nor(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regs) << 5) | rega,
			0x00f8 | (regb << 11) | (updateflags ? 1 : 0));
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

/* Compiles srawi instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		shift - shift amount
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_srawi(int rega, int regs, int shift, int updateflags)
{
	// ## srawi(x) rega, regs, shift
	comp_ppc_emit_halfwords(0x7C00 | ((regs) << 5) | rega,
			0x0670 | (shift << 11) | (updateflags ? 1 : 0));
}

/* Compiles stb instruction
 * Parameters:
 * 		regs - source register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_stb(int regs, uae_u16 delta, int rega)
{
	// ## stb regs, delta(rega)
	comp_ppc_emit_halfwords(0x9800 | ((regs) << 5) | rega, delta);
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

/* Compiles sthu instruction
 * Parameters:
 * 		regs - source register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_sthu(int regs, uae_u16 delta, int rega)
{
	// ## sth regs, delta(rega)
	comp_ppc_emit_halfwords(0xb400 | ((regs) << 5) | rega, delta);
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

/* Compiles stwu instruction
 * Parameters:
 * 		regs - source register
 * 		delta - offset for the source address register
 * 		rega - source register
 */
void comp_ppc_stwu(int regs, uae_u16 delta, int rega)
{
	// ## stw regs, delta(rega)
	comp_ppc_emit_halfwords(0x9400 | ((regs) << 5) | rega, delta);
}

/* Compiles subfco instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_subfco(int regd, int rega, int regb, int updateflags)
{
	// ## subfco(x) regd, rega, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x0410 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles subfe instruction
 * Parameters:
 * 		regd - target register
 * 		rega - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_subfe(int regd, int rega, int regb, int updateflags)
{
	// ## subfe(x) regd, rega, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regd) << 5) | rega,
			0x0110 | (regb << 11) | (updateflags ? 1 : 0));
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

/* Compiles xor instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register 1
 * 		regb - source register 2
 * 		updateflags - compiles the flag updating version if TRUE
 */
void comp_ppc_xor(int rega, int regs, int regb, int updateflags)
{
	// ## xor(x) rega, regs, regb
	comp_ppc_emit_halfwords(0x7c00 | ((regs) << 5) | rega,
			0x0278 | (regb << 11) | (updateflags ? 1 : 0));
}

/* Compiles xori instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be or'ed to the register
 */
void comp_ppc_xori(int rega, int regs, uae_u16 imm)
{
	// ## xori rega, regs, imm
	comp_ppc_emit_halfwords(0x6800 | ((regs) << 5) | rega, imm);
}

/* Compiles xoris instruction
 * Parameters:
 * 		rega - target register
 * 		regs - source register
 * 		imm - immediate to be or'ed to the register
 */
void comp_ppc_xoris(int rega, int regs, uae_u16 imm)
{
	// ## xoris rega, regs, imm
	comp_ppc_emit_halfwords(0x6c00 | ((regs) << 5) | rega, imm);
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

	//Is the offset to the target address less than 0x02000000 or more than 0xfdffffff?
	if ((offset < 0x02000000) || (offset > 0xfdffffff))
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

/* Compiles a subroutine call to an address in a register
 * Parameters:
 * 		addrreg - target address for the subroutine in a register
 */
void comp_ppc_call_reg(int addrreg)
{
		comp_ppc_mtlr(addrreg);
		comp_ppc_blrl();
}


/* Compiles a direct jump to an address
 * Parameters:
 * 		addr - target address for the jump
 */
void comp_ppc_jump(uae_uintptr addr)
{
	//Calculate the offset to the target from the actual PC address
	uae_u32 offset = ((uae_u32) addr) - ((uae_u32) current_compile_p);

	//Is the offset to the target address less than 0x02000000 or more than 0xfdffffff?
	if ((offset < 0x02000000) || (offset > 0xfdffffff))
	{
		//Yes - we can use relative branch instruction
		comp_ppc_b(offset);
	}
	else
	{
		//No - offset is too large, indirect jump is used
		comp_ppc_liw(PPCR_SPECTMP, addr);
		comp_ppc_mtlr(PPCR_SPECTMP);
		comp_ppc_blr();
	}
}

/* Compiles prolog to the beginnig of the function call (stackframe preparing)
 * Note: emitted code uses PPCR_TMP0 register, it is not restored. Three additional
 * longwords are available in the stack frame for storing registers temporarily.
 * Parameters:
 *   save_regs - save the PPC registers to the stack that are marked with a high
 *   bit in this longword (e.g. R4 and R6 = (1 << 4) | (1 <<6) = %101000)
 */
#ifndef __APPLE__
void comp_ppc_prolog(uae_u32 save_regs)
{
	int i;
	int regnum = COMP_STACKFRAME_ALLOCATED_SLOTS;	//Additional free longwords for temporary register storage

	//How many registers do we want to save?
	for(i = 0; i < 32; i++) regnum += (save_regs & (1 << i)) ? 1 : 0;

	comp_ppc_mflr(PPCR_TMP0); //Read LR
	comp_ppc_stw(PPCR_TMP0, -4, PPCR_SP); //Store in stack frame

	comp_ppc_addi(PPCR_TMP0, PPCR_SP, -16 - (regnum * 4)); //Calculate new stack frame
	comp_ppc_rlwinm(PPCR_TMP0, PPCR_TMP0, 0, 0, 27, FALSE); //Align stack pointer
	comp_ppc_stw(PPCR_SP, 0, PPCR_TMP0); //Store backchain pointer
	comp_ppc_mr(PPCR_SP, PPCR_TMP0, FALSE); //Set real stack pointer

	//Save registers
	regnum = 8;
	for(i = 0; i < 32; i++)
	{
		if (save_regs & (1 << i))
		{
			comp_ppc_stw(i, regnum, PPCR_SP);
			regnum += 4;
		}
	}

	//Store offset for the non-volatile register slots
	nonvolatile_regs_in_stackframe = regnum;
}
#else
//Prolog for MacOSX Darwin ABI
void comp_ppc_prolog(uae_u32 save_regs)
{
	int i;
	int offset = -4;
	int regnum = COMP_STACKFRAME_ALLOCATED_SLOTS;	//Additional free longwords for temporary register storage

	//How many registers do we want to save?
	for(i = 0; i < 32; i++) {
		regnum += (save_regs & (1 << i)) ? 1 : 0;
	}

	comp_ppc_mflr(PPCR_TMP0); //Read LR
	comp_ppc_stw(PPCR_TMP0, 8, PPCR_SP); //Store in linkage area

	//Save registers
	for(i = 0; i < 32; i++)
	{
		if (save_regs & (1 << i))
		{
			comp_ppc_stw(i, offset, PPCR_SP);
			offset -= 4;
		}
	}

	comp_ppc_stwu(PPCR_SP, ((-24 - (regnum * 4) + 15) & (-16)), PPCR_SP); //Calculate new stack frame; 16 byte aligned, no parameter area!!! Set real stack pointer

	//Store offset for the non-volatile register slots
	nonvolatile_regs_in_stackframe = 20 - offset;
}
#endif

/* Compiles epilog to the end of the function call (stackframe freeing)
 * Note: emitted code uses PPCR_TMP0 and PPCR_SPECTMP registers, these are not restored
 * Parameters:
 *   restore_regs - restore the PPC registers from the stack that are marked with a high
 *   bit in this longword (e.g. R4 and R6 = (1 << 4) | (1 <<6) = %101000)
 */
#ifndef __APPLE__
void comp_ppc_epilog(uae_u32 restore_regs)
{
	int i;

	//Restore registers
	int regnum = 8;
	for(i = 0; i < 32; i++)
	{
		if (restore_regs & (1 << i))
		{
			comp_ppc_lwz(i, regnum, PPCR_SP);
			regnum += 4;
		}
	}

	comp_ppc_lwz(PPCR_SP, 0, PPCR_SP); //Read the pointer to the previous stack frame and free up stack space by using the backchain pointer
	comp_ppc_lwz(PPCR_SPECTMP, -4, PPCR_SP); //Read LR from the stackframe
	comp_ppc_mtlr(PPCR_SPECTMP); //Restore LR
}
#else
//Epilog for MacOSX Darwin ABI
void comp_ppc_epilog(uae_u32 restore_regs)
{
	int i;

	comp_ppc_lwz(PPCR_SP, 0, PPCR_SP); //Read the pointer to the previous stack frame and free up stack space by using the backchain pointer
	comp_ppc_lwz(PPCR_SPECTMP, 8, PPCR_SP); //Read LR from the stackframe
	comp_ppc_mtlr(PPCR_SPECTMP); //Restore LR

	//Restore registers
	int regnum = -4;
	for(i = 0; i < 32; i++)
	{
		if (restore_regs & (1 << i))
		{
			comp_ppc_lwz(i, regnum, PPCR_SP);
			regnum -= 4;
		}
	}
}
#endif

/* Saves a specified register to a slot in the stack frame. */
void comp_ppc_save_to_slot(int reg, int slot)
{
	//Subtract the used stack frames from prolog
	if (slot > COMP_STACKFRAME_ALLOCATED_SLOTS - PPCR_REG_USED_NONVOLATILE_NUM)
	{
		//Oops, the specified slot is not available
		write_log("Error: slot #%d is not available in JIT stack frame for saving register\n", slot);
		abort();
	}

	comp_ppc_stw(reg, nonvolatile_regs_in_stackframe + (slot * 4), PPCR_SP);
}

/* Restores the specified non-volatile register from the stack frame.
 * See PPCR_TMP_NONVOL... defines for more information. */
void comp_ppc_restore_from_slot(int reg, int slot)
{
	//Subtract the used stack frames from prolog
	if (slot > COMP_STACKFRAME_ALLOCATED_SLOTS - PPCR_REG_USED_NONVOLATILE_NUM)
	{
		//Oops, the specified slot is not available
		write_log("Error: slot #%d is not available in JIT stack frame for restoring register\n", slot);
		abort();
	}

	comp_ppc_lwz(reg, nonvolatile_regs_in_stackframe + (slot * 4), PPCR_SP);
}

/* Saves allocated temporary registers to the stack.
 * Parameters:
 *    exceptions - registers that must be skipped while saving marked with a high
 *                 bit in this longword (e.g. R4 and R6 = (1 << 4) | (1 <<6) = %101000)
 * Returns:
 *    A longword with the saved registers: the register that was saved is marked with high bit.
 *
 * Note: this function generates macroblocks, must be called from the macroblock
 * collection phase.
 */
uae_u32 comp_ppc_save_temp_regs(uae_u32 exceptions)
{
	uae_u32 saved_regs = 0;
	int i, count = 0;

	for(i = 0; i < PPC_TMP_REGS_COUNT; i++)
	{
		uae_u32 reg = 1 << PPC_TMP_REGS[i];

		//If the register is allocated and not on the exceptions list
		if ((used_tmp_regs[i] != PPC_TMP_REG_NOTUSED) && ((exceptions & reg) == 0))
		{
			//Then save it
			saved_regs |= reg;
			count++;
		}
	}

	//Adjust stack pointer: allocate the space for the registers
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_SP,
			PPCR_SP,
			-count * 4);

	//Walk through the list of registers again and save the marked ones
	count = 0;
	for(i = 0; i < PPC_TMP_REGS_COUNT; i++)
	{
		int reg = PPC_TMP_REGS[i];
		if ((saved_regs & (1 << reg)) != 0)
		{
			comp_macroblock_push_save_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_NONE | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					reg,
					PPCR_SP,
					count * 4);
			count++;
		}
	}

	return saved_regs;
}

/* Restore saved temporary registers
 * Parameters:
 *    saved_regs - the returned longword from the matching comp_ppc_save_temp_regs().
 *
 * Note: this function generates macroblocks, must be called from the macroblock
 * collection phase.
 */
void comp_ppc_restore_temp_regs(uae_u32 saved_regs)
{
	int i, count = 0;

	//Walk through the list of registers and restore the marked ones
	for(i = 0; i < PPC_TMP_REGS_COUNT; i++)
	{
		int reg = PPC_TMP_REGS[i];
		if ((saved_regs & (1 << reg)) != 0)
		{
			comp_macroblock_push_load_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_NONE | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					reg,
					PPCR_SP,
					count * 4);
			count++;
		}
	}

	//Adjust stack pointer: free up the space
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_SP,
			PPCR_SP,
			count * 4);
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

/* Compiles "return from function" code to the code cache actual position
 * Parameters:
 *   restore_regs - restore the PPC registers from the stack that are marked with a high
 *   bit in this longword (e.g. R4 and R6 = (1 << 4) | (1 <<6) = %101000)
 */
void comp_ppc_return_to_caller(uae_u32 restore_regs)
{
	comp_ppc_epilog(restore_regs);
	comp_ppc_blr();
}

 /* Compile verification of 68k PC against the expected PC and call
  * cache miss function if these were not matching
  *
  * Note: this function cannot be called after collecting of macroblocks
  * started. There is no handling of temporary registers properly, because
  * this instruction block is not scheduled for compiling.
  * Also this function needs a previously loaded PPCR_REGS_BASE register.
  *
  * Parameters:
  *      pc_addr_exp - expected 68k PC address (value)
  */
void comp_ppc_verify_pc(uae_u8* pc_addr_exp)
{
	comp_ppc_liw(PPCR_TMP0, (uae_u32) pc_addr_exp);	//Load original PC address into tempreg1
	comp_ppc_lwz(PPCR_TMP1, COMP_GET_OFFSET_IN_REGS(pc_p), PPCR_REGS_BASE);	//Load the recent PC into tempreg2 from regs structure
	comp_ppc_cmplw(PPCR_CR_TMP0, PPCR_TMP0, PPCR_TMP1);	//Compare registers
	comp_ppc_bc(PPC_B_CR_TMP0_EQ | PPC_B_TAKEN);	//beq+ skip

	//PC is not the same as the cached
	//Dispose stack frame
	comp_ppc_epilog(PPCR_REG_USED_NONVOLATILE);

	//Continue on cache miss function
	comp_ppc_jump((uae_u32) cache_miss);

	//skip:
	comp_ppc_branch_target();
}

/* Compiles a piece of code to reload the regs.pc_p pointer to the specified address. */
void comp_ppc_reload_pc_p(uae_u8* new_pc_p)
{
	comp_ppc_liw(PPCR_SPECTMP, (uae_u32) new_pc_p);
	comp_ppc_stw(PPCR_SPECTMP, COMP_GET_OFFSET_IN_REGS(pc_p), PPCR_REGS_BASE);
}
