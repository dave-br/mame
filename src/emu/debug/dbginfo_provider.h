#ifndef MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H
#define MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H

#pragma once

#include "mdisimple.h"


class file_line
{
public:
	file_line(u16 file_index, u32 line_number)
	{
		m_file_index = file_index;
		m_line_number = line_number;
	}

	u16 file_index() { return m_file_index; };
	u32 line_number() { return m_line_number; };

	bool operator == (const file_line & that)
	{
		return (this->m_file_index == that.m_file_index) && (this->m_line_number == that.m_line_number);
	}

private:
	u16 m_file_index;
	u32 m_line_number;
};

// abstract base class for debug-info (symbols) file readers
class debug_info_provider_base
{
public:
	// TODO: MAKE FRIEND CLASS, AND ONLY GETTERS PUBLIC
	class source_file_path
	{
	public:
		source_file_path(std::string & built_p, std::string & local_p) :
			m_built(std::move(built_p)),
			m_local(std::move(local_p))
		{
		}

		// TODO: VERIFY THIS IS CALLED WHEN SFP IS PLACED IN VECTOR
		// source_file_path(source_file_path && sfp) :
		// 	m_built(sfp.m_built),
		// 	m_local(sfp.m_local)
		// {
		// }
		
		const char * built() const { return m_built.c_str(); }
		const char * local() const { return m_local.c_str(); }  // TODO: WORKS IF LOCAL EMPTY?

	private:
		std::string m_built;
		std::string m_local;
	};

	class symbol
	{
	public:
		symbol(std::string & name_p, offs_t value_p) :
			m_name(std::move(name_p)),
			m_value(value_p)
		{
		}

		const char * name() { return m_name.c_str(); };
		s64 value() { return m_value; };

	private:
		std::string m_name;
		s64 m_value;
	};

	typedef std::pair<offs_t,offs_t> address_range;
	static std::unique_ptr<debug_info_provider_base> create_debug_info(running_machine &machine);
	virtual ~debug_info_provider_base() {};
	virtual std::size_t num_files() const = 0;
	virtual const source_file_path & file_index_to_path(u16 file_index) const = 0;
	virtual std::optional<int> file_path_to_index(const char * file_path) const = 0;
	virtual std::optional<address_range> file_line_to_address_range (u16 file_index, u32 line_number) const = 0;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const = 0;
	virtual const std::vector<symbol> & global_symbols() const = 0;
};

// debug-info provider for the simple format
// TODO: NAME?  MOVE TO ANOTHER FILE?

class debug_info_simple : public debug_info_provider_base
{
public:
	debug_info_simple(running_machine& machine, std::vector<uint8_t>& data);
	~debug_info_simple() { }
	virtual std::size_t num_files() const override { return m_source_file_paths.size(); }
	virtual const source_file_path & file_index_to_path(u16 file_index) const override { return m_source_file_paths[file_index]; };
	virtual std::optional<int> file_path_to_index(const char * file_path) const override;
	virtual std::optional<address_range> file_line_to_address_range (u16 file_index, u32 line_number) const override;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const override;
	virtual const std::vector<symbol> & global_symbols() const override { return m_symbols; };

private:
	struct address_line
	{
		u16 address_first;
		u16 address_last;
		u32 line_number;
	};

	void generate_local_path(running_machine& machine, const std::string & built, std::string & local);
	void apply_source_map(running_machine& machine, std::string & local);

	// std::vector<char>                        m_source_file_path_chars; // Storage for source file path characters
	std::vector<source_file_path>            m_source_file_paths;      // Starting points for source file path strings
	std::vector<mdi_line_mapping>            m_linemaps_by_address;    // a list of mdi_line_mappings, sorted by address
	std::vector<std::vector<address_line>>   m_linemaps_by_line;       // m_linemaps_by_line[i] is a list of address/line pairs,
	                                                                   // sorted by line, from file #i
	std::vector<symbol>                      m_symbols;
};


#endif // MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H
