// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_format.h

    Helper for reading MAME source-level debugging info files

***************************************************************************/


#ifndef MAME_UTIL_SRCDBG_FORMATREADER_H
#define MAME_UTIL_SRCDBG_FORMATREADER_H

#pragma once

#include "srcdbg_format.h"

#include <string>
#include <cstdint>

using u16 = uint16_t;
using u32 = uint32_t;


enum srcdbg_format
{
	SRCDBG_FORMAT_SIMPLE,
};

// Base class from which a caller of srcdbg_format_simp_read derives their
// callback implementations
class srcdbg_format_reader_callback
{
public:
	// Callbacks implemented by subclasses to react as portions of the file
	// are read.  Return true to continue reading or false to abort.
	// The "on_" functions pass an element as it is read
	// The "end_" functions notify that all elements of a given kind have been read
	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) { return true; }
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) { return true; }
	virtual bool on_read_source_path(u32 source_path_index, std::string && source_path ) { return true; }
	virtual bool end_read_source_paths() { return true; }
	virtual bool on_read_line_mapping(const srcdbg_line_mapping & line_map) { return true; }
	virtual bool end_read_line_mappings() { return true; }
	virtual bool on_read_symbol_name(u32 symbol_name_index, std::string && symbol_name) { return true; }
	virtual bool end_read_symbol_names() { return true; }
	virtual bool on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value) { return true; }
	virtual bool end_read_global_fixed_symbol_values() { return true; }
	virtual bool on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value) { return true; }
	virtual bool end_read_local_fixed_symbol_values() { return true; }
	virtual bool on_read_local_relative_symbol_value(const local_relative_symbol_value & value) { return true; }
	virtual bool end_read_local_relative_symbol_values() { return true; }
};

// Call this first to determine the source-level debugging information
// file format and verify base header fields
bool srcdbg_format_header_read(const char * srcdbg_path, srcdbg_format & format, std::string & error);

// Call this to fully read a source-level debugging information file determined
// to be in the "simple" format
bool srcdbg_format_simp_read(const char * srcdbg_path, srcdbg_format_reader_callback & callback, std::string & error);



#endif // MAME_UTIL_SRCDBG_FORMATREADER_H
