/**
 * Header file for the JIT macroblock compiler implementation structures
 */

//Macroblock base description structure
struct comp_compiler_mb
{
	comp_compiler_macroblock_func* handler;	//Handler function for compiling the macroblock
	uae_u64 input_registers;				//Input registers, each register is marked with one bit, see COMP_COMPILER_MACROBLOCK_* constants
	uae_u64 output_registers;				//Output registers, each register is marked with one bit, see COMP_COMPILER_MACROBLOCK_* constants
	const char* name;						//Name of the macroblock
	uae_u16* m68k_ptr;						//Pointer to the instruction that was compiled
	void* start;							//Start address of the compiled code (or NULL)
	void* end;								//End address of the compiled code (only valid if there was a non-null start address)
};

//Structure for unsupported opcode macroblock
struct comp_compiler_mb_unsupported
{
	struct comp_compiler_mb mb;				//Default macroblock descriptor
	uae_u16	opcode;							//Unsupported opcode
};

//Structure for normal three register input: two input and one output registers
//For example: addco output_reg, input_reg1, input_reg2
struct comp_compiler_mb_three_regs_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8	input_reg1;					//Input register #1
	uae_u8	input_reg2;					//Input register #2
	uae_u8	output_reg;					//Output register
};

//Structure for normal two register and one immediate value input
//For example: addis. output_reg, input_reg, immediate
struct comp_compiler_mb_two_regs_imm_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u16	immediate;					//Immediate value
	uae_u8	input_reg;					//Input register
	uae_u8	output_reg;					//Output register
};

//Structure for normal two register input
//For example: mr output_reg, input_reg
struct comp_compiler_mb_two_regs_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8	input_reg;					//Input register
	uae_u8	output_reg;					//Output register
};

//Structure for normal one register input
//For example: mfcr reg
struct comp_compiler_mb_one_reg_opcode
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8	reg;						//Handled register
};

//Structure for loading an immediate value into a register
//Note: size of the immediate value is not specified here, the handler must be aware of the proper size
struct comp_compiler_mb_load_register
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u32 immediate;					//Immediate value in a longword
	uae_u8 output_reg;					//Output register
};

//Structure for getting physical memory mapping into a register
struct comp_compiler_mb_map_physical_mem
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8 output_reg;					//Destination register
	uae_u8 input_reg;					//Source register
	uae_u8 temp_reg;						//Preallocated temporary register
};

//Structure for accessing data on a memory address using
//target/source register, a base register and an offset
struct comp_compiler_mb_access_memory
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u16 offset;						//offset to the base register
	uae_u8 output_reg;					//Output register
	uae_u8 base_reg;					//Register for the base memory address
};

//Structure for accessing data on a memory address using
//target/source register and access size
struct comp_compiler_mb_access_memory_size
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	uae_u8 output_reg;					//Output register
	uae_u8 base_reg;					//Register for the base memory address
	uae_u8 size;						//Access size: 1 - byte, 2 - word, 4 - longword
};

//Structure for shift opcode with AND mask specified
struct comp_compiler_mb_shift_opcode_with_mask
{
	struct comp_compiler_mb mb;			//Default macroblock descriptor
	int update_flags;					//N and Z flags are updated if TRUE
	uae_u8 output_reg;					//Target register
	uae_u8 input_reg;					//Input register
	uae_u8 shift;						//Shift steps to the left
	uae_u8 begin_mask;					//Beginning of the mask (bit#)
	uae_u8 end_mask;					//End of the mask (bit#)
};

//Union of all macroblock descriptor structures
union comp_compiler_mb_union
{
	struct comp_compiler_mb base;
	struct comp_compiler_mb_unsupported unsupported;
	struct comp_compiler_mb_one_reg_opcode one_reg_opcode;
	struct comp_compiler_mb_two_regs_opcode two_regs_opcode;
	struct comp_compiler_mb_two_regs_imm_opcode two_regs_imm_opcode;
	struct comp_compiler_mb_three_regs_opcode three_regs_opcode;
	struct comp_compiler_mb_shift_opcode_with_mask shift_opcode_with_mask;
	struct comp_compiler_mb_load_register load_register;
	struct comp_compiler_mb_access_memory access_memory;
	struct comp_compiler_mb_access_memory_size access_memory_size;
	struct comp_compiler_mb_map_physical_mem map_physical_mem;
};
