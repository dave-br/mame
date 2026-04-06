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

#include <filesystem>


//-------------------------------------------------
// line_indexed_file - constructor
//-------------------------------------------------

line_indexed_file::line_indexed_file() :
	m_err(),
	m_data(),
	m_line_starts()
{
}


//-------------------------------------------------
// open - Reads full contents of text file,
// and initializes line index
//-------------------------------------------------

const std::error_condition & line_indexed_file::open(const char * file_path)
{
    // TODO: This should be configurable
    const u32 SPACES_PER_TAB_STOP = 4;

	m_data.resize(0);
	m_line_starts.resize(0);
	m_err = util::core_file::load(file_path, m_data);
	if (m_err)
	{
		return m_err;
	}

	u32 cur_line_start = 0;
    for (u32 i = 0; i < m_data.size() - 1; i++)                 // Ignore final char, enable [i+1] in body
	{
		// Replace tabs with spaces for more consistent alignment
		if (m_data[i] == '\t')
		{
            u32 col = i - cur_line_start;
            s32 num_spaces_until_next_tab_stop = SPACES_PER_TAB_STOP - (col % SPACES_PER_TAB_STOP);
            m_data[i] = ' ';									// Tab char -> first space
			for (s32 j = 0; j < num_spaces_until_next_tab_stop - 1; j++)
			{
                m_data.insert(m_data.cbegin() + i, ' ');		// Insert remaining spaces
			}
            i += num_spaces_until_next_tab_stop - 1;			// Skip over inserted spaces
			continue;
		}
		
		// Check for line endings
		bool crlf = (m_data[i] == '\r' && m_data[i+1] == '\n');
		bool line_end = crlf || (m_data[i] == '\n');
		if (!line_end)
		{
			continue;
		}

		m_data[i] = '\0';                                       // Terminate line
		m_line_starts.push_back(cur_line_start);                // Record line's starting index
		if (crlf)
		{
			i++;                                                // Skip \n in \r\n
		}
		cur_line_start = i+1;                                   // Prepare for next line
	}

	m_line_starts.push_back(cur_line_start);
	m_data.push_back('\0');
	return m_err;
}

// static 
std::unique_ptr<srcdbg_info> srcdbg_info::create_debug_info(running_machine &machine)
{
	const char * di_paths = machine.options().srcdbginfo();
	if (di_paths[0] == 0)
	{
		return nullptr;
	}

	std::unique_ptr<srcdbg_info> ret = std::make_unique<srcdbg_info>(machine);

	path_iterator path_it(di_paths);
	std::string di_path;
	while (path_it.next(di_path))
	{
		srcdbg_provider_base * provider = srcdbg_provider_base::create_debug_info(machine, di_path);
		if (provider == nullptr)
		{
			return nullptr;
		}

		srcdbg_provider_entry sp(di_path, provider);
		ret->m_providers.push_back(std::move(sp));
	}

	ret->coalesce();

	// TODO: verify ~srcdbg_info called if return null
	return ret;
}

srcdbg_info::srcdbg_info(const running_machine& machine)
	: m_agg_file_to_provider_files()
	, m_provider_file_to_agg_file()
	, m_providers()
	, m_offset(machine.options().srcdbg_offset())
	, m_view_needs_full_refresh(true)
{
}

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
		for (const srcdbg_provider_base::local_fixed_symbol & sym : srcdbg_local_fixed_symbols)
		{
			symtable_srcdbg_locals->add(sym.name(), pc_getter_binding, sym.ranges(), sym.value());
		}

		// Local "relative" symbols (values are offsets to a register)
		const std::vector<srcdbg_provider_base::local_relative_symbol> & srcdbg_local_relative_symbols = 
			sp.c_provider()->local_relative_symbols();
		for (const srcdbg_provider_base::local_relative_symbol & sym : srcdbg_local_relative_symbols)
		{
			symtable_srcdbg_locals->add(sym.name(), pc_getter_binding, sym.ranges());
		}
	}
}

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
	return m_agg_file_to_provider_files.size();
}


bool srcdbg_info::file_index_to_path(u32 file_index, const source_file_path ** path) const
{ 
	std::vector<std::pair<std::size_t, u32>> provider_files;
	if (!file_index_to_provider_files(file_index, provider_files))
	{
		return false;
	}

	for (const std::pair<std::size_t, u32> & pf : provider_files)
	{
		std::size_t prov_idx = pf.first;
		if (!m_providers[prov_idx].enabled())
		{
			continue;
		}

		u32 local_file_idx = pf.second;
		return m_providers[prov_idx].c_provider()->file_index_to_path(local_file_idx, path);
	}

	return false;
}


std::optional<u32> srcdbg_info::file_path_to_index(const char * file_path) const
{
	// Find first enabled provider who claims this path, to look up
	// the aggregated file index
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
			return m_provider_file_to_agg_file[provider_idx][file_idx.value()];
		}
	}

	return std::optional<u32>();
}


// Private helper to look up aggregated file index, and return list of
// (provider, local index) pairs
bool srcdbg_info::file_index_to_provider_files(u32 file_index, std::vector<std::pair<std::size_t, u32>> & ret) const
{
	if (file_index >= m_agg_file_to_provider_files.size())
	{
		return false;
	}

	ret = m_agg_file_to_provider_files[file_index];
	return true;
}


void srcdbg_info::file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const
{
	std::vector<std::pair<std::size_t, u32>> provider_files;	
	if (!file_index_to_provider_files(file_index, provider_files))
	{
		return;
	}

	// Multiple providers might know about this file.  Find the first one who
	// knows about this line
	for (u32 i = 0; i < provider_files.size(); i++)
	{
		std::size_t provider_idx = provider_files[i].first;
		const srcdbg_provider_entry & provider = m_providers[provider_idx];
		if (!provider.enabled())
		{
			continue;
		}

		provider.c_provider()->file_line_to_address_ranges(provider_files[i].second, line_number, ranges);
		if (ranges.size() > 0)
		{
			return;
		}
	}
}

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

bool srcdbg_info::update_view_needs_full_refresh()
{
	bool ret = m_view_needs_full_refresh;
	m_view_needs_full_refresh = false;
	return ret;
}


void srcdbg_info::coalesce()
{
	namespace fs = std::filesystem;

	m_provider_file_to_agg_file.clear();
	m_agg_file_to_provider_files.clear();

	// Keep track of duplicate file paths.
	std::vector<std::pair<const char *, u32>> path_and_agg_idxs;

	// Ensure m_provider_file_to_agg_file is pre-sized so as we encounter
	// each provider, we'll always have an entry ready for it
	m_provider_file_to_agg_file.reserve(m_providers.size());
	m_provider_file_to_agg_file.resize(m_providers.size());

	for (offs_t provider_idx = 0; provider_idx < m_providers.size(); provider_idx++)
	{
		const srcdbg_provider_entry & sp = m_providers[provider_idx];
		const srcdbg_provider_base * provider = sp.c_provider();
		if (provider->num_files() == 0)
		{
			continue;
		}

		m_provider_file_to_agg_file[provider_idx] = std::vector<u32>();
		for (u32 file_idx = 0; file_idx < provider->num_files(); file_idx++)
		{
			const source_file_path * sfp = nullptr;
			if (!provider->file_index_to_path(file_idx, &sfp))
			{
				break;
			}

			// If this file has already been encountered from another provider,
			// reuse the same aggregated file index
			u32 agg_file_idx = u32(-1);
			
			for (const std::pair<const char *, u32> & path_agg : path_and_agg_idxs)
			{
				if (fs::equivalent(path_agg.first, sfp->local()))
				{
					// Already seen.  Reuse its aggregated file index
					agg_file_idx = path_agg.second;
					break;
				}
			}

			if (agg_file_idx == u32(-1))
			{
				// New.  Use the next available aggregated index
				agg_file_idx = m_agg_file_to_provider_files.size();
				m_agg_file_to_provider_files.push_back(std::vector<std::pair<std::size_t, u32>>());
				path_and_agg_idxs.push_back(std::pair<const char *, u32>(sfp->local(), agg_file_idx));
			}

			// (provider_idx, file_idx) maps to agg_file_idx
			m_provider_file_to_agg_file[provider_idx].push_back(agg_file_idx);

			// agg_file_idx maps to (provider_idx, file_idx)
			// TODO: These pairs should be replaced with a struct
			m_agg_file_to_provider_files[agg_file_idx].push_back(std::pair(provider_idx, file_idx));
		}
	}
}

bool srcdbg_info::disenable_provider(u64 index, bool enable, std::string & error)
{
	// std::vector<srcdbg_info::srcdbg_provider_entry> & providers = srcdbg->providers();
	if (index >= m_providers.size())
	{
		// TODO: srcdbg_info shouldn't be providing error messages specific to debugcmd.
		// Should return an error code with enough info that debugcmd can craft
		// a complete message itself.
		error = util::string_format(
			"Invalid source-debugging info number: %X\n"
			"Run sdlist for a list of valid source-debugging info numbers.\n",
			index);
		return false;
	}

	srcdbg_info::srcdbg_provider_entry & sp = m_providers[index];
	if (sp.enabled() == enable)
	{
		error = util::string_format(
			"Source-debugging info %X is already %s\n", index, enable ? "enabled" : "disabled");
		return false;
	}

	sp.set_enabled(enable);
	m_view_needs_full_refresh = true;
	return true;
}