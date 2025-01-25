// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider_simple.cpp

    Implementation of interface to source-debugging info for the
	"simple" format

***************************************************************************/


#include "emu.h"
#include "srcdbg_provider_simple.h"

#include "emuopts.h"
#include "fileio.h"
#include "path.h"

#include "corestr.h"

#include <filesystem>
#include <sstream>


srcdbg_import::srcdbg_import(srcdbg_provider_simple & srcdbg_simple)
	: m_srcdbg_simple(srcdbg_simple) 
	, m_symbol_names()
{
}


bool srcdbg_import::on_read_source_path(u32 source_path_index, std::string && source_path)
{
	// srcdbg_format_simp_read is required to notify in (contiguous) index order
	assert (m_srcdbg_simple.m_source_file_paths.size() == source_path_index);

	std::string local;
	m_srcdbg_simple.generate_local_path(source_path, local);
	srcdbg_provider_base::source_file_path sfp(std::move(source_path), std::move(local));
	m_srcdbg_simple.m_source_file_paths.push_back(std::move(sfp));
	return true;
}


bool srcdbg_import::end_read_source_paths()
{
	// Ensure m_linemaps_by_line is pre-sized so as we encounter a mapping, we'll always have
	// an entry ready for its source file index.
	m_srcdbg_simple.m_linemaps_by_line.reserve(m_srcdbg_simple.m_source_file_paths.size());
	m_srcdbg_simple.m_linemaps_by_line.resize(m_srcdbg_simple.m_source_file_paths.size());
	return true; 
}


bool srcdbg_import::on_read_line_mapping(const srcdbg_line_mapping & line_map)
{
	m_srcdbg_simple.m_linemaps_by_address.push_back(line_map);
	srcdbg_provider_simple::address_line addrline = {line_map.range.address_first, line_map.range.address_last, line_map.line_number};
	m_srcdbg_simple.m_linemaps_by_line[line_map.source_file_index].push_back(addrline);
	return true;
}

bool srcdbg_import::end_read_line_mappings()
{
	// For each source file, sort its linemaps_by_line by line number
	for (u32 file_idx = 0; file_idx < m_srcdbg_simple.m_source_file_paths.size(); file_idx++)
	{
		std::sort(
			m_srcdbg_simple.m_linemaps_by_line[file_idx].begin(),
			m_srcdbg_simple.m_linemaps_by_line[file_idx].end(), 
			[] (const srcdbg_provider_simple::address_line& adrline1, const srcdbg_provider_simple::address_line &adrline2)
			{
				if (adrline1.line_number == adrline2.line_number)
				{
					return adrline1.address_first < adrline2.address_first;
				}
				return adrline1.line_number < adrline2.line_number; 
			});
	}

	// Sort the linemaps_by_address by the first address of each range
	std::sort(
		m_srcdbg_simple.m_linemaps_by_address.begin(),
		m_srcdbg_simple.m_linemaps_by_address.end(), 
		[] (const srcdbg_line_mapping &linemap1, const srcdbg_line_mapping &linemap2)
		{
			return linemap1.range.address_first < linemap2.range.address_first;
		});
	
	return true; 
}

bool srcdbg_import::on_read_symbol_name(u32 symbol_name_index, std::string && symbol_name)
{
	// srcdbg_format_simp_read is required to notify in (contiguous) index order
	assert (m_symbol_names.size() == symbol_name_index);
	m_symbol_names.push_back(std::move(symbol_name));
	return true;
}


bool srcdbg_import::on_read_global_fixed_symbol_value(const global_fixed_symbol_value & value)
{
	srcdbg_provider_base::global_fixed_symbol sym(m_symbol_names[value.symbol_name_index], value.symbol_value);
	m_srcdbg_simple.m_global_fixed_symbols.push_back(std::move(sym));
	return true;
}


bool srcdbg_import::on_read_local_fixed_symbol_value(const local_fixed_symbol_value & value)
{
	std::vector<std::pair<offs_t,offs_t>> ranges;
	for (u32 i = 0; i < value.num_address_ranges; i++)
	{
		ranges.push_back(
			std::move(
				std::pair<offs_t,offs_t>(value.ranges[i].address_first, value.ranges[i].address_last)));
	}

	srcdbg_provider_base::local_fixed_symbol sym(
		m_symbol_names[value.symbol_name_index],
		std::move(ranges),
		value.symbol_value);

	m_srcdbg_simple.m_local_fixed_symbols.push_back(std::move(sym));

	return true;
}


bool srcdbg_import::on_read_local_relative_symbol_value(const local_relative_symbol_value & value)
{
	std::vector<srcdbg_provider_simple::local_relative_eval_rule_internal> eval_rules_internal;
	for (u32 i = 0; i < value.num_local_relative_eval_rules; i++)
	{
		const local_relative_eval_rule & eval_rule = value.local_relative_eval_rules[i];
		srcdbg_provider_simple::local_relative_eval_rule_internal eval_rule_internal(
			std::move(std::pair<offs_t,offs_t>(eval_rule.range.address_first, eval_rule.range.address_last)),
			eval_rule.reg,
			eval_rule.reg_offset);

		eval_rules_internal.push_back(std::move(eval_rule_internal));
	}

	srcdbg_provider_simple::local_relative_symbol_internal sym(m_symbol_names[value.symbol_name_index], eval_rules_internal);
	m_srcdbg_simple.m_local_relative_symbols_internal.push_back(std::move(sym));

	return true;
}



srcdbg_provider_simple::srcdbg_provider_simple(const running_machine& machine)
	: srcdbg_provider_base()
	, m_machine(machine)
	, m_source_file_paths()
	, m_linemaps_by_address()
	, m_linemaps_by_line()
	, m_global_fixed_symbols()
	, m_local_fixed_symbols()
{
}

void srcdbg_provider_simple::complete_local_relative_initialization()
{
	assert (m_local_relative_symbols.empty());

	device_state_interface * state = device_interface_enumerator<device_state_interface>(m_machine.root_device()).first();

	for (local_relative_symbol_internal sym_internal : m_local_relative_symbols_internal)
	{
		std::vector<symbol_table::local_range_expression> values;
		for (local_relative_eval_rule_internal & eval_rule_internal : sym_internal.m_eval_rules)
		{
			std::ostringstream expr;
			expr << "(" 
				<< state->state_find_entry(eval_rule_internal.m_reg)->symbol() 
				<< " + " 
				<< eval_rule_internal.m_reg_offset 
				<< ")";
			symbol_table::local_range_expression value(std::move(eval_rule_internal.m_range), std::move(std::move(expr).str()));
			values.push_back(std::move(value));
		}

		srcdbg_provider_base::local_relative_symbol sym(sym_internal.m_name, std::move(values));
		m_local_relative_symbols.push_back(std::move(sym));
	}

	m_local_relative_symbols_internal.clear();
}



void srcdbg_provider_simple::generate_local_path(const std::string & built, std::string & local)
{
	namespace fs = std::filesystem;
	local = built;                          // Default local path to the originally built source path
	apply_source_map(local);     			// Apply the source map if built matches a prefix
	if (osd_is_absolute_path(local))        // If local is already absolute, we're done
	{
		return;
	}

	// Go through source search path until we can construct an
	// absolute path to an existing file
	path_iterator path(m_machine.options().debug_source_path());
	std::string source_dir;
	while (path.next(source_dir))
	{
		std::string new_local = util::path_append(source_dir, local);
		if (fs::exists(fs::status(new_local)))
		{
			// Found an existing absolute path, done
			local = std::move(new_local);
			return;
		}
	}

	// None found, leave local == built
}

void srcdbg_provider_simple::apply_source_map(std::string & local)
{
	path_iterator path(m_machine.options().debug_source_path_map());
	std::string prefix_find, prefix_replace;
	while (path.next(prefix_find))
	{
		if (!path.next(prefix_replace))
		{
			// Invalid map string (odd number of paths), so just skip last entry
			break;
		}

		if (strncmp(prefix_find.c_str(), local.c_str(), prefix_find.size()) == 0)
		{
			// Found a match; replace local's prefix_find with prefix_replace
			std::string new_local = prefix_replace;
			new_local += &local[prefix_find.size()];
			local = std::move(new_local);
			return;
		}
	}

	// If we made it here, no match.  So leave local alone
}

static bool suffix_match(const char * full_string, const char * suffix, bool case_sensitive)
{
	size_t full_string_length = strlen(full_string);
	size_t suffix_length = strlen(suffix);
	if (full_string_length < suffix_length)
	{
		return false;
	}

	if (case_sensitive)
	{
		return strcmp(full_string + full_string_length - suffix_length, suffix) == 0;
	}

	return core_stricmp(full_string + full_string_length - suffix_length, suffix) == 0;
}


std::optional<u32> srcdbg_provider_simple::file_path_to_index(const char * file_path) const
{
	std::vector<u32> full_insensitive;
	std::vector<u32> suffix_sensitive;
	std::vector<u32> suffix_insensitive;
	// TODO: Fancy heuristics to match full paths of source file from
	// dbginfo and local machine
	for (u32 i=0; i < m_source_file_paths.size(); i++)
	{
		// Full, case-sensitive match?  Done.
		if (strcmp(m_source_file_paths[i].built(), file_path) == 0 ||
			strcmp(m_source_file_paths[i].local(), file_path) == 0)
		{
			return i;
		}
		// Full, case-insensitive match?  Save and see if we find anything better
		else if (core_stricmp(m_source_file_paths[i].built(), file_path) == 0 ||
			core_stricmp(m_source_file_paths[i].local(), file_path) == 0)
		{
			full_insensitive.push_back(i);
		}
		// Suffix, case-sensitive match?  Save and see if we find anything better
		else if (suffix_match(m_source_file_paths[i].built(), file_path, true) ||
			suffix_match(m_source_file_paths[i].local(), file_path, true))
		{
			suffix_sensitive.push_back(i);
		}
		// Suffix, case-insensitive match?  Save and see if we find anything better
		else if (suffix_match(m_source_file_paths[i].built(), file_path, false) ||
			suffix_match(m_source_file_paths[i].local(), file_path, false))
		{
			suffix_insensitive.push_back(i);
		}
	}

	// Go through lists in priority order to find a match
	const std::vector<u32> * match_lists[] = { &full_insensitive, &suffix_sensitive, &suffix_insensitive };
	for (u32 list_idx = 0; list_idx < 3; list_idx++)
	{
		if (match_lists[list_idx]->size() == 1)
		{
			return match_lists[list_idx]->at(0);
		}

		if (match_lists[list_idx]->size() > 1)
		{
			// Error: file_path ambiguous
			return std::optional<int>();
		}
	}

	// Error: file_path not found
	return std::optional<int>();
}

// Given a source file & line number, return the address of the first byte of
// the first instruction of the range of instructions attributable to that line
void srcdbg_provider_simple::file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const
{
	if (file_index >= m_linemaps_by_line.size())
	{
		return;
	}

	const std::vector<address_line> & list = m_linemaps_by_line[file_index];
	if (list.size() == 0)
	{
		return;
	}

	auto answer = std::lower_bound(
		list.cbegin(), 
		list.cend(), 
		line_number,
		[] (auto const &adrline, u32 line) { return adrline.line_number < line; });
	if (answer == list.cend())
	{
		// line_number > last mapped line, so just use the last mapped line
		return ranges.push_back(address_range((answer-1)->address_first, (answer-1)->address_last));
	}

	// m_line_maps_by_line is sorted by line, then address.  answer is the leftmost entry
	// with line_number <= answer->line_number.  Add all ranges with matching line number
	while (answer->line_number == line_number)
	{
		ranges.push_back(address_range(answer->address_first, answer->address_last));
		answer++;
	}
}

// Given an address, return the source file & line number attributable to the
// range of addresses that includes the specified address
std::optional<file_line> srcdbg_provider_simple::address_to_file_line (offs_t address) const
{
	assert(m_linemaps_by_address.size() > 0);

	auto guess = std::lower_bound(
		m_linemaps_by_address.cbegin(), 
		m_linemaps_by_address.cend(),
		address,
		[] (auto const &linemap, u16 addr) { return linemap.range.address_first < addr; });
	if (guess == m_linemaps_by_address.cend())
	{
		// address > last mapped address_first, so consider the last address range
		guess--;
	}

	// m_linemaps_by_address is sorted by address_first.  guess is
	// the leftmost entry with address <= guess->address_first.  If they're
	// equal, guess is our answer.  Otherwise, check the preceding entry.
	if (guess->range.address_first <= address && address <= guess->range.address_last)
	{
		return std::optional<file_line>({ guess->source_file_index, guess->line_number });
	}
	guess--;
	if (guess >= m_linemaps_by_address.cbegin() &&
		guess->range.address_first <= address && address <= guess->range.address_last)
	{
		return std::optional<file_line>({ guess->source_file_index, guess->line_number });
	}

	return std::optional<file_line>();
}