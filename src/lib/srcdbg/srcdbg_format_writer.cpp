#include "srcdbg_format_writer.h"
#include "srcdbg_format.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

class resizeable_array
{
public:
	// typedef bool (*data_compare)(const void * data1, const void * data2);
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

	void push_back(const void * data_bytes, int data_size)
	{
		ensure_capacity(m_size + data_size);
		memcpy(m_data + m_size, data_bytes, data_size); 
		m_size += data_size;
	}

	// int find_or_push_back(const void * data_bytes, unsigned int data_size, data_compare comp /*, unsigned int & size */)
	// {
	// 	const char * data = m_data;
	// 	const char * data_end = m_data + m_size;
	// 	int ret = 0;
	// 	while (data < data_end)
	// 	{
	// 		if (comp(data, data_bytes))
	// 		{
	// 			return ret;
	// 		}

	// 		data += data_size;
	// 		ret++;
	// 	}

	// 	push_back(data_bytes, data_size);
	// 	return ret;
	// }

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
	void ensure_capacity(unsigned int req_capacity)
	{
		if (req_capacity <= m_capacity)
		{
			return;
		}

		unsigned int new_capacity = (m_capacity + 1) << 1;
		while (new_capacity < req_capacity)
		{
			new_capacity <<= 1;
		}

		void * new_data = malloc(new_capacity);
		if (new_data == nullptr)
		{
			// TODO: ERROR
		}

		memcpy(new_data, m_data, m_size);
		free(m_data);
		m_data = (char *) new_data;
		m_capacity = new_capacity;
	}

	char * m_data;
	unsigned int m_size;
	unsigned int m_capacity;
};

class int_resizeable_array : public resizeable_array
{
public:
	void push_back(unsigned int new_int)
	{
		resizeable_array::push_back((char *) &new_int, sizeof(unsigned int));
	}

	// int find_or_push_back(const void * data_bytes, unsigned int data_size, data_compare comp /*, unsigned int & size */)
	// {
	// 	return 0;		// not implemented
	// }

	unsigned int get(unsigned int i)
	{
		return * (((unsigned int *) m_data) + i);
	}

	unsigned int size()
	{
		return m_size / sizeof(unsigned int);
	}	
};

class string_resizeable_array : public resizeable_array
{
public:
	void construct()
	{
		// TODO: are ctors / dtors really out?  I remember new/delete bad for C, but why ctors?
		resizeable_array::construct();
		m_offsets.construct();
	}

	unsigned int find_or_push_back(const char * new_string, unsigned int & size)
	{
		for (unsigned int i = 0; i < m_offsets.size(); i++)
		{
			const char * string = m_data + m_offsets.get(i);
			if (strcmp(string, new_string) == 0)
			{
				return i;
			}
		}

		// String not found, add it to the end.  Remember that
		// offset in m_offsets array
		m_offsets.push_back(m_size);
		size_t length = strlen(new_string);
		push_back(new_string, length);
		char null_terminator = '\0';
		push_back(&null_terminator, 1);
		size += length + 1;
		return m_offsets.size() - 1;		// Index into m_offsets representing newly-added string
	}

private:
	int_resizeable_array m_offsets;
	// data_compare m_comp;
};


struct local_constant
{
	unsigned int symbol_name_index;
	int symbol_value;
	unsigned int num_address_ranges;
	resizeable_array ranges;
};

struct local_dynamic
{
	unsigned int symbol_name_index;
	unsigned int num_local_dynamic_scoped_values;
	resizeable_array values;
};

class srcdbg_simple_generator
{
public:
	void construct()
	{
		m_output = nullptr;
		m_num_source_file_paths = 0;
		m_source_file_paths.construct();
		m_line_mappings.construct();
		m_symbol_names.construct();
		m_global_constant_symbol_values.construct();
		m_local_constant_symbol_values.construct();
		m_local_dynamic_symbol_values.construct();

		memcpy(&m_header.header_base.magic[0], "MDbI", 4);
		memcpy(&m_header.header_base.type[0], "simp", 4);
		m_header.header_base.version = 1;
		m_header.source_file_paths_size = 0;
		m_header.num_line_mappings = 0;
		m_header.symbol_names_size = 0;
		m_header.num_global_constant_symbol_values = 0;
		m_header.local_constant_symbol_values_size = 0;
		m_header.local_dynamic_symbol_values_size = 0;
	}

	void destruct()
	{
		if (m_output != nullptr)
		{
			fclose(m_output);
			m_output = nullptr;
		}
		m_source_file_paths.destruct();
		m_line_mappings.destruct();
		m_symbol_names.destruct();
		m_global_constant_symbol_values.destruct();
	}

	void open(const char * file_path)
	{
		m_output = fopen(file_path, "wb");
		if (m_output == nullptr)
		{
			// TODO: ERROR
			return;
		}
	}

	unsigned short add_source_file_path(const char * source_file_path)
	{
		add_string(m_source_file_paths, m_header.source_file_paths_size, source_file_path);
		return m_num_source_file_paths++;
	}

	void add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
	{
		m_header.num_line_mappings++;
		srcdbg_line_mapping line_mapping = { { address_first, address_last }, source_file_index, line_number };
		m_line_mappings.push_back((const char *) &line_mapping, sizeof(line_mapping));
	}

	void add_global_constant_symbol(const char * symbol_name, int symbol_value)
	{
		global_constant_symbol_value global;
		global.symbol_name_index = m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size);
		global.symbol_value = symbol_value;
		m_global_constant_symbol_values.push_back(&global, sizeof(global));
		m_header.num_global_constant_symbol_values++;
	}

	void add_local_constant_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value)
	{
		local_constant * entry_ptr;
		local_constant local;
		local.symbol_name_index = m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size);
		unsigned int entry_idx = m_local_constant_symbol_values.find(local.symbol_name_index, sizeof(local));
		if (entry_idx == (unsigned int) -1)
		{
			local.symbol_value = symbol_value;
			local.num_address_ranges = 0;
			local.ranges.construct();
			entry_ptr = &local;
			m_local_constant_symbol_values.push_back(&local, sizeof(local));
			m_header.local_constant_symbol_values_size += sizeof(local_constant_symbol_value);
		}
		else
		{
			entry_ptr = ((local_constant *) m_local_constant_symbol_values.get()) + entry_idx;
		}

		address_range range;
		range.address_first = address_first;
		range.address_last = address_last;
		entry_ptr->ranges.push_back(&range, sizeof(range));
		m_header.local_constant_symbol_values_size += sizeof(range);
		entry_ptr->num_address_ranges++;
	}

	void add_local_dynamic_symbol(const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset)
	{
		local_dynamic * entry_ptr;
		local_dynamic local;
		local.symbol_name_index = m_symbol_names.find_or_push_back(symbol_name, m_header.symbol_names_size);
		unsigned int entry_idx = m_local_dynamic_symbol_values.find(local.symbol_name_index, sizeof(local));
		if (entry_idx == (unsigned int) -1)
		{
			local.num_local_dynamic_scoped_values = 0;
			local.values.construct();
			m_local_dynamic_symbol_values.push_back(&local, sizeof(local));
			entry_ptr = (local_dynamic *) (m_local_dynamic_symbol_values.get() + m_local_dynamic_symbol_values.size() - sizeof(local));
			m_header.local_dynamic_symbol_values_size += sizeof(local_dynamic_symbol_value);
		}
		else
		{
			entry_ptr = ((local_dynamic *) m_local_dynamic_symbol_values.get()) + entry_idx;
		}
		local_dynamic_scoped_value value;
		value.range.address_first = address_first;
		value.range.address_last = address_last;
		value.reg = reg;
		value.reg_offset = reg_offset;
		entry_ptr->values.push_back(&value, sizeof(value));
		m_header.local_dynamic_symbol_values_size += sizeof(value);
		entry_ptr->num_local_dynamic_scoped_values++;
	}


	void close()
	{
		// TODO:
		// verification code to ensure references to source_file_index are in range
		fwrite(&m_header, sizeof(m_header), 1, m_output);
		for (int i=0; i < m_source_file_paths.size(); i += sizeof(char))
		{
			fwrite(m_source_file_paths.get() + i, sizeof(char), 1, m_output);
		}
		for (int i=0; i < m_line_mappings.size(); i += sizeof(srcdbg_line_mapping))
		{
			fwrite(m_line_mappings.get() + i, sizeof(srcdbg_line_mapping), 1, m_output);
		}
		for (int i=0; i < m_symbol_names.size(); i += sizeof(char))
		{
			fwrite(m_symbol_names.get() + i, sizeof(char), 1, m_output);
		}
		for (int i=0; i < m_global_constant_symbol_values.size(); i += sizeof(global_constant_symbol_value))
		{
			fwrite(m_global_constant_symbol_values.get() + i, sizeof(global_constant_symbol_value), 1, m_output);
		}
		for (int i=0; i < m_local_constant_symbol_values.size(); i += sizeof(local_constant))
		{
			local_constant * loc = (local_constant *) (m_local_constant_symbol_values.get() + i);
			if (loc->ranges.size() == 0)
			{
				continue;
			}
			fwrite(loc, sizeof(local_constant_symbol_value), 1, m_output);
			fwrite(loc->ranges.get(), sizeof(address_range), loc->ranges.size() / sizeof(address_range), m_output);
		}
		for (int i=0; i < m_local_dynamic_symbol_values.size(); i += sizeof(local_dynamic))
		{
			local_dynamic * loc = (local_dynamic *) (m_local_dynamic_symbol_values.get() + i);
			if (loc->values.size() == 0)
			{
				continue;
			}
			fwrite(loc, sizeof(local_dynamic_symbol_value), 1, m_output);
			fwrite(loc->values.get(), sizeof(local_dynamic_scoped_value), loc->values.size() / sizeof(local_dynamic_scoped_value), m_output);
		}
		fclose(m_output);
		m_output = nullptr;
	}

private:
	void add_string(resizeable_array & ra, unsigned int & size, const char * s)
	{
		char null_terminator = '\0';
		for (const char * c = s; *c != '\0'; c++)
		{
			ra.push_back(c, 1);
			size++;
		}
		ra.push_back(&null_terminator, 1);
		size++;
	}

	FILE * m_output;
	mame_debug_info_simple_header m_header;
	unsigned short m_num_source_file_paths;
	resizeable_array m_source_file_paths;
	resizeable_array m_line_mappings;
	string_resizeable_array m_symbol_names;
	resizeable_array m_global_constant_symbol_values;
	resizeable_array m_local_constant_symbol_values;
	resizeable_array m_local_dynamic_symbol_values;
};



// C Interface to assemblers / compilers

void * mame_srcdbg_simp_open_new(const char * file_path)
{
	srcdbg_simple_generator * generator = (srcdbg_simple_generator *) malloc(sizeof(srcdbg_simple_generator));
	if (generator == nullptr)
	{
		// TODO: ERROR
	}
	generator->construct();
	generator->open(file_path);
	return (void *) generator;
}


unsigned short mame_srcdbg_simp_add_source_file_path(void * srcdbg_simp_state, const char * source_file_path)
{
	return ((srcdbg_simple_generator *) srcdbg_simp_state)->add_source_file_path(source_file_path);
}


void mame_srcdbg_simp_add_line_mapping(void * srcdbg_simp_state, unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
{
	((srcdbg_simple_generator *) srcdbg_simp_state)->add_line_mapping(address_first, address_last, source_file_index, line_number);
}

void mame_srcdbg_simp_add_global_constant_symbol(void * srcdbg_simp_state, const char * symbol_name, int symbol_value)
{
	((srcdbg_simple_generator *) srcdbg_simp_state)->add_global_constant_symbol(symbol_name, symbol_value);
}

void mame_srcdbg_simp_add_local_constant_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, int symbol_value)
{
	((srcdbg_simple_generator *) srcdbg_simp_state)->add_local_constant_symbol(symbol_name, address_first, address_last, symbol_value);
}
void mame_srcdbg_simp_add_local_dynamic_symbol(void * srcdbg_simp_state, const char * symbol_name, unsigned short address_first, unsigned short address_last, unsigned char reg, int reg_offset)
{
	((srcdbg_simple_generator *) srcdbg_simp_state)->add_local_dynamic_symbol(symbol_name, address_first, address_last, reg, reg_offset);
}


void mame_srcdbg_simp_close(void * srcdbg_simp_state)
{
	srcdbg_simple_generator * generator = (srcdbg_simple_generator *) srcdbg_simp_state;
	generator->close();
	generator->destruct();
}