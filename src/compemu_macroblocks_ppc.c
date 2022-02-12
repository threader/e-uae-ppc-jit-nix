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
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset);
STATIC_INLINE void helper_move_inst_flags(void);
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
STATIC_INLINE void helper_MOVREG2MEM(const cpu_history* history, uae_u8 size);
STATIC_INLINE void helper_MOVIMM2REG(uae_u8 size, int checkflags);

/**
 * Local variables
 */

//Source register (allocated temporary register) for the opcode
//Initialized and released by the addressing mode
int src_reg;

//Source register (allocated temporary register mapped to PPC register) for the opcode
//Initialized and released by the addressing mode
uae_u8 src_reg_mapped;

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
}

/**
 * Addressing mode compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->srcreg), 1, 0);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->srcreg);
}
void comp_addr_pre_regA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), 1, 0);
	input_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->srcreg);
}
void comp_addr_pre_indA_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, 0);
}
void comp_addr_pre_indmAL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -4);
}
void comp_addr_pre_indmAW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -2);
}
void comp_addr_pre_indmAB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_src_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_src_ax(props, -1);
}
void comp_addr_pre_indApL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, 1);

	//Increase the address register by the size of the operation
	helper_add_imm_to_src_ax(props, 4);
}
void comp_addr_pre_indApW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, 1);

	//Increase the address register by the size of the operation
	helper_add_imm_to_src_ax(props, 2);
}
void comp_addr_pre_indApB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_src_mem_regs_copy(props, 1);

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
	//Source register in props is the immediate value,
	//load it with sign extension
	src_immediate = *((signed int*)pc_ptr);
	pc_ptr += 2;
}
void comp_addr_pre_immedW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Source register in props is the immediate value,
	//load it with sign extension
	src_immediate = *((signed short*)pc_ptr);
	pc_ptr++;
}
void comp_addr_pre_immedB_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Source register in props is the immediate value,
	//load it with sign extension
	src_immediate = *(((signed char*)pc_ptr + 1));
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
	helper_allocate_2_ax_src_mem_regs(props, 0);

	//Add the offset from the next word after the opcode to the register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_REGS_ADDRREG(props->srcreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(src_mem_addrreg),
			src_mem_addrreg_mapped,
			src_reg_mapped,
			*(pc_ptr++));
}
void comp_addr_pre_indPCd16_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_absW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_absL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_indAcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_indPCcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_absW_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_absL_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_indAcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_indPCcp_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}

/**
 * Addressing mode compiler functions for destination addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_DATAREG(props->destreg), 1, 1);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_DX(props->destreg);
}
void comp_addr_pre_regA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), 1, 1);
	output_dep |= COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}
void comp_addr_pre_indA_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, 0);
}
void comp_addr_pre_indmAL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -4);
}
void comp_addr_pre_indmAW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -2);
}
void comp_addr_pre_indmAB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_ax_dest_mem_reg(props, 1);

	//Decrease the register before use by the size of the operation
	helper_add_imm_to_dest_ax(props, -1);
}
void comp_addr_pre_indApL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, 1);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 4);
}
void comp_addr_pre_indApW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, 1);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, 2);
}
void comp_addr_pre_indApB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs_copy(props, 1);

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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_immedQ_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Invalid addressing mode
	write_log("JIT error: invalid addressing mode - immedQ as destination\n");
	abort();
}
void comp_addr_pre_indAd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_allocate_2_ax_dest_mem_regs(props, 0);

	//Add the offset from the next word after the opcode to the register
	comp_macroblock_push_add_register_imm(
			COMP_COMPILER_REGS_ADDRREG(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
			dest_mem_addrreg_mapped,
			dest_reg_mapped,
			*(pc_ptr++));
}
void comp_addr_pre_indPCd16_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_absW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_absL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_indAcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_pre_indPCcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_immedW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_immedB_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_absW_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_absL_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_indAcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_addr_post_indPCcp_dest(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}

/**
 * Condition check compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_cond_pre_CC_cc_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_cs_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_eq_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ge_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_gt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_hi_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_le_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ls_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_lt_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_mi_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ne_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_pl_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_vc_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_vs_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_t_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_cond_pre_CC_f_src(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	helper_move_inst_flags();
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
	helper_move_inst_flags();
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
	helper_move_inst_flags();
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVREG2MEM(history, 4);
}
void comp_opcode_MOVREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVREG2MEM(history, 2);
}
void comp_opcode_MOVREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVREG2MEM(history, 1);
}
void comp_opcode_MOVMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVAMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVAMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(4, 1);
}
void comp_opcode_MOVIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(2, 1);
}
void comp_opcode_MOVIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(1, 1);
}
void comp_opcode_MOVAIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(4, 0);
}
void comp_opcode_MOVAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	helper_MOVIMM2REG(2, 0);
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMUW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRMEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRMEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CLRMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LEAIMML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LEAIMMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_PEAIMML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_PEAIMMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_PEAIND(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVPREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVPREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVPMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVPMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_STREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SFREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SCCREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_STMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SFMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SCCMEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
void comp_opcode_MOVSR2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVSR2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVUSP2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2USPL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVCREG2CTRL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_MOVCCTR2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
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
void comp_opcode_RTR(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RTE(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
void comp_opcode_ANDIMM2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2SRW(const cpu_history* history, struct comptbl* props) REGPARAM
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
void comp_opcode_CASREG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CASREG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CASREG2MEMB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CAS2REG2MEML(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CAS2REG2MEMW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKMEM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_CHKMEM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	//Value 0 means value 8
	if (src_immediate == 0) src_immediate = 8;

	//Allocate temp reg for the immediate
	uae_u8 tmpreg = helper_allocate_tmp_reg_with_init(src_immediate);

	//Compile ADDCO PPC opcode
	comp_macroblock_push_add_with_flags(
			COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg) | COMP_COMPILER_MACROBLOCK_REG_DX(props->destreg),
			COMP_COMPILER_MACROBLOCK_REG_DX(props->destreg) | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL,
			dest_reg_mapped,
			dest_reg_mapped,
			comp_get_gpr_for_temp_register(tmpreg));

	//Save flags
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL,
			COMP_COMPILER_MACROBLOCK_REG_NONE,
			COMP_COMPILER_MACROBLOCK_REG_NONE);

	helper_free_tmp_reg(tmpreg);
}

void comp_opcode_ADDQ2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAQ2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAIMM2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ADDAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAIMM2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAREG2REGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_SUBAREG2REGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_EXG(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TSTREGL(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TSTREGW(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TSTREGB(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
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
void comp_opcode_TRAP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_TRAPCC(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_STOP(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_RESET(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_BKPT(const cpu_history* history, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*(history->location)); /* TODO: addressing mode */
}
void comp_opcode_ILLEGAL(const cpu_history* history, struct comptbl* props) REGPARAM
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
void comp_opcode_FTRAPCC(const cpu_history* history, struct comptbl* props) REGPARAM
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
 *    cpu_history - execution history item for the instruction
 *    opcode - unsupported opcode number
 */
void comp_opcode_unsupported(const const cpu_history* history, uae_u16 opcode)
{
	comp_macroblock_push_opcode_unsupported(history->location, opcode);
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
 */
STATIC_INLINE void helper_update_flags(uae_u16 flagscheck, uae_u16 flagsclear, uae_u16 flagsset)
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
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_FLAGC,
					COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS,
					PPCR_FLAGS,
					16, 26, 26, 0);
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
					0, 0, 10, 0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Flags: N, C, V and X
			//Move XER to CR2, copy CR to temp, copy X to C then insert temp register relevant part to the flag register
			comp_macroblock_push_copy_nzcv_flags_to_register(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg));
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg),
					comp_get_gpr_for_temp_register(tmpreg),
					16, 26, 26, 0);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 9, 0, 0);
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
					0, 9, 10, 0);
			comp_macroblock_push_rotate_and_copy_bits(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS,
					comp_get_gpr_for_temp_register(tmpreg),
					0, 0, 0, 0);
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
					0, 0, 9, 0);
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
					0, 0, 2, 0);
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
					0, 2, 2, 0);
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
				0, 26, 26, 0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV):
			//Clear flags: C and V: mask out these registers and keep N, Z and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 8, 0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX):
			//Clear flags: C and X: mask out these registers and keep N, Z and V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGX,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 27, 9, 0);
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
					0, 26, 26, 0);
			} else {
				//Clear flags: C, V and N: mask out these registers and keep Z and X
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV,
					PPCR_FLAGS,
					PPCR_FLAGS,
					0, 11, 8, 0);
				comp_macroblock_push_rotate_and_mask_bits(
					COMP_COMPILER_MACROBLOCK_REG_NONE,
					COMP_COMPILER_MACROBLOCK_REG_FLAGN,
					PPCR_FLAGS,
					PPCR_FLAGS,
					0, 1, 31, 0);
			}
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: C, V and Z: mask out these registers and keep N and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC | COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 1, 0);
			break;
		case (COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ):
			//Clear flags: N and Z: mask out these registers and keep C, V and X
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 3, 31, 0);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGC:
			//Clear flag: C
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGC,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 11, 9, 0);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGV:
			//Clear flag: V
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGV,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 10, 8, 0);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGN:
			//Clear flag: N
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGN,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 1, 31, 0);
			break;
		case COMP_COMPILER_MACROBLOCK_REG_FLAGZ:
			//Clear flag: Z
			comp_macroblock_push_rotate_and_mask_bits(
				COMP_COMPILER_MACROBLOCK_REG_NONE,
				COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
				PPCR_FLAGS,
				PPCR_FLAGS,
				0, 3, 1, 0);
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

/* Saving the flags for a move instruction */
STATIC_INLINE void helper_move_inst_flags()
{
	//Save flags: N and Z, clear: V and C
	helper_update_flags(
			COMP_COMPILER_MACROBLOCK_REG_FLAGN | COMP_COMPILER_MACROBLOCK_REG_FLAGZ,
			COMP_COMPILER_MACROBLOCK_REG_FLAGV | COMP_COMPILER_MACROBLOCK_REG_FLAGC,
			COMP_COMPILER_MACROBLOCK_REG_NONE);
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
			set);
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
	src_mem_addrreg_mapped = src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), 1, modified);
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
	src_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->srcreg), 1, modified);
	src_mem_addrreg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
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
			COMP_COMPILER_REGS_ADDRREG(props->srcreg),
			COMP_COMPILER_REGS_ADDRREG(props->srcreg),
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
	dest_mem_addrreg_mapped = dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), 1, modified);
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
	dest_reg_mapped = comp_map_temp_register(COMP_COMPILER_REGS_ADDRREG(props->destreg), 1, modified);
	dest_mem_addrreg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
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
			COMP_COMPILER_REGS_ADDRREG(props->destreg),
			COMP_COMPILER_REGS_ADDRREG(props->destreg),
			dest_reg_mapped,
			dest_reg_mapped,
			immediate);
}

/**
 * Implementation of all MOVREG2MEM instruction
 * Parameters:
 *    history - pointer to cpu execution history
 *    size - size of the operation: byte (1), word (2) or long (4)
 */
STATIC_INLINE void helper_MOVREG2MEM(const cpu_history* history, uae_u8 size)
{
	int spec;

	//Special memory determination by operation size
	switch (size)
	{
	case 4:
		spec = comp_is_spec_memory_write_long(history->pc, history->specmem);
		comp_macroblock_push_check_long_register(input_dep, src_reg_mapped);
		break;
	case 2:
		spec = comp_is_spec_memory_write_word(history->pc, history->specmem);
		comp_macroblock_push_check_word_register(input_dep, src_reg_mapped);
		break;
	case 1:
		spec = comp_is_spec_memory_write_byte(history->pc, history->specmem);
		comp_macroblock_push_check_byte_register(input_dep, src_reg_mapped);
		break;
	default:
		write_log("Error: wrong operation size for MOVREG2MEM\n");
		abort();
	}

	//Save flags
	helper_move_inst_flags();

	if (spec)
	{
		//Special memory access
		comp_macroblock_push_save_memory_spec(
				input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
				src_reg_mapped,
				dest_mem_addrreg_mapped,
				size);

		//The previous will kill all temporary register mappings,
		//because it calls a GCC function
		dest_mem_addrreg = PPC_TMP_REG_NOTUSED;
	}
	else
	{
		//Normal memory access

		//Get memory address into the temp register
		comp_macroblock_push_map_physical_mem(
				COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
				dest_mem_addrreg_mapped,
				dest_mem_addrreg_mapped);

		//Save long to memory, prevent from optimizing away
		switch (size)
		{
		case 4:
			comp_macroblock_push_save_memory_long(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					dest_mem_addrreg_mapped,
					0);
			break;
		case 2:
			comp_macroblock_push_save_memory_word(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					dest_mem_addrreg_mapped,
					0);
			break;
		case 1:
			comp_macroblock_push_save_memory_byte(
					input_dep | COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg),
					output_dep | COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM,
					src_reg_mapped,
					dest_mem_addrreg_mapped,
					0);
			break;
		}
	}
}

/**
 * Implementation of all MOV(A)IMM2REG instruction
 * Parameters:
 *    size - size of the operation: byte (1), word (2) or long (4)
 *    checkflags - if TRUE then flags are checked after the operation (MOVEA does not need this)
 */
STATIC_INLINE void helper_MOVIMM2REG(uae_u8 size, int checkflags)
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
		uae_u8 tmpreg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
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
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		} else {
			//We assume that this must be byte size, lazy-lazy...

			//Copy the lowest byte into the destination register
			comp_macroblock_push_copy_register_byte(
					COMP_COMPILER_MACROBLOCK_REG_TMP(tmpreg),
					output_dep,
					dest_reg_mapped,
					tmpreg_mapped);
		}

		//Done with the temp register
		comp_free_temp_register(tmpreg);
	}

	//Set up flags if required
	if (checkflags)
	{
		helper_move_inst_static_flags(src_immediate);
	}
}

STATIC_INLINE void helper_addr_indmAk_dest(struct comptbl* props, uae_u8 size)
{
	//Allocate the address register
	helper_allocate_ax_dest_mem_reg(props, 1);

	//HACK: Switch off source register with a temp register,
	//that preserves the original value of the address register
	src_reg = comp_allocate_temp_register(PPC_TMP_REG_ALLOCATED);
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
	helper_allocate_2_ax_dest_mem_regs_copy(props, 1);

	//Increase the address register by the size of the operation
	helper_add_imm_to_dest_ax(props, size);

	//HACK: Switch off source register with a temp register,
	//that preserves the original value of the address register
	src_reg_mapped = dest_mem_addrreg_mapped;

	//Instruction is depending on both registers
	input_dep = COMP_COMPILER_MACROBLOCK_REG_TMP(dest_mem_addrreg) | COMP_COMPILER_MACROBLOCK_REG_AX(props->destreg);
}
