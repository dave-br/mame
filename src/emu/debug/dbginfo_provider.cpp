#include "emu.h"
#include "mdisimple.h"
#include "dbginfo_provider.h"
#include "emuopts.h"
#include "fileio.h"
#include "path.h"

#include <filesystem>


// static
std::unique_ptr<debug_info_provider_base> debug_info_provider_base::create_debug_info(running_machine &machine)
{
	const char * di_path = machine.options().debug_info();
	if (di_path[0] == 0)
	{
		return nullptr;
		// TODO: Or, do something like...
		// return std::make_unique<debug_info_empty>(machine);
	}

	std::unique_ptr<debug_info_simple> ret = std::make_unique<debug_info_simple>(machine);
	srcdbg_import importer(*ret);
	std::string error;

	// TODO: ERROR HANDLING
	if (!srcdbg_format_read(di_path, importer, error))
	{
		if (!error.empty())
		{
		}
		return nullptr;
	}

	return ret;
}


	// mame_debug_info_header_base * header_base = (mame_debug_info_header_base *) &data[0];
	// if (strncmp(header_base->magic, "MDbI", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info as err condition
	// if (strncmp(header_base->type, "simp", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info to decide to call this fcn
	// if (header_base->version != 1) { assert(false); };						// TODO: ERROR

	// u32 i = 0;
	// mame_debug_info_simple_header header;
	// read_field<mame_debug_info_simple_header>(header, data, i);

	// if (header.source_file_paths_size == 0)
	// {
	// 	// TODO ERROR
	// 	assert(false);
	// }

	// u32 first_line_mapping = i + header.source_file_paths_size;
	// if (data.size() <= first_line_mapping - 1)
	// {
	// 	// TODO ERROR
	// 	assert(false);
	// }

	// // Final byte before first_line_mapping must be null-terminator.
	// // Ensuring this now avoids buffer overruns when reading the strings
	// if (data[first_line_mapping - 1] != '\0')
	// {
	// 	// TODO ERROR
	// 	assert(false);		
	// }

	// u32 first_symbol_name = first_line_mapping + (header.num_line_mappings * sizeof(mdi_line_mapping));
	// u32 first_symbol_address = first_symbol_name + header.symbol_names_size;

	// if (header.symbol_names_size > 0)
	// {
	// 	if (data.size() <= first_symbol_address)
	// 	{
	// 		// TODO ERROR
	// 		assert(false);
	// 	}

	// 	if (data[first_symbol_address - 1] != '\0')
	// 	{
	// 		// TODO ERROR
	// 		assert(false);
	// 	}
	// }

srcdbg_import::srcdbg_import(debug_info_simple & srcdbg_simple)
	: m_srcdbg_simple(srcdbg_simple) 
	, m_read_line_mapping_yet(false)
	, m_symbol_names()
	, m_state(device_interface_enumerator<device_state_interface>(m_srcdbg_simple.m_machine.root_device()).first())
{
	// device_t * cpu = m_srcdbg_simple.m_machine.device<cpu_device>("maincpu");
	// 	for (device_t &device : device_enumerator(m_machine.root_device()))
	// {
	// 	auto *cpu = dynamic_cast<cpu_device *>(&device);
	// 	if (cpu)
	// 	{
	// 		m_visiblecpu = cpu;
	// 		break;
	// 	}
	// }

	// .cpu	
	// device.interface(m_state);
}


bool srcdbg_import::on_read_source_path(u16 source_path_index, std::string && source_path)
{
	// srcdbg_format_read is required to notify in (contiguous) index order
	assert (m_srcdbg_simple.m_source_file_paths.size() == source_path_index);

	std::string local;
	m_srcdbg_simple.generate_local_path(source_path, local);
	debug_info_provider_base::source_file_path sfp(source_path, local);
	m_srcdbg_simple.m_source_file_paths.push_back(std::move(sfp));
	return true;
}

bool srcdbg_import::on_read_line_mapping(const mdi_line_mapping & line_map)
{
	if (!m_read_line_mapping_yet)
	{
  		// Ensure m_linemaps_by_line is pre-sized so as we encounter a mapping, we'll always have
		// an entry ready for its source file index.
		m_srcdbg_simple.m_linemaps_by_line.reserve(m_srcdbg_simple.m_source_file_paths.size());
		m_srcdbg_simple.m_linemaps_by_line.resize(m_srcdbg_simple.m_source_file_paths.size());
		m_read_line_mapping_yet = true;
	}
	m_srcdbg_simple.m_linemaps_by_address.push_back(line_map);
	debug_info_simple::address_line addrline = {line_map.range.address_first, line_map.range.address_last, line_map.line_number};
	m_srcdbg_simple.m_linemaps_by_line[line_map.source_file_index].push_back(addrline);
	return true;
}

// TODO: SOMETIME AFTER READ ALL LINE MAPPINGS
	// std::sort(
	// 	m_linemaps_by_address.begin(),
	// 	m_linemaps_by_address.end(), 
	// 	[] (const mdi_line_mapping &linemap1, const mdi_line_mapping &linemap2) { return linemap1.address_first < linemap2.address_first; });
	
	// for (u16 file_idx = 0; file_idx < m_source_file_paths.size(); file_idx++)
	// {
	// 	std::sort(
	// 		m_linemaps_by_line[file_idx].begin(),
	// 		m_linemaps_by_line[file_idx].end(), 
	// 		[] (const address_line& adrline1, const address_line &adrline2)
	// 		{
	// 			if (adrline1.line_number == adrline2.line_number)
	// 			{
	// 				return adrline1.address_first < adrline2.address_first;
	// 			}
	// 			return adrline1.line_number < adrline2.line_number; 
	// 		});
	// }

bool srcdbg_import::on_read_symbol_name(u16 symbol_name_index, std::string && symbol_name)
{
	// srcdbg_format_read is required to notify in (contiguous) index order
	assert (m_symbol_names.size() == symbol_name_index);
	m_symbol_names.push_back(std::move(symbol_name));
	return true;
}


bool srcdbg_import::on_read_global_constant_symbol_value(const global_constant_symbol_value & value)
{
	debug_info_provider_base::global_static_symbol sym(m_symbol_names[value.symbol_name_index], value.symbol_value);
	m_srcdbg_simple.m_global_static_symbols.push_back(std::move(sym));
	return true;
}


bool srcdbg_import::on_read_local_constant_symbol_value(const local_constant_symbol_value & value)
{
	std::vector<std::pair<offs_t,offs_t>> ranges;
	for (u32 i = 0; i < value.num_address_ranges; i++)
	{
		ranges.push_back(
			std::move(
				std::pair<offs_t,offs_t>(value.ranges[i].address_first, value.ranges[i].address_last)));
	}

	debug_info_provider_base::local_static_symbol sym(
		m_symbol_names[value.symbol_name_index],
		ranges,
		value.symbol_value);

	// TODO: names: static -> constant
	m_srcdbg_simple.m_local_static_symbols.push_back(std::move(sym));

	return true;
}


bool srcdbg_import::on_read_local_dynamic_symbol_value(const local_dynamic_symbol_value & value)
{
	std::vector<scoped_value_internal> scoped_values;
	for (u32 i = 0; i < value.num_local_dynamic_scoped_values; i++)
	{
		const local_dynamic_scoped_value & sv = value.local_dynamic_scoped_values[i];
		scoped_value_internal scoped_value(
			std::pair<offs_t,offs_t>(sv.range.address_first, sv.range.address_last),
			sv.reg,
			sv.reg_offset);

		scoped_values.push_back(std::move(scoped_value));
	}

	debug_info_simple::local_dynamic_symbol_internal sym(m_symbol_names[value.symbol_name_index], scoped_values);
	m_srcdbg_simple.m_local_dynamic_symbols_internal.push_back(std::move(sym));

	return true;
}



debug_info_simple::debug_info_simple(const running_machine& machine)
	: debug_info_provider_base()
	, m_machine(machine)
	, m_source_file_paths()
	, m_linemaps_by_address()
	, m_linemaps_by_line()
	, m_global_static_symbols()
	, m_local_static_symbols()
{
}

void debug_info_simple::ensure_local_dynamics_ready()
{
	if (m_local_dynamics_ready)
	{
		return;
	}

	for (local_dynamic_symbol_internal sym_internal : m_local_dynamic_symbols_internal)
	{
		std::vector<symbol_table::scoped_value> scoped_values;
		for (scoped_value_internal & sv : sym_internal.scoped_values)
		{
			std::string expr = m_state->state_find_entry(sv.reg)->symbol();
			expr += " + ";
			expr += sv.reg_offset;
			symbol_table::scoped_value scoped_value(std::move(sv.range), expr);
			scoped_values.push_back(std::move(scoped_value));
		}

		debug_info_provider_base::local_dynamic_symbol sym(std::move(sym_internal.symbol_name), scoped_values);
		m_local_dynamic_symbols.push_back(std::move(sym));
	}

	m_local_dynamic_symbols_internal.clear();
	m_local_dynamics_ready = true;
}



void debug_info_simple::generate_local_path(const std::string & built, std::string & local)
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

void debug_info_simple::apply_source_map(std::string & local)
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

		// TODO: VERIFY THIS WORKS WHEN LOCAL IS TOO SHORT
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


// JUST NOW:
/*

use path_iterator against src map to create vector of prefix replacement pairs
Use replacement pairs to adjust each source file (on demand or on init?)
use file_enumerator to search one adjusted file within a set of search paths
*/

std::optional<int> debug_info_simple::file_path_to_index(const char * file_path) const
{
	// TODO: Fancy heuristics to match full paths of source file from
	// dbginfo and local machine
	for (int i=0; i < m_source_file_paths.size(); i++)
	{
		// TODO: Use path helpers to deal with case-insensitivity on other platforms
		if (stricmp(m_source_file_paths[i].built(), file_path) == 0 ||
			stricmp(m_source_file_paths[i].local(), file_path) == 0)
		{
			return i;
		}
	}
	return std::optional<int>();
}

// Given a source file & line number, return the address of the first byte of
// the first instruction of the range of instructions attributable to that line
void debug_info_simple::file_line_to_address_ranges(u16 file_index, u32 line_number, std::vector<address_range> & ranges) const
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
std::optional<file_line> debug_info_simple::address_to_file_line (offs_t address) const
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