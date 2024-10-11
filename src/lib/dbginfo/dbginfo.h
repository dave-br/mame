#ifdef __cplusplus
extern "C" {
#endif


/*
ABI register numbers for Motorola 8/16-bit processors
taken from https://refspecs.linuxfoundation.org/elf/m8-16eabi.pdf

Register Name					Number		HC05	HC08	HC11	HC12	HC16
Accumulator A 					0			X		X		X		X		X
Accumulator B 					1							X		X		X
Accumulator D 					3							X		X		X
Accumulator E 					4 											X
Index Register H 				6 					X
Index Register X 				7			X		X		X		X		X
Index Register Y 				8							X		X		X
Index Register Z 				9											X
Stack Pointer 					15			X		X		X		X		X
Program Counter 				16			X		X		X		X		X
Condition Code Register 		17			X		X		X		X		X
Address Extension Register (K) 	20											X
Stack Extension Register (SK) 	21											X
MAC Multiplier Register (HR) 	27											X
MAC Multiplicand Register (IR)	28											X
MAC Accumulator (AM) 			29											X
MAC XY Mask Register 			30											X
*/


// Cannot find ABI register numbers for 6809, so extending the above
#define MAME_DBGSRC_REGISTER_6809_A		0
#define MAME_DBGSRC_REGISTER_6809_B		1
#define MAME_DBGSRC_REGISTER_6809_D		3
#define MAME_DBGSRC_REGISTER_6809_X		7
#define MAME_DBGSRC_REGISTER_6809_Y		8
#define MAME_DBGSRC_REGISTER_6809_U		10
#define MAME_DBGSRC_REGISTER_6809_DP	11
#define MAME_DBGSRC_REGISTER_6809_SP	15
#define MAME_DBGSRC_REGISTER_6809_PC	16
#define MAME_DBGSRC_REGISTER_6809_CC	17


void * __cdecl mame_mdi_simp_open_new(const char * file_path);
unsigned short __cdecl mame_mdi_simp_add_source_file_path(void * mdi_simp_state, const char * source_file_path);
void __cdecl mame_mdi_simp_add_line_mapping(void * mdi_simp_state, unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number);
void __cdecl mame_mdi_simp_add_global_constant_symbol(void * mdi_simp_state, const char * symbol_name, int symbol_value);
void __cdecl mame_mdi_simp_add_local_constant_symbol(void * mdi_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value);
void __cdecl mame_mdi_simp_add_local_dynamic_symbol(void * mdi_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char register, int register_offset);
void __cdecl mame_mdi_simp_close(void * mdi_simp_state);

#ifdef __cplusplus
}
#endif