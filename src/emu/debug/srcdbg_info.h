// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_info.h

    TODO

***************************************************************************/


#ifndef MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H
#define MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H

#pragma once

#include "srcdbg_provider.h"

class srcdbg_info : public srcdbg_provider_base
{
public:
	class srcdbg_provider_entry
	{
	public:
		srcdbg_provider_entry(const std::string & name, srcdbg_provider_base * provider)
			: m_name(name)
			, m_provider(provider)
			, m_enabled(true)
		{
		}

		srcdbg_provider_entry(srcdbg_provider_entry && that)
			: m_name(std::move(that.m_name))
			, m_provider(std::move(that.m_provider))
			, m_enabled(that.m_enabled)
		{			
		}

		~srcdbg_provider_entry() {}

		const std::string & name() const { return m_name; }
		const srcdbg_provider_base * c_provider() const { return m_provider.get(); }
		srcdbg_provider_base * provider() { return m_provider.get(); }
		bool enabled() const { return m_enabled; }
		void set_enabled(bool enabled) { m_enabled = enabled; }

	private:
		std::string m_name;
		std::unique_ptr<srcdbg_provider_base> m_provider;
		bool m_enabled;
	};

	static std::unique_ptr<srcdbg_info> create_debug_info(running_machine &machine);

	srcdbg_info(const running_machine& machine);
	~srcdbg_info() { }

	// robin all, change params so caller creates the tables,
	// and callees just populate them
	void get_srcdbg_symbols(
		symbol_table * symtable_srcdbg_globals,
		symbol_table * symtable_srcdbg_locals,
		const device_state_interface * state) const;

	// robin all
	virtual void complete_local_relative_initialization() override;

	// Sum
	virtual u32 num_files() const override;

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
	virtual const source_file_path & file_index_to_path(u32 file_index) const override;

	// robin to first successful
	virtual std::optional<u32> file_path_to_index(const char * file_path) const override;

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
	virtual void file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const override;

	// robin to first successful
	virtual bool address_to_file_line (offs_t address, file_line & loc) const override;

	// own offset and remove from base class
	void set_offset(s32 offset) { m_offset = offset; }
	s32 get_offset() const { return m_offset; }

	std::vector<srcdbg_provider_entry> & providers() { return m_providers; }

	void coalesce();

private:
	// agg file index to provider index + local file index
	std::vector<std::pair<std::size_t, u32>> m_agg_file_to_provider_file;
	
	// provider index + local file inex to agg file index
	std::vector<std::vector<u32>> m_provider_file_to_agg_file;
	std::vector<srcdbg_provider_entry> m_providers;
	s32                               m_offset;
};


#endif // MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H
