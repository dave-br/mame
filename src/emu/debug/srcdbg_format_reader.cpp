// TODO
/***************************************************************************


***************************************************************************/

#include "srcdbg_format_reader.h"
#include "mdisimple.h"

#include "corefile.h"

#include <cstdio>

using u16 = uint16_t;
using u32 = uint32_t;


template <typename T> static bool read_field(const T * & var, const std::vector<uint8_t> & data, u32 & i, std::string & error)
{
	if (data.size() < i + sizeof(T))
	{
		error = "File ended prematurely while reading next field of size ";
		error += sizeof(T);
		return false;
	}

	var = (const T *)(&data[i]);
	i += sizeof(T);
	return true;
}

static bool scan_bytes(u32 num_bytes, const std::vector<uint8_t>& data, u32& i, std::string & error)
{
	if (data.size() < i + num_bytes)
	{
		error = "File ended prematurely while scanning ahead ";
		error += num_bytes;
		error += " bytes.";
		return false;
	}

	i += num_bytes;
	return true;
}

bool srcdbg_format_read(const char * srcdbg_path, srcdbg_format_reader_callback & callback, std::string & error)
{
	std::vector<uint8_t> data;
	util::core_file::load(srcdbg_path, data);

	// Read base header first to detemine type
	u32 i = 0;
	const mame_debug_info_header_base * header_base;
	if (!read_field<mame_debug_info_header_base>(header_base, data, i, error))
	{
		return false;
	}
	if (!callback.on_read_header_base(*header_base))
	{
		return true;
	}

	// Reread header as simple ('simp') type
	i = 0;
	const mame_debug_info_simple_header * header;
	if (!read_field<mame_debug_info_simple_header>(header, data, i, error))
	{
		return false;
	}
	if (!callback.on_read_simp_header(*header))
	{
		return true;
	}

	u32 first_line_mapping = i + header->source_file_paths_size;
	u32 first_symbol_name = first_line_mapping + (header->num_line_mappings * sizeof(mdi_line_mapping));
	u32 after_symbol_names = first_symbol_name + header->symbol_names_size;
	if (header->symbol_names_size > 0)
	{
		if (data.size() <= after_symbol_names)
		{
			error = "File too small to contain reported symbol_names_size=";
			error += header->symbol_names_size;
			return false;
		}

		if (data[after_symbol_names - 1] != '\0')
		{
			error = "null terminator missing at end of last symbol name";
			return false;
		}
	}
	std::string str;
	u16 source_index = 0;
	for (; i < first_line_mapping; i++)
	{
		if (data[i] == '\0')
		{
			if (!callback.on_read_source_path(source_index++, std::move(str)))
			{
				return true;
			}
			str.clear();
		}
		else
		{
			str += char(data[i]);
		}
	}

	for (u32 line_map_idx = 0; line_map_idx < header->num_line_mappings; line_map_idx++)
	{
		const mdi_line_mapping * line_map;
		if (!read_field<mdi_line_mapping>(line_map, data, i, error))
		{
			return false;
		}
		if (!callback.on_read_line_mapping(*line_map))
		{
			return true;
		}
	}

	u16 symbol_index = 0;
	for (; i < after_symbol_names; i++)
	{
		if (data[i] == '\0')
		{
			if (!callback.on_read_symbol_name(symbol_index++, std::move(str)))
			{
				return true;
			}
			str.clear();
		}
		else
		{
			str += char(data[i]);
		}
	}

	for (u32 global_constant_idx = 0; global_constant_idx < header->num_global_constant_symbol_values; global_constant_idx++)
	{
		const global_constant_symbol_value * value;
		if (!read_field<global_constant_symbol_value>(value, data, i, error))
		{
			return false;
		}
		if (!callback.on_read_global_constant_symbol_value(*value))
		{
			return true;
		}
	}

	u32 after_local_constant_symbol_values = i + header->local_constant_symbol_values_size;
	while (i < after_local_constant_symbol_values)
	{
		const local_constant_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_constant_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_address_ranges * sizeof(address_range), data, i, error))
		{
			return false;
		}
		if (!callback.on_read_local_constant_symbol_value(*(const local_constant_symbol_value *) &data[value_start_idx]))
		{
			return true;
		}
	}

	u32 after_local_dynamic_symbol_values = i + header->local_dynamic_symbol_values_size;
	while (i < after_local_dynamic_symbol_values)
	{
		const local_dynamic_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_dynamic_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_local_dynamic_symbol_entries * sizeof(local_dynamic_symbol_entry), data, i, error))
		{
			return false;
		}
		if (!callback.on_read_local_dynamic_symbol_value(*(const local_dynamic_symbol_value *) &data[value_start_idx]))
		{
			return true;
		}
	}

	// TODO: MORE CHECKING OF HEADER SIZE VALS AND NUM_SYMS FOR LOCS
	return true;
}