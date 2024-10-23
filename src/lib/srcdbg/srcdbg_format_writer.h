// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_format_writer.h

    Library of helper functions to generate MAME source-level
	debugging info files

***************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


/*
ABI register numbers for Motorola 8/16-bit processors
taken from https://refspecs.linuxfoundation.org/elf/m8-16eabi.pdf

Register Name					Number		68HC05	68HC08	68HC11	68HC12	68HC16
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

// For tools targeting 6809, these values are for the reg parameter to
// mame_srcdbg_simp_add_local_dynamic_symbol()

// TODO: MAME symbol evaluation assumes these values match those of each
// enum under devices\cpu.  What is the best way to ensure this?  Should
// those enums be lifted out into headers like this intended for 3rd-party tools?
#define MAME_DBGSRC_REGISTER_6809_PC	-1
#define MAME_DBGSRC_REGISTER_6809_SP	0
#define MAME_DBGSRC_REGISTER_6809_CC	1
#define MAME_DBGSRC_REGISTER_6809_A		2
#define MAME_DBGSRC_REGISTER_6809_B		3
#define MAME_DBGSRC_REGISTER_6809_D		4
#define MAME_DBGSRC_REGISTER_6809_U		5
#define MAME_DBGSRC_REGISTER_6809_X		6
#define MAME_DBGSRC_REGISTER_6809_Y		7
#define MAME_DBGSRC_REGISTER_6809_DP	8


/*********************************************************************
  Functions for generating "simple" format debugging info files
**********************************************************************/

void * __cdecl mame_srcdbg_simp_open_new(const char * file_path);
unsigned short __cdecl mame_srcdbg_simp_add_source_file_path(void * srcdbg_simp_state, const char * source_file_path);
void __cdecl mame_srcdbg_simp_add_line_mapping(void * srcdbg_simp_state, unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number);
void __cdecl mame_srcdbg_simp_add_global_constant_symbol(void * srcdbg_simp_state, const char * symbol_name, int symbol_value);
void __cdecl mame_srcdbg_simp_add_local_constant_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value);
void __cdecl mame_srcdbg_simp_add_local_dynamic_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset);
void __cdecl mame_srcdbg_simp_close(void * srcdbg_simp_state);

#ifdef __cplusplus
}
#endif