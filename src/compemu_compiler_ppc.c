/**
 * Code compiler for JIT compiling
 *
 * These functions are handling the collection of the macroblocks to the buffer,
 * the high level optimization of the register flow and the compiling of
 * the macroblocks into PowerPC executable instructions.
 */

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "events.h"
#include "include/memory.h"
#include "custom.h"
#include "newcpu.h"
#include "compemu.h"
#include "compemu_compiler.h"
#include "compemu_macroblock_structs.h"
#include "debug.h"

/* Number of maximum macroblocks we handle
 * Note: we assume that each instruction consists of 4 macroblocks in average,
 * this might be a wrong assumtion...
 */
#define MAXMACROBLOCKS (MAXRUN*4)

/* Collection of pre-compiled macroblocks
 */
union comp_compiler_mb_union macroblocks[MAXMACROBLOCKS];

/**
 * Macro for initializing the basic macroblock structure
 * Parameters:
 *   n - name of the new macroblock structure
 *   h - handler function
 *   ir - input registers
 *   or - output registers
 */
#define comp_mb_init(n, h, ir, or) union comp_compiler_mb_union* n = comp_compiler_get_next_macroblock(); n->base.handler=(h); n->base.input_registers=(ir); n->base.output_registers=(or); n->base.name = __func__; n->base.start = NULL; n->base.m68k_ptr = comp_current_m68k_location(); n->base.remove = FALSE;

//Pointer to the end of the macroblock buffer
int macroblock_ptr;

/**
 * Prototypes of the internal macroblock implementation functions
 */
void comp_macroblock_impl_load_register_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_register_word_extended(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_spec(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_spec_save_temps(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_word(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_word_extended(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_byte(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_map_physical_mem(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_spec(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_word(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_word_update(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_byte(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_sub(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_sub_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_sub_register_from_immediate(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add_high_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_negate_with_overflow(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_long_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_opcode_unsupported(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_low_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_high_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_register_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_register_complement_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_low_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_high_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_register_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_not_or_register_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_immed_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_xor_register_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_xor_low_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_xor_high_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_registers(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_multiply_registers_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_nzcv_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_nz_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_cv_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_check_byte_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_check_word_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_word_extended(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_byte_extended(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_rotate_and_copy_bits(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_rotate_and_mask_bits(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_rotate_and_mask_bits_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_arithmetic_shift_right_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_logic_shift_left_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_logic_shift_right_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_arithmetic_left_shift_extract_v_flag(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_left_shift_extract_c_flag(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_reg_slot(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_reg_slot(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_set_byte_from_z_flag(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_stop(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_nop(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_null_operation(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_pc_from_immediate_conditional(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_pc_from_immediate_conditional_decrement_register(union comp_compiler_mb_union* mb);

/**
 * Prototypes for local helper functions
 */
STATIC_INLINE void helper_access_memory_spec(union comp_compiler_mb_union* mb, int iswrite);
STATIC_INLINE void helper_map_physical_mem(uae_u8 inreg, uae_u8 outreg, uae_u8 tmpreg);

/**
 * Initialization of the code compiler
 */
void comp_compiler_init()
{
	//Reset actual macroblock pointer
	macroblock_ptr = 0;
}

/**
 * Clean-up for the code compiler
 */
void comp_compiler_done()
{
	//Nothing to do for cleanup right now
}

/**
 * Push a macroblock to the next position of the buffer and increase the buffer top index
 * Parameters:
 *    macroblock - pointer to the macroblock that has to be pushed to buffer
 */
union comp_compiler_mb_union* comp_compiler_get_next_macroblock()
{
	if (macroblock_ptr == MAXMACROBLOCKS)
	{
		//Oops, we ran out of the array
		write_log("Error: JIT macroblock array ran out of space, please enlarge the static array\n");
		abort();
	}

	union comp_compiler_mb_union* mb = &macroblocks[macroblock_ptr];
	macroblock_ptr++;

	return mb;
}

/**
 * Optimize the macroblocks in the buffer, the output goes back to the same buffer
 */
void comp_compiler_optimize_macroblocks()
{
	int i;

	//Optimize the compiled code only if it was enabled in the config
	if (!currprefs.compoptim) return;

	//Run thru the collected macroblocks in reverse order and calculate
	//registration usage flags
	union comp_compiler_mb_union* mb = macroblocks + (macroblock_ptr - 1);

	//At the end of the block we depend on all registers and flags, except internal flags
	uae_u64 carry = COMP_COMPILER_MACROBLOCK_REG_ALL;
	uae_u64 flagsin, flagsout;
	char remove;

	for(i = macroblock_ptr; i > 0; i-- , mb--)
	{
		flagsin = (mb->base.input_registers & (~COMP_COMPILER_MACROBLOCK_CONTROL_FLAGS));
		flagsout = (mb->base.output_registers & (~COMP_COMPILER_MACROBLOCK_CONTROL_FLAGS));

		//Remove the registers from the output that is not needed by the following instructions
		flagsout &= carry;

		//If no registers left in the output then this instruction is useless,
		//unless the "no optim" control flag was specified
		remove = mb->base.remove = (flagsout == 0) &&
					(((mb->base.output_registers & COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM) == 0) &&
						((mb->base.input_registers & COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM) == 0));

		//Which registers are not overwritten by this instruction must remain in the carry
		carry &= ~flagsout;

		//Should we keep this macroblock?
		if (!remove)
		{
			//Then add the registers which are required by this instruction to the carry
			carry |= flagsin;
		}

		//This step is only for the debug logging, it can be removed later on
		mb->base.carry_registers = carry;
	}
}

/**
 * Generate the PowerPC native code for the macroblocks in the buffer
 */
void comp_compiler_generate_code()
{
	int i;

	if (macroblock_ptr == 0)
	{
		//There are no macroblocks in the buffer
		write_jit_log("Warning: JIT macroblock array is empty at code generation\n");
		return;
	}

	//Run thru the collected macroblocks and call the code generator handler for each,
	//also check the compiling buffer top to avoid running out of the buffer
	union comp_compiler_mb_union* mb = macroblocks;
	for(i = 0; (i < macroblock_ptr) && (!comp_ppc_check_top()); i++ , mb++)
	{
		comp_compiler_macroblock_func* handler = mb->base.handler;

		//If there is a handler then call it
		if ((handler) && (!mb->base.remove))
		{
			//Store the start of the compiled code
			mb->base.start = comp_ppc_buffer_top();

			//Call handler
			handler(mb);

			//Store the end of the compiled code
			mb->base.end = comp_ppc_buffer_top();
		}
	}
}

/**
 * Dump the compiled code with the macroblocks to the console
 */
void comp_compiler_debug_dump_compiled()
{
	int i;
	uaecptr nextpc;
	char str[200];
	char inputregs_str[200];
	char outputregs_str[200];
	char carry_str[200];
	uae_u16* prev_68k_inst = NULL;

	//Dump the compiled only if it was enabled in the config
	if (!currprefs.complogcompiled) return;

	//Run thru the collected macroblocks and dump the compiled native code for the
	//console using disassembler routine
	union comp_compiler_mb_union* mb = macroblocks;
	for(i = 0; i < macroblock_ptr; i++ , mb++)
	{
		//Macroblocks won't be listed if there was no corresponding M68k instruction to it
		if (mb->base.m68k_ptr)
		{
			//Is this the same M68k instruction?
			if (prev_68k_inst != mb->base.m68k_ptr)
			{
				//No: dump it
				m68k_disasm_str(str, (uaecptr) mb->base.m68k_ptr, &nextpc, 1);
				write_jit_log("M68k: %s", str);
				prev_68k_inst = mb->base.m68k_ptr;
			}

			//Dump the name of the macroblock (bit of a hack: skip the function suffix)
			write_jit_log("Mblk: %s%s\n", mb->base.name + 21, (mb->base.remove ? " *rem*": ""));
			comp_dump_reg_usage(mb->base.input_registers, inputregs_str, TRUE);
			comp_dump_reg_usage(mb->base.output_registers, outputregs_str, TRUE);
			comp_dump_reg_usage(mb->base.carry_registers, carry_str, FALSE);
			write_jit_log("Flg: %s:%s:%s\n", carry_str, inputregs_str, outputregs_str);

			//Dump disassembled code only if there was a start address for the compiled code
			if (mb->base.start)
			{
				disassemble_compiled(mb->base.start, mb->base.end);
			}
		}
	}
}

/***********************************************************************
 * Macroblock compiling handlers
 */

/**
 * Macroblock: Unsuppordted opcode interpretive handler callback compiling function
 * Note: this function purges all temporary registers, because it calls
 * an external function.
 * Parameters:
 *    opcode - unsupported opcode number
 */
void comp_macroblock_push_opcode_unsupported(uae_u16 opcode)
{
	//Before the call write back the M68k registers and clear the temp register mapping
	comp_flush_temp_registers(FALSE);

	comp_mb_init(mb,
				comp_macroblock_impl_opcode_unsupported,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM);
	mb->unsupported.opcode = opcode;
}

void comp_macroblock_impl_opcode_unsupported(union comp_compiler_mb_union* mb)
{
	uae_u16 opcode = mb->unsupported.opcode;

	//Compile call to the interpretive emulator
	// ## liw	r3,opcode
	// ## liw	r4,&reg
	// ## liw	r0,inst_func
	// ## mtlr	r0
	// ## blrl
	comp_ppc_liw(PPCR_PARAM1, opcode);
	comp_ppc_liw(PPCR_PARAM2, (uae_u32) &regs);
	comp_ppc_call(PPCR_SPECTMP, (uae_uintptr) cpufunctbl[opcode]);
}


/**
 * Macroblock: Load flags
 * Read the M68k flags from the interpretive emulator's structure into a register
 * We assume the GCC PPC target, there are two fields in the structure: cznv and x
 * These two has to be merged into one register.
 * Have a look on md-ppc-gcc/m68k.h for the details.
 */
void comp_macroblock_push_load_flags()
{
	uae_u8 tempreg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);

	//Load flag_struct.cznv to the flags register
	comp_macroblock_push_load_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			PPCR_FLAGS,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(ccrflags.cznv));

	//Load flag_struct.x to a temp register
	comp_macroblock_push_load_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
			comp_get_gpr_for_temp_register(tempreg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(ccrflags.x));

	//Rotate X flag to the position and insert it into the flag register
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS,
			comp_get_gpr_for_temp_register(tempreg),
			16, 26, 26, FALSE);

	comp_free_temp_register(tempreg);
}

/**
 * Macroblock: Save flags
 * Write the M68k flags from register to the interpretive emulator's structure
 * We assume the GCC PPC target, there are two fields in the structure: cznv and x
 * These two fields both have to be saved, but no other operation is needed to separate the flags.
 * Have a look on md-ppc-gcc/m68k.h for the details.
 */
void comp_macroblock_push_save_flags()
{
	//Save flags register to flag_struct.cznv and avoid optimize away this block
	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_FLAGS,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(ccrflags.cznv));

	//Save flags register to flag_struct.x and avoid optimize away this block
	//There is a little trick in this: to let us skip the shifting
	//operation, we store the lower half word of the flag register,
	//because X flag should go to the same bit as C flag in the other
	//field (flag_struct.cznv).
	comp_macroblock_push_save_memory_word(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_FLAGS,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(ccrflags.x));
}

/**
 * Macroblock: Loads the M68k PC
 * Note: this macroblock won't be optimized away
 */
void comp_macroblock_push_load_pc(const cpu_history * inst_history)
{
	uae_u8 temp_reg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);

	//Load real memory pointer for the executed instruction
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg) | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			(uae_u32)inst_history->location);

	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc_p));

	//Synchronize the executed instruction pointer from the block start to the actual
	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc_oldp));

	//Load emulated instruction pointer (PC register)
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg) | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			(uae_u32)inst_history->pc);

	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc));

	comp_free_temp_register(temp_reg);
}


/**
 * Macroblock: Adds two registers then copies the result into a third and updates all
 * the flags in PPC flag registers (NZCVX)
 */
void comp_macroblock_push_add_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_add_with_flags,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = input_reg1;
	mb->three_regs_opcode.input_reg2 = input_reg2;
}

void comp_macroblock_impl_add_with_flags(union comp_compiler_mb_union* mb)
{
	comp_ppc_addco(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2,
			TRUE);
}

/**
 * Macroblock: Adds two registers then copies the result into a third without flag update.
 */
void comp_macroblock_push_add(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_add,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = input_reg1;
	mb->three_regs_opcode.input_reg2 = input_reg2;
}

void comp_macroblock_impl_add(union comp_compiler_mb_union* mb)
{
	comp_ppc_add(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2,
			FALSE);
}

/**
 * Macroblock: Subtracts a register from another register then copies the result into a third without flag update.
 */
void comp_macroblock_push_sub(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 subtrahend_input_reg1, uae_u8 minuend_input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_sub,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = subtrahend_input_reg1;
	mb->three_regs_opcode.input_reg2 = minuend_input_reg2;
}

void comp_macroblock_impl_sub(union comp_compiler_mb_union* mb)
{
	comp_ppc_subf(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2,
			FALSE);
}

/**
 * Macroblock: Subtracts a register from another register then copies
 * the result into a third and updates all the flags in PPC flag registers (NZCVX)
 */
void comp_macroblock_push_sub_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 subtrahend_input_reg1, uae_u8 minuend_input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_sub_with_flags,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = subtrahend_input_reg1;
	mb->three_regs_opcode.input_reg2 = minuend_input_reg2;
}

void comp_macroblock_impl_sub_with_flags(union comp_compiler_mb_union* mb)
{
	comp_ppc_subfco(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2,
			TRUE);
}

/**
 * Macroblock: Subtracts a register from an immediate then copies the result into a second register.
 */
void comp_macroblock_push_sub_register_from_immediate(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 subtrahend_input_reg, uae_u8 minuend_input_imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_sub_register_from_immediate,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = subtrahend_input_reg;
	mb->two_regs_imm_opcode.immediate = minuend_input_imm;
}

void comp_macroblock_impl_sub_register_from_immediate(union comp_compiler_mb_union* mb)
{
	comp_ppc_subfic(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: Copies a long date from a register into another register and updates NZ flags in PPC flag registers
 */
void comp_macroblock_push_copy_register_long_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_register_long_with_flags,
				regsin,
				regsout | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ);
	mb->two_regs_opcode.output_reg = output_reg;
	mb->two_regs_opcode.input_reg = input_reg;
}

void comp_macroblock_impl_copy_register_long_with_flags(union comp_compiler_mb_union* mb)
{
	comp_ppc_mr(mb->two_regs_opcode.output_reg, mb->two_regs_opcode.input_reg, TRUE);
}

/**
 * Macroblock: Copies a register into another register without flag update
 * Note: this function is guaranteed not to allocate a temp register,
 * it is safe to use it for copying a previously not mapped register into a
 * mapped temporary register.
 */
void comp_macroblock_push_copy_register_long(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_register_long,
				regsin, regsout);
	mb->two_regs_opcode.output_reg = output_reg;
	mb->two_regs_opcode.input_reg = input_reg;
}

void comp_macroblock_impl_copy_register_long(union comp_compiler_mb_union* mb)
{
	comp_ppc_mr(mb->two_regs_opcode.output_reg, mb->two_regs_opcode.input_reg, FALSE);
}

/**
 * Macroblock: Copies a word data from a register into another register
 */
void comp_macroblock_push_copy_register_word(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			regsout,
			output_reg,
			input_reg,
			0, 16, 31, FALSE);
}

/**
 * Macroblock: Copies a byte data from a register into another register
 */
void comp_macroblock_push_copy_register_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			regsout,
			output_reg,
			input_reg,
			0, 24, 31, FALSE);
}

/**
 * Macroblock: Loads an immediate longword value into a temporary register
 */
void comp_macroblock_push_load_register_long(uae_u64 regsout, uae_u8 output_reg, uae_u32 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_register_long,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				regsout);
	mb->load_register.immediate = imm;
	mb->load_register.output_reg = output_reg;
}

void comp_macroblock_impl_load_register_long(union comp_compiler_mb_union* mb)
{
	comp_ppc_liw(mb->load_register.output_reg, mb->load_register.immediate);
}

/**
 * Macroblock: Loads an immediate word value into a temporary register
 * sign extended to longword
 */
void comp_macroblock_push_load_register_word_extended(uae_u64 regsout, uae_u8 output_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_register_word_extended,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				regsout);
	mb->load_register.immediate = imm;
	mb->load_register.output_reg = output_reg;
}

void comp_macroblock_impl_load_register_word_extended(union comp_compiler_mb_union* mb)
{
	comp_ppc_li(mb->load_register.output_reg, (uae_u16)mb->load_register.immediate);
}

/**
 * Macroblock: Loads a longword data from memory into a temporary register
 */
void comp_macroblock_push_load_memory_long(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_long,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = output_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_load_memory_long(union comp_compiler_mb_union* mb)
{
	comp_ppc_lwz(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Loads a word data from memory into a temporary register
 */
void comp_macroblock_push_load_memory_word(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_word,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = output_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_load_memory_word(union comp_compiler_mb_union* mb)
{
	comp_ppc_lhz(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Loads a word data from memory into a temporary register,
 * data is sign-extended to longword
 */
void comp_macroblock_push_load_memory_word_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_word_extended,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = output_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_load_memory_word_extended(union comp_compiler_mb_union* mb)
{
	comp_ppc_lha(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Loads a byte data from memory into a temporary register
 */
void comp_macroblock_push_load_memory_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_byte,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = output_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_load_memory_byte(union comp_compiler_mb_union* mb)
{
	comp_ppc_lbz(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Saves a longword data from a temporary register into memory
 */
void comp_macroblock_push_save_memory_long(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_long,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = source_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_save_memory_long(union comp_compiler_mb_union* mb)
{
	comp_ppc_stw(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Saves a word data from a temporary register into memory
 */
void comp_macroblock_push_save_memory_word(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_word,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = source_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_save_memory_word(union comp_compiler_mb_union* mb)
{
	comp_ppc_sth(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Saves a word data from a temporary register into memory with updating the register before saving
 */
void comp_macroblock_push_save_memory_word_update(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_word_update,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = source_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_save_memory_word_update(union comp_compiler_mb_union* mb)
{
	comp_ppc_sthu(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Saves a byte data from a temporary register into memory
 */
void comp_macroblock_push_save_memory_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset)
{
	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_byte,
				regsin, regsout);
	mb->access_memory.offset = offset;
	mb->access_memory.output_reg = source_reg;
	mb->access_memory.base_reg = base_reg;
}

void comp_macroblock_impl_save_memory_byte(union comp_compiler_mb_union* mb)
{
	comp_ppc_stb(mb->access_memory.output_reg, mb->access_memory.offset, mb->access_memory.base_reg);
}

/**
 * Macroblock: Loads the physical address of a mapped memory address into a register
 */
void comp_macroblock_push_map_physical_mem(uae_u64 regsin, uae_u64 regsout, uae_u8 dest_mem_reg, uae_u8 source_reg)
{
	uae_u8 tmpreg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);

	comp_mb_init(mb,
				comp_macroblock_impl_map_physical_mem,
				regsin, regsout);
	mb->map_physical_mem.output_reg = dest_mem_reg;
	mb->map_physical_mem.input_reg = source_reg;
	mb->map_physical_mem.temp_reg = comp_get_gpr_for_temp_register(tmpreg);

	comp_free_temp_register(tmpreg);
}

void comp_macroblock_impl_map_physical_mem(union comp_compiler_mb_union* mb)
{
	helper_map_physical_mem(
			mb->map_physical_mem.input_reg,
			mb->map_physical_mem.output_reg,
			mb->map_physical_mem.temp_reg);
}

/** Macroblock helper: implementation of the physical address mapping
 *  Parameters:
 *     inreg - source 68k address
 *     outreg - output register for the result (physical address)
 *     tmpreg - temporary register for the calculation
 *  Note: inreg and outreg can be the same, but the tmpreg must be an independent
 *  register which is trashed.
 *  PPCR_SPECTMP (R0) is used by this function, it cannot be specified as any of the parameter registers.
 */
STATIC_INLINE void helper_map_physical_mem(uae_u8 inreg, uae_u8 outreg, uae_u8 tmpreg)
{
	//TODO: preload base address array start into a GPR
	//Load base address array address to tmp reg
	comp_ppc_liw(tmpreg, (uae_u32)baseaddr);

	//Get 64k page index into R0: R0 = (inreg >> 14) & 0x3fffc;
	comp_ppc_rlwinm(PPCR_SPECTMP, inreg, 18, 14, 29, FALSE);

	//Load base address for the memory block to temp reg: rtmp = rtmp[r0]
	comp_ppc_lwzx(tmpreg, tmpreg, PPCR_SPECTMP);

	//TODO: base and offset could be sent separately and used in instruction code
	//Sum address and offset to outreg: outreg = inreg + rtmp
	comp_ppc_add(outreg, inreg, tmpreg, FALSE);
}

/**
 * Macroblock: Writes a value into emulated memory from a specified register
 * using the special memory bank handlers.
 * Note: this function purges all temporary registers, because it calls
 * an external function.
 */
void comp_macroblock_push_save_memory_spec(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u8 dest_mem_reg, uae_u8 size)
{
	//Before the call write back the M68k registers and clear the temp register mapping
	//Warning is suppressed, we are in the middle of some instruction compiling.
	//probably there are temporary registers, but we cannot do anything about it
	comp_flush_temp_registers(TRUE);

	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_spec,
				regsin, regsout);
	mb->access_memory_size.base_reg = dest_mem_reg;
	mb->access_memory_size.output_reg = source_reg;
	mb->access_memory_size.size = size;
}

void comp_macroblock_impl_save_memory_spec(union comp_compiler_mb_union* mb)
{
	helper_access_memory_spec(mb, TRUE);
}

/**
 * Macroblock: Reads a value into a specified register from emulated memory
 * using the special memory bank handlers.
 * Note: this function purges all temporary registers, because it calls
 * an external function.
 */
void comp_macroblock_push_load_memory_spec(uae_u64 regsin, uae_u64 regsout, uae_u8 src_mem_reg, uae_u8 dest_reg, uae_u8 size)
{
	//Before the call write back the M68k registers and clear the temp register mapping
	//Warning is suppressed, we are in the middle of some instruction compiling.
	//probably there are temporary registers, but we cannot do anything about it
	comp_flush_temp_registers(TRUE);

	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_spec,
				regsin, regsout);
	mb->access_memory_size.base_reg = src_mem_reg;
	mb->access_memory_size.output_reg = dest_reg;	//Copy the result into this register
	mb->access_memory_size.size = size;
}

void comp_macroblock_impl_load_memory_spec(union comp_compiler_mb_union* mb)
{
	helper_access_memory_spec(mb, FALSE);
}

/**
 * Macroblock: Reads a value into a specified register from emulated memory
 * using the special memory bank handlers.
 * Note: this function preserves all allocated temporary registers,
 * because it calls an external function.
 * If possible then use comp_macroblock_push_load_memory_spec() instead.
 */
void comp_macroblock_push_load_memory_spec_save_temps(uae_u64 regsin, uae_u64 regsout, uae_u8 src_mem_reg, uae_u8 dest_reg, uae_u8 size)
{
	//Save the allocated temporary register, except the destination register
	uae_u32 saved_regs = comp_ppc_save_temp_regs(1 << dest_reg);

	comp_mb_init(mb,
				comp_macroblock_impl_load_memory_spec_save_temps,
				regsin, regsout);
	mb->access_memory_size.base_reg = src_mem_reg;
	mb->access_memory_size.output_reg = dest_reg;	//Copy the result into this register
	mb->access_memory_size.size = size;

	//Restore the saved temporary registers
	comp_ppc_restore_temp_regs(saved_regs);
}

void comp_macroblock_impl_load_memory_spec_save_temps(union comp_compiler_mb_union* mb)
{
	helper_access_memory_spec(mb, FALSE);
}

/**
 * Helper function for compiling special memory access
 * Parameters:
 *    mb - pointer to the actual macroblock descriptor
 *    iswrite - if TRUE then it is a write operation otherwise it is read
 */
STATIC_INLINE void helper_access_memory_spec(union comp_compiler_mb_union* mb, int iswrite)
{
	uae_u8 srcreg = mb->access_memory_size.output_reg;
	uae_u8 basereg = mb->access_memory_size.base_reg;
	addrbank refaddrstruct;		//We don't really use this, only for calculating the offsets in the structure
	uae_u16 handleroffset;

	//At this stage all temporary registers are free'd,
	//we need to operate on raw registers without mapping.
	//But we have to be careful not to overwrite the other register
	//while moving registers.

	//Move the output value to R4, but avoid
	//any conflicts with the target address register (only if it is write access)
	if ((srcreg != PPCR_PARAM2) && (iswrite))
	{
		//Source register is not R4, we need to move the value
		if (basereg != PPCR_PARAM2)
		{
			//We are lucky: the address register is not R4, safe to overwrite
			comp_ppc_mr(PPCR_PARAM2, srcreg, FALSE);
		} else {
			//Oops, the target register is the same as the address register,
			//move the amount thru R0 and move the address to R3
			comp_ppc_mr(PPCR_SPECTMP, srcreg, FALSE);		//R0 = srcreg
			comp_ppc_mr(PPCR_PARAM1, basereg, FALSE);		//R3 = basereg (R4)
			comp_ppc_mr(PPCR_PARAM2, PPCR_SPECTMP, FALSE);	//R4 = R0
			basereg = PPCR_PARAM1;
		}
	}

	//Move the address to R3, if it is not already in that register
	if (basereg != PPCR_PARAM1)
	{
		comp_ppc_mr(PPCR_PARAM1, basereg, FALSE);
	}

	//At this point the registers are:
	//R3 = address
	//R4 = value (if it is write access)

	//Get 64k page index into R0: R0 = (inreg >> 14) & 0x3fffc;
	comp_ppc_rlwinm(PPCR_SPECTMP, PPCR_PARAM1, 18, 14, 29, FALSE);

	//Load the mem_banks array address into PPCR_TMP2
	//This is a bit of a hack: we assume that PPCR_TMP2 is neither PPCR_PARAM1 nor PPCR_PARAM2
	//TODO: we could load this from the regs structure
	comp_ppc_liw(PPCR_TMP2, (uae_u32)mem_banks);

	//Load address structure pointer for the memory block to PPCR_TMP2: r5 = r5[r0]
	comp_ppc_lwzx(PPCR_TMP2, PPCR_TMP2, PPCR_SPECTMP);

	//Select the handler according to the size
	switch(mb->access_memory_size.size)
	{
	case 1:
		//Byte
		if (iswrite)
		{
			handleroffset = ((void*)&refaddrstruct.bput) - ((void*)&refaddrstruct);
		} else {
			handleroffset = ((void*)&refaddrstruct.bget) - ((void*)&refaddrstruct);
		}
		break;
	case 2:
		//Word
		if (iswrite)
		{
			handleroffset = ((void*)&refaddrstruct.wput) - ((void*)&refaddrstruct);
		} else {
			handleroffset = ((void*)&refaddrstruct.wget) - ((void*)&refaddrstruct);
		}
		break;
	case 4:
		//Long
		if (iswrite)
		{
			handleroffset = ((void*)&refaddrstruct.lput) - ((void*)&refaddrstruct);
		} else {
			handleroffset = ((void*)&refaddrstruct.lget) - ((void*)&refaddrstruct);
		}
		break;
	default:
		write_log("JIT error: unknown memory write size: %d\n", mb->access_memory_size.size);
		abort();
	}

	//Load the address of the handler to PPCR_TMP2
	comp_ppc_lwz(PPCR_TMP2, handleroffset, PPCR_TMP2);

	//Call handler
	comp_ppc_call_reg(PPCR_TMP2);

	//If it was read access then the result is returned in R3,
	//copy to the specified register if it was not there already
	if ((!iswrite) && (srcreg != PPCR_PARAM1))
	{
		comp_ppc_mr(srcreg, PPCR_PARAM1, FALSE);
	}
}

/**
 * Macroblock: OR a 16 bit immediate to the lower half word of a register and put it into a new register
 */
void comp_macroblock_push_or_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_or_low_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = imm;
}

void comp_macroblock_impl_or_low_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_ori(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: OR a 16 bit immediate to the higher half word of a register and put it into a new register
 */
void comp_macroblock_push_or_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_or_high_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = imm;
}

void comp_macroblock_impl_or_high_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_oris(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: OR a register to another register
 */
void comp_macroblock_push_or_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_or_register_register,
				regsin, regsout);
	mb->three_regs_opcode_flags.output_reg = output_reg;
	mb->three_regs_opcode_flags.input_reg1 = input_reg1;
	mb->three_regs_opcode_flags.input_reg2 = input_reg2;
	mb->three_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_or_register_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_or(
			mb->three_regs_opcode_flags.output_reg,
			mb->three_regs_opcode_flags.input_reg1,
			mb->three_regs_opcode_flags.input_reg2,
			mb->three_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: OR a register to another register
 */
void comp_macroblock_push_not_or_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_not_or_register_register,
				regsin, regsout);
	mb->three_regs_opcode_flags.output_reg = output_reg;
	mb->three_regs_opcode_flags.input_reg1 = input_reg1;
	mb->three_regs_opcode_flags.input_reg2 = input_reg2;
	mb->three_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_not_or_register_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_nor(
			mb->three_regs_opcode_flags.output_reg,
			mb->three_regs_opcode_flags.input_reg1,
			mb->three_regs_opcode_flags.input_reg2,
			mb->three_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: AND a 16 bit immediate to the lower half word of a register and put it into a new register
 */
void comp_macroblock_push_and_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_low_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = immediate;
}

void comp_macroblock_impl_and_low_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_andi(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: AND a 16 bit immediate to the higher half word of a register and put it into a new register
 */
void comp_macroblock_push_and_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_high_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = immediate;
}

void comp_macroblock_impl_and_high_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_andis(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: AND a register to another register
 */
void comp_macroblock_push_and_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_register_register,
				regsin, regsout);
	mb->three_regs_opcode_flags.output_reg = output_reg;
	mb->three_regs_opcode_flags.input_reg1 = input_reg1;
	mb->three_regs_opcode_flags.input_reg2 = input_reg2;
	mb->three_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_and_register_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_and(
			mb->three_regs_opcode_flags.output_reg,
			mb->three_regs_opcode_flags.input_reg1,
			mb->three_regs_opcode_flags.input_reg2,
			mb->three_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: AND a register to another register's complement
 */
void comp_macroblock_push_and_register_complement_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_compl_reg2, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_register_complement_register,
				regsin, regsout);
	mb->three_regs_opcode_flags.output_reg = output_reg;
	mb->three_regs_opcode_flags.input_reg1 = input_reg1;
	mb->three_regs_opcode_flags.input_reg2 = input_compl_reg2;
	mb->three_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_and_register_complement_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_andc(
			mb->three_regs_opcode_flags.output_reg,
			mb->three_regs_opcode_flags.input_reg1,
			mb->three_regs_opcode_flags.input_reg2,
			mb->three_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: XOR a register to another register
 */
void comp_macroblock_push_xor_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_xor_register_register,
				regsin, regsout);
	mb->three_regs_opcode_flags.output_reg = output_reg;
	mb->three_regs_opcode_flags.input_reg1 = input_reg1;
	mb->three_regs_opcode_flags.input_reg2 = input_reg2;
	mb->three_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_xor_register_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_xor(
			mb->three_regs_opcode_flags.output_reg,
			mb->three_regs_opcode_flags.input_reg1,
			mb->three_regs_opcode_flags.input_reg2,
			mb->three_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: XOR an immediate to the low half word of a register
 */
void comp_macroblock_push_xor_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate)
{
	comp_mb_init(mb,
				comp_macroblock_impl_xor_low_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = immediate;
}

void comp_macroblock_impl_xor_low_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_xori(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: XOR an immediate to the high half word of a register
 */
void comp_macroblock_push_xor_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate)
{
	comp_mb_init(mb,
				comp_macroblock_impl_xor_high_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = immediate;
}

void comp_macroblock_impl_xor_high_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_xoris(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: Add a word immediate to a register and put it into a new register
 * Note: the immediate is sign extended to 32 bit.
 */
void comp_macroblock_push_add_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_add_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = imm;
}

void comp_macroblock_impl_add_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_addi(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: Add a word immediate to the high half word of a register and
 * put it into a new register
 * Note: the immediate is sign extended to 32 bit.
 */
void comp_macroblock_push_add_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_add_high_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = imm;
}

void comp_macroblock_impl_add_high_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_addis(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: Negate register into another register with overflow flag set.
 */
void comp_macroblock_push_negate_with_overflow(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_negate_with_overflow,
				regsin,
				regsout);
	mb->two_regs_opcode_flags.input_reg = input_reg;
	mb->two_regs_opcode_flags.output_reg = output_reg;
	mb->two_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_negate_with_overflow(union comp_compiler_mb_union* mb)
{
	comp_ppc_nego(mb->two_regs_opcode_flags.output_reg, mb->two_regs_opcode_flags.input_reg, mb->two_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: AND an immediate to a register and put it into a new register
 * Note: the higher half word of the register will be cleared (andi instruction)
 */
void comp_macroblock_push_and_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_register_imm,
				regsin, regsout);
	mb->two_regs_imm_opcode.output_reg = output_reg;
	mb->two_regs_imm_opcode.input_reg = input_reg;
	mb->two_regs_imm_opcode.immediate = imm;
}

void comp_macroblock_impl_and_register_imm(union comp_compiler_mb_union* mb)
{
	comp_ppc_andi(
			mb->two_regs_imm_opcode.output_reg,
			mb->two_regs_imm_opcode.input_reg,
			mb->two_regs_imm_opcode.immediate);
}

/**
 * Macroblock: AND a register to another register and put it into a new register
 * Note: the higher half word of the register will be cleared (andi instruction)
 */
void comp_macroblock_push_and_registers(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_and_registers,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = input_reg1;
	mb->three_regs_opcode.input_reg2 = input_reg2;
}

void comp_macroblock_impl_and_registers(union comp_compiler_mb_union* mb)
{
	comp_ppc_and(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2, FALSE);
}

/**
 * Macroblock: multiply the low half word of a register by the low half word
 * of another register and set the arithmetic flags
 */
void comp_macroblock_push_multiply_registers_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2)
{
	comp_mb_init(mb,
				comp_macroblock_impl_multiply_registers_with_flags,
				regsin, regsout);
	mb->three_regs_opcode.output_reg = output_reg;
	mb->three_regs_opcode.input_reg1 = input_reg1;
	mb->three_regs_opcode.input_reg2 = input_reg2;
}

void comp_macroblock_impl_multiply_registers_with_flags(union comp_compiler_mb_union* mb)
{
	comp_ppc_mullwo(
			mb->three_regs_opcode.output_reg,
			mb->three_regs_opcode.input_reg1,
			mb->three_regs_opcode.input_reg2,
			TRUE);
}

/**
 * Macroblock: Copy all (N, Z, C, V) PPC flag registers into a GPR register
 * Note: this macroblock depends on the internal PPC flags: N, Z, C, V
 */
void comp_macroblock_push_copy_nzcv_flags_to_register(uae_u64 regsout, uae_u8 output_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_nzcv_flags_to_register,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				regsout);
	mb->one_reg_opcode.reg = output_reg;
}

void comp_macroblock_impl_copy_nzcv_flags_to_register(union comp_compiler_mb_union* mb)
{
#ifdef _ARCH_PWR4
	//Copy CR0 to the output register (N, Z)
	comp_ppc_mfocrf(PPCR_CR_TMP0, mb->one_reg_opcode.reg);
	//Copy XER to PPCR_SPECTMP
	comp_ppc_mfxer(PPCR_SPECTMP);
	//Insert the XER bits into the output register (C, V)
	comp_ppc_rlwimi(mb->one_reg_opcode.reg, PPCR_SPECTMP, 24, 8, 11, FALSE);
#else
	//Copy XER to CR2
	comp_ppc_mcrxr(PPCR_CR_TMP2);
	//Copy CR2 to the output register
	comp_ppc_mfcr(mb->one_reg_opcode.reg);
#endif
}

/**
 * Macroblock: Copy N and Z PPC flag registers into a GPR register
 * Note: this macroblock depends on the internal PPC flags: N and Z
 */
void comp_macroblock_push_copy_nz_flags_to_register(uae_u64 regsout, uae_u8 output_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_nz_flags_to_register,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				regsout);
	mb->one_reg_opcode.reg = output_reg;
}

void comp_macroblock_impl_copy_nz_flags_to_register(union comp_compiler_mb_union* mb)
{
#ifdef _ARCH_PWR4
	comp_ppc_mfocrf(PPCR_CR_TMP0, mb->one_reg_opcode.reg);
#else
	comp_ppc_mfcr(mb->one_reg_opcode.reg);
#endif
}

/**
 * Macroblock: Copy C and V PPC flag registers into a GPR register
 * Note: this macroblock depends on the internal PPC flags: C and V
 */
void comp_macroblock_push_copy_cv_flags_to_register(uae_u64 regsout, uae_u8 output_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_cv_flags_to_register,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				regsout);
	mb->one_reg_opcode.reg = output_reg;
}

void comp_macroblock_impl_copy_cv_flags_to_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_mfxer(mb->one_reg_opcode.reg);
}

/**
 * Macroblock: Check long-sized register value and set the PPC N and Z flag according
 * to the content of the register half word.
 */
void comp_macroblock_push_check_long_register(uae_u64 regsin, uae_u8 input_reg)
{
	//It is a simple register move with flag checking
	//The target register is irrelevant, so we can use r0
	comp_macroblock_push_copy_register_long_with_flags(
				regsin,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN,
				PPCR_SPECTMP,
				input_reg);
}

/**
 * Macroblock: Check word-sized register value and set the PPC N and Z flag according
 * to the content of the register half word.
 */
void comp_macroblock_push_check_word_register(uae_u64 regsin, uae_u8 input_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_check_word_register,
				regsin,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN);
	mb->one_reg_opcode.reg = input_reg;
}

void comp_macroblock_impl_check_word_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_extsh(PPCR_SPECTMP, mb->one_reg_opcode.reg, TRUE);
}

/**
 * Macroblock: Sign-extend lower word content of the specified register
 * into another register.
 */
void comp_macroblock_push_copy_register_word_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_register_word_extended,
				regsin,
				regsout);
	mb->two_regs_opcode_flags.input_reg = input_reg;
	mb->two_regs_opcode_flags.output_reg = output_reg;
	mb->two_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_copy_register_word_extended(union comp_compiler_mb_union* mb)
{
	comp_ppc_extsh(mb->two_regs_opcode_flags.output_reg, mb->two_regs_opcode_flags.input_reg, mb->two_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: Sign-extend lowest byte content of the specified register
 * into another register.
 */
void comp_macroblock_push_copy_register_byte_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, char updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_copy_register_byte_extended,
				regsin,
				regsout);
	mb->two_regs_opcode_flags.input_reg = input_reg;
	mb->two_regs_opcode_flags.output_reg = output_reg;
	mb->two_regs_opcode_flags.updateflags = updateflags;
}

void comp_macroblock_impl_copy_register_byte_extended(union comp_compiler_mb_union* mb)
{
	comp_ppc_extsb(mb->two_regs_opcode.output_reg, mb->two_regs_opcode.input_reg, mb->two_regs_opcode_flags.updateflags);
}

/**
 * Macroblock: Check byte-sized register value and set the PPC N and Z flag according
 * to the content of the register half word.
 */
void comp_macroblock_push_check_byte_register(uae_u64 regsin, uae_u8 input_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_check_byte_register,
				regsin,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN);
	mb->one_reg_opcode.reg = input_reg;
}

void comp_macroblock_impl_check_byte_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_extsb(PPCR_SPECTMP, mb->one_reg_opcode.reg, TRUE);
}

/**
 * Macroblock: Rotate and copy specified bits
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_rotate_and_copy_bits(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, uae_u8 maskb, uae_u8 maske, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_rotate_and_copy_bits,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode_with_mask.output_reg = output_reg;
	mb->shift_opcode_with_mask.input_reg = input_reg;
	mb->shift_opcode_with_mask.shift = shift;
	mb->shift_opcode_with_mask.begin_mask = maskb;
	mb->shift_opcode_with_mask.end_mask = maske;
	mb->shift_opcode_with_mask.update_flags = updateflags;
}

void comp_macroblock_impl_rotate_and_copy_bits(union comp_compiler_mb_union* mb)
{
	comp_ppc_rlwimi(
			mb->shift_opcode_with_mask.output_reg,
			mb->shift_opcode_with_mask.input_reg,
			mb->shift_opcode_with_mask.shift,
			mb->shift_opcode_with_mask.begin_mask,
			mb->shift_opcode_with_mask.end_mask,
			mb->shift_opcode_with_mask.update_flags);
}

/**
 * Macroblock: Rotate by immediate and mask specified bits
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_rotate_and_mask_bits(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, uae_u8 maskb, uae_u8 maske, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_rotate_and_mask_bits,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode_with_mask.output_reg = output_reg;
	mb->shift_opcode_with_mask.input_reg = input_reg;
	mb->shift_opcode_with_mask.shift = shift;
	mb->shift_opcode_with_mask.begin_mask = maskb;
	mb->shift_opcode_with_mask.end_mask = maske;
	mb->shift_opcode_with_mask.update_flags = updateflags;
}

void comp_macroblock_impl_rotate_and_mask_bits(union comp_compiler_mb_union* mb)
{
	comp_ppc_rlwinm(
			mb->shift_opcode_with_mask.output_reg,
			mb->shift_opcode_with_mask.input_reg,
			mb->shift_opcode_with_mask.shift,
			mb->shift_opcode_with_mask.begin_mask,
			mb->shift_opcode_with_mask.end_mask,
			mb->shift_opcode_with_mask.update_flags);
}

/**
 * Macroblock: Rotate by register and mask specified bits
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_rotate_and_mask_bits_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift_reg, uae_u8 maskb, uae_u8 maske, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_rotate_and_mask_bits_register,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode_with_mask.output_reg = output_reg;
	mb->shift_opcode_with_mask.input_reg = input_reg;
	mb->shift_opcode_with_mask.shift = shift_reg;
	mb->shift_opcode_with_mask.begin_mask = maskb;
	mb->shift_opcode_with_mask.end_mask = maske;
	mb->shift_opcode_with_mask.update_flags = updateflags;
}

void comp_macroblock_impl_rotate_and_mask_bits_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_rlwnm(
			mb->shift_opcode_with_mask.output_reg,
			mb->shift_opcode_with_mask.input_reg,
			mb->shift_opcode_with_mask.shift,
			mb->shift_opcode_with_mask.begin_mask,
			mb->shift_opcode_with_mask.end_mask,
			mb->shift_opcode_with_mask.update_flags);
}

/**
 * Macroblock: Arithmetic shift to the right of the register
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_arithmetic_shift_right_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_arithmetic_shift_right_register,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode.output_reg = output_reg;
	mb->shift_opcode.input_reg = input_reg;
	mb->shift_opcode.shift = shift;
	mb->shift_opcode.update_flags = updateflags;
}

void comp_macroblock_impl_arithmetic_shift_right_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_srawi(
			mb->shift_opcode.output_reg,
			mb->shift_opcode.input_reg,
			mb->shift_opcode.shift,
			mb->shift_opcode.update_flags);
}

/**
 * Macroblock: Logic shift to the left of the register
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_logic_shift_left_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_logic_shift_left_register,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode.output_reg = output_reg;
	mb->shift_opcode.input_reg = input_reg;
	mb->shift_opcode.shift = shift;
	mb->shift_opcode.update_flags = updateflags;
}

void comp_macroblock_impl_logic_shift_left_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_slw(
			mb->shift_opcode.output_reg,
			mb->shift_opcode.input_reg,
			mb->shift_opcode.shift,
			mb->shift_opcode.update_flags);
}

/**
 * Macroblock: Logic shift to the right of the register
 * Note: when flag update is specified then the output registers will specify
 * internal flag update
 */
void comp_macroblock_push_logic_shift_right_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, int updateflags)
{
	comp_mb_init(mb,
				comp_macroblock_impl_logic_shift_right_register,
				regsin,
				regsout | (updateflags ?
								COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ :
								COMP_COMPILER_MACROBLOCK_REG_NONE));
	mb->shift_opcode.output_reg = output_reg;
	mb->shift_opcode.input_reg = input_reg;
	mb->shift_opcode.shift = shift;
	mb->shift_opcode.update_flags = updateflags;
}

void comp_macroblock_impl_logic_shift_right_register(union comp_compiler_mb_union* mb)
{
	comp_ppc_srw(
			mb->shift_opcode.output_reg,
			mb->shift_opcode.input_reg,
			mb->shift_opcode.shift,
			mb->shift_opcode.update_flags);
}

/**
 * Macroblock: Calculate V flag for arithmetic left shift, inserts the V flag directly
 * into the flag emulation register
 */
void comp_macroblock_push_arithmetic_left_shift_extract_v_flag(uae_u64 regsin, uae_u8 input_reg, uae_u8 shift_reg, uae_u8 tmp_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_arithmetic_left_shift_extract_v_flag,
				regsin,
				COMP_COMPILER_MACROBLOCK_REG_FLAGV);
	mb->extract_v_flag_arithmetic_left_shift.input_reg = input_reg;
	mb->extract_v_flag_arithmetic_left_shift.shift_reg = shift_reg;
	mb->extract_v_flag_arithmetic_left_shift.temp_reg = tmp_reg;
}

void comp_macroblock_impl_arithmetic_left_shift_extract_v_flag(union comp_compiler_mb_union* mb)
{
	uae_u8 input_reg = mb->extract_v_flag_arithmetic_left_shift.input_reg;
	uae_u8 temp_reg = mb->extract_v_flag_arithmetic_left_shift.temp_reg;
	uae_u8 shift_reg = mb->extract_v_flag_arithmetic_left_shift.shift_reg;

	//Count leading 0 bits to R0
	comp_ppc_cntlwz(PPCR_SPECTMP, input_reg, FALSE);

	//Invert the source register into the temp register
	comp_ppc_nor(temp_reg, input_reg, input_reg, FALSE);

	//Count leading 0 bits again to the temp register
	//(counting leading 1 bits by using the result from the previous inversion)
	comp_ppc_cntlwz(temp_reg, temp_reg, FALSE);

	//Calculate the distance of the first changing bit to temp register,
	//also clear XER[CA] (Carry flag) for the next instruction
	comp_ppc_addc(temp_reg, temp_reg, PPCR_SPECTMP, FALSE);

	//Subtract the number of shifting steps from the calculated distance
	//and decrease it by one (tmpreg = tmpreg - shift - 1).
	//By this we get a negative number if the number of the steps is
	//equal or less than the distance of the first bit change.
	comp_ppc_subfe(temp_reg, shift_reg, temp_reg, FALSE);

	//If the result is negative then the overflow happens while shifting
	//V flag should be set according to the MSB
	comp_ppc_rlwimi(PPCR_FLAGS, temp_reg, 22, 9, 9, FALSE);
}

/**
 * Macroblock: Calculate C flag for shift, inserts the C flag directly
 * into the flag emulation register.
 * If shift equals to 0 and left_shift is TRUE, or shift equals to 32 and left_shift is FALSE,
 * then the C flag cleared (no bit shift was done).
 * Note: this macroblock uses CRF0 for comparison.
 */
void comp_macroblock_push_left_shift_extract_c_flag(uae_u64 regsin, uae_u8 input_reg, uae_u8 shift_reg, BOOL left_shift)
{
	comp_mb_init(mb,
				comp_macroblock_impl_left_shift_extract_c_flag,
				regsin,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC);
	mb->extract_c_flag_shift.input_reg = input_reg;
	mb->extract_c_flag_shift.shift_reg = shift_reg;
	mb->extract_c_flag_shift.left_shift = left_shift;
}

void comp_macroblock_impl_left_shift_extract_c_flag(union comp_compiler_mb_union* mb)
{
	//Is the shift register equals to zero (left shift) or 32 (right shift)?
	comp_ppc_cmplwi(PPCR_CR_TMP0,
				    mb->extract_c_flag_shift.shift_reg,
				    mb->extract_c_flag_shift.left_shift ? 0 : 32);

	//Then skip the extraction
	comp_ppc_bc(PPC_B_CR_TMP0_EQ | PPC_B_NONTAKEN, 0);

	//Extract C flag from lowest bit to temp reg
	comp_ppc_rlwnm(PPCR_SPECTMP,
				  mb->extract_c_flag_shift.input_reg,
				  mb->extract_c_flag_shift.shift_reg,
				  0, 31, FALSE);

	//Copy to flag register
	comp_ppc_rlwimi(PPCR_FLAGS,
				  PPCR_SPECTMP,
				  mb->extract_c_flag_shift.left_shift ? 21 : 22,
				  10, 10, FALSE);

	//Branch target 0 is reached
	comp_ppc_branch_target(0);
}

/**
 * Macroblock: TRAP PPC opcode to the top of the buffer
 * Note: this opcode won't be optimized away
 */
void comp_macroblock_push_stop()
{
	//Registers are not required for input to avoid any interfere with the optimization
	comp_mb_init(mb,
				comp_macroblock_impl_stop,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM);
}

void comp_macroblock_impl_stop(union comp_compiler_mb_union* mb)
{
	comp_ppc_trap();
}

/**
 * Macroblock: NOP PPC opcode to the top of the buffer, useless instruction just to mark some location in the output
 * Note: this opcode won't be optimized away
 */
void comp_macroblock_push_nop()
{
	//Registers are not required for input to avoid any interfere with the optimization
	comp_mb_init(mb,
				comp_macroblock_impl_nop,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM);
}

void comp_macroblock_impl_nop(union comp_compiler_mb_union* mb)
{
	comp_ppc_nop();
}

/**
 * Macroblock: null operation.
 * This macroblock has no output in the compiled code, can be used for
 * specific tweaking on the register flow optimizations.
 */
void comp_macroblock_push_null_operation(uae_u64 regsin, uae_u64 regsout)
{
	//There is no real macroblock behind this, only input/output register specification
	comp_mb_init(mb,
				comp_macroblock_impl_null_operation,
				regsin,
				regsout);
}

void comp_macroblock_impl_null_operation(union comp_compiler_mb_union* mb)
{
	//Do nothing
}

/**
 * Macroblock: save the specified register into the specified slot in the Regs structure
 * Slot specifies the target longword in Regs structure (see COMP_REGS_ALLOCATED_SLOTS).
 * Note: there is no checking for the allocation of the slots, make sure you know
 * which one is in use.
 */
void comp_macroblock_push_save_reg_slot(uae_u64 regsin, uae_u8 input_reg, unsigned int slot)
{
	//Registers are not required for input to avoid any interfere with the optimization
	comp_mb_init(mb,
				comp_macroblock_impl_save_reg_slot,
				regsin,
				COMP_COMPILER_MACROBLOCK_REG_NONE);
	mb->reg_in_slot.slot = slot;
	mb->reg_in_slot.reg = input_reg;
}

void comp_macroblock_impl_save_reg_slot(union comp_compiler_mb_union* mb)
{
	comp_ppc_save_to_slot(mb->reg_in_slot.reg, mb->reg_in_slot.slot);
}

/**
 * Macroblock: load the specified register from the specified slot in the Regs structure
 * Slot specifies the target longword in Regs structure (see COMP_REGS_ALLOCATED_SLOTS).
 * Note: there is no checking for the allocation of the slots, make sure you know
 * which one is in use.
 */
void comp_macroblock_push_load_reg_slot(uae_u64 regsout, uae_u8 output_reg, unsigned int slot)
{
	//Registers are not required for input to avoid any interfere with the optimization
	comp_mb_init(mb,
				comp_macroblock_impl_load_reg_slot,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				regsout);
	mb->reg_in_slot.slot = slot;
	mb->reg_in_slot.reg = output_reg;
}

void comp_macroblock_impl_load_reg_slot(union comp_compiler_mb_union* mb)
{
	comp_ppc_restore_from_slot(mb->reg_in_slot.reg, mb->reg_in_slot.slot);
}

/**
 * Macroblock: load the specified 68k address into the PC register.
 * The specified address is in the 68k emulated address space which is translated
 * to the physical memory address and load into the emulated PC register.
 */
void comp_macroblock_push_load_pc_from_register(uae_u64 regsin, uae_u8 address_reg)
{
	uae_u8 temp_reg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
	uae_u8 temp_reg_mapped = comp_get_gpr_for_temp_register(temp_reg);

	//TODO: odd address must trigger exception

	//Save it to emulated instruction pointer (PC register)
	comp_macroblock_push_save_memory_long(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			address_reg,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc));

	//Get memory address into the temp register
	comp_macroblock_push_map_physical_mem(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			temp_reg_mapped,
			address_reg);

	//Save physical memory pointer for the executed instruction
	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			temp_reg_mapped,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc_p));

	//Synchronize the executed instruction pointer from the block start to the actual
	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			temp_reg_mapped,
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc_oldp));

	comp_free_temp_register(temp_reg);
}

/**
 * Macroblock: load the specified 68k address into the PC register
 * or do nothing according to the Z flag state.
 * Target address is loaded to the PC when PPC Z flag is set and negate is FALSE or
 * PPC Z flag is not set and negate is TRUE.
 * The specified address is in the 68k emulated address space which is translated
 * to the physical memory address and load into the emulated PC register.
 * Skip address is loaded if the condition was false.
 * This macroblock needs two preallocated temporary registers.
 * Parameters:
 *    target_address - target address for emulated PC if evaluation came up as TRUE
 *    skip_address - skip address for emulated PC if evaluation came up as FALSE
 *    negate - if TRUE then the condition evaluation is negated (will be TRUE if Z flag is FALSE)
 *    address_reg - mapped temporary register for the operation
 *    tmp_reg - mapped temporary register for the operation
 */
void comp_macroblock_push_load_pc_from_immediate_conditional(uae_u32 target_address, uae_u32 skip_address, BOOL negate, uae_u8 address_reg, uae_u8 tmp_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_pc_from_immediate_conditional,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM);
	mb->set_pc_on_z_flag.negate = negate;
	mb->set_pc_on_z_flag.target_address = target_address;
	mb->set_pc_on_z_flag.skip_address = skip_address;
	mb->set_pc_on_z_flag.address_reg = address_reg;
	mb->set_pc_on_z_flag.tmp_reg = tmp_reg;
}

void comp_macroblock_impl_load_pc_from_immediate_conditional(union comp_compiler_mb_union* mb)
{
	uae_u8 address_reg = mb->set_pc_on_z_flag.address_reg;
	uae_u8 tmp_reg = mb->set_pc_on_z_flag.tmp_reg;

	//Condition is evaluated into the CRF0 Z flag by the source address mode handler or
	//any other previous evaluation process.
	//Branch accordingly to the previously calculated flag.
	//If Z flag was set then the condition was evaluated to FALSE.
	//Check negate option and invert the branch instruction if it was set.
	comp_ppc_bc(
			(mb->set_pc_on_z_flag.negate ?
					PPC_B_CR_TMP0_NE : PPC_B_CR_TMP0_EQ) | PPC_B_TAKEN, 0);

	//Load target address immediate into the address register
	comp_ppc_liw(address_reg, mb->set_pc_on_z_flag.target_address);

	//Skip to the storing
	comp_ppc_b(0, 1);

	//Branch target #0 reached, set it
	comp_ppc_branch_target(0);

	//Load skip address immediate into the address register
	comp_ppc_liw(address_reg, mb->set_pc_on_z_flag.skip_address);

	//Branch target #1 is reached, set it
	comp_ppc_branch_target(1);

	//Save it to emulated instruction pointer (PC register)
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc), PPCR_REGS_BASE);

	//Get memory address into the address register
	helper_map_physical_mem(address_reg, address_reg, tmp_reg);

	//Save physical memory pointer for the executed instruction
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc_p), PPCR_REGS_BASE);

	//Synchronize the executed instruction pointer from the block start to the actual
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc_oldp), PPCR_REGS_BASE);
}

/**
 * Macroblock: load the specified 68k address into the PC register
 * or do nothing according to the Z flag state and when decremented register reaches -1.
 * Skip address is loaded to the PC when PPC Z flag is not set and negate is FALSE or
 * PPC Z flag is set and negate is TRUE.
 * If the previous condition was FALSE then the specified decrement register will be
 * decremented, Z flag set to TRUE when it reaches -1. Condition is
 * evaluated again, and skip address loaded if Z flag is FALSE or target address otherwise.
 * The specified address is in the 68k emulated address space which is translated
 * to the physical memory address and load into the emulated PC register.
 * Skip address is loaded if the condition was false.
 * This macroblock needs two preallocated temporary registers.
 *
 * Note: if this macroblock seems to be confusing then have a look on
 * the behavior of M68k DBcc.W instruction.
 *
 * Parameters:
 *    regsin - input and output register dependency (register to decrement)
 *    decrement_reg - mapped register to decrement
 *    target_address - target address for emulated PC if evaluation came up as TRUE
 *    skip_address - skip address for emulated PC if evaluation came up as FALSE
 *    negate - if TRUE then the condition evaluation is negated (will be TRUE if Z flag is FALSE)
 *    address_reg - mapped temporary register for the operation
 *    tmp_reg - mapped temporary register for the operation
 */
void comp_macroblock_push_load_pc_from_immediate_conditional_decrement_register(uae_u64 regsin, uae_u8 decrement_reg, uae_u32 target_address, uae_u32 skip_address, BOOL negate, uae_u8 address_reg, uae_u8 tmp_reg)
{
	comp_mb_init(mb,
				comp_macroblock_impl_load_pc_from_immediate_conditional_decrement_register,
				regsin | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				regsin | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM);
	mb->set_pc_on_z_flag.negate = negate;
	mb->set_pc_on_z_flag.target_address = target_address;
	mb->set_pc_on_z_flag.skip_address = skip_address;
	mb->set_pc_on_z_flag.address_reg = address_reg;
	mb->set_pc_on_z_flag.tmp_reg = tmp_reg;
	mb->set_pc_on_z_flag.decrement_reg = decrement_reg;
}

void comp_macroblock_impl_load_pc_from_immediate_conditional_decrement_register(union comp_compiler_mb_union* mb)
{
	uae_u8 address_reg = mb->set_pc_on_z_flag.address_reg;
	uae_u8 tmp_reg = mb->set_pc_on_z_flag.tmp_reg;
	uae_u8 decrement_reg = mb->set_pc_on_z_flag.decrement_reg;

	//Condition is evaluated into the CRF0 Z flag by the source address mode handler or
	//any other previous evaluation process.
	//Branch accordingly to the previously calculated flag.
	//If Z flag was set then the condition was evaluated to FALSE.
	//Check negate option and invert the branch instruction if it was set.
	comp_ppc_bc(
			(mb->set_pc_on_z_flag.negate ?
					PPC_B_CR_TMP0_EQ : PPC_B_CR_TMP0_NE) | PPC_B_NONTAKEN, 0);

	//Condition evaluated to FALSE, decrement register
	//Word sized operation: mask out lowest word from target register
	//This will also set the Z flag on PPC
	comp_ppc_andi(tmp_reg, decrement_reg, 0xffff);

	//Decrease target register
	comp_ppc_addi(tmp_reg, tmp_reg, -1);

	//Insert result back to source register
	comp_ppc_rlwimi(decrement_reg, tmp_reg, 0, 16, 31, FALSE);

	//Jump if Z flag is set (decremented register was 0 before the operation)
	comp_ppc_bc(PPC_B_CR_TMP0_EQ | PPC_B_NONTAKEN, 1);

	//Load target address immediate into the address register
	comp_ppc_liw(address_reg, mb->set_pc_on_z_flag.target_address);

	//Skip to the storing
	comp_ppc_b(0, 2);

	//Branch target #1 reached, set it
	comp_ppc_branch_target(1);

	//Branch target #0 reached, set it
	comp_ppc_branch_target(0);

	//Load skip address immediate into the address register
	comp_ppc_liw(address_reg, mb->set_pc_on_z_flag.skip_address);

	//Branch target #2 reached, set it
	comp_ppc_branch_target(2);

	//Save it to emulated instruction pointer (PC register)
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc), PPCR_REGS_BASE);

	//Get memory address into the address register
	helper_map_physical_mem(address_reg, address_reg, tmp_reg);

	//Save physical memory pointer for the executed instruction
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc_p), PPCR_REGS_BASE);

	//Synchronize the executed instruction pointer from the block start to the actual
	comp_ppc_stw(address_reg, COMP_GET_OFFSET_IN_REGS(pc_oldp), PPCR_REGS_BASE);
}

void comp_macroblock_push_set_byte_from_z_flag(uae_u64 regsout, uae_u8 output_reg, int negate)
{
	comp_mb_init(mb,
				comp_macroblock_impl_set_byte_from_z_flag,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				regsout);
	mb->set_byte_from_z_flag.negate = negate;
	mb->set_byte_from_z_flag.output_reg = output_reg;
}

void comp_macroblock_impl_set_byte_from_z_flag(union comp_compiler_mb_union* mb)
{
	int output_reg = mb->set_byte_from_z_flag.output_reg;

	//Condition is evaluated into the CRF0 Z flag by the source address mode handler.

	//Set byte to 0 (false) by default
	comp_ppc_rlwinm(output_reg, output_reg, 0, 0, 23, FALSE);

	//Branch accordingly to the previously calculated flag.
	//If Z flag was set then the condition was evaluated to FALSE.
	//Check negate option and invert the branch instruction if it was set.
	comp_ppc_bc(
			(mb->set_byte_from_z_flag.negate ?
					PPC_B_CR_TMP0_NE : PPC_B_CR_TMP0_EQ) | PPC_B_TAKEN, 0);

	//Set byte to 0xff (true)
	comp_ppc_ori(output_reg, output_reg, 0xff);

	//Branch target reached, set it
	comp_ppc_branch_target(0);
}
