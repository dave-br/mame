// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_format_writer.h

    Internal implementation of portions of srcdbg_api.h.
	
	WARNING: Tools external to MAME should only use functionality
	declared in srcdg_format.h and srcdbg_api.h.

***************************************************************************/

#ifndef MAME_SRCDBG_FORMAT_WRITER_H
#define MAME_SRCDBG_FORMAT_WRITER_H


#define RET_IF_FAIL(expr)                              \
	{                                                  \
		int _err = (expr);                             \
		if (_err != MAME_SRCDBG_E_SUCCESS)             \
			return _err;                               \
	}

#define FWRITE_OR_RETURN(BUF, SIZE, COUNT, STREAM)     \
	{                                                  \
		int _ret = fwrite(BUF, SIZE, COUNT, STREAM);   \
		if (_ret < COUNT)                              \
			return MAME_SRCDBG_E_FWRITE_ERROR;         \
	}

#define FCLOSE_OR_RETURN(STREAM)                       \
{                                                      \
	if (fclose(STREAM) != 0)                           \
		return MAME_SRCDBG_E_FCLOSE_ERROR;             \
}


class srcdbg_simple_generator
{
public:
	void construct();
	int destruct();
	int open(const char * file_path);
	int add_source_file_path(const char * source_file_path, unsigned int & index);
	int add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned int source_file_index, unsigned int line_number);
	int add_global_fixed_symbol(const char * symbol_name, int symbol_value);
	int add_local_fixed_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value);
	int add_local_relative_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset);
	int import(const char * srcdbg_file_path_to_import, short offset, char * error_details, unsigned int num_bytes_error_details);
	int close();

private:
	int add_string(resizeable_array & ra, unsigned int & size, const char * s);

	FILE * m_output;
	mame_debug_info_simple_header m_header;
	string_resizeable_array m_source_file_paths;
	resizeable_array m_line_mappings;
	string_resizeable_array m_symbol_names;
	resizeable_array m_global_fixed_symbol_values;
	resizeable_array m_local_fixed_symbol_values;
	resizeable_array m_local_relative_symbol_values;
};


#endif /* MAME_SRCDBG_FORMAT_WRITER_H */
