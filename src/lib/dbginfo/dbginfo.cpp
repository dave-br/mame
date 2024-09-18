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

		unsigned int new_capacity = (m_capacity + 1) * 2;
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
	}

	void open(const char * file_path)
	{
		FILE * output = fopen(file_path, "wb");
		if (output == nullptr)
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
		char null_terminator = '\0';
		for (const char * c = source_file_path; *c != '\0'; c++)
		{
			m_source_file_paths.push_back(c, 1);
			m_header.source_file_paths_size++;
		}
		m_source_file_paths.push_back(&null_terminator, 1);
		m_header.source_file_paths_size++;
		return m_num_source_file_paths++;
	}

	void add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
	{
		m_header.num_line_mappings++;
		mdi_line_mapping line_mapping = { address_first, address_last, source_file_index, line_number };
		m_line_mappings.push_back(&line_mapping, sizeof(line_mapping));
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
		fclose(m_output);
		m_output = nullptr;
	}

private:
	FILE * m_output;
	mame_debug_info_simple_header m_header;
	unsigned short m_num_source_file_paths;
	resizeable_array m_source_file_paths;
	resizeable_array m_line_mappings;
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


void mame_mdi_simp_close(void * mdi_simp_state)
{
	mdi_simple_generator * generator = (mdi_simple_generator *) mdi_simp_state;
	generator->close();
	generator->destruct();
}