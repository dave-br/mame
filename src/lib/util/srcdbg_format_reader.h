// TODO
/*********************************************************************

***************************************************************************/
#ifndef MAME_EMU_DEBUG_SRCDBG_FORMATREADER_H
#define MAME_EMU_DEBUG_SRCDBG_FORMATREADER_H

#pragma once

#include "srcdbg_format.h"

#include <string>
#include <cstdint>

using u16 = uint16_t;



class srcdbg_format_reader_callback
{
public:
	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) = 0;
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) = 0;
	virtual bool on_read_source_path(u16 source_path_index, std::string && source_path) = 0;
	virtual bool on_read_line_mapping(const srcdbg_line_mapping & line_map) = 0;
	virtual bool on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name) = 0;
	virtual bool on_read_global_constant_symbol_value(const global_constant_symbol_value & value) = 0;
	virtual bool on_read_local_constant_symbol_value(const local_constant_symbol_value & value) = 0;
	virtual bool on_read_local_dynamic_symbol_value(const local_dynamic_symbol_value & value) = 0;
};

bool srcdbg_format_read(const char * srcdbg_path, srcdbg_format_reader_callback & callback, std::string & error);



#endif // MAME_EMU_DEBUG_SRCDBG_FORMATREADER_H