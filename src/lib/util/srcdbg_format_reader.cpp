// TODO
/***************************************************************************


***************************************************************************/

#include "srcdbg_format_reader.h"

#include "corefile.h"

#include <cstdio>

using u32 = uint32_t;


template <typename T> static bool read_field(const T * & var, const std::vector<uint8_t> & data, u32 & i, std::string & error)
{
	if (data.size() < i + sizeof(T))
	{
		std::ostringstream err;
		err << "File ended prematurely while reading next field of size " << sizeof(T);
		error = std::move(std::move(err).str());
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
	if (!read_field<mame_debug_info_header_base>(header_base, data, i, error) ||
	 	!callback.on_read_header_base(*header_base))
	{
		return false;
	}

	if (strncmp(header_base->type, "simp", 4) != 0)
	{
		error = "Error while reading header: Only type 'simp' is currently supported";
		return false;
	};
	
	if (header_base->version != 1)
	{
		error = "Error while reading header: Only version 1 is currently supported";
		return false;
	};

	// Reread header as simple ('simp') type
	i = 0;
	const mame_debug_info_simple_header * header;
	if (!read_field<mame_debug_info_simple_header>(header, data, i, error) ||
		!callback.on_read_simp_header(*header))
	{
		return false;
	}

	u32 first_line_mapping = i + header->source_file_paths_size;
	if (data.size() <= first_line_mapping - 1)
	{
		error = "File too small to contain reported source_file_paths_size=";
		error += header->source_file_paths_size;
		return false;
	}
	if (data[first_line_mapping - 1] != '\0')
	{
		error = "null terminator missing at end of last source file";
		return false;
	}

	u32 first_symbol_name = first_line_mapping + (header->num_line_mappings * sizeof(srcdbg_line_mapping));
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

	u32 after_global_constant_symbol_values = 
		after_symbol_names + header->num_global_constant_symbol_values * sizeof(global_constant_symbol_value);
	if (data.size() < after_global_constant_symbol_values)
	{
		error = "File too small to contain reported num_global_constant_symbol_values=";
		error += header->num_global_constant_symbol_values;
		return false;
	}

	u32 after_local_constant_symbol_values = after_global_constant_symbol_values + header->local_constant_symbol_values_size;
	if (data.size() < after_local_constant_symbol_values)
	{
		error = "File too small to contain reported local_constant_symbol_values_size=";
		error += header->local_constant_symbol_values_size;
		return false;
	}

	u32 after_local_dynamic_symbol_values = after_local_constant_symbol_values + header->local_dynamic_symbol_values_size;
	if (data.size() != after_local_dynamic_symbol_values)
	{
		error = "File size (";
		error += data.size();
		error += ") not an exact match to the sum of section sizes reported in header";
		return false;
	}

	std::string str;
	u16 source_index = 0;
	for (; i < first_line_mapping; i++)
	{
		if (data[i] == '\0')
		{
			if (!callback.on_read_source_path(source_index++, std::move(str)))
			{
				return false;
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
		const srcdbg_line_mapping * line_map;
		if (!read_field<srcdbg_line_mapping>(line_map, data, i, error))
		{
			return false;
		}

		if (line_map->source_file_index >= source_index)
		{
			error = "Invalid source_file_index encountered in line map: ";
			error += line_map->source_file_index;
			return false;
		}

 		if (!callback.on_read_line_mapping(*line_map))
		{
			return false;
		}
	}

	u16 symbol_index = 0;
	str.clear();
	for (; i < after_symbol_names; i++)
	{
		if (data[i] == '\0')
		{
			if (!callback.on_read_symbol_name(symbol_index++, std::move(str)))
			{
				return false;
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
		if (!read_field<global_constant_symbol_value>(value, data, i, error) ||
			!callback.on_read_global_constant_symbol_value(*value))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	while (i < after_local_constant_symbol_values)
	{
		const local_constant_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_constant_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_address_ranges * sizeof(address_range), data, i, error) ||
			!callback.on_read_local_constant_symbol_value(*(const local_constant_symbol_value *) &data[value_start_idx]))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	while (i < after_local_dynamic_symbol_values)
	{
		const local_dynamic_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_dynamic_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_local_dynamic_scoped_values * sizeof(local_dynamic_scoped_value), data, i, error) ||
			!callback.on_read_local_dynamic_symbol_value(*(const local_dynamic_symbol_value *) &data[value_start_idx]))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	// TODO: MORE CHECKING OF HEADER SIZE VALS AND NUM_SYMS FOR LOCS
	return true;
}