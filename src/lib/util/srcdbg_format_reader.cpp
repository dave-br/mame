// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_format_reader.cpp

    Helper for reading MAME source-level debugging info files

***************************************************************************/


#include "srcdbg_format_reader.h"

#include "corefile.h"

#include <cstdio>


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
		std::ostringstream err;
		err << "File ended prematurely while scanning ahead " << num_bytes << " bytes.";
		error = std::move(std::move(err).str());
		return false;
	}

	i += num_bytes;
	return true;
}

bool srcdbg_format_header_read(const char * srcdbg_path, srcdbg_format & format, std::string & error)
{
	util::core_file::ptr file;
	std::error_condition err_code = util::core_file::open(srcdbg_path, OPEN_FLAG_READ, file);
	if (err_code)
	{
		error = util::string_format("Error opening file\n%s\n\nError code: %d", srcdbg_path, err_code);
		return false;
	}

	mame_debug_info_header_base header;
	std::size_t actual_size;
	err_code = file->read_some(&header, sizeof(header), actual_size);
	if (err_code)
	{
		error = util::string_format("Error reading from file\n%s\n\nError code: %d", srcdbg_path, err_code);
		return false;
	}

	if (actual_size != sizeof(header))
	{
		error = util::string_format("Error reading from file\n%s\n\nFile too small to contain header", srcdbg_path);
		return false;
	}

	// FUTURE: If machines for which "simp" is unsuitable get their own formats created,
	// add code here to read the format identifier and return the appropriate format enum

	if (strncmp(header.type, "simp", 4) != 0)
	{
		error = util::string_format("Error while reading header from file\n%s\n\nOnly type 'simp' is currently supported", srcdbg_path);
		return false;
	};
	
	if (header.version != 1)
	{
		error = util::string_format("Error while reading header from file\n%s\n\nOnly version 1 is currently supported", srcdbg_path);
		return false;
	};

	format = SRCDBG_FORMAT_SIMPLE;
	return true;
}


bool srcdbg_format_simp_read(const char * srcdbg_path, srcdbg_format_reader_callback & callback, std::string & error)
{
	std::vector<uint8_t> data;
	std::error_condition err_code = util::core_file::load(srcdbg_path, data);
	if (err_code)
	{
		error = util::string_format("Error opening file\n%s\n\nError code: %d", srcdbg_path, err_code);
		return false;
	}

	u32 i = 0;
	const mame_debug_info_simple_header * header;
	if (!read_field<mame_debug_info_simple_header>(header, data, i, error) ||
		!callback.on_read_simp_header(*header))
	{
		return false;
	}

	u32 first_line_mapping = i + header->source_file_paths_size;
	if (data.size() <= first_line_mapping - 1)
	{
		error = util::string_format("Error reading file\n%s\n\nFile too small to contain reported source_file_paths_size=%d", srcdbg_path, header->source_file_paths_size);
		return false;
	}
	if (data[first_line_mapping - 1] != '\0')
	{
		error = util::string_format("Error reading file\n%s\n\nnull terminator missing at end of last source file", srcdbg_path);
		return false;
	}

	u32 first_symbol_name = first_line_mapping + (header->num_line_mappings * sizeof(srcdbg_line_mapping));
	u32 after_symbol_names = first_symbol_name + header->symbol_names_size;
	if (header->symbol_names_size > 0)
	{
		if (data.size() <= after_symbol_names)
		{
			error = util::string_format("Error reading file\n%s\n\nFile too small to contain reported symbol_names_size=%d", srcdbg_path, header->symbol_names_size);
			return false;
		}

		if (data[after_symbol_names - 1] != '\0')
		{
			error = util::string_format("Error reading file\n%s\n\nnull terminator missing at end of last symbol name", srcdbg_path);
			return false;
		}
	}

	u32 after_global_fixed_symbol_values = 
		after_symbol_names + header->num_global_fixed_symbol_values * sizeof(global_fixed_symbol_value);
	if (data.size() < after_global_fixed_symbol_values)
	{
		error = util::string_format("Error reading file\n%s\n\nFile too small to contain reported num_global_fixed_symbol_values=%d", srcdbg_path, header->num_global_fixed_symbol_values);
		return false;
	}

	u32 after_local_fixed_symbol_values = after_global_fixed_symbol_values + header->local_fixed_symbol_values_size;
	if (data.size() < after_local_fixed_symbol_values)
	{
		error = util::string_format("Error reading file\n%s\n\nFile too small to contain reported local_fixed_symbol_values_size=%d", srcdbg_path, header->local_fixed_symbol_values_size);
		return false;
	}

	u32 after_local_relative_symbol_values = after_local_fixed_symbol_values + header->local_relative_symbol_values_size;
	if (data.size() != after_local_relative_symbol_values)
	{
		error = util::string_format("Error reading file\n%s\n\nFile size (%d) not an exact match to the sum of section sizes reported in header", srcdbg_path, data.size());
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
			error = util::string_format("Error reading file\n%s\n\nInvalid source_file_index encountered in line map: %d", srcdbg_path, line_map->source_file_index);
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

	for (u32 global_fixed_idx = 0; global_fixed_idx < header->num_global_fixed_symbol_values; global_fixed_idx++)
	{
		const global_fixed_symbol_value * value;
		if (!read_field<global_fixed_symbol_value>(value, data, i, error) ||
			!callback.on_read_global_fixed_symbol_value(*value))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	while (i < after_local_fixed_symbol_values)
	{
		const local_fixed_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_fixed_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_address_ranges * sizeof(address_range), data, i, error) ||
			!callback.on_read_local_fixed_symbol_value(*(const local_fixed_symbol_value *) &data[value_start_idx]))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	while (i < after_local_relative_symbol_values)
	{
		const local_relative_symbol_value * value;
		u32 value_start_idx = i;
		if (!read_field<local_relative_symbol_value>(value, data, i, error) ||
			!scan_bytes(value->num_local_relative_eval_rules * sizeof(local_relative_eval_rule), data, i, error) ||
			!callback.on_read_local_relative_symbol_value(*(const local_relative_symbol_value *) &data[value_start_idx]))
		{
			return false;
		}
		// TODO: INVALID SYMBOL INDEX
	}

	// TODO: MORE CHECKING OF HEADER SIZE VALS AND NUM_SYMS FOR LOCS
	return true;
}