/**
 * Header file for the JIT macroblock emitter functions
 */

//Support compiling functions
void comp_opcode_init(const cpu_history* history, uae_u8 extension);
void comp_opcode_unsupported(uae_u16 opcode);
