// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_api.h

	Declarations of public API for manipulating MAME source-level
	debugging information file.

	Only functionality declared in this file and srcdbg_format.h are
	safe for use by tools outside of MAME.
	TODO: URL

    Tools should prefer using this header and linking with
    -lmame_srcdbg_static or -lmame_srcdbg_shared to generate debugging
	info files where possible, instead of directly including
	srcdbg_format.h to manually generate debugging info files.

	This header intentionally written in C to allow C-only tools
    (assemblers, compilers) to #include it.


***************************************************************************/

#ifndef MAME_SRCDBG_API_H
#define MAME_SRCDBG_API_H

// Define LIB_PUBLIC to identify client-facing API to be exported
// from the so / dll, depending on platform and whether the library
// is being built or consumed.
//
// Assumes -fvisibility=hidden is used when compiling any files that
// contribute to the implementation of the library, so that symbols
// not adorned with LIB_PUBLIC default to not being exported.
//
// Note: It appears that mingw GCC currently ignores
// -fvisibility=hidden and just exports everything, whereas
// Linux GCC obeys.
//
// Adapted from https://gcc.gnu.org/wiki/Visibility
#if !defined MAME_SRCDBG_SHARED
	#define LIB_PUBLIC
#elif defined _WIN32 || defined __CYGWIN__
	#ifdef BUILDING_LIB
		#ifdef __GNUC__
			#define LIB_PUBLIC __attribute__ ((dllexport))
		#else
			#define LIB_PUBLIC __declspec(dllexport)
		#endif
	#else    // Consuming lib from Windows
		#ifdef __GNUC__
			#define LIB_PUBLIC __attribute__ ((dllimport))
		#else
			#define LIB_PUBLIC __declspec(dllimport)
		#endif
	#endif
#else	    // NOT Windows, either building or consuming
	#define LIB_PUBLIC __attribute__ ((visibility ("default")))
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* For tools targeting 6809, these values are for the reg parameter to
   mame_srcdbg_simp_add_local_relative_symbol() */

#define MAME_SRCDBG_REGISTER_6809_PC    -1
#define MAME_SRCDBG_REGISTER_6809_SP    0
#define MAME_SRCDBG_REGISTER_6809_CC    1
#define MAME_SRCDBG_REGISTER_6809_A     2
#define MAME_SRCDBG_REGISTER_6809_B     3
#define MAME_SRCDBG_REGISTER_6809_D     4
#define MAME_SRCDBG_REGISTER_6809_U     5
#define MAME_SRCDBG_REGISTER_6809_X     6
#define MAME_SRCDBG_REGISTER_6809_Y     7
#define MAME_SRCDBG_REGISTER_6809_DP    8
// TODO: MAME symbol evaluation assumes these values match those of each
// enum under devices\cpu.  What is the best way to ensure this?  Should
// those enums be lifted out into headers like this intended for 3rd-party tools?


/*********************************************************************
  Error codes
**********************************************************************/

#define MAME_SRCDBG_E_SUCCESS           0
#define MAME_SRCDBG_E_OUTOFMEMORY       1
#define MAME_SRCDBG_E_IMPORT_FAILED     2
#define MAME_SRCDBG_E_FOPEN_ERROR       3
#define MAME_SRCDBG_E_FWRITE_ERROR      4
#define MAME_SRCDBG_E_FCLOSE_ERROR      5
#define MAME_SRCDBG_E_INVALID_SRC_IDX   6



/*********************************************************************
  Functions for generating "simple" format debugging info files

  All functions return one of the MAME_SRCDBG_E_* codes above
**********************************************************************/

/*
    mame_srcdbg_simp_open_new - Initiates the source-debugging information file writing process.
    [in] file_path - full path to the MAME source-debugging information file to create
    [out] srcdbg_simp_state - pointer to memory location to receive srcdbg_simp_state, which
        is passed to subsequent API calls
*/
LIB_PUBLIC int mame_srcdbg_simp_open_new(const char * file_path, void ** srcdbg_simp_state);

/*
    mame_srcdbg_simp_add_source_file_path - Adds a new source file path to the generated
    source-debugging information file.  If this is called repeatedly for the same source file
    path (case-sensitive), the same index will be returned.
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
    [in] source_file_path - source file path to add
    [out] index_ptr - pointer to memory location to receive the 0-based index assigned to source_file_path
*/
LIB_PUBLIC int mame_srcdbg_simp_add_source_file_path(void * srcdbg_simp_state, const char * source_file_path, unsigned int * index_ptr);

/*
    mame_srcdbg_simp_add_line_mapping - Adds a new mapping between a range of addresses and
    source file path / line number pair.  May be called repeatedly for the same source file path
    / line number pair, if that pair is associated with multiple address ranges.  No two calls
    should have intersecting address ranges.
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
    [in] address_first - Low address of the range (inclusive), representing address of first byte
        of first instruction
    [in] address_last - High address of the range (inclusive), representing address of first byte
        of last instruction
    [in] source_file_index - Index of source file path associated with the range
    [in] line_number - Line number associated with the range.
*/
LIB_PUBLIC int mame_srcdbg_simp_add_line_mapping(void * srcdbg_simp_state, unsigned short address_first, unsigned short address_last, unsigned int source_file_index, unsigned int line_number);

/*
    mame_srcdbg_simp_add_global_fixed_symbol - Adds a new global fixed symbol.  Such symbols
    are not limited to a scope, and represent a single value (such as a single variable address)
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
    [in] symbol_name - Name of symbol
    [in] symbol_value - Value of symbol (such as an address)
*/
LIB_PUBLIC int mame_srcdbg_simp_add_global_fixed_symbol(void * srcdbg_simp_state, const char * symbol_name, int symbol_value);

/*
    mame_srcdbg_simp_add_local_fixed_symbol - Adds a new local fixed symbol.  Such symbols
    are limited to a scope, and represent a single value (such as a single variable address).
    May be called repeatedly with the same symbol_name, if it can have different values in different scopes
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
    [in] symbol_name - Name of symbol
    [in] address_first - Low address of the scope (inclusive), representing address of first byte
        of first instruction
    [in] address_last - High address of the scope (inclusive), representing address of first byte
        of last instruction
    [in] symbol_value - Value of symbol (such as an address)
*/
LIB_PUBLIC int mame_srcdbg_simp_add_local_fixed_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value);

/*
    mame_srcdbg_simp_add_local_relative_symbol - Adds a new local relative symbol.  Such symbols
    are limited to a scope, and represent a value calculated as an offset to a register (such
    as an offset to a stack or frame register).  May be called repeatedly with the same
    symbol_name, if it can have different register offsets in different scopes.
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
    [in] symbol_name - Name of symbol
    [in] address_first - Low address of the scope (inclusive), representing address of first byte
        of first instruction
    [in] address_last - High address of the scope (inclusive), representing address of first byte
        of last instruction
    [in] reg - Register identifier this symbol is offset from.  See list of MAME_SRCDBG_REGISTER_* values
    [in] reg_offset - Offset to be applied to register's value to produce the value of this symbol
*/
LIB_PUBLIC int mame_srcdbg_simp_add_local_relative_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset);

// TODO
LIB_PUBLIC int mame_srcdbg_simp_import(void * srcdbg_simp_state, const char * mdi_file_path_to_import, short offset, char * error_details, unsigned int num_bytes_error_details);

/*
    mame_srcdbg_simp_close - Ends the source-debugging information file writing process.  Writes
        accumulated information to the source-debugging information file, and closes it.
    [in] srcdbg_simp_state - Handle to source-debugging information file generation, as returned by mame_srcdbg_simp_open_new
*/
LIB_PUBLIC int mame_srcdbg_simp_close(void * srcdbg_simp_state);

#ifdef __cplusplus
}
#endif

#endif /* MAME_SRCDBG_FORMAT_WRITER_H */
