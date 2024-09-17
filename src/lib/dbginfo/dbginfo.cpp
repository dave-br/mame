#include "dbginfo.h"
#include "mdisimple.h"

#include <vector>
#include <stdio.h>
#include <string.h>

class mdi_simple_generator
{
public:
	mdi_simple_generator() : 
		m_output(nullptr),
		m_num_source_file_paths(0),
		m_source_file_paths(),
		m_line_mappings()
	{ 
	};

	~mdi_simple_generator()
	{
		if (m_output != nullptr)
		{
			fclose(m_output);
			m_output = nullptr;
		}
	};

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
		for (const char * c = source_file_path; *c != '\0'; c++)
		{
			m_source_file_paths.push_back(*c);
			m_header.source_file_paths_size++;
		}
		m_source_file_paths.push_back('\0');
		m_header.source_file_paths_size++;
		return m_num_source_file_paths++;
	}

	void add_line_mapping(unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number)
	{
		m_header.num_line_mappings++;
		mdi_line_mapping line_mapping = { address_first, address_last, source_file_index, line_number };
		m_line_mappings.push_back(line_mapping);
	}

	void close()
	{
		// TODO:
		// verification code to ensure references to source_file_index are in range
		fwrite(&m_header, sizeof(m_header), 1, m_output);
		for (int i=0; i < m_source_file_paths.size(); i++)
		{
			fwrite(&m_source_file_paths[i], sizeof(char), 1, m_output);
		}
		for (int i=0; i < m_line_mappings.size(); i++)
		{
			fwrite(&m_line_mappings[i], sizeof(mdi_line_mapping), 1, m_output);
		}
		fclose(m_output);
		m_output = nullptr;
	}

private:
	FILE * m_output;
	mame_debug_info_simple_header m_header;
	unsigned short m_num_source_file_paths;
	std::vector<char> m_source_file_paths;
	std::vector<mdi_line_mapping> m_line_mappings;
};



// C Interface to assemblers / compilers

void * mame_mdi_simp_open_new(const char * file_path)
{
	mdi_simple_generator * generator = new mdi_simple_generator();
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
	delete generator;
}