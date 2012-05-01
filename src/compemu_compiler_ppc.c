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
#define comp_mb_init(n, h, ir, or) union comp_compiler_mb_union* n = comp_compiler_get_next_macroblock(); n->base.handler=(h); n->base.input_registers=(ir); n->base.output_registers=(or);

//Pointer to the end of the macroblock buffer
int macroblock_ptr;

/**
 * Prototypes of the internal macroblock implementation functions
 */
void comp_macroblock_impl_load_register_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_load_memory_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_map_physical_mem(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_spec(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_word(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_word_update(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_save_memory_byte(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_add_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_long_with_flags(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_register_long(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_opcode_unsupported(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_low_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_or_high_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_register_imm(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_and_registers(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_nzcv_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_nz_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_copy_cv_flags_to_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_check_byte_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_check_word_register(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_rotate_and_copy_bits(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_rotate_and_mask_bits(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_stop(union comp_compiler_mb_union* mb);
void comp_macroblock_impl_nop(union comp_compiler_mb_union* mb);

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
	//TODO: implement optimization of the macroblocks
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

	//Run thru the collected macroblocks and call the code generator handler for each
	union comp_compiler_mb_union* mb = macroblocks;
	for(i = 0; i < macroblock_ptr; i++ , mb++)
	{
		comp_compiler_macroblock_func* handler = mb->base.handler;

		//If there is a handler then call it
		if (handler) handler(mb);
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
 *    location - location of the instruction in memory
 *    opcode - unsupported opcode number
 */
void comp_macroblock_push_opcode_unsupported(uae_u16* location, uae_u16 opcode)
{
	//Before the call write back the M68k registers and clear the temp register mapping
	comp_flush_temp_registers(0);

	comp_mb_init(mb,
				comp_macroblock_impl_opcode_unsupported,
				COMP_COMPILER_MACROBLOCK_REG_ALL,
				COMP_COMPILER_MACROBLOCK_REG_ALL);
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
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			comp_get_gpr_for_temp_register(tempreg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(ccrflags.x));

	//Rotate X flag to the position and insert it into the flag register
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS,
			comp_get_gpr_for_temp_register(tempreg),
			16, 26, 26, 0);

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
void comp_macroblock_push_load_pc(uae_u16* location)
{
	uae_u8 temp_reg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg) | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			(uae_u32)location);

	comp_macroblock_push_save_memory_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(temp_reg),
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			comp_get_gpr_for_temp_register(temp_reg),
			PPCR_REGS_BASE,
			COMP_GET_OFFSET_IN_REGS(pc_p));

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
			1);
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
	comp_ppc_mr(mb->two_regs_opcode.output_reg, mb->two_regs_opcode.input_reg, 1);
}

/**
 * Macroblock: Copies a register into another register without flag update
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
	comp_ppc_mr(mb->two_regs_opcode.output_reg, mb->two_regs_opcode.input_reg, 0);
}

/**
 * Macroblock: Copies a word date from a register into another register and updates NZ flags in PPC flag registers
 */
void comp_macroblock_push_copy_register_word(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			regsout,
			output_reg,
			input_reg,
			0, 16, 31, 0);
}

/**
 * Macroblock: Copies a byte date from a register into another register and updates NZ flags in PPC flag registers
 */
void comp_macroblock_push_copy_register_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg)
{
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			regsout,
			output_reg,
			input_reg,
			0, 24, 31, 0);
}

/**
 * Macroblock: Loads an immediate value into a temporary register
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
void comp_macroblock_push_map_physical_mem(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u8 dest_mem_reg)
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
	uae_u8 tmpreg = mb->map_physical_mem.temp_reg;
	uae_u8 inreg = mb->map_physical_mem.input_reg;
	uae_u8 outreg = mb->map_physical_mem.output_reg;

	//TODO: preload base address array start into a GPR
	//Load base address array address to tmp reg
	comp_ppc_liw(tmpreg, (uae_u32)baseaddr);

	//Get 64k page index into R0: R0 = (inreg >> 14) & 0x3fffc;
	comp_ppc_rlwinm(PPCR_SPECTMP, inreg, 18, 14, 29, 0);

	//Load base address for the memory block to temp reg: rtmp = rtmp[r0]
	comp_ppc_lwzx(tmpreg, tmpreg, PPCR_SPECTMP);

	//TODO: base and offset could be sent separately and used in instruction code
	//Sum address and offset to outreg: outreg = inreg + rtmp
	comp_ppc_add(outreg, inreg, tmpreg, 0);
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
	//Warning is supressed, we are in the middle of some instruction compiling.
	//probably there are temporary registers, but we cannot do anything about it
	comp_flush_temp_registers(1);

	comp_mb_init(mb,
				comp_macroblock_impl_save_memory_spec,
				regsin, regsout);
	mb->access_memory_size.base_reg = dest_mem_reg;
	mb->access_memory_size.output_reg = source_reg;
	mb->access_memory_size.size = size;
}

void comp_macroblock_impl_save_memory_spec(union comp_compiler_mb_union* mb)
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
	//any conflicts with the target address register
	if (srcreg != PPCR_PARAM2)
	{
		//Source register is not R4, we need to move the value
		if (basereg != PPCR_PARAM2)
		{
			//We are lucky: the address register is not R4, safe to overwrite
			comp_ppc_mr(PPCR_PARAM2, srcreg, 0);
		} else {
			//Oops, the target register is the same as the address register,
			//move the amount thru R0 and move the address to R3
			comp_ppc_mr(PPCR_SPECTMP, srcreg, 0);		//R0 = srcreg
			comp_ppc_mr(PPCR_PARAM1, basereg, 0);		//R3 = basereg (R4)
			comp_ppc_mr(PPCR_PARAM2, PPCR_SPECTMP, 0);	//R4 = R0
			basereg = PPCR_PARAM1;
		}
	}

	//Move the address to R3, if it is not already in that register
	if (basereg != PPCR_PARAM1)
	{
		comp_ppc_mr(PPCR_PARAM1, basereg, 0);
	}

	//At this point the registers are:
	//R3 = address
	//R4 = value

	//Get 64k page index into R0: R0 = (inreg >> 14) & 0x3fffc;
	comp_ppc_rlwinm(PPCR_SPECTMP, PPCR_PARAM1, 18, 14, 29, 0);

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
		handleroffset = ((void*)&refaddrstruct.bput) - ((void*)&refaddrstruct);
		break;
	case 2:
		//Word
		handleroffset = ((void*)&refaddrstruct.wput) - ((void*)&refaddrstruct);
		break;
	case 4:
		//Long
		handleroffset = ((void*)&refaddrstruct.lput) - ((void*)&refaddrstruct);
		break;
	default:
		write_log("JIT error: unknown memory write size: %d\n", mb->access_memory_size.size);
		abort();
	}

	//Load the address of the handler to PPCR_TMP2
	comp_ppc_lwz(PPCR_TMP2, handleroffset, PPCR_TMP2);

	//Call handler
	comp_ppc_call_reg(PPCR_TMP2);
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
			mb->three_regs_opcode.input_reg2, 0);
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
	//Copy XER to CR2
	comp_ppc_mcrxr(PPCR_CR_TMP2);
	//Copy CR2 to the output register
	comp_ppc_mfcr(mb->one_reg_opcode.reg);
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
	comp_ppc_mfcr(mb->one_reg_opcode.reg);
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
	comp_ppc_extsh(PPCR_SPECTMP, mb->one_reg_opcode.reg, 1);
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
	comp_ppc_extsb(PPCR_SPECTMP, mb->one_reg_opcode.reg, 1);
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
 * Macroblock: Rotate and mask specified bits
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
