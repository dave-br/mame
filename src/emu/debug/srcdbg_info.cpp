// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_info.cpp

    TODO

***************************************************************************/


#include "emu.h"
#include "srcdbg_info.h"
#include "srcdbg_api.h"

#include "emuopts.h"
#include "fileio.h"


// static
std::unique_ptr<srcdbg_info> srcdbg_info::create_debug_info(running_machine &machine)
{
	const char * di_paths = machine.options().srcdbg_info();
	if (di_paths[0] == 0)
	{
		return nullptr;
	}

	std::unique_ptr<srcdbg_info> ret = std::make_unique<srcdbg_info>(machine);

	path_iterator path_it(di_paths);
	std::string di_path;
	while (!path_it.next(di_path))
	{
		std::unique_ptr<srcdbg_provider_base> provider(srcdbg_provider_base::create_debug_info(machine, di_path));
		if (provider == nullptr)
		{
			return nullptr;
		}

		srcdbg_provider_entry sp(/* di_path,*/ provider);
	}

	// TODO: verify ~srcdbg_info called if return null
}


srcdbg_info::srcdbg_info(const running_machine& machine)
{}

	// robin all, change params so caller creates the tables,
	// and callees just populate them
void srcdbg_info::get_srcdbg_symbols(
		symbol_table * symtable_srcdbg_globals,
		symbol_table * symtable_srcdbg_locals,
		const device_state_interface * state) const
{
	for (const srcdbg_provider_entry & sp : m_providers)
	{
		if (!sp.enabled())
		{
			continue;
		}

		// Global fixed symbols
		const std::vector<srcdbg_provider_base::global_fixed_symbol> & srcdbg_global_symbols = 
			sp.c_provider()->global_fixed_symbols();
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
		const std::vector<srcdbg_provider_base::local_fixed_symbol> & srcdbg_local_fixed_symbols = 
			sp.c_provider()->local_fixed_symbols();
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
void srcdbg_info::complete_local_relative_initialization()
{
	for (srcdbg_provider_entry & sp : m_providers)
	{
		if (!sp.enabled())
		{
			continue;
		}

		sp.provider()->complete_local_relative_initialization();
	}
}

u32 srcdbg_info::num_files() const
{
	return m_agg_file_to_provider_file.size();
}


const srcdbg_provider_base::source_file_path & srcdbg_info::file_index_to_path(u32 file_index) const
{
	std::pair provider_file = m_agg_file_to_provider_file[file_index];
	return m_providers[provider_file.first].c_provider()->file_index_to_path(provider_file.second);
}

std::optional<u32> srcdbg_info::file_path_to_index(const char * file_path) const
{
	// Ask all enabled providers for the answer without short-circuiting, so we
	// can detect if > 1 provider found a match (in which case file_path is
	// ambiguous, and an empty optional should be returned)
	std::optional<std::pair<offs_t, u32>> found_index;
	for (offs_t provider_idx = 0; provider_idx < m_providers.size(); provider_idx++)
	{
		const srcdbg_provider_entry & sp = m_providers[provider_idx];
		if (!sp.enabled())
		{
			continue;
		}

		std::optional<u32> file_idx = sp.c_provider()->file_path_to_index(file_path);
		if (file_idx.has_value())
		{
			if (found_index.has_value())
			{
				// Two providers found a match, so input string is ambiguous
				return std::optional<u32>();
			}
			found_index = std::pair(provider_idx, file_idx.value());
		}
	}

	if (!found_index.has_value())
	{
		return std::optional<u32>();
	}

	// Convert from provider's index space to coalesced index space
	return m_provider_file_to_agg_file[found_index.value().first][found_index.value().second];
}

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
void srcdbg_info::file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const
{
	std::pair provider_file = m_agg_file_to_provider_file[file_index];
	return m_providers[provider_file.first].c_provider()->
		file_line_to_address_ranges(provider_file.second, line_number, ranges);
}

	// robin to first successful
bool srcdbg_info::address_to_file_line (offs_t address, file_line & loc) const
{
	for (offs_t provider_idx = 0; provider_idx < m_providers.size(); provider_idx++)
	{
		const srcdbg_provider_entry & sp = m_providers[provider_idx];
		if (!sp.enabled())
		{
			continue;
		}

		if (sp.c_provider()->address_to_file_line(address, loc))
		{
			// Convert from provider index space into coalesced index space
			loc.set(
				m_provider_file_to_agg_file[provider_idx][loc.file_index()],
				loc.line_number());
			return true;
		}
	}
	return false;
}
