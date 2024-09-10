// license:BSD-3-Clause
// copyright-holders: TODO
/*********************************************************************

    dvsourcecode.cpp

    Source code debugger view.

***************************************************************************/

#include "emu.h"
#include "dvsourcecode.h"
#include "emuopts.h"
#include "debugger.h"
#include "mdisimple.h"

line_indexed_file::line_indexed_file() :
	m_data(),
	m_line_starts()
{
}

std::error_condition line_indexed_file::open(const char * file_path)
{
	std::error_condition err = util::core_file::load(file_path, m_data);
	if (err)
	{
		return err;
	}

	u32 cur_line_start = 0;
	for (u32 i = 0; i < m_data.size() - 1; i++)                 // Ignore final char, enable [i+1] in body
	{
		// Check for line endings
		bool crlf = false;
		if (m_data[i] == '\r' && m_data[i+1] == '\n')
		{
			crlf = true;
		}
		else if (m_data[i] != '\n')
		{
			// Neither crlf nor lf, so not a line ending
			continue;
		}

		m_data[i] = '\0';                                       // Terminate line
		m_line_starts.push_back(cur_line_start);                // Record line's starting index
		if (crlf)
		{
			i++;    // Skip \n in \r\n
		}
		cur_line_start = i+1;                                   // Prepare for next line
	}
	m_data[m_data.size()-1] = '\0';
	return std::error_condition();
}


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

	// TODO: TEMP CODE TO CREATE TEST DI
	// (SOME OF THIS MAY BE REPURPOSED TO THE HELPER WRITE FUNCTIONS USED BY TOOLS)
	FILE * testfptr = fopen(di_path, "w");

	const char * source_file_paths[] = { "D:\\coco\\asm\\moon\\Final\\mpE.asm", "D:\\coco\\asm\\sd.asm", "test" };
	mame_debug_info_simple_header hdr;
	strncpy(&hdr.magic[0], "MDbI", 4);
	strncpy(&hdr.type[0], "simp", 4);
	hdr.version = 1;
	hdr.source_file_paths_size = 0;
	for (int i=0; i < _countof(source_file_paths); i++)
	{
		hdr.source_file_paths_size += strlen(source_file_paths[i]) + 1;
	}
	std::pair<u16,u32> line_mappings[] =
	{
		{0x3F02, 17},
		{0x3F0D, 52},
		{0x3F07, 26},
		{0x3F06, 25},
		{0x3F0A, 34},
		{0x3F00, 14},
		{0x3F09, 33},
		{0x3F0C, 39},
		{0x3F04, 24},
	};
	hdr.num_line_mappings = _countof(line_mappings);
	fwrite(&hdr, sizeof(mame_debug_info_simple_header), 1, testfptr);
	for (int i=0; i < _countof(source_file_paths); i++)
	{
		fwrite(source_file_paths[i], strlen(source_file_paths[i])+1, 1, testfptr);
	}
	for (int i=0; i < _countof(line_mappings); i++)
	{
		mdi_line_mapping mapping = { line_mappings[i].first, line_mappings[i].first, 1, line_mappings[i].second };
		fwrite(&mapping, sizeof(mapping), 1, testfptr);
	}
	fclose(testfptr);

	// END TEMP CODE

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
	m_source_file_path_chars(),
	m_source_file_paths(),
	m_linemaps_by_address(),
	m_linemaps_by_line()
{
	mame_debug_info_header_base * header_base = (mame_debug_info_header_base *) &data[0];
	if (strncmp(header_base->magic, "MDbI", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info as err condition
	if (strncmp(header_base->type, "simp", 4) != 0) { assert(false); };		// TODO: Move to debug_info_provider_base::create_debug_info to decide to call this fcn
	if (header_base->version != 1) { assert(false); };						// TODO: ERROR

	u32 i = 0;
	mame_debug_info_simple_header header;
	read_field<mame_debug_info_simple_header>(header, data, i);

	u32 first_line_mapping = i + header.source_file_paths_size;
	if (data.size() <= first_line_mapping - 1)
	{
		// TODO ERROR
		assert(false);
	}

	// Final byte before first_line_mapping must be null-terminator.
	// Ensuring this now avoids buffer overruns when reading the strings
	if (data[first_line_mapping - 1] != 0)
	{
		// TODO ERROR
		assert(false);		
	}

	// Read the starting char address for each source file path into vector
	m_source_file_paths.push_back((const char*) &data[i++]);     // First string starts immediately
	for (; i < first_line_mapping - 1; i++)                      // Exclude first_line_mapping - 1; it is final null-terminator
	{
		if (data[i-1] == '\0')
		{
			// i points to character immediately following null terminator, so
			// i begins a new string
			m_source_file_paths.push_back((const char*) &data[i]);
		}
	}

	// Populate m_line_maps_by_line and m_linemaps_by_address with mdi_line_mapping
	// entries from debug info
	m_linemaps_by_line.reserve(m_source_file_paths.size());
	for (u32 line_map_idx = 0; line_map_idx < header.num_line_mappings; line_map_idx++)
	{
		mdi_line_mapping line_map;
		read_field<mdi_line_mapping>(line_map, data, i);
		m_linemaps_by_address.push_back(line_map);
		if (line_map.source_file_index >= m_linemaps_by_line.size())
		{
			// TODO ERROR
			assert(false);		
		}
		m_linemaps_by_line[line_map.source_file_index].push_back({line_map.address_first, line_map.line_number});
	}

	std::sort(
		m_linemaps_by_address.begin(),
		m_linemaps_by_address.end(), 
		[] (const mdi_line_mapping &linemap1, const mdi_line_mapping &linemap2) { return linemap1.address_first < linemap2.address_first; });
	
	for (u16 i = 0; i < m_source_file_paths.size(); i++)
	{
		std::sort(
			m_linemaps_by_line[i].begin(),
			m_linemaps_by_line[i].end(), 
			[] (const address_line& adrline1, const address_line &adrline2) { return adrline1.line_number < adrline1.line_number; });
	}
}

std::optional<int> debug_info_simple::file_path_to_index(const char * file_path) const
{
	// TODO: Fancy heuristics to match full paths of source file from
	// dbginfo and local machine
	for (int i=0; i < this->m_source_file_paths.size(); i++)
	{
		// TODO: Use path helpers to deal with case-insensitivity on other platforms
		if (stricmp(m_source_file_paths[i], file_path) == 0)
		{
			return i;
		}
	}
	return std::optional<int>();
}

// Given a source file & line number, return the address of the first byte of
// the first instruction of the range of instructions attributable to that line
std::optional<u16> debug_info_simple::file_line_to_address (u16 file_index, u32 line_number) const
{
	if (file_index >= m_linemaps_by_line.size())
	{
		return std::optional<u16>();
	}

	const std::vector<address_line> & list = m_linemaps_by_line[file_index];
	assert (list.size() > 0);

	auto answer = std::lower_bound(
		list.cbegin(), 
		list.cend(), 
		line_number,
		[] (auto const &adrline, u32 line) { return adrline.line_number < line; });
	if (answer == list.cend())
	{
		// line_number > last mapped line, so just use the last mapped line
		return (answer-1)->address_first;
	}

	// m_line_maps_by_line is sorted by line.  answer is the leftmost entry
	// with line_number <= answer->line_number.  If they're equal, answer
	// is clearly correct.  If line_number < answer->line_number, then
	// using answer bumps us forward to the next mapped line after the 
	// line specified by the user.
	return answer->address_first;
}

// Given an address, return the source file & line number attributable to the
// range of addresses that includes the specified address
std::optional<file_line> debug_info_simple::address_to_file_line (u16 address) const
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


// // line_to_mapping:
// // Returns pointer to mdi_line_mapping with address matching parameter.  If no
// // such mdi_line_mapping exists, returns pointer to mdi_line_mapping with

// // Assumes mdi_line_mapping structures are contiguously ordered by address.
// // Returns pointer to mdi_line_mapping with address matching parameter.  If no
// // such mdi_line_mapping exists, returns pointer to mdi_line_mapping with
// // 
// const mdi_line_mapping * address_to_mapping(u16 address, mdi_line_mapping * start, int count)
// {
// 	// I hate adding my own binary search here, but I see no way in O(1) time
// 	// to initialize an STL class to wrap the C-style mdi_line_mapping[], so that
// 	// the requirements of stl::lower_bound are in place.  My best hope was
// 	// stl::span, but that does not appear to be compiled into MAME, as
// 	// it's hidden under "#ifdef __cpp_lib_span // C++ >= 20 && concepts"

// 	// Adapted from GCC's binary search

// 	const mdi_line_mapping * base = start;
// 	const mdi_line_mapping * cur = start;

// 	for (; count != 0; count >>= 1)
// 	{
// 		cur = base + (count >> 1);	// * sizeof(mdi_line_mapping);
// 		if (address == cur->address)
// 		{
// 			return cur;
// 		}
// 		else if (address > cur->address)
// 		{
// 			// Move right
// 			base = cur + 1;
// 			count--;
// 		}
// 		// Else, will automatically move left
// 	}
// 	return cur;

// 	// TODO: Actually implement lower bound
// 	// TODO: Add tests somewhere
// }


//-------------------------------------------------
//  debug_view_sourcecode - constructor
//-------------------------------------------------

debug_view_sourcecode::debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate) :
	// debug_view(machine, DVT_SOURCE, osdupdate, osdprivate),
	debug_view_disasm(machine, osdupdate, osdprivate, DVT_SOURCE),
	m_debug_info(machine.debugger().debug_info()),
	m_cur_src_index(0),
	m_displayed_src_index(-1),
	m_displayed_src_file(std::make_unique<line_indexed_file>()),
	m_highlighted_line(4),
	m_first_visible_line(1)
{
}


//-------------------------------------------------
//  ~debug_view_sourcecode - destructor
//-------------------------------------------------

debug_view_sourcecode::~debug_view_sourcecode()
{
}


//-------------------------------------------------
//  view_update - update the contents of the
//  source code view
//-------------------------------------------------

void debug_view_sourcecode::view_update()
{
	if (m_cur_src_index != m_displayed_src_index)
	{
		// Displaying different source file.

		// TODO: Put this directly in open
		//   Close old file, open new file.
		// if (m_displayed_src_file.is_open())
		// {
		// 	m_displayed_src_file.close();
		// }

		std::error_condition err = m_displayed_src_file->open(this->m_debug_info.file_index_to_path(m_cur_src_index));
		if (err)
		{
			// TODO LOG ERROR
			return;
		}

		m_displayed_src_index = m_cur_src_index;
	}

	if (!is_visible(m_highlighted_line))
	{
		adjust_visible_lines();
	}

	// TODO: line_indexed_file class should not include newlines in the buffer, but
	// null-terminators instead

	// 

	for (u32 row = 0; row < m_visible.y; row++)
	{
		u32 line = row + m_first_visible_line;
		if (line > m_displayed_src_file->num_lines())
		{
			print_line(row, " ", DCA_NORMAL);
		}
		else
		{
			print_line(
				row,
				m_displayed_src_file->get_line_text(line),
				(line == m_highlighted_line) ? DCA_CURRENT : DCA_NORMAL);
		}
	}

	// std::string textE = "I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     ";
	// std::string textO = " ";
	// for(u32 row = 0; row < m_visible.y; row++)
	// {
	// 	if (row % 2 == 0)
	// 		print_line(row, textE);
	// 	else
	// 		print_line(row, textO);
	// }

}

void debug_view_sourcecode::adjust_visible_lines()
{
	// Generally center visible line in view, but there are corner cases

	if (m_displayed_src_file->num_lines() <= m_visible.y)
	{
		// Entire file fits in visible view.  Start at begining
		m_first_visible_line = 1;
	}
	else if (m_highlighted_line <= m_visible.y / 2)
	{
		// m_highlighted_line close to top, start at top
		m_first_visible_line = 1;
	}
	else if (m_highlighted_line + m_visible.y / 2 > m_displayed_src_file->num_lines())
	{
		// m_highlighted_line close to bottom, so bottom line at bottom
		m_first_visible_line = m_displayed_src_file->num_lines() - m_visible.y;
	}
	else
	{
		// Main case, center visible line in view
		m_first_visible_line = m_highlighted_line - m_visible.y / 2;
	}
}

void debug_view_sourcecode::print_line(u32 row, const char * text, u8 attrib)
{
	for(s32 visible_col=m_topleft.x; visible_col < m_topleft.x + m_visible.x; visible_col++)
	{
		int viewdata_col = visible_col - m_topleft.x;
		if (visible_col >= strlen(text))
		{
			m_viewdata[row * m_visible.x + viewdata_col] = { ' ', attrib };
		}
		else
		{
			m_viewdata[row * m_visible.x + viewdata_col] = { u8(text[visible_col]), attrib };
		}
	}
}



//-------------------------------------------------
//  view_click - handle a mouse click within the
//  current view
//-------------------------------------------------

void debug_view_sourcecode::view_click(const int button, const debug_view_xy& pos)
{
}