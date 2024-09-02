struct mame_debug_info_header
{
	char magic[4];
	char type[4];
	unsigned char version;
};

struct mdi_source_file_entry
{
	unsigned short source_file_index;   // 0-based index identifying source file
	char source_file_path[256];         // Includes null terminator
};

struct mdi_line_mapping
{
	unsigned short address;             // address in CPU space of first instruction for this line
	unsigned short source_file_index;   // 0-based index of source file containing this line
	unsigned line_number;               // 1-based line number in source file
};


/*
	mame_debug_info_simple format:
	
	mame_debug_info_header header
	u32                    size                   Number of bytes of rest of file (appearing after size field)
	u16                    num_source_files
	mdi_source_file_entry  source_files[]         (ordered by contiguous 0-based mdi_source_file_entry::source_file_index)
	u32                    num_line_mappings
	mdi_line_mapping       line_mappings[]        (ordered by (non-contiguous) address)

*/