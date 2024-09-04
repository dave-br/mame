/* All mame debug info headers derive from this */
struct mame_debug_info_header_base
{
	char magic[4];
	char type[4];
	unsigned char version;
};


/* mame's simple debug info format uses this as its complete header */
struct mame_debug_info_simple_header : mame_debug_info_header_base
{
	/* size in bytes of source_file_paths[][] */
	unsigned int source_file_paths_size;

	/* number of elements of line_mappings[] */
	unsigned int num_line_mappings;
};


/* 
	A single mapping FROM a CPU address corresponding to the first byte of a machine-language
	instruction TO the line number of a source file that assembled into that instruction
*/
struct mdi_line_mapping
{
	/* address in CPU space of first instruction for this line */
	unsigned short address;

	/* 0-based index of source file (from source_file_paths) containing this line */
	unsigned short source_file_index;

	/* 1-based line number in source file */
	unsigned int line_number;
};


/*
	mame_debug_info_simple format:
	
	mame_debug_info_simple_header   header
	char                            source_file_paths[][]
	mdi_line_mapping                line_mappings[]      

	Description:
	- Each source_file_paths[i] is a null-terminated string path to a source file.  The
	  first dimension index fits into an unsigned short.
	- line_mappings are ordered by (non-contiguous) address.  Only one entry per source
	  file line number, and only for line numbers corresponding to the first byte of a
	  machine-language instruction.  The dimension index fits into an unsigned int.

*/

   
