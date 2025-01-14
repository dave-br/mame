// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_format_writer.h

    Library of helper functions to generate MAME source-level
    debugging info files.

    This library makes only minimal use of C++ (e.g., no STL or even
    new / delete) to avoid the need for consumers to link to the
    standard C++ library.  This allows C-only tools to easily link
    to this library.

***************************************************************************/


#include "srcdbg_format_writer.h"
#include "srcdbg_format.h"
// #include "osdcomm.h"
#include "srcdbg_format_reader.h"

// #include "strformat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

// #include <vector>

// TODO:
unsigned int little_endianize_int32(unsigned int n) { return n; }
unsigned short little_endianize_int16(unsigned short n) { return n; }

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



// ------------------------------------------------------------------
// resizeable_array - a cheap STL::vector replacement that
// stores bytes
// ------------------------------------------------------------------

class resizeable_array
{
public:
	void construct()
	{
		m_data = nullptr;
		m_size = 0;
		m_capacity = 0;
	}

	void destruct()
	{
		if (m_data != nullptr)
		{
			free(m_data);
		}
	}

	char * get()
	{
		return m_data;
	}

	int push_back(const void * data_bytes, int data_size)
	{
		RET_IF_FAIL(ensure_capacity(m_size + data_size));
		memcpy(m_data + m_size, data_bytes, data_size);
		m_size += data_size;
		return MAME_SRCDBG_E_SUCCESS;
	}

	unsigned int find(unsigned int search, unsigned int data_size)
	{
		const char * data = m_data;
		const char * data_end = m_data + m_size;
		int ret = 0;
		while (data < data_end)
		{
			if (*(unsigned int *) data == search)
			{
				return ret;
			}

			data += data_size;
			ret++;
		}

		return (unsigned int) - 1;
	}

	unsigned int size()
	{
		return m_size;
	}

protected:
	int ensure_capacity(unsigned int req_capacity)
	{
		if (req_capacity <= m_capacity)
		{
			return MAME_SRCDBG_E_SUCCESS;
		}

		unsigned int new_capacity = (m_capacity + 1) << 1;
		while (new_capacity < req_capacity)
		{
			new_capacity <<= 1;
		}

		void * new_data = malloc(new_capacity);
		if (new_data == nullptr)
		{
			return MAME_SRCDBG_E_OUTOFMEMORY;
		}

		memcpy(new_data, m_data, m_size);
		free(m_data);
		m_data = (char *) new_data;
		m_capacity = new_capacity;
		return MAME_SRCDBG_E_SUCCESS;
	}

	char * m_data;
	unsigned int m_size;
	unsigned int m_capacity;
};


// ------------------------------------------------------------------
// int_resizeable_array - a cheap STL::vector<unsigned int>
// replacement
// ------------------------------------------------------------------

class int_resizeable_array : public resizeable_array
{
public:
	int push_back(unsigned int new_int)
	{
		return resizeable_array::push_back((char *) &new_int, sizeof(unsigned int));
	}

	unsigned int get(unsigned int i)
	{
		return * (((unsigned int *) m_data) + i);
	}

	unsigned int size()
	{
		return m_size / sizeof(unsigned int);
	}
};



// ------------------------------------------------------------------
// string_resizeable_array - a cheap STL::unordered_set<const char *>
// replacement.  Stores characters packed together, keeps
// track of string starting-points with an int_resizeable_array
// field, and avoid dupes when storing new elements
// ------------------------------------------------------------------

class string_resizeable_array : public resizeable_array
{
public:
	void construct()
	{
		resizeable_array::construct();
		m_offsets.construct();
	}

	int find_or_push_back(const char * new_string, unsigned int & size, unsigned int & offset)
	{
		for (unsigned int i = 0; i < m_offsets.size(); i++)
		{
			const char * string = m_data + m_offsets.get(i);
			if (strcmp(string, new_string) == 0)
			{
				offset = i;
				return MAME_SRCDBG_E_SUCCESS;
			}
		}

		// String not found, add it to the end.  Remember that
		// offset in m_offsets array
		RET_IF_FAIL(m_offsets.push_back(m_size));
		size_t length = strlen(new_string);
		RET_IF_FAIL(push_back(new_string, length));
		char null_terminator = '\0';
		RET_IF_FAIL(push_back(&null_terminator, 1));
		size += length + 1;
		offset = m_offsets.size() - 1;      // Index into m_offsets representing newly-added string
		return MAME_SRCDBG_E_SUCCESS;
	}

	unsigned int num_strings()
	{
		return m_offsets.size();
	}

private:
	int_resizeable_array m_offsets;
};


// Interim storage of local fixed variables
struct local_fixed : local_fixed_symbol_value
{
	resizeable_array ranges;
};


// Interim storage of local relative variables
struct local_relative : local_relative_symbol_value
{
	resizeable_array values;
};

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


// TODO COMMENT
class writer_importer : public srcdbg_format_reader_callback
{
public:
	writer_importer(srcdbg_simple_generator & gen, short offset)
		: m_gen(gen)
		, m_offset(offset)
		// , m_old_to_new_source_path_indices()
		// , m_symbol_names()
	{
	}
	~writer_importer() {}

	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) override	{ return true; }
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) override { return true; }
	virtual bool on_read_source_path(u32 source_path_index /*, std::string && source_path */) override;
	virtual bool on_read_line_mapping(const srcdbg_line_mapping & line_map) override;
	virtual bool on_read_symbol_name(u32 symbol_name_index /*, std::string && symbol_name */) override;
	virtual bool on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value) override;
	virtual bool on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value) override;
	virtual bool on_read_local_relative_symbol_value(const local_relative_symbol_value & value) override;

private:
	srcdbg_simple_generator & m_gen;
	short                     m_offset;
	// std::vector<u32>          m_old_to_new_source_path_indices;
	// std::vector<std::string>  m_symbol_names;
};

bool writer_importer::on_read_source_path(u32 source_path_index /*, std::string && source_path*/)
{
	// srcdbg_format_simp_read is required to notify in (contiguous) index order
	// assert (m_old_to_new_source_path_indices.size() == source_path_index);

	u32 new_index;
	m_gen.add_source_file_path("todo", /* source_path.c_str(), */ new_index);
	
	// m_old_to_new_source_path_indices.push_back(new_index);
	return true;
}

bool writer_importer::on_read_line_mapping(const srcdbg_line_mapping & line_map)
{
	// assert (line_map.source_file_index < m_old_to_new_source_path_indices.size());

	m_gen.add_line_mapping(
		line_map.range.address_first + m_offset,
		line_map.range.address_last + m_offset,
		0, //m_old_to_new_source_path_indices[line_map.source_file_index],
		line_map.line_number);
	return true;
}

bool writer_importer::on_read_symbol_name(u32 symbol_name_index /*, std::string && symbol_name */)
{
	// srcdbg_format_simp_read is required to notify in (contiguous) index order
	// assert (m_symbol_names.size() == symbol_name_index);
	// m_symbol_names.push_back(std::move(symbol_name));
	return true;
}

bool writer_importer::on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value)
{
	// assert (value.symbol_name_index < m_symbol_names.size());
	const char * sym_name = "todo"; // m_symbol_names[value.symbol_name_index].c_str();
	m_gen.add_global_fixed_symbol(sym_name, value.symbol_value + m_offset);
	return true;
}

bool writer_importer::on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value)
{
	// assert (value.symbol_name_index < m_symbol_names.size());
	const char * sym_name = "todo"; // m_symbol_names[value.symbol_name_index].c_str();
	for (u32 i = 0; i < value.num_address_ranges; i++)
	{
		m_gen.add_local_fixed_symbol(
			sym_name,
			value.ranges[i].address_first + m_offset,
			value.ranges[i].address_last + m_offset,
			value.symbol_value + m_offset);
	}
	return true;
}

bool writer_importer::on_read_local_relative_symbol_value(const local_relative_symbol_value & value)
{
	// assert (value.symbol_name_index < m_symbol_names.size());
	const char * sym_name = "todo"; // m_symbol_names[value.symbol_name_index].c_str();
	for (u32 i = 0; i < value.num_local_relative_eval_rules; i++)
	{
		m_gen.add_local_relative_symbol(
			sym_name, 
			value.local_relative_eval_rules[i].range.address_first + m_offset,
			value.local_relative_eval_rules[i].range.address_last + m_offset,
			value.local_relative_eval_rules[i].reg,
			value.local_relative_eval_rules[i].reg_offset);
	}
	return true;
}

// ------------------------------------------------------------------
// srcdbg_simple_generator - Primary class for implementing
// the public API.
// ------------------------------------------------------------------



void srcdbg_simple_generator::construct()
{
	// Init members
	m_output = nullptr;
	m_source_file_paths.construct();
	m_line_mappings.construct();
	m_symbol_names.construct();
	m_global_fixed_symbol_values.construct();
	m_local_fixed_symbol_values.construct();
	m_local_relative_symbol_values.construct();

	// Init header_base
	memcpy(&m_header.header_base.magic[0], "MDbI", 4);
	memcpy(&m_header.header_base.type[0], "simp", 4);
	m_header.header_base.version = 1;
	strncpy(
		&m_header.header_base.info[0],
		"Generated by MAME source-level debugging format writer.  https://docs.mamedev.org/debugger/TODO",
		sizeof(m_header.header_base.info));

	// Init rest of header
	m_header.source_file_paths_size = 0;
	m_header.num_line_mappings = 0;
	m_header.symbol_names_size = 0;
	m_header.num_global_fixed_symbol_values = 0;
	m_header.local_fixed_symbol_values_size = 0;
	m_header.local_relative_symbol_values_size = 0;
}

int srcdbg_simple_generator::destruct()
{
	if (m_output != nullptr)
	{
		FCLOSE_OR_RETURN(m_output);
		m_output = nullptr;
	}
	m_source_file_paths.destruct();
	m_line_mappings.destruct();
	m_symbol_names.destruct();
	m_global_fixed_symbol_values.destruct();
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::open(const char * file_path)
{
	m_output = fopen(file_path, "wb");
	if (m_output == nullptr)
	{
		return MAME_SRCDBG_E_FOPEN_ERROR;
	}
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::add_source_file_path(const char * source_file_path, unsigned int & index)
{
	RET_IF_FAIL(m_source_file_paths.find_or_push_back(source_file_path, m_header.source_file_paths_size, index));
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned int source_file_index, unsigned int line_number)
{
	if (source_file_index >= m_source_file_paths.num_strings())
	{
		return MAME_SRCDBG_E_INVALID_SRC_IDX;
	}
	m_header.num_line_mappings = little_endianize_int32(little_endianize_int32(m_header.num_line_mappings) + 1);
	srcdbg_line_mapping line_mapping =
	{
		{
			little_endianize_int16(address_first),
			little_endianize_int16(address_last)
		},
		little_endianize_int32(source_file_index),
		little_endianize_int32(line_number)
	};
	return m_line_mappings.push_back((const char *) &line_mapping, sizeof(line_mapping));
}

int srcdbg_simple_generator::add_global_fixed_symbol(const char * symbol_name, int symbol_value)
{
	global_fixed_symbol_value global;
	RET_IF_FAIL(m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size, global.symbol_name_index));
	global.symbol_value = little_endianize_int32(symbol_value);
	RET_IF_FAIL(m_global_fixed_symbol_values.push_back(&global, sizeof(global)));
	m_header.num_global_fixed_symbol_values = little_endianize_int32(little_endianize_int32(m_header.num_global_fixed_symbol_values) + 1);
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::add_local_fixed_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value)
{
	local_fixed * entry_ptr;
	local_fixed local;
	RET_IF_FAIL(m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size, local.symbol_name_index));
	unsigned int entry_idx = m_local_fixed_symbol_values.find(local.symbol_name_index, sizeof(local));
	if (entry_idx == (unsigned int) -1)
	{
		local.symbol_value = little_endianize_int32(symbol_value);
		local.num_address_ranges = 0;
		local.ranges.construct();
		entry_ptr = &local;
		RET_IF_FAIL(m_local_fixed_symbol_values.push_back(&local, sizeof(local)));
		m_header.local_fixed_symbol_values_size = little_endianize_int32(
			little_endianize_int32(m_header.local_fixed_symbol_values_size) +
			sizeof(local_fixed_symbol_value));
	}
	else
	{
		entry_ptr = ((local_fixed *) m_local_fixed_symbol_values.get()) + entry_idx;
	}

	address_range range;
	range.address_first = little_endianize_int16(address_first);
	range.address_last = little_endianize_int16(address_last);
	RET_IF_FAIL(entry_ptr->ranges.push_back(&range, sizeof(range)));
	m_header.local_fixed_symbol_values_size = little_endianize_int32(
		little_endianize_int32(m_header.local_fixed_symbol_values_size) +
		sizeof(range));
	entry_ptr->num_address_ranges = little_endianize_int32(little_endianize_int32(entry_ptr->num_address_ranges) + 1);
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::add_local_relative_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset)
{
	local_relative * entry_ptr;
	local_relative local;
	RET_IF_FAIL(m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size, local.symbol_name_index));
	unsigned int entry_idx = m_local_relative_symbol_values.find(local.symbol_name_index, sizeof(local));
	if (entry_idx == (unsigned int) -1)
	{
		local.num_local_relative_eval_rules = 0;
		local.values.construct();
		RET_IF_FAIL(m_local_relative_symbol_values.push_back(&local, sizeof(local)));
		entry_ptr = (local_relative *) (m_local_relative_symbol_values.get() + m_local_relative_symbol_values.size() - sizeof(local));
		m_header.local_relative_symbol_values_size = little_endianize_int32(
			little_endianize_int32(m_header.local_relative_symbol_values_size) +
			sizeof(local_relative_symbol_value));
	}
	else
	{
		entry_ptr = ((local_relative *) m_local_relative_symbol_values.get()) + entry_idx;
	}
	local_relative_eval_rule value;
	value.range.address_first = little_endianize_int16(address_first);
	value.range.address_last = little_endianize_int16(address_last);
	value.reg = reg;
	value.reg_offset = little_endianize_int32(reg_offset);
	RET_IF_FAIL(entry_ptr->values.push_back(&value, sizeof(value)));
	m_header.local_relative_symbol_values_size = little_endianize_int32(
		little_endianize_int32(m_header.local_relative_symbol_values_size) +
		sizeof(value));
	entry_ptr->num_local_relative_eval_rules = little_endianize_int32(little_endianize_int32(entry_ptr->num_local_relative_eval_rules) + 1);
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::import(const char * srcdbg_file_path_to_import, short offset, char * error_details, unsigned int num_bytes_error_details)
{
	// std::string error;
	srcdbg_format format;
	if (!srcdbg_format_header_read(srcdbg_file_path_to_import, format)) //, error))
	{
		// util::string_format(
		// 	error_details,
		// 	num_bytes_error_details,
		// 	"Error reading source-level debugging information file\n%s\n\n%s",
		// 	srcdbg_file_path_to_import,
		// 	error.c_str());
		return MAME_SRCDBG_E_IMPORT_FAILED;
	}

	switch (format)
	{
	case SRCDBG_FORMAT_SIMPLE:
	{
		writer_importer importer(*this, offset);
		if (!srcdbg_format_simp_read(srcdbg_file_path_to_import, importer)) //, error))
		{
			// writer_importer always returns true from callbacks, so if we're here,
			// the reader provided an error
			// assert (!error.empty());
			
			// util::string_format(
			// 	error_details,
			// 	num_bytes_error_details,
			// 	"Error reading source-level debugging information file\n%s\n\n%s",
			// 	srcdbg_file_path_to_import,
			// 	error.c_str());
				return MAME_SRCDBG_E_IMPORT_FAILED;
		}
		return MAME_SRCDBG_E_SUCCESS;
	}

	// FUTURE: If more file formats are invented, add cases for them here to read them

	default:
		assert(!"Unexpected source-level debugging information file format");
		return MAME_SRCDBG_E_IMPORT_FAILED;
	}
	// return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::close()
{
	FWRITE_OR_RETURN(&m_header, sizeof(m_header), 1, m_output);
	for (int i=0; i < m_source_file_paths.size(); i += sizeof(char))
	{
		FWRITE_OR_RETURN(m_source_file_paths.get() + i, sizeof(char), 1, m_output);
	}
	for (int i=0; i < m_line_mappings.size(); i += sizeof(srcdbg_line_mapping))
	{
		FWRITE_OR_RETURN(m_line_mappings.get() + i, sizeof(srcdbg_line_mapping), 1, m_output);
	}
	for (int i=0; i < m_symbol_names.size(); i += sizeof(char))
	{
		FWRITE_OR_RETURN(m_symbol_names.get() + i, sizeof(char), 1, m_output);
	}
	for (int i=0; i < m_global_fixed_symbol_values.size(); i += sizeof(global_fixed_symbol_value))
	{
		FWRITE_OR_RETURN(m_global_fixed_symbol_values.get() + i, sizeof(global_fixed_symbol_value), 1, m_output);
	}
	for (int i=0; i < m_local_fixed_symbol_values.size(); i += sizeof(local_fixed))
	{
		local_fixed * loc = (local_fixed *) (m_local_fixed_symbol_values.get() + i);
		if (loc->ranges.size() == 0)
		{
			continue;
		}
		FWRITE_OR_RETURN(loc, sizeof(local_fixed_symbol_value), 1, m_output);
		FWRITE_OR_RETURN(loc->ranges.get(), sizeof(address_range), loc->ranges.size() / sizeof(address_range), m_output);
	}
	for (int i=0; i < m_local_relative_symbol_values.size(); i += sizeof(local_relative))
	{
		local_relative * loc = (local_relative *) (m_local_relative_symbol_values.get() + i);
		if (loc->values.size() == 0)
		{
			continue;
		}
		FWRITE_OR_RETURN(loc, sizeof(local_relative_symbol_value), 1, m_output);
		FWRITE_OR_RETURN(loc->values.get(), sizeof(local_relative_eval_rule), loc->values.size() / sizeof(local_relative_eval_rule), m_output);
	}
	FCLOSE_OR_RETURN(m_output);
	m_output = nullptr;
	return MAME_SRCDBG_E_SUCCESS;
}

int srcdbg_simple_generator::add_string(resizeable_array & ra, unsigned int & size, const char * s)
{
	char null_terminator = '\0';
	for (const char * c = s; *c != '\0'; c++)
	{
		RET_IF_FAIL(ra.push_back(c, 1));
		size++;
	}
	RET_IF_FAIL(ra.push_back(&null_terminator, 1));
	size++;
	return MAME_SRCDBG_E_SUCCESS;
}

// ------------------------------------------------------------------
// Public API - C Interface for assemblers / compilers.  These
// are simple wrappers around public member functions from
// srcdbg_simple_generator
// ------------------------------------------------------------------

LIB_PUBLIC int mame_srcdbg_simp_open_new(const char * file_path, void ** handle_ptr)
{
	srcdbg_simple_generator * generator = (srcdbg_simple_generator *) malloc(sizeof(srcdbg_simple_generator));
	if (generator == nullptr)
	{
		return MAME_SRCDBG_E_OUTOFMEMORY;
	}
	generator->construct();
	RET_IF_FAIL(generator->open(file_path));
	*handle_ptr = (void *) generator;
	return MAME_SRCDBG_E_SUCCESS;
}

LIB_PUBLIC int mame_srcdbg_simp_add_source_file_path(void * srcdbg_simp_state, const char * source_file_path, unsigned int * index_ptr)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_source_file_path(source_file_path, *index_ptr);
}

LIB_PUBLIC int mame_srcdbg_simp_add_line_mapping(void * srcdbg_simp_state, unsigned short address_first, unsigned short address_last, unsigned int source_file_index, unsigned int line_number)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_line_mapping(address_first, address_last, source_file_index, line_number);
}

LIB_PUBLIC int mame_srcdbg_simp_add_global_fixed_symbol(void * srcdbg_simp_state, const char * symbol_name, int symbol_value)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_global_fixed_symbol(symbol_name, symbol_value);
}

LIB_PUBLIC int mame_srcdbg_simp_add_local_fixed_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_local_fixed_symbol(symbol_name, address_first, address_last, symbol_value);
}

LIB_PUBLIC int mame_srcdbg_simp_add_local_relative_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_local_relative_symbol(symbol_name, address_first, address_last, reg, reg_offset);
}

LIB_PUBLIC int mame_srcdbg_simp_import(void * srcdbg_simp_state, const char * srcdbg_file_path_to_import, short offset, char * error_details, unsigned int num_bytes_error_details)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->import(srcdbg_file_path_to_import, offset, error_details, num_bytes_error_details);
}

LIB_PUBLIC int mame_srcdbg_simp_close(void * srcdbg_simp_state)
{
	srcdbg_simple_generator * generator = (srcdbg_simple_generator *) srcdbg_simp_state;
	RET_IF_FAIL(generator->close());
	RET_IF_FAIL(generator->destruct());
	return MAME_SRCDBG_E_SUCCESS;
}
