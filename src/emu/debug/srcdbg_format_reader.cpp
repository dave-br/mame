// TODO
/***************************************************************************


***************************************************************************/

#include "mdisimple.h"
#include "srcdbg_format_reader.h"

#include "corefile.h"


#include <cstdio>

using u16 = uint16_t;
using u32 = uint32_t;


template <typename T> static bool read_field(T& var, std::vector<uint8_t>& data, u32& i)
{
	if (data.size() < i + sizeof(T))
	{
		fprintf(stderr, "File ended prematurely while reading next field of size %u.\n", i);
		return false;
	}

	var = *(T*) &data[i];
	i += sizeof(T);
	return true;
}

int srcdbg_format_read(srcdbg_format_reader_callback & callback)
{
	std::vector<uint8_t> data;

	// Read base header first to detemine type
	u32 i = 0;
	mame_debug_info_header_base header_base;
	memset(&header_base, 0, sizeof(header_base));
	read_field<mame_debug_info_header_base>(header_base, data, i);
	if (!callback.on_read_header_base(header_base))
	{
		return 0;
	}

	// Reread header as simple ('simp') type
	i = 0;
	mame_debug_info_simple_header header;
	memset(&header, 0, sizeof(header));
	read_field<mame_debug_info_simple_header>(header, data, i);
	if (!callback.on_read_simp_header(header))
	{
		return 0;
	}

	u32 first_line_mapping = i + header.source_file_paths_size;
	u32 first_symbol_name = first_line_mapping + (header.num_line_mappings * sizeof(mdi_line_mapping));
	u32 after_symbol_names = first_symbol_name + header.symbol_names_size;
	// if (header.symbol_names_size > 0)
	// {
	// 	if (data.size() <= first_symbol_address)
	// 	{
	// 		fprintf(stderr, "File too small to contain reported symbol_names_size=%u\n", header.symbol_names_size);
	// 		return 1;
	// 	}

	// 	if (data[first_symbol_address - 1] != '\0')
	// 	{
	// 		fprintf(stderr, "null terminator missing at end of last symbol name\n");
	// 		return 1;
	// 	}
	// }
	std::string str;
	u16 source_index = 0;
	for (; i < first_line_mapping; i++)
	{
		if (data[i] == '\0')
		{
			callback.on_read_source_path(source_index++, std::move(str));
			str.clear();
		}
		else
		{
			str += char(data[i]);
		}
	}

	for (u32 line_map_idx = 0; line_map_idx < header.num_line_mappings; line_map_idx++)
	{
		mdi_line_mapping line_map;
		memset(&line_map, 0, sizeof(line_map));
		read_field<mdi_line_mapping>(line_map, data, i);
		callback.on_read_line_mapping(line_map);
	}

	u16 symbol_index = 0;
	for (; i < after_symbol_names; i++)
	{
		if (data[i] == '\0')
		{
			callback.on_read_symbol_name(symbol_index++, std::move(str));
			str.clear();
		}
		else
		{
			str += char(data[i]);
		}
	}

	u32 j = first_symbol_address;
	for (; i < first_symbol_address; i++)
	{
		if (data[i] == '\0')
		{
			printf("\t%d\n", *(int*) &data[j]);
			j += sizeof(int);
		}
		else
		{
			printf("%c", data[i]);
		}
	}
}