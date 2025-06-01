// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider_aggregator.cpp

    TODO

***************************************************************************/


#include "emu.h"
#include "srcdbg_provider_aggregator.h"
#include "srcdbg_api.h"

#include "fileio.h"



srcdbg_provider_aggregator::srcdbg_provider_aggregator(const running_machine& machine)
{}

	// robin all, change params so caller creates the tables,
	// and callees just populate them
void srcdbg_provider_aggregator::get_srcdbg_symbols(
		symbol_table * symtable_srcdbg_globals,
		symbol_table * symtable_srcdbg_locals,
		const device_state_interface * state) const
{
	for (const srcdbg_provider_base & sp : m_providers)
	{
		// Global fixed symbols
		const std::vector<srcdbg_provider_base::global_fixed_symbol> & srcdbg_global_symbols = sp.global_fixed_symbols();
		// *symtable_srcdbg_globals = new symbol_table(parent->machine(), symbol_table::SRCDBG_GLOBALS, parent, device);
		for (const srcdbg_provider_base::global_fixed_symbol & sym : srcdbg_global_symbols)
		{
			// Apply offset to symbol when appropriate
			offs_t value = sym.value();
			if ((sym.flags() & MAME_SRCDBG_SYMFLAG_CONSTANT) == 0)
			{
				value += m_offset;
			}

			symtable_srcdbg_globals->add(sym.name(), value);
		}

		// Local symbols require a PC getter function so they can test if they're
		// currently in scope
		auto pc_getter_binding = std::bind(&device_state_entry::value, state->state_find_entry(STATE_GENPC));

		// Local fixed symbols
		const std::vector<srcdbg_provider_base::local_fixed_symbol> & srcdbg_local_fixed_symbols = sp.local_fixed_symbols();
		// *symtable_srcdbg_locals = new symbol_table(parent->machine(), symbol_table::SRCDBG_LOCALS, *symtable_srcdbg_globals, device);
		for (const srcdbg_provider_base::local_fixed_symbol & sym : srcdbg_local_fixed_symbols)
		{
			symtable_srcdbg_locals->add(sym.name(), pc_getter_binding, sym.ranges(), sym.value());
		}

		// Local "relative" symbols (values are offsets to a register)
		const std::vector<srcdbg_provider_base::local_relative_symbol> & srcdbg_local_relative_symbols = local_relative_symbols();
		for (const srcdbg_provider_base::local_relative_symbol & sym : srcdbg_local_relative_symbols)
		{
			symtable_srcdbg_locals->add(sym.name(), pc_getter_binding, sym.ranges());
		}
	}
}

	// robin all
void srcdbg_provider_aggregator::complete_local_relative_initialization()
{
	for (srcdbg_provider_base & sp : m_providers)
	{
		sp.complete_local_relative_initialization();
	}
}

	// Sum
u32 srcdbg_provider_aggregator::num_files() const
{
	u32 ret = 0;
	for (const srcdbg_provider_base & sp : m_providers)
	{
		ret += sp.num_files();
	}
	return ret;
}

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
const srcdbg_provider_base::source_file_path & srcdbg_provider_aggregator::file_index_to_path(u32 file_index) const
{
	for (const srcdbg_provider_base & sp : m_providers)
	{
		if (file_index < sp.num_files())
		{
			return sp.file_index_to_path(sp.num_files() - file_index);
		}
		file_index -= sp.num_files();
	}
}

	// robin to first successful
std::optional<u32> srcdbg_provider_aggregator::file_path_to_index(const char * file_path) const
{
	for (const srcdbg_provider_base & sp : m_providers)
	{
		std::optional<u32> ret = sp.file_path_to_index(file_path);
		if (ret.has_value())
		{
			return ret;
		}
	}
	return std::optional<u32>();
}

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
void srcdbg_provider_aggregator::file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const
{
	for (const srcdbg_provider_base & sp : m_providers)
	{
		if (file_index < sp.num_files())
		{
			return sp.file_line_to_address_ranges(file_index, line_number, ranges);
		}
		file_index -= sp.num_files();
	}
}

	// robin to first successful
bool srcdbg_provider_aggregator::address_to_file_line (offs_t address, file_line & loc) const
{
	for (const srcdbg_provider_base & sp : m_providers)
	{
		if (sp.address_to_file_line(address, loc))
		{
			return true;
		}
	}
	return false;
}
