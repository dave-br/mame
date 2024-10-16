#ifndef MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H
#define MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H

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

	class global_static_symbol
	{
	public:
		global_static_symbol(std::string & name_p, s64 value_p) :
			m_name(std::move(name_p)),
			m_value(value_p)
		{
		}

		const char * name() const { return m_name.c_str(); };
		s64 value() const { return m_value; };

	private:
		std::string m_name;
		s64 m_value;
	};

	class local_static_symbol
	{
	public:
		local_static_symbol(std::string & name, std::vector<std::pair<offs_t,offs_t>> scope_ranges, s64 value)
			: m_name(std::move(name))
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

	class local_dynamic_symbol
	{
	public:
		local_dynamic_symbol(std::string & name, std::vector<symbol_table::scoped_value> scoped_values)
			: m_name(std::move(name))
			, m_scoped_values(scoped_values)
		{
		}
		const char * name() const { return m_name.c_str(); };
		const std::vector<symbol_table::scoped_value> & scoped_values() const { return m_scoped_values; };

	private:
		std::string m_name;
		std::vector<symbol_table::scoped_value> m_scoped_values;
	};


	typedef std::pair<offs_t,offs_t> address_range;
	static std::unique_ptr<debug_info_provider_base> create_debug_info(running_machine &machine);
	virtual ~debug_info_provider_base() {};
	virtual std::size_t num_files() const = 0;
	virtual const source_file_path & file_index_to_path(u16 file_index) const = 0;
	virtual std::optional<int> file_path_to_index(const char * file_path) const = 0;
	virtual void file_line_to_address_ranges(u16 file_index, u32 line_number, std::vector<address_range> & ranges) const = 0;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const = 0;
	virtual const std::vector<global_static_symbol> & global_static_symbols() const = 0;
	virtual const std::vector<local_static_symbol> & local_static_symbols() const = 0;
	virtual const std::vector<local_dynamic_symbol> & local_dynamic_symbols() const = 0;
};


class srcdbg_import;

// debug-info provider for the simple format
// TODO: NAME?  MOVE TO ANOTHER FILE?

class debug_info_simple : public debug_info_provider_base
{
	friend class srcdbg_import;

public:
	debug_info_simple(const running_machine& machine);
	~debug_info_simple() { }
	virtual std::size_t num_files() const override { return m_source_file_paths.size(); }
	virtual const source_file_path & file_index_to_path(u16 file_index) const override { return m_source_file_paths[file_index]; };
	virtual std::optional<int> file_path_to_index(const char * file_path) const override;
	virtual void file_line_to_address_ranges(u16 file_index, u32 line_number, std::vector<address_range> & ranges) const override;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const override;
	virtual const std::vector<global_static_symbol> & global_static_symbols() const override { return m_global_static_symbols; };
	virtual const std::vector<local_static_symbol> & local_static_symbols() const override { return m_local_static_symbols; };
	virtual const std::vector<local_dynamic_symbol> & local_dynamic_symbols() override { ensure_local_dynamics_ready(); return m_local_dynamic_symbols; };

private:
	struct address_line
	{
		u16 address_first;
		u16 address_last;
		u32 line_number;
	};

	void generate_local_path(const std::string & built, std::string & local);
	void apply_source_map(std::string & local);
	void ensure_local_dynamics_ready();


	const running_machine& m_machine;

	// std::vector<char>                        m_source_file_path_chars; // Storage for source file path characters
	std::vector<source_file_path>            m_source_file_paths;      // Starting points for source file path strings
	std::vector<mdi_line_mapping>            m_linemaps_by_address;    // a list of mdi_line_mappings, sorted by address
	std::vector<std::vector<address_line>>   m_linemaps_by_line;       // m_linemaps_by_line[i] is a list of address/line pairs,
	                                                                   // sorted by line, from file #i
	std::vector<global_static_symbol>        m_global_static_symbols;
	std::vector<local_static_symbol>		 m_local_static_symbols;
	std::vector<local_dynamic_symbol> 		m_local_dynamic_symbols;
};


class srcdbg_import : public srcdbg_format_reader_callback
{
public:
	srcdbg_import(debug_info_simple & srcdbg_simple);
	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) override { return true;};
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) override { return true;};
	virtual bool on_read_source_path(u16 source_path_index, std::string && source_path) override;
	virtual bool on_read_line_mapping(const mdi_line_mapping & line_map) override;
	virtual bool on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name) override;
	virtual bool on_read_global_constant_symbol_value(const global_constant_symbol_value & value) override;
	virtual bool on_read_local_constant_symbol_value(const local_constant_symbol_value & value) override;
	virtual bool on_read_local_dynamic_symbol_value(const local_dynamic_symbol_value & value) override;

private:
	debug_info_simple & m_srcdbg_simple;
	bool m_read_line_mapping_yet;
	std::vector<std::string> m_symbol_names;
	device_state_interface * m_state;
};


#endif // MAME_EMU_DEBUG_DEBUGINFO_PROVIDER_H
