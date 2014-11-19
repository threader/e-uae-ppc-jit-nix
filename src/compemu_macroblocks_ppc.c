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
#include "comptbl.h"

/**
 * Local function protos
 */
STATIC_INLINE comp_tmp_reg* helper_allocate_tmp_reg(void);
STATIC_INLINE comp_tmp_reg* helper_allocate_preferred_tmp_reg(comp_ppc_reg preferred);
STATIC_INLINE comp_tmp_reg* helper_allocate_tmp_reg_with_init(uae_u32 immed);
STATIC_INLINE void helper_free_tmp_reg(comp_tmp_reg* reg);
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset, BOOL invertc);
STATIC_INLINE void helper_check_nz_flags(void);
STATIC_INLINE void helper_check_z_flag(void);
STATIC_INLINE void helper_check_nz_clear_cv_flags(void);
STATIC_INLINE void helper_check_nzcvx_flags(BOOL invertc);
STATIC_INLINE void helper_check_ncvx_flags_copy_z_flag_if_cleared(BOOL invertc);
STATIC_INLINE void helper_check_nzcv_flags(BOOL invertc);
STATIC_INLINE void helper_copy_x_flag_to_c_flag(void);
STATIC_INLINE void helper_copy_x_flag_to_internal_c_flag(BOOL invertX);
STATIC_INLINE void helper_extract_c_clear_nzvx_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit);
STATIC_INLINE void helper_extract_c_clear_nzv_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit);
STATIC_INLINE void helper_extract_cx_clear_nzv_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit);
STATIC_INLINE void helper_extract_c_clear_v_flags(uae_u64 regsin, comp_tmp_reg* input_reg, comp_tmp_reg* rotation_reg, BOOL left_shift);
STATIC_INLINE void helper_extract_cx_clear_v_flags(uae_u64 regsin, comp_tmp_reg* input_reg, comp_tmp_reg* rotation_reg, BOOL left_shift);
STATIC_INLINE void helper_move_inst_static_flags(int immediate);
STATIC_INLINE void helper_free_src_mem_addr_temp_reg(void);
STATIC_INLINE void helper_allocate_ax_src_mem_reg(struct comptbl* props, int modified);
STATIC_INLINE void helper_add_imm_to_src_ax(struct comptbl* props, uae_u16 immediate);
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs(struct comptbl* props, int modified);
STATIC_INLINE void helper_allocate_2_ax_src_mem_regs_copy(struct comptbl* props, int modified);
STATIC_INLINE void helper_free_dest_mem_addr_temp_reg(void);
STATIC_INLINE void helper_allocate_ax_dest_mem_reg(struct comptbl* props, int modified);
STATIC_INLINE void helper_add_imm_to_dest_ax(struct comptbl* props, uae_u16 immediate);
STATIC_INLINE void helper_allocate_2_ax_dest_mem_regs(struct comptbl* props, int modified);
STATIC_INLINE comp_tmp_reg* helper_read_memory(uae_u64 regsin, const cpu_history* history, comp_tmp_reg* target_reg, uae_u8 size, BOOL preservedestreg);
STATIC_INLINE comp_tmp_reg* helper_read_memory_mapped(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, uae_u8 size, BOOL preservedestreg);
STATIC_INLINE void helper_write_memory(uae_u64 regsin, const cpu_history* history, comp_tmp_reg* target_mem, comp_tmp_reg* input_reg, uae_u8 size);
STATIC_INLINE void helper_write_memory_mapped(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, comp_tmp_reg* input_reg, uae_u8 size);
STATIC_INLINE BOOL helper_write_memory_mapped_no_free(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, comp_ppc_reg input_reg_mapped, uae_u8 size);
STATIC_INLINE void helper_check_result_set_flags(uae_u64 regsin, comp_ppc_reg input_reg, uae_u8 size);
STATIC_INLINE void helper_copy_result_set_flags(comp_tmp_reg* src_reg, uae_u8 size);
STATIC_INLINE void helper_MOVIMMREG2MEM(const cpu_history* history, uae_u8 size, int immediate, int checkflags);
STATIC_INLINE void helper_MOVMEM2REG(const cpu_history* history, struct comptbl* props, uae_u8 size, int dataregmode);
STATIC_INLINE void helper_MOVEM2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size, BOOL update);
STATIC_INLINE void helper_MOVEM2REG(const cpu_history* history, struct comptbl* props, uae_u8 size, BOOL update);
STATIC_INLINE void helper_MOVIMM2REG(uae_u8 size);
STATIC_INLINE void helper_RTS_RTD(const cpu_history* history, uae_u16 stackptr_change);
STATIC_INLINE void helper_MULS(uae_u64 regsin, comp_ppc_reg src_input_reg_mapped);
STATIC_INLINE void helper_MULU(uae_u64 regsin, comp_ppc_reg src_input_reg_mapped);
STATIC_INLINE void helper_copy_word_with_flagcheck(comp_tmp_reg* tmpreg);
STATIC_INLINE void helper_copy_byte_with_flagcheck(comp_tmp_reg* tmpreg);
STATIC_INLINE comp_tmp_reg* helper_pre_word(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE comp_tmp_reg* helper_pre_word_filled(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE void helper_pre_word_filled_noalloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped);
STATIC_INLINE void helper_pre_word_no_alloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped);
STATIC_INLINE void helper_post_word(uae_u64 regsout, comp_tmp_reg* tmpreg, comp_tmp_reg* output_reg);
STATIC_INLINE void helper_post_word_no_free(uae_u64 regsout, comp_tmp_reg* output_reg);
STATIC_INLINE comp_tmp_reg* helper_pre_byte(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE comp_tmp_reg* helper_pre_byte_filled(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE void helper_pre_byte_filled_noalloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped);
STATIC_INLINE void helper_pre_byte_no_alloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped);
STATIC_INLINE void helper_post_byte(uae_u64 regsout, comp_tmp_reg* tmpreg, comp_tmp_reg* output_reg);
STATIC_INLINE void helper_post_byte_no_free(uae_u64 regsout, comp_tmp_reg* output_reg);
STATIC_INLINE comp_tmp_reg* helper_prepare_word_shift(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE void helper_prepare_word_shift_no_alloc(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE comp_tmp_reg* helper_prepare_byte_shift_left(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE comp_tmp_reg* helper_prepare_byte_shift_right(uae_u64 regsin, comp_tmp_reg* input_reg);
STATIC_INLINE void helper_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, comp_tmp_reg* output_mem_reg, comp_tmp_reg* base_reg, uae_u32 base_address);
STATIC_INLINE void helper_complete_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, comp_tmp_reg* output_mem_reg, comp_tmp_reg* base_reg, uae_u32 base_address, uae_u16 ext);
STATIC_INLINE comp_tmp_reg* helper_calculate_complex_index(uae_u16 ext);
STATIC_INLINE void helper_test_bit_register_imm(uae_u64 regsin, int bitnum, comp_tmp_reg* input_reg);
STATIC_INLINE comp_tmp_reg* helper_test_bit_register_register(uae_u64 regsbit, uae_u64 regsin, comp_tmp_reg* bitnum_reg, comp_tmp_reg* input_reg, int modulo);
STATIC_INLINE uae_u32 helper_convert_ccr_to_internal_static(uae_u8 ccr);
STATIC_INLINE uae_u64 helper_calculate_ccr_flag_dependency(uae_u8 ccr);
STATIC_INLINE void helper_bit_field_reg_opertion_flag_test(signed int extword, comp_tmp_reg** returned_mask_reg, comp_tmp_reg** returned_summary_offset_reg);
STATIC_INLINE void helper_bit_field_mem_opertion_flag_test(const cpu_history* history, signed int extword, comp_tmp_reg** returned_data_reg_combined, comp_tmp_reg** returned_data_reg_high, comp_tmp_reg** returned_data_reg_low, comp_tmp_reg** returned_mask_reg_high, comp_tmp_reg** returned_mask_reg_low, comp_tmp_reg** returned_summary_offset_reg, comp_tmp_reg** returned_complement_width, BOOL return_adjusted_dest_address, BOOL return_offset);
STATIC_INLINE void helper_bit_field_mem_save(const cpu_history* history, comp_tmp_reg* data_reg_low, comp_tmp_reg* data_reg_high);
STATIC_INLINE comp_tmp_reg* helper_extract_bitfield_offset(signed int extword);
STATIC_INLINE comp_tmp_reg* helper_create_bitfield_mask(signed int extword, comp_tmp_reg* summary_offset_reg, uae_u64 bit_field_offset_dep, comp_ppc_reg bit_field_offset_mapped, comp_tmp_reg** returned_bit_field_complement_width);
STATIC_INLINE comp_tmp_reg* helper_bit_field_extract_reg(signed int extword, uae_u64* returned_dependency, BOOL is_src_reg);
STATIC_INLINE void helper_mov16(const cpu_history* history, uae_u64 local_src_dep, comp_tmp_reg* local_src_reg, BOOL update_src, uae_u64 local_dest_dep, comp_tmp_reg* local_dest_reg, BOOL update_dest);
STATIC_INLINE void helper_divl(const cpu_history* history, comp_tmp_reg* local_src_reg, BOOL free_reg);
STATIC_INLINE void helper_mull(const cpu_history* history, comp_tmp_reg* local_src_reg, uae_s8 src_reg_num, BOOL free_reg);
STATIC_INLINE void helper_ABCD_SBCD_MEM(const cpu_history* history, BOOL subtraction);
STATIC_INLINE void helper_extract_flags_for_decimal(comp_tmp_reg* flagc_reg, comp_tmp_reg* local_dest_reg);
STATIC_INLINE comp_tmp_reg* helper_copy_x_flag_to_register(void);

/**
 * Local variables
 */

//Source register (allocated temporary register) for the opcode
//Initialized and released by the addressing mode
comp_tmp_reg* src_reg;

//Flag for negate the value of the evaluated condition code before use
//This boolean variable is used by the condition code addressing modes
//and the depending instruction handlers. If it is set to TRUE then
//the condition will be negated before use.
int src_condition_negate;

//Destination register (allocated temporary register) for the opcode
//Initialized and released by the addressing mode
comp_tmp_reg* dest_reg;

//Input register dependency mask
//Initialized and released by the addressing mode
uae_u64 input_dep;

//Output register dependency mask
//Initialized and released by the addressing mode
uae_u64 output_dep;

//Source immediate value
//Initialized by the addressing mode
signed int src_immediate;

//Destination immediate value
//Initialized by the addressing mode
signed int dest_immediate;

//Source addressing mode register (allocated temporary register) that contains
//the precalculated memory address for the opcode memory operations
//Initialized and released by the addressing mode
comp_tmp_reg* src_mem_addrreg;

//Destination addressing mode register (allocated temporary register) that contains
//the precalculated memory address for the opcode memory operations
//Initialized and released by the addressing mode
comp_tmp_reg* dest_mem_addrreg;

//Pointer to the next word sized data in memory after the opcode
//Each addressing mode that needs additional data increments this pointer
//to the next memory address after the read data
uae_u16* pc_ptr;

//Addressing mode indAd16 fallback to indA flag, if TRUE then the displacement is
//zero, indAd16 is executed as indA for source.
BOOL inda16_src_fallback;

//Addressing mode indAd16 fallback to indA flag, if TRUE then the displacement is
//zero, indAd16 is executed as indA for destination.
BOOL inda16_dest_fallback;

void comp_opcode_init(const cpu_history* history, uae_u8 extension)
{
	//The next word after the opcode
	pc_ptr = history->location + 1 + extension;

	//Reset variables
	input_dep = output_dep = COMP_COMPILER_MACROBLOCK_REG_NONE;
	src_mem_addrreg = dest_mem_addrreg = NULL;
	src_condition_negate = FALSE;
}

/**
 * Addressing mode compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
 */
void comp_addr_pre_regD_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->srcreg);
}
void comp_addr_pre_regA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, FALSE);
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
	//If the address register is the stack pointer then it is always
	//word aligned: step 2 bytes
	helper_add_imm_to_src_ax(props, props->srcreg == 7 ? -2 : -1);
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
	//If the address register is the stack pointer then it is always
	//word aligned: step 2 bytes
	helper_add_imm_to_src_ax(props, props->srcreg == 7 ? 2 : 1);
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
				src_mem_addrreg->reg_usage_mapping,
				src_mem_addrreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				*(pc_ptr++));

		//Addressing mode was executed with displacement
		inda16_src_fallback = FALSE;
	} else {
		comp_addr_pre_indA_src(history, props);
		pc_ptr++;

		//Addressing mode fell back to indA
		inda16_src_fallback = TRUE;
	}
}
void comp_addr_pre_indPCd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the word offset,
	//load it with sign extension and add it to the current PC
	//(also taking account the previous steps for the PC)
	uae_u32 address = COMP_GET_CURRENT_PC + *((signed short*)pc_ptr);
	pc_ptr++;

	src_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_absW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the address value,
	//load it without sign extension
	uae_u16 address = *pc_ptr;
	pc_ptr++;

	src_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_absL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the address value, load it
	uae_u32 address = *((uae_u32*)pc_ptr);
	pc_ptr += 2;

	src_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_indAcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in src_mem_addrreg
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			src_reg, 0);
}
void comp_addr_pre_indPCcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the current PC to the address variable
	uae_u32 address = COMP_GET_CURRENT_PC;

	//Map registers
	helper_allocate_2_ax_src_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in src_mem_addrreg
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			NULL,
			address);
}

/**
 * Addressing mode compiler functions for source addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
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
	if (!inda16_src_fallback)
	{
		//Release temp register
		helper_free_src_mem_addr_temp_reg();
	} else {
		//Addressing mode fell back to indA, execute indA post function
		comp_addr_post_indA_src(history, props);
	}
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
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
 */
void comp_addr_pre_regD_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->destreg);
}
void comp_addr_pre_regA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}
void comp_addr_pre_indA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, FALSE);
}
void comp_addr_pre_indmAL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate the destination register and a temporary memory target register,
	//address register won't be modified here
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//TODO: this is not the optimal way: the memory reading and the decrease must be done twice. See also post_indmAL_dest
	//Decrease the temporary memory target register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			-4);
}
void comp_addr_pre_indmAW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate the destination register and a temporary memory target register,
	//address register won't be modified here
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//TODO: this is not the optimal way: the memory reading and the decrease must be done twice. See also post_indmAW_dest
	//Decrease the temporary memory target register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			-2);
}
void comp_addr_pre_indmAB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate the destination register and a temporary memory target register,
	//address register won't be modified here
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//TODO: this is not the optimal way: the memory reading and the decrease must be done twice. See also post_indmAB_dest
	//Decrease the temporary memory target register
	//If the address register is the stack pointer then it is always
	//word aligned: step 2 bytes
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			props->destreg == 7 ? -2 : -1);
}
void comp_addr_pre_indApL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate register for the memory address, post increment delayed
	helper_allocate_ax_dest_mem_reg(props, FALSE);
}
void comp_addr_pre_indApW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate register for the memory address, post increment delayed
	helper_allocate_ax_dest_mem_reg(props, FALSE);
}
void comp_addr_pre_indApB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate register for the memory address, post increment delayed
	helper_allocate_ax_dest_mem_reg(props, FALSE);
}
void comp_addr_pre_immedL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the immediate value,
	//load it with sign extension
	dest_immediate = *((signed int*)pc_ptr);
	pc_ptr += 2;
}
void comp_addr_pre_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the immediate value,
	//load it with sign extension
	dest_immediate = *((signed short*)pc_ptr);
	pc_ptr++;
}
void comp_addr_pre_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next byte after the instruction is the immediate value,
	//load it with sign extension
	dest_immediate = *(((signed char*)pc_ptr + 1));
	//Step a whole word, because the instruction pointer cannot point to odd address
	pc_ptr++;
}
void comp_addr_pre_immedQ_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Destination register in props is the immediate value,
	//load it with sign extension
	dest_immediate = (signed char)props->destreg;
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
				dest_mem_addrreg->reg_usage_mapping,
				dest_mem_addrreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				*(pc_ptr++));

		//Addressing mode was executed with displacement
		inda16_dest_fallback = FALSE;
	} else {
		comp_addr_pre_indA_dest(history, props);
		pc_ptr++;

		//Addressing mode fell back to indA
		inda16_dest_fallback = TRUE;
	}
}
void comp_addr_pre_indPCd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the word offset,
	//load it with sign extension and add it to the current PC
	//(also taking account the previous steps for the PC)
	uae_u32 address = COMP_GET_CURRENT_PC + *((signed short*)pc_ptr);
	pc_ptr++;

	dest_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_absW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next word after the instruction is the address value,
	//load it without sign extension
	uae_u16 address = *pc_ptr;
	pc_ptr++;

	dest_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_absL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//The next longword after the instruction is the address value, load it
	uae_u32 address = *((uae_u32*)pc_ptr);
	pc_ptr += 2;

	dest_mem_addrreg = helper_allocate_tmp_reg();

	comp_macroblock_push_load_register_long(
			dest_mem_addrreg->reg_usage_mapping,
			dest_mem_addrreg->mapped_reg_num,
			address);
}
void comp_addr_pre_indAcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in dest_mem_addrreg
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			dest_reg, 0);
}
void comp_addr_pre_indPCcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the current PC to the address variable
	uae_u32 address = COMP_GET_CURRENT_PC - (uae_u32)history->location;

	//Map registers
	helper_allocate_2_ax_dest_mem_regs(props, FALSE);

	//Process the extension word, result is coming back in dest_mem_addrreg
	helper_complex_addressing(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			NULL,
			address);
}

/**
 * Addressing mode compiler functions for destination addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
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
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();

	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Decrease the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, -4);
}
void comp_addr_post_indmAW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();

	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Decrease the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, -2);
}
void comp_addr_post_indmAB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Release temp register
	helper_free_dest_mem_addr_temp_reg();

	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Decrease the address register by the size of the operation
	//If the address register is the stack pointer then it is always
	//word aligned: step 2 bytes
	helper_add_imm_to_dest_ax(props, props->destreg == 7 ? -2 : -1);
}
void comp_addr_post_indApL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 4);
}
void comp_addr_post_indApW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 2);
}
void comp_addr_post_indApB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//(Re)allocate the destination address register with modify flag
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	//Increase the address register by the size of the operation
	//If the address register is the stack pointer then it is always
	//word aligned: step 2 bytes
	helper_add_imm_to_dest_ax(props, props->destreg == 7 ? 2 : 1);
}
void comp_addr_post_immedL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_immedQ_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//No need to do anything
}
void comp_addr_post_indAd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (!inda16_dest_fallback)
	{
		//Release temp register
		helper_free_dest_mem_addr_temp_reg();
	} else {
		//Addressing mode fell back to indA, execute indA post function
		comp_addr_post_indA_dest(history, props);
	}
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
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
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
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			0, PPC_FLAGBIT_C, PPC_FLAGBIT_C, TRUE);
}
void comp_cond_pre_CC_eq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if Z
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
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
	comp_tmp_reg* tmp_reg = helper_allocate_tmp_reg();

	//Z <= if (N^V)|Z
	//Mask out N and Z flag to temp register
	comp_macroblock_push_and_high_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			tmp_reg->reg_usage_mapping,
			tmp_reg->mapped_reg_num,
			PPCR_FLAGS_MAPPED,
			(1 << (FLAGBIT_N - 16)) | (1 << (FLAGBIT_Z - 16)));

	//Rotate V flag to N flag position to R0
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			PPC_FLAGBIT_V - PPC_FLAGBIT_N, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Exclusive OR N and repositioned V flag, Z flag left unchanged (OR'ed to the result)
	comp_macroblock_push_xor_register_register(
			tmp_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			PPCR_SPECTMP_MAPPED,
			tmp_reg->mapped_reg_num, TRUE);

	comp_free_temp_register(tmp_reg);
}
void comp_cond_pre_CC_ls_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if C|Z
	//Mask out C flag to temp register
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			0, PPC_FLAGBIT_C, PPC_FLAGBIT_C, FALSE);

	//Insert Z flag to the temp register
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			0, PPC_FLAGBIT_Z, PPC_FLAGBIT_Z, TRUE);
}
void comp_cond_pre_CC_lt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* tmp_reg = helper_allocate_tmp_reg();

	//Z <= if N^V
	//Mask out N flag to temp register
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN,
			tmp_reg->reg_usage_mapping,
			tmp_reg->mapped_reg_num,
			PPCR_FLAGS_MAPPED,
			0, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Rotate V flag to N flag position to R0
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			PPC_FLAGBIT_V - PPC_FLAGBIT_N, PPC_FLAGBIT_N, PPC_FLAGBIT_N, FALSE);

	//Exclusive OR N and repositioned V flag
	comp_macroblock_push_xor_register_register(
			tmp_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			PPCR_SPECTMP_MAPPED,
			tmp_reg->mapped_reg_num, TRUE);

	comp_free_temp_register(tmp_reg);
}
void comp_cond_pre_CC_mi_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Z <= if N
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
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
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
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
 *    history - pointer to the execution history
 *    props - pointer to the instruction properties
 */
void comp_opcode_MOVREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		comp_macroblock_push_copy_register_long_with_flags(
				input_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);
		break;
	case 2:
		comp_macroblock_push_copy_register_word(
				input_dep | output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);

		comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		break;
	case 1:
		comp_macroblock_push_copy_register_byte(
				input_dep | output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);

		comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		break;
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_MOVAREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		comp_macroblock_push_copy_register_long(
				input_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);
	break;
	case 2:
		//Address registers are always longword sized: sign extend source data
		comp_macroblock_push_copy_register_word_extended(
				input_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);
		break;
	}

	//No flag change
}
void comp_opcode_MOVMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tmpreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			TRUE);

	//Check register
    helper_check_result_set_flags(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num, size);

    //Save flags
	helper_check_nz_clear_cv_flags();

	//Write memory to destination and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, size);
}
void comp_opcode_MOVREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, props->size, FALSE, TRUE);
}
void comp_opcode_MOVMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, props->size, TRUE);
}
void comp_opcode_MOVAMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVMEM2REG(history, props, props->size, FALSE);
}
void comp_opcode_MOVIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(props->size);
}
void comp_opcode_MOVAIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		//Load the immediate value to the destination register
		comp_macroblock_push_load_register_long(
				output_dep,
				dest_reg->mapped_reg_num,
				src_immediate);
		break;
	case 2:
		//Load the immediate value to the destination register
		//sign-extended to longword size
		comp_macroblock_push_load_register_word_extended(
				output_dep,
				dest_reg->mapped_reg_num,
				src_immediate);
		break;
	}
}
void comp_opcode_MOVEQ(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_load_register_long(
			output_dep,
			dest_reg->mapped_reg_num,
			src_immediate);

	//Set up flags
	helper_move_inst_static_flags(src_immediate);
}
void comp_opcode_MOVEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2MEM(history, props, props->size, FALSE);
}
void comp_opcode_MOVEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2REG(history, props, props->size, FALSE);
}
void comp_opcode_MOVEM2MEMU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2MEM(history, props, props->size, TRUE);
}
void comp_opcode_MOVEM2REGU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVEM2REG(history, props, props->size, TRUE);
}
void comp_opcode_MOVIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMMREG2MEM(history, props->size, TRUE, TRUE);
}
void comp_opcode_MOV16REG2REGU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Get extension word
	signed int extword = *((signed short*)(history->location + 1));

	//Extract destination register
	int regnum = (extword >> 12) & 7;

	//Map destination register
	comp_tmp_reg* local_dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(regnum), TRUE, TRUE);
	uae_u64 local_dest_reg_dep = COMP_COMPILER_MACROBLOCK_REG_AX(regnum);

	//Remap source register for update
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, TRUE);
	input_dep = COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);

	//Copy memory line
	helper_mov16(history, input_dep, src_reg, TRUE, local_dest_reg_dep, local_dest_reg, TRUE);
}
void comp_opcode_MOV16REG2MEMU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Remap source register for update
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, TRUE);
	input_dep = COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);

	//Allocate memory address register for destination and init with the address
	//TODO: this can be optimized by pre-loading the immediate directly into the address register used in the helper
	comp_tmp_reg* local_dest_reg = helper_allocate_tmp_reg_with_init(dest_immediate);

	//Copy memory line
	helper_mov16(history,
				 input_dep, src_reg, TRUE,
				 local_dest_reg->reg_usage_mapping, local_dest_reg, FALSE);
}
void comp_opcode_MOV16MEM2REGU(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Remap destination register for update
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);
	output_dep = COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);

	//Allocate memory address register for source and init with the address
	//TODO: this can be optimized by pre-loading the immediate directly into the address register used in the helper
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Copy memory line
	helper_mov16(history,
				 local_src_reg->reg_usage_mapping, local_src_reg, FALSE,
				 output_dep, dest_reg, TRUE);
}
void comp_opcode_MOV16REG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate memory address register for destination and init with the address
	//TODO: this can be optimized by pre-loading the immediate directly into the address register used in the helper
	comp_tmp_reg* local_dest_reg = helper_allocate_tmp_reg_with_init(dest_immediate);

	//Copy memory line
	helper_mov16(history,
				 input_dep, src_reg, FALSE,
				 local_dest_reg->reg_usage_mapping, local_dest_reg, FALSE);
}
void comp_opcode_MOV16MEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate memory address register for source and init with the address
	//TODO: this can be optimized by pre-loading the immediate directly into the address register used in the helper
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Copy memory line
	helper_mov16(history,
				 local_src_reg->reg_usage_mapping, local_src_reg, FALSE,
				 output_dep, dest_reg, FALSE);
}
void comp_opcode_CLRREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.L #imm,reg instruction
	src_immediate = 0;
	helper_MOVIMM2REG(props->size);
}
void comp_opcode_CLRMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.L #imm,mem instruction
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, props->size, TRUE, TRUE);
}
void comp_opcode_LEAIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		//Load the immediate value to the destination register
		comp_macroblock_push_load_register_long(
				output_dep,
				dest_reg->mapped_reg_num,
				src_immediate);
		break;
	case 2:
		//Load the immediate value to the destination register
		//sign-extended to longword size
		comp_macroblock_push_load_register_word_extended(
				output_dep,
				dest_reg->mapped_reg_num,
				src_immediate);
		break;
	}
}
void comp_opcode_LEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//TODO: this useless move instruction could be removed if the registers could be swapped
	//Move temporary register to the destination address register
	comp_macroblock_push_copy_register_long(
			input_dep | (src_mem_addrreg == NULL ? 0 : src_mem_addrreg->reg_usage_mapping),
			output_dep,
			dest_reg->mapped_reg_num,
			src_mem_addrreg->mapped_reg_num);
}
void comp_opcode_PEAIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Save immediate address to the stack:
	//decrease stack pointer (A7) and store the longword to the pointed address

	//Immediate is sign-extended at loading (addresses are always longwords)
	//everything else is the same for the word variant as the longword variant

	//Map A7 register as destination
	dest_mem_addrreg = dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(7);

	//Decrease A7 register by 4
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			-4);

	//Store immediate using the A7 register into memory, skip the flag checking
	helper_MOVIMMREG2MEM(history, 4, TRUE, FALSE);
}
void comp_opcode_PEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Save immediate address to the stack:
	//decrease stack pointer (A7) and store the longword to the pointed address

	//Map A7 register as destination
	dest_mem_addrreg = dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(7);

	//The source memory mapped register is the source register for the memory operation
	//Is A7 the source register?
	if (src_mem_addrreg == dest_mem_addrreg)
	{
		//We need a temporary register with the unchanged content of A7 as source
		src_reg = helper_allocate_tmp_reg();
		input_dep |= src_reg->reg_usage_mapping;

		//Copy the content of A7 to the temp register
		comp_macroblock_push_copy_register_long(
				dest_reg->reg_usage_mapping,
				src_reg->reg_usage_mapping,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Use the source memory register as source
		src_reg = src_mem_addrreg;
		input_dep |= src_mem_addrreg->reg_usage_mapping;
	}

	//Decrease A7 register by 4
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			-4);

	//Store source address from the register using the A7 register into memory,
	//skip the flag checking
	helper_MOVIMMREG2MEM(history, 4, FALSE, FALSE);
}
void comp_opcode_STREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Set lowest byte to 0xff (Scc with the condition of TRUE)
	comp_macroblock_push_or_low_register_imm(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			0xff);
}
void comp_opcode_SFREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Set lowest byte to 0 (Scc with the condition of FALSE)
	comp_macroblock_push_rotate_and_mask_bits(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			0, 0, 23, FALSE);
}
void comp_opcode_SCCREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//This instruction was implemented in one macroblock for simplicity
	comp_macroblock_push_set_byte_from_z_flag(
			output_dep,
			dest_reg->mapped_reg_num,
			src_condition_negate);
}
void comp_opcode_STMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load -1 as immediate and call normal MOVE.B #imm,mem instruction
	//flag checking is skipped
	src_immediate = -1;
	helper_MOVIMMREG2MEM(history, 1, TRUE, FALSE);
}
void comp_opcode_SFMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load zero as immediate and call normal MOVE.B #imm,mem instruction
	//flag checking is skipped
	src_immediate = 0;
	helper_MOVIMMREG2MEM(history, 1, TRUE, FALSE);
}
void comp_opcode_SCCMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate a temp register for the calculated source byte
	src_reg = helper_allocate_tmp_reg();
	input_dep |= src_reg->reg_usage_mapping;

	//Call the macroblock which produces the source byte into the temp register
	comp_macroblock_push_set_byte_from_z_flag(
			src_reg->reg_usage_mapping,
			src_reg->mapped_reg_num,
			src_condition_negate);

	//Call normal MOVE.B reg,mem instruction
	helper_MOVIMMREG2MEM(history, 1, FALSE, FALSE);

	comp_free_temp_register(src_reg);
}
void comp_opcode_MOVCCR2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

	//Convert the CCR register to internal flag format
	comp_macroblock_push_convert_internal_to_ccr(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

    comp_macroblock_push_copy_register_word(
			tempreg->reg_usage_mapping | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_MOVCCR2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

	//Convert the CCR register to internal flag format
	comp_macroblock_push_convert_internal_to_ccr(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 2);
}
void comp_opcode_MOVIMM2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Convert CCR to internal flag format and return it as an immediate
	uae_u32 ccr = helper_convert_ccr_to_internal_static(src_immediate);

	//Load the immediate directly into the flag register
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			PPCR_FLAGS_MAPPED,
			ccr);
}
void comp_opcode_MOVREG2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Convert the CCR register to internal flag format
	comp_macroblock_push_convert_ccr_to_internal(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			PPCR_FLAGS_MAPPED,
			src_reg->mapped_reg_num);
}
void comp_opcode_MOVMEM2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			FALSE);

	//Convert the CCR register to internal flag format
	comp_macroblock_push_convert_ccr_to_internal(
			tempreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			PPCR_FLAGS_MAPPED,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_DBCOND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* address_reg = helper_allocate_tmp_reg();
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

	//The next word after the instruction is the branch offset,
	//load it with sign extension
	signed int branch_offset = *((signed short*)(history->location + 1));

    //The whole instruction is implemented in this macroblock:
	//checks the previous result (Z flag) and jumps if evaluated to TRUE
	//or decrements the specified register and jumps if it reaches -1.
	comp_macroblock_push_load_pc_from_immediate_conditional_decrement_register(
			output_dep,
			dest_reg->mapped_reg_num,
			(uae_u32)history->pc + branch_offset + 2,
			(uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location),
			src_condition_negate,
			address_reg->mapped_reg_num,
			tempreg->mapped_reg_num);

	//Free temporary registers
	helper_free_tmp_reg(address_reg);
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_DBF(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* address_reg = helper_allocate_tmp_reg();
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

	//Word sized operation: mask out lowest word from target register
	//This will also set the Z flag on PPC
	comp_macroblock_push_and_low_register_imm(
			output_dep,
			tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tempreg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			0xffff);

	//Decrease target register
	comp_macroblock_push_add_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			-1);

	//Insert result back to source register
    comp_macroblock_push_copy_register_word(
			tempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			tempreg->mapped_reg_num);


    //Branch if the result is -1 (the Z flag is coming from the register before decrementation)
	//load target address to PC, if Z flag set
	comp_macroblock_push_load_pc_from_immediate_conditional(
			(uae_u32)history->pc + src_immediate + 2,
			(uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location),
			FALSE,
			address_reg->mapped_reg_num,
			tempreg->mapped_reg_num);

	//Free temporary registers
	helper_free_tmp_reg(address_reg);
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_BCOND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate required temporary registers
	comp_tmp_reg* address_reg = helper_allocate_tmp_reg();
	comp_tmp_reg* tmp_reg = helper_allocate_tmp_reg();

	//Load target address to PC, if Z flag set (or not set if condition is negated)
	comp_macroblock_push_load_pc_from_immediate_conditional(
			(uae_u32)history->pc + dest_immediate + 2,
			(uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location),
			src_condition_negate,
			address_reg->mapped_reg_num,
			tmp_reg->mapped_reg_num);

	//Free temporary registers
	helper_free_tmp_reg(address_reg);
	helper_free_tmp_reg(tmp_reg);
}
void comp_opcode_BRA(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load target address into a temporary register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init((uae_u32)history->pc + src_immediate + 2);

	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_BSR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load target address into a temporary register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init((uae_u32)history->pc + src_immediate + 2);

	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);

	//Load the next address after the current instruction into a temp register
	tempreg = helper_allocate_tmp_reg_with_init((uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location));

	//Load stack register
	comp_tmp_reg* a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Subtract data size from stack register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			COMP_COMPILER_MACROBLOCK_REG_A7,
			a7_reg->mapped_reg_num,
			a7_reg->mapped_reg_num,
			-4);

	//Save it to the stack and free temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_A7,
			history,
			a7_reg,
			tempreg, 4);
}
void comp_opcode_JMPIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load immediate into a temporary register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_JMPIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num);
}
void comp_opcode_JSRIMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load immediate into a temporary register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);

	//Load the next address after the current instruction into a temp register
	tempreg = helper_allocate_tmp_reg_with_init((uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location));

	//Load stack register
	comp_tmp_reg* a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Subtract data size from stack register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			COMP_COMPILER_MACROBLOCK_REG_A7,
			a7_reg->mapped_reg_num,
			a7_reg->mapped_reg_num,
			-4);

	//Save it to the stack and free temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_A7,
			history,
			a7_reg,
			tempreg, 4);
}
void comp_opcode_JSRIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load target address to PC
	comp_macroblock_push_load_pc_from_register(
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num);

	//Load the next address after the current instruction into a temp register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init((uae_u32)history->pc + ((uae_u32)pc_ptr - (uae_u32)history->location));

	//Load stack register
	comp_tmp_reg* a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Subtract data size from stack register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			COMP_COMPILER_MACROBLOCK_REG_A7,
			a7_reg->mapped_reg_num,
			a7_reg->mapped_reg_num,
			-4);

	//Save it to the stack and free temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_A7,
			history,
			a7_reg,
			tempreg, 4);
}
void comp_opcode_RTS(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_RTS_RTD(history, 4);
}
void comp_opcode_RTD(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_RTS_RTD(history, 4 + src_immediate);

	//TODO: throw exception if A7 is on odd address
}
void comp_opcode_RTR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load stack register
	comp_tmp_reg* a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, FALSE);

	//Read the CCR from the stack
	comp_tmp_reg* tempreg = helper_read_memory(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			history,
			a7_reg,
			2,
			FALSE);

	//Remap stack register
	a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Add CCR data size to stack register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			COMP_COMPILER_MACROBLOCK_REG_A7,
			a7_reg->mapped_reg_num,
			a7_reg->mapped_reg_num,
			2);

	//Convert the CCR register to internal flag format
	comp_macroblock_push_convert_ccr_to_internal(
			tempreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			PPCR_FLAGS_MAPPED,
			tempreg->mapped_reg_num);

	helper_free_tmp_reg(tempreg);

	//Do the normal RTS
	helper_RTS_RTD(history, 4);
}
void comp_opcode_EORIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		comp_tmp_reg* immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile XOR PPC opcode with flag update
		comp_macroblock_push_xor_register_register(
				output_dep | immtempreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile XOR immediate PPC opcode
		comp_macroblock_push_xor_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(output_dep, dest_reg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_EORIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		comp_tmp_reg* immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile XOR PPC opcode with flag update
		comp_macroblock_push_xor_register_register(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile XOR immediate PPC opcode
		comp_macroblock_push_xor_low_register_imm(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_EORREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	if (size == 4)
	{
		comp_macroblock_push_xor_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		comp_macroblock_push_xor_register_register(
				input_dep | output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				FALSE);

		helper_copy_result_set_flags(tmpreg, size);

		comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_EORREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Source register was free'd: reload
	comp_addr_pre_regD_src(history, props);

	if (size == 4)
	{
		//Compile XOR PPC opcode with flag update
		comp_macroblock_push_xor_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				TRUE);
	} else {
		//Compile XOR PPC opcode
		comp_macroblock_push_xor_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);

		//Check register
	    helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ANDIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* desttempreg;
	comp_tmp_reg* immtempreg;
	int size = props->size;

	if (size == 4)
	{
		immtempreg = helper_allocate_tmp_reg();

		//Load immediate into a temporary register
		comp_macroblock_push_load_register_long(
				immtempreg->reg_usage_mapping,
				immtempreg->mapped_reg_num,
				src_immediate);

		//Compile AND PPC opcode with flag update
		comp_macroblock_push_and_register_register(
				output_dep | immtempreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();

		helper_free_tmp_reg(immtempreg);
	} else {
		desttempreg = helper_allocate_tmp_reg();

		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile ANDI PPC opcode
		comp_macroblock_push_and_low_register_imm(
				output_dep,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				desttempreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_nz_clear_cv_flags();

		//Check flags
		helper_copy_result_set_flags(desttempreg, size);

		comp_free_temp_register(desttempreg);
	}
}
void comp_opcode_ANDIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile AND PPC opcode with flag update
		comp_macroblock_push_and_register_register(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile AND immediate PPC opcode
		comp_macroblock_push_and_low_register_imm(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ANDREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	if (size == 4)
	{
		comp_macroblock_push_and_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		comp_macroblock_push_and_register_register(
				input_dep | output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				FALSE);

		helper_copy_result_set_flags(tmpreg, size);

		comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_ANDREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Source register was free'd: reload
	comp_addr_pre_regD_src(history, props);

	if (size == 4)
	{
		//Compile AND PPC opcode with flag update
		comp_macroblock_push_and_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				TRUE);
	} else {
		//Compile AND PPC opcode
		comp_macroblock_push_and_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);

		//Check register
	    helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ANDMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		comp_macroblock_push_and_register_register(
				tempreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		comp_macroblock_push_and_register_register(
				input_dep | output_dep,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				FALSE);

		helper_copy_result_set_flags(tempreg, size);
	}

	comp_free_temp_register(tempreg);
}
void comp_opcode_ORIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile OR PPC opcode with flag update
		comp_macroblock_push_or_register_register(
				output_dep | immtempreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile OR immediate PPC opcode
		comp_macroblock_push_or_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(output_dep, dest_reg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_ORIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile OR PPC opcode with flag update
		comp_macroblock_push_or_register_register(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				TRUE);

		helper_free_tmp_reg(immtempreg);
	} else {
		//Prepare immediate, we use it as unsigned
		if (size == 1) src_immediate &= 0xff;

		//Compile OR immediate PPC opcode
		comp_macroblock_push_or_low_register_imm(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_immediate);

		//Check flags
		helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ORREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	if (size == 4)
	{
		comp_macroblock_push_or_register_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		comp_macroblock_push_or_register_register(
				input_dep | output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				FALSE);

		helper_copy_result_set_flags(tmpreg, size);

		comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_ORREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Source register was free'd: reload
	comp_addr_pre_regD_src(history, props);

	if (size == 4)
	{
		//Compile OR PPC opcode with flag update
		comp_macroblock_push_or_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				TRUE);
	} else {
		//Compile OR PPC opcode
		comp_macroblock_push_or_register_register(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);

		//Check register
	    helper_check_result_set_flags(tempreg->reg_usage_mapping, tempreg->mapped_reg_num, size);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ORMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		comp_macroblock_push_or_register_register(
				tempreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		helper_check_nz_clear_cv_flags();
	} else {
		comp_macroblock_push_or_register_register(
				tempreg->reg_usage_mapping | output_dep,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				FALSE);

		helper_copy_result_set_flags(tempreg, size);
	}

	comp_free_temp_register(tempreg);
}
void comp_opcode_NOTREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		comp_macroblock_push_not_or_register_register(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);
		break;
	case 2:
		comp_macroblock_push_xor_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				0xffff);

		//Check result
		comp_macroblock_push_check_word_register(
				output_dep,
				dest_reg->mapped_reg_num);
		break;
	case 1:
		comp_macroblock_push_xor_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				0xff);

		//Check result
		comp_macroblock_push_check_byte_register(
				output_dep,
				dest_reg->mapped_reg_num);
		break;
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_NOTMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	switch (size)
	{
	case 4:
		comp_macroblock_push_not_or_register_register(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				TRUE);
		break;
	case 2:
		comp_macroblock_push_xor_low_register_imm(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				0xffff);

		//Check result
		comp_macroblock_push_check_word_register(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		break;
	case 1:
		comp_macroblock_push_xor_low_register_imm(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				0xff);

		//Check result
		comp_macroblock_push_check_byte_register(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		break;
	default:
		write_log("Error: wrong operation size for NOTMEM\n");
		abort();
	}

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ANDIMM2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Convert CCR to internal flag format and return it as an immediate
	uae_u32 ccr = helper_convert_ccr_to_internal_static(src_immediate);

	//Preload immediate to a temp register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init(ccr);

	//And operation between the source and the flag register
	comp_macroblock_push_and_register_register(
			tempreg->reg_usage_mapping,
			helper_calculate_ccr_flag_dependency(src_immediate),
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			tempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_ORIMM2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Convert CCR to internal flag format and return it as an immediate
	uae_u32 ccr = helper_convert_ccr_to_internal_static(src_immediate);
	uae_u64 flag_dep = helper_calculate_ccr_flag_dependency(src_immediate);

	//Or operation between the source and the flag register for the lower word
	if ((ccr & 0xffff) != 0)
	{
		comp_macroblock_push_or_low_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGX & flag_dep,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				ccr & 0xffff);
	}

	//Or operation between the source and the flag register for the higher word
	if (((ccr >> 16) & 0xffff) != 0)
	{
		comp_macroblock_push_or_high_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				(COMP_COMPILER_MACROBLOCK_REG_FLAGC |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGV |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGN |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGZ) & flag_dep,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				(ccr >> 16) & 0xffff);
	}
}
void comp_opcode_EORIMM2CCR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Convert CCR to internal flag format and return it as an immediate
	uae_u32 ccr = helper_convert_ccr_to_internal_static(src_immediate);
	uae_u64 flag_dep = helper_calculate_ccr_flag_dependency(src_immediate);

	//Exclusive or operation between the source and the flag register for the lower word
	if ((ccr & 0xffff) != 0)
	{
		comp_macroblock_push_xor_low_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGX & flag_dep,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				ccr & 0xffff);
	}

	//Exclusive or operation between the source and the flag register for the higher word
	if (((ccr >> 16) & 0xffff) != 0)
	{
		comp_macroblock_push_xor_high_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				(COMP_COMPILER_MACROBLOCK_REG_FLAGC |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGV |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGN |
				  COMP_COMPILER_MACROBLOCK_REG_FLAGZ) & flag_dep,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				(ccr >> 16) & 0xffff);
	}
}
void comp_opcode_BTSTIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			FALSE);

	//Limit immediate into 0-7 range
	src_immediate &= 7;

	//Test bit in register
	helper_test_bit_register_imm(
			tempreg->reg_usage_mapping,
			src_immediate,
			tempreg);

	//Free temp register
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_BTSTREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			FALSE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	//Test bit in register and return mask in a register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			tempreg->reg_usage_mapping,
			src_reg,
			tempreg,
			8);

	//Free temp regs
	helper_free_tmp_reg(masktempreg);
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_BTSTIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Limit immediate into 0-31 range
	src_immediate &= 31;

	//Test bit in register
	helper_test_bit_register_imm(output_dep, src_immediate, dest_reg);
}
void comp_opcode_BTSTREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Test bit in register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			output_dep,
			src_reg,
			dest_reg,
			32);

	helper_free_tmp_reg(masktempreg);
}
void comp_opcode_BTSTREG2IMM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* masktempreg = helper_allocate_tmp_reg_with_init(1);

	comp_tmp_reg* srctmpreg = helper_allocate_tmp_reg();

	//Modulo 8 for the input register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			srctmpreg->reg_usage_mapping,
			srctmpreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			7);

	//Rotate the masking bit to the right position
	comp_macroblock_push_rotate_and_mask_bits_register(
			srctmpreg->reg_usage_mapping | masktempreg->reg_usage_mapping,
			masktempreg->reg_usage_mapping,
			masktempreg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			srctmpreg->mapped_reg_num,
			0, 31, FALSE);

	//And operation with the source register mask and the immediate
	comp_macroblock_push_and_low_register_imm(
			masktempreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			masktempreg->mapped_reg_num,
			dest_immediate);

	//Free temp registers
	helper_free_tmp_reg(masktempreg);
	helper_free_tmp_reg(srctmpreg);

	//Save Z flag
	helper_check_z_flag();
}
void comp_opcode_BSETIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Limit immediate into 0-7 range
	src_immediate &= 7;

	//Test bit in register
	helper_test_bit_register_imm(
			tempreg->reg_usage_mapping,
			src_immediate,
			tempreg);

	//Set the bit in the lower word
	comp_macroblock_push_or_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			1 << src_immediate);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BSETREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	//Test bit in register and return mask in a register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			tempreg->reg_usage_mapping,
			src_reg,
			tempreg,
			8);

	//Set the bit
	comp_macroblock_push_or_register_register(
			masktempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BSETIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Limit immediate into 0-31 range
	src_immediate &= 31;

	//Test bit in register
	helper_test_bit_register_imm(output_dep, src_immediate, dest_reg);

	if (src_immediate < 16)
	{
		//Bit is in the lower word
		comp_macroblock_push_or_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1 << src_immediate);
	} else {
		//Bit is in the higher word
		comp_macroblock_push_or_high_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1 << (src_immediate - 16));
	}
}
void comp_opcode_BSETREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Test bit in register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			output_dep,
			src_reg,
			dest_reg,
			32);

	//Set bit in output register
	comp_macroblock_push_or_register_register(
			output_dep | masktempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);
}
void comp_opcode_BCLRIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Limit immediate into 0-7 range
	src_immediate &= 7;

	//Test bit in register
	helper_test_bit_register_imm(
			tempreg->reg_usage_mapping,
			src_immediate,
			tempreg);

	//Clear the bit in the lower word (operation is byte-sized)
	comp_macroblock_push_and_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			~(1 << src_immediate));

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BCLRREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	//Test bit in register and return mask in a register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			tempreg->reg_usage_mapping,
			src_reg,
			tempreg,
			8);

	//Clear the bit
	comp_macroblock_push_and_register_complement_register(
			masktempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BCLRIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Limit immediate into 0-31 range
	src_immediate &= 31;

	//Test bit in register
	helper_test_bit_register_imm(output_dep, src_immediate, dest_reg);

	//Load inverted mask into temp register
	comp_tmp_reg* tempreg = helper_allocate_tmp_reg_with_init(~(1 << src_immediate));

	//Mask it
	comp_macroblock_push_and_register_register(
			output_dep | tempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			tempreg->mapped_reg_num,
			FALSE);

	comp_free_temp_register(tempreg);
}
void comp_opcode_BCLRREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Test bit in register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			output_dep,
			src_reg,
			dest_reg,
			32);

	//Clear bit in output register (and with complement of the mask)
	comp_macroblock_push_and_register_complement_register(
			output_dep | masktempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);
}
void comp_opcode_BCHGIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Limit immediate into 0-7 range
	src_immediate &= 7;

	//Test bit in register
	helper_test_bit_register_imm(
			tempreg->reg_usage_mapping,
			src_immediate,
			tempreg);

	//Reverse the bit in the lower word
	comp_macroblock_push_xor_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			1 << src_immediate);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BCHGREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source (byte size operation)
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	//Test bit in register and return mask in a register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			tempreg->reg_usage_mapping,
			src_reg,
			tempreg,
			8);

	//Set the bit
	comp_macroblock_push_xor_register_register(
			masktempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_BCHGIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Limit immediate into 0-31 range
	src_immediate &= 31;

	//Test bit in register
	helper_test_bit_register_imm(output_dep, src_immediate, dest_reg);

	if (src_immediate < 16)
	{
		//Bit is in the lower word
		comp_macroblock_push_xor_low_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1 << src_immediate);
	} else {
		//Bit is in the higher word
		comp_macroblock_push_xor_high_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1 << (src_immediate - 16));
	}
}
void comp_opcode_BCHGREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Test bit in register
	comp_tmp_reg* masktempreg = helper_test_bit_register_register(
			input_dep,
			output_dep,
			src_reg,
			dest_reg,
			32);

	//Set bit in output register
	comp_macroblock_push_xor_register_register(
			output_dep | masktempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(masktempreg);
}
void comp_opcode_TAS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Test byte-sized register
	comp_macroblock_push_check_byte_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Set highest bit of the lowest byte in source register
	comp_macroblock_push_or_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			0x80);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_TAS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Test byte-sized register
	comp_macroblock_push_check_byte_register(
			output_dep,
			dest_reg->mapped_reg_num);

	//Save flags
	helper_check_nz_clear_cv_flags();

	//Set highest bit of the lowest byte in source register
	comp_macroblock_push_or_high_register_imm(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			0x80);
}
void comp_opcode_ASLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* src_tmpreg;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C and X flag
	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, (size * 8) - src_immediate);

	//Load the shifting into a temp register
	//TODO: this could be managed by using the immediate directly
	src_tmpreg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Extract V flag
	comp_macroblock_push_arithmetic_left_shift_extract_v_flag(
			output_dep | src_tmpreg->reg_usage_mapping,
			dest_reg->mapped_reg_num,
			src_tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num);

	comp_free_temp_register(src_tmpreg);
	comp_free_temp_register(tmpreg);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate,
				0, 31 - src_immediate, TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate,
				0,
				size == 2 ? 15 - src_immediate : 7 - src_immediate,
				TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg);
		}
	}

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ASLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();
	int modulo = (8  * size) - 1;

	//Modulo for shift register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			modulo);

	//Extract C and X flag, clear V flag
	helper_extract_cx_clear_v_flags(output_dep | shiftreg->reg_usage_mapping, dest_reg, shiftreg, TRUE);

	//Extract V flag
	comp_macroblock_push_arithmetic_left_shift_extract_v_flag(
			output_dep | shiftreg->reg_usage_mapping,
			dest_reg->mapped_reg_num,
			shiftreg->mapped_reg_num,
			tmpreg->mapped_reg_num);

	comp_free_temp_register(tmpreg);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_logic_shift_left_register_register(
				output_dep | shiftreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Shifting to the left
		comp_macroblock_push_logic_shift_left_register_register(
				tmpreg->reg_usage_mapping | shiftreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg);
		}
	}

	//Free shift reg
	helper_free_tmp_reg(shiftreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ASLMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* dest_tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	//Extract C and X flag
	helper_extract_cx_clear_nzv_flags(dest_tmpreg->reg_usage_mapping, dest_tmpreg, (2 * 8) - 1);

	//Load the shifting into a temp register
	//TODO: this could be managed by using the immediate directly
	comp_tmp_reg* src_tmpreg = helper_allocate_tmp_reg_with_init(1);
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Extract V flag
	comp_macroblock_push_arithmetic_left_shift_extract_v_flag(
			dest_tmpreg->reg_usage_mapping | src_tmpreg->reg_usage_mapping,
			dest_tmpreg->mapped_reg_num,
			src_tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num);

	comp_free_temp_register(src_tmpreg);
	comp_free_temp_register(tmpreg);

	//Prepare the source for arithmetic operation
	helper_pre_word_no_alloc(dest_tmpreg->reg_usage_mapping, dest_tmpreg->mapped_reg_num);

	//Shifting to the left
	comp_macroblock_push_rotate_and_mask_bits(
			dest_tmpreg->reg_usage_mapping,
			dest_tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			dest_tmpreg->mapped_reg_num,
			dest_tmpreg->mapped_reg_num,
			1, 0, 15 - 1, TRUE);

	//Copy result to the destination register
	helper_post_word_no_free(dest_tmpreg->reg_usage_mapping, dest_tmpreg);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			dest_tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			dest_tmpreg, 2);
}
void comp_opcode_ASRIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, src_immediate - 1);

	if (size == 4)
	{
		//Arithmetic shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register_imm(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate, TRUE);
	} else {
		//Get temporary register for the operations
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		//Sign-extend the register to longword
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word_extended(
					output_dep,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					dest_reg->mapped_reg_num,
					FALSE);
		} else {
			comp_macroblock_push_copy_register_byte_extended(
					output_dep,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					dest_reg->mapped_reg_num,
					FALSE);
		}

		//Arithmetic shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register_imm(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate, TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		}

		comp_free_temp_register(tmpreg);
	}

	helper_check_nz_flags();
}
void comp_opcode_ASRREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();
	int modulo = (8  * size) - 1;

	//Modulo for shift register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			modulo);

	//Subtract shift register from 32 to reverse direction of shifting for the C flag extraction
	comp_macroblock_push_sub_register_from_immediate(
			shiftreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			shiftreg->mapped_reg_num,
			32);

	//Extract C and X flag, always clear V
	helper_extract_cx_clear_v_flags(output_dep | tmpreg->reg_usage_mapping, dest_reg, tmpreg, FALSE);

	comp_free_temp_register(tmpreg);

	if (size == 4)
	{
		//Shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register_register(
				output_dep | shiftreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Shifting to the right
		comp_macroblock_push_arithmetic_shift_right_register_register(
				tmpreg->reg_usage_mapping | shiftreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				FALSE);

		//Copy result to the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg);
		    comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg);
		    comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	//Free shift reg
	helper_free_tmp_reg(shiftreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ASRMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	helper_extract_cx_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, 1 - 1);

	//Sign-extend the register to longword
	comp_macroblock_push_copy_register_word_extended(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			FALSE);

	//Arithmetic shifting to the right
	comp_macroblock_push_arithmetic_shift_right_register_imm(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			1, TRUE);

	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_LSLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C and X flag
	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, (size * 8) - src_immediate);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate,
				0, 31 - src_immediate, TRUE);
	} else {
		comp_tmp_reg* tmpreg;

		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate,
				0,
				size == 2 ? 15 - src_immediate : 7 - src_immediate,
				TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg);
		}
	}

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_LSLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();
	int modulo = (8  * size) - 1;

	//Modulo for shift register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			modulo);

	if (size == 4)
	{
		//Extract C and X flag, clear V flag
		helper_extract_cx_clear_v_flags(
				shiftreg->reg_usage_mapping | output_dep,
				dest_reg,
				shiftreg,
				FALSE);

		//Shifting to the left
		comp_macroblock_push_logic_shift_left_register_register(
				shiftreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num, TRUE);
	} else {
		comp_tmp_reg* tmpreg;

		if (size == 2)
		{
			tmpreg = helper_pre_word(output_dep, dest_reg);
		} else {
			tmpreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Extract C and X flag, clear V flag
		helper_extract_cx_clear_v_flags(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg,
				shiftreg,
				TRUE);

		//Shifting to the left
		comp_macroblock_push_logic_shift_left_register_register(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num, FALSE);

		if (size == 2)
		{
			helper_post_word(output_dep, tmpreg, dest_reg);
		    comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			helper_post_byte(output_dep, tmpreg, dest_reg);
		    comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	comp_free_temp_register(shiftreg);

	//Check N and Z flags
	helper_check_nz_flags();
}
void comp_opcode_LSLMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	//Extract C and X flag
	helper_extract_cx_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, (2 * 8) - 1);

	//Prepare the source for arithmetic operation
	helper_pre_word_no_alloc(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num);

	//Shifting to the left
	comp_macroblock_push_rotate_and_mask_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			1, 0, 15 - 1,
			TRUE);

	//Copy result to the destination register
	helper_post_word_no_free(tmpreg->reg_usage_mapping, tmpreg);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_LSRIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, src_immediate - 1);

	if (size == 4)
	{
		//Simulate SRWI by using RLWINM
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - src_immediate, src_immediate, 31, TRUE);
	} else {
		//Get temporary register for the operations
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		//Simulate SRWI by using RLWINM
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - src_immediate, (32 - size * 8) + src_immediate, 31, TRUE);

		//Copy result to the destination register
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		}

		comp_free_temp_register(tmpreg);
	}

	helper_check_nz_flags();
}
void comp_opcode_LSRREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();
	int modulo = (8  * size) - 1;
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Modulo for shift register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			modulo);

	//Subtract shift register from 32 to reverse direction of shifting for the C flag extraction
	comp_macroblock_push_sub_register_from_immediate(
			shiftreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			shiftreg->mapped_reg_num,
			32);

	if (size == 4)
	{
		//Extract C and X flag, clear V flag
		helper_extract_cx_clear_v_flags(
				tmpreg->reg_usage_mapping | output_dep,
				dest_reg,
				tmpreg,
				FALSE);

		//Shifting to the right
		comp_macroblock_push_logic_shift_right_register_register(
				shiftreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num, TRUE);
	} else {
		//Extract C and X flag, clear V flag
		helper_extract_cx_clear_v_flags(
				tmpreg->reg_usage_mapping | output_dep,
				dest_reg,
				tmpreg,
				FALSE);

		//TODO: it would be nice to get rid of this masking
		//Clear the rest of the longword
		comp_macroblock_push_and_low_register_imm(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				size == 2 ? 0xffff : 0x00ff);

		//Shifting to the right
		comp_macroblock_push_logic_shift_right_register_register(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num, FALSE);

		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		    comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		    comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	helper_free_tmp_reg(tmpreg);
	comp_free_temp_register(shiftreg);

	//Check N and Z flags
	helper_check_nz_flags();
}
void comp_opcode_LSRMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	helper_extract_cx_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, 1 - 1);

	//Simulate SRWI by using RLWINM
	comp_macroblock_push_rotate_and_mask_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			32 - 1, (32 - 2 * 8) + 1, 31, TRUE);

	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_ROLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C, clear V flag
	helper_extract_c_clear_nzv_flags(output_dep, dest_reg, (size * 8) - src_immediate);

	if (size == 4)
	{
		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate,
				0, 31, TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_prepare_word_shift(output_dep, dest_reg);
		} else {
			tmpreg = helper_prepare_byte_shift_left(output_dep, dest_reg);
		}

		//Shifting to the left
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate,
				size == 2 ? 16 : 24, 31, FALSE);

		//Copy result to the destination register
		if (size == 2)
		{
		    comp_macroblock_push_check_word_register(
		    		output_dep,
		    		dest_reg->mapped_reg_num);
		} else {
		    comp_macroblock_push_check_byte_register(
		    		output_dep,
		    		dest_reg->mapped_reg_num);
		}

		comp_free_temp_register(tmpreg);
	}

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ROLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	if (size == 4)
	{
		//Extract C flag, clear V flag
		helper_extract_c_clear_v_flags(
				input_dep | output_dep,
				dest_reg,
				src_reg,
				TRUE);

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits_register(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				0, 31, TRUE);
	} else {
		comp_tmp_reg* tmpreg;

		if (size == 2)
		{
			tmpreg = helper_prepare_word_shift(output_dep, dest_reg);
		} else {
			tmpreg = helper_prepare_byte_shift_left(output_dep, dest_reg);
		}

		comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();

		//Modulo for shift register
		comp_macroblock_push_and_low_register_imm(
				input_dep,
				shiftreg->reg_usage_mapping,
				shiftreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				size == 2 ? 15 : 7);

		//Extract C flag, clear V flag
		helper_extract_c_clear_v_flags(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg,
				shiftreg,
				TRUE);

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits_register(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				0, 31, FALSE);

		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		    comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
		    comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}

		comp_free_temp_register(tmpreg);
		comp_free_temp_register(shiftreg);
	}

	//Check N and Z flags
	helper_check_nz_flags();
}
void comp_opcode_ROLMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	//Extract C, clear V flag
	helper_extract_c_clear_nzvx_flags(tmpreg->reg_usage_mapping, tmpreg, (2 * 8) - 1);

	helper_prepare_word_shift_no_alloc(tmpreg->reg_usage_mapping, tmpreg);

	//Shifting to the left
	comp_macroblock_push_rotate_and_mask_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			1,
			16, 31, FALSE);

	//Copy result to the destination register
	comp_macroblock_push_check_word_register(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_ROXLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Extract C, clear V flag
	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, (size * 8) - src_immediate);

	if (size == 4)
	{
		//Copy original destination register to a temp register for later use
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 16 bit: shift 1 bit to the right
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - 1, 0, 15, FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | xflagreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 6, 0, 0, FALSE);

		//Rotate the saved original register
		comp_macroblock_push_rotate_and_mask_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate, 0, 31, FALSE);

		//Rotate the prepared register
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate, 0, 31, FALSE);

		//Put the two half longword together
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				0, 0, 15, TRUE);
	} else {
		//Copy original destination register to a temp register
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 8/16 bit: copy lower word to the top, but 1 bit to the right
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				(size == 2 ? 16 : 24) - 1,
				0,
				(size == 2 ? 15 : 7), FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 6, 0, 0, FALSE);

		//Rotate the prepared register and insert it into the destination
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_immediate,
				(size == 2 ? 16 : 24), 31, FALSE);

		//Check flags on result
		if (size == 2)
		{
			comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	//Free temp registers
	comp_free_temp_register(tmpreg);
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ROXLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Clear V flag
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Copy X flag to C
	//This is needed only if the shift register is zero, the situation is handled by the next extractor function
	helper_copy_x_flag_to_c_flag();

	if (size == 4)
	{
		//Extract C and X flag (implemented as a macroblock for simplicity)
		//C flag is required from the previous step, otherwise that step would be optimized away
		comp_macroblock_push_shift_extract_cx_flag(
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | input_dep | output_dep,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				TRUE);

		//Copy original destination register to a temp register for later use
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 16 bit: shift 1 bit to the right
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - 1, 0, 15, FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | xflagreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 6, 0, 0, FALSE);

		//Rotate the saved original register
		comp_macroblock_push_rotate_and_mask_bits_register(
				input_dep | tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				0, 31, FALSE);

		//Rotate the prepared register
		comp_macroblock_push_rotate_and_mask_bits_register(
				input_dep | output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				0, 31, FALSE);

		//Put the two half longword together
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				0, 0, 15, TRUE);
	} else {
		comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();

		//Modulo for shift register
		comp_macroblock_push_and_low_register_imm(
				input_dep,
				shiftreg->reg_usage_mapping,
				shiftreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				size == 2 ? 15 : 7);

		//Extract C and X flag (implemented as a macroblock for simplicity)
		//C flag is required from the previous step, otherwise that step would be optimized away
		comp_macroblock_push_shift_extract_cx_flag(
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | shiftreg->reg_usage_mapping | output_dep,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				TRUE);

		//Copy original destination register to a temp register
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 8/16 bit: copy lower word to the top, but 1 bit to the right
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				(size == 2 ? 16 : 24) - 1,
				0,
				(size == 2 ? 15 : 7), FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 6, 0, 0, FALSE);

		//Rotate the prepared register
		comp_macroblock_push_rotate_and_mask_bits_register(
				tmpreg->reg_usage_mapping | shiftreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				(size == 2 ? 16 : 24), 31, FALSE);

		helper_free_tmp_reg(shiftreg);

		//Check flags on result
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	//Free temp registers
	comp_free_temp_register(tmpreg);
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ROXLMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Extract C, clear V flag
	helper_extract_cx_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, (2 * 8) - 1);

	//Prepare higher 8/16 bit: copy lower word to the top, but 1 bit to the right
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			16 - 1, 0, 15, FALSE);

	//Insert X flag to the highest bit into the prepared register
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			xflagreg->mapped_reg_num,
			32 - 6, 0, 0, FALSE);

	//Rotate the prepared register
	comp_macroblock_push_rotate_and_mask_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			1,
			16, 31, FALSE);

	//Check flags on result
	comp_macroblock_push_check_word_register(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num);

	//Free temp register
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_RORIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg;

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Extract C, clear V flag
	helper_extract_c_clear_nzv_flags(output_dep, dest_reg, src_immediate - 1);

	if (size == 4)
	{
		//Shifting to the right
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - src_immediate,
				0, 31, TRUE);
	} else {
		//Prepare the source for arithmetic operation
		if (size == 2)
		{
			tmpreg = helper_prepare_word_shift(output_dep, dest_reg);
		} else {
			tmpreg = helper_prepare_byte_shift_right(output_dep, dest_reg);
		}

		//Shifting to the left
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				32 - src_immediate,
				size == 2 ? 16 : 24, 31, FALSE);

		//Check result in destination register
		if (size == 2)
		{
		    comp_macroblock_push_check_word_register(
		    		output_dep,
		    		dest_reg->mapped_reg_num);
		} else {
		    comp_macroblock_push_check_byte_register(
		    		output_dep,
		    		dest_reg->mapped_reg_num);
		}

		comp_free_temp_register(tmpreg);
	}

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_RORREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();
	int modulo = (8  * size) - 1;

	//Modulo for shift register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			modulo);

	//Subtract shift register from 32 to reverse direction of shifting
	comp_macroblock_push_sub_register_from_immediate(
			shiftreg->reg_usage_mapping,
			shiftreg->reg_usage_mapping,
			shiftreg->mapped_reg_num,
			shiftreg->mapped_reg_num,
			32);

	if (size == 4)
	{
		//Extract C flag, clear V flag
		helper_extract_c_clear_v_flags(
				shiftreg->reg_usage_mapping | output_dep,
				dest_reg,
				shiftreg,
				FALSE);

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits_register(
				shiftreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				0, 31, TRUE);
	} else {
		comp_tmp_reg* tmpreg;

		if (size == 2)
		{
			tmpreg = helper_prepare_word_shift(output_dep, dest_reg);
		} else {
			tmpreg = helper_prepare_byte_shift_right(output_dep, dest_reg);
		}

		//Extract C flag, clear V flag
		helper_extract_c_clear_v_flags(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				tmpreg,
				shiftreg,
				FALSE);

		//Shifting to the left
		comp_macroblock_push_rotate_and_mask_bits_register(
				shiftreg->reg_usage_mapping | tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				size == 2 ? 16 : 24, 31, FALSE);

		//Check result in destination register
		if (size == 2)
		{
		    comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
		    comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	comp_free_temp_register(shiftreg);

	//Check N and Z flags
	helper_check_nz_flags();
}
void comp_opcode_RORMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	//Extract C, clear V flag
	helper_extract_c_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, 1 - 1);

	//Prepare the source for arithmetic operation
	helper_prepare_word_shift_no_alloc(tmpreg->reg_usage_mapping, tmpreg);

	//Shifting to the left
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			32 - 1,
			16, 31, FALSE);

	//Copy result to the destination register
	comp_macroblock_push_check_word_register(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_ROXRIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();

	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Extract C and X, clear V flag
	helper_extract_cx_clear_nzv_flags(output_dep, dest_reg, src_immediate - 1);

	if (size == 4)
	{
		//Copy original destination register to a temp register for later use
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare lower 16 bit: shift 1 bit to the left
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1, 16, 31, FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | xflagreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 5, 31, 31, FALSE);

		//Rotate the saved original register
		comp_macroblock_push_rotate_and_mask_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				32 - src_immediate, 0, 31, FALSE);

		//Rotate the prepared register
		comp_macroblock_push_rotate_and_mask_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				32 - src_immediate, 0, 31, FALSE);

		//Put the two half longword together
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				0, 16, 31, TRUE);
	} else {
		//Copy original destination register to a temp register
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 8/16 bit: copy lower word to the top, but 1 bit to the left
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				(size == 2 ? 16 : 8) + 1,
				0,
				(size == 2 ? 15 : 23), FALSE);

		//Insert X flag to the highest bit into the prepared register
		if (size == 2)
		{
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					xflagreg->mapped_reg_num,
					11, 15, 15, FALSE);
		} else {
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					xflagreg->mapped_reg_num,
					3, 23, 23, FALSE);
		}

		//Rotate the prepared register and insert it into the destination
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				32 - src_immediate,
				(size == 2 ? 16 : 24), 31, FALSE);

		//Check flags on result
		if (size == 2)
		{
			comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	//Free temp registers
	comp_free_temp_register(tmpreg);
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ROXRREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();
	comp_tmp_reg* shiftreg = helper_allocate_tmp_reg();

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Clear V flag
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Copy X flag to C
	//This is needed only if the shift register is zero, the situation is handled by the next extractor function
	helper_copy_x_flag_to_c_flag();

	if (size == 4)
	{
		//Subtract shift register from 32 to reverse direction of shifting for the C flag extraction
		comp_macroblock_push_sub_register_from_immediate(
				input_dep,
				shiftreg->reg_usage_mapping,
				shiftreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				32);

		//Extract C and X flag (implemented as a macroblock for simplicity)
		//C flag is required from the previous step, otherwise that step would be optimized away
		comp_macroblock_push_shift_extract_cx_flag(
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | shiftreg->reg_usage_mapping | output_dep,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				FALSE);

		//Copy original destination register to a temp register for later use
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare lower 16 bit: shift 1 bit to the left
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1, 16, 31, FALSE);

		//Insert X flag to the highest bit into the prepared register
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | xflagreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				xflagreg->mapped_reg_num,
				32 - 5, 31, 31, FALSE);

		//Rotate the saved original register
		comp_macroblock_push_rotate_and_mask_bits_register(
				tmpreg->reg_usage_mapping | shiftreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num, 0, 31, FALSE);

		//Rotate the prepared register
		comp_macroblock_push_rotate_and_mask_bits_register(
				output_dep | shiftreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				shiftreg->mapped_reg_num, 0, 31, FALSE);

		//Put the two half longword together
		comp_macroblock_push_rotate_and_copy_bits(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN  | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				0, 16, 31, TRUE);
	} else {
		//Modulo for shift register
		comp_macroblock_push_and_low_register_imm(
				input_dep,
				shiftreg->reg_usage_mapping,
				shiftreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				size == 2 ? 15 : 7);

		//Subtract shift register from 32 to reverse direction of shifting for the C flag extraction
		comp_macroblock_push_sub_register_from_immediate(
				shiftreg->reg_usage_mapping,
				shiftreg->reg_usage_mapping,
				shiftreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				32);

		//Copy original destination register to a temp register
		comp_macroblock_push_copy_register_long(
				output_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		//Prepare higher 8/16 bit: copy lower word to the top, but 1 bit to the left
		comp_macroblock_push_rotate_and_copy_bits(
				tmpreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				(size == 2 ? 16 : 8) + 1,
				0,
				(size == 2 ? 15 : 23), FALSE);

		//Insert X flag to the highest bit into the prepared register
		if (size == 2)
		{
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					xflagreg->mapped_reg_num,
					11, 15, 15, FALSE);
		} else {
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					xflagreg->mapped_reg_num,
					3, 23, 23, FALSE);
		}

		//Rotate the prepared register and insert it into the destination
		comp_macroblock_push_rotate_and_mask_bits_register(
				output_dep | tmpreg->reg_usage_mapping | shiftreg->reg_usage_mapping,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				tmpreg->mapped_reg_num,
				shiftreg->mapped_reg_num,
				(size == 2 ? 16 : 24), 31, FALSE);

		//Check flags on result
		if (size == 2)
		{
			comp_macroblock_push_copy_register_word(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_check_word_register(output_dep, dest_reg->mapped_reg_num);
		} else {
			comp_macroblock_push_copy_register_byte(
					tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_check_byte_register(output_dep, dest_reg->mapped_reg_num);
		}
	}

	//Free temp registers
	comp_free_temp_register(tmpreg);
	comp_free_temp_register(shiftreg);
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();
}
void comp_opcode_ROXRMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* tmpreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			2,
			TRUE);

	comp_tmp_reg* xflagreg = helper_allocate_tmp_reg();

	//Save X flag
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			xflagreg->reg_usage_mapping,
			xflagreg->mapped_reg_num,
			PPCR_FLAGS_MAPPED);

	//Extract C and X, clear V flag
	helper_extract_cx_clear_nzv_flags(tmpreg->reg_usage_mapping, tmpreg, 1 - 1);

	//Prepare higher 16 bit: copy lower word to the top, but 1 bit to the left
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			16 + 1, 0, 15, FALSE);

	//Insert X flag to the highest bit into the prepared register
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping | xflagreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			xflagreg->mapped_reg_num,
			11, 15, 15, FALSE);

	//Rotate the prepared register
	comp_macroblock_push_rotate_and_mask_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			32 - 1, 16, 31, FALSE);

	//Check flags on result
	comp_macroblock_push_check_word_register(tmpreg->reg_usage_mapping, tmpreg->mapped_reg_num);

	//Free temp register
	comp_free_temp_register(xflagreg);

	//Check flags
	helper_check_nz_flags();

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tmpreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tmpreg, 2);
}
void comp_opcode_CMPMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* desttempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, FALSE);

	if (size == 4)
	{
		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | tempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		helper_free_tmp_reg(desttempreg);
	}

	//Free temp register
	helper_free_tmp_reg(tempreg);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);
}
void comp_opcode_CMPREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* srctempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				input_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				PPCR_SPECTMP_MAPPED,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				srctempreg->reg_usage_mapping | desttempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				PPCR_SPECTMP_MAPPED,
				srctempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		helper_free_tmp_reg(srctempreg);
		helper_free_tmp_reg(desttempreg);
	}

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);
}
void comp_opcode_CMPMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* desttempreg;

	//!TODO: copying to the non-volatile register is not required if the memory access is non-special (temp regs are preserved), but we don't know that at this stage
	//Save non-volatile register temporarily
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);

	//Read memory from source
	comp_tmp_reg* tempreg_src = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			TRUE);

	//Save source data to non-volatile register
	comp_macroblock_push_copy_register_long(
			tempreg_src->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			tempreg_src->mapped_reg_num);

	//Free source register
	helper_free_tmp_reg(tempreg_src);

	//Read memory from destination
	comp_tmp_reg* tempreg_dest = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			FALSE);

	if (size == 4)
	{
		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				PPCR_TMP_NONVOL0_MAPPED,
				PPCR_TMP_NONVOL0_MAPPED,
				tempreg_dest->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_word(tempreg_dest->reg_usage_mapping, tempreg_dest);
		} else {
			helper_pre_byte_no_alloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_byte(tempreg_dest->reg_usage_mapping, tempreg_dest);
		}

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				PPCR_TMP_NONVOL0_MAPPED,
				PPCR_TMP_NONVOL0_MAPPED,
				desttempreg->mapped_reg_num);

		helper_free_tmp_reg(desttempreg);
	}

	//Free destination register
	helper_free_tmp_reg(tempreg_dest);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);

	//Restore non-volatile register
	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);
}
void comp_opcode_CMPAMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, FALSE);

	if (size == 4)
	{
		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | tempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Address registers are always longword sized: sign extend source data
		comp_macroblock_push_copy_register_word_extended(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				FALSE);

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | tempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	}

	//Free temp register
	helper_free_tmp_reg(tempreg);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);
}
void comp_opcode_CMPAREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* srctempreg = helper_allocate_tmp_reg();

	//Address registers are always longword sized: sign extend source data
	comp_macroblock_push_copy_register_word_extended(
			input_dep,
			srctempreg->reg_usage_mapping,
			srctempreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			FALSE);

	//Compile SUBFCO PPC opcode
	comp_macroblock_push_sub_with_flags(
			srctempreg->reg_usage_mapping | output_dep,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
			PPCR_SPECTMP_MAPPED,
			srctempreg->mapped_reg_num,
			dest_reg->mapped_reg_num);

	helper_free_tmp_reg(srctempreg);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);
}
void comp_opcode_CMPIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* immtempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | immtempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		helper_free_tmp_reg(desttempreg);
	}

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);

	helper_free_tmp_reg(immtempreg);
}
void comp_opcode_CMPAIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* immtempreg = helper_allocate_tmp_reg();

	//Load the immediate value to the temp register
	//sign-extended to longword size
	comp_macroblock_push_load_register_word_extended(
			immtempreg->reg_usage_mapping,
			immtempreg->mapped_reg_num,
			src_immediate);

	//Do longword compare: compile SUBFCO PPC opcode
	comp_macroblock_push_sub_with_flags(
			output_dep | immtempreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
			immtempreg->mapped_reg_num,
			immtempreg->mapped_reg_num,
			dest_reg->mapped_reg_num);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);

	helper_free_tmp_reg(immtempreg);
}
void comp_opcode_CMPIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			FALSE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				tempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		//Compile SUBFCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				immtempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				tempreg->mapped_reg_num);
	}

	//Free temp regs
	helper_free_tmp_reg(immtempreg);
	helper_free_tmp_reg(tempreg);

	//Save flags: NZVC
	helper_check_nzcv_flags(TRUE);
}
void comp_opcode_ADDREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* srctempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	if (size == 4)
	{
		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				tempreg->reg_usage_mapping | srctempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				srctempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word_no_free(tempreg->reg_usage_mapping, tempreg);
		} else {
			helper_post_byte_no_free(tempreg->reg_usage_mapping, tempreg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ADDMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* desttempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				tempreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				desttempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				tempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_ADDREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* srctempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				desttempreg->reg_usage_mapping | srctempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);
}
void comp_opcode_ADDIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word_no_free(tempreg->reg_usage_mapping, tempreg);
		} else {
			helper_post_byte_no_free(tempreg->reg_usage_mapping, tempreg);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);

	helper_free_tmp_reg(immtempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_ADDIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* immtempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				output_dep | immtempreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				immtempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile ADDCO PPC opcode
		comp_macroblock_push_add_with_flags(
				desttempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				immtempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(FALSE);

	helper_free_tmp_reg(immtempreg);
}
void comp_opcode_ADDQ2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//ADDQ is just a special case of ADDI
	comp_opcode_ADDIMM2REG(history, props);
}
void comp_opcode_ADDAQ2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	comp_macroblock_push_add_register_imm(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			src_immediate);
}
void comp_opcode_ADDQ2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//ADDQ is just a special case of ADDI
	comp_opcode_ADDIMM2MEM(history, props);
}
void comp_opcode_ADDAMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		//Add the output to the target register
		comp_macroblock_push_add(
				output_dep | tempreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num);
	} else {
		//Sign-extend source data from word to longword
		//(address registers are woring with longwords)
		comp_macroblock_push_copy_register_word_extended(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				FALSE);

		//Add the output to the target register
		comp_macroblock_push_add(
				output_dep | tempreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num);
	}

	//Free temp register
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_ADDAREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (props->size == 4)
	{
		comp_macroblock_push_add(
				output_dep | input_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);
	} else {
		comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

		comp_macroblock_push_copy_register_word_extended(
				input_dep,
				tmpreg->reg_usage_mapping,
				tmpreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);

		comp_macroblock_push_add(
				output_dep | tmpreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				tmpreg->mapped_reg_num);

		comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_ADDAIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//If the operation size is word, or the immediate fits into a word then this is a word operation
	if ((props->size == 2) || ((props->size == 4) && ((src_immediate < 32678) && (src_immediate >= -32768))))
	{
		comp_macroblock_push_add_register_imm(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_immediate);
	} else {
			comp_tmp_reg* tmpreg = helper_allocate_tmp_reg_with_init(src_immediate);

			comp_macroblock_push_add(
					output_dep | tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);

			comp_free_temp_register(tmpreg);
	}
}
void comp_opcode_ADDXREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* srctempreg;
	comp_tmp_reg* desttempreg;

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(FALSE);

	if (size == 4)
	{
		//Compile ADDEO PPC opcode
		comp_macroblock_push_add_extended_with_flags(
				input_dep | output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word_filled(input_dep, src_reg);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			srctempreg = helper_pre_byte_filled(input_dep, src_reg);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile ADDEO PPC opcode
		comp_macroblock_push_add_extended_with_flags(
				desttempreg->reg_usage_mapping | srctempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(FALSE);
}
void comp_opcode_ADDXMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* desttempreg;

	//!TODO: copying to the non-volatile register is not required if the memory access is non-special (temp regs are preserved), but we don't know that at this stage
	//Save non-volatile register temporarily
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);

	//Read memory from source
	comp_tmp_reg* tempreg_src = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			TRUE);

	//Save source data to non-volatile register
	comp_macroblock_push_copy_register_long(
			tempreg_src->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			tempreg_src->mapped_reg_num);

	//Free source register
	helper_free_tmp_reg(tempreg_src);

	//Read memory from destination
	comp_tmp_reg* tempreg_dest = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(FALSE);

	if (size == 4)
	{
		//Compile ADDEO PPC opcode
		comp_macroblock_push_add_extended_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0 |tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg_dest->mapped_reg_num,
				tempreg_dest->mapped_reg_num,
				PPCR_TMP_NONVOL0_MAPPED);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_filled_noalloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_word(tempreg_dest->reg_usage_mapping, tempreg_dest);
		} else {
			helper_pre_byte_filled_noalloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_byte(tempreg_dest->reg_usage_mapping, tempreg_dest);
		}

		//Compile ADDEO PPC opcode
		comp_macroblock_push_add_extended_with_flags(
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				PPCR_TMP_NONVOL0_MAPPED);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		} else {
			helper_post_byte(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		}
	}

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg_dest->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg_dest, size);

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(FALSE);

	//Restore non-volatile register
	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);
}
void comp_opcode_SUBREG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* srctempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Re-map source register
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, FALSE);

	if (size == 4)
	{
		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				input_dep | tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				tempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | srctempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				srctempreg->mapped_reg_num,
				tempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word_no_free(tempreg->reg_usage_mapping, tempreg);
		} else {
			helper_post_byte_no_free(tempreg->reg_usage_mapping, tempreg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(TRUE);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_SUBMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* desttempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | tempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(TRUE);

	helper_free_tmp_reg(tempreg);
}
void comp_opcode_SUBREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	comp_tmp_reg* srctempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				input_dep | output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | srctempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_nzcvx_flags(TRUE);
}
void comp_opcode_SUBIMM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* immtempreg;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				tempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				tempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				tempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word_no_free(tempreg->reg_usage_mapping, tempreg);
		} else {
			helper_post_byte_no_free(tempreg->reg_usage_mapping, tempreg);
		}
	}

	//Save flags
	helper_check_nzcvx_flags(TRUE);

	helper_free_tmp_reg(immtempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);
}
void comp_opcode_SUBIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* immtempreg;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		//Allocate temp register for the immediate
		immtempreg = helper_allocate_tmp_reg_with_init(src_immediate);

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				output_dep | immtempreg->reg_usage_mapping,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 16);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			immtempreg = helper_allocate_tmp_reg_with_init(src_immediate << 24);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBCO PPC opcode
		comp_macroblock_push_sub_with_flags(
				desttempreg->reg_usage_mapping | immtempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				immtempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	//Save flags (invert C flag)
	helper_check_nzcvx_flags(TRUE);

	helper_free_tmp_reg(immtempreg);
}
void comp_opcode_SUBQ2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//SUBQ is just a special case of SUBI
	comp_opcode_SUBIMM2REG(history, props);
}
void comp_opcode_SUBAQ2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	comp_macroblock_push_add_register_imm(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			-src_immediate);
}
void comp_opcode_SUBQ2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//SUBQ is just a special case of SUBI
	comp_opcode_SUBIMM2MEM(history, props);
}
void comp_opcode_SUBAMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, TRUE);

	if (size == 4)
	{
		//Subtract the output from the target register
		comp_macroblock_push_sub(
				output_dep | tempreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Sign-extend source data from word to longword
		//(address registers are woring with longwords)
		comp_macroblock_push_copy_register_word_extended(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				FALSE);

		//Subtract the output from the target register
		comp_macroblock_push_sub(
				output_dep | tempreg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	}

	//Free temp register
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_SUBAIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Fall back to ADDA with a negative immediate
	src_immediate = 0 - (props->size == 4 ? src_immediate : ((unsigned short)src_immediate));
	comp_opcode_ADDAIMM2REG(history, props);
}
void comp_opcode_SUBAREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (props->size == 4)
	{
		if (src_reg->mapped_reg_num.r == dest_reg->mapped_reg_num.r)
		{
			//The source and the destination register is the same: clear register instead of subtracting
			//Clearing needs no preloading of the emulated register
			comp_macroblock_push_load_register_long(
					output_dep,
					dest_reg->mapped_reg_num,
					0);
		} else {
			//Subtract the registers, this is so simple
			comp_macroblock_push_sub(
					input_dep | output_dep,
					output_dep,
					dest_reg->mapped_reg_num,
					src_reg->mapped_reg_num,
					dest_reg->mapped_reg_num);
		}
	} else {
		comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

		//Extend the source register to long into a temp register
		comp_macroblock_push_copy_register_word_extended(
				input_dep,
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num,
				src_reg->mapped_reg_num,
				FALSE);

		//Subtract the registers
		comp_macroblock_push_sub(
				tempreg->reg_usage_mapping | output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				tempreg->mapped_reg_num,
				dest_reg->mapped_reg_num);

		comp_free_temp_register(tempreg);
	}
}
void comp_opcode_SUBXREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* srctempreg;
	comp_tmp_reg* desttempreg;

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(TRUE);

	if (size == 4)
	{
		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				input_dep | output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				src_reg->mapped_reg_num,
				dest_reg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			srctempreg = helper_pre_word(input_dep, src_reg);
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			srctempreg = helper_pre_byte(input_dep, src_reg);
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				desttempreg->reg_usage_mapping | srctempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num,
				desttempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}

		helper_free_tmp_reg(srctempreg);
	}

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(TRUE);
}
void comp_opcode_SUBXMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* desttempreg;

	//!TODO: copying to the non-volatile register is not required if the memory access is non-special (temp regs are preserved), but we don't know that at this stage
	//Save non-volatile register temporarily
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);

	//Read memory from source
	comp_tmp_reg* tempreg_src = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			TRUE);

	//Save source data to non-volatile register
	comp_macroblock_push_copy_register_long(
			tempreg_src->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			tempreg_src->mapped_reg_num);

	//Free source register
	helper_free_tmp_reg(tempreg_src);

	//Read memory from destination
	comp_tmp_reg* tempreg_dest = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(TRUE);

	if (size == 4)
	{
		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0 |tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg_dest->mapped_reg_num,
				PPCR_TMP_NONVOL0_MAPPED,
				tempreg_dest->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_word(tempreg_dest->reg_usage_mapping, tempreg_dest);
		} else {
			helper_pre_byte_no_alloc(COMP_COMPILER_MACROBLOCK_REG_NONVOL0, PPCR_TMP_NONVOL0_MAPPED);
			desttempreg = helper_pre_byte(tempreg_dest->reg_usage_mapping, tempreg_dest);
		}

		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				PPCR_TMP_NONVOL0_MAPPED,
				desttempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		} else {
			helper_post_byte(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		}
	}

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg_dest->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg_dest, size);

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(TRUE);

	//Restore non-volatile register
	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);
}
void comp_opcode_MULSIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load the source immediate into the spec temp register (word sign extended to long)
	comp_macroblock_push_load_register_word_extended(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			src_immediate);

	//Call generic multiply helper
	helper_MULS(COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC, PPCR_SPECTMP_MAPPED);
}
void comp_opcode_MULSREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Copy the low half word with sign extension to long from source register to spec temp reg (R0)
	comp_macroblock_push_copy_register_word_extended(
			input_dep,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			src_reg->mapped_reg_num,
			FALSE);

	//Call generic multiply helper
	helper_MULS(COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC, PPCR_SPECTMP_MAPPED);
}
void comp_opcode_MULSMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			FALSE);

	//!TODO: sign extension of the read data could be moved to the memory handler helper function
	//Sign extend source data to longword
	comp_macroblock_push_copy_register_word_extended(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	//Call generic multiply helper
	helper_MULS(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);

	//Release temporary register
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_MULUIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Make sure there is nothing left in the high word of the immediate
	src_immediate &= 0xffff;

	//Load the source immediate into the spec temp register
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			src_immediate);

	//Call generic multiply helper
	helper_MULU(COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC, PPCR_SPECTMP_MAPPED);
}
void comp_opcode_MULUREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Keep the low half word from source register
	comp_macroblock_push_and_low_register_imm(
			input_dep,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			src_reg->mapped_reg_num,
			0xffff);

	helper_MULU(COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC, PPCR_SPECTMP_MAPPED);
}
void comp_opcode_MULUMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	//Call generic multiply helper
	helper_MULU(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);

	//Release temporary register
	helper_free_tmp_reg(tempreg);
}
void comp_opcode_MULLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Load immediate into a register
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);
	input_dep |= local_src_reg->reg_usage_mapping;

	//Compile the multiplication
	helper_mull(history, local_src_reg, -1, TRUE);
}
void comp_opcode_MULLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Compile the multiplication
	helper_mull(history, src_reg, props->srcreg, FALSE);
}
void comp_opcode_MULLMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* local_src_reg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			4,
			FALSE);
	input_dep |= local_src_reg->reg_usage_mapping;

	//Compile the multiplication
	helper_mull(history, local_src_reg, -1, TRUE);
}
void comp_opcode_DIVSIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//TODO: this implementation can be improved by detecting the zero immediate while compiling

	//Load immediate into a register
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//TODO: pre-loaded immediate needs no processing (e.g. masking), but the implementation is a generic macroblock
	comp_macroblock_push_division_32_16bit(
			local_src_reg->reg_usage_mapping | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			TRUE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);

	//Release temp register
	helper_free_tmp_reg(local_src_reg);
}
void comp_opcode_DIVSREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_division_32_16bit(
			input_dep | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			src_reg->mapped_reg_num,
			TRUE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);
}
void comp_opcode_DIVSMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* local_src_reg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	comp_macroblock_push_division_32_16bit(
			local_src_reg->reg_usage_mapping | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			TRUE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);

	//Release temp register
	helper_free_tmp_reg(local_src_reg);
}
void comp_opcode_DIVUIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//TODO: this implementation can be improved by detecting the zero immediate while compiling

	//Load immediate into a register
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//TODO: pre-loaded immediate needs no processing (e.g. masking), but the implementation is a generic macroblock
	comp_macroblock_push_division_32_16bit(
			local_src_reg->reg_usage_mapping | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			FALSE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);

	//Release temp register
	helper_free_tmp_reg(local_src_reg);
}
void comp_opcode_DIVUREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_division_32_16bit(
			input_dep | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			src_reg->mapped_reg_num,
			FALSE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);
}
void comp_opcode_DIVUMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* local_src_reg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			FALSE);

	//Re-map destination register
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), TRUE, TRUE);

	comp_macroblock_push_division_32_16bit(
			local_src_reg->reg_usage_mapping | output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			FALSE,
			COMP_GET_CURRENT_PC,
			(uae_u32)pc_ptr);

	//Release temp register
	helper_free_tmp_reg(local_src_reg);
}
void comp_opcode_DIVLIMM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//TODO: this implementation can be improved by detecting the zero immediate while compiling

	//Load immediate into a register
	comp_tmp_reg* local_src_reg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Compile the division
	helper_divl(history, local_src_reg, TRUE);
}
void comp_opcode_DIVLREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Compile the division
	helper_divl(history, src_reg, FALSE);
}
void comp_opcode_DIVLMEM2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from source
	comp_tmp_reg* local_src_reg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			4,
			FALSE);

	//Compile the division
	helper_divl(history, local_src_reg, TRUE);
}
void comp_opcode_NEGREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* desttempreg;

	if (size == 4)
	{
		comp_macroblock_push_negate_with_overflow(
				output_dep,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				1);
	} else {
		//Prepare input: shift up to the highest word/byte
		if (size == 2)
		{
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		comp_macroblock_push_negate_with_overflow(
				desttempreg->reg_usage_mapping,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				1);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	//Save flags: N, Z and V
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Copy Z to C
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			24, 10, 10, FALSE);

	//Invert C
	comp_macroblock_push_xor_high_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			1 << 5);

	//Copy C to X
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			16, 26, 26, FALSE);
}
void comp_opcode_NEGMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	if (size == 4)
	{
		comp_macroblock_push_negate_with_overflow(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				1);
	} else {
		//Prepare input: shift up to the highest word/byte
		if (size == 2)
		{
			helper_pre_word_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		} else {
			helper_pre_byte_no_alloc(tempreg->reg_usage_mapping, tempreg->mapped_reg_num);
		}

		comp_macroblock_push_negate_with_overflow(
				tempreg->reg_usage_mapping,
				tempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV,
				tempreg->mapped_reg_num,
				tempreg->mapped_reg_num,
				1);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word_no_free(tempreg->reg_usage_mapping, tempreg);
		} else {
			helper_post_byte_no_free(tempreg->reg_usage_mapping, tempreg);
		}
	}

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, size);

	//Save flags: N, Z and V
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Copy Z to C
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			24, 10, 10, FALSE);

	//Invert C
	comp_macroblock_push_xor_high_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			1 << 5);

	//Copy C to X
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			16, 26, 26, FALSE);
}
void comp_opcode_NEGXREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* srctempreg = helper_allocate_tmp_reg_with_init(0);
	comp_tmp_reg* desttempreg;

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(TRUE);

	if (size == 4)
	{
		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				srctempreg->reg_usage_mapping | output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				srctempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			desttempreg = helper_pre_word(output_dep, dest_reg);
		} else {
			desttempreg = helper_pre_byte(output_dep, dest_reg);
		}

		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				desttempreg->reg_usage_mapping | srctempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(output_dep, desttempreg, dest_reg);
		} else {
			helper_post_byte(output_dep, desttempreg, dest_reg);
		}
	}

	helper_free_tmp_reg(srctempreg);

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(TRUE);
}
void comp_opcode_NEGXMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;
	comp_tmp_reg* desttempreg;

	//Read memory from destination
	comp_tmp_reg* tempreg_dest = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			size,
			TRUE);

	comp_tmp_reg* srctempreg = helper_allocate_tmp_reg_with_init(0);

	//Copy the X register to the internal C flag
	helper_copy_x_flag_to_internal_c_flag(TRUE);

	if (size == 4)
	{
		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				srctempreg->reg_usage_mapping |tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				tempreg_dest->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				tempreg_dest->mapped_reg_num,
				tempreg_dest->mapped_reg_num,
				srctempreg->mapped_reg_num);
	} else {
		//Prepare inputs: shift up to the highest word/byte
		if (size == 2)
		{
			desttempreg = helper_pre_word(tempreg_dest->reg_usage_mapping, tempreg_dest);
		} else {
			desttempreg = helper_pre_byte(tempreg_dest->reg_usage_mapping, tempreg_dest);
		}

		//Compile SUBEO PPC opcode
		comp_macroblock_push_sub_extended_with_flags(
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
				desttempreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
				desttempreg->mapped_reg_num,
				desttempreg->mapped_reg_num,
				srctempreg->mapped_reg_num);

		//Shift result back to normal position and insert into the destination register
		if (size == 2)
		{
			helper_post_word(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		} else {
			helper_post_byte(tempreg_dest->reg_usage_mapping, desttempreg, tempreg_dest);
		}
	}

	helper_free_tmp_reg(srctempreg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg_dest->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg_dest, size);

	//Save flags
	helper_check_ncvx_flags_copy_z_flag_if_cleared(TRUE);
}
void comp_opcode_ABCDREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Get X flag into the register
	comp_tmp_reg* temp_reg = helper_copy_x_flag_to_register();

	//Sum decimal numbers and X flag, returns result in destination and C flag in the temp register
	comp_macroblock_push_add_decimal(
			input_dep | output_dep | temp_reg->reg_usage_mapping,
			output_dep | temp_reg->reg_usage_mapping,
			dest_reg->mapped_reg_num,
			src_reg->mapped_reg_num,
			temp_reg->mapped_reg_num);

	//Get the flags and free temporary register
	helper_extract_flags_for_decimal(temp_reg, dest_reg);
}
void comp_opcode_ABCDMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ABCD_SBCD_MEM(history, FALSE);
}
void comp_opcode_SBCDREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Get X flag into the register
	comp_tmp_reg* temp_reg = helper_copy_x_flag_to_register();

	//Subtract source decimal number and X flag from the destination, returns result in destination and C flag in the temp register
	comp_macroblock_push_sub_decimal(
			input_dep | output_dep | temp_reg->reg_usage_mapping,
			output_dep | temp_reg->reg_usage_mapping,
			dest_reg->mapped_reg_num,
			src_reg->mapped_reg_num,
			temp_reg->mapped_reg_num);

	//Get the flags and free temporary register
	helper_extract_flags_for_decimal(temp_reg, dest_reg);
}
void comp_opcode_SBCDMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_ABCD_SBCD_MEM(history, TRUE);
}
void comp_opcode_NBCDREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Get X flag into the register
	comp_tmp_reg* temp_reg = helper_copy_x_flag_to_register();

	//Subtract destination and the X flag from zero
	comp_macroblock_push_negate_decimal(
			output_dep | temp_reg->reg_usage_mapping,
			output_dep | temp_reg->reg_usage_mapping,
			dest_reg->mapped_reg_num,
			temp_reg->mapped_reg_num);

	//Get the flags and free temporary register
	helper_extract_flags_for_decimal(temp_reg, dest_reg);
}
void comp_opcode_NBCDMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read memory from destination
	comp_tmp_reg* local_dest_reg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Get X flag into the register
	comp_tmp_reg* temp_flagx_reg = helper_copy_x_flag_to_register();

	//Subtract destination and the X flag from zero
	comp_macroblock_push_negate_decimal(
			local_dest_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
			local_dest_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
			local_dest_reg->mapped_reg_num,
			temp_flagx_reg->mapped_reg_num);

	//Get the flags and free temporary register
	helper_extract_flags_for_decimal(temp_flagx_reg, local_dest_reg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			local_dest_reg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			local_dest_reg, 1);
}
void comp_opcode_PACKREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Read extension word
	uae_u16 extword = *(history->location + 1);

	//Add extension to the source register, result goes to the temp reg
	comp_macroblock_push_add_register_imm(
			input_dep,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			src_reg->mapped_reg_num,
			extword);

	//Shift higher 4 bit of the result byte to the right position
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			28, 24, 27, FALSE);

	//Insert result into the destination register
    comp_macroblock_push_copy_register_byte(
			tmpreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			tmpreg->mapped_reg_num);

    helper_free_tmp_reg(tmpreg);
}
void comp_opcode_PACKMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read extension word
	uae_u16 extword = *(history->location + 1);

	//Read source bytes from memory
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			2,
			TRUE);

	//Add extension to the source register, result goes to the temp reg
	comp_macroblock_push_add_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			extword);

	//Shift higher 4 bit of the result byte to the right position
	comp_macroblock_push_rotate_and_copy_bits(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			28, 24, 27, FALSE);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 1);
}
void comp_opcode_UNPKREG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read extension word
	uae_u16 extword = *(history->location + 1);

	comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

	//Copy source register into the temporary register
	comp_macroblock_push_copy_register_long(
			input_dep,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			src_reg->mapped_reg_num);

	//Shift higher 4 bit of the result byte to the right position
	comp_macroblock_push_rotate_and_copy_bits(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			4, 20, 23, FALSE);

	//Mask out empty two 4 bits
	comp_macroblock_push_and_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			0x0f0f);

	//Add extension to the result
	comp_macroblock_push_add_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			extword);

	//Insert result into the destination register
    comp_macroblock_push_copy_register_word(
			tempreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			tempreg->mapped_reg_num);

    helper_free_tmp_reg(tempreg);
}
void comp_opcode_UNPKMEM2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Read extension word
	uae_u16 extword = *(history->location + 1);

	//Read source bytes from memory
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			1,
			TRUE);

	//Shift higher 4 bit of the result byte to the right position
	comp_macroblock_push_rotate_and_copy_bits(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			4, 20, 23, FALSE);

	//Mask out empty two 4 bits
	comp_macroblock_push_and_low_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			0x0f0f);

	//Add extension to the result
	comp_macroblock_push_add_register_imm(
			tempreg->reg_usage_mapping,
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num,
			tempreg->mapped_reg_num,
			extword);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			tempreg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			tempreg, 2);
}
void comp_opcode_SWAP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_rotate_and_mask_bits(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			16, 0, 31, TRUE);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_EXGA(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate source register for modifying
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, TRUE);

	//Swap temporary register mapping to simulate the exchanging of the registers
	comp_swap_temp_register_mapping(
			src_reg,
			dest_reg);

	//Both source and destination registers are needed for the input and the output
	comp_macroblock_push_null_operation(
			input_dep | output_dep,
			input_dep | output_dep);
}
void comp_opcode_EXGD(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate source register for modifying
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), TRUE, TRUE);

	//Swap temporary register mapping to simulate the exchanging of the registers
	comp_swap_temp_register_mapping(
			src_reg,
			dest_reg);

	//Both source and destination registers are needed for the input and the output
	comp_macroblock_push_null_operation(
			input_dep | output_dep,
			input_dep | output_dep);
}
void comp_opcode_EXTB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (props->size == 4)
	{
		comp_macroblock_push_copy_register_byte_extended(
				output_dep,
				output_dep,
				dest_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);
	} else {
		comp_tmp_reg* tmp_reg = helper_allocate_tmp_reg();

		comp_macroblock_push_copy_register_byte_extended(
				output_dep,
				tmp_reg->reg_usage_mapping,
				tmp_reg->mapped_reg_num,
				dest_reg->mapped_reg_num,
				TRUE);

		comp_macroblock_push_copy_register_word(
				tmp_reg->reg_usage_mapping,
				output_dep,
				dest_reg->mapped_reg_num,
				tmp_reg->mapped_reg_num);

		helper_free_tmp_reg(tmp_reg);
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_EXTW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_macroblock_push_copy_register_word_extended(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			TRUE);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_NOP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Do nothing, nothing at all
}
void comp_opcode_TSTREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	switch (props->size)
	{
	case 4:
		comp_macroblock_push_copy_register_long_with_flags(
				input_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				src_reg->mapped_reg_num);
		break;
	case 2:
		comp_macroblock_push_check_word_register(
				input_dep,
				src_reg->mapped_reg_num);
		break;
	case 1:
		comp_macroblock_push_check_byte_register(
				input_dep,
				src_reg->mapped_reg_num);
		break;
	default:
		write_log("Error: wrong operation size for TSTREG\n");
		abort();
	}

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_TSTMEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	int size = props->size;

	//Read memory from source
	comp_tmp_reg* tempreg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			size,
			FALSE);

	switch (size)
	{
	case 4:
		comp_macroblock_push_copy_register_long_with_flags(
				tempreg->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				tempreg->mapped_reg_num);
		break;
	case 2:
		comp_macroblock_push_check_word_register(
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num);
		break;
	case 1:
		comp_macroblock_push_check_byte_register(
				tempreg->reg_usage_mapping,
				tempreg->mapped_reg_num);
		break;
	default:
		write_log("Error: wrong operation size for TSTMEM\n");
		abort();
	}

	//Free temp reg
	helper_free_tmp_reg(tempreg);

	//Save flags
	helper_check_nz_clear_cv_flags();
}
void comp_opcode_LINK(const cpu_history* history, struct comptbl* props) REGPARAM
{
	if (props->size == 2)
	{
		//Sign extend the word immediate to longword
		if ((src_immediate & 0x8000) != 0)
		{
			src_immediate = src_immediate | 0xffff0000;
		}
	}

	//Allocate temporary register for A7
	comp_tmp_reg* sptemp = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Subtract 4 from SP
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			sptemp->mapped_reg_num,
			sptemp->mapped_reg_num,
			-4);

	//Push input Ax register to the stack
	helper_write_memory(
			COMP_COMPILER_MACROBLOCK_REG_AX(7) | input_dep,
			history,
			sptemp,
			dest_reg,
			4);

	//Get the register mappings again, memory operation might free registers
	sptemp = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), FALSE, TRUE);

	//Copy SP to destination register
	comp_macroblock_push_copy_register_long(
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg) | COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
			dest_reg->mapped_reg_num,
			sptemp->mapped_reg_num);

	//Add immediate to the SP
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			sptemp->mapped_reg_num,
			sptemp->mapped_reg_num,
			src_immediate);
}
void comp_opcode_UNLINK(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Allocate temporary register for A7
	comp_tmp_reg* sptemp = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Copy source register to A7
	comp_macroblock_push_copy_register_long(
			input_dep,
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			sptemp->mapped_reg_num,
			src_reg->mapped_reg_num);

	//Read Ax from the top of the stack
	comp_tmp_reg* tmpreg = helper_read_memory(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			history,
			sptemp,
			4,
			FALSE);

	//Get the register mappings again, memory operation might free registers
	sptemp = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);
	comp_tmp_reg* src_tempreg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), FALSE, TRUE);

	//Copy read data into the source (destination) register
	comp_macroblock_push_copy_register_long(
			tmpreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
			src_tempreg->mapped_reg_num,
			tmpreg->mapped_reg_num);

	//Add 4 to SP
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			COMP_COMPILER_MACROBLOCK_REG_AX(7),
			sptemp->mapped_reg_num,
			sptemp->mapped_reg_num,
			4);

	//Free temp reg
	helper_free_tmp_reg(tmpreg);
}
void comp_opcode_BFCHG2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg;
	helper_bit_field_reg_opertion_flag_test(
			*((signed short*)(history->location + 1)),
			&mask_reg,
			NULL);

	//Exclusive or the mask to the target register
	comp_macroblock_push_xor_register_register(
			output_dep | mask_reg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Release mask register
	helper_free_tmp_reg(mask_reg);
}
void comp_opcode_BFCHG2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg_low = NULL;
	comp_tmp_reg* mask_reg_high = NULL;
	comp_tmp_reg* data_reg_low = NULL;
	comp_tmp_reg* data_reg_high = NULL;

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			*((signed short*)(history->location + 1)),
			NULL,
			&data_reg_high,
			&data_reg_low,
			&mask_reg_high,
			&mask_reg_low,
			NULL, NULL,
			TRUE, FALSE);

	//Exclusive or the mask to the target register on higher 32 bits
	comp_macroblock_push_xor_register_register(
			data_reg_high->reg_usage_mapping | mask_reg_high->reg_usage_mapping,
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			FALSE);

	//Exclusive or the mask to the target register on lower 8 bits
	comp_macroblock_push_xor_register_register(
			data_reg_low->reg_usage_mapping | mask_reg_low->reg_usage_mapping,
			data_reg_low->reg_usage_mapping,
			data_reg_low->mapped_reg_num,
			data_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			FALSE);

	//Release mask registers
	helper_free_tmp_reg(mask_reg_low);
	helper_free_tmp_reg(mask_reg_high);

	//Save result back to memory, also release temporary registers
	helper_bit_field_mem_save(history, data_reg_high, data_reg_low);
}
void comp_opcode_BFCLR2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg;
	helper_bit_field_reg_opertion_flag_test(
			*((signed short*)(history->location + 1)),
			&mask_reg,
			NULL);

	//And the complement of the mask to the target register
	comp_macroblock_push_and_register_complement_register(
			output_dep | mask_reg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Release mask register
	helper_free_tmp_reg(mask_reg);
}
void comp_opcode_BFCLR2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg_low = NULL;
	comp_tmp_reg* mask_reg_high = NULL;
	comp_tmp_reg* data_reg_low = NULL;
	comp_tmp_reg* data_reg_high = NULL;

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			*((signed short*)(history->location + 1)),
			NULL,
			&data_reg_high,
			&data_reg_low,
			&mask_reg_high,
			&mask_reg_low,
			NULL, NULL,
			TRUE, FALSE);

	//And the mask to the target register on higher 32 bits
	comp_macroblock_push_and_register_complement_register(
			data_reg_high->reg_usage_mapping | mask_reg_high->reg_usage_mapping,
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			FALSE);

	//And the mask to the target register on lower 8 bits
	comp_macroblock_push_and_register_complement_register(
			data_reg_low->reg_usage_mapping | mask_reg_low->reg_usage_mapping,
			data_reg_low->reg_usage_mapping,
			data_reg_low->mapped_reg_num,
			data_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			FALSE);

	//Release mask registers
	helper_free_tmp_reg(mask_reg_low);
	helper_free_tmp_reg(mask_reg_high);

	//Save result back to memory, also release temporary registers
	helper_bit_field_mem_save(history, data_reg_high, data_reg_low);
}
void comp_opcode_BFEXTS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* offset_reg;
	comp_tmp_reg* mask_reg;
	uae_u64 dest_reg_dep = 0ULL;

	signed int extword = *((signed short*)(history->location + 1));

	helper_bit_field_reg_opertion_flag_test(
			*((signed short*)(history->location + 1)),
			&mask_reg,
			&offset_reg);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//And the mask to the target register
	comp_macroblock_push_and_register_register(
			output_dep | mask_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Set the rest of the bits according to the N flag
	comp_macroblock_push_or_negative_mask_if_n_flag_set(
			dest_reg_dep | mask_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num);

	//Rotate result to the bottom of the register using the offset
	comp_macroblock_push_rotate_and_mask_bits_register(
			dest_reg_dep | offset_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			local_dest_reg->mapped_reg_num,
			offset_reg->mapped_reg_num,
			0, 31, FALSE);

	//Release temp registers
	helper_free_tmp_reg(mask_reg);
	helper_free_tmp_reg(offset_reg);
}
void comp_opcode_BFEXTS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* data_reg = NULL;
	comp_tmp_reg* complement_width_reg = NULL;
	uae_u64 dest_reg_dep = 0ULL;

	signed int extword = *((signed short*)(history->location + 1));

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			extword,
			&data_reg,
			NULL, NULL, NULL, NULL, NULL,
			&complement_width_reg,
			FALSE, FALSE);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//Rotate result to the bottom of the register using the width
	comp_macroblock_push_arithmetic_shift_right_register_register(
			data_reg->reg_usage_mapping | complement_width_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			data_reg->mapped_reg_num,
			complement_width_reg->mapped_reg_num,
			FALSE);

	//Release registers
	helper_free_tmp_reg(data_reg);
	helper_free_tmp_reg(complement_width_reg);
}
void comp_opcode_BFEXTU2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* offset_reg;
	comp_tmp_reg* mask_reg;
	uae_u64 dest_reg_dep = 0ULL;

	signed int extword = *((signed short*)(history->location + 1));

	helper_bit_field_reg_opertion_flag_test(
			*((signed short*)(history->location + 1)),
			&mask_reg,
			&offset_reg);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//And the mask to the target register
	comp_macroblock_push_and_register_register(
			output_dep | mask_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

 	//Rotate result to the bottom of the register using the offset
	comp_macroblock_push_rotate_and_mask_bits_register(
			dest_reg_dep | offset_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			local_dest_reg->mapped_reg_num,
			offset_reg->mapped_reg_num,
			0, 31, FALSE);

	//Release temp registers
	helper_free_tmp_reg(mask_reg);
	helper_free_tmp_reg(offset_reg);
}
void comp_opcode_BFEXTU2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* data_reg = NULL;
	comp_tmp_reg* complement_width_reg = NULL;
	uae_u64 dest_reg_dep = 0ULL;

	signed int extword = *((signed short*)(history->location + 1));

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			extword,
			&data_reg,
			NULL, NULL, NULL, NULL, NULL,
			&complement_width_reg,
			FALSE, FALSE);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//Rotate result to the bottom of the register using the width
	comp_macroblock_push_logic_shift_right_register_register(
			data_reg->reg_usage_mapping | complement_width_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			data_reg->mapped_reg_num,
			complement_width_reg->mapped_reg_num,
			FALSE);

	//Release registers
	helper_free_tmp_reg(data_reg);
	helper_free_tmp_reg(complement_width_reg);
}
void comp_opcode_BFFFO2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* offset_reg;
	comp_tmp_reg* mask_reg;
	uae_u64 dest_reg_dep = 0ULL;

	//Read the extension word
	signed int extword = *((signed short*)(history->location + 1));

	helper_bit_field_reg_opertion_flag_test(extword, &mask_reg, &offset_reg);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//And the mask to the destination register
	comp_macroblock_push_and_register_register(
			output_dep | mask_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Prepare mask for bits after the bit field, let's reuse the mask register
	comp_macroblock_push_load_register_long(
			mask_reg->reg_usage_mapping,
			mask_reg->mapped_reg_num,
			0xffffffff);

	//Shift register to the right to clear bits for the offset+width
	comp_macroblock_push_logic_shift_right_register_register(
			mask_reg->reg_usage_mapping | offset_reg->reg_usage_mapping,
			mask_reg->reg_usage_mapping,
			mask_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			offset_reg->mapped_reg_num,
			FALSE);

	//Or mask into destination register
	comp_macroblock_push_or_register_register(
			dest_reg_dep | mask_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			local_dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Count leading zero bits, this will be the result of the instruction
	comp_macroblock_push_count_leading_zeroes_register(
			dest_reg_dep,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			local_dest_reg->mapped_reg_num,
			FALSE);

	//Release temp registers
	helper_free_tmp_reg(mask_reg);
	helper_free_tmp_reg(offset_reg);
}
void comp_opcode_BFFFO2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* data_reg = NULL;
	comp_tmp_reg* complement_width_reg = NULL;
	uae_u64 dest_reg_dep = 0ULL;

	signed int extword = *((signed short*)(history->location + 1));

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			extword,
			&data_reg,
			NULL, NULL, NULL, NULL, NULL,
			&complement_width_reg, FALSE, TRUE);

	//Extract destination register
	comp_tmp_reg* local_dest_reg = helper_bit_field_extract_reg(extword, &dest_reg_dep, FALSE);

	//Prepare stop bit for the counting to the end of the data
	comp_tmp_reg* stop_bit_reg = helper_allocate_tmp_reg_with_init(0x80000000);

	//Shift stop bit register to the left until it reaches the end of the data width
	//If the complement width is 0 then no need for stop bit (highest bit is masked out)
	comp_macroblock_push_rotate_and_mask_bits_register(
			stop_bit_reg->reg_usage_mapping | complement_width_reg->reg_usage_mapping,
			stop_bit_reg->reg_usage_mapping,
			stop_bit_reg->mapped_reg_num,
			stop_bit_reg->mapped_reg_num,
			complement_width_reg->mapped_reg_num,
			1, 31, FALSE);

	//Or mask into destination register
	comp_macroblock_push_or_register_register(
			data_reg->reg_usage_mapping | stop_bit_reg->reg_usage_mapping,
			data_reg->reg_usage_mapping,
			data_reg->mapped_reg_num,
			data_reg->mapped_reg_num,
			stop_bit_reg->mapped_reg_num,
			FALSE);

	//Count leading zero bits, this will be the result of the instruction
	comp_macroblock_push_count_leading_zeroes_register(
			data_reg->reg_usage_mapping,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			data_reg->mapped_reg_num,
			FALSE);

	//Summarize counted leading zero bits and the full offset
	comp_macroblock_push_add_register_register(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | dest_reg_dep,
			dest_reg_dep,
			local_dest_reg->mapped_reg_num,
			local_dest_reg->mapped_reg_num,
			PPCR_TMP_NONVOL0_MAPPED,
			FALSE);

	//Release registers
	helper_free_tmp_reg(data_reg);
	helper_free_tmp_reg(stop_bit_reg);
	helper_free_tmp_reg(complement_width_reg);

	//Restore non-volatile register #0 (see helper_bit_field_mem_opertion_flag_test function)
	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);
}
void comp_opcode_BFINS2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* bit_field_width;
	uae_u64 src_reg_dep = 0ULL;

	//Read the extension word
	signed int extword = *((signed short*)(history->location + 1));

	//Extract destination register
	comp_tmp_reg* local_src_reg = helper_bit_field_extract_reg(extword, &src_reg_dep, TRUE);

	//Get the offset
	comp_tmp_reg* bit_field_offset = helper_extract_bitfield_offset(extword);

	//Get the mask and the width
	comp_tmp_reg* bit_field_mask = helper_create_bitfield_mask(extword, NULL, bit_field_offset->reg_usage_mapping, bit_field_offset->mapped_reg_num, &bit_field_width);

	//Allocate temporary register for the result
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Rotate source data to the top using complement width
	comp_macroblock_push_logic_shift_left_register_register(
			src_reg_dep | bit_field_width->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			bit_field_width->mapped_reg_num,
			FALSE);

	//Width register is not needed anymore
	helper_free_tmp_reg(bit_field_width);

	//Mask out bits from the prepared source register
	comp_macroblock_push_and_register_register(
			tmpreg->reg_usage_mapping | bit_field_mask->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			bit_field_mask->mapped_reg_num,
			TRUE);

	//Save N and Z flags, clear C and V
	helper_check_nz_clear_cv_flags();

	//Rotate the mask to the final position using the offset
	comp_macroblock_push_logic_shift_right_register_register(
			bit_field_mask->reg_usage_mapping | bit_field_offset->reg_usage_mapping,
			bit_field_mask->reg_usage_mapping,
			bit_field_mask->mapped_reg_num,
			bit_field_mask->mapped_reg_num,
			bit_field_offset->mapped_reg_num,
			FALSE);

	//And complement of the mask to the destination register
	comp_macroblock_push_and_register_complement_register(
			output_dep | bit_field_mask->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			bit_field_mask->mapped_reg_num,
			FALSE);

	//Rotate the prepared source to the final position using offset
	comp_macroblock_push_logic_shift_right_register_register(
			tmpreg->reg_usage_mapping | bit_field_offset->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			bit_field_offset->mapped_reg_num,
			FALSE);

	//Or the source into the destination
	comp_macroblock_push_or_register_register(
			output_dep | tmpreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			FALSE);

	helper_free_tmp_reg(tmpreg);
	helper_free_tmp_reg(bit_field_mask);
	helper_free_tmp_reg(bit_field_offset);
}
void comp_opcode_BFINS2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* local_src_reg_low;
	comp_tmp_reg* local_src_reg_high;
	comp_tmp_reg* data_reg_low;
	comp_tmp_reg* data_reg_high;
	comp_tmp_reg* mask_reg_low;
	comp_tmp_reg* mask_reg_high;
	comp_tmp_reg* bit_field_complement_width;
	comp_tmp_reg* bit_field_summary_offset;
	uae_u64 src_reg_dep = 0ULL;

	//Read the extension word
	signed int extword = *((signed short*)(history->location + 1));

	//TODO: the special case can be handled a more optimized way when both the offset and the width is specified in the extension word
	//Get the offset from the extension word
	comp_tmp_reg* bit_field_offset = helper_extract_bitfield_offset(extword);

	//TODO: preserving of data in non-volatile registers is not necessary when direct memory access is used
	//Save non-volatile register #0
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);

	//Calculate byte offset in memory into non-volatile register #0
	comp_macroblock_push_arithmetic_shift_right_register_imm(
			bit_field_offset->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			bit_field_offset->mapped_reg_num,
			3, FALSE);

	//Summarize original memory address and offset into non-volatile register #0
	comp_macroblock_push_add(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | dest_mem_addrreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			PPCR_TMP_NONVOL0_MAPPED,
			dest_mem_addrreg->mapped_reg_num);

	//Save non-volatile register #1
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL1_MAPPED);

	//Calculate remaining bits from the offset into non-volatile register #1
	comp_macroblock_push_and_low_register_imm(
			bit_field_offset->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			PPCR_TMP_NONVOL1_MAPPED,
			bit_field_offset->mapped_reg_num,
			7);

	//Free offset temp register
	helper_free_tmp_reg(bit_field_offset);

	//Read high 32 bit into a register from memory
	comp_tmp_reg* result_reg = helper_read_memory_mapped(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			history,
			PPCR_TMP_NONVOL0_MAPPED,
			4, FALSE);

	//TODO: if it would be possible to specify a target register to the helper then this step would not be needed
	//Save result into the temporary storage
	comp_macroblock_push_save_register_to_context(
			result_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			result_reg->mapped_reg_num);

	//Release result temp reg
	helper_free_tmp_reg(result_reg);

	//Calculate next address for lower 8 bit into spec temp reg (R0)
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_TMP_NONVOL0_MAPPED,
			4);

	//Read lower 8 bit into temp reg
	data_reg_low = helper_read_memory_mapped(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			history,
			PPCR_SPECTMP_MAPPED,
			1, FALSE);

	//Reallocate higher data reg
	data_reg_high = helper_allocate_tmp_reg();

	//Restore previously read data
	comp_macroblock_push_restore_register_from_context(
			data_reg_high->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			data_reg_high->mapped_reg_num);

	//Allocate temp register for low 8 bit shifting steps
	comp_tmp_reg* bit_field_offset_low = helper_allocate_tmp_reg();

	//Calculate shifting for low bits: 8 - offset into offset
	comp_macroblock_push_sub_register_from_immediate(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			bit_field_offset_low->reg_usage_mapping,
			bit_field_offset_low->mapped_reg_num,
			PPCR_TMP_NONVOL1_MAPPED,
			8);

	//Allocate summary offset register
	bit_field_summary_offset = helper_allocate_tmp_reg();

	//Get the mask and the complement width
	mask_reg_high = helper_create_bitfield_mask(extword, bit_field_summary_offset, COMP_COMPILER_MACROBLOCK_REG_NONVOL1, PPCR_TMP_NONVOL1_MAPPED, &bit_field_complement_width);

	//Calculate complement of summary offset for rotation to the left
	comp_macroblock_push_sub_register_from_immediate(
			bit_field_summary_offset->reg_usage_mapping,
			bit_field_summary_offset->reg_usage_mapping,
			bit_field_summary_offset->mapped_reg_num,
			bit_field_summary_offset->mapped_reg_num,
			32);

	//Allocate new temporary register for the low 8 bit mask
	mask_reg_low = helper_allocate_tmp_reg();

	//Rotate mask to the final position and keep the lowest byte only
	comp_macroblock_push_logic_shift_left_register_register(
			mask_reg_high->reg_usage_mapping | bit_field_offset_low->reg_usage_mapping,
			mask_reg_low->reg_usage_mapping,
			mask_reg_low->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			bit_field_offset_low->mapped_reg_num,
			FALSE);

	//Keep only the lowest 8 bit
	comp_macroblock_push_and_low_register_imm(
			mask_reg_low->reg_usage_mapping,
			mask_reg_low->reg_usage_mapping,
			mask_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			0xff);

	//Rotate mask to the right position for the higher 32 bit using the offset for the bit field operation
	comp_macroblock_push_logic_shift_right_register_register(
			mask_reg_high->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			mask_reg_high->reg_usage_mapping,
			mask_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			PPCR_TMP_NONVOL1_MAPPED,
			FALSE);

	//Extract destination register
	comp_tmp_reg* local_src_reg = helper_bit_field_extract_reg(extword, &src_reg_dep, TRUE);

	//Rotate source data to the top into local src high using complement width, update N and Z internal flags
	comp_macroblock_push_logic_shift_left_register_register(
			src_reg_dep | bit_field_complement_width->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			local_src_reg->mapped_reg_num,
			bit_field_complement_width->mapped_reg_num,
			TRUE);

	//Complement width register is not needed anymore
	helper_free_tmp_reg(bit_field_complement_width);

	//Save N and Z flags, clear C and V
	helper_check_nz_clear_cv_flags();

	//Allocate new temporary register for the low 8 bit source register
	local_src_reg_low = helper_allocate_tmp_reg();

	//Rotate source data to the final position for lower 8 bit
	comp_macroblock_push_logic_shift_left_register_register(
			src_reg_dep | bit_field_offset_low->reg_usage_mapping,
			local_src_reg_low->reg_usage_mapping,
			local_src_reg_low->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			bit_field_offset_low->mapped_reg_num,
			FALSE);

	//Keep only the masked part from the 8 bit
	comp_macroblock_push_and_register_register(
			local_src_reg_low->reg_usage_mapping | mask_reg_low->reg_usage_mapping,
			local_src_reg_low->reg_usage_mapping,
			local_src_reg_low->mapped_reg_num,
			local_src_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			FALSE);

	local_src_reg_high = helper_allocate_tmp_reg();

	//Rotate source data to the final position for higher 32 bit
	comp_macroblock_push_rotate_and_mask_bits_register(
			src_reg_dep | bit_field_summary_offset->reg_usage_mapping,
			local_src_reg_high->reg_usage_mapping,
			local_src_reg_high->mapped_reg_num,
			local_src_reg->mapped_reg_num,
			bit_field_summary_offset->mapped_reg_num,
			0, 31, FALSE);

	//Summary offset is not needed anymore
	helper_free_tmp_reg(bit_field_summary_offset);

	//Keep only the masked part from the 32 bit
	comp_macroblock_push_and_register_register(
			local_src_reg_high->reg_usage_mapping | mask_reg_high->reg_usage_mapping,
			local_src_reg_high->reg_usage_mapping,
			local_src_reg_high->mapped_reg_num,
			local_src_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			FALSE);

	//And complement of the mask to the high data register
	comp_macroblock_push_and_register_complement_register(
			data_reg_high->reg_usage_mapping | mask_reg_high->reg_usage_mapping,
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			FALSE);

	//And complement of the mask to the low data register
	comp_macroblock_push_and_register_complement_register(
			data_reg_low->reg_usage_mapping | mask_reg_low->reg_usage_mapping,
			data_reg_low->reg_usage_mapping,
			data_reg_low->mapped_reg_num,
			data_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			FALSE);

	//Release mask
	helper_free_tmp_reg(mask_reg_low);
	helper_free_tmp_reg(mask_reg_high);

	//Or source high into the data high
	comp_macroblock_push_or_register_register(
			data_reg_high->reg_usage_mapping | local_src_reg_high->reg_usage_mapping,
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			local_src_reg_high->mapped_reg_num,
			FALSE);

	//Or source low into the data low
	comp_macroblock_push_or_register_register(
			data_reg_low->reg_usage_mapping | local_src_reg_low->reg_usage_mapping,
			data_reg_low->reg_usage_mapping,
			data_reg_low->mapped_reg_num,
			data_reg_low->mapped_reg_num,
			local_src_reg_low->mapped_reg_num,
			FALSE);

	//Free temp registers
	helper_free_tmp_reg(bit_field_offset_low);
	helper_free_tmp_reg(local_src_reg_high);
	helper_free_tmp_reg(local_src_reg_low);

	//Save result back to memory, also release temporary registers
	helper_bit_field_mem_save(history, data_reg_high, data_reg_low);
}
void comp_opcode_BFSET2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg;
	helper_bit_field_reg_opertion_flag_test(
			*((signed short*)(history->location + 1)),
			&mask_reg,
			NULL);

	//Or the mask to the target register
	comp_macroblock_push_or_register_register(
			output_dep | mask_reg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			mask_reg->mapped_reg_num,
			FALSE);

	//Release mask register
	helper_free_tmp_reg(mask_reg);
}
void comp_opcode_BFSET2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_tmp_reg* mask_reg_low = NULL;
	comp_tmp_reg* mask_reg_high = NULL;
	comp_tmp_reg* data_reg_low = NULL;
	comp_tmp_reg* data_reg_high = NULL;

	//Get data from memory, mask, adjusted destination address, set flags
	helper_bit_field_mem_opertion_flag_test(
			history,
			*((signed short*)(history->location + 1)),
			NULL,
			&data_reg_high,
			&data_reg_low,
			&mask_reg_high,
			&mask_reg_low,
			NULL, NULL,
			TRUE, FALSE);

	//Or the mask to the target register on higher 32 bits
	comp_macroblock_push_or_register_register(
			data_reg_high->reg_usage_mapping | mask_reg_high->reg_usage_mapping,
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			mask_reg_high->mapped_reg_num,
			FALSE);

	//Or the mask to the target register on lower 8 bits
	comp_macroblock_push_or_register_register(
			data_reg_low->reg_usage_mapping | mask_reg_low->reg_usage_mapping,
			data_reg_low->reg_usage_mapping,
			data_reg_low->mapped_reg_num,
			data_reg_low->mapped_reg_num,
			mask_reg_low->mapped_reg_num,
			FALSE);

	//Release mask registers
	helper_free_tmp_reg(mask_reg_low);
	helper_free_tmp_reg(mask_reg_high);

	//Save result back to memory, also release temporary registers
	helper_bit_field_mem_save(history, data_reg_high, data_reg_low);
}
void comp_opcode_BFTST2REG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_bit_field_reg_opertion_flag_test(*((signed short*)(history->location + 1)), NULL, NULL);
}
void comp_opcode_BFTST2MEM(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_bit_field_mem_opertion_flag_test(
			history,
			*((signed short*)(history->location + 1)),
			NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			FALSE, FALSE);
}
void comp_opcode_FBCOND(const cpu_history* history, struct comptbl* props) REGPARAM
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
void comp_opcode_FSCCREG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_FSCCMEM(const cpu_history* history, struct comptbl* props) REGPARAM
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
STATIC_INLINE comp_tmp_reg* helper_allocate_tmp_reg()
{
	//This function is simple for now, but this might change in the future
	return comp_allocate_temp_register(NULL, PPC_TMP_REG_NOTUSED_MAPPED);
}

/**
 * Allocate temporary register, preferred specified (no guarantee that it will be allocated)
 */
STATIC_INLINE comp_tmp_reg* helper_allocate_preferred_tmp_reg(comp_ppc_reg preferred)
{
	//This function is simple for now, but this might change in the future
	return comp_allocate_temp_register(NULL, preferred);
}

/**
 * Allocate a temporary register and load an immediate data into it
 */
STATIC_INLINE comp_tmp_reg* helper_allocate_tmp_reg_with_init(uae_u32 immed)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();
	comp_macroblock_push_load_register_long(
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			immed);

	return tmpreg;
}

/**
 * Free a previously allocated temporary register
 */
STATIC_INLINE void helper_free_tmp_reg(comp_tmp_reg* reg)
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
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset, BOOL invertc)
{
	comp_tmp_reg* tmpreg;
	comp_tmp_reg* flagtmp;
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
					PPCR_FLAGS_MAPPED);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS_MAPPED,
						PPCR_FLAGS_MAPPED,
						1 << 5);
			}
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_FLAGC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					16, 26, 26, FALSE);
			break;

		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//All flags but X are updated:
			//Move XER to CR2, copy CR to the temp register then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 0, 10, FALSE);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS_MAPPED,
						PPCR_FLAGS_MAPPED,
						1 << 5);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Flags: N, C, V and X
			//Move XER to CR2, copy CR to temp, copy X to C then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						tmpreg->mapped_reg_num,
						tmpreg->mapped_reg_num,
						1 << 5);
			}
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					tmpreg->mapped_reg_num,
					16, 26, 26, FALSE);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 9, 0, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Flags: N, C and V
			//Move XER to CR2, copy CR to temp then insert temp register relevant part to the flag register in two rounds: C and V first, then N
			comp_macroblock_push_copy_nzcv_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 9, 10, FALSE);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 0, 0, FALSE);
			if (invertc)
			{
				comp_macroblock_push_xor_high_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						COMP_COMPILER_MACROBLOCK_REG_FLAGC,
						PPCR_FLAGS_MAPPED,
						PPCR_FLAGS_MAPPED,
						1 << 5);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Flags: N, Z and V
			//Move XER to CR2, copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 0, 9, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Flags: N, Z
			//Copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nz_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 0, 2, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGZ:
			//Flag: Z
			//Copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nz_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 2, 2, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGN:
			//Flag: N
			//Copy CR to temp then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nz_flags_to_register(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num);
			comp_macroblock_push_rotate_and_copy_bits(
					tmpreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS_MAPPED,
					tmpreg->mapped_reg_num,
					0, 0, 0, FALSE);
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
					PPCR_FLAGS_MAPPED,
					0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: N, Z, C and V: mask out these registers and keep X only
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 26, 26, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Clear flags: C and V: mask out these registers and keep N, Z and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 11, 8, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Clear flags: C and X: mask out these registers and keep N, Z and V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
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
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					0, 26, 26, FALSE);
			} else {
				//Clear flags: C, V and N: mask out these registers and keep Z and X
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					0, 11, 8, FALSE);
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					0, 1, 31, FALSE);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: C, V and Z: mask out these registers and keep N and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 11, 1, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Clear flags: N, Z and V: mask out these registers and keep C and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 10, 31, FALSE);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: N and Z: mask out these registers and keep C, V and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 3, 31, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGC:
			//Clear flag: C
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 11, 9, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGV:
			//Clear flag: V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 10, 8, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGN:
			//Clear flag: N
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				0, 1, 31, FALSE);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGZ:
			//Clear flag: Z
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
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
				PPCR_FLAGS_MAPPED, PPCR_FLAGS_MAPPED, flagmask);

		if ((flagsset & COMP_COMPILER_MACROBLOCK_REG_FLAGX) != 0)
		{
			//Set X flag in the low half word, depends on all flags, except X
			comp_macroblock_push_or_low_register_imm(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS_MAPPED, PPCR_FLAGS_MAPPED, (1 << (PPCR_FLAG_X)));
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
STATIC_INLINE void helper_check_nzcvx_flags(BOOL invertc)
{
	//Save all flags
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			invertc);
}

/**
 * Saves N, V, C, X registers and clears Z flag if the last operation result was non-zero
 * Note: PPCR_SPECTMP (R0) is used by this function, the content is not preserved.
 */
STATIC_INLINE void helper_check_ncvx_flags_copy_z_flag_if_cleared(BOOL invertc)
{
	//Set all bits to 1 in spec temp register
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			-1);

	//Mask out all flags to 1, except the Z flag in the flag register
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			PPCR_FLAGS_MAPPED,
			PPCR_SPECTMP_MAPPED,
			0, 3, 1, FALSE);

	//All flags but X are updated: move XER to CR2, copy CR to the spec temp register
	comp_macroblock_push_copy_nzcv_flags_to_register(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED);

	//Copy the harvested flags, Z will only change if it was cleared
	comp_macroblock_push_and_register_register(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			PPCR_FLAGS_MAPPED,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			FALSE);

	//Invert C flag if required
	if (invertc)
	{
		comp_macroblock_push_xor_high_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_FLAGC,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC,
				PPCR_FLAGS_MAPPED,
				PPCR_FLAGS_MAPPED,
				1 << 5);
	}

	//Copy C flag to X
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			16, 26, 26, FALSE);
}

/* Copy the emulated X flag to the emulated X flag */
STATIC_INLINE void helper_copy_x_flag_to_c_flag()
{
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			16, 10, 10, FALSE);
}

/* Copy the emulated X flag to the internal PowerPC C flag in XER register
 * Note: PPCR_SPECTMP (R0) is used by this function, the content is not preserved. */
STATIC_INLINE void helper_copy_x_flag_to_internal_c_flag(BOOL invertX)
{
	//Get the X flag from the flag register and rotate it to the right position for the PPC C internal flag.
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_FLAGS_MAPPED,
			24, 2, 2, FALSE);

	//Invert flag before copy if required
	if (invertX)
	{
		comp_macroblock_push_xor_high_register_imm(
				COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				PPCR_SPECTMP_MAPPED,
				PPCR_SPECTMP_MAPPED,
				0x2000);
	}

	//Copy X flag to XER
	comp_macroblock_push_copy_register_to_cv_flags(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED);
}

/* Extract C flag from the specified register using the specified bit,
 * clear N, Z, V and X flags */
STATIC_INLINE void helper_extract_c_clear_nzvx_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit)
{
	//C flag is at bit 21
	int shift = 21 - input_bit;

	//Right direction instead of left
	if (shift < 0) shift += 32;

	//Extract C flag
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			input_reg->mapped_reg_num,
			shift, 10, 10, FALSE);
}

/* Extract C flag from the specified register using the specified bit,
 * clear N, Z, and V flags, X flag will be preserved.
 * Note: use helper_extract_c_clear_nzvx_flags() if X flag will be overwritten, that function generates
 *       less instructions. */
STATIC_INLINE void helper_extract_c_clear_nzv_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit)
{
	//C flag is at bit 21
	int shift = 21 - input_bit;

	//Right direction instead of left
	if (shift < 0) shift += 32;

	//Clear N, Z and V flags
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Extract C flag and insert it into the flag register
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			input_reg->mapped_reg_num,
			shift, 10, 10, FALSE);
}

/* Extract C and X flags from the specified register using the specified bit,
 * clear N, Z and V flags. */
STATIC_INLINE void helper_extract_cx_clear_nzv_flags(uae_u64 regsin, comp_tmp_reg* input_reg, int input_bit)
{
	helper_extract_c_clear_nzvx_flags(regsin, input_reg, input_bit);

	//Copy C flag to X
	comp_macroblock_push_rotate_and_copy_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			PPCR_FLAGS_MAPPED,
			PPCR_FLAGS_MAPPED,
			16, 26, 26, FALSE);
}

/* Extract C flag from the specified register using the register for rotation,
 * clear V flag. C flag is extracted from the lowest bit.
 * If shift is zero then C flag is cleared.*/
STATIC_INLINE void helper_extract_c_clear_v_flags(uae_u64 regsin, comp_tmp_reg* input_reg, comp_tmp_reg* rotation_reg, BOOL left_shift)
{
	//Clear C and V flags in one step
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Extract C flag (implemented as a macroblock for simplicity)
	comp_macroblock_push_shift_extract_c_flag(
			regsin,
			input_reg->mapped_reg_num,
			rotation_reg->mapped_reg_num,
			left_shift);
}

/* Extract C flag from the specified register using the register for rotation,
 * copy C to X flag, clear V flag. C flag is extracted from the lowest bit.
 * If shift is zero then C flag is cleared.*/
STATIC_INLINE void helper_extract_cx_clear_v_flags(uae_u64 regsin, comp_tmp_reg* input_reg, comp_tmp_reg* rotation_reg, BOOL left_shift)
{
	//Clear C and V flags in one step
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			FALSE);

	//Extract C flag (implemented as a macroblock for simplicity)
	comp_macroblock_push_shift_extract_cx_flag(
			regsin,
			input_reg->mapped_reg_num,
			rotation_reg->mapped_reg_num,
			left_shift);
}

/* Saving all flags for an arithmetic instruction except X */
STATIC_INLINE void helper_check_nzcv_flags(BOOL invertc)
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
	if (src_mem_addrreg)
	{
		comp_free_temp_register(src_mem_addrreg);
		src_mem_addrreg = NULL;
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
	src_mem_addrreg = src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, modified);
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
	src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), TRUE, modified);
	src_mem_addrreg = helper_allocate_tmp_reg();

	//Instruction is depending on both registers
	input_dep |= src_mem_addrreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);
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
			src_mem_addrreg->reg_usage_mapping,
			src_mem_addrreg->mapped_reg_num,
			src_reg->mapped_reg_num);
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
			src_reg->mapped_reg_num,
			src_reg->mapped_reg_num,
			immediate);
}

/**
 * Release the temp register for the destination memory address register
 */
STATIC_INLINE void helper_free_dest_mem_addr_temp_reg()
{
	//Release destination memory temp address register, if it is still allocated
	if (dest_mem_addrreg)
	{
		comp_free_temp_register(dest_mem_addrreg);
		dest_mem_addrreg = NULL;
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
	dest_mem_addrreg = dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, modified);
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
	dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), TRUE, modified);
	dest_mem_addrreg = helper_allocate_tmp_reg();

	//Instruction is depending on both registers
	input_dep |= dest_mem_addrreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
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
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
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
	comp_tmp_reg* tmpregaddr;

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_write_long(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_long_register(input_dep, src_reg->mapped_reg_num);
		break;
	case 2:
		spec = comp_is_spec_memory_write_word(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_word_register(input_dep, src_reg->mapped_reg_num);
		break;
	case 1:
		spec = comp_is_spec_memory_write_byte(history->pc, history->specmem);
		if ((!immediate) && checkflags) comp_macroblock_push_check_byte_register(input_dep, src_reg->mapped_reg_num);
		break;
	default:
		write_log("Error: wrong operation size for MOVIMMREG2MEM\n");
		abort();
	}

	//Is it immediate mode?
	if (immediate)
	{
		//Immediate: load the immediate into a temporary register as source
		src_reg  = helper_allocate_tmp_reg_with_init(src_immediate);
		input_dep |=  src_reg->reg_usage_mapping;

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
				input_dep | (dest_mem_addrreg == NULL ? 0 : dest_mem_addrreg->reg_usage_mapping),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_reg->mapped_reg_num,
				dest_mem_addrreg->mapped_reg_num,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = dest_mem_addrreg = NULL;
		if (immediate)
		{
			src_reg = NULL;
		}
	}
	else
	{
		//Normal memory access
		tmpregaddr = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				dest_mem_addrreg == NULL ? output_dep : dest_mem_addrreg->reg_usage_mapping,
				tmpregaddr->reg_usage_mapping,
				tmpregaddr->mapped_reg_num,
				dest_mem_addrreg->mapped_reg_num);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_save_memory_long(
					input_dep | tmpregaddr->reg_usage_mapping,
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 2:
			comp_macroblock_push_save_memory_word(
					input_dep | tmpregaddr->reg_usage_mapping,
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 1:
			comp_macroblock_push_save_memory_byte(
					input_dep | tmpregaddr->reg_usage_mapping,
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
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
				dest_reg->mapped_reg_num,
				src_immediate);
	} else {
		//Word or byte sized: needs an additional bit insert

		//Is the immediate zero?
		if (src_immediate)
		{
			comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

			//Load the immediate value to the temporary register
			comp_macroblock_push_load_register_long(
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					src_immediate);

			if (size == 2)
			{
				//Copy the lower word into the destination register
				comp_macroblock_push_copy_register_word(
						output_dep | tmpreg->reg_usage_mapping,
						output_dep,
						dest_reg->mapped_reg_num,
						tmpreg->mapped_reg_num);
			} else {
				//We assume that this must be byte size, lazy-lazy...

				//Copy the lowest byte into the destination register
				comp_macroblock_push_copy_register_byte(
						output_dep | tmpreg->reg_usage_mapping,
						output_dep,
						dest_reg->mapped_reg_num,
						tmpreg->mapped_reg_num);
			}

			//Done with the temp register
			comp_free_temp_register(tmpreg);
		} else {
			//Immediate is zero: no need for additional register, we can simply clear the bits
			comp_macroblock_push_rotate_and_mask_bits(
					output_dep,
					output_dep,
					dest_reg->mapped_reg_num,
					dest_reg->mapped_reg_num,
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
	comp_tmp_reg* tmpreg;
	comp_tmp_reg* tmpregaddr;

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
				input_dep | (src_mem_addrreg == NULL ? 0 : src_mem_addrreg->reg_usage_mapping),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_mem_addrreg->mapped_reg_num,
				PPCR_SPECTMP_MAPPED,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = NULL;

		//Reallocate the target register
		dest_reg = comp_map_temp_register(
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
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					output_dep,
					dest_reg->mapped_reg_num,
					PPCR_SPECTMP_MAPPED);
			break;
		case 2:
			if (dataregmode)
			{
				//Result is inserted into data register
				comp_macroblock_push_copy_register_word(
						output_dep | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
						output_dep,
						dest_reg->mapped_reg_num,
						PPCR_SPECTMP_MAPPED);
			} else {
				//Result is sign-extended into address register
				comp_macroblock_push_copy_register_word_extended(
						COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
						output_dep,
						dest_reg->mapped_reg_num,
						PPCR_SPECTMP_MAPPED,
						FALSE);
			}
			break;
		case 1:
			comp_macroblock_push_copy_register_byte(
					output_dep | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					output_dep,
					dest_reg->mapped_reg_num,
					PPCR_SPECTMP_MAPPED);
			break;
		}
	}
	else
	{
		//Normal memory access

		tmpregaddr = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				src_mem_addrreg == NULL ? input_dep : src_mem_addrreg->reg_usage_mapping,
				tmpregaddr->reg_usage_mapping,
				tmpregaddr->mapped_reg_num,
				src_mem_addrreg->mapped_reg_num);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_load_memory_long(
					input_dep | tmpregaddr->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 2:
			if (dataregmode)
			{
				//Result is inserted into data register
				tmpreg = helper_allocate_tmp_reg();

				comp_macroblock_push_load_memory_word(
						input_dep | tmpregaddr->reg_usage_mapping,
						tmpreg->reg_usage_mapping,
						tmpreg->mapped_reg_num,
						tmpregaddr->mapped_reg_num,
						0);
				comp_macroblock_push_copy_register_word(
						output_dep | tmpreg->reg_usage_mapping,
						output_dep,
						dest_reg->mapped_reg_num,
						tmpreg->mapped_reg_num);

				comp_free_temp_register(tmpreg);
			} else {
				//Result is loaded with sign-extension into address register
				comp_macroblock_push_load_memory_word_extended(
						input_dep | tmpregaddr->reg_usage_mapping,
						output_dep,
						dest_reg->mapped_reg_num,
						tmpregaddr->mapped_reg_num,
						0);
			}
			break;
		case 1:
			tmpreg = helper_allocate_tmp_reg();
			comp_macroblock_push_load_memory_byte(
					input_dep | tmpregaddr->reg_usage_mapping,
					tmpreg->reg_usage_mapping,
					tmpreg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			comp_macroblock_push_copy_register_byte(
					output_dep | tmpreg->reg_usage_mapping,
					output_dep,
					dest_reg->mapped_reg_num,
					tmpreg->mapped_reg_num);
			comp_free_temp_register(tmpreg);
			break;
		}

		comp_free_temp_register(tmpregaddr);
	}

	//Check result for flags by operation size
	if (dataregmode)
	{
		helper_check_result_set_flags(output_dep, dest_reg->mapped_reg_num, size);

		//Save flags
		helper_check_nz_clear_cv_flags();
	}
}

/**
 * Implementation of all MOVEM2MEM and MOVEM2MEMU instructions
 * Parameters:
 *    history - pointer to cpu execution history
 *    props - pointer to the instruction properties
 *    size - size of the operation: word (2) or long (4)
 *    update - if TRUE then the destination address register will be used and
 *			   the decremented address will be saved back to the register (move to memory with update),
 *             if FALSE then the destination memory register will be used.
 */
STATIC_INLINE void helper_MOVEM2MEM(const cpu_history* history, struct comptbl* props, uae_u8 size, BOOL update)
{
	int spec, i, offset;
	comp_tmp_reg * selected_reg;
	comp_ppc_reg selected_reg_mapped;
	comp_tmp_reg* tempaddr_reg;

	//Read the extension word
	unsigned short extword  = *((unsigned short*)(history->location + 1));

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
		write_log("Error: wrong operation size for MOVEM2MEM\n");
		abort();
	}

	if (spec)
	{
		//Save non-volatile register temporarily
		comp_macroblock_push_save_register_to_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);

		//Copy destination address register to the non-volatile register
		comp_macroblock_push_copy_register_long(
				input_dep | (update ? 0 : dest_mem_addrreg->reg_usage_mapping),
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
				PPCR_TMP_NONVOL0_MAPPED,
				update ? dest_reg->mapped_reg_num :  dest_mem_addrreg->mapped_reg_num);

		//Flush temp registers back to the store
		comp_flush_temp_registers(TRUE);

		//Clear destination address register
		dest_mem_addrreg = NULL;

		//Check specified registers in the extension word
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Decrease address register by the size of the operation if update
				if (update)
				{
					comp_macroblock_push_add_register_imm(
							COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
							COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
							PPCR_TMP_NONVOL0_MAPPED,
							PPCR_TMP_NONVOL0_MAPPED,
							-size);
				}

				//Read register content from store to the second argument register (to avoid copying)
				//For this MOVEM instruction the registers are stored in reversed order in the extension word
				//when the operation is update, normal order otherwise.
				comp_macroblock_push_load_memory_long(
						COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_PARAM2_MAPPED,
						PPCR_REGS_BASE_MAPPED,
						update ? (15 - i) * 4 : i * 4);

				//Store register content in memory using special memory access
				comp_macroblock_push_save_memory_spec(
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_PARAM2_MAPPED,
						PPCR_TMP_NONVOL0_MAPPED,
						size);

				//Increase address register by the size of the operation if not update
				if (!update)
				{
					comp_macroblock_push_add_register_imm(
							COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
							COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
							PPCR_TMP_NONVOL0_MAPPED,
							PPCR_TMP_NONVOL0_MAPPED,
							size);
				}
			}
		}


		if (update)
		{
			//Keep modified address register content, but it must be reallocated
			dest_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), FALSE, TRUE);

			//Copy modified address to the allocated address register
			comp_macroblock_push_copy_register_long(
					COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
					COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg),
					dest_reg->mapped_reg_num,
					PPCR_TMP_NONVOL0_MAPPED);
		}

		//Restore non-volatile register
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);
	}
	else
	{
		//Normal memory access
		tempaddr_reg = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				output_dep,
				tempaddr_reg->reg_usage_mapping,
				tempaddr_reg->mapped_reg_num,
				update ? dest_reg->mapped_reg_num : dest_mem_addrreg->mapped_reg_num);

		//Check specified registers in the extension word
		offset = 0;
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Step to the next address before the operation if update
				if (update) offset -= size;

				//Get the temp register if it was mapped already
				//For this MOVEM instruction the registers are stored in reversed order in the extension word
				//when the operation is update, normal order otherwise.
				int j = update ? 15 - i : i;
				selected_reg = comp_get_mapped_temp_register(
						j < 8 ? COMP_COMPILER_REGS_ADDRREG(j) :
								COMP_COMPILER_REGS_DATAREG(j - 8));

				if (selected_reg == NULL)
				{
					//Not mapped yet, let's read it directly from the register store
					//Read register content from store to R0
					comp_macroblock_push_load_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							PPCR_SPECTMP_MAPPED,
							PPCR_REGS_BASE_MAPPED,
							j * 4);

					selected_reg_mapped = PPCR_SPECTMP_MAPPED;
				} else {
					selected_reg_mapped = selected_reg->mapped_reg_num;
				}

				//Save data to memory, prevent from optimizing away
				switch (size)
				{
				case 4:
					comp_macroblock_push_save_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg_mapped,
							tempaddr_reg->mapped_reg_num,
							offset);
					break;
				case 2:
					comp_macroblock_push_save_memory_word(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg_mapped,
							tempaddr_reg->mapped_reg_num,
							offset);
					break;
				}

				//Step to the next address after the operation if not update
				if (!update) offset += size;
			}
		}

		if (update)
		{
			//Update register by the calculated offset
			comp_macroblock_push_add_register_imm(
					output_dep,
					output_dep,
					dest_reg->mapped_reg_num,
					dest_reg->mapped_reg_num,
					offset);
		}

		comp_free_temp_register(tempaddr_reg);
	}
}

/**
 * Implementation of all MOVEM2REG and MOVEM2REGU instructions
 * Parameters:
 *    history - pointer to cpu execution history
 *    props - pointer to the instruction properties
 *    size - size of the operation: word (2) or long (4)
 *    update - if TRUE then the source address register will be used and
 *			   the incremented address will be saved back to the register (move to register with update),
 *             if FALSE then the source memory register will be used.
 *
 */
STATIC_INLINE void helper_MOVEM2REG(const cpu_history* history, struct comptbl* props, uae_u8 size, BOOL update)
{
	int spec, i, offset;
	comp_tmp_reg * selected_reg;
	comp_ppc_reg selected_reg_mapped;
	comp_tmp_reg* tempaddr_reg;

	//Read the extension word
	unsigned short extword  = *((unsigned short*)(history->location + 1));

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_read_long(history->pc, history->specmem);
		break;
	case 2:
		spec = comp_is_spec_memory_read_word(history->pc, history->specmem);
		break;
	default:
		write_log("Error: wrong operation size for MOVEM2REG\n");
		abort();
	}

	if (spec)
	{
		//Save non-volatile register temporarily
		comp_macroblock_push_save_register_to_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);

		//Copy source address register to the non-volatile register
		comp_macroblock_push_copy_register_long(
				input_dep | (update ? 0 : src_mem_addrreg->reg_usage_mapping),
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
				PPCR_TMP_NONVOL0_MAPPED,
				update ? src_reg->mapped_reg_num : src_mem_addrreg->mapped_reg_num);

		//Flush temp registers back to the store
		comp_flush_temp_registers(TRUE);

		//Clear source address register
		src_mem_addrreg = NULL;

		//Check specified registers in the extension word
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Load register content from memory using special memory access to the first argument register (to avoid copying between registers)
				comp_macroblock_push_load_memory_spec(
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_TMP_NONVOL0_MAPPED,
						PPCR_PARAM1_MAPPED,
						size);

				//If the operation size is word then the registers are sign-extended to longword
				if (size == 2)
				{
					comp_macroblock_push_copy_register_word_extended(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							PPCR_PARAM1_MAPPED,
							PPCR_PARAM1_MAPPED,
							FALSE);
				}

				//Save register content to the store from the first argument register
				//For this MOVEM instruction the registers are stored in normal order in the extension word
				comp_macroblock_push_save_memory_long(
						COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
						COMP_COMPILER_MACROBLOCK_REG_NONE,
						PPCR_PARAM1_MAPPED,
						PPCR_REGS_BASE_MAPPED,
						i * 4);

				//Increase address register by the size of the operation
				comp_macroblock_push_add_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
						PPCR_TMP_NONVOL0_MAPPED,
						PPCR_TMP_NONVOL0_MAPPED,
						size);
			}
		}

		if (update)
		{
			//Keep modified address register content, but it must be reallocated
			src_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), FALSE, TRUE);

			//Copy modified address to the allocated address register
			comp_macroblock_push_copy_register_long(
					COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
					COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg),
					src_reg->mapped_reg_num,
					PPCR_TMP_NONVOL0_MAPPED);
		}

		//Restore non-volatile register
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);
	}
	else
	{
		//Normal memory access
		tempaddr_reg = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				input_dep,
				tempaddr_reg->reg_usage_mapping,
				tempaddr_reg->mapped_reg_num,
				update ? src_reg->mapped_reg_num : src_mem_addrreg->mapped_reg_num);

		//Check specified registers in the extension word
		offset = 0;
		for(i = 0; i < 16; i++)
		{
			if ((extword & (1 << i)) != 0)
			{
				//Get the temp register if it was mapped already
				//For this MOVEM instruction the registers are stored in normal order in the extension word
				selected_reg = comp_get_mapped_temp_register(
						i < 8 ? COMP_COMPILER_REGS_ADDRREG(i) :
								COMP_COMPILER_REGS_DATAREG(i - 8));

				if (selected_reg == NULL)
				{
					//Not mapped yet: we need a register to store the data temporarily
					selected_reg_mapped = PPCR_SPECTMP_MAPPED;
				} else {
					selected_reg_mapped = selected_reg->mapped_reg_num;
				}

				//Load data from memory, prevent from optimizing away
				switch (size)
				{
				case 4:
					comp_macroblock_push_load_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg_mapped,
							tempaddr_reg->mapped_reg_num,
							offset);
					break;
				case 2:
					comp_macroblock_push_save_memory_word(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg_mapped,
							tempaddr_reg->mapped_reg_num,
							offset);

					//If the operation size is word then the registers are sign-extended to longword
					comp_macroblock_push_copy_register_word_extended(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							selected_reg_mapped,
							selected_reg_mapped,
							FALSE);
					break;
				}

				if (selected_reg == NULL)
				{
					//Register was not mapped yet, let's store it directly in the register store
					//Save register content to store from R0
					comp_macroblock_push_save_memory_long(
							COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
							COMP_COMPILER_MACROBLOCK_REG_NONE,
							PPCR_SPECTMP_MAPPED,
							PPCR_REGS_BASE_MAPPED,
							i * 4);
				}

				//Step to the next address
				offset += size;
			}
		}

		if (update)
		{
			//Update register by the calculated offset
			comp_macroblock_push_add_register_imm(
					input_dep,
					input_dep,
					src_reg->mapped_reg_num,
					src_reg->mapped_reg_num,
					offset);
		}

		comp_free_temp_register(tempaddr_reg);
	}
}

/**
 * Implementation of RTS and RTD instruction
 * Parameters:
 *    history - pointer to the execution history
 *    stackptr_change - number of bytes to be added to the stack pointer (A7)
 */
STATIC_INLINE void helper_RTS_RTD(const cpu_history* history, uae_u16 stackptr_change)
{
	//Load stack register
	comp_tmp_reg* a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, FALSE);

	//Read return address from the stack
	comp_tmp_reg* tempreg = helper_read_memory(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			history,
			a7_reg,
			4,
			FALSE);

	//Load return address to PC
	comp_macroblock_push_load_pc_from_register(
			tempreg->reg_usage_mapping,
			tempreg->mapped_reg_num);

	//Remap stack register
	a7_reg = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(7), TRUE, TRUE);

	//Add data size to stack register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_A7,
			COMP_COMPILER_MACROBLOCK_REG_A7,
			a7_reg->mapped_reg_num,
			a7_reg->mapped_reg_num,
			stackptr_change);

	//Free temp register
	helper_free_tmp_reg(tempreg);
}

/*
 *
 * Implementation of generic functionality for MULS instructions
 * Parameters:
 *    regsin - input register dependency
 * 	  src_input_reg_mapped - mapped source register for the multiplication
 */
STATIC_INLINE void helper_MULS(uae_u64 regsin, comp_ppc_reg src_input_reg_mapped)
{
	//Sign extend the low half word to long in destination register
	comp_macroblock_push_copy_register_word_extended(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			FALSE);

	//Multiply the registers and set the flags
	comp_macroblock_push_multiply_registers(
			regsin | output_dep,
			output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
			dest_reg->mapped_reg_num,
			src_input_reg_mapped,
			dest_reg->mapped_reg_num,
			TRUE);

	//Save flags
	helper_check_nzcv_flags(FALSE);
}

/*
 *
 * Implementation of generic functionality for MULU instructions
 * Parameters:
 *    regsin - input register dependency
 * 	  src_input_reg_mapped - mapped source register for the multiplication
 */
STATIC_INLINE void helper_MULU(uae_u64 regsin, comp_ppc_reg src_input_reg_mapped)
{
	//Keep the low half word from destination register
	comp_macroblock_push_and_low_register_imm(
			output_dep,
			output_dep,
			dest_reg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			0xffff);

	//Multiply the registers and set the flags
	comp_macroblock_push_multiply_registers(
			regsin | output_dep,
			output_dep | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC,
			dest_reg->mapped_reg_num,
			src_input_reg_mapped,
			dest_reg->mapped_reg_num,
			TRUE);

	//Save flags
	helper_check_nzcv_flags(FALSE);
}

/*
 * Copy a result word from the specified temp register to the destination register with normal flag checks
 */
STATIC_INLINE void helper_copy_word_with_flagcheck(comp_tmp_reg* tmpreg)
{
    comp_macroblock_push_copy_register_word(
			tmpreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			tmpreg->mapped_reg_num);

    comp_macroblock_push_check_word_register(
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num);

    	helper_check_nz_clear_cv_flags();
}

/*
 * Copy a result byte from the specified temp register to the destination register with normal flag checks
 */
STATIC_INLINE void helper_copy_byte_with_flagcheck(comp_tmp_reg* tmpreg)
{
    comp_macroblock_push_copy_register_byte(
			tmpreg->reg_usage_mapping,
			output_dep,
			dest_reg->mapped_reg_num,
			tmpreg->mapped_reg_num);

    comp_macroblock_push_check_byte_register(
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num);

    	helper_check_nz_clear_cv_flags();
}

/**
 * Before an arithmetic operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest word.
 * Note: Must be in pair with helper_post_word() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 * Returns:
 *    allocated temporary register with the shifted data
 */
STATIC_INLINE comp_tmp_reg* helper_pre_word(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num,
			16, 0, 15, FALSE);

	return tmpreg;
}

/**
 * Before an arithmetic operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest word.
 * Lowest 16 bits are filled up with 1 to carry over the C flag in
 * the arithmetic operations. (See ADDX implementations.)
 * Note: Must be in pair with helper_post_word() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 * Returns:
 *    allocated temporary register with the shifted data
 */
STATIC_INLINE comp_tmp_reg* helper_pre_word_filled(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Fill up the whole register with 1 bits
	comp_macroblock_push_load_register_long(
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			-1);

	//Insert the shifted data
	comp_macroblock_push_rotate_and_copy_bits(
			regsin | tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num,
			16, 0, 15, FALSE);

	return tmpreg;
}

/**
 * Before an arithmetic operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest word.
 * Lowest 16 bits are filled up with 1 to carry over the C flag in
 * the arithmetic operations. (See ADDX implementations.)
 * Note: This function does not allocate a temporary register for the output.
 *       Usually this function call is in pair with helper_post_word_no_free() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - mapped input register for the copy
 * Returns:
 *    allocated temporary register with the shifted data
 */
STATIC_INLINE void helper_pre_word_filled_noalloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped)
{
	//Insert the shifted data
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			regsin,
			input_reg_mapped,
			input_reg_mapped,
			16, 0, 15, FALSE);

	//Fill up the rest of the register with 1 bits
	comp_macroblock_push_or_low_register_imm(
			regsin,
			regsin,
			input_reg_mapped,
			input_reg_mapped,
			0xffff);
}

/**
 * Before an arithmetic operation with a word specified register is
 * shifted up the highest word.
 * Note: This function does not allocate a temporary register for the output.
 *       Usually this function call is in pair with helper_post_word_no_free() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - mapped input/output register for the copy
 */
STATIC_INLINE void helper_pre_word_no_alloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped)
{
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			regsin,
			input_reg_mapped,
			input_reg_mapped,
			16, 0, 15, FALSE);
}

/**
 * Before a rotate operation with a word sized data allocate
 * a temp register and copy the data from the source register into
 * it twice: copy the low half word and shifted up the same to the highest word.
 * Note: the allocated temp register must be free'd after use.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 */
STATIC_INLINE comp_tmp_reg* helper_prepare_word_shift(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_copy_register_long(
			regsin,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num);

	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			16, 0, 15, FALSE);

	return tmpreg;
}

/**
 * Before a rotate operation with a word sized data copy the data from the source register into
 * the same register twice: copy the low half word and shifted up the same to the highest word.
 * Note: This function does not allocate a temporary register for the output.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 */
STATIC_INLINE void helper_prepare_word_shift_no_alloc(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			regsin,
			regsin,
			input_reg->mapped_reg_num,
			input_reg->mapped_reg_num,
			16, 0, 15, FALSE);
}

/**
 * Before a rotate operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it twice: copy the lowest byte and shifted up the same to the highest byte.
 * Note: the allocated temp register must be free'd after use.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 */
STATIC_INLINE comp_tmp_reg* helper_prepare_byte_shift_left(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_copy_register_long(
			regsin,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num);

	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			24, 0, 7, FALSE);

	return tmpreg;
}

/**
 * Before a rotate operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it twice: copy the lowest byte and shifted up the same to the second lowest byte.
 * Note: the allocated temp register must be free'd after use.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 */
STATIC_INLINE comp_tmp_reg* helper_prepare_byte_shift_right(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_copy_register_long(
			regsin,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num);

	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			8, 16, 23, FALSE);

	return tmpreg;
}

/**
 * After an arithmetic operation shift back the result from the highest word
 * to the normal position, insert into the destination register and free temp
 * register.
 * Note: Must be in pair with helper_pre_word() function.
 * Parameters:
 *    regsout - input register dependency
 *    tmpreg - previously allocated temporary register number with the operation result
 *    output_reg - input register for the copy
 */
STATIC_INLINE void helper_post_word(uae_u64 regsout, comp_tmp_reg* tmpreg, comp_tmp_reg* output_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			regsout,
			output_reg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			16, 16, 31, FALSE);

	comp_free_temp_register(tmpreg);
}

/**
 * After an arithmetic operation shift back the result from the highest word
 * to the normal position.
 * Note: This function does not free up the specified register.
 *       Usually this function is in pair with helper_pre_word_noalloc() function.
 * Parameters:
 *    regsout - input/output register dependency
 *    output_reg - input/output register for the copy
 */
STATIC_INLINE void helper_post_word_no_free(uae_u64 regsout, comp_tmp_reg* output_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			regsout,
			regsout,
			output_reg->mapped_reg_num,
			output_reg->mapped_reg_num,
			16, 16, 31, FALSE);
}

/**
 * Before an arithmetic operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest byte.
 * Note: Must be in pair with helper_post_byte() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 * Returns:
 *    allocated temporary register with the shifted data
 */
STATIC_INLINE comp_tmp_reg* helper_pre_byte(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	comp_macroblock_push_rotate_and_mask_bits(
			regsin | tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num,
			24, 0, 7, FALSE);

	return tmpreg;
}

/**
 * Before an arithmetic operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest byte.
 * Lowest 24 bits are filled up with 1 to carry over the C flag in
 * the arithmetic operations. (See ADDX implementations.)
 * Note: Must be in pair with helper_post_byte() function.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg - input register for the copy
 * Returns:
 *    allocated temporary register with the shifted data
 */
STATIC_INLINE comp_tmp_reg* helper_pre_byte_filled(uae_u64 regsin, comp_tmp_reg* input_reg)
{
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Fill up the whole register with 1 bits
	comp_macroblock_push_load_register_long(
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			-1);

	//Insert the shifted data
	comp_macroblock_push_rotate_and_copy_bits(
			regsin | tmpreg->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			input_reg->mapped_reg_num,
			24, 0, 7, FALSE);

	return tmpreg;
}

/**
 * Before an arithmetic operation with a byte sized data allocate
 * a temp register and copy the data from the source register into
 * it shifted up to the highest byte.
 * Lowest 24 bits are filled up with 1 to carry over the C flag in
 * the arithmetic operations. (See ADDX implementations.)
 * Note: This function does not allocate a temporary register for the output.
 *       Usually this function call is in pair with helper_post_byte_no_free() function.
 *       This function uses the special temp (R0) register.
 * Parameters:
 *    regsin - input register dependency
 *    input_reg_mapped - mapped input/output register for the copy
 */
STATIC_INLINE void helper_pre_byte_filled_noalloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped)
{
	//Fill up the whole spec temp register with 1 bits
	comp_macroblock_push_load_register_long(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			-1);

	//Insert the shifted data
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			regsin,
			input_reg_mapped,
			input_reg_mapped,
			24, 0, 7, FALSE);

	//Insert 1 bits to the rest of the longword
	comp_macroblock_push_rotate_and_copy_bits(
			regsin | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			regsin,
			input_reg_mapped,
			PPCR_SPECTMP_MAPPED,
			0, 8, 31, FALSE);
}

/**
 * Before an arithmetic operation with a byte specified register is
 * shifted up the highest word.
 * Note: This function does not allocate a temporary register for the output.
 *       Usually this function call is in pair with helper_post_byte_no_free() function.
 * Parameters:
 *    regsin - input/output register dependency
 *    input_reg_mapped - mapped input/output register for the copy
 */
STATIC_INLINE void helper_pre_byte_no_alloc(uae_u64 regsin, comp_ppc_reg input_reg_mapped)
{
	comp_macroblock_push_rotate_and_mask_bits(
			regsin,
			regsin,
			input_reg_mapped,
			input_reg_mapped,
			24, 0, 7, FALSE);
}

/**
 * After an arithmetic operation shift back the result from the highest byte
 * to the normal position, insert into the destination register and free temp
 * register.
 * Note: Must be in pair with helper_pre_byte() function.
 * Parameters:
 *    regsout - output register dependency
 *    tmpreg - previously allocated temporary register number with the operation result
 *    output_reg - input register for the copy
 */
STATIC_INLINE void helper_post_byte(uae_u64 regsout, comp_tmp_reg* tmpreg, comp_tmp_reg* output_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			tmpreg->reg_usage_mapping,
			regsout,
			output_reg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			8, 24, 31, FALSE);

	comp_free_temp_register(tmpreg);
}

/**
 * After an arithmetic operation shift back the result from the highest word
 * to the normal position.
 * Note: This function does not free up the specified register.
 *       Usually this function is in pair with helper_pre_byte_noalloc() function.
 * Parameters:
 *    regsout - input/output register dependency
 *    output_reg - input/output register for the copy
 */
STATIC_INLINE void helper_post_byte_no_free(uae_u64 regsout, comp_tmp_reg* output_reg)
{
	comp_macroblock_push_rotate_and_copy_bits(
			regsout,
			regsout,
			output_reg->mapped_reg_num,
			output_reg->mapped_reg_num,
			8, 24, 31, FALSE);
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
STATIC_INLINE void helper_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, comp_tmp_reg* output_mem_reg, comp_tmp_reg* base_reg, uae_u32 base_address)
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
		comp_tmp_reg* scaled_index_reg = helper_calculate_complex_index(ext);

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
					output_mem_reg->mapped_reg_num,
					base_address);

			//Add the index * scale to the output register
			comp_macroblock_push_add(
					regsout | scaled_index_reg->reg_usage_mapping,
					regsout,
					output_mem_reg->mapped_reg_num,
					output_mem_reg->mapped_reg_num,
					scaled_index_reg->mapped_reg_num);
		} else {
			//No base address

			//Add the displacement to the output only if it is non-zero
			if (displacement)
			{
				comp_macroblock_push_add_register_imm(
						regsin,
						regsout,
						output_mem_reg->mapped_reg_num,
						base_reg->mapped_reg_num,
						displacement);

				//Add the index * scale to the output register
				comp_macroblock_push_add(
						regsout | scaled_index_reg->reg_usage_mapping,
						regsout,
						output_mem_reg->mapped_reg_num,
						output_mem_reg->mapped_reg_num,
						scaled_index_reg->mapped_reg_num);
			} else {
				//There is no displacement: add base and index * scale to the output register
				//Add the index * scale to the output register
				comp_macroblock_push_add(
						regsin | scaled_index_reg->reg_usage_mapping,
						regsout,
						output_mem_reg->mapped_reg_num,
						base_reg->mapped_reg_num,
						scaled_index_reg->mapped_reg_num);
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
STATIC_INLINE void helper_complete_complex_addressing(uae_u64 regsin, uae_u64 regsout, const cpu_history* history, comp_tmp_reg* output_mem_reg, comp_tmp_reg* base_reg, uae_u32 base_address, uae_u16 ext)
{
	uae_u32 base_displacement = 0;
	uae_u32 outer_displacement = 0;
	int memory_indirect = FALSE;
	int indexing_enabled = FALSE;
	int preindex = TRUE;

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
		if (base_reg == NULL)
		{
			//Not specified, base address is used: summarize BD and base address
			base_address += base_displacement;

			//Load it to the output register
			comp_macroblock_push_load_register_long(
					regsout,
					output_mem_reg->mapped_reg_num,
					base_address);
		} else {
			//Base register is specified: add base displacement to it

			//Add lower half word of BD to the base register
			comp_macroblock_push_add_register_imm(
					regsin,
					regsout,
					output_mem_reg->mapped_reg_num,
					base_reg->mapped_reg_num,
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
						output_mem_reg->mapped_reg_num,
						output_mem_reg->mapped_reg_num,
						((base_displacement >> 16) + (base_displacement & 0x8000 ? 1 : 0)) & 0xffff);
			}
		}
	} else {
		//Base register is suppressed, we still need to initialize the output register
		//TODO: remove the unnecessary initialization of the output register
		comp_macroblock_push_load_register_long(
				regsout,
				output_mem_reg->mapped_reg_num,
				base_displacement);
	}

	//Is the indexing enabled? (Bit is set if suppressed)
	indexing_enabled = ((ext & (1 << 6)) == 0);

	if (indexing_enabled)
	{
		//Indexing enabled
		switch (ext & 7)
		{
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
			//No memory indirect action
			memory_indirect = FALSE;
		}

		//Is pre-indexing enabled?
		if (preindex)
		{
			//Yes, add the index register to the output
			comp_tmp_reg* scaled_index_reg = helper_calculate_complex_index(ext);

			//Add the index * scale to the output register
			comp_macroblock_push_add(
					regsout | scaled_index_reg->reg_usage_mapping,
					regsout,
					output_mem_reg->mapped_reg_num,
					output_mem_reg->mapped_reg_num,
					scaled_index_reg->mapped_reg_num);

			comp_free_temp_register(scaled_index_reg);
		}
	} else {
		//Indexing suppressed
		switch (ext & 7)
		{
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
			//No memory indirect action
			memory_indirect = FALSE;
			break;
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
					output_mem_reg->mapped_reg_num,
					output_mem_reg->mapped_reg_num,
					4);
		} else {
			//Get memory address into the target temp register
			comp_macroblock_push_map_physical_mem(
					regsout,
					regsout,
					output_mem_reg->mapped_reg_num,
					output_mem_reg->mapped_reg_num);
		}
	}

	//Is indexing and post-indexing enabled (not pre-indexing)?
	if (indexing_enabled && !preindex)
	{
		//Calculate scaled index register
		comp_tmp_reg* scaled_index_reg = helper_calculate_complex_index(ext);

		//Add the index * scale to the output register
		comp_macroblock_push_add(
				regsout | scaled_index_reg->reg_usage_mapping,
				regsout,
				output_mem_reg->mapped_reg_num,
				output_mem_reg->mapped_reg_num,
				scaled_index_reg->mapped_reg_num);

		comp_free_temp_register(scaled_index_reg);
	}

	//Is there any outer displacement?
	if (outer_displacement != 0)
	{
		//Add lower half word of OD to the output register
		comp_macroblock_push_add_register_imm(
				regsout,
				regsout,
				output_mem_reg->mapped_reg_num,
				output_mem_reg->mapped_reg_num,
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
					output_mem_reg->mapped_reg_num,
					output_mem_reg->mapped_reg_num,
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
STATIC_INLINE comp_tmp_reg* helper_calculate_complex_index(uae_u16 ext)
{
	uae_u64 index_reg_dep;

	//Get the number of the register from the extension word
	uae_u8 index_reg_num = (ext >> 12) & 7;

	//Get the real register number: if bit 15 == 1 then address reg else data reg
	if (ext & (1 << 15))
	{
		index_reg_dep = COMP_COMPILER_MACROBLOCK_REG_AX(index_reg_num);
		index_reg_num = COMP_COMPILER_REGS_ADDRREG(index_reg_num);
	} else {
		index_reg_dep = COMP_COMPILER_MACROBLOCK_REG_DX(index_reg_num);
		index_reg_num = COMP_COMPILER_REGS_DATAREG(index_reg_num);
	}

	//Map register
	comp_tmp_reg* index_reg = comp_map_temp_register(index_reg_num, TRUE, FALSE);

	//Allocate temporary register for manipulating the content of the index register
	comp_tmp_reg* scaled_index_reg = helper_allocate_tmp_reg();

	//Is the index register word sized?
	if (ext & (1 << 11))
	{
		//TODO: this copy is not necessary, but we must preserve the content of the original register, because it can be written back later on
		//Long sized: simply copy to the temp register
		comp_macroblock_push_copy_register_long(
				index_reg_dep,
				scaled_index_reg->reg_usage_mapping,
				scaled_index_reg->mapped_reg_num,
				index_reg->mapped_reg_num);
	} else {
		//Word sized: extend to long sized to the temp register
		comp_macroblock_push_copy_register_word_extended(
				index_reg_dep,
				scaled_index_reg->reg_usage_mapping,
				scaled_index_reg->mapped_reg_num,
				index_reg->mapped_reg_num,
				FALSE);
	}

	//Get scaling
	int scaling = (ext >> 9) & 3;

	//Scale it if necessary
	if (scaling != 0)
	{
		//Scale copied index register by 2, 4 or 8
		comp_macroblock_push_rotate_and_mask_bits(
				scaled_index_reg->reg_usage_mapping,
				scaled_index_reg->reg_usage_mapping,
				scaled_index_reg->mapped_reg_num,
				scaled_index_reg->mapped_reg_num,
				scaling,
				0, 31 - scaling, 0);
	}

	return scaled_index_reg;
}

/**
 * Allocate temporary register and read data into it from source memory address
 * Parameters:
 *    regsin - input register dependency
 *    history - pointer to the execution history
 *    target_reg - target memory address in register
 *    size - size of the operation (1,2 or 4)
 *    preservedestreg - if TRUE then the destination memory adddress register is saved to a store slot and restored after the memory reading
 * Returns:
 *    allocated temporary register for output
 */
STATIC_INLINE comp_tmp_reg* helper_read_memory(uae_u64 regsin, const cpu_history* history, comp_tmp_reg* target_reg, uae_u8 size, BOOL preservedestreg)
{
	return helper_read_memory_mapped(regsin, history, target_reg->mapped_reg_num, size, preservedestreg);
}

/**
 * Allocate temporary register and read data into it from source memory address
 * Parameters:
 *    regsin - input register dependency
 *    history - pointer to the execution history
 *    target_reg_mapped - target memory address in a mapped register
 *    size - size of the operation (1,2 or 4)
 *    preservedestreg - if TRUE then the destination memory adddress register is saved to a store slot and restored after the memory reading
 * Returns:
 *    allocated temporary register for output
 */
STATIC_INLINE comp_tmp_reg* helper_read_memory_mapped(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, uae_u8 size, BOOL preservedestreg)
{
	int specread;
	comp_tmp_reg* tmpreg;

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
		if (preservedestreg)
		{
			//Store the destination register temporarily
			comp_macroblock_push_save_register_to_context(
					input_dep | dest_mem_addrreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					dest_mem_addrreg->mapped_reg_num);
		}

		//Special memory access, result returned in R3
		comp_macroblock_push_load_memory_spec(
				regsin,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				target_reg_mapped,
				PPCR_PARAM1_MAPPED,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_reg = NULL;
		dest_reg = NULL;
		src_mem_addrreg = NULL;

		//Allocate temporary register for the result, we prefer the second parameter register
		tmpreg = helper_allocate_preferred_tmp_reg(PPCR_PARAM2_MAPPED);

		if (preservedestreg)
		{
			//Reallocate temporary register for the destination memory address
			//We prefer the first parameter register
			dest_mem_addrreg = helper_allocate_preferred_tmp_reg(PPCR_PARAM1_MAPPED);
		} else {
			dest_mem_addrreg = NULL;
		}

		//If the new temp register is not R3 then copy
		//the previous result into it (but we tweaked the
		//temporary register mapping already, so the allocated
		//register will be R3 always)
		if (tmpreg->mapped_reg_num.r != PPCR_PARAM1)
		{
			comp_macroblock_push_copy_register_long(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg->mapped_reg_num,
					PPCR_PARAM1_MAPPED);
		}

		if (preservedestreg)
		{
			//Restore the destination address register
			comp_macroblock_push_restore_register_from_context(
					dest_mem_addrreg->reg_usage_mapping,
					dest_mem_addrreg->mapped_reg_num);
		}
	}
	else
	{
		//Normal memory access
		tmpreg = helper_allocate_tmp_reg();
		comp_tmp_reg* tmpregaddr = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				src_mem_addrreg == NULL ? input_dep : src_mem_addrreg->reg_usage_mapping,
				tmpregaddr->reg_usage_mapping,
				tmpregaddr->mapped_reg_num,
				target_reg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_load_memory_long(
					tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 2:
			comp_macroblock_push_load_memory_word(
					tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 1:
			comp_macroblock_push_load_memory_byte(
					tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					tmpreg->mapped_reg_num,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		}

		comp_free_temp_register(tmpregaddr);
	}

	return tmpreg;
}

/**
 * Write data from register to the destination address in memory and free input register if specified pre-mapped.
 * Parameters:
 *    regsin - input register dependency
 *    history - pointer to the execution history
 *    target_reg - target memory address in a register
 *    input_reg - source register to be written into memory (or NULL)
 *    size - size of the operation (1,2 or 4)
 */
STATIC_INLINE void helper_write_memory(uae_u64 regsin, const cpu_history* history, comp_tmp_reg* target_reg, comp_tmp_reg* input_reg, uae_u8 size)
{
	helper_write_memory_mapped(regsin, history, target_reg->mapped_reg_num, input_reg, size);
}

/**
 * Write data from register to the destination address in memory and free input register if specified pre-mapped.
 * Parameters:
 *    regsin - input register dependency
 *    history - pointer to the execution history
 *    target_reg_mapped - target memory address in a mapped register
 *    input_reg - source register to be written into memory
 *    size - size of the operation (1,2 or 4)
 */
STATIC_INLINE void helper_write_memory_mapped(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, comp_tmp_reg* input_reg, uae_u8 size)
{
	BOOL specwrite = helper_write_memory_mapped_no_free(regsin, history, target_reg_mapped, input_reg->mapped_reg_num, size);

	//Free the input register if it was a normal write (not free'ed automatically)
	if (!specwrite)
	{
		comp_free_temp_register(input_reg);
	}
}

/**
 * Write data from register to the destination address in memory.
 * Parameters:
 *    regsin - input register dependency
 *    history - pointer to the execution history
 *    target_reg_mapped - target memory address in a mapped register
 *    input_reg_mapped - mapped source register to be written into memory
 *    size - size of the operation (1,2 or 4)
 * Returns:
 *    TRUE if the write operation was a special write, FALSE otherwise.
 */
STATIC_INLINE BOOL helper_write_memory_mapped_no_free(uae_u64 regsin, const cpu_history* history, comp_ppc_reg target_reg_mapped, comp_ppc_reg input_reg_mapped, uae_u8 size)
{
	int specwrite;
	comp_tmp_reg* tmpregaddr = NULL;

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
				regsin,
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				input_reg_mapped,
				target_reg_mapped,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		src_mem_addrreg = NULL;
		dest_mem_addrreg = NULL;
	}
	else
	{
		//Normal memory access

		//(Re)allocate address calculation temporary register
		tmpregaddr = helper_allocate_tmp_reg();

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				regsin,
				tmpregaddr->reg_usage_mapping,
				tmpregaddr->mapped_reg_num,
				target_reg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_save_memory_long(
					regsin | tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					input_reg_mapped,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 2:
			comp_macroblock_push_save_memory_word(
					regsin | tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					input_reg_mapped,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		case 1:
			comp_macroblock_push_save_memory_byte(
					regsin | tmpregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					input_reg_mapped,
					tmpregaddr->mapped_reg_num,
					0);
			break;
		}
		comp_free_temp_register(tmpregaddr);
	}

	return specwrite;
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
STATIC_INLINE void helper_check_result_set_flags(uae_u64 regsin, comp_ppc_reg input_reg, uae_u8 size)
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
 *    size - operation size (1 or 2 bytes)
 */
STATIC_INLINE void helper_copy_result_set_flags(comp_tmp_reg* src_reg, uae_u8 size)
{
    //Copy result and set flags by operation size
	switch (size)
	{
	case 2:
		helper_copy_word_with_flagcheck(src_reg);
		break;
	case 1:
		helper_copy_byte_with_flagcheck(src_reg);
		break;
	default:
		write_log("Error: wrong operation size for copy result and set flags function\n");
		abort();
	}
}

/**
 * Tests the bit specified by the bitnum immediate in the input register and sets
 * Z flag in the emulated flag register according to the result.
 * Parameters:
 *    regsin - input register dependency
 *    bitnum - bit number for the test
 *    input_reg - mapped input register
 */
STATIC_INLINE void helper_test_bit_register_imm(uae_u64 regsin, int bitnum, comp_tmp_reg* input_reg)
{
	if (bitnum < 16)
	{
		//Bit is in the lower half word
		comp_macroblock_push_and_low_register_imm(
				regsin,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				input_reg->mapped_reg_num,
				1 << bitnum);
	} else {
		//Bit is in the higher half word
		comp_macroblock_push_and_high_register_imm(
				regsin,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				input_reg->mapped_reg_num,
				1 << (bitnum - 16));
	}

	//Save Z flag
	helper_check_z_flag();
}

/**
 * Tests the bit specified by the bitnum register in the input register and sets
 * Z flag in the emulated flag register according to the result.
 * Parameters:
 *    regsbit - bitnum register dependency
 *    regsin - input register dependency
 *    bitnum_reg - bit number for the test
 *    input_reg - mapped input register
 *    modulo - maximum number of bits for the testing
 * Returns:
 *    Temporary register number with the bit mask in it.
 */
STATIC_INLINE comp_tmp_reg* helper_test_bit_register_register(uae_u64 regsbit, uae_u64 regsin, comp_tmp_reg* bitnum_reg, comp_tmp_reg* input_reg, int modulo)
{
	comp_tmp_reg* masktempreg = helper_allocate_tmp_reg_with_init(1);
	comp_tmp_reg* srctmpreg;
	uae_u64 srctmpreg_dep;
	comp_ppc_reg mapped_reg;

	//If modulo other than 32 for the input register then apply it
	//Modulo 32 is automatically applied by the rotation
	if (modulo != 32)
	{
		//Allocate source register for the shifting
		srctmpreg = helper_allocate_tmp_reg();
		srctmpreg_dep = srctmpreg->reg_usage_mapping;

		//Copy bit number from source register and apply modulo
		comp_macroblock_push_and_low_register_imm(
				regsbit,
				srctmpreg->reg_usage_mapping,
				srctmpreg->mapped_reg_num,
				bitnum_reg->mapped_reg_num,
				modulo - 1);

		mapped_reg = srctmpreg->mapped_reg_num;
	} else {
		//Modulo is 32, register is not copied, let's copy the source register number
		srctmpreg = NULL;
		mapped_reg = bitnum_reg->mapped_reg_num;
		srctmpreg_dep = regsbit;
	}

	//Rotate the masking bit to the right position
	comp_macroblock_push_rotate_and_mask_bits_register(
			srctmpreg_dep | masktempreg->reg_usage_mapping,
			masktempreg->reg_usage_mapping,
			masktempreg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			mapped_reg,
			0, 31, FALSE);

	//Check bit in output register
	comp_macroblock_push_and_register_register(
			regsin | masktempreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			PPCR_SPECTMP_MAPPED,
			input_reg->mapped_reg_num,
			masktempreg->mapped_reg_num,
			TRUE);

	//Free source temp register
	if (srctmpreg)
	{
		helper_free_tmp_reg(srctmpreg);
	}

	//Save Z flag
	helper_check_z_flag();

	//Return temp register number for the mask
	return masktempreg;
}

/**
 * Converts the CCR register format to internal flag register format as a static immediate.
 * The returned immediate can be directly loaded into the PPCR_FLAGS register, the flags
 * will be set the same state as it was in the input CCR formatted constant.
 */
STATIC_INLINE uae_u32 helper_convert_ccr_to_internal_static(uae_u8 ccr)
{
	return (ccr & 1 ? 1L << 21 : 0) |
			(ccr & 2 ? 1L << 22 : 0) |
			(ccr & 4 ? 1L << 29 : 0) |
			(ccr & 8 ? 1L << 31 : 0) |
			(ccr & 16 ? 1L << 5 : 0);
}

/**
 * Calculate the flag dependency based on the CCR register.
 */
STATIC_INLINE uae_u64 helper_calculate_ccr_flag_dependency(uae_u8 ccr)
{
	return (ccr & 1 ? COMP_COMPILER_MACROBLOCK_REG_FLAGC : 0) |
			(ccr & 2 ? COMP_COMPILER_MACROBLOCK_REG_FLAGV : 0) |
			(ccr & 4 ? COMP_COMPILER_MACROBLOCK_REG_FLAGZ : 0) |
			(ccr & 8 ? COMP_COMPILER_MACROBLOCK_REG_FLAGN : 0) |
			(ccr & 16 ? COMP_COMPILER_MACROBLOCK_REG_FLAGX : 0);
}

/**
 * Prepares the flags and the mask for all register accessing bit field instructions.
 * Parameters:
 *    extword - extension word for the instruction
 *    returned_mask_reg - if it is not NULL then it points to a variable for returning the mask register
 *    returned_summary_offset_reg - if it is not NULL then it points to a variable for returning the summary of the offset and the mask length in a temporary register
 * Note: the returned temporary registers must be free'd by the caller.
 */
STATIC_INLINE void helper_bit_field_reg_opertion_flag_test(signed int extword, comp_tmp_reg** returned_mask_reg, comp_tmp_reg** returned_summary_offset_reg)
{
	if (returned_summary_offset_reg != NULL)
	{
		//We have to return the offset+width in a register
		*returned_summary_offset_reg = helper_allocate_tmp_reg();
	}

	//TODO: the special case can be handled a more optimized way when both the offset and the width is specified in the extension word
	//Get the offset from the extension word
	comp_tmp_reg* bit_field_offset = helper_extract_bitfield_offset(extword);

	//Allocate temporary register for the result
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Rotate source data to the top using offset
	comp_macroblock_push_logic_shift_left_register_register(
			output_dep | bit_field_offset->reg_usage_mapping,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			dest_reg->mapped_reg_num,
			bit_field_offset->mapped_reg_num,
			FALSE);

	//Create mask for the field and also get the summary of the offset and the width in the offset_reg
	comp_tmp_reg* bit_field_mask = helper_create_bitfield_mask(
			extword,
			returned_summary_offset_reg != NULL ? *returned_summary_offset_reg : NULL,
			bit_field_offset->reg_usage_mapping, bit_field_offset->mapped_reg_num,
			NULL);

	//Mask out bits from the target register, all we need is the flags
	comp_macroblock_push_and_register_register(
			tmpreg->reg_usage_mapping | bit_field_mask->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			bit_field_mask->mapped_reg_num,
			TRUE);

	//Temp register is not needed anymore
	helper_free_tmp_reg(tmpreg);

	//Save N and Z flags, clear C and V
	helper_check_nz_clear_cv_flags();

	//Do we need to return the mask?
	if (returned_mask_reg != NULL)
	{
		//Rotate mask to the right position using the offset for the bit field operation
		comp_macroblock_push_logic_shift_right_register_register(
				bit_field_mask->reg_usage_mapping | bit_field_offset->reg_usage_mapping,
				bit_field_mask->reg_usage_mapping,
				bit_field_mask->mapped_reg_num,
				bit_field_mask->mapped_reg_num,
				bit_field_offset->mapped_reg_num,
				FALSE);

		//Return mask register
		*returned_mask_reg = bit_field_mask;
	} else {
		//No need to return the mask: free up temp register
		helper_free_tmp_reg(bit_field_mask);
	}

	//Free up offset register
	helper_free_tmp_reg(bit_field_offset);
}

/**
 * Prepares the flags and the mask for all memory accessing bit field instructions.
 * Parameters:
 *    history - pointer to the execution history
 *    extword - extension word for the instruction
 *    returned_data_reg_combined - if not NULL then it points to a variable for returning the data register as combined from memory and shifted to the top of the longword
 *    returned_data_reg_high - if not NULL then it points to a variable for returning the higher 32 bit data register
 *    returned_data_reg_low - if not NULL then it points to a variable for returning the lower 8 bit data register
 *    returned_mask_reg_high - if not NULL then it points to a variable for returning the higher 32 bit mask register
 *    returned_mask_reg_low - if not NULL then it points to a variable for returning the lower 8 bit mask register
 *    returned_summary_offset_reg - if not NULL then it points to a variable for returning the summary of the offset and the mask length in a temporary register
 *    returned_complement_width - if not NULL then it points to a variable for returning the complement width (32-width) in a temporary register
 *    return_adjusted_dest_address - if TRUE then the offset-adjusted destination address will be returned in non-volatile register #0, otherwise non-volatile registers are restored at the end of the function.
 *    return_offset - if TRUE then the full offset will be returned through non-volatile register #0, otherwise non-volatile registers are restored at the end of the function. Must be free'd by the caller. (See BFFFO2MEM implementation.)
 *    Notes:
 *       This function call must match with helper_bit_field_mem_save() function call, if any registers were returned. That function will release the allocated
 *       registers which are provided as parameter to that function and clean up the non-volatile registers. The other returned temporary registers must be free'd by the caller.
 *
 *       When adjusted destination address is returned then non-volatile register #1 is also preserved,
 *       so the memory saving function can make use of it.
 *
 *       When offset bytes are returned then the caller function must restore non-volatile register #0 from the context.
 *
 *       Parameters return_adjusted_dest_address and return_offset_bytes are mutually exclusive.
 *
 *       I am sorry about this mess, this is very bad design, but I ran out of ideas.
 */
STATIC_INLINE void helper_bit_field_mem_opertion_flag_test(const cpu_history* history, signed int extword, comp_tmp_reg** returned_data_reg_combined, comp_tmp_reg** returned_data_reg_high, comp_tmp_reg** returned_data_reg_low, comp_tmp_reg** returned_mask_reg_high, comp_tmp_reg** returned_mask_reg_low, comp_tmp_reg** returned_summary_offset_reg, comp_tmp_reg** returned_complement_width, BOOL return_adjusted_dest_address, BOOL return_offset)
{
	comp_tmp_reg* data_reg_low;
	comp_tmp_reg* data_reg_high;

	//TODO: the special case can be handled a more optimized way when both the offset and the width is specified in the extension word
	//Get the offset from the extension word
	comp_tmp_reg* bit_field_offset = helper_extract_bitfield_offset(extword);

	//TODO: preserving of data in non-volatile registers is not necessary when direct memory access is used
	//Save non-volatile register #0 and #1 to the context
	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);

	comp_macroblock_push_save_register_to_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL1_MAPPED);

	//Calculate byte offset in memory into non-volatile register #0
	comp_macroblock_push_arithmetic_shift_right_register_imm(
			bit_field_offset->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			bit_field_offset->mapped_reg_num,
			3, FALSE);

	if (return_offset)
	{
		//Full offset must be returned: save it to the context for now
		comp_macroblock_push_save_register_to_context(
				bit_field_offset->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				bit_field_offset->mapped_reg_num);
	}

	//Summarize original memory address and offset into non-volatile register #0
	comp_macroblock_push_add(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0 | dest_mem_addrreg->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			PPCR_TMP_NONVOL0_MAPPED,
			dest_mem_addrreg->mapped_reg_num);

	//Calculate remaining bits from the offset into non-volatile register #1
	comp_macroblock_push_and_low_register_imm(
			bit_field_offset->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			PPCR_TMP_NONVOL1_MAPPED,
			bit_field_offset->mapped_reg_num,
			7);

	//Free offset temp register
	helper_free_tmp_reg(bit_field_offset);

	//Read high 32 bit into a register from memory
	comp_tmp_reg* result_reg = helper_read_memory_mapped(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			history,
			PPCR_TMP_NONVOL0_MAPPED,
			4, FALSE);

	//TODO: if it would be possible to specify a target register to the helper then this step would not be needed
	//Save result into the temporary storage
	comp_macroblock_push_save_register_to_context(
			result_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			result_reg->mapped_reg_num);

	//Release result temp reg
	helper_free_tmp_reg(result_reg);

	//Calculate next address for lower 8 bit into spec temp reg (R0)
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_TMP_NONVOL0_MAPPED,
			4);

	//Read lower 8 bit into temp reg
	data_reg_low = helper_read_memory_mapped(
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			history,
			PPCR_SPECTMP_MAPPED,
			1, FALSE);

	//Reallocate higher data reg
	data_reg_high = helper_allocate_tmp_reg();

	//Restore previously read data
	comp_macroblock_push_restore_register_from_context(
			data_reg_high->reg_usage_mapping,
			data_reg_high->mapped_reg_num);

	//Allocate temporary register for the result
	comp_tmp_reg* tmpreg = helper_allocate_tmp_reg();

	//Rotate the high 32 bits to the left by the remaining offset bits, copy result into the temp reg
	comp_macroblock_push_logic_shift_left_register_register(
			data_reg_high->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			data_reg_high->mapped_reg_num,
			PPCR_TMP_NONVOL1_MAPPED,
			FALSE);

	//Calculate shifting for low bits: 8 - offset into spec temp (R0)
	comp_macroblock_push_sub_register_from_immediate(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			PPCR_TMP_NONVOL1_MAPPED,
			8);

	//Rotate the low 8 bits to the right by 8 minus the remaining offset bits, copy result into spec temp (R0)
	comp_macroblock_push_logic_shift_right_register_register(
			data_reg_low->reg_usage_mapping |COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			PPCR_SPECTMP_MAPPED,
			data_reg_low->mapped_reg_num,
			PPCR_SPECTMP_MAPPED,
			FALSE);

	//Put the final data together into temp reg
	comp_macroblock_push_or_register_register(
			tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
			tmpreg->reg_usage_mapping,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			PPCR_SPECTMP_MAPPED,
			FALSE);

	if (returned_data_reg_high != NULL)
	{
		//We have to return the data high 32 bit in a register
		*returned_data_reg_high = data_reg_high;
	} else {
		//Free data high register
		helper_free_tmp_reg(data_reg_high);
	}

	if (returned_data_reg_low != NULL)
	{
		//We have to return the data low 8 bit in a register
		*returned_data_reg_low = data_reg_low;
	} else {
		//Free data low register
		helper_free_tmp_reg(data_reg_low);
	}

	if (returned_summary_offset_reg != NULL)
	{
		//We have to return the offset+width in a register
		*returned_summary_offset_reg = helper_allocate_tmp_reg();
	}

	//Create mask for the field and also get the summary of the offset and the width in the offset_reg
	comp_tmp_reg* bit_field_mask = helper_create_bitfield_mask(
			extword,
			returned_summary_offset_reg != NULL ? *returned_summary_offset_reg : NULL,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			PPCR_TMP_NONVOL1_MAPPED,
			returned_complement_width != NULL ? returned_complement_width : NULL);

	//Mask out bits from the target register
	comp_macroblock_push_and_register_register(
			tmpreg->reg_usage_mapping | bit_field_mask->reg_usage_mapping,
			tmpreg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
			tmpreg->mapped_reg_num,
			tmpreg->mapped_reg_num,
			bit_field_mask->mapped_reg_num,
			TRUE);

	if (returned_data_reg_combined != NULL)
	{
		//We have to return the combined data register
		*returned_data_reg_combined = tmpreg;
	} else {
		//Temp register is not needed anymore
		helper_free_tmp_reg(tmpreg);
	}

	//Save N and Z flags, clear C and V
	helper_check_nz_clear_cv_flags();

	//Do we need to return the lower mask?
	if (returned_mask_reg_low != NULL)
	{
		//Calculate rotation steps into the spec temp
		comp_macroblock_push_sub_register_from_immediate(
				COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
				COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				PPCR_SPECTMP_MAPPED,
				PPCR_TMP_NONVOL1_MAPPED,
				8);

		//Allocate new temporary register for the result
		comp_tmp_reg* mask_reg_low = helper_allocate_tmp_reg();

		//Rotate mask to the final position and keep the lowest byte only
		comp_macroblock_push_logic_shift_left_register_register(
				bit_field_mask->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				mask_reg_low->reg_usage_mapping,
				mask_reg_low->mapped_reg_num,
				bit_field_mask->mapped_reg_num,
				PPCR_SPECTMP_MAPPED,
				FALSE);

		//Keep only the lowest 8 bit
		comp_macroblock_push_and_low_register_imm(
				mask_reg_low->reg_usage_mapping,
				mask_reg_low->reg_usage_mapping,
				mask_reg_low->mapped_reg_num,
				mask_reg_low->mapped_reg_num,
				0xff);

		*returned_mask_reg_low = mask_reg_low;
	}

	//Do we need to return the higher mask?
	if (returned_mask_reg_high != NULL)
	{
		//Rotate mask to the right position for the higher 32 bit using the offset for the bit field operation
		comp_macroblock_push_logic_shift_right_register_register(
				bit_field_mask->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
				bit_field_mask->reg_usage_mapping,
				bit_field_mask->mapped_reg_num,
				bit_field_mask->mapped_reg_num,
				PPCR_TMP_NONVOL1_MAPPED,
				FALSE);

		//Return mask register
		*returned_mask_reg_high = bit_field_mask;
	} else {
		//No need to return the mask: free up temp register
		helper_free_tmp_reg(bit_field_mask);
	}

	if (return_offset)
	{
		//Full offset must be returned: restore it from the context into non-volatile register #0
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);

		//Restore non-volatile register #1
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL1_MAPPED);

		//Non-volatile register #0 remained in the context, will be restored by the caller
	} else if (!return_adjusted_dest_address)
	{
		//No need to return the adjusted destination address: there will be no saving back
		//to the memory, we can restore the non-volatile registers
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL1_MAPPED);

		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);
	}
}

/**
 * Saves the result of a bit field operation into memory.
 * Parameters:
 *    history - pointer to the execution history
 *    data_reg_high - register with the low part of the bit field operation result in a register
 *    data_reg_low - register with the low part of the bit field operation result in a register
 *  Note:
 *    The function expects the target memory address in non-volatile register #0 and it also
 *    uses non-volatile register #1 for storing the data temporarily.
 *    All registers will be free'd, non-volatile registers will be restored from
 *    context.
 */
STATIC_INLINE void helper_bit_field_mem_save(const cpu_history* history, comp_tmp_reg* data_reg_high, comp_tmp_reg* data_reg_low)
{
	//Copy low data register to non-volatile #1 (otherwise it would be lost)
	comp_macroblock_push_copy_register_long(
			data_reg_low->reg_usage_mapping,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
			PPCR_TMP_NONVOL1_MAPPED,
			data_reg_low->mapped_reg_num);

	//Release low data register
	helper_free_tmp_reg(data_reg_low);

	//Write back high 32 bit data
	helper_write_memory_mapped(
			data_reg_high->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			history,
			PPCR_TMP_NONVOL0_MAPPED,
			data_reg_high,
			4);

	//Increment target address by 4 bytes
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			PPCR_TMP_NONVOL0_MAPPED,
			PPCR_TMP_NONVOL0_MAPPED,
			4);

	//Write back high 8 bit data
	helper_write_memory_mapped_no_free(
			COMP_COMPILER_MACROBLOCK_REG_NONVOL1 | COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
			history,
			PPCR_TMP_NONVOL0_MAPPED,
			PPCR_TMP_NONVOL1_MAPPED,
			1);

	//Restore non-volatile registers
	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL1_MAPPED);

	comp_macroblock_push_restore_register_from_context(
			COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			PPCR_TMP_NONVOL0_MAPPED);
}

/**
 * Extracts the offset from the extension word for a bit field instruction.
 * Parameters:
 *   extword - extension word for the instruction
 * Returns:
 *   the allocated temporary register with the offset.
 * Note: the returned temporary register must be free'ed by the caller.
 */
STATIC_INLINE comp_tmp_reg* helper_extract_bitfield_offset(signed int extword)
{
	comp_tmp_reg* bit_field_offset;

	//TODO: the special case can be handled in more optimized way when both the offset and the width is specified in the extension word
	//Is the Do bit set?
	if ((extword & (1 << 11)) == 0)
	{
		//Not set: the offset is coming from the extension word
		//load it into a new temp register
		bit_field_offset = helper_allocate_tmp_reg_with_init((extword >> 6) & 31);
	} else {
		//Set: the offset is coming from the register specified in the offset bits,
		//copy the offset into a new temp register and mask the relevant part only
		bit_field_offset = helper_allocate_tmp_reg();

		uae_u8 offset_reg_num = ((uae_u8) (extword >> 6) & 7);

		comp_macroblock_push_copy_register_long(
				COMP_COMPILER_MACROBLOCK_REG_DX(offset_reg_num),
				bit_field_offset->reg_usage_mapping,
				bit_field_offset->mapped_reg_num,
				comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(offset_reg_num), TRUE, FALSE)->mapped_reg_num);
	}

	return bit_field_offset;
}

/**
 * Create mask for bit field instructions and also provide the summary of the width and the offset
 * Parameters:
 *   extword - extension word for the instruction
 *   summary_offset_reg - target register for calculating the summary of the width and the offset or NULL
 *   bit_field_offset_dep - dependency of the offset register
 *   bit_field_offset_mapped - extracted offset in a mapped register
 *   returned_bit_field_complement_width - if it is not NULL then it points to a variable for returning the complement (subtracted from 32) field width in a temporary register
 * Returns:
 *   the allocated temporary register with the mask.
 * Note: the returned temporary registers must be free'ed by the caller.
 */
STATIC_INLINE comp_tmp_reg* helper_create_bitfield_mask(signed int extword, comp_tmp_reg* summary_offset_reg, uae_u64 bit_field_offset_dep, comp_ppc_reg bit_field_offset_mapped, comp_tmp_reg** returned_bit_field_complement_width)
{
	comp_tmp_reg* bit_field_mask;

	//Is the Dw bit set?
	if ((extword & (1 << 5)) == 0)
	{
		//Not set: the width is coming from the extension word
		//load it into a new temp register
		uae_u8 width = extword & 31;

		//Zero width is 32
		if (width == 0) width = 32;

		//Do we need to return the complement of the width?
		if (returned_bit_field_complement_width != NULL)
		{
			//Allocate a temporary register with the value
			*returned_bit_field_complement_width = helper_allocate_tmp_reg_with_init(32 - width);
		}

		//Calculate mask: shift up to the top by that many bits what is left after the width
		//and load it into a new temp register
		bit_field_mask = helper_allocate_tmp_reg_with_init(0xffffffffUL << (32 - width));

		if (summary_offset_reg != NULL)
		{
			//We need to return the summary of offset and width
			comp_macroblock_push_add_register_imm(
					bit_field_offset_dep,
					summary_offset_reg->reg_usage_mapping,
					summary_offset_reg->mapped_reg_num,
					bit_field_offset_mapped,
					width);
		}
	} else {
		//Set: the width is coming from the register specified in the width bits,
		//copy the width into a new temp register and mask the relevant part only
		comp_tmp_reg* bit_field_width = helper_allocate_tmp_reg();

		uae_u8 width_reg_num = extword & 7;

		comp_macroblock_push_and_low_register_imm(
				COMP_COMPILER_MACROBLOCK_REG_DX(width_reg_num),
				bit_field_width->reg_usage_mapping,
				bit_field_width->mapped_reg_num,
				comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(width_reg_num), TRUE, FALSE)->mapped_reg_num,
				31);

		//Zero width is 32
		//Subtract 1 from the width and if it flipped over to negative then
		//it was zero previously, in this case bit 5 will change from 0 to 1.
		//Spec temp (R0) register is used for the operation.
		comp_macroblock_push_add_register_imm(
				bit_field_width->reg_usage_mapping,
				COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				PPCR_SPECTMP_MAPPED,
				bit_field_width->mapped_reg_num,
				-1);

		//Insert bit 5 into the width register
		comp_macroblock_push_rotate_and_copy_bits(
				bit_field_width->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
				bit_field_width->reg_usage_mapping,
				bit_field_width->mapped_reg_num,
				PPCR_SPECTMP_MAPPED,
				0, 26, 26, FALSE);

		if (summary_offset_reg != NULL)
		{
			//We need to return the summary of offset and width
			comp_macroblock_push_add(
					bit_field_width->reg_usage_mapping | bit_field_offset_dep,
					summary_offset_reg->reg_usage_mapping,
					summary_offset_reg->mapped_reg_num,
					bit_field_offset_mapped,
					bit_field_width->mapped_reg_num);
		}

		//Create mask for the width: all bits are set
		bit_field_mask = helper_allocate_tmp_reg_with_init(0xffffffffUL);

		//Subtract width from 32 -> get the bits which are not part of the mask, can be cleared
		comp_macroblock_push_sub_register_from_immediate(
				bit_field_width->reg_usage_mapping,
				bit_field_width->reg_usage_mapping,
				bit_field_width->mapped_reg_num,
				bit_field_width->mapped_reg_num,
				32);

		//Shift mask up to the top by that many bits what is left after the width
		comp_macroblock_push_logic_shift_left_register_register(
				bit_field_mask->reg_usage_mapping | bit_field_width->reg_usage_mapping,
				bit_field_mask->reg_usage_mapping,
				bit_field_mask->mapped_reg_num,
				bit_field_mask->mapped_reg_num,
				bit_field_width->mapped_reg_num,
				FALSE);

		//Do we need to return the width?
		if (returned_bit_field_complement_width != NULL)
		{
			//Return the width register
			*returned_bit_field_complement_width = bit_field_width;
		} else {
			//Width register is not needed anymore
			helper_free_tmp_reg(bit_field_width);
		}
	}

	return bit_field_mask;
}

/**
 * Extract and map destination register for BFEXTU and BFEXTS bit field instructions.
 * Parameters:
 *   extword - extension word for the instruction
 *   returned_dependency - pointer to a long-long register where the dependency can be returned
 *   is_src_reg - if TRUE then the register is initialized using the emulated register, otherwise it will be saved back after modification
 * Returns:
 *   The mapped temporary register and the dependency via the sent variable.
 */
STATIC_INLINE comp_tmp_reg* helper_bit_field_extract_reg(signed int extword, uae_u64* returned_dependency, BOOL is_src_reg)
{
	int regnum = (extword >> 12) & 7;

	*returned_dependency = COMP_COMPILER_MACROBLOCK_REG_DX(regnum);
	return comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(regnum), is_src_reg, !is_src_reg);
}

/**
 * Copies one memory line (16 bytes) from the source address to the destination.
 * Source and destination registers will be aligned to 16 byte boundary before the copy operation.
 *
 * Parameters:
 *   history - pointer to the execution history
 *   local_src_dep - dependency for source register
 *   local_src_reg - mapped source register
 *   update_src - if TRUE then the source register is increased by 16, unchanged otherwise
 *   local_dest_dep - dependency for source register
 *   local_dest_reg - mapped source register
 *   update_dest - if TRUE then the source register is increased by 16, unchanged otherwise
 */
STATIC_INLINE void helper_mov16(const cpu_history* history, uae_u64 local_src_dep, comp_tmp_reg* local_src_reg, BOOL update_src, uae_u64 local_dest_dep, comp_tmp_reg* local_dest_reg, BOOL update_dest)
{
	BOOL spec = comp_is_spec_memory_read_long(history->pc, history->specmem) ||
				comp_is_spec_memory_write_long(history->pc, history->specmem);
	comp_tmp_reg* srcregaddr = NULL;
	comp_tmp_reg* destregaddr = NULL;
	int i;

	//Does this operation require special read/write?
	if (spec)
	{
		//Special memory access

		//Save non-volatile reg #0 and #1 to the context
		comp_macroblock_push_save_register_to_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);

		comp_macroblock_push_save_register_to_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL1_MAPPED);

		//Copy source address register into non-volatile register #0 and align it to 16 byte boundary
		comp_macroblock_push_rotate_and_mask_bits(
				local_src_dep,
				COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
				PPCR_TMP_NONVOL0_MAPPED,
				local_src_reg->mapped_reg_num,
				0, 0, 27, FALSE);

		//Copy destination address register into non-volatile register #1 and align it to 16 byte boundary
		comp_macroblock_push_rotate_and_mask_bits(
				local_dest_dep,
				COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
				PPCR_TMP_NONVOL1_MAPPED,
				local_dest_reg->mapped_reg_num,
				0, 0, 27, FALSE);
	} else {
		//Direct memory access

		//Allocate address calculation temporary registers
		comp_tmp_reg* srcregaddr = helper_allocate_tmp_reg();
		comp_tmp_reg* destregaddr = helper_allocate_tmp_reg();

		//Copy source address register into local temp register and align it to 16 byte boundary
		comp_macroblock_push_rotate_and_mask_bits(
				local_src_dep,
				srcregaddr->reg_usage_mapping,
				srcregaddr->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				0, 0, 27, FALSE);

		//Get physical source memory address into temp register
		comp_macroblock_push_map_physical_mem(
				srcregaddr->reg_usage_mapping,
				srcregaddr->reg_usage_mapping,
				srcregaddr->mapped_reg_num,
				srcregaddr->mapped_reg_num);

		//Copy destination address register into local temp register and align it to 16 byte boundary
		comp_macroblock_push_rotate_and_mask_bits(
				local_dest_dep,
				destregaddr->reg_usage_mapping,
				destregaddr->mapped_reg_num,
				local_dest_reg->mapped_reg_num,
				0, 0, 27, FALSE);

		//Get physical destination memory address into temp register
		comp_macroblock_push_map_physical_mem(
				destregaddr->reg_usage_mapping,
				destregaddr->reg_usage_mapping,
				destregaddr->mapped_reg_num,
				destregaddr->mapped_reg_num);
	}

	//Update source register if needed
	if (update_src)
	{
		comp_macroblock_push_add_register_imm(
				local_src_dep,
				local_src_dep,
				local_src_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				16);
	}

	//Update destination register if needed
	if (update_dest)
	{
		comp_macroblock_push_add_register_imm(
				local_dest_dep,
				local_dest_dep,
				local_dest_reg->mapped_reg_num,
				local_dest_reg->mapped_reg_num,
				16);
	}

	//Does this operation require special read/write?
	if (spec)
	{
		//Special memory access

		//Copy memory in an unrolled loop
		for(i = 0; i < 4; i++)
		{
			//Load longword into spec temp from memory address specified in non-volatile reg #0
			comp_macroblock_push_load_memory_spec(
					COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					PPCR_TMP_NONVOL0_MAPPED,
					PPCR_SPECTMP_MAPPED,
					4);

			//Save longword from spec temp into memory address specified in non-volatile reg #1
			comp_macroblock_push_save_memory_spec(
					COMP_COMPILER_MACROBLOCK_REG_NONVOL1 | COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					PPCR_SPECTMP_MAPPED,
					PPCR_TMP_NONVOL1_MAPPED,
					4);

			if (i < 3)
			{
				//This is not the last iteration yet: step to the next source and destination address
				comp_macroblock_push_add_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
						COMP_COMPILER_MACROBLOCK_REG_NONVOL0,
						PPCR_TMP_NONVOL0_MAPPED,
						PPCR_TMP_NONVOL0_MAPPED,
						4);

				comp_macroblock_push_add_register_imm(
						COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
						COMP_COMPILER_MACROBLOCK_REG_NONVOL1,
						PPCR_TMP_NONVOL1_MAPPED,
						PPCR_TMP_NONVOL1_MAPPED,
						4);
			}
		}

		//Restore non-volatile regs from the context
		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL1_MAPPED);

		comp_macroblock_push_restore_register_from_context(
				COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				PPCR_TMP_NONVOL0_MAPPED);
	} else {
		//Direct memory access

		//Copy memory in an unrolled loop
		for(i = 0; i < 16; i += 4)
		{
			//Read long from memory into spec temp, prevent from optimizing away
			comp_macroblock_push_load_memory_long(
					srcregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					PPCR_SPECTMP_MAPPED,
					srcregaddr->mapped_reg_num,
					i);

			//Save long to memory from spec temp, prevent from optimizing away
			comp_macroblock_push_save_memory_long(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | destregaddr->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					PPCR_SPECTMP_MAPPED,
					destregaddr->mapped_reg_num,
					i);
		}

		//Release temp registers
		helper_free_tmp_reg(srcregaddr);
		helper_free_tmp_reg(destregaddr);
	}
}

/**
 * Evaluates the extension word and compiles a long division instruction.
 *
 * Parameters:
 *   history - pointer to the execution history
 *   local_src_reg - mapped source register
 *   free_reg - if TRUE then the local_src_reg will be free'd as temporary register after the division is finished
 */
STATIC_INLINE void helper_divl(const cpu_history* history, comp_tmp_reg* local_src_reg, BOOL free_reg)
{
	//Read the extension word
	unsigned int extword = *((signed short*)(history->location + 1));

	//Get target registers from extension word
	int reg_dq = (extword >> 12) & 7;
	int reg_dr = extword & 7;

	//Get signed/unsigned operation flag
	BOOL is_signed = (extword & (1 << 11)) != 0 ? TRUE : FALSE;

	//Is this a 64 bit division?
	if ((extword & (1 << 10)) != 0)
	{
		//TODO: what if the quotient and the remainder registers are the same for the 64 bit division?

		//64 bit divided by 32 bit
		comp_macroblock_push_division_64_32bit(
				local_src_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_DX(reg_dq) | COMP_COMPILER_MACROBLOCK_REG_DX(reg_dr),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_DX(reg_dq) | COMP_COMPILER_MACROBLOCK_REG_DX(reg_dr),
				COMP_COMPILER_REGS_DATAREG(reg_dr),
				COMP_COMPILER_REGS_DATAREG(reg_dq),
				local_src_reg->mapped_reg_num,
				is_signed,
				COMP_GET_CURRENT_PC,
				(uae_u32)pc_ptr);

		//No need to free the temp register, 64 bit division releases all register mappings
	} else {
		//32 bit divided by 32 bit

		//Map quotient register
		comp_tmp_reg* dest_quot_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(reg_dq), TRUE, TRUE);
		output_dep = COMP_COMPILER_MACROBLOCK_REG_DX(reg_dq);

		//Is the quotient and the remainder the same?
		if (reg_dq == reg_dr)
		{
			//No need for remainder: simplified 32 by 32 bit division
			comp_macroblock_push_division_32_32bit_no_remainder(
					local_src_reg->reg_usage_mapping | output_dep,
					output_dep,
					dest_quot_reg->mapped_reg_num,
					local_src_reg->mapped_reg_num,
					is_signed,
					COMP_GET_CURRENT_PC,
					(uae_u32)pc_ptr);
		} else {
			//Full 32 by 32 bit division

			//Map remainder register
			comp_tmp_reg* dest_rem_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(reg_dr), TRUE, TRUE);
			output_dep = COMP_COMPILER_MACROBLOCK_REG_DX(reg_dr);

			//32 bit divided by 32 bit
			comp_macroblock_push_division_32_32bit(
					local_src_reg->reg_usage_mapping | output_dep,
					output_dep,
					dest_quot_reg->mapped_reg_num,
					dest_rem_reg->mapped_reg_num,
					local_src_reg->mapped_reg_num,
					is_signed,
					COMP_GET_CURRENT_PC,
					(uae_u32)pc_ptr);
		}

		//Do we need to release the source register?
		if (free_reg)
		{
			helper_free_tmp_reg(local_src_reg);
		}
	}
}

/**
 * Evaluates the extension word and compiles a long multiplication instruction.
 *
 * Parameters:
 *   history - pointer to the execution history
 *   local_src_reg - mapped source register
 *   src_reg_num - 68k source register number or -1
 *   free_reg - if TRUE then the local_src_reg will be free'd as temporary register after the multiplication is finished
 */
STATIC_INLINE void helper_mull(const cpu_history* history, comp_tmp_reg* local_src_reg, uae_s8 src_reg_num, BOOL free_reg)
{
	//Read the extension word
	unsigned int extword = *((signed short*)(history->location + 1));

	//Get signed/unsigned operation flag
	BOOL is_signed = (extword & (1 << 11)) != 0 ? TRUE : FALSE;

	//Get low order target registers from extension word
	int reg_dl = (extword >> 12) & 7;

	//Map low order destination register
	comp_tmp_reg* local_dest_low_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(reg_dl), TRUE, TRUE);
	output_dep = COMP_COMPILER_MACROBLOCK_REG_DX(reg_dl);

	//Check operation output size
	if ((extword & (1 << 10)) != 0)
	{
		//32x32->64 bit operation

		//Extract the high order register from extension word
		int reg_dh = extword & 7;

		//Set this flag to true if the registers for the following operations are overlapping
		BOOL is_same_reg = (reg_dh == reg_dl) || (reg_dh == src_reg_num);

		//Map high order destination register
		comp_tmp_reg* local_dest_high_reg = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(reg_dh), FALSE, TRUE);
		uae_u64 local_output_dep = COMP_COMPILER_MACROBLOCK_REG_DX(reg_dh);

		comp_tmp_reg* tempreg;

		//Allocate temporary register for high order destination result if needed
		if (is_same_reg)
		{
			tempreg = helper_allocate_tmp_reg();
		}

		//Multiply the registers: high order
		comp_macroblock_push_multiply_registers_high(
				input_dep | output_dep,
				is_same_reg ? tempreg->reg_usage_mapping : local_output_dep,
				is_same_reg ? tempreg->mapped_reg_num : local_dest_high_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				is_signed,
				FALSE);

		//Multiply the registers: low order
		comp_macroblock_push_multiply_registers(
				input_dep | output_dep,
				output_dep,
				local_dest_low_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				FALSE);

		//Copy temporary register to the final register if needed
		if (is_same_reg)
		{
			//TODO: probably this situation could be resolved without copy by using the register swapping, but it is complicated to maintain the register dependency then
			comp_macroblock_push_copy_register_long(
					tempreg->reg_usage_mapping,
					local_output_dep,
					local_dest_high_reg->mapped_reg_num,
					tempreg->mapped_reg_num);

			//Release temp register
			helper_free_tmp_reg(tempreg);
		}

		//Set internal Z flag from the two outputs
		comp_macroblock_push_or_register_register(
				local_output_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				local_dest_low_reg->mapped_reg_num,
				local_dest_high_reg->mapped_reg_num,
				TRUE);

		//Check Z, clear C and V flags
		helper_update_flags(
				COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				FALSE);

		//Set internal N flag from high order output register
		comp_macroblock_push_or_register_register(
				local_output_dep,
				COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN,
				PPCR_SPECTMP_MAPPED,
				local_dest_high_reg->mapped_reg_num,
				local_dest_high_reg->mapped_reg_num,
				TRUE);

		//Check N flag
		helper_update_flags(
				COMP_COMPILER_MACROBLOCK_REG_FLAGN,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				FALSE);

	} else {
		//32x32->32 bit multiplication

		//Do the multiplication on the high order word to find out the overflow flag for an unsigned multiplication
		comp_macroblock_push_multiply_registers_high(
				input_dep | output_dep,
				COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
				PPCR_SPECTMP_MAPPED,
				local_src_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				is_signed,
				TRUE);

		if (!is_signed)
		{
			//Unsigned operation: the result must be zero if there was no overflow - copy Z flag to V
			comp_macroblock_push_copy_nz_flags_to_register(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					PPCR_SPECTMP_MAPPED);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					PPCR_SPECTMP_MAPPED,
					25, 9, 9, FALSE);

			//Also extract N flag
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS_MAPPED,
					PPCR_SPECTMP_MAPPED,
					0, 0, 0, FALSE);

			//Invert V flag
			comp_macroblock_push_xor_high_register_imm(
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					1 << (PPCR_FLAG_V - 16));
		}

		//Multiply the registers and set the flags
		comp_macroblock_push_multiply_registers(
				input_dep | output_dep,
				output_dep,
				local_dest_low_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				FALSE);

		//Check flags
		comp_macroblock_push_or_register_register(
				output_dep,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				local_dest_low_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				local_dest_low_reg->mapped_reg_num,
				TRUE);

		if (is_signed)
		{
			//Signed operation: Save N and Z, clear C flags
			helper_update_flags(
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC,
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					FALSE);

			comp_tmp_reg* tempreg = helper_allocate_tmp_reg();

			//Highest bit of the result must match the entire high order result which was calculated earlier
			//Extract highest bit from result
			comp_macroblock_push_rotate_and_mask_bits(
					output_dep,
					tempreg->reg_usage_mapping,
					tempreg->mapped_reg_num,
					local_dest_low_reg->mapped_reg_num,
					16, 16, 16, FALSE);

			//Extend copied highest bit to the entire higher word
			comp_macroblock_push_copy_register_word_extended(
					tempreg->reg_usage_mapping,
					tempreg->reg_usage_mapping,
					tempreg->mapped_reg_num,
					tempreg->mapped_reg_num,
					FALSE);

			//Repeat higher word in the lower word
			comp_macroblock_push_rotate_and_copy_bits(
					tempreg->reg_usage_mapping,
					tempreg->reg_usage_mapping,
					tempreg->mapped_reg_num,
					tempreg->mapped_reg_num,
					16, 16, 31, FALSE);

			//Match previous high order result with the calculated bit mask
			comp_macroblock_push_xor_register_register(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC | tempreg->reg_usage_mapping,
					COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ,
					PPCR_SPECTMP_MAPPED,
					tempreg->mapped_reg_num,
					PPCR_SPECTMP_MAPPED,
					TRUE);

			helper_free_tmp_reg(tempreg);

			//The result must be zero if there was no overflow - copy Z flag to V
			comp_macroblock_push_copy_nz_flags_to_register(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					PPCR_SPECTMP_MAPPED);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					PPCR_SPECTMP_MAPPED,
					25, 9, 9, FALSE);

			//Invert V flag
			comp_macroblock_push_xor_high_register_imm(
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS_MAPPED,
					PPCR_FLAGS_MAPPED,
					1 << (PPCR_FLAG_V - 16));
		} else {
			//Save Z, clear C flags for unsigned (V and N was calculated earlier for unsigned)
			helper_update_flags(
					COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC,
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					FALSE);
		}
	}

	//Do we need to release the source register?
	if (free_reg)
	{
		helper_free_tmp_reg(local_src_reg);
	}
}

/**
 * Helper function for the implementation of ABCD -(Ax),-(Ay) and SBCD -(Ax),-(Ay) instructions.
 * Due to the close similarity these two instructions are fully implemented in this
 * helper function.
 * Parameters:
 *    history - pointer to the execution history
 *    subtraction - if TRUE then SBCD is compiled, otherwise ABCD
 */
STATIC_INLINE void helper_ABCD_SBCD_MEM(const cpu_history* history, BOOL subtraction)
{
	//Read memory from source
	comp_tmp_reg* local_src_reg = helper_read_memory(
			src_mem_addrreg->reg_usage_mapping,
			history,
			src_mem_addrreg,
			1,
			TRUE);

	//!TODO: saving data into the context is not required if the memory access is non-special (temp regs are preserved), but we don't know that at this stage
	//Save source data to context
	comp_macroblock_push_save_register_to_context(
			local_src_reg->reg_usage_mapping | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
			local_src_reg->mapped_reg_num);

	//Free source register
	helper_free_tmp_reg(local_src_reg);

	//Read memory from destination
	comp_tmp_reg* local_dest_reg = helper_read_memory(
			dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			1,
			TRUE);

	//Re-allocate source register
	local_src_reg = helper_allocate_tmp_reg();

	//Restore source register from context
	comp_macroblock_push_restore_register_from_context(
			local_src_reg->reg_usage_mapping,
			local_src_reg->mapped_reg_num);

	//Get X flag into the register
	comp_tmp_reg* temp_flagx_reg = helper_copy_x_flag_to_register();

	if (subtraction)
	{
		//Subtract source decimal number and X flag from the destination, returns result in destination and C flag in the temp register
		comp_macroblock_push_sub_decimal(
				local_dest_reg->reg_usage_mapping | local_src_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
				local_dest_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
				local_dest_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				temp_flagx_reg->mapped_reg_num);
	} else {
		//Sum decimal numbers and X flag, returns result in destination and C flag in the temp register
		comp_macroblock_push_add_decimal(
				local_dest_reg->reg_usage_mapping | local_src_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
				local_dest_reg->reg_usage_mapping | temp_flagx_reg->reg_usage_mapping,
				local_dest_reg->mapped_reg_num,
				local_src_reg->mapped_reg_num,
				temp_flagx_reg->mapped_reg_num);
	}

	//Get the flags and free temporary register
	helper_extract_flags_for_decimal(temp_flagx_reg, local_dest_reg);

	//Write result to destination memory address and release temp register
	helper_write_memory(
			local_dest_reg->reg_usage_mapping | dest_mem_addrreg->reg_usage_mapping,
			history,
			dest_mem_addrreg,
			local_dest_reg, 1);
}

/**
 * Allocate a register and copy the X flag into the lowest bit of the register.
 */
STATIC_INLINE comp_tmp_reg* helper_copy_x_flag_to_register()
{
	comp_tmp_reg* output_reg = helper_allocate_tmp_reg();

	//Rotate flag X to the lowest bit into new reg
	comp_macroblock_push_rotate_and_mask_bits(
			COMP_COMPILER_MACROBLOCK_REG_FLAGX,
			output_reg->reg_usage_mapping,
			output_reg->mapped_reg_num,
			PPCR_FLAGS_MAPPED,
			27, 31, 31, FALSE);

	return output_reg;
}

/**
 * Extract the flags from the results for the decimal operations then free C flag register.
 * Parameters:
 * 	 flagc_reg - C flag in the lowest bit of a register
 * 	 local_dest_reg - destination register with the calculated value in the lowest byte
 * Note: this function uses the spec temp register
 */
STATIC_INLINE void helper_extract_flags_for_decimal(comp_tmp_reg* flagc_reg, comp_tmp_reg* local_dest_reg)
{
	//Copy C flag into the flag register
	comp_macroblock_push_rotate_and_copy_bits(flagc_reg->reg_usage_mapping,
		COMP_COMPILER_MACROBLOCK_REG_FLAGC,
		PPCR_FLAGS_MAPPED,
		flagc_reg->mapped_reg_num,
		21, 10, 10, FALSE);

	//Copy C flag to X
	comp_macroblock_push_rotate_and_copy_bits(
		COMP_COMPILER_MACROBLOCK_REG_FLAGC,
		COMP_COMPILER_MACROBLOCK_REG_FLAGX,
		PPCR_FLAGS_MAPPED,
		PPCR_FLAGS_MAPPED, 16, 26, 26, FALSE);

	//TODO: do we need to adjust the V flag? The interpretive changes it, but it is not defined in the documentation

	//Check N and Z flag
	comp_macroblock_push_check_byte_register(local_dest_reg->reg_usage_mapping, local_dest_reg->mapped_reg_num);

	//Store N and Z flag
	helper_check_nz_flags();

	//Release C flag register
	helper_free_tmp_reg(flagc_reg);
}
