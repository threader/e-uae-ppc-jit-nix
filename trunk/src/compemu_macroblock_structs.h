/**
 * Header file for the JIT macroblock compiler implementation structures
 */

//Macroblock base description structure
struct comp_compiler_mb
{
	uae_u64 input_registers;				//Input registers, each register is marked with one bit, see COMP_COMPILER_MACROBLOCK_* constants
	uae_u64 output_registers;				//Output registers, each register is marked with one bit, see COMP_COMPILER_MACROBLOCK_* constants
	uae_u64 carry_registers;				//Carry of the register usage optimization for this macroblock
	comp_compiler_macroblock_func* handler;	//Handler function for compiling the macroblock
	const char* name;						//Name of the macroblock
	uae_u16* m68k_ptr;						//Pointer to the instruction that was compiled
	void* start;							//Start address of the compiled code (or NULL)
	void* end;								//End address of the compiled code (only valid if there was a non-null start address)
	BOOL remove;							//After the register optimization if this field true then the macroblock will be removed
};

//Structure for unsupported opcode macroblock
struct comp_compiler_mb_unsupported
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u16	opcode;						//Unsupported opcode
};

//Structure for normal three register input: two input and one output registers
//For example: addco output_reg, input_reg1, input_reg2
struct comp_compiler_mb_three_regs_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg	input_reg1;			//Input register #1
	comp_ppc_reg	input_reg2;			//Input register #2
	comp_ppc_reg	output_reg;			//Output register
	comp_ppc_reg	temp_reg;			//Optional temporary register
};

//Structure for normal three register input: two input and one output registers, plus specify if flag update needed
//For example: or(x) output_reg, input_reg1, input_reg2
struct comp_compiler_mb_three_regs_opcode_flags
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg	input_reg1;			//Input register #1
	comp_ppc_reg	input_reg2;			//Input register #2
	comp_ppc_reg	output_reg;			//Output register
	BOOL			updateflags;		//Flags are updated (1) or not (0)
};

//Structure for normal two register and one immediate value input
//For example: addis. output_reg, input_reg, immediate
struct comp_compiler_mb_two_regs_imm_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u16			immediate;					//Immediate value
	comp_ppc_reg	input_reg;					//Input register
	comp_ppc_reg	output_reg;					//Output register
};

//Structure for normal two register input, plus specify if flag update needed
//For example: mr(x) output_reg, input_reg
struct comp_compiler_mb_two_regs_opcode_flags
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg	input_reg;			//Input register
	comp_ppc_reg	output_reg;			//Output register
	BOOL			updateflags;		//Flags are updated (1) or not (0)
};

//Structure for normal two register input
//For example: mr output_reg, input_reg
struct comp_compiler_mb_two_regs_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg	input_reg;			//Input register
	comp_ppc_reg	output_reg;			//Output register
};

//Structure for normal one register input
//For example: mfcr reg
struct comp_compiler_mb_one_reg_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg	reg;				//Handled register
};

//Structure for loading an immediate value into a register
//Note: size of the immediate value is not specified here, the handler must be aware of the proper size
struct comp_compiler_mb_load_register
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u32 immediate;					//Immediate value in a longword
	comp_ppc_reg output_reg;			//Output register
};

//Structure for getting physical memory mapping into a register
struct comp_compiler_mb_map_physical_mem
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg output_reg;			//Destination register
	comp_ppc_reg input_reg;				//Source register
	comp_ppc_reg temp_reg;				//Preallocated temporary register
};

//Structure for accessing data on a memory address using
//target/source register, a base register and an offset
struct comp_compiler_mb_access_memory
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u16 offset;						//offset to the base register
	comp_ppc_reg output_reg;			//Output register
	comp_ppc_reg base_reg;				//Register for the base memory address
};

//Structure for accessing data on a memory address using
//target/source register and access size
struct comp_compiler_mb_access_memory_size
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg output_reg;			//Output register
	comp_ppc_reg base_reg;				//Register for the base memory address
	uae_u8 size;						//Access size: 1 - byte, 2 - word, 4 - longword
};

//Structure for shift opcode with immediate steps
struct comp_compiler_mb_shift_opcode_imm
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL update_flags;					//N and Z flags are updated if TRUE
	comp_ppc_reg output_reg;			//Target register
	comp_ppc_reg input_reg;				//Input register
	uae_u8 shift;						//Shift steps as immediate
};

//Structure for shift opcode with register steps
struct comp_compiler_mb_shift_opcode_reg
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL update_flags;					//N and Z flags are updated if TRUE
	comp_ppc_reg output_reg;			//Target register
	comp_ppc_reg input_reg;				//Input register
	comp_ppc_reg shift_reg;				//Shift steps in register
};

//Structure for shift opcode immediate steps with AND mask specified
struct comp_compiler_mb_shift_opcode_imm_with_mask
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL update_flags;					//N and Z flags are updated if TRUE
	comp_ppc_reg output_reg;					//Target register
	comp_ppc_reg input_reg;					//Input register
	uae_u8 shift;						//Shift steps to the left (immediate)
	uae_u8 begin_mask;					//Beginning of the mask (bit#)
	uae_u8 end_mask;					//End of the mask (bit#)
};

//Structure for shift opcode register steps with AND mask specified
struct comp_compiler_mb_shift_opcode_reg_with_mask
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL update_flags;					//N and Z flags are updated if TRUE
	comp_ppc_reg output_reg;			//Target register
	comp_ppc_reg input_reg;				//Input register
	comp_ppc_reg shift_reg;				//Shift steps to the left (immediate)
	uae_u8 begin_mask;					//Beginning of the mask (bit#)
	uae_u8 end_mask;					//End of the mask (bit#)
};

//Structure for extracting V flag for an arithmetic left shift instruction
struct comp_compiler_mb_extract_v_flag_arithmetic_left_shift
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg input_reg;				//Input register
	comp_ppc_reg shift_reg;				//Shift steps to the left
	comp_ppc_reg temp_reg;				//Temporary register for the operation
};

//Structure for extracting C flag for a shift instruction
struct comp_compiler_mb_extract_c_flag_shift
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_ppc_reg input_reg;				//Input register
	comp_ppc_reg shift_reg;				//Shift steps to the left
	BOOL left_shift;					//Boolean that specifies if left shift (TRUE) or right shift (FALSE) operation was done
};

//Structure for Regs structure slot operations
struct comp_compiler_mb_reg_in_slot
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8 slot;						//Target longword slot in Regs structure
	comp_ppc_reg reg;					//Input/output register
};

//Structure for setting a byte in a register from CRF0 Z flag
struct comp_compiler_mb_set_byte_from_z_flag
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL negate;						//Flag must be negated before use
	comp_ppc_reg output_reg;			//Output register
};

//Structure for setting the emulated PC if CRF0 Z flag is set
struct comp_compiler_mb_set_pc_on_z_flag
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	BOOL negate;						//Flag must be negated before use
	uae_u32 target_address;				//Target address as an immediate
	uae_u32 skip_address;				//Skipping to this address if condition is false
	comp_ppc_reg address_reg;			//Mapped address register
	comp_ppc_reg tmp_reg;				//Mapped temporary register
	comp_ppc_reg decrement_reg;			//Mapped decrement register (optional)
};

//Structure for division using two registers (32 bit / 16 bit -> 2 x 16 bit)
struct comp_compiler_mb_division_two_reg_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_exception_data exception_data;	//Data for the exception triggering
	comp_ppc_reg output_reg;			//Mapped output register
	comp_ppc_reg divisor_reg;			//Mapped divisor register
	comp_ppc_reg dividend_reg;			//Mapped dividend register
	comp_ppc_reg temp_reg1;				//Mapped temporary#1 register
	comp_ppc_reg temp_reg2;				//Mapped temporary#2 register
	BOOL signed_division;				//If TRUE then this division is a signed operation, unsigned otherwise
};

//Structure for division using three registers (32 bit / 32 bit -> 32 bit, 32 bit)
struct comp_compiler_mb_division_three_reg_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_exception_data exception_data;	//Data for the exception triggering
	comp_ppc_reg remainder_reg;			//Mapped remainder output register
	comp_ppc_reg divisor_reg;			//Mapped divisor register
	comp_ppc_reg quotient_reg;			//Mapped dividend and quotient register
	comp_ppc_reg temp_reg;				//Mapped temporary register
	BOOL signed_division;				//If TRUE then this division is a signed operation, unsigned otherwise
};

//Structure for 64 bit division using three registers (64 bit / 32 bit -> 32 bit, 32 bit)
struct comp_compiler_mb_division_64_bit_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	comp_exception_data exception_data;	//Data for the exception triggering
	comp_ppc_reg divisor_reg;			//Mapped divisor register
	int dividend_low_reg_num;			//Number of the 68k register for dividend and quotient
	int dividend_high_reg_num;			//Number of the 68k register for dividend and remainder
	BOOL signed_division;				//If TRUE then this division is a signed operation, unsigned otherwise
};

//Structure for comparing a word from memory to a reference word
struct comp_compiler_mb_check_word_in_memory
{
	struct comp_compiler_mb mb;
	comp_ppc_reg tmpreg;				//Temporary register for the operation
	comp_ppc_reg locreg;				//Temporary register for the location
	uae_u32 location;					//Memory address for the location of the word
	uae_u16 content;					//Reference content word
};


//Union of all macroblock descriptor structures
union comp_compiler_mb_union
{
	struct comp_compiler_mb base;
	struct comp_compiler_mb_unsupported unsupported;
	struct comp_compiler_mb_one_reg_opcode one_reg_opcode;
	struct comp_compiler_mb_two_regs_opcode two_regs_opcode;
	struct comp_compiler_mb_two_regs_opcode_flags two_regs_opcode_flags;
	struct comp_compiler_mb_two_regs_imm_opcode two_regs_imm_opcode;
	struct comp_compiler_mb_three_regs_opcode three_regs_opcode;
	struct comp_compiler_mb_three_regs_opcode_flags three_regs_opcode_flags;
	struct comp_compiler_mb_shift_opcode_imm shift_opcode_imm;
	struct comp_compiler_mb_shift_opcode_reg shift_opcode_reg;
	struct comp_compiler_mb_shift_opcode_imm_with_mask shift_opcode_imm_with_mask;
	struct comp_compiler_mb_shift_opcode_reg_with_mask shift_opcode_reg_with_mask;
	struct comp_compiler_mb_load_register load_register;
	struct comp_compiler_mb_access_memory access_memory;
	struct comp_compiler_mb_access_memory_size access_memory_size;
	struct comp_compiler_mb_map_physical_mem map_physical_mem;
	struct comp_compiler_mb_reg_in_slot reg_in_slot;
	struct comp_compiler_mb_extract_v_flag_arithmetic_left_shift extract_v_flag_arithmetic_left_shift;
	struct comp_compiler_mb_extract_c_flag_shift extract_c_flag_shift;
	struct comp_compiler_mb_set_byte_from_z_flag set_byte_from_z_flag;
	struct comp_compiler_mb_set_pc_on_z_flag set_pc_on_z_flag;
	struct comp_compiler_mb_division_two_reg_opcode division_two_reg_opcode;
	struct comp_compiler_mb_division_three_reg_opcode division_three_reg_opcode;
	struct comp_compiler_mb_division_64_bit_opcode division_64_bit_opcode;
	struct comp_compiler_mb_check_word_in_memory check_word_in_memory;
};
