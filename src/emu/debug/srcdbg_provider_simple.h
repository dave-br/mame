// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider_simple.h

    Implementation of interface to source-debugging info for the
	"simple" format

***************************************************************************/


#ifndef MAME_EMU_DEBUG_SRCDBG_PROVIDER_SIMPLE_H
#define MAME_EMU_DEBUG_SRCDBG_PROVIDER_SIMPLE_H

#pragma once

#include "srcdbg_provider.h"


class srcdbg_import;

class srcdbg_provider_simple : public srcdbg_provider_base
{
	friend class srcdbg_import;

public:
	srcdbg_provider_simple(const running_machine& machine);
	~srcdbg_provider_simple() { }
	virtual void complete_initialization() override;		// TODO: COMMENT
	virtual std::size_t num_files() const override { return m_source_file_paths.size(); }
	virtual const source_file_path & file_index_to_path(u16 file_index) const override { return m_source_file_paths[file_index]; };
	virtual std::optional<int> file_path_to_index(const char * file_path) const override;
	virtual void file_line_to_address_ranges(u16 file_index, u32 line_number, std::vector<address_range> & ranges) const override;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const override;
	virtual const std::vector<global_static_symbol> & global_static_symbols() const override { return m_global_static_symbols; };
	virtual const std::vector<local_static_symbol> & local_static_symbols() const override { return m_local_static_symbols; };
	virtual const std::vector<local_dynamic_symbol> & local_dynamic_symbols() const override { return m_local_dynamic_symbols; };

private:
	struct address_line
	{
		u16 address_first;
		u16 address_last;
		u32 line_number;
	};
	class scoped_value_internal
	{
	public:
		scoped_value_internal(std::pair<offs_t,offs_t> && range, char reg, s32 reg_offset)
			: m_range(std::move(range))
			, m_reg(reg)
			, m_reg_offset(reg_offset)
		{}
		std::pair<offs_t,offs_t> m_range;
		char m_reg;
		s32 m_reg_offset;
	};

	class local_dynamic_symbol_internal
	{
	public:
		local_dynamic_symbol_internal(const std::string & name, std::vector<scoped_value_internal> scoped_values)
			: m_name(name)
			, m_scoped_values(scoped_values)
		{
		}
		std::string m_name;
		std::vector<scoped_value_internal> m_scoped_values;
	};

	void generate_local_path(const std::string & built, std::string & local);
	void apply_source_map(std::string & local);
	void ensure_local_dynamics_ready();


	const running_machine& m_machine;
	// std::vector<char>                        m_source_file_path_chars; // Storage for source file path characters
	std::vector<source_file_path>            m_source_file_paths;      // Starting points for source file path strings
	std::vector<srcdbg_line_mapping>            m_linemaps_by_address;    // a list of srcdbg_line_mappings, sorted by address
	std::vector<std::vector<address_line>>   m_linemaps_by_line;       // m_linemaps_by_line[i] is a list of address/line pairs,
	                                                                   // sorted by line, from file #i
	std::vector<global_static_symbol>        m_global_static_symbols;
	std::vector<local_static_symbol>		 m_local_static_symbols;
	std::vector<local_dynamic_symbol_internal>	m_local_dynamic_symbols_internal;
	std::vector<local_dynamic_symbol> 		m_local_dynamic_symbols;
};


class srcdbg_import : public srcdbg_format_reader_callback
{
public:
	srcdbg_import(srcdbg_provider_simple & srcdbg_simple);
	virtual bool on_read_header_base(const mame_debug_info_header_base & header_base) override { return true;};
	virtual bool on_read_simp_header(const mame_debug_info_simple_header & simp_header) override { return true;};
	virtual bool on_read_source_path(u16 source_path_index, std::string && source_path) override;
	virtual bool on_read_line_mapping(const srcdbg_line_mapping & line_map) override;
	virtual bool on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name) override;
	virtual bool on_read_global_constant_symbol_value(const global_constant_symbol_value & value) override;
	virtual bool on_read_local_constant_symbol_value(const local_constant_symbol_value & value) override;
	virtual bool on_read_local_dynamic_symbol_value(const local_dynamic_symbol_value & value) override;

private:
	srcdbg_provider_simple & m_srcdbg_simple;
	bool m_read_line_mapping_yet;
	std::vector<std::string> m_symbol_names;
	device_state_interface * m_state;
};

#endif // MAME_EMU_DEBUG_SRCDBG_PROVIDER_SIMPLE_H