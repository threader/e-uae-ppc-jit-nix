/**
 * Header file for the JIT macroblock compiler implementation
 */

//Prototype for macroblock union to let us use it in the handler prototype definition.
union comp_compiler_mb_union;

typedef void comp_compiler_macroblock_func (union comp_compiler_mb_union*) REGPARAM;

//M68k register offset calculators
#define COMP_COMPILER_REGS_DATAREG(x) (x)
#define COMP_COMPILER_REGS_ADDRREG(x)	((x) + 8)

//Macroblock input/output register mapping to bits
//Flag register mapping to bits for emulated 68k flag dependency tracking
//Please note: the flag copying implementation is depending on this layout,
//flags are stored in PPCR_FLAGS register in this order.
//If you want to change it then also update function: comp_macroblock_update_flags()
#define COMP_COMPILER_MACROBLOCK_REG_FLAGC	(1ULL << 0)
#define COMP_COMPILER_MACROBLOCK_REG_FLAGV	(1ULL << 1)
#define COMP_COMPILER_MACROBLOCK_REG_FLAGZ	(1ULL << 2)
#define COMP_COMPILER_MACROBLOCK_REG_FLAGN	(1ULL << 3)
#define COMP_COMPILER_MACROBLOCK_REG_FLAGX	(1ULL << 4)

//Register mapping to bits for data registers
#define COMP_COMPILER_MACROBLOCK_REGS_START	5
#define COMP_COMPILER_MACROBLOCK_REG_D0		(1ULL << 5)
#define COMP_COMPILER_MACROBLOCK_REG_D1		(1ULL << 6)
#define COMP_COMPILER_MACROBLOCK_REG_D2		(1ULL << 7)
#define COMP_COMPILER_MACROBLOCK_REG_D3		(1ULL << 8)
#define COMP_COMPILER_MACROBLOCK_REG_D4		(1ULL << 9)
#define COMP_COMPILER_MACROBLOCK_REG_D5		(1ULL << 10)
#define COMP_COMPILER_MACROBLOCK_REG_D6		(1ULL << 11)
#define COMP_COMPILER_MACROBLOCK_REG_D7		(1ULL << 12)

//Register mapping to bits for address registers
#define COMP_COMPILER_MACROBLOCK_ADDR_REGS_START	13
#define COMP_COMPILER_MACROBLOCK_REG_A0		(1ULL << 13)
#define COMP_COMPILER_MACROBLOCK_REG_A1		(1ULL << 14)
#define COMP_COMPILER_MACROBLOCK_REG_A2		(1ULL << 15)
#define COMP_COMPILER_MACROBLOCK_REG_A3		(1ULL << 16)
#define COMP_COMPILER_MACROBLOCK_REG_A4		(1ULL << 17)
#define COMP_COMPILER_MACROBLOCK_REG_A5		(1ULL << 18)
#define COMP_COMPILER_MACROBLOCK_REG_A6		(1ULL << 19)
#define COMP_COMPILER_MACROBLOCK_REG_A7		(1ULL << 20)

//Internal flag register mapping to bits that are used for native PPC flag dependency tracking
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGS_START 21
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC	(1ULL << 21)
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV	(1ULL << 22)
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ	(1ULL << 23)
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN	(1ULL << 24)

//R0 register dependency tracking
#define COMP_COMPILER_MACROBLOCK_TMP_REG_SPEC	(1ULL << 25)
#define COMP_COMPILER_MACROBLOCK_TMP_REGS_START	26

//No register dependency
#define COMP_COMPILER_MACROBLOCK_REG_NONE	0

//Do not optimize away this block
#define COMP_COMPILER_MACROBLOCK_CONTROL_FLAGS_START	63
#define COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM (1ULL << 63)

//All control flags for the optimization
#define COMP_COMPILER_MACROBLOCK_CONTROL_FLAGS (COMP_COMPILER_MACROBLOCK_REG_NO_OPTIM)

//All registers in one constant
#define COMP_COMPILER_MACROBLOCK_REG_ALL	((1ULL << COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGS_START) - 1)

//All flag registers in one constant
#define COMP_COMPILER_MACROBLOCK_REG_FLAG_ALL	((1ULL << COMP_COMPILER_MACROBLOCK_REGS_START) - 1)

//All flag internal registers in one constant
#define COMP_COMPILER_MACROBLOCK_INTERNAL_FLAG_ALL	(COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGC | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGV | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGZ | COMP_COMPILER_MACROBLOCK_INTERNAL_FLAGN)

//Get a specific register mapping
#define COMP_COMPILER_MACROBLOCK_REG_DX_OR_AX(x) (1ULL << (x + COMP_COMPILER_MACROBLOCK_REGS_START))
#define COMP_COMPILER_MACROBLOCK_REG_DX(x)	(1ULL << (x + COMP_COMPILER_MACROBLOCK_REGS_START))
#define COMP_COMPILER_MACROBLOCK_REG_AX(x)	(1ULL << (x + COMP_COMPILER_MACROBLOCK_ADDR_REGS_START))
#define COMP_COMPILER_MACROBLOCK_REG_TMP(x)	(1ULL << (x + COMP_COMPILER_MACROBLOCK_TMP_REGS_START))

/* Getting an offset inside the regs structure for a specified field */
#define COMP_GET_OFFSET_IN_REGS(x) (((void*)&(regs.x)) - ((void*)&regs))

//Prototypes for functions
void comp_compiler_init(void);
void comp_compiler_done(void);
union comp_compiler_mb_union* comp_compiler_get_next_macroblock(void);
void comp_compiler_optimize_macroblocks(void);
void comp_compiler_generate_code(void);
void comp_compiler_debug_dump_compiled(void);

//Macroblock compiling handler prototypes
void comp_macroblock_push_opcode_unsupported(uae_u16 opcode);
void comp_macroblock_push_add_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2);
void comp_macroblock_push_add(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2);
void comp_macroblock_push_add_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm);
void comp_macroblock_push_add_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm);
void comp_macroblock_push_sub(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 subtrahend_input_reg1, uae_u8 minuend_input_reg2);
void comp_macroblock_push_sub_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 subtrahend_input_reg1, uae_u8 minuend_input_reg2);
void comp_macroblock_push_copy_register_long_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_copy_register_long(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_copy_register_word(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_copy_register_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_load_register_long(uae_u64 regsout, uae_u8 output_reg, uae_u32 imm);
void comp_macroblock_push_load_register_word_extended(uae_u64 regsout, uae_u8 output_reg, uae_u16 imm);
void comp_macroblock_push_load_memory_spec(uae_u64 regsin, uae_u64 regsout, uae_u8 src_mem_reg, uae_u8 dest_reg, uae_u8 size);
void comp_macroblock_push_load_memory_spec_save_temps(uae_u64 regsin, uae_u64 regsout, uae_u8 src_mem_reg, uae_u8 dest_reg, uae_u8 size);
void comp_macroblock_push_load_memory_long(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_load_memory_word(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_load_memory_word_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_load_memory_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_map_physical_mem(uae_u64 regsin, uae_u64 regsout, uae_u8 dest_mem_reg, uae_u8 source_reg);
void comp_macroblock_push_save_memory_spec(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u8 dest_mem_reg, uae_u8 size);
void comp_macroblock_push_save_memory_long(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_save_memory_word(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_save_memory_word_update(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_save_memory_byte(uae_u64 regsin, uae_u64 regsout, uae_u8 source_reg, uae_u32 base_reg, uae_u32 offset);
void comp_macroblock_push_and_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate);
void comp_macroblock_push_and_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate);
void comp_macroblock_push_and_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags);
void comp_macroblock_push_or_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm);
void comp_macroblock_push_or_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm);
void comp_macroblock_push_or_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags);
void comp_macroblock_push_xor_register_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2, char updateflags);
void comp_macroblock_push_xor_low_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate);
void comp_macroblock_push_xor_high_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 immediate);
void comp_macroblock_push_and_register_imm(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u16 imm);
void comp_macroblock_push_and_registers(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2);
void comp_macroblock_push_multiply_registers_with_flags(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg1, uae_u8 input_reg2);
void comp_macroblock_push_copy_nzcv_flags_to_register(uae_u64 regsout, uae_u8 output_reg);
void comp_macroblock_push_copy_nz_flags_to_register(uae_u64 regsout, uae_u8 output_reg);
void comp_macroblock_push_copy_cv_flags_to_register(uae_u64 regsout, uae_u8 output_reg);
void comp_macroblock_push_check_long_register(uae_u64 regsin, uae_u8 input_reg);
void comp_macroblock_push_check_word_register(uae_u64 regsin, uae_u8 input_reg);
void comp_macroblock_push_copy_register_word_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_copy_register_byte_extended(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg);
void comp_macroblock_push_check_byte_register(uae_u64 regsin, uae_u8 input_reg);
void comp_macroblock_push_rotate_and_copy_bits(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, uae_u8 maskb, uae_u8 maske, int updateflags);
void comp_macroblock_push_rotate_and_mask_bits(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, uae_u8 maskb, uae_u8 maske, int updateflags);
void comp_macroblock_push_arithmetic_shift_right_register(uae_u64 regsin, uae_u64 regsout, uae_u8 output_reg, uae_u8 input_reg, uae_u8 shift, int updateflags);
void comp_macroblock_push_arithmetic_left_shift_extract_v_flag(uae_u64 regsin, uae_u8 input_reg, uae_u8 shift_reg, uae_u8 tmp_reg);
void comp_macroblock_push_save_reg_slot(uae_u64 regsin, uae_u8 input_reg, unsigned int slot);
void comp_macroblock_push_load_reg_slot(uae_u64 regsout, uae_u8 output_reg, unsigned int slot);
void comp_macroblock_push_set_byte_from_z_flag(uae_u64 regsout, uae_u8 output_reg, int negate);
void comp_macroblock_push_stop(void);
void comp_macroblock_push_nop(void);
void comp_macroblock_push_null_operation(uae_u64 regsin, uae_u64 regsout);
void comp_macroblock_push_load_flags(void);
void comp_macroblock_push_save_flags(void);
void comp_macroblock_push_load_pc(const cpu_history * inst_history);

