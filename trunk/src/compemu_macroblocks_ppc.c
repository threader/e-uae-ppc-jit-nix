/**
 * Instruction building macroblock functions for JIT compiling
 *
 * Macroblocks are used by the JIT compiling for create the intermediate
 * representation of the compiled code.
 * The macroblock instructions are collected into a buffer when the 68k code
 * is compiled.
 * After high level optimization these are compiled into native PPC code by
 * the functions in this source code.
 */

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

/**
 * Local function protos
 */
STATIC_INLINE uae_u8 helper_allocate_tmp_reg(void);
STATIC_INLINE uae_u8 helper_allocate_tmp_reg_with_init(uae_u32 immed);
STATIC_INLINE void helper_free_tmp_reg(uae_u8 reg);
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset, int invertc);
STATIC_INLINE void helper_check_nz_flags(void);
STATIC_INLINE void helper_check_z_flag(void);
STATIC_INLINE void helper_check_nz_clear_cv_flags(void);
STATIC_INLINE void helper_check_nzcvx_flags(int invertc);
STATIC_INLINE void helper_check_nzcv_flags(int invertc);
STATIC_INLINE void helper_extract_cx_clear_v_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit);
STATIC_INLINE void helper_extract_c_clear_v_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit);
STATIC_INLINE void helper_extract_cx_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit);
STATIC_INLINE void helper_extract_c_flag(uae_u64 regsin, uae_u8 input_reg, int input_bit);
STATIC_INLINE void helper_move_inst_static_flags(int immediate);
STATIC_INLINE void helper_free_src_mem_addr_temp_reg(void);
STATIC_INLINE void helper_free_src_temp_reg(void);
STATIC_INLINE void helper_allocate_ax_src_mem_reg(struct comptbl* props, int modified);
STATIC_INLINE void helper_add_imm_to_src_ax(struct comptbl* props, uae_u16 immediate);
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs(struct comptbl* props, int modified);
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs_copy(struct comptbl* props, int modified);
STATIC_INLINE void helper_free_dest_mem_addr_temp_reg(void);
STATIC_INLINE void helper_free_dest_temp_reg(void);
STATIC_INLINE void helper_allocate_ax_dest_mem_reg(struct comptbl* props, int modified);
STATIC_INLINE void helper_add_imm_to_dest_ax(struct comptbl* props, uae_u16 immediate);
STATIC_INLINE void helper_allocate_2_ax_dest_mem_regs(struct comptbl* props, int modified);
STATIC_INLINE void helper_allocate_2_ax_dest_mem_regs_copy(struct comptbl* props, int modified);
STATIC_INLINE void helper_addr_indmAk_dest(struct comptbl* props, uae_u8 size);
STATIC_INLINE void helper_addr_indApk_dest(struct comptbl* props, uae_u8 size);
STATIC_INLINE int helper_read_memory(const cpu_history* history, uae_u8 size, int srcsamedest);
STATIC_INLINE void helper_write_memory(const cpu_history* history, int input_reg, uae_u8 size);
STATIC_INLINE void helper_check_result_set_flags(int regsin, int input_reg, uae_u8 size);
STATIC_INLINE void helper_copy_result_set_flags(uae_u8 src_reg, int src_reg_mapped, uae_u8 size);
STATIC_INLINE void helper_MOVIMMREG2MEM(const cpu_history* history, uae_u8 size, int immediate, int checkflags);
STATIC_INLINE void helper_MOVMEM2REG(const cpu_history* history, struct comptbl* props, uae_u8 size, int dataregmode);
STATIC_INLINE void helper_MOVEM2MEMU(const cpu_history* history, struct comptbl* props, uae_u8 size);
STATIC_INLINE void helper_MOVMEM2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size);
STATIC_INLINE void helper_MOVIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ORREG2REG(uae_u8 size);
STATIC_INLINE void helper_ORIMM2REG(uae_u8 size);
STATIC_INLINE void helper_EORIMM2REG(uae_u8 size);
STATIC_INLINE void helper_EORREG2REG(uae_u8 size);
STATIC_INLINE void helper_EORREG2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size);
STATIC_INLINE void helper_ADDQ2REG(uae_u8 size);
STATIC_INLINE void helper_ADDIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ADDREG2REG(uae_u8 size);
STATIC_INLINE void helper_SUBREG2REG(uae_u8 size);
STATIC_INLINE void helper_ANDIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ANDREG2REG(uae_u8 size);
STATIC_INLINE void helper_TSTREG(uae_u8 size);
STATIC_INLINE void helper_CMPIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ASRIMM2REG(uae_u8 size);
STATIC_INLINE void helper_LSRIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ASLIMM2REG(uae_u8 size);
STATIC_INLINE void helper_ROLIMM2REG(uae_u8 size);
STATIC_INLINE void helper_copy_word_with_flagcheck(uae_u8 tmpreg, int tmpreg_mapped);
STATIC_INLINE void helper_copy_byte_with_flagcheck(uae_u8 tmpreg, int tmpreg_mapped);
STATIC_INLINE uae_u8 helper_pre_word(uae_u64 regsin, uae_u8 input_reg_mapped);
STATIC_INLINE void helper_post_word(uae_u64 regsout, uae_u8 tmpreg, uae_u8 output_reg_mapped);
STATIC_INLINE uae_u8 helper_pre_byte(uae_u64 regsin, uae_u8 input_reg_mapped);
STATIC_INLINE void helper_post_byte(uae_u64 regsout, uae_u8 tmpreg, uae_u8 output_reg_mapped);
STATIC_INLINE uae_u8 helper_prepare_word_shift(uae_u64 regsin, uae_u8 input_reg_mapped);
STATIC_INLINE uae_u8 helper_prepare_byte_shift(uae_u64 regsin, uae_u8 input_reg_mapped);
STATIC_INLINE void helper_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, uae_u8 output_mem_reg, uae_u8 base_reg, uae_u32 base_address);
STATIC_INLINE void helper_complete_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, uae_u8 output_mem_reg, uae_u8 base_reg, uae_u32 base_address, uae_u16 ext);
STATIC_INLINE uae_u8 helper_calculate_complex_index(uae_u16 ext);

/**
 * Local variables
 */

//Source register (allocated temporary register) for the opcode
//Initialized and released by the addressing mode
int src_reg;

//Source register (allocated temporary register mapped to PPC register) for the opcode
//Initialized and released by the addressing mode
uae_u8 src_reg_mapped;

//Flag for negate the value of the evaluated condition code before use
//This boolean variable is used by the condition code addressing modes
//and the depending instruction handlers. If it is set to TRUE then
//the condition will be negated before use.
int src_condition_negate;

//Destination register (allocated temporary register) for the opcode
//Initialized and released by the addressing mode
int dest_reg;

//Destination register (allocated temporary register mapped to PPC register) for the opcode
//Initialized and released by the addressing mode
uae_u8 dest_reg_mapped;

//Input register dependency mask
//Initialized and released by the addressing mode
uae_u64 input_dep;

//Output register dependency mask
//Initialized and released by the addressing mode
uae_u64 output_dep;

//Source immediate value
//Initialized by the addressing mode
signed int src_immediate;

//Source addressing mode register (allocated temporary register) that contains
//the precalculated memory address for the opcode memory operations
//Initialized and released by the addressing mode
int src_mem_addrreg;

//Same as src_mem_addrreg, but mapped to the physical PPC register
uae_u8 src_mem_addrreg_mapped;

//Destination addressing mode register (allocated temporary register) that contains
//the precalculated memory address for the opcode memory operations
//Initialized and released by the addressing mode
int dest_mem_addrreg;

//Same as dest_mem_addrreg, but mapped to the physical PPC register
uae_u8 dest_mem_addrreg_mapped;

//Pointer to the next word sized data in memory after the opcode
//Each addressing mode that needs additional data increments this pointer
//to the next memory address after the read data
uae_u16* pc_ptr;

void comp_opcode_init(const cpu_history* history)
{
	//The next word after the opcode
	pc_ptr = history->location + 1;

	//Reset variables
	input_dep = output_dep = COMP_COMPILER_MACROBLOCK_REG_NONE;
	src_mem_addrreg = dest_mem_addrreg = PPC_TMP_REG_NOTUSED;
	src_condition_negate = FALSE;
}

/**
 * Addressing mode compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->srcreg);
}
void comp_addr_pre_regA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, FALSE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);
}
void comp_addr_pre_indA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, FALSE);
}
void comp_addr_pre_indmAL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -4);
}
void comp_addr_pre_indmAW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -2);
}
void comp_addr_pre_indmAB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -1);
}
void comp_addr_pre_indApL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_src_ax(props, 4);
}
void comp_addr_pre_indApW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_src_ax(props, 2);
}
void comp_addr_pre_indApB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_src_ax(props, 1);
}
void comp_addr_pre_indmALk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indmALk as source\n");
	abort();
}
void comp_addr_pre_indmAWk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indmAWk as source\n");
	abort();
}
void comp_addr_pre_indmABk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indmABk as source\n");
	abort();
}
void comp_addr_pre_indApLk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indApLk as source\n");
	abort();
}
void comp_addr_pre_indApWk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indApWk as source\n");
	abort();
}
void comp_addr_pre_indApBk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - indApBk as source\n");
	abort();
}
void comp_addr_pre_immedL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the immediate value,
	//load it with sign extension
	src_immediate = *((signed int*)pc_ptr);
	pc_ptr += 2;
}
void comp_addr_pre_immedW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the immediate value,
	//load it with sign extension
	src_immediate = *((signed short*)pc_ptr);
	pc_ptr++;
}
void comp_addr_pre_immedB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next byte after the instruction is the immediate value,
	//load it with sign extension
	src_immediate = *(((signed char*)pc_ptr + 1));
	//Step a whole word, because the instruction pointer cannot point to odd address
	pc_ptr++;
}
void comp_addr_pre_immedQ_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Source register in props is the immediate value,
	//load it with sign extension
	src_immediate = (signed char)props->srcreg;
}
void comp_addr_pre_indAd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//If the offset is zero then fall back to (Ax)
	if (*pc_ptr != 0)
	{
		helper_allocate_2_ax_src_mem_regs(props, FALSE);

		//Add the offset from the next word after the opcode to the register
		comp_macroblock_push_add_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
				src_mem_addrreg_mapped,
				src_reg_mapped,
				*(pc_ptr++));
	} else {
		comp_addr_pre_indA_src(history, props);
		pc_ptr++;
	}
}
void comp_addr_pre_indPCd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the word offset,
	//load it with sign extension and add it to the current PC
	//(also taking account the previous steps for the PC)
	uae_u32 address = ((uae_u32)history->pc) + ((uae_u32)pc_ptr - (uae_u32)history->location) + *((signed short*)pc_ptr);
	pc_ptr++;

	src_mem_addrreg = helper_allocate_tmp_reg();
	src_mem_addrreg_mapped = comp_get_gpr_for_temp_register(src_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			src_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_absW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the address value,
	//load it without sign extension
	uae_u16 address = *pc_ptr;
	pc_ptr++;

	src_mem_addrreg = helper_allocate_tmp_reg();
	src_mem_addrreg_mapped = comp_get_gpr_for_temp_register(src_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			src_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_absL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the address value, load it
	uae_u32 address = *((uae_u32*)pc_ptr);
	pc_ptr += 2;

	src_mem_addrreg = helper_allocate_tmp_reg();
	src_mem_addrreg_mapped = comp_get_gpr_for_temp_register(src_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			src_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_indAcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in src_mem_addrreg_mapped
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			history,
			src_mem_addrreg_mapped,
			src_reg_mapped, 0);
}
void comp_addr_pre_indPCcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the current PC to the address variable
	uae_u32 address = ((uae_u32)history->pc) + ((uae_u32)pc_ptr - (uae_u32)history->location);

	//Map registers
	helper_allocate_2_ax_src_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in src_mem_addrreg_mapped
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			history,
			src_mem_addrreg_mapped,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			address);
}

/**
 * Addressing mode compiler functions for source addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_post_regD_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_regA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indApL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indApW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indApB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indmALk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indmAWk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indmABk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indApLk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indApWk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indApBk_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_immedL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedQ_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indAd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indPCd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_absW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_absL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indAcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}
void comp_addr_post_indPCcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_src_mem_addr_temp_reg();
}

/**
 * Addressing mode compiler functions for destination addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->destreg);
}
void comp_addr_pre_regA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}
void comp_addr_pre_indA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, FALSE);
}
void comp_addr_pre_indmAL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -4);
}
void comp_addr_pre_indmAW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -2);
}
void comp_addr_pre_indmAB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, TRUE);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -1);
}
void comp_addr_pre_indApL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 4);
}
void comp_addr_pre_indApW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 2);
}
void comp_addr_pre_indApB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 1);
}
void comp_addr_pre_indmALk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indmAk_dest(props, 4);
}
void comp_addr_pre_indmAWk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indmAk_dest(props, 2);
}
void comp_addr_pre_indmABk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indmAk_dest(props, 1);
}
void comp_addr_pre_indApLk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indApk_dest(props, 4);
}
void comp_addr_pre_indApWk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indApk_dest(props, 2);
}
void comp_addr_pre_indApBk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_addr_indApk_dest(props, 1);
}
void comp_addr_pre_immedL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - immedL as destination\n");
	abort();
}
void comp_addr_pre_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - immedW as destination\n");
	abort();
}
void comp_addr_pre_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - immedB as destination\n");
	abort();
}
void comp_addr_pre_immedQ_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - immedQ as destination\n");
	abort();
}
void comp_addr_pre_indAd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//If the offset is zero then fall back to (Ax)
	if (*pc_ptr != 0)
	{
		helper_allocate_2_ax_dest_mem_regs(props, FALSE);

		//Add the offset from the next word after the opcode to the register
		comp_macroblock_push_add_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				dest_mem_addrreg_mapped,
				dest_reg_mapped,
				*(pc_ptr++));
	} else {
		comp_addr_pre_indA_dest(history, props);
		pc_ptr++;
	}
}
void comp_addr_pre_indPCd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the word offset,
	//load it with sign extension and add it to the current PC
	//(also taking account the previous steps for the PC)
	uae_u32 address = ((uae_u32)history->pc) + ((uae_u32)pc_ptr - (uae_u32)history->location) + *((signed short*)pc_ptr);
	pc_ptr++;

	dest_mem_addrreg = helper_allocate_tmp_reg();
	dest_mem_addrreg_mapped = comp_get_gpr_for_temp_register(dest_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			dest_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_absW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the address value,
	//load it without sign extension
	uae_u16 address = *pc_ptr;
	pc_ptr++;

	dest_mem_addrreg = helper_allocate_tmp_reg();
	dest_mem_addrreg_mapped = comp_get_gpr_for_temp_register(dest_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			dest_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_absL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the address value, load it
	uae_u32 address = *((uae_u32*)pc_ptr);
	pc_ptr += 2;

	dest_mem_addrreg = helper_allocate_tmp_reg();
	dest_mem_addrreg_mapped = comp_get_gpr_for_temp_register(dest_mem_addrreg);

	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			dest_mem_addrreg_mapped,
			address);
}
void comp_addr_pre_indAcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in dest_mem_addrreg_mapped
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			history,
			dest_mem_addrreg_mapped,
			dest_reg_mapped, 0);
}
void comp_addr_pre_indPCcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the current PC to the address variable
	uae_u32 address = ((uae_u32)history->pc) + ((uae_u32)pc_ptr - (uae_u32)history->location);

	//Map registers
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in dest_mem_addrreg_mapped
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			history,
			dest_mem_addrreg_mapped,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			address);
}

/**
 * Addressing mode compiler functions for destination addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_post_regD_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_regA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indmAB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indApL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indApW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indApB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indmALk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register: this is not a mistake,
	//we rigged the source register with our temp register
	helper_free_src_temp_reg();
}
void comp_addr_post_indmAWk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register: this is not a mistake,
	//we rigged the source register with our temp register
	helper_free_src_temp_reg();
}
void comp_addr_post_indmABk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register: this is not a mistake,
	//we rigged the source register with our temp register
	helper_free_src_temp_reg();
}
void comp_addr_post_indApLk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indApWk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indApBk_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_immedL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_immedQ_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//We won't come here anyway, this addressing mode is not supported
}
void comp_addr_post_indAd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indPCd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_absW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_absL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indAcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}
void comp_addr_post_indPCcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();
}

/**
 * Condition check compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_cond_pre_CC_cc_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !C
	src_condition_negate = TRUE;
	comp_cond_pre_CC_cs_src(history, props);
}
void comp_cond_pre_CC_cs_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if C
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_C, PPC_FLAGBIT_C, TRUE);
}
void comp_cond_pre_CC_eq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if Z
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_Z, PPC_FLAGBIT_Z, TRUE);
}
void comp_cond_pre_CC_ge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !(N^V)
	src_condition_negate = TRUE;
	comp_cond_pre_CC_lt_src(history, props);
}
void comp_cond_pre_CC_gt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !((N^V)|Z)
	src_condition_negate = TRUE;
	comp_cond_pre_CC_le_src(history, props);
}
void comp_cond_pre_CC_hi_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !(C|Z)
	src_condition_negate = TRUE;
	comp_cond_pre_CC_ls_src(history, props);
}
void comp_cond_pre_CC_le_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 tmp_reg = helper_allocate_tmp_reg();
	uae_u8 tmp_reg_mapped = comp_get_gpr_for_temp_register(tmp_reg);

	//Z <= if (N^V)|Z
	//Mask out N and Z flag to temp register
	comp_macroblock_push_and_high_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmp_reg),
			tmp_reg_mapped,
			PPCR_FLAGS,
			(1 << (FLAGBIT_N - 16)) | (1 << (FLAGBIT_Z - 16)));

	//Rotate V flag to N flag position to R0
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			PPC_FLAGBIT_V - PPC_FLAGBIT_N, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Exclusive OR N and repositioned V flag, Z flag left unchanged (OR'ed to the result)
	comp_macroblock_push_xor_register_register(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmp_reg) | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_SPECTMP,
			tmp_reg_mapped, TRUE);

	comp_free_temp_register(tmp_reg);
}
void comp_cond_pre_CC_ls_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if C|Z
	//Mask out C flag to temp register
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_C, PPC_FLAGBIT_C, FALSE);

	//Insert Z flag to the temp register
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_Z, PPC_FLAGBIT_Z, TRUE);
}
void comp_cond_pre_CC_lt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 tmp_reg = helper_allocate_tmp_reg();
	uae_u8 tmp_reg_mapped = comp_get_gpr_for_temp_register(tmp_reg);

	//Z <= if N^V
	//Mask out N flag to temp register
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmp_reg),
			tmp_reg_mapped,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Rotate V flag to N flag position to R0
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			PPC_FLAGBIT_V - PPC_FLAGBIT_N, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Exclusive OR N and repositioned V flag
	comp_macroblock_push_xor_register_register(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmp_reg) | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_SPECTMP,
			tmp_reg_mapped, TRUE);

	comp_free_temp_register(tmp_reg);
}
void comp_cond_pre_CC_mi_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if N
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_N, PPC_FLAGBIT_N, TRUE);
}
void comp_cond_pre_CC_ne_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !Z
	src_condition_negate = TRUE;
	comp_cond_pre_CC_eq_src(history, props);
}
void comp_cond_pre_CC_pl_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !N
	src_condition_negate = TRUE;
	comp_cond_pre_CC_mi_src(history, props);
}
void comp_cond_pre_CC_vc_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if !V
	src_condition_negate = TRUE;
	comp_cond_pre_CC_vs_src(history, props);
}
void comp_cond_pre_CC_vs_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if V
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP,
			PPCR_FLAGS,
			0, PPC_FLAGBIT_V, PPC_FLAGBIT_V, TRUE);
}
void comp_cond_pre_FCC_f_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_eq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ogt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_oge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_olt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ole_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ogl_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_or_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_un_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ueq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ugt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_uge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ult_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ule_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ne_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_t_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_sf_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_seq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_lt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_le_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gl_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gle_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngle_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngl_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nle_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nlt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_sne_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_st_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}

/**
 * Instruction compiling functions
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_opcode_MOVREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_long_with_flags(
			input_dep,
			output_dep,
			dest_reg_mapped,
			src_reg_mapped);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_MOVREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_word(
			input_dep,
			output_dep,
			dest_reg_mapped,
			src_reg_mapped);

	comp_macroblock_push_check_word_register(output_dep, dest_reg_mapped);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_MOVREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_byte(
			input_dep,
			output_dep,
			dest_reg_mapped,
			src_reg_mapped);

	comp_macroblock_push_check_byte_register(output_dep, dest_reg_mapped);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_MOVAREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_long(
			input_dep,
			output_dep,
			dest_reg_mapped,
			src_reg_mapped);

	//No flag change
}
void comp_opcode_MOVAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_word(
			input_dep,
			output_dep,
			dest_reg_mapped,
			src_reg_mapped);

	//No flag change
}
void comp_opcode_MOVMEM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2MEM(history, props, 4);
}
void comp_opcode_MOVMEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2MEM(history, props, 2);
}
void comp_opcode_MOVMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2MEM(history, props, 1);
}
void comp_opcode_MOVREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 4, FALSE, TRUE);
}
void comp_opcode_MOVREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 2, FALSE, TRUE);
}
void comp_opcode_MOVREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 1, FALSE, TRUE);
}
void comp_opcode_MOVMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, 4, TRUE);
}
void comp_opcode_MOVMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, 2, TRUE);
}
void comp_opcode_MOVMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, 1, TRUE);
}
void comp_opcode_MOVAMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, 4, FALSE);
}
void comp_opcode_MOVAMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, 2, FALSE);
}
void comp_opcode_MOVIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(4);
}
void comp_opcode_MOVIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(2);
}
void comp_opcode_MOVIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(1);
}
void comp_opcode_MOVAIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the immediate value to the destination register
	comp_macroblock_push_load_register_long(
			output_dep,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_MOVAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the immediate value to the destination register
	//sign-extended to longword size
	comp_macroblock_push_load_register_word_extended(
			output_dep,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_MOVEQ(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_load_register_long(
			output_dep,
			dest_reg_mapped,
			src_immediate);

	//Set up flags
	helper_move_inst_static_flags(src_immediate);
}
void comp_opcode_MOVEM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMUL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2MEMU(history, props, 4);
}
void comp_opcode_MOVEM2MEMUW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2MEMU(history, props, 2);
}
void comp_opcode_MOVEM2REGUL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGUW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 4, TRUE, TRUE);
}
void comp_opcode_MOVIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 2, TRUE, TRUE);
}
void comp_opcode_MOVIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, 1, TRUE, TRUE);
}
void comp_opcode_MOV16REG2REGU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOV16REG2MEMU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOV16MEM2REGU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOV16REG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOV16MEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.L #imm,reg instruction
	src_immediate = 0;
	helper_MOVIMM2REG(4);
}
void comp_opcode_CLRREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.W #imm,reg instruction
	src_immediate = 0;
	helper_MOVIMM2REG(2);
}
void comp_opcode_CLRREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.B #imm,reg instruction
	src_immediate = 0;
	helper_MOVIMM2REG(1);
}
void comp_opcode_CLRMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.L #imm,mem instruction
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, 4, TRUE, TRUE);
}
void comp_opcode_CLRMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.W #imm,mem instruction
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, 2, TRUE, TRUE);
}
void comp_opcode_CLRMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.B #imm,mem instruction
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, 1, TRUE, TRUE);
}
void comp_opcode_LEAIMML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the immediate value to the destination register
	comp_macroblock_push_load_register_long(
			output_dep,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_LEAIMMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the immediate value to the destination register
	//sign-extended to longword size
	comp_macroblock_push_load_register_word_extended(
			output_dep,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_LEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//TODO: this useless move instruction could be removed if the registers could be swapped
	//Move temporary register to the destination address register
	comp_macroblock_push_copy_register_long(
			input_dep | (src_mem_addrreg == PPC_TMP_REG_NOTUSED ? 0 : COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg)),
			output_dep,
			dest_reg_mapped,
			src_mem_addrreg_mapped);
}
void comp_opcode_PEAIMML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Save immediate address to the stack:
	//decrease stack pointer (A7) and store the longword to the pointed address

	//Map A7 register as destination
	dest_mem_addrreg_mapped = dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(7);

	//Decrease A7 register by 4
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			dest_reg_mapped,
			dest_reg_mapped,
			-4);

	//Store immediate using the A7 register into memory, skip the flag checking
	helper_MOVIMMREG2MEM(history, 4, TRUE, FALSE);
}
void comp_opcode_PEAIMMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Immediate is sign-extended at loading (addresses are always longwords)
	//everything else is the same as the longword variant
	comp_opcode_PEAIMML(history, props);
}
void comp_opcode_PEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Save immediate address to the stack:
	//decrease stack pointer (A7) and store the longword to the pointed address

	//Map A7 register as destination
	dest_mem_addrreg_mapped = dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(7);

	//Decrease A7 register by 4
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			dest_reg_mapped,
			dest_reg_mapped,
			-4);

	//The source memory mapped register is the source register for the memory operation
	src_reg_mapped = src_mem_addrreg_mapped;

	//Store source address from the register using the A7 register into memory,
	//skip the flag checking
	helper_MOVIMMREG2MEM(history, 4, FALSE, FALSE);
}
void comp_opcode_STREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Set lowest byte to 0xff (Scc with the condition of TRUE)
	comp_macroblock_push_or_low_register_imm(
			output_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			0xff);
}
void comp_opcode_SFREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Set lowest byte to 0 (Scc with the condition of FALSE)
	comp_macroblock_push_rotate_and_mask_bits(
			output_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			0, 0, 23, FALSE);
}
void comp_opcode_SCCREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//This instruction was implemented in one macroblock for simplicity
	comp_macroblock_push_set_byte_from_z_flag(
			output_dep,
			dest_reg_mapped,
			src_condition_negate);
}
void comp_opcode_STMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load -1 as immediate and call normal MOVE.B #imm,mem instruction
	//flag checking is skipped
	src_immediate = -1;
	helper_MOVIMMREG2MEM(history, 1, TRUE, FALSE);
}
void comp_opcode_SFMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.B #imm,mem instruction
	//flag checking is skipped
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, 1, TRUE, FALSE);
}
void comp_opcode_SCCMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate a temp register for the calculated source byte
	src_reg = helper_allocate_tmp_reg();
	src_reg_mapped = comp_get_gpr_for_temp_register(src_reg);

	//Call the macroblock which produces the source byte into the temp register
	comp_macroblock_push_set_byte_from_z_flag(
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_reg),
			src_reg_mapped,
			src_condition_negate);

	//Call normal MOVE.B reg,mem instruction
	helper_MOVIMMREG2MEM(history, 1, FALSE, FALSE);

	comp_free_temp_register(src_reg);
}
void comp_opcode_MOVCCR2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVCCR2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2CCRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2CCRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2CCRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DBCOND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DBF(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCONDB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BRAB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSRB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCONDW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BRAW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCONDL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BRAL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSRL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_JMPIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_JMPIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_JSRIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_JSRIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RTS(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RTD(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RTM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORIMM2REG(4);
}
void comp_opcode_EORIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORIMM2REG(2);
}
void comp_opcode_EORIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORIMM2REG(1);
}
void comp_opcode_EORIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2REG(4);
}
void comp_opcode_EORREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2REG(2);
}
void comp_opcode_EORREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2REG(1);
}
void comp_opcode_EORREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2MEM(history, props, 4);
}
void comp_opcode_EORREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2MEM(history, props, 2);
}
void comp_opcode_EORREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_EORREG2MEM(history, props, 1);
}
void comp_opcode_ANDIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDIMM2REG(4);
}
void comp_opcode_ANDIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDIMM2REG(2);
}
void comp_opcode_ANDIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDIMM2REG(1);
}
void comp_opcode_ANDIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDREG2REG(4);
}
void comp_opcode_ANDREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDREG2REG(2);
}
void comp_opcode_ANDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ANDREG2REG(1);
}
void comp_opcode_ANDREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORIMM2REG(4);
}
void comp_opcode_ORIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORIMM2REG(2);
}
void comp_opcode_ORIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORIMM2REG(1);
}
void comp_opcode_ORIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORREG2REG(4);
}
void comp_opcode_ORREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORREG2REG(2);
}
void comp_opcode_ORREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ORREG2REG(1);
}
void comp_opcode_ORREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOTMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2CCRB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2CCRB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2CCRB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BTSTIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BTSTREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BTSTIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Limit immediate into 0-31 range
	//(don't change this, other instuctions are calling this implementation!)
	src_immediate &= 31;
	if (src_immediate < 16)
	{
		//Bit is in the lower half word
		comp_macroblock_push_and_low_register_imm(
				output_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP,
				dest_reg_mapped,
				1 << src_immediate);
	} else {
		//Bit is in the higher half word
		comp_macroblock_push_and_high_register_imm(
				output_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP,
				dest_reg_mapped,
				1 << (src_immediate - 16));
	}

	//Save Z flag
	helper_check_z_flag();
}
void comp_opcode_BTSTREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BTSTREG2IMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSETIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSETREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BSETIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Flag checking is the same as for BTST
	comp_opcode_BTSTIMM2REG(history, props);

	if (src_immediate < 16)
	{
		//Bit is in the lower word
		comp_macroblock_push_or_low_register_imm(
				output_dep,
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				1 << src_immediate);
	} else {
		//Bit is in the higher word
		comp_macroblock_push_or_high_register_imm(
				output_dep,
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				1 << (src_immediate - 16));
	}
}
void comp_opcode_BSETREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCLRIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCLRREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCLRIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Flag checking is the same as for BTST
	comp_opcode_BTSTIMM2REG(history, props);

	//Load inverted mask into temp register
	int tempreg = helper_allocate_tmp_reg_with_init(~(1 << src_immediate));

	//Mask it
	comp_macroblock_push_and_register_register(
			output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			comp_get_gpr_for_temp_register(tempreg),
			FALSE);

	comp_free_temp_register(tempreg);
}
void comp_opcode_BCLRREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCHGIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCHGREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCHGIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BCHGREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TAS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TAS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASLIMM2REG(4);
}
void comp_opcode_ASLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASLIMM2REG(2);
}
void comp_opcode_ASLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASLIMM2REG(1);
}
void comp_opcode_ASLREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASRIMM2REG(4);
}
void comp_opcode_ASRIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASRIMM2REG(2);
}
void comp_opcode_ASRIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ASRIMM2REG(1);
}
void comp_opcode_ASRREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSLMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_LSRIMM2REG(4);
}
void comp_opcode_LSRIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_LSRIMM2REG(2);
}
void comp_opcode_LSRIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_LSRIMM2REG(1);
}
void comp_opcode_LSRREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ROLIMM2REG(4);
}
void comp_opcode_ROLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ROLIMM2REG(2);
}
void comp_opcode_ROLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ROLIMM2REG(1);
}
void comp_opcode_ROLREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXLMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RORMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROXRMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPAMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_CMPIMM2REG(4);
}
void comp_opcode_CMPAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 immtempreg = helper_allocate_tmp_reg();
	uae_u8 immtempreg_mapped = comp_get_gpr_for_temp_register(immtempreg);

	//Load the immediate value to the temp register
	//sign-extended to longword size
	comp_macroblock_push_load_register_word_extended(
			COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
			immtempreg_mapped,
			src_immediate);

	//Do longword compare: compile SUBFCO PPC opcode
	comp_macroblock_push_sub_with_flags(
			output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
			immtempreg_mapped,
			immtempreg_mapped,
			dest_reg_mapped);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);

	helper_free_tmp_reg(immtempreg);
}
void comp_opcode_CMPIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_CMPIMM2REG(2);
}
void comp_opcode_CMPIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_CMPIMM2REG(1);
}
void comp_opcode_CMPIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDREG2REG(4);
}
void comp_opcode_ADDREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDREG2REG(2);
}
void comp_opcode_ADDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDREG2REG(1);
}
void comp_opcode_ADDIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDIMM2REG(4);
}
void comp_opcode_ADDIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDIMM2REG(2);
}
void comp_opcode_ADDIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDIMM2REG(1);
}
void comp_opcode_ADDQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDQ2REG(4);
}
void comp_opcode_ADDQ2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDQ2REG(2);
}
void comp_opcode_ADDQ2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ADDQ2REG(1);
}
void comp_opcode_ADDAQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	comp_macroblock_push_add_register_imm(
			output_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_ADDQ2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_add(
			output_dep | input_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			src_reg_mapped);
}
void comp_opcode_ADDAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();
	uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

	comp_macroblock_push_copy_register_word_extended(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tmpreg_mapped,
			src_reg_mapped);

	comp_macroblock_push_add(
			output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			tmpreg_mapped);

	comp_free_temp_register(tmpreg);
}
void comp_opcode_ADDAIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if ((src_immediate < 32678) && (src_immediate >= -32768))
	{
		//Immediate is within the word range: call word sized function
		comp_opcode_ADDAIMM2REGW(history, props);
	} else {
		uae_u8 tmpreg = helper_allocate_tmp_reg_with_init(src_immediate);
		uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		comp_macroblock_push_add(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				tmpreg_mapped);

		comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_ADDAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_add_register_imm(
			output_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			src_immediate);
}
void comp_opcode_ADDXREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDXREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDXREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_SUBREG2REG(4);
}
void comp_opcode_SUBREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_SUBREG2REG(2);
}
void comp_opcode_SUBREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_SUBREG2REG(1);
}
void comp_opcode_SUBIMM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Fall back to ADDA with a negative immediate
	src_immediate = 0 - src_immediate;
	comp_opcode_ADDAIMM2REGL(history, props);
}
void comp_opcode_SUBAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Fall back to ADDA with a negative immediate
	src_immediate = 0 - ((unsigned short)src_immediate);
	comp_opcode_ADDAIMM2REGW(history, props);
}
void comp_opcode_SUBAREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (src_reg_mapped == dest_reg_mapped)
	{
		//The source and the destination register is the same: clear register instead of subtracting
		//Clearing needs no preloading of the emulated register
		comp_macroblock_push_load_register_long(
				output_dep,
				dest_reg_mapped,
				0);
	} else {
		//Subtract the registers, this is so simple
		comp_macroblock_push_sub(
				input_dep | output_dep,
				output_dep,
				dest_reg_mapped,
				src_reg_mapped,
				dest_reg_mapped);
	}
}
void comp_opcode_SUBAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int tempreg = helper_allocate_tmp_reg();
	int tempreg_mapped = comp_get_gpr_for_temp_register(tempreg);

	//Extend the source register to long into a temp register
	comp_macroblock_push_copy_register_word_extended(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
			tempreg_mapped,
			src_reg_mapped);

	//Subtract the registers
	comp_macroblock_push_sub(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg) | output_dep,
			output_dep,
			dest_reg_mapped,
			tempreg_mapped,
			dest_reg_mapped);

	comp_free_temp_register(tempreg);
}
void comp_opcode_SUBXREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBXREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBXREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULSIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULSREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 srctemp = helper_allocate_tmp_reg();
	uae_u8 srctemp_mapped = comp_get_gpr_for_temp_register(srctemp);
	uae_u8 desttemp = helper_allocate_tmp_reg();
	uae_u8 desttemp_mapped = comp_get_gpr_for_temp_register(desttemp);

	//Keep the low half word from source register
	comp_macroblock_push_copy_register_word_extended(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(srctemp),
			srctemp_mapped,
			src_reg_mapped);

	//Keep the low half word from destination register
	comp_macroblock_push_copy_register_word_extended(
			output_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(desttemp),
			desttemp_mapped,
			dest_reg_mapped);

	//Multiply the registers and set the flags
	comp_macroblock_push_multiply_registers_with_flags(
			COMP_COMPILER_MACROBLOCK_REG_TMP(srctemp) | COMP_COMPILER_MACROBLOCK_REG_TMP(desttemp),
			output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
			dest_reg_mapped,
			srctemp_mapped,
			desttemp_mapped);

	comp_free_temp_register(srctemp);
	comp_free_temp_register(desttemp);

	//Save flags
	helper_check_nzcv_flags(FALSE);
}
void comp_opcode_MULSMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULUIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULUREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	uae_u8 srctemp = helper_allocate_tmp_reg();
	uae_u8 srctemp_mapped = comp_get_gpr_for_temp_register(srctemp);
	uae_u8 desttemp = helper_allocate_tmp_reg();
	uae_u8 desttemp_mapped = comp_get_gpr_for_temp_register(desttemp);

	//Keep the low half word from source register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(srctemp),
			srctemp_mapped,
			src_reg_mapped,
			0xffff);

	//Keep the low half word from destination register
	comp_macroblock_push_and_low_register_imm(
			output_dep,
			COMP_COMPILER_MACROBLOCK_REG_TMP(desttemp),
			desttemp_mapped,
			dest_reg_mapped,
			0xffff);

	//Multiply the registers and set the flags
	comp_macroblock_push_multiply_registers_with_flags(
			COMP_COMPILER_MACROBLOCK_REG_TMP(srctemp) | COMP_COMPILER_MACROBLOCK_REG_TMP(desttemp),
			output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
			dest_reg_mapped,
			srctemp_mapped,
			desttemp_mapped);

	comp_free_temp_register(srctemp);
	comp_free_temp_register(desttemp);

	//Save flags
	helper_check_nzcv_flags(FALSE);
}
void comp_opcode_MULUMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MULMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVSIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVSREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVSMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVUIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVUREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVUMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_DIVMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ABCDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ABCDMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SBCDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SBCDMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NBCDREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NBCDMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_PACKREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_PACKMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_UNPKREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_UNPKMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SWAP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_rotate_and_mask_bits(
			output_dep,
			output_dep,
			dest_reg_mapped,
			dest_reg_mapped,
			16, 0, 31, TRUE);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_EXGA(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate source register for modifying
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, TRUE);

	//Swap temporary register mapping to simulate the exchanging of the registers
	comp_swap_temp_register_mapping(
			comp_get_temp_for_gpr_register(src_reg_mapped),
			comp_get_temp_for_gpr_register(dest_reg_mapped));

	//Both source and destination registers are needed for the input and the output
	comp_macroblock_push_null_operation(
			input_dep | output_dep,
			input_dep | output_dep);
}
void comp_opcode_EXGD(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate source register for modifying
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, TRUE);

	//Swap temporary register mapping to simulate the exchanging of the registers
	comp_swap_temp_register_mapping(
			comp_get_temp_for_gpr_register(src_reg_mapped),
			comp_get_temp_for_gpr_register(dest_reg_mapped));

	//Both source and destination registers are needed for the input and the output
	comp_macroblock_push_null_operation(
			input_dep | output_dep,
			input_dep | output_dep);
}
void comp_opcode_EXTBW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EXTWL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EXTBL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_NOP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Do nothing, nothing at all
}
void comp_opcode_TSTREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_TSTREG(4);
}
void comp_opcode_TSTREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_TSTREG(2);
}
void comp_opcode_TSTREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_TSTREG(1);
}
void comp_opcode_TSTMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TSTMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TSTMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LINKW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LINKL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_UNLINK(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFCHG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFCHG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFCLR2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFCLR2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFEXTS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFEXTS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFEXTU2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFEXTU2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFFFO2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFFFO2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFINS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFINS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFSET2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFSET2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFTST2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BFTST2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FBCONDW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FBCONDL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FDBCOND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FGENREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FGENMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FGENMEMUM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FGENMEMUP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FGENIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FSAVEMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FSAVEMEMUM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FRESTOREMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FRESTOREMEMUP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FSCCREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FSCCMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FNOP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}

/**
 * Unsupported opcode handler: compile a direct call to the interpretive emulator
 * Parameters:
 *    opcode - unsupported opcode number
 */
void comp_opcode_unsupported(uae_u16 opcode)
{
	comp_macroblock_push_opcode_unsupported(opcode);
}

/**
 * Allocate a temporary register
 */
STATIC_INLINE uae_u8 helper_allocate_tmp_reg()
{
	//This function is simple for now, but this might change in the future
	return comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
}

/**
 * Allocate a temporary register and load an immediate data into it
 */
STATIC_INLINE uae_u8 helper_allocate_tmp_reg_with_init(uae_u32 immed)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			comp_get_gpr_for_temp_register(tmpreg),
			immed);

	return tmpreg;
}

/**
 * Free a previously allocated temporary register
 */
STATIC_INLINE void helper_free_tmp_reg(uae_u8 reg)
{
	//This function is simple for now, but this might change in the future
	//What ever needs to be done in the translated code is handled in this function
	comp_free_temp_register(reg);
}

/**
 * Update the 68k flags according to the actual state of the PPC flags
 * For the parameters the flags must be provided or'ed together,
 * see COMP_COMPILER_MACROBLOCK_REG_FLAG... constants.
 *
 * Parameters:
 *   flagscheck - flags to check from PPC flags
 *   flagsclear - flags to clear
 *   flagsset - flags to set
 *   invertc - if TRUE then the extracted C flag will be inverted (before it is copied to X)
 */
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset, int invertc)
{
	uae_u8 tmpreg;
	uae_u8 flagtmp;
	uae_u16 flagmask;

	//Get temporary register for the operations
	tmpreg = helper_allocate_tmp_reg();

	if (flagscheck)
	{
		//Evaluate the special cases of the flag checking
		switch (flagscheck)
		{
		case COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL:
			//All flags are updated:
			//Move XER to CR2, copy CR to the flags register then copy X flag from C
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS,
						PPCR_FLAGS,
						1 << 5);
			}
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_FLAGC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS,
					PPCR_FLAGS,
					16, 26, 26, FALSE);
			break;

		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//All flags but X are updated:
			//Move XER to CR2, copy CR to the temp register then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 0, 10, FALSE);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS,
						PPCR_FLAGS,
						1 << 5);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Flags: N, C, V and X
			//Move XER to CR2, copy CR to temp, copy X to C then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						comp_get_gpr_for_temp_register(tmpreg),
						comp_get_gpr_for_temp_register(tmpreg),
						1 << 5);
			}
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg),
					16, 26, 26, FALSE);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 9, 0, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Flags: N, C and V
			//Move XER to CR2, copy CR to temp then insert temp register relevant part to the flag register in two rounds: C and V first, then N
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 9, 10, FALSE);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 0, 0, FALSE);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS,
						PPCR_FLAGS,
						1 << 5);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Flags: N, Z and V
			//Move XER to CR2, copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 0, 9, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Flags: N, Z
			//Copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nz_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 0, 2, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGZ:
			//Flag: Z
			//Copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nz_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 2, 2, FALSE);
			break;
		default:
			write_log("JIT error: unknown flag set for flag checking: %d\n", (int)flagscheck);
			abort();
		}
	}

	if (flagsclear)
	{
		//We have flags to clear
		switch (flagsclear)
		{
		case COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL:
			//Clear all flags: N, Z, C, V and X
			comp_macroblock_push_load_register_long(COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
					PPCR_FLAGS,
					0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: N, Z, C and V: mask out these registers and keep X only
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 26, 26, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Clear flags: C and V: mask out these registers and keep N, Z and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 8, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Clear flags: C and X: mask out these registers and keep N, Z and V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 27, 9, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN):
			//This is a very special case, if we set Z after clearing these flags, then it can be
			//managed from one instruction. If we don't modify Z then we must clear the other flags
			//by using two separate instructions.
			if (flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGZ)
			{
				//Clear flags: N, Z, C and V: mask out these registers and keep X only
				//Z will be set later on
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS,
					PPCR_FLAGS,
					0, 26, 26, FALSE);
			} else {
				//Clear flags: C, V and N: mask out these registers and keep Z and X
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS,
					PPCR_FLAGS,
					0, 11, 8, FALSE);
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS,
					PPCR_FLAGS,
					0, 1, 31, FALSE);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: C, V and Z: mask out these registers and keep N and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 1, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: N and Z: mask out these registers and keep C, V and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 3, 31, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGC:
			//Clear flag: C
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 9, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGV:
			//Clear flag: V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 10, 8, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGN:
			//Clear flag: N
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 1, 31, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGZ:
			//Clear flag: Z
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 3, 1, FALSE);
			break;
		default:
			write_log("JIT error: unknown flag set for flag clearing: %d\n", (int)flagsclear);
			abort();
		}
	}

	if (flagsset)
	{
		//We have flags to set
		flagmask = 0;

		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGN) != 0) flagmask |= (1 << (PPCR_FLAG_N - 16));
		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGZ) != 0) flagmask |= (1 << (PPCR_FLAG_Z - 16));
		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGV) != 0) flagmask |= (1 << (PPCR_FLAG_V - 16));
		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGC) != 0) flagmask |= (1 << (PPCR_FLAG_C - 16));

		//Set the high half word (NZVC)
		comp_macroblock_push_or_high_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				flagsset & (~COMP_COMPILER_MACROBLOCK_REG_FLAGX),
				PPCR_FLAGS, PPCR_FLAGS, flagmask);

		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGX) != 0)
		{
			//Set X flag in the low half word, depends on all flags, except X
			comp_macroblock_push_or_low_register_imm(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS, PPCR_FLAGS, (1 << (PPCR_FLAG_X)));
		}
	}

	//We are done, release temporary register
	helper_free_tmp_reg(tmpreg);
}


/* Saving the flags for a generic instruction: check N and Z */
STATIC_INLINE void helper_check_nz_flags()
{
	//Save flags: N and Z
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);
}

/* Saving the flags for a generic instruction: check Z */
STATIC_INLINE void helper_check_z_flag()
{
	//Save flag: Z
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);
}

/* Saving the flags for a generic instruction: check N and Z, clear C and V */
STATIC_INLINE void helper_check_nz_clear_cv_flags()
{
	//Save flags: N and Z, clear: V and C
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);
}

/* Saving all flags for an arithmetic instruction */
STATIC_INLINE void helper_check_nzcvx_flags(int invertc)
{
	//Save all flags
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			invertc);
}

/* Extract C flag from the specified register using the specified  bit */
STATIC_INLINE void helper_extract_c_flag(uae_u64 regsin, uae_u8 input_reg, int input_bit)
{
	//C flag is at bit 21
	int shift = 21 - input_bit;

	//Right direction instead of left
	if (shift < 0) shift += 32;

	//Extract C flag
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS,
			input_reg,
			shift, 10, 10, FALSE);
}

/* Extract C and X flags from the specified register using the specified  bit */
STATIC_INLINE void helper_extract_cx_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit)
{
	helper_extract_c_flag(regsin, input_reg, input_bit);

	//Copy C flag to X
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS,
			PPCR_FLAGS,
			16, 26, 26, FALSE);
}

/* Extract C and X flag from the specified register using the specified
 * bit, clear V flag. */
STATIC_INLINE void helper_extract_cx_clear_v_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit)
{
	helper_extract_cx_flags(regsin, input_reg, input_bit);

	//Clear V flag
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);
}

/* Extract C flag from the specified register using the specified
 * bit, clear V flag. */
STATIC_INLINE void helper_extract_c_clear_v_flags(uae_u64 regsin, uae_u8 input_reg, int input_bit)
{
	helper_extract_c_flag(regsin, input_reg, input_bit);

	//Clear V flag
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);
}

/* Saving all flags for an arithmetic instruction except X */
STATIC_INLINE void helper_check_nzcv_flags(int invertc)
{
	//Save all flags, but X
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			invertc);
}

/* Setting up the flags for a move instruction according to an immediate value */
STATIC_INLINE void helper_move_inst_static_flags(signed int immediate)
{
	uae_u16 set, clear;
	set = clear = COMP_COMPILER_MACROBLOCK_REG_NONE;

	if (immediate == 0)
	{
		//Immediate is zero: clear N, set Z
		clear = COMP_COMPILER_MACROBLOCK_REG_FLAGN;
		set = COMP_COMPILER_MACROBLOCK_REG_FLAGZ;
	} else {
		//Immediate is not zero: clear Z
		clear = COMP_COMPILER_MACROBLOCK_REG_FLAGZ;
		if (immediate < 0)
		{
			//Immediate is negative: set N
			set = COMP_COMPILER_MACROBLOCK_REG_FLAGN;
		} else {
			//Immediate is positive: clear N
			clear |= COMP_COMPILER_MACROBLOCK_REG_FLAGN;
		}
	}

	//Set/clear flags: N and Z, clear: V and C
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			clear | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			set, FALSE);
}

/**
 * Some local helper functions to make life easier
 */

/**
 * Release the temp register for the source memory address register
 */
STATIC_INLINE void helper_free_src_mem_addr_temp_reg()
{
	//Release source memory temp address register, if it is still allocated
	if (src_mem_addrreg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(src_mem_addrreg);
		src_mem_addrreg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Release the temp register for the source register
 */
STATIC_INLINE void helper_free_src_temp_reg()
{
	//Release source memory temp register, if it is still allocated
	if (src_reg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(src_reg);
		src_reg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Allocate temp register for the specified address register (Ax) from
 * the properties of the instruction, also specifies the register as
 * the source address register for the memory operations.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_ax_src_mem_reg(struct comptbl* props, int modified)
{
	src_mem_addrreg_mapped = src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, modified);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);
}

/**
 * Allocate two temp registers for the specified address register (Ax) from
 * the properties of the instruction, map the source address register.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs(struct comptbl* props, int modified)
{
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, modified);
	src_mem_addrreg = helper_allocate_tmp_reg();
	src_mem_addrreg_mapped = comp_get_gpr_for_temp_register(src_mem_addrreg);
}

/**
 * Allocate two temp registers for the specified address register (Ax) from
 * the properties of the instruction, map the address register and copy
 * the value to the other temp register. Also specify the first temp register as
 * the source address register for the memory operations.
 * This way the temporary register can preserve the original value of
 * the address register for the memory operations.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs_copy(struct comptbl* props, int modified)
{
	helper_allocate_2_ax_src_mem_regs(props, modified);

	//TODO: this useless move instruction could be removed if the registers could be swapped
	//Move target address register's original value to a temp register
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			src_mem_addrreg_mapped,
			src_reg_mapped);
}

/**
 * Add the specified immediate value to the source address register coming from
 * the instruction properties.
 * Parameters:
 *    props - pointer to the instruction properties
 *    immediate - immediate value
 */
STATIC_INLINE void helper_add_imm_to_src_ax(struct comptbl* props, uae_u16 immediate)
{
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			src_reg_mapped,
			src_reg_mapped,
			immediate);
}

/**
 * Release the temp register for the destination memory address register
 */
STATIC_INLINE void helper_free_dest_mem_addr_temp_reg()
{
	//Release destination memory temp address register, if it is still allocated
	if (dest_mem_addrreg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(dest_mem_addrreg);
		dest_mem_addrreg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Release the temp register for the destination register
 */
STATIC_INLINE void helper_free_dest_temp_reg()
{
	//Release destination memory temp register, if it is still allocated
	if (dest_reg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(dest_reg);
		dest_reg = PPC_TMP_REG_NOTUSED;
	}
}

/**
 * Allocate temp register for the specified address register (Ax) from
 * the properties of the instruction, also specifies the register as
 * the destination address register for the memory operations.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_ax_dest_mem_reg(struct comptbl* props, int modified)
{
	dest_mem_addrreg_mapped = dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, modified);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}

/**
 * Allocate two temp registers for the specified address register (Ax) from
 * the properties of the instruction, map the destination address register.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_2_ax_dest_mem_regs(struct comptbl* props, int modified)
{
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, modified);
	dest_mem_addrreg = helper_allocate_tmp_reg();
	dest_mem_addrreg_mapped = comp_get_gpr_for_temp_register(dest_mem_addrreg);
}

/**
 * Allocate two temp registers for the specified address register (Ax) from
 * the properties of the instruction, map the address register and copy
 * the value to the other temp register. Also specify the first temp register as
 * the destination address register for the memory operations.
 * This way the temporary register can preserve the original value of
 * the address register for the memory operations.
 * Parameters:
 *    props - pointer to the instruction properties
 *    modified - if TRUE then allocate the register for saving back, because it is modified in the instruction
 */
STATIC_INLINE void helper_allocate_2_ax_dest_mem_regs_copy(struct comptbl* props, int modified)
{
	helper_allocate_2_ax_dest_mem_regs(props, modified);

	//TODO: this useless move instruction could be removed if the registers could be swapped
	//Move target address register's original value to a temp register
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			dest_mem_addrreg_mapped,
			dest_reg_mapped);
}

/**
 * Add the specified immediate value to the destination address register
 * coming from the instruction properties.
 * Parameters:
 *    props - pointer to the instruction properties
 *    immediate - immediate value
 */
STATIC_INLINE void helper_add_imm_to_dest_ax(struct comptbl* props, uae_u16 immediate)
{
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_reg_mapped,
			dest_reg_mapped,
			immediate);
}

/**
 * Implementation of all MOVIMM2MEM and MOVREG2MEM instruction
 * Parameters:
 *    history - pointer to cpu execution history
 *    size - size of the operation: byte (1), word (2) or long (4)
 *    immediate - use src_immediate instead of register if TRUE, src_reg otherwise
 *    checkflags - check the moved data and set flags accordingly if TRUE, skip flag checking otherwise
 */
STATIC_INLINE void helper_MOVIMMREG2MEM(const cpu_history* history, uae_u8 size, int immediate, int checkflags)
{
	int spec;
	int tmpregaddr;
	int tmpregaddr_mapped;

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_write_long(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_long_register(input_dep, src_reg_mapped);
		break;
	case 2:
		spec = comp_is_spec_memory_write_word(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_word_register(input_dep, src_reg_mapped);
		break;
	case 1:
		spec = comp_is_spec_memory_write_byte(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_byte_register(input_dep, src_reg_mapped);
		break;
	default:
		write_log("Error: wrong operation size for MOVIMMREG2MEM\n");
		abort();
	}

	//Is it immediate mode?
	if (immediate)
	{
		//Immediate: load the immediate into a temporary register as source
		src_reg  = helper_allocate_tmp_reg();
		src_reg_mapped = comp_get_gpr_for_temp_register(src_reg);
		input_dep |= COMP_COMPILER_MACROBLOCK_REG_TMP(src_reg);
		comp_macroblock_push_load_register_long(
				input_dep,
				src_reg_mapped,
				src_immediate);

		//Compile static flag settings
		if (checkflags) helper_move_inst_static_flags(src_immediate);
	} else {
		//Not immediate: check source register for flags
		if (checkflags) helper_check_nz_clear_cv_flags();
	}

	if (spec)
	{
		//Special memory access
		comp_macroblock_push_save_memory_spec(
				input_dep | (dest_mem_addrreg == PPC_TMP_REG_NOTUSED ? 0 : COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg)),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_reg_mapped,
				dest_mem_addrreg_mapped,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = dest_mem_addrreg = PPC_TMP_REG_NOTUSED;
		if (immediate)
		{
			src_reg = PPC_TMP_REG_NOTUSED;
		}
	}
	else
	{
		//Normal memory access
		tmpregaddr = helper_allocate_tmp_reg();
		tmpregaddr_mapped = comp_get_gpr_for_temp_register(tmpregaddr);

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				dest_mem_addrreg == PPC_TMP_REG_NOTUSED ? output_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
				tmpregaddr_mapped,
				dest_mem_addrreg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_save_memory_long(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 2:
			comp_macroblock_push_save_memory_word(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 1:
			comp_macroblock_push_save_memory_byte(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		}

		comp_free_temp_register(tmpregaddr);

		//If immediate mode was activated then the locally allocated source register must be released
		if (immediate)
		{
			comp_free_temp_register(src_reg);
		}
	}
}

/**
 * Implementation of all MOV(A)IMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_MOVIMM2REG(uae_u8 size)
{
	//Long operation size needs a bit different implementation
	if (size == 4)
	{
		comp_macroblock_push_load_register_long(
				output_dep,
				dest_reg_mapped,
				src_immediate);
	} else {
		//Word or byte sized: needs an additional bit insert

		//Is the immediate zero?
		if (src_immediate)
		{
			uae_u8 tmpreg = helper_allocate_tmp_reg();
			uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

			//Load the immediate value to the temporary register
			comp_macroblock_push_load_register_long(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					tmpreg_mapped,
					src_immediate);

			if (size == 2)
			{
				//Copy the lower word into the destination register
				comp_macroblock_push_copy_register_word(
						output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
						output_dep,
						dest_reg_mapped,
						tmpreg_mapped);
			} else {
				//We assume that this must be byte size, lazy-lazy...

				//Copy the lowest byte into the destination register
				comp_macroblock_push_copy_register_byte(
						output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
						output_dep,
						dest_reg_mapped,
						tmpreg_mapped);
			}

			//Done with the temp register
			comp_free_temp_register(tmpreg);
		} else {
			//Immediate is zero: no need for additional register, we can simply clear the bits
			comp_macroblock_push_rotate_and_mask_bits(
					output_dep,
					output_dep,
					dest_reg_mapped,
					dest_reg_mapped,
					0, 0, size == 2 ? 15 : 23 , FALSE);
		}
	}

	helper_move_inst_static_flags(src_immediate);
}

/**
 * Implementation of all MOVMEM2REG instruction
 * Parameters:
 *    props - pointer to the instruction properties
 *    history - pointer to cpu execution history
 *    size - size of the operation: byte (1), word (2) or long (4)
 *    dataregmode - if TRUE then the target is data reg, else address reg (flags are not checked)
 */
STATIC_INLINE void helper_MOVMEM2REG(const cpu_history* history, struct comptbl* props, uae_u8 size, int dataregmode)
{
	int spec;
	int tmpreg;
	int tmpreg_mapped;
	int tmpregaddr;
	int tmpregaddr_mapped;

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_read_long(history->pc, history->specmem);
		break;
	case 2:
		spec = comp_is_spec_memory_read_word(history->pc, history->specmem);
		break;
	case 1:
		spec = comp_is_spec_memory_read_byte(history->pc, history->specmem);
		break;
	default:
		write_log("Error: wrong operation size for MOVMEM2REG\n");
		abort();
	}

	if (spec)
	{
		//Special memory access, result returned in R0
		comp_macroblock_push_load_memory_spec(
				input_dep | (src_mem_addrreg == PPC_TMP_REG_NOTUSED ? 0 : COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg)),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_mem_addrreg_mapped,
				PPCR_SPECTMP,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = PPC_TMP_REG_NOTUSED;

		//Reallocate the target register
		dest_reg_mapped = comp_map_temp_register(
				dataregmode ?
						COMP_COMPILER_REGS_DATAREG(props->destreg):
						COMP_COMPILER_REGS_ADDRREG(props->destreg),
				size != 4,
				TRUE);

		//Copy data from R0 to the mapped target register
		switch (size)
		{
		case 4:
			comp_macroblock_push_copy_register_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					output_dep,
					dest_reg_mapped,
					PPCR_SPECTMP);
			break;
		case 2:
			if (dataregmode)
			{
				//Result is inserted into data register
				comp_macroblock_push_copy_register_word(
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						output_dep,
						dest_reg_mapped,
						PPCR_SPECTMP);
			} else {
				//Result is sign-extended into address register
				comp_macroblock_push_copy_register_word_extended(
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						output_dep,
						dest_reg_mapped,
						PPCR_SPECTMP);
			}
			break;
		case 1:
			comp_macroblock_push_copy_register_byte(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					output_dep,
					dest_reg_mapped,
					PPCR_SPECTMP);
			break;
		}
	}
	else
	{
		//Normal memory access

		tmpregaddr = helper_allocate_tmp_reg();
		tmpregaddr_mapped = comp_get_gpr_for_temp_register(tmpregaddr);

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				src_mem_addrreg == PPC_TMP_REG_NOTUSED ? input_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
				tmpregaddr_mapped,
				src_mem_addrreg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_load_memory_long(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep,
					dest_reg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 2:
			if (dataregmode)
			{
				//Result is inserted into data register
				tmpreg = helper_allocate_tmp_reg();
				tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

				comp_macroblock_push_load_memory_word(
						input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
						COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
						tmpreg_mapped,
						tmpregaddr_mapped,
						0);
				comp_macroblock_push_copy_register_word(
						output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
						output_dep,
						dest_reg_mapped,
						tmpreg_mapped);

				comp_free_temp_register(tmpreg);
			} else {
				//Result is loaded with sign-extension into address register
				comp_macroblock_push_load_memory_word_extended(
						input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
						output_dep,
						dest_reg_mapped,
						tmpregaddr_mapped,
						0);
			}
			break;
		case 1:
			tmpreg = helper_allocate_tmp_reg();
			tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);
			comp_macroblock_push_load_memory_byte(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			comp_macroblock_push_copy_register_byte(
					output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
			comp_free_temp_register(tmpreg);
			break;
		}

		comp_free_temp_register(tmpregaddr);
	}

	//Check result for flags by operation size
	if (dataregmode)
	{
		helper_check_result_set_flags(output_dep, dest_reg_mapped, size);

		//Save flags
		helper_check_nz_clear_cv_flags();
	}
}

/**
 * Implementation of all MOVMEM2MEM instruction
 * Parameters:
 *    props - pointer to the instruction properties
 *    history - pointer to cpu execution history
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_MOVMEM2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size)
{
	//Read memory from source
	int tmpreg = helper_read_memory(history, size, FALSE);

	//Check register
    helper_check_result_set_flags(COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg), comp_get_gpr_for_temp_register(tmpreg), size);

    //Save flags
	helper_check_nz_clear_cv_flags();

	//Write memory to destination and release temp register
	helper_write_memory(history, tmpreg, size);
}

/**
 * Implementation of all MOVEM2MEMU instruction
 * Parameters:
 *    history - pointer to cpu execution history
 *    size - size of the operation: word (2) or long (4)
 */
STATIC_INLINE void helper_MOVEM2MEMU(const cpu_history* history, struct comptbl* props, uae_u8 size)
{
	int spec, i, offset, selected_reg;
	uae_u8 tempaddr_reg;
	uae_u8 tempaddr_reg_mapped;

	//Read the extended word
	unsigned short extword  = *((unsigned short*)pc_ptr);

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_write_long(history->pc, history->specmem);
		break;
	case 2:
		spec = comp_is_spec_memory_write_word(history->pc, history->specmem);
		break;
	default:
		write_log("Error: wrong operation size for MOVEM2MEMU\n");
		abort();
	}

	if (spec)
	{
		//Save non-volatile register #0 temporarily
		comp_macroblock_push_save_reg_slot(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0, 0);

		//Copy destination address register to the non-volatile register
		comp_macroblock_push_copy_register_long(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				PPCR_TMP_NONVOL0,
				dest_reg_mapped);

		//Flush temp registers back to the store
		comp_flush_temp_registers(TRUE);

		//Check specified registers in the extension word
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Decrease address register by the size of the operation
				comp_macroblock_push_add_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_TMP_NONVOL0,
						PPCR_TMP_NONVOL0,
						-size);

				//Read register content from store to the second argument register (to avoid copying)
				//For this MOVEM instruction the registers are stored in reversed order in the extension word
				comp_macroblock_push_load_memory_long(
						COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_PARAM2,
						PPCR_REGS_BASE,
						(15 - i) * 4);

				//Store register content in memory using special memory access
				comp_macroblock_push_save_memory_spec(
						COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_PARAM2,
						PPCR_TMP_NONVOL0,
						size);
			}
		}

		//Keep modified address register content, but it must be reallocated
		dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), FALSE, TRUE);

		//Copy modified address to the allocated address register
		comp_macroblock_push_copy_register_long(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
				dest_reg_mapped,
				PPCR_TMP_NONVOL0);

		//Restore non-volatile register
		comp_macroblock_push_load_reg_slot(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0, 0);
	}
	else
	{
		//Normal memory access
		tempaddr_reg = helper_allocate_tmp_reg();
		tempaddr_reg_mapped = comp_get_gpr_for_temp_register(tempaddr_reg);

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(tempaddr_reg),
				tempaddr_reg_mapped,
				dest_reg_mapped);

		//Check specified registers in the extension word
		offset = 0;
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Step to the next address
				offset -= size;

				//Get the temp register if it was mapped already
				selected_reg = comp_get_mapped_temp_register(
						i < 8 ? COMP_COMPILER_REGS_ADDRREG(i) :
								COMP_COMPILER_REGS_DATAREG(i - 8));

				if (selected_reg == PPC_TMP_REG_NOTUSED)
				{
					//Not mapped yet, let's read it directly from the register store
					//Read register content from store to R0
					//For this MOVEM instruction the registers are stored in reversed order in the extension word
					comp_macroblock_push_load_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							PPCR_SPECTMP,
							PPCR_REGS_BASE,
							(15 - i) * 4);

					selected_reg = PPCR_SPECTMP;
				}

				//Save long to memory, prevent from optimizing away
				switch (size)
				{
				case 4:
					comp_macroblock_push_save_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg,
							tempaddr_reg_mapped,
							offset);
					break;
				case 2:
					comp_macroblock_push_save_memory_word(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg,
							tempaddr_reg_mapped,
							offset);
					break;
				}
			}
		}

		//Update register by the calculated offset
		comp_macroblock_push_add_register_imm(
				output_dep,
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				offset);

		comp_free_temp_register(tempaddr_reg);
	}
}

/**
 * Implementation of all ORREG2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ORREG2REG(uae_u8 size)
{
	if (size == 4)
	{
		comp_macroblock_push_or_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		uae_u8 tmpreg = helper_allocate_tmp_reg();
		int tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		comp_macroblock_push_or_register_register(
				input_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				tmpreg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				FALSE);

		helper_copy_result_set_flags(tmpreg, tmpreg_mapped, size);

		comp_free_temp_register(tmpreg);
	}
}

/**
 * Implementation of all ADDQ2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ADDQ2REG(uae_u8 size)
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//ADDQ is just a special case of ADDI
	helper_ADDIMM2REG(size);
}

/**
 * Implementation of all ADDIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ADDIMM2REG(uae_u8 size)
{
	uae_u8 immtempreg;
	uae_u8 desttempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg_mapped,
				dest_reg_mapped,
				comp_get_gpr_for_temp_register(immtempreg));
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			desttempreg = helper_pre_word(output_dep, dest_reg_mapped);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			desttempreg = helper_pre_byte(output_dep, dest_reg_mapped);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				comp_get_gpr_for_temp_register(desttempreg),
				comp_get_gpr_for_temp_register(desttempreg),
				comp_get_gpr_for_temp_register(immtempreg));

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg_mapped);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg_mapped);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);

	helper_free_tmp_reg(immtempreg);
}

/**
 * Implementation of all ADDREG2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ADDREG2REG(uae_u8 size)
{
	uae_u8 srctempreg;
	uae_u8 desttempreg;

	if (size == 4)
	{
		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg_mapped,
				dest_reg_mapped,
				src_reg_mapped);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg_mapped);
			desttempreg = helper_pre_word(output_dep, dest_reg_mapped);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg_mapped);
			desttempreg = helper_pre_byte(output_dep, dest_reg_mapped);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_REG_TMP(srctempreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				comp_get_gpr_for_temp_register(desttempreg),
				comp_get_gpr_for_temp_register(desttempreg),
				comp_get_gpr_for_temp_register(srctempreg));

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg_mapped);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg_mapped);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);
}

/**
 * Implementation of all SUBREG2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_SUBREG2REG(uae_u8 size)
{
	uae_u8 srctempreg;
	uae_u8 desttempreg;

	if (size == 4)
	{
		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg_mapped,
				src_reg_mapped,
				dest_reg_mapped);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg_mapped);
			desttempreg = helper_pre_word(output_dep, dest_reg_mapped);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg_mapped);
			desttempreg = helper_pre_byte(output_dep, dest_reg_mapped);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_REG_TMP(srctempreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				comp_get_gpr_for_temp_register(desttempreg),
				comp_get_gpr_for_temp_register(srctempreg),
				comp_get_gpr_for_temp_register(desttempreg));

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg_mapped);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg_mapped);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(TRUE);
}

/**
 * Implementation of all ANDIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ANDIMM2REG(uae_u8 size)
{
	uae_u8 desttempreg;
	uae_u8 desttempreg_mapped;
	uae_u8 immtempreg;
	uae_u8 immtempreg_mapped;

	if (size == 4)
	{
		immtempreg = helper_allocate_tmp_reg();
		immtempreg_mapped = comp_get_gpr_for_temp_register(immtempreg);

		//Load immediate into a temporary register
		comp_macroblock_push_load_register_long(
				COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				immtempreg_mapped,
				src_immediate);

		//Compile AND PPC opcode with flag update
		comp_macroblock_push_and_register_register(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				immtempreg_mapped,
				TRUE);

		helper_check_nz_clear_cv_flags();

		helper_free_tmp_reg(immtempreg);
	} else {
		desttempreg = helper_allocate_tmp_reg();
		desttempreg_mapped = comp_get_gpr_for_temp_register(desttempreg);

		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile ANDI PPC opcode
		comp_macroblock_push_and_low_register_imm(
				output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				desttempreg_mapped,
				dest_reg_mapped,
				src_immediate);

		//Check flags
		helper_check_nz_clear_cv_flags();

		//Check flags
		helper_copy_result_set_flags(desttempreg, desttempreg_mapped, size);

		comp_free_temp_register(desttempreg);
	}
}

/**
 * Implementation of all ANDREG2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ANDREG2REG(uae_u8 size)
{
	if (size == 4)
	{
		comp_macroblock_push_and_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		uae_u8 tmpreg = helper_allocate_tmp_reg();
		int tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		comp_macroblock_push_and_register_register(
				input_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				tmpreg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				FALSE);

		helper_copy_result_set_flags(tmpreg, tmpreg_mapped, size);

		comp_free_temp_register(tmpreg);
	}
}

/**
 * Implementation of all ORIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ORIMM2REG(uae_u8 size)
{
	uae_u8 immtempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile OR PPC opcode with flag update
		comp_macroblock_push_or_register_register(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				comp_get_gpr_for_temp_register(immtempreg),
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile OR immediate PPC opcode
		comp_macroblock_push_or_low_register_imm(
				output_dep,
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(output_dep, dest_reg_mapped, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}

/**
 * Implementation of all EORIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_EORIMM2REG(uae_u8 size)
{
	uae_u8 immtempreg;
	uae_u8 desttempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile XOR PPC opcode with flag update
		comp_macroblock_push_xor_register_register(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				comp_get_gpr_for_temp_register(immtempreg),
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile XOR immediate PPC opcode
		comp_macroblock_push_xor_low_register_imm(
				output_dep,
				output_dep,
				dest_reg_mapped,
				dest_reg_mapped,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(output_dep, dest_reg_mapped, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}

/**
 * Implementation of all EORREG2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_EORREG2REG(uae_u8 size)
{
	if (size == 4)
	{
		comp_macroblock_push_xor_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		uae_u8 tmpreg = helper_allocate_tmp_reg();
		int tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		comp_macroblock_push_xor_register_register(
				input_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				tmpreg_mapped,
				src_reg_mapped,
				dest_reg_mapped,
				FALSE);

		helper_copy_result_set_flags(tmpreg, tmpreg_mapped, size);

		comp_free_temp_register(tmpreg);
	}
}

/**
 * Implementation of all EORREG2MEM instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_EORREG2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size)
{
	//Read memory from source
	int tempreg = helper_read_memory(history, size, TRUE);
	int tempreg_mapped = comp_get_gpr_for_temp_register(tempreg);

	//Source register was free'd: reload
	comp_addr_pre_regD_src(history, props);

	if (size == 4)
	{
		//Compile XOR PPC opcode with flag update
		comp_macroblock_push_xor_register_register(
				input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg_mapped,
				tempreg_mapped,
				src_reg_mapped,
				TRUE);
	} else {
		//Prepare source register: keep lowest byte only
		if (size == 2)
		{
			comp_macroblock_push_and_low_register_imm(
					input_dep,
					input_dep,
					src_reg_mapped,
					src_reg_mapped,
					0xffff);
		} else {
			comp_macroblock_push_and_low_register_imm(
					input_dep,
					input_dep,
					src_reg_mapped,
					src_reg_mapped,
					0xff);
		}

		//Compile XOR PPC opcode
		comp_macroblock_push_xor_register_register(
				input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg),
				tempreg_mapped,
				tempreg_mapped,
				src_reg_mapped,
				FALSE);

		//Check register
	    helper_check_result_set_flags(COMP_COMPILER_MACROBLOCK_REG_TMP(tempreg), tempreg_mapped, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(history, tempreg, size);
}

/**
 * Implementation of all TSTREG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_TSTREG(uae_u8 size)
{
	switch (size)
	{
	case 4:
		comp_macroblock_push_copy_register_long_with_flags(
				input_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP,
				src_reg_mapped);
		break;
	case 2:
		comp_macroblock_push_check_word_register(
				input_dep,
				src_reg_mapped);
		break;
	case 1:
		comp_macroblock_push_check_byte_register(
				input_dep,
				src_reg_mapped);
		break;
	default:
		write_log("Error: wrong operation size for TSTREG\n");
		abort();
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}

/**
 * Implementation of all CMPIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_CMPIMM2REG(uae_u8 size)
{
	uae_u8 immtempreg;
	uae_u8 immtempreg_mapped;
	uae_u8 desttempreg;
	uae_u8 desttempreg_mapped;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);
		immtempreg_mapped = comp_get_gpr_for_temp_register(immtempreg);

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg_mapped,
				immtempreg_mapped,
				dest_reg_mapped);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			desttempreg = helper_pre_word(output_dep, dest_reg_mapped);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			desttempreg = helper_pre_byte(output_dep, dest_reg_mapped);
		}

		immtempreg_mapped = comp_get_gpr_for_temp_register(immtempreg);
		desttempreg_mapped = comp_get_gpr_for_temp_register(desttempreg);

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_TMP(desttempreg) | COMP_COMPILER_MACROBLOCK_REG_TMP(immtempreg),
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg_mapped,
				immtempreg_mapped,
				desttempreg_mapped);

		helper_free_tmp_reg(desttempreg);
	}

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);

	helper_free_tmp_reg(immtempreg);
}

/**
 * Implementation of all ASRIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ASRIMM2REG(uae_u8 size)
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	helper_extract_cx_clear_v_flags(output_dep, dest_reg_mapped, src_immediate - 1);

	if (size == 4)
	{
		//Arithmetic shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				src_immediate, TRUE);
	} else {
		//Get temporary register for the operations
		uae_u8 tmpreg = helper_allocate_tmp_reg();
		uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		//Sign-extend the register to longword
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word_extended(
					output_dep,
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					tmpreg_mapped,
					dest_reg_mapped);
		} else {
			comp_macroblock_push_copy_register_byte_extended(
					output_dep,
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					tmpreg_mapped,
					dest_reg_mapped);
		}

		//Arithmetic shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register(
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg_mapped,
				tmpreg_mapped,
				src_immediate, TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		} else {
			comp_macroblock_push_copy_register_byte(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		}

		comp_free_temp_register(tmpreg);
	}

	helper_check_nz_flags();
}

/**
 * Implementation of all LSRIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_LSRIMM2REG(uae_u8 size)
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	helper_extract_cx_clear_v_flags(output_dep, dest_reg_mapped, src_immediate - 1);

	if (size == 4)
	{
		//Simulate SRWI by using RLWINM
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				32 - src_immediate, src_immediate, 31, TRUE);
	} else {
		//Get temporary register for the operations
		uae_u8 tmpreg = helper_allocate_tmp_reg();
		uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		//Simulate SRWI by using RLWINM
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg_mapped,
				dest_reg_mapped,
				32 - src_immediate, (32 - size * 8) + src_immediate, 31, TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		} else {
			comp_macroblock_push_copy_register_byte(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		}

		comp_free_temp_register(tmpreg);
	}

	helper_check_nz_flags();
}

/**
 * Implementation of all ASLIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ASLIMM2REG(uae_u8 size)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();
	uae_u8 tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);
	uae_u8 src_reg;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C and X flag
	helper_extract_cx_flags(output_dep, dest_reg_mapped, (size * 8) - src_immediate);

	//Load the shifting into a temp register
	//TODO: this could be managed by using the immediate directly
	src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Extract V flag
	comp_macroblock_push_arithmetic_left_shift_extract_v_flag(
			output_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(src_reg),
			dest_reg_mapped,
			comp_get_gpr_for_temp_register(src_reg),
			tmpreg_mapped);

	comp_free_temp_register(src_reg);
	comp_free_temp_register(tmpreg);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				src_immediate,
				0, 31 - src_immediate, TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg_mapped);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg_mapped);
		}

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg_mapped,
				tmpreg_mapped,
				src_immediate,
				0,
				size == 2 ? 15 - src_immediate : 7 - src_immediate,
				TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg_mapped);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg_mapped);
		}
	}

	//Check flags
	helper_check_nz_flags();
}


/**
 * Implementation of all ROLIMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_ROLIMM2REG(uae_u8 size)
{
	uae_u8 tmpreg;
	uae_u8 tmpreg_mapped;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C, clear V flag
	helper_extract_c_clear_v_flags(output_dep, dest_reg_mapped, (size * 8) - src_immediate);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg_mapped,
				dest_reg_mapped,
				src_immediate,
				0, 31, TRUE);
	} else {
		//Prepare the source for arithmetic operation
		//TODO: this can be optimized by using two rotates: the second one can insert the rotated out bits into the result instead of using register move first.
		if (size == 2)
		{
			tmpreg = helper_prepare_word_shift(output_dep, dest_reg_mapped);
		} else {
			tmpreg = helper_prepare_byte_shift(output_dep, dest_reg_mapped);
		}
		tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		//Shifting to the left
		comp_macroblock_push_rotate_and_copy_bits(
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
				output_dep,
				dest_reg_mapped,
				tmpreg_mapped,
				src_immediate,
				size == 2 ? 16 : 24, 31, FALSE);

		//Copy result to the destination register
		if (size == 2)
		{
		    comp_macroblock_push_check_word_register(
		    		output_dep,
		    		dest_reg_mapped);
		} else {
		    comp_macroblock_push_check_byte_register(
		    		output_dep,
		    		dest_reg_mapped);
		}

		comp_free_temp_register(tmpreg);
	}

	//Check flags
	helper_check_nz_flags();
}



STATIC_INLINE void helper_addr_indmAk_dest(struct comptbl* props, uae_u8 size)
{
	//Allocate the address register
	helper_allocate_ax_dest_mem_reg(props, TRUE);

	//HACK: Switch off source register with a temp register,
	//that preserves the original value of the address register
	src_reg = helper_allocate_tmp_reg();
	src_reg_mapped = comp_get_gpr_for_temp_register(src_reg);

	//Move target address register's original value to a temp register
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_reg),
			src_reg_mapped,
			dest_reg_mapped);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -size);

	//Instruction is depending on both registers
	input_dep = COMP_COMPILER_MACROBLOCK_REG_TMP(src_reg) | COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}

STATIC_INLINE void helper_addr_indApk_dest(struct comptbl* props, uae_u8 size)
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, size);

	//HACK: Switch off source register with a temp register,
	//that preserves the original value of the address register
	src_reg_mapped = dest_mem_addrreg_mapped;

	//Instruction is depending on both registers
	input_dep = COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg) | COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}

/*
 * Copy a result word from the specified temp register to the destination register with normal flag checks
 */
STATIC_INLINE void helper_copy_word_with_flagcheck(uae_u8 tmpreg, int tmpreg_mapped)
{
    comp_macroblock_push_copy_register_word(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			output_dep,
			dest_reg_mapped,
			tmpreg_mapped);

    comp_macroblock_push_check_word_register(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tmpreg_mapped);

    	helper_check_nz_clear_cv_flags();
}

/*
 * Copy a result byte from the specified temp register to the destination register with normal flag checks
 */
STATIC_INLINE void helper_copy_byte_with_flagcheck(uae_u8 tmpreg, int tmpreg_mapped)
{
    comp_macroblock_push_copy_register_byte(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			output_dep,
			dest_reg_mapped,
			tmpreg_mapped);

    comp_macroblock_push_check_byte_register(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tmpreg_mapped);

    	helper_check_nz_clear_cv_flags();
}

/**
 * Before an arithmetic operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up the highest word.
 * Note: Must be in pair with helper_post_word() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE uae_u8 helper_pre_word(uae_u64 regsin, uae_u8 input_reg_mapped)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			comp_get_gpr_for_temp_register(tmpreg),
			input_reg_mapped,
			16, 0, 15, FALSE);

	return tmpreg;
}

/**
 * Before a rotate operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it twice: copy the low half word and shifted up the same to the highest word.
 * Note: the allocated temp register must be free'd after use.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE uae_u8 helper_prepare_word_shift(uae_u64 regsin, uae_u8 input_reg_mapped)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();
	uae_u8 tempreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

	comp_macroblock_push_copy_register_long(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tempreg_mapped,
			input_reg_mapped);

	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tempreg_mapped,
			tempreg_mapped,
			16, 0, 15, FALSE);

	return tmpreg;
}

/**
 * Before a rotate operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it twice: copy the lowest byte and shifted up the same to the highest byte.
 * Note: the allocated temp register must be free'd after use.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE uae_u8 helper_prepare_byte_shift(uae_u64 regsin, uae_u8 input_reg_mapped)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();
	uae_u8 tempreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

	comp_macroblock_push_copy_register_long(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tempreg_mapped,
			input_reg_mapped);

	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			tempreg_mapped,
			tempreg_mapped,
			24, 0, 7, FALSE);

	return tmpreg;
}

/**
 * After an arithmetic operation shift back the result from the highest word
 * to the normal position, insert into the destination register and free temp
 * register.
 * Note: Must be in pair with helper_pre_word() function.
 * Parameters:
 *    regsout - input register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE void helper_post_word(uae_u64 regsout, uae_u8 tmpreg, uae_u8 output_reg_mapped)
{
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			regsout,
			output_reg_mapped,
			comp_get_gpr_for_temp_register(tmpreg),
			16, 16, 31, FALSE);

	comp_free_temp_register(tmpreg);
}

/**
 * Before an arithmetic operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up the highest byte.
 * Note: Must be in pair with helper_post_byte() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE uae_u8 helper_pre_byte(uae_u64 regsin, uae_u8 input_reg_mapped)
{
	uae_u8 tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_rotate_and_mask_bits(
			regsin | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			comp_get_gpr_for_temp_register(tmpreg),
			input_reg_mapped,
			24, 0, 7, FALSE);

	return tmpreg;
}

/**
 * After an arithmetic operation shift back the result from the highest byte
 * to the normal position, insert into the destination register and free temp
 * register.
 * Note: Must be in pair with helper_pre_byte() function.
 * Parameters:
 *    regsout - output register dependency
 *    input_reg_mapped - input register for the copy
 */
STATIC_INLINE void helper_post_byte(uae_u64 regsout, uae_u8 tmpreg, uae_u8 output_reg_mapped)
{
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
			regsout,
			output_reg_mapped,
			comp_get_gpr_for_temp_register(tmpreg),
			8, 24, 31, FALSE);

	comp_free_temp_register(tmpreg);
}

/**
 * Complex (68020) addressing mode implementation, handles the processing
 * of the extension word for the non-scaled base-displaced 68000 instruction
 * up to the 68020 base-displaced, memory indirect addressing modes.
 * The final address is returned in the register specified in the output_mem_reg
 * parameter.
 * Note: this function also changes the current instruction PC
 *
 * Parameters:
 *    regsin - input register dependency
 *    regsout - output register dependency
 *    history - pointer to the execution history
 *    output_mem_reg - mapped output memory address register
 *    base_reg - mapped base register (address register) or COMP_COMPILER_MACROBLOCK_REG_NONE
 *    base_address - pre-calculated base address or 0 (for PC indirect addressing)
 *
 * Note: parameters base_reg and base_address are mutually exclusive.
 */
STATIC_INLINE void helper_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, uae_u8 output_mem_reg, uae_u8 base_reg, uae_u32 base_address)
{
	//Load address extension word
	uae_u16 ext = *pc_ptr;
	pc_ptr++;

	//Is it complete complex addressing?
	if ((ext & (1 << 8)) != 0)
	{
		// Complete complex addressing with displacement and indexing/indirect referencing
		helper_complete_complex_addressing(regsin, regsout, history, output_mem_reg, base_reg, base_address, ext);
	} else {
		// Simple base-displaced addressing
		uae_u8 scaled_index_reg = helper_calculate_complex_index(ext);
		uae_u8 scaled_index_reg_mapped = comp_get_gpr_for_temp_register(scaled_index_reg);

		//Get the displacement from the extension word
		uae_u32 displacement = ext & 255;

		//Sign extend it
		if (displacement & 128) displacement |= 0xffffff00;

		//Is base register not specified (meaning: there should be base address instead)
		if (base_reg == COMP_COMPILER_MACROBLOCK_REG_NONE)
		{
			//Summarize the constant data
			base_address += displacement;

			//Load it to the output register
			comp_macroblock_push_load_register_long(
					regsout,
					output_mem_reg,
					base_address);

			//Add the index * scale to the output register
			comp_macroblock_push_add(
					regsout | COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
					regsout,
					output_mem_reg,
					output_mem_reg,
					scaled_index_reg_mapped);
		} else {
			//No base address

			//Add the displacement to the output only if it is non-zero
			if (displacement)
			{
				comp_macroblock_push_add_register_imm(
						regsin,
						regsout,
						output_mem_reg,
						base_reg,
						displacement);

				//Add the index * scale to the output register
				comp_macroblock_push_add(
						regsout | COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
						regsout,
						output_mem_reg,
						output_mem_reg,
						scaled_index_reg_mapped);
			} else {
				//There is no displacement: add base and index * scale to the output register
				//Add the index * scale to the output register
				comp_macroblock_push_add(
						regsin | COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
						regsout,
						output_mem_reg,
						base_reg,
						scaled_index_reg_mapped);
			}
		}

		comp_free_temp_register(scaled_index_reg);
	}
}

/**
 * Complex (68020) addressing mode implementation, complete complex addressing
 * (full extension word) with base and outer displacement and indexing/indirect
 * referencing.
 * The final address is returned in the register specified in the output_mem_reg
 * parameter.
 * Note: this function also changes the current instruction PC
 *
 * Parameters:
 *    regsin - input register dependency
 *    regsout - output register dependency
 *    history - pointer to the execution history
 *    output_mem_reg - mapped output memory address register
 *    base_reg - mapped base register (address register) or COMP_COMPILER_MACROBLOCK_REG_NONE
 *    base_address - pre-calculated base address or 0 (for PC indirect addressing)
 *    ext - preloaded addressing extension word
 *
 * Note: parameters base_reg and base_address are mutually exclusive.
 */
STATIC_INLINE void helper_complete_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, uae_u8 output_mem_reg, uae_u8 base_reg, uae_u32 base_address, uae_u16 ext)
{
	uae_u32 base_displacement = 0;
	uae_u32 outer_displacement = 0;
	int memory_indirect = FALSE;
	int indexing_enabled = FALSE;
	int preindex = TRUE;

	//Filter out invalid forms
	if (((ext & (1 << 3)) != 0) || ((ext & (1 << 4 | 1 << 5)) == 0))
	{
		//Bit 3 must be 0, base displacement size bits (4 and 5) must not be zero
		write_log("JIT error: wrong complex addressing extension word: %04x\n", ext);
		abort();
	}

	//Is base displacement enabled?
	if ((ext & (1 << 5)) != 0)
	{
		//Get base displacement from the next word(s)
		if ((ext & (1 << 4)) == 0)
		{
			//BD is word sized (sign extended)
			base_displacement = (uae_s32)*(uae_s16*)pc_ptr;
			pc_ptr++;
		} else {
			//BD is longword sized
			base_displacement = *(uae_u32*)pc_ptr;
			pc_ptr += 2;
		}
	}

	//Is base register enabled? (Bit is set if suppressed)
	if ((ext & (1 << 7)) == 0)
	{
		//Is base register specified?
		if (base_reg == COMP_COMPILER_MACROBLOCK_REG_NONE)
		{
			//Not specified, base address is used: summarize BD and base address
			base_address += base_displacement;

			//Load it to the output register
			comp_macroblock_push_load_register_long(
					regsout,
					output_mem_reg,
					base_address);
		} else {
			//Base register is specified: add base displacement to it

			//Add lower half word of BD to the base register
			comp_macroblock_push_add_register_imm(
					regsin,
					regsout,
					output_mem_reg,
					base_reg,
					base_displacement & 0xffff);

			//Can we use word-sized immediate?
			//(Highest bit in the lower word is the same as all the bits in the higher word.)
			uae_u32 tmp = base_displacement & 0xffff8000;
			if (tmp != 0 && tmp != 0xffff8000)
			{
				//No, add the high half word too
				//If the lower half can be extended to negative then we have to compensate
				//it by adding 1 to the upper half
				comp_macroblock_push_add_high_register_imm(
						regsout,
						regsout,
						output_mem_reg,
						output_mem_reg,
						((base_displacement >> 16) + (base_displacement & 0x8000 ? 1 : 0)) & 0xffff);
			}
		}
	} else {
		//Base register is suppressed, we still need to initialize the output register
		//TODO: remove the unnecessary initialization of the output register
		comp_macroblock_push_load_register_long(
				regsout,
				output_mem_reg,
				base_displacement);
	}

	//Is the indexing enabled? (Bit is set if suppressed)
	indexing_enabled = ((ext & (1 << 6)) == 0);

	if (indexing_enabled)
	{
		//Indexing enabled
		switch (ext & 7)
		{
		case 0:	//000
			//No memory indirect action
			memory_indirect = FALSE;
			break;
		case 1:	//001
			//Indirect pre-indexed without outer displacement
			memory_indirect = TRUE;
			preindex = TRUE;
			break;
		case 2:	//010
			//Indirect pre-indexed with word sized outer displacement (sign extended)
			memory_indirect = TRUE;
			preindex = TRUE;
			outer_displacement = (uae_s32)*(uae_s16*)pc_ptr;
			pc_ptr++;
			break;
		case 3:	//011
			//Indirect pre-indexed with longword sized outer displacement
			memory_indirect = TRUE;
			preindex = TRUE;
			outer_displacement = *(uae_u32*)pc_ptr;
			pc_ptr += 2;
			break;
		case 5:	//101
			//Indirect post-indexed without outer displacement
			memory_indirect = TRUE;
			preindex = FALSE;
			break;
		case 6:	//110
			//Indirect post-indexed with word sized outer displacement (sign extended)
			memory_indirect = TRUE;
			preindex = FALSE;
			outer_displacement = (uae_s32)*(uae_s16*)pc_ptr;
			pc_ptr++;
			break;
		case 7:	//111
			//Indirect post-indexed with longword outer displacement
			memory_indirect = TRUE;
			preindex = FALSE;
			outer_displacement = *(uae_u32*)pc_ptr;
			pc_ptr += 2;
			break;
		default:
			//Unknown format
			write_log("JIT error: wrong complex addressing extension word: %04x\n", ext);
			abort();
		}

		//Is pre-indexing enabled?
		if (preindex)
		{
			//Yes, add the index register to the output
			uae_u8 scaled_index_reg = helper_calculate_complex_index(ext);
			uae_u8 scaled_index_reg_mapped = comp_get_gpr_for_temp_register(scaled_index_reg);

			//Add the index * scale to the output register
			comp_macroblock_push_add(
					regsout | COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
					regsout,
					output_mem_reg,
					output_mem_reg,
					scaled_index_reg_mapped);

			comp_free_temp_register(scaled_index_reg);
		}
	} else {
		//Indexing suppressed
		switch (ext & 7)
		{
		case 0:	//000
			//No memory indirect action
			memory_indirect = FALSE;
			break;
		case 1:	//001
			//Indirect without outer displacement
			memory_indirect = TRUE;
			break;
		case 2:	//010
			//Indirect with word sized outer displacement (sign extended)
			memory_indirect = TRUE;
			outer_displacement = (uae_s32)*(uae_s16*)pc_ptr;
			pc_ptr++;
			break;
		case 3:	//011
			//Indirect with longword sized outer displacement
			memory_indirect = TRUE;
			outer_displacement = *(uae_u32*)pc_ptr;
			pc_ptr += 2;
			break;
		default:
			//Unknown format
			write_log("JIT error: wrong complex addressing extension word: %04x\n", ext);
			abort();
		}
	}

	//Is indirect action enabled?
	if (memory_indirect)
	{
		//Read target address into the output register
		int spec = comp_is_spec_memory_read_long(history->pc, history->specmem);

		if (spec)
		{
			//TODO: purge the mapped registers - no need to save these, but source/destination mapped register must be reloaded
			//Special memory access, saves all allocated temporary registers
			comp_macroblock_push_load_memory_spec_save_temps(
					regsout,
					COMP_COMPILER_MACROBLOCK_REG_NONE | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					output_mem_reg,
					output_mem_reg,
					4);
		} else {
			//Get memory address into the target temp register
			comp_macroblock_push_map_physical_mem(
					regsout,
					regsout,
					output_mem_reg,
					output_mem_reg);
		}
	}

	//Is indexing and post-indexing enabled (not pre-indexing)?
	if (indexing_enabled && !preindex)
	{
		//Calculate scaled index register
		uae_u8 scaled_index_reg = helper_calculate_complex_index(ext);
		uae_u8 scaled_index_reg_mapped = comp_get_gpr_for_temp_register(scaled_index_reg);

		//Add the index * scale to the output register
		comp_macroblock_push_add(
				regsout | COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
				regsout,
				output_mem_reg,
				output_mem_reg,
				scaled_index_reg_mapped);

		comp_free_temp_register(scaled_index_reg);
	}

	//Is there any outer displacement?
	if (outer_displacement != 0)
	{
		//Add lower half word of OD to the output register
		comp_macroblock_push_add_register_imm(
				regsout,
				regsout,
				output_mem_reg,
				output_mem_reg,
				outer_displacement & 0xffff);

		//Can we use word-sized immediate?
		//(Highest bit in the lower word is the same as all the bits in the higher word.)
		uae_u32 tmp = outer_displacement & 0xffff8000;
		if (tmp != 0 && tmp != 0xffff8000)
		{
			//No, add the high half word too
			//If the lower half can be extended to negative then we have to compensate
			//it by adding 1 to the upper half
			comp_macroblock_push_add_high_register_imm(
					regsout,
					regsout,
					output_mem_reg,
					output_mem_reg,
					((outer_displacement >> 16) + (outer_displacement & 0x8000 ? 1 : 0)) & 0xffff);
		}
	}
}

/**
 * Calculates the scaled index from a complex addressing extension word.
 *
 * Parameters:
 *    ext - extension word for the complex addressing
 *
 * Returns:
 * Allocated temporary register populated with the calculated index.
 * The returned temporary register must be released by the caller.
 */
STATIC_INLINE uae_u8 helper_calculate_complex_index(uae_u16 ext)
{
	uae_u64 index_reg_dep;

	//Get the number of the register from the extension word
	uae_u8 index_reg = (ext >> 12) & 7;

	//Get the real register number: if bit 15 == 1 then address reg else data reg
	if (ext & (1 << 15))
	{
		index_reg_dep = COMP_COMPILER_MACROBLOCK_REG_AX(index_reg);
		index_reg = COMP_COMPILER_REGS_ADDRREG(index_reg);
	} else {
		index_reg_dep = COMP_COMPILER_MACROBLOCK_REG_DX(index_reg);
		index_reg = COMP_COMPILER_REGS_DATAREG(index_reg);
	}

	//Map register
	uae_u8 index_reg_mapped = comp_map_temp_register(index_reg, TRUE, FALSE);

	//Allocate temporary register for manipulating the content of the index register
	uae_u8 scaled_index_reg = helper_allocate_tmp_reg();
	uae_u8 scaled_index_reg_mapped = comp_get_gpr_for_temp_register(scaled_index_reg);

	//Is the index register word sized?
	if (ext & (1 << 11))
	{
		//TODO: this copy is not necessary, but we must preserve the content of the original register, because it can be written back later on
		//Long sized: simply copy to the temp register
		comp_macroblock_push_copy_register_long(
				index_reg_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
				scaled_index_reg_mapped,
				index_reg_mapped);
	} else {
		//Word sized: extend to long sized to the temp register
		comp_macroblock_push_copy_register_word_extended(
				index_reg_dep,
				COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
				scaled_index_reg_mapped,
				index_reg_mapped);
	}

	//Get scaling
	int scaling = (ext >> 9) & 3;

	//Scale it if necessary
	if (scaling != 0)
	{
		//Scale copied index register by 2, 4 or 8
		comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(scaled_index_reg),
				scaled_index_reg_mapped,
				scaled_index_reg_mapped,
				scaling,
				0, 31 - scaling, 0);
	}

	return scaled_index_reg;
}

/**
 * Allocate temporary register and read data into it from source memory address
 * Parameters:
 *    history - pointer to the execution history
 *    size - size of the operation (1,2 or 4)
 *    srcsamedest - if TRUE then source memory address is same as destination - destination will be used
 * Returns:
 *    number of allocated temporary register
 */
STATIC_INLINE int helper_read_memory(const cpu_history* history, uae_u8 size, int srcsamedest)
{
	int specread;
	int tmpreg;
	int tmpreg_mapped;

	if (srcsamedest)
	{
		src_mem_addrreg = dest_mem_addrreg;
		src_mem_addrreg_mapped = dest_mem_addrreg_mapped;
	}

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		specread = comp_is_spec_memory_read_long(history->pc, history->specmem);
		break;
	case 2:
		specread = comp_is_spec_memory_read_word(history->pc, history->specmem);
		break;
	case 1:
		specread = comp_is_spec_memory_read_byte(history->pc, history->specmem);
		break;
	default:
		write_log("Error: wrong operation size for memory read\n");
		abort();
	}

	if (specread)
	{
		//Store the destination register temporarily
		comp_macroblock_push_save_reg_slot(
				(dest_mem_addrreg == PPC_TMP_REG_NOTUSED ? output_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg)) | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				dest_mem_addrreg_mapped,
				0);

		//Special memory access, result returned in R4
		comp_macroblock_push_load_memory_spec(
				src_mem_addrreg == PPC_TMP_REG_NOTUSED ? input_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_mem_addrreg_mapped,
				PPCR_PARAM2,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = PPC_TMP_REG_NOTUSED;

		//Reallocate temporary register for the destination memory address
		//(due to the order of the temp registers this will be R3)
		dest_mem_addrreg = helper_allocate_tmp_reg();
		dest_mem_addrreg_mapped = comp_get_gpr_for_temp_register(dest_mem_addrreg);

		//Allocate temporary register for the result
		//(due to the order of the temp registers this will be R4)
		tmpreg = helper_allocate_tmp_reg();
		tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);

		//If the new temp register is not R4 then copy
		//the previous result into it (but we tweaked the
		//temporary register mapping already, so the allocated
		//register will be R4 always)
		if (tmpreg_mapped != PPCR_PARAM2)
		{
			comp_macroblock_push_copy_register_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					tmpreg_mapped,
					PPCR_PARAM2);
		}

		//Restore the destination address register
		comp_macroblock_push_load_reg_slot(
				COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				dest_mem_addrreg_mapped,
				0);
	}
	else
	{
		//Normal memory access
		tmpreg = helper_allocate_tmp_reg();
		tmpreg_mapped = comp_get_gpr_for_temp_register(tmpreg);
		int tmpregaddr = helper_allocate_tmp_reg();
		int tmpregaddr_mapped = comp_get_gpr_for_temp_register(tmpregaddr);

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				src_mem_addrreg == PPC_TMP_REG_NOTUSED ? input_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
				tmpregaddr_mapped,
				src_mem_addrreg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_load_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 2:
			comp_macroblock_push_load_memory_word(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 1:
			comp_macroblock_push_load_memory_byte(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		}

		comp_free_temp_register(tmpregaddr);
	}

	return tmpreg;
}

/**
 * Write data from register to the destination address in memory and free register.
 * Parameters:
 *    history - pointer to the execution history
 *    input_reg - source register to be written into memory
 *    size - size of the operation (1,2 or 4)
 */
STATIC_INLINE void helper_write_memory(const cpu_history* history, int input_reg, uae_u8 size)
{
	int specwrite;
	int tmpreg_mapped = comp_get_gpr_for_temp_register(input_reg);
	int tmpregaddr = PPC_TMP_REG_NOTUSED;
	int tmpregaddr_mapped = 0;

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		specwrite = comp_is_spec_memory_write_long(history->pc, history->specmem);
		break;
	case 2:
		specwrite = comp_is_spec_memory_write_word(history->pc, history->specmem);
		break;
	case 1:
		specwrite = comp_is_spec_memory_write_byte(history->pc, history->specmem);
		break;
	default:
		write_log("Error: wrong operation size for memory write\n");
		abort();
	}

	if (specwrite)
	{
		//Special memory access
		comp_macroblock_push_save_memory_spec(
				COMP_COMPILER_MACROBLOCK_REG_TMP(input_reg) | (dest_mem_addrreg  == PPC_TMP_REG_NOTUSED ? output_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg)),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				tmpreg_mapped,
				dest_mem_addrreg_mapped,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		dest_mem_addrreg = PPC_TMP_REG_NOTUSED;
		input_reg = PPC_TMP_REG_NOTUSED;
	}
	else
	{
		//Normal memory access

		//(Re)allocate address calculation temporary register
		tmpregaddr = helper_allocate_tmp_reg();
		tmpregaddr_mapped = comp_get_gpr_for_temp_register(tmpregaddr);

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				dest_mem_addrreg == PPC_TMP_REG_NOTUSED ? output_dep : COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
				tmpregaddr_mapped,
				dest_mem_addrreg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_save_memory_long(
					COMP_COMPILER_MACROBLOCK_REG_TMP(input_reg) | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 2:
			comp_macroblock_push_save_memory_word(
					COMP_COMPILER_MACROBLOCK_REG_TMP(input_reg) | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		case 1:
			comp_macroblock_push_save_memory_byte(
					COMP_COMPILER_MACROBLOCK_REG_TMP(input_reg) | COMP_COMPILER_MACROBLOCK_REG_TMP(tmpregaddr),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg_mapped,
					tmpregaddr_mapped,
					0);
			break;
		}
		comp_free_temp_register(tmpregaddr);
	}

	if (input_reg != PPC_TMP_REG_NOTUSED)
	{
		comp_free_temp_register(input_reg);
	}
}

/**
 * Check result and set flag register according to the arithmetic result,
 * taking account the operation size.
 * Result is copied into the PPC flag register.
 * Parameters:
 *    regsin - input register dependencies
 *    input_reg - (mapped) input register
 *    size - operation size (1,2 or 4 bytes)
 */
STATIC_INLINE void helper_check_result_set_flags(int regsin, int input_reg, uae_u8 size)
{
    //Check result for flags by operation size
    switch (size)
	{
	case 4:
		comp_macroblock_push_check_long_register(regsin, input_reg);
		break;
	case 2:
		comp_macroblock_push_check_word_register(regsin, input_reg);
		break;
	case 1:
		comp_macroblock_push_check_byte_register(regsin, input_reg);
		break;
	}
}

/**
 * Check result while copy to the destination register and set flag register
 * according to the arithmetic result, taking account the operation size.
 * Result is copied into the PPC flag register.
 * Parameters:
 *    src_reg - input register
 *    src_reg_mapped - (mapped) input register
 *    size - operation size (1 or 2 bytes)
 */
STATIC_INLINE void helper_copy_result_set_flags(uae_u8 src_reg, int src_reg_mapped, uae_u8 size)
{
    //Copy result and set flags by operation size
	switch (size)
	{
	case 2:
		helper_copy_word_with_flagcheck(src_reg, src_reg_mapped);
		break;
	case 1:
		helper_copy_byte_with_flagcheck(src_reg, src_reg_mapped);
		break;
	default:
		write_log("Error: wrong operation size for copy result and set flags function\n");
		abort();
	}
}
