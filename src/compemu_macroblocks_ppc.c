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

#include "compemu_macroblocks.h"

/**
 * Addressing mode compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_regA_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indA_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmALk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAWk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmABk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApLk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApWk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApBk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedQ_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indAd16_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indPCd16_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_absW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_absL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indAcp_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indPCcp_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

/**
 * Addressing mode compiler functions for source addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_post_regD_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_regA_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indA_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmALk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAWk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmABk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApLk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApWk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApBk_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedB_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedQ_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indAd16_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indPCd16_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_absW_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_absL_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indAcp_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indPCcp_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

/**
 * Addressing mode compiler functions for destination addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_pre_regD_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_regA_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indA_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmALk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmAWk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indmABk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApLk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApWk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indApBk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_immedQ_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indAd16_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indPCd16_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_absW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_absL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indAcp_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_pre_indPCcp_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

/**
 * Addressing mode compiler functions for destination addressing executed after the instruction
 * compiling function was finished
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_addr_post_regD_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_regA_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indA_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmALk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmAWk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indmABk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApLk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApWk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indApBk_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedB_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_immedQ_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indAd16_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indPCd16_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_absW_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_absL_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indAcp_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_addr_post_indPCcp_dest(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

/**
 * Condition check compiler functions for source addressing executed before the instruction
 * compiling function is called
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_cond_pre_CC_cc_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_cs_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_eq_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ge_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_gt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_hi_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_le_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ls_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_lt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_mi_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_ne_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_pl_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_vc_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_vs_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_t_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_CC_f_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_f_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_eq_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ogt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_oge_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_olt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ole_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ogl_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_or_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_un_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ueq_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ugt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_uge_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ult_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ule_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ne_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_t_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_sf_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_seq_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ge_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_lt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_le_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gl_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_gle_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngle_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngl_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nle_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nlt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_nge_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_ngt_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_sne_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_cond_pre_FCC_st_src(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

/**
 * Instruction compiling functions
 * Parameter:
 *     pc - pointer to the instruction in (mapped) REGPARAM memory
 */
void comp_opcode_MOVREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVAIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEQ(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMUL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2MEMUW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGUL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVEM2REGUW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOV16REG2REGU(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOV16REG2MEMU(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOV16MEM2REGU(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOV16REG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOV16MEM2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRREGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRREGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRMEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CLRMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LEAIMML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LEAIMMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LEAIND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_PEAIMML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_PEAIMMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_PEAIND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVPREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVPREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVPMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVPMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_STREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SFREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SCCREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_STMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SFMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SCCMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVCCR2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVCCR2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2CCRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2CCRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2CCRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVSR2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVSR2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVIMM2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVMEM2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVUSP2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVREG2USPL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVCREG2CTRL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MOVCCTR2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DBCOND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DBF(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCONDB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BRAB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSRB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCONDW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BRAW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCONDL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BRAL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSRL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_JMPIMM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_JMPIND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_JSRIMM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_JSRIND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RTS(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RTD(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RTR(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RTE(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTREGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTREGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTMEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOTMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2CCRB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2CCRB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2CCRB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ANDIMM2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ORIMM2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EORIMM2SRW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BTSTIMM2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BTSTREG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BTSTIMM2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BTSTREG2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BTSTREG2IMM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSETIMM2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSETREG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSETIMM2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BSETREG2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCLRIMM2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCLRREG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCLRIMM2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCLRREG2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCHGIMM2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCHGREG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCHGIMM2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BCHGREG2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TAS2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TAS2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASLMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ASRMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSLMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LSRMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROLMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXLMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RORMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ROXRMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPAMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPAREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPAIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMPIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CMP2MEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CASREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CASREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CASREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CAS2REG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CAS2REG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_CHKMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAQ2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDQ2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDAIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ADDXMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBMEM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBIMM2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAQ2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBQ2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBAREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SUBXMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULSIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULSREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULSMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULUIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULUREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULUMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_MULMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVSIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVSREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVSMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVUIMM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVUREG2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVUMEM2REGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVIMM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVREG2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_DIVMEM2REGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGREGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGREGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGMEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NEGXMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ABCDREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ABCDMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SBCDREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SBCDMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NBCDREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NBCDMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_PACKREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_PACKMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_UNPKREG2REGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_UNPKMEM2MEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_SWAP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EXG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EXTBW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EXTWL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_EXTBL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_NOP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTREGL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTREGW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTMEML(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTMEMW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TSTMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LINKW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_LINKL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_UNLINK(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TRAP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_TRAPCC(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_STOP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_RESET(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BKPT(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_ILLEGAL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFCHG2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFCHG2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFCLR2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFCLR2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFEXTS2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFEXTS2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFEXTU2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFEXTU2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFFFO2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFFFO2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFINS2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFINS2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFSET2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFSET2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFTST2REG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_BFTST2MEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FBCONDW(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FBCONDL(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FDBCOND(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FGENREG(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FGENMEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FGENMEMUM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FGENMEMUP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FGENIMM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FSAVEMEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FSAVEMEMUM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FRESTOREMEM(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FRESTOREMEMUP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FSCCREGB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FSCCMEMB(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FTRAPCC(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}
void comp_opcode_FNOP(uae_u16* pc_p, struct comptbl* props) REGPARAM
{
	comp_not_implemented(*pc_p); /* TODO: addressing mode */
}

