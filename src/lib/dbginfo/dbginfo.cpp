#include "dbginfo.h"
#include "mdisimple.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

	unsigned char * get()
	{
		return m_data;
	}

	void push_back(const void * data_bytes, unsigned int data_size)
	{
		ensure_capacity(m_size + data_size);
		memcpy(m_data + m_size, data_bytes, data_size); 
		m_size += data_size;
	}

	unsigned int size()
	{
		return m_size;
	}

private:
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
		m_data = (unsigned char *) new_data;
		m_capacity = new_capacity;
	}

	unsigned char * m_data;
	unsigned int m_size;
	unsigned int m_capacity;
};

class mdi_simple_generator
{
public:
	void construct()
	{
		m_output = nullptr;
		m_num_source_file_paths = 0;
		m_source_file_paths.construct();
		m_line_mappings.construct();
		m_symbol_names.construct();
		m_symbol_values.construct();
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
		m_symbol_values.destruct();
	}

	void open(const char * file_path)
	{
		m_output = fopen(file_path, "wb");
		if (m_output == nullptr)
		{
			// TODO: ERROR
			return;
		}

		memcpy(&m_header.header_base.magic[0], "MDbI", 4);
		memcpy(&m_header.header_base.type[0], "simp", 4);
		m_header.header_base.version = 1;
		m_header.source_file_paths_size = 0;
		m_header.num_line_mappings = 0;
	}

	unsigned short add_source_file_path(const char * source_file_path)
	{
		add_string(m_source_file_paths, m_header.source_file_paths_size, source_file_path);
		return m_num_source_file_paths++;
	}

	void add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
	{
		m_header.num_line_mappings++;
		mdi_line_mapping line_mapping = { address_first, address_last, source_file_index, line_number };
		m_line_mappings.push_back(&line_mapping, sizeof(line_mapping));
	}

	void add_symbol(const char * symbol_name, int symbol_value)
	{
		add_string(m_symbol_names, m_header.symbol_names_size, symbol_name);
		m_symbol_values.push_back(&symbol_value, sizeof(symbol_value));
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
		for (int i=0; i < m_line_mappings.size(); i += sizeof(mdi_line_mapping))
		{
			fwrite(m_line_mappings.get() + i, sizeof(mdi_line_mapping), 1, m_output);
		}
		for (int i=0; i < m_symbol_names.size(); i += sizeof(char))
		{
			fwrite(m_symbol_names.get() + i, sizeof(char), 1, m_output);
		}
		for (int i=0; i < m_symbol_values.size(); i += sizeof(int))
		{
			fwrite(m_symbol_values.get() + i, sizeof(int), 1, m_output);
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
	resizeable_array m_symbol_names;
	resizeable_array m_symbol_values;
};



// C Interface to assemblers / compilers

void * mame_mdi_simp_open_new(const char * file_path)
{
	mdi_simple_generator * generator = (mdi_simple_generator *) malloc(sizeof(mdi_simple_generator));
	if (generator == nullptr)
	{
		// TODO: ERROR
	}
	generator->construct();
	generator->open(file_path);
	return (void *) generator;
}


unsigned short mame_mdi_simp_add_source_file_path(void * mdi_simp_state, const char * source_file_path)
{
	return ((mdi_simple_generator *) mdi_simp_state)->add_source_file_path(source_file_path);
}


void mame_mdi_simp_add_line_mapping(void * mdi_simp_state, unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
{
	((mdi_simple_generator *) mdi_simp_state)->add_line_mapping(address_first, address_last, source_file_index, line_number);
}

void mame_mdi_simp_add_symbol(void * mdi_simp_state, const char * symbol_name, int symbol_value)
{
	((mdi_simple_generator *) mdi_simp_state)->add_symbol(symbol_name, symbol_value);
}



void mame_mdi_simp_close(void * mdi_simp_state)
{
	mdi_simple_generator * generator = (mdi_simple_generator *) mdi_simp_state;
	generator->close();
	generator->destruct();
}