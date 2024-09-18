// license:BSD-3-Clause
// copyright-holders: TODO
/***************************************************************************

    dbginfodump.cpp

    MAME debugging info dump utility

***************************************************************************/

#include "mdisimple.h"

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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage:\ndbginfodump <path to mdi file>\n");
		return 1;
	}

	printf("Dumping '%s'...\n", argv[1]);
	std::vector<uint8_t> data;
	util::core_file::load(argv[1], data);

	// Read base header first to detemine type

	u32 i = 0;
	mame_debug_info_header_base header_base;
	memset(&header_base, 0, sizeof(header_base));
	read_field<mame_debug_info_header_base>(header_base, data, i);

	printf("magic:\t'%.4s'\n", header_base.magic);
	printf("type:\t'%.4s'\n", header_base.type);
	
	if (strncmp(header_base.type, "simp", 4) != 0)
	{
		fprintf(stderr, "Only type 'simp' is currently supported.\n");
		return 1;
	};
	
	printf("type:\t%d\n", (int) header_base.version);
	
	if (header_base.version != 1)
	{
		fprintf(stderr, "Only version 1 is currently supported.\n");
		return 1;
	};

	// Reread header as simple ('simp') type
	printf("\nReading simple format...\n\n");
	i = 0;
	mame_debug_info_simple_header header;
	memset(&header, 0, sizeof(header));
	read_field<mame_debug_info_simple_header>(header, data, i);

	printf("source_file_paths_size:\t%u\n", header.source_file_paths_size);
	printf("num_line_mappings:\t%u\n", header.num_line_mappings);

	u32 first_line_mapping = i + header.source_file_paths_size;
	if (data.size() <= first_line_mapping - 1)
	{
		fprintf(stderr, "File too small to contain reported source_file_paths_size\n");
		return 1;
	}
	if (data[first_line_mapping - 1] != '\0')
	{
		fprintf(stderr, "null terminator missing at end of last source file\n");
		return 1;
	}

	printf("\n**** Source file paths: ****\n");
	u16 source_index = 0;
	for (; i < first_line_mapping; i++)
	{
		if (data[i] == '\0')
		{
			printf(" (index %u)\n", source_index++);
		}
		else
		{
			printf("%c", data[i]);
		}
	}

	printf("\n**** Line mappings: ****\n");
	for (u32 line_map_idx = 0; line_map_idx < header.num_line_mappings; line_map_idx++)
	{
		mdi_line_mapping line_map;
		memset(&line_map, 0, sizeof(line_map));
		read_field<mdi_line_mapping>(line_map, data, i);
		printf("address_first: $%X\taddress_last: $%X\tsource_file_index: %u\tline_number: %u\n",
			u32(line_map.address_first),
			u32(line_map.address_last),
			u32(line_map.source_file_index),
			line_map.line_number);
	}
}