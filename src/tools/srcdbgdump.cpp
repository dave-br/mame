// license:BSD-3-Clause
// copyright-holders:David Broman
/***************************************************************************

    srcdbgdump.cpp

    MAME source-level debugging info dump utility

***************************************************************************/

#include "srcdbg_format_reader.h"

#include "corefile.h"

#include <cstdio>
#include <string>

using u16 = uint16_t;
using u32 = uint32_t;


class srcdbg_dump : public srcdbg_format_reader_callback
{
public:
	srcdbg_dump() 
		: m_printed_source_file_paths_title(false)
		, m_printed_line_mapping_title(false)
		, m_printed_symbol_names_title(false)
		, m_printed_global_fixed_symbol_value_title(false)
		, m_printed_local_fixed_symbol_value_title(false)
		, m_printed_local_relative_symbol_value_title(false)
		{}
	int go();
	void print_error(const char * when, const char * what);
	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) override;
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) override;
	virtual bool on_read_source_path(u16 source_path_index, std::string && source_path) override;
	virtual bool on_read_line_mapping(const srcdbg_line_mapping & line_map) override;
	virtual bool on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name) override;
	virtual bool on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value) override;
	virtual bool on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value) override;
	virtual bool on_read_local_relative_symbol_value(const local_relative_symbol_value & value) override;

private:
	bool m_printed_source_file_paths_title;
	bool m_printed_line_mapping_title;
	bool m_printed_symbol_names_title;
	bool m_printed_global_fixed_symbol_value_title;
	bool m_printed_local_fixed_symbol_value_title;
	bool m_printed_local_relative_symbol_value_title;
};


bool srcdbg_dump::on_read_header_base(const mame_debug_info_header_base & header_base)
{
	printf("magic:\t'%.4s'\n", header_base.magic);
	printf("type:\t'%.4s'\n", header_base.type);
	printf("version:\t%d\n", u32(header_base.version));
	return true;
}

bool srcdbg_dump::on_read_simp_header(const mame_debug_info_simple_header & simp_header)
{
	printf("\nReading simple format...\n\n");
	printf("source_file_paths_size:\t%u\n", simp_header.source_file_paths_size);
	printf("num_line_mappings:\t%u\n", simp_header.num_line_mappings);
	printf("symbol_names_size:\t%u\n", simp_header.symbol_names_size);
	printf("num_global_fixed_symbol_values:\t%u\n", simp_header.num_global_fixed_symbol_values);
	printf("local_fixed_symbol_values_size:\t%u\n", simp_header.local_fixed_symbol_values_size);
	printf("local_relative_symbol_values_size:\t%u\n", simp_header.local_relative_symbol_values_size);
	return true;
}

bool srcdbg_dump::on_read_source_path(u16 source_path_index, std::string && source_path)
{
	if (!m_printed_source_file_paths_title)
	{
		printf("\n**** Source file paths: ****\n");
		m_printed_source_file_paths_title = true;
	}
	printf("%s (index %u)\n", source_path.c_str(), u32(source_path_index));
	return true;
}

bool srcdbg_dump::on_read_line_mapping(const srcdbg_line_mapping & line_map)
{
	if (!m_printed_line_mapping_title)
	{
		printf("\n**** Line mappings: ****\n");
		m_printed_line_mapping_title = true;
	}
	printf("address_first: $%X\taddress_last: $%X\tsource_file_index: %u\tline_number: %u\n",
		u32(line_map.range.address_first),
		u32(line_map.range.address_last),
		u32(line_map.source_file_index),
		line_map.line_number);
	return true;
}

bool srcdbg_dump::on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name)
{
	if (!m_printed_symbol_names_title)
	{
		printf("\n**** Symbol names: ****\n");
		m_printed_symbol_names_title = true;
	}
	printf("%s (index %u)\n", symbol_name.c_str(), u32(symbol_name_index));
	return true;
}

bool srcdbg_dump::on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value)
{
	if (!m_printed_global_fixed_symbol_value_title)
	{
		printf("\n**** Global fixed symbol values: ****\n");
		m_printed_global_fixed_symbol_value_title = true;
	}
	printf("Symbol name index: %u, symbol value: %d\n", value.symbol_name_index, value.symbol_value);
	return true;
} 	

bool srcdbg_dump::on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value)
{
	if (!m_printed_local_fixed_symbol_value_title)
	{
		printf("\n**** Local fixed symbol values: ****\n");
		m_printed_local_fixed_symbol_value_title = true;
	}
	printf("Symbol name index: %u, symbol value: %d\n", value.symbol_name_index, value.symbol_value);
	for (u32 i = 0; i < value.num_address_ranges; i++)
	{
		printf("\taddress range: %04X-%04X\n", value.ranges[i].address_first, value.ranges[i].address_last);
	}
	return true;
}

bool srcdbg_dump::on_read_local_relative_symbol_value(const local_relative_symbol_value & value)
{
	if (!m_printed_local_relative_symbol_value_title)
	{
		printf("\n**** Local relative symbol values: ****\n");
		m_printed_local_relative_symbol_value_title = true;
	}
	printf("Symbol name index: %u\n", value.symbol_name_index);
	for (u32 i = 0; i < value.num_local_relative_ranges; i++)
	{
		printf(
			"\tvalue: (reg idx %d) + %d\taddress range: %04X-%04X\n", 
			value.local_relative_ranges[i].reg,
			value.local_relative_ranges[i].reg_offset,
			value.local_relative_ranges[i].range.address_first, 
			value.local_relative_ranges[i].range.address_last);
	}
	return true;
}


int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage:\ndbginfodump <path to MAME source debugging information file>\n");
		return 1;
	}

	printf("Dumping '%s'...\n", argv[1]);

	srcdbg_dump dumper;
	std::string error;
	if (!srcdbg_format_read(argv[1], dumper, error))
	{
		if (!error.empty())
		{
			fprintf(stderr, "%s\n", error.c_str());
		}
		return 1;
	}

	return 0;
}



// int orig(int argc, char *argv[])
// {
// 	if (argc != 2)
// 	{
// 		fprintf(stderr, "Usage:\ndbginfodump <path to mdi file>\n");
// 		return 1;
// 	}

// 	printf("Dumping '%s'...\n", argv[1]);
// 	std::vector<uint8_t> data;
// 	util::core_file::load(argv[1], data);

// 	// Read base header first to detemine type

// 	u32 i = 0;
// 	mame_debug_info_header_base header_base;
// 	memset(&header_base, 0, sizeof(header_base));
// 	read_field<mame_debug_info_header_base>(header_base, data, i);

// 	printf("magic:\t'%.4s'\n", header_base.magic);
// 	printf("type:\t'%.4s'\n", header_base.type);
	
// 	if (strncmp(header_base.type, "simp", 4) != 0)
// 	{
// 		fprintf(stderr, "Only type 'simp' is currently supported.\n");
// 		return 1;
// 	};
	
// 	printf("type:\t%d\n", (int) header_base.version);
	
// 	if (header_base.version != 1)
// 	{
// 		fprintf(stderr, "Only version 1 is currently supported.\n");
// 		return 1;
// 	};

// 	// Reread header as simple ('simp') type
// 	printf("\nReading simple format...\n\n");
// 	i = 0;
// 	mame_debug_info_simple_header header;
// 	memset(&header, 0, sizeof(header));
// 	read_field<mame_debug_info_simple_header>(header, data, i);

// 	printf("source_file_paths_size:\t%u\n", header.source_file_paths_size);
// 	printf("num_line_mappings:\t%u\n", header.num_line_mappings);

// 	u32 first_line_mapping = i + header.source_file_paths_size;
// 	if (data.size() <= first_line_mapping - 1)
// 	{
// 		fprintf(stderr, "File too small to contain reported source_file_paths_size=%u\n", header.source_file_paths_size);
// 		return 1;
// 	}
// 	if (data[first_line_mapping - 1] != '\0')
// 	{
// 		fprintf(stderr, "null terminator missing at end of last source file\n");
// 		return 1;
// 	}

// 	u32 first_symbol_name = first_line_mapping + (header.num_line_mappings * sizeof(srcdbg_line_mapping));
// 	u32 first_symbol_address = first_symbol_name + header.symbol_names_size;
// 	if (header.symbol_names_size > 0)
// 	{
// 		if (data.size() <= first_symbol_address)
// 		{
// 			fprintf(stderr, "File too small to contain reported symbol_names_size=%u\n", header.symbol_names_size);
// 			return 1;
// 		}

// 		if (data[first_symbol_address - 1] != '\0')
// 		{
// 			fprintf(stderr, "null terminator missing at end of last symbol name\n");
// 			return 1;
// 		}
// 	}

// 	printf("\n**** Source file paths: ****\n");
// 	u16 source_index = 0;
// 	for (; i < first_line_mapping; i++)
// 	{
// 		if (data[i] == '\0')
// 		{
// 			printf(" (index %u)\n", source_index++);
// 		}
// 		else
// 		{
// 			printf("%c", data[i]);
// 		}
// 	}

// 	printf("\n**** Line mappings: ****\n");
// 	for (u32 line_map_idx = 0; line_map_idx < header.num_line_mappings; line_map_idx++)
// 	{
// 		srcdbg_line_mapping line_map;
// 		memset(&line_map, 0, sizeof(line_map));
// 		read_field<srcdbg_line_mapping>(line_map, data, i);
// 		printf("address_first: $%X\taddress_last: $%X\tsource_file_index: %u\tline_number: %u\n",
// 			u32(line_map.address_first),
// 			u32(line_map.address_last),
// 			u32(line_map.source_file_index),
// 			line_map.line_number);
// 	}

// 	printf("\n**** Symbols: ****\n");
// 	u32 j = first_symbol_address;
// 	for (; i < first_symbol_address; i++)
// 	{
// 		if (data[i] == '\0')
// 		{
// 			printf("\t%d\n", *(int*) &data[j]);
// 			j += sizeof(int);
// 		}
// 		else
// 		{
// 			printf("%c", data[i]);
// 		}
// 	}
// }