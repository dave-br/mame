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

	// // TODO: TEMP CODE TO CREATE TEST DI
	// // (SOME OF THIS MAY BE REPURPOSED TO THE HELPER WRITE FUNCTIONS USED BY TOOLS)
	// FILE * testfptr = fopen(di_path, "wb");

	// const char * source_file_paths[] = { "D:\\coco\\asm\\moon\\Final\\mpE.asm", "D:\\coco\\asm\\sd.asm", "test" };
	// mame_debug_info_simple_header hdr;
	// strncpy(&hdr.header_base.magic[0], "MDbI", 4);
	// strncpy(&hdr.header_base.type[0], "simp", 4);
	// hdr.header_base.version = 1;
	// hdr.source_file_paths_size = 0;
	// for (int i=0; i < _countof(source_file_paths); i++)
	// {
	// 	hdr.source_file_paths_size += strlen(source_file_paths[i]) + 1;
	// }
	// std::pair<u16,u32> line_mappings[] =
	// {
	// 	{0x3F02, 17},
	// 	{0x3F0D, 52},
	// 	{0x3F07, 26},
	// 	{0x3F06, 25},
	// 	{0x3F0A, 34},
	// 	{0x3F00, 14},
	// 	{0x3F09, 33},
	// 	{0x3F0C, 39},
	// 	{0x3F04, 24},
	// };
	// hdr.num_line_mappings = _countof(line_mappings);
	// fwrite(&hdr, sizeof(mame_debug_info_simple_header), 1, testfptr);
	// for (int i=0; i < _countof(source_file_paths); i++)
	// {
	// 	fwrite(source_file_paths[i], strlen(source_file_paths[i])+1, 1, testfptr);
	// }
	// for (int i=0; i < _countof(line_mappings); i++)
	// {
	// 	mdi_line_mapping mapping = { line_mappings[i].first, line_mappings[i].first, 1, line_mappings[i].second };
	// 	fwrite(&mapping, sizeof(mapping), 1, testfptr);
	// }
	// fclose(testfptr);

	// // END TEMP CODE

	std::vector<uint8_t> data;
	util::core_file::load(di_path, data);
	
	// TODO: Validate header

	// FUTURE: Insert code here that validates di_path is a path to a MAME
	// debug info file, reads the header, and instantiates the corresponding
	// debug_info_* class to read it.  For now, only debug_info_simple
	// is supported

	return std::make_unique<debug_info_simple>(machine, data);

}

template <typename T> static void read_field(T& var, std::vector<uint8_t>& data, u32& i)
{
	if (data.size() < i + sizeof(T))
	{
		// TODO ERROR
		assert(false);
	}

	var = *(T*) &data[i];
	i += sizeof(T);
}

debug_info_simple::debug_info_simple(running_machine& machine, std::vector<uint8_t>& data) :
	debug_info_provider_base(),
	// m_source_file_path_chars(),
	m_source_file_paths(),
	m_linemaps_by_address(),
	m_linemaps_by_line(),
	m_symbols()
{
	mame_debug_info_header_base * header_base = (mame_debug_info_header_base *) &data[0];
	if (strncmp(header_base->magic, "MDbI", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info as err condition
	if (strncmp(header_base->type, "simp", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info to decide to call this fcn
	if (header_base->version != 1) { assert(false); };						// TODO: ERROR

	u32 i = 0;
	mame_debug_info_simple_header header;
	read_field<mame_debug_info_simple_header>(header, data, i);

	if (header.source_file_paths_size == 0)
	{
		// TODO ERROR
		assert(false);
	}

	u32 first_line_mapping = i + header.source_file_paths_size;
	if (data.size() <= first_line_mapping - 1)
	{
		// TODO ERROR
		assert(false);
	}

	// Final byte before first_line_mapping must be null-terminator.
	// Ensuring this now avoids buffer overruns when reading the strings
	if (data[first_line_mapping - 1] != '\0')
	{
		// TODO ERROR
		assert(false);		
	}

	u32 first_symbol_name = first_line_mapping + (header.num_line_mappings * sizeof(mdi_line_mapping));
	u32 first_symbol_address = first_symbol_name + header.symbol_names_size;

	if (header.symbol_names_size > 0)
	{
		if (data.size() <= first_symbol_address)
		{
			// TODO ERROR
			assert(false);
		}

		if (data[first_symbol_address - 1] != '\0')
		{
			// TODO ERROR
			assert(false);
		}
	}

	u32 string_start;
	string_start = i;
	for (; i < first_line_mapping; i++)
	{
		if (data[i] == '\0')
		{
			// i points to character immediately following null terminator, so
			// previous string runs from string_start through i - 1, and
			// i begins a new string
			std::string built((const char *) &data[string_start], i - string_start);
			std::string local;
			generate_local_path(machine, built, local);
			debug_info_provider_base::source_file_path sfp(built, local);
			m_source_file_paths.push_back(std::move(sfp));
			string_start = i + 1;
		}
	}

	// Populate m_line_maps_by_line and m_linemaps_by_address with mdi_line_mapping
	// entries from debug info.  Ensure m_linemaps_by_line is pre-sized so as
	// we encounter a mapping, we'll always have an entry ready for its source file index.
	m_linemaps_by_line.reserve(m_source_file_paths.size());
	m_linemaps_by_line.resize(m_source_file_paths.size());
	for (u32 line_map_idx = 0; line_map_idx < header.num_line_mappings; line_map_idx++)
	{
		mdi_line_mapping line_map;
		read_field<mdi_line_mapping>(line_map, data, i);
		m_linemaps_by_address.push_back(line_map);
		if (line_map.source_file_index >= m_source_file_paths.size())
		{
			// TODO ERROR
			assert(false);		
		}
		address_line addrline = {line_map.address_first, line_map.address_last, line_map.line_number};
		m_linemaps_by_line[line_map.source_file_index].push_back(addrline);
	}

	std::sort(
		m_linemaps_by_address.begin(),
		m_linemaps_by_address.end(), 
		[] (const mdi_line_mapping &linemap1, const mdi_line_mapping &linemap2) { return linemap1.address_first < linemap2.address_first; });
	
	for (u16 file_idx = 0; file_idx < m_source_file_paths.size(); file_idx++)
	{
		std::sort(
			m_linemaps_by_line[file_idx].begin(),
			m_linemaps_by_line[file_idx].end(), 
			[] (const address_line& adrline1, const address_line &adrline2) { return adrline1.line_number < adrline2.line_number; });
	}

	// Symbols
	if (header.symbol_names_size == 0)
	{
		return;
	}
	string_start = i;
	u32 j = first_symbol_address;
	for (; i < first_symbol_address; i++)
	{
		if (data[i] == '\0')
		{
			// i points to character immediately following null terminator, so
			// previous string runs from string_start through i - 1, and
			// i begins a new string
			std::string symbol_name((const char *) &data[string_start], i - string_start);
			s32 symbol_value;
			read_field<s32>(symbol_value, data, j);
			symbol sym(symbol_name, symbol_value);
			m_symbols.push_back(std::move(sym));
			string_start = i + 1;
		}
	}
}

void debug_info_simple::generate_local_path(running_machine& machine, const std::string & built, std::string & local)
{
	namespace fs = std::filesystem;
	local = built;                          // Default local path to the originally built source path
	apply_source_map(machine, local);       // Apply the source map if built matches a prefix
	if (osd_is_absolute_path(local))        // If local is already absolute, we're done
	{
		return;
	}

	// Go through source search path until we can construct an
	// absolute path to an existing file
	path_iterator path(machine.options().debug_source_path());
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

void debug_info_simple::apply_source_map(running_machine& machine, std::string & local)
{
	path_iterator path(machine.options().debug_source_path_map());
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
std::optional<debug_info_simple::address_range> debug_info_simple::file_line_to_address_range (u16 file_index, u32 line_number) const
{
	if (file_index >= m_linemaps_by_line.size())
	{
		return std::optional<address_range>();
	}

	const std::vector<address_line> & list = m_linemaps_by_line[file_index];
	if (list.size() == 0)
	{
		return std::optional<address_range>();
	}

	auto answer = std::lower_bound(
		list.cbegin(), 
		list.cend(), 
		line_number,
		[] (auto const &adrline, u32 line) { return adrline.line_number < line; });
	if (answer == list.cend())
	{
		// line_number > last mapped line, so just use the last mapped line
		return address_range((answer-1)->address_first, (answer-1)->address_last);
	}

	// m_line_maps_by_line is sorted by line.  answer is the leftmost entry
	// with line_number <= answer->line_number.  If they're equal, answer
	// is clearly correct.  If line_number < answer->line_number, then
	// using answer bumps us forward to the next mapped line after the 
	// line specified by the user, which is reasonable.
	return address_range(answer->address_first, answer->address_last);
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
		[] (auto const &linemap, u16 addr) { return linemap.address_first < addr; });
	if (guess == m_linemaps_by_address.cend())
	{
		// address > last mapped address_first, so consider the last address range
		guess--;
	}

	// m_linemaps_by_address is sorted by address_first.  guess is
	// the leftmost entry with address <= guess->address_first.  If they're
	// equal, guess is our answer.  Otherwise, check the preceding entry.
	if (guess->address_first <= address && address <= guess->address_last)
	{
		return std::optional<file_line>({ guess->source_file_index, guess->line_number });
	}
	guess--;
	if (guess >= m_linemaps_by_address.cbegin() &&
		guess->address_first <= address && address <= guess->address_last)
	{
		return std::optional<file_line>({ guess->source_file_index, guess->line_number });
	}

	return std::optional<file_line>();
}