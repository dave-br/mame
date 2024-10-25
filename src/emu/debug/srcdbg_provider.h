// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider.h

    Format-agnostic interface to source-debugging info

***************************************************************************/


#ifndef MAME_EMU_DEBUG_SRCDBG_PROVIDER_H
#define MAME_EMU_DEBUG_SRCDBG_PROVIDER_H

#pragma once

#include "srcdbg_format_reader.h"

#include "express.h"


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
class srcdbg_provider_base
{
public:
	// TODO: MAKE FRIEND CLASS, AND ONLY GETTERS PUBLIC
	class source_file_path
	{
	public:
		source_file_path(std::string && built_p, std::string && local_p)
			: m_built(std::move(built_p))
			, m_local(std::move(local_p))
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

	class global_fixed_symbol
	{
	public:
		global_fixed_symbol(const std::string & name_p, s64 value_p)
			: m_name(name_p)
			, m_value(value_p)
		{
		}

		const char * name() const { return m_name.c_str(); };
		s64 value() const { return m_value; };

	private:
		std::string m_name;
		s64 m_value;
	};

	class local_fixed_symbol
	{
	public:
		local_fixed_symbol(const std::string & name, std::vector<std::pair<offs_t,offs_t>> scope_ranges, s64 value)
			: m_name(name)
			, m_scope_ranges(scope_ranges)
			, m_value_integer(value)
		{
		}

		const char * name() const { return m_name.c_str(); };
		const std::vector<std::pair<offs_t,offs_t>> & scope_ranges() const { return m_scope_ranges; };
		s64 value() const { return m_value_integer; };

	private:
		std::string m_name;
		std::vector<std::pair<offs_t,offs_t>> m_scope_ranges;
		s64 m_value_integer;
	};

	class local_relative_symbol
	{
	public:
		local_relative_symbol(const std::string & name, std::vector<symbol_table::local_range_expression> && scoped_values)
			: m_name(name)
			, m_ranges(std::move(scoped_values))
		{
		}
		const char * name() const { return m_name.c_str(); };
		const std::vector<symbol_table::local_range_expression> & scoped_values() const { return m_ranges; };

	private:
		std::string m_name;
		std::vector<symbol_table::local_range_expression> m_ranges;
	};


	typedef std::pair<offs_t,offs_t> address_range;
	static std::unique_ptr<srcdbg_provider_base> create_debug_info(running_machine &machine);
	virtual ~srcdbg_provider_base() {};
	virtual void complete_initialization() = 0;
	virtual std::size_t num_files() const = 0;
	virtual const source_file_path & file_index_to_path(u16 file_index) const = 0;
	virtual std::optional<int> file_path_to_index(const char * file_path) const = 0;
	virtual void file_line_to_address_ranges(u16 file_index, u32 line_number, std::vector<address_range> & ranges) const = 0;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const = 0;
	virtual const std::vector<global_fixed_symbol> & global_fixed_symbols() const = 0;
	virtual const std::vector<local_fixed_symbol> & local_fixed_symbols() const = 0;
	virtual const std::vector<local_relative_symbol> & local_relative_symbols() const = 0;
};


#endif // MAME_EMU_DEBUG_SRCDBG_PROVIDER_H
