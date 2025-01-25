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
	m_err(),
	m_data(),
	m_line_starts()
{
}

const std::error_condition & line_indexed_file::open(const char * file_path)
{
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
	return m_err;
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
	FILE * testfptr = fopen(di_path, "wb");

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
	if (data[first_line_mapping - 1] != 0)
	{
		// TODO ERROR
		assert(false);		
	}

	// m_source_file_path_chars to own memory containing path characters
	m_source_file_path_chars.reserve(header.source_file_paths_size);
	m_source_file_path_chars.resize(header.source_file_paths_size);
	std::copy(&data[i], &data[i+header.source_file_paths_size], m_source_file_path_chars.begin());

	// Read the starting char address for each source file path into m_source_file_paths
	m_source_file_paths.push_back((const char*) &m_source_file_path_chars[0]);     // First string starts immediately
	for (u32 j = 1; j < m_source_file_path_chars.size() - 1; j++)                  // Exclude m_source_file_path_chars.size() - 1; it is final null-terminator
	{
		if (m_source_file_path_chars[j-1] == '\0')
		{
			// j points to character immediately following null terminator, so
			// j begins a new string
			m_source_file_paths.push_back((const char*) &m_source_file_path_chars[j]);
		}
	}
	i += header.source_file_paths_size;     // Advance i to first mdi_line_mapping

	// Populate m_line_maps_by_line and m_linemaps_by_address with mdi_line_mapping
	// entries from debug info
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
	// line specified by the user.
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


//-------------------------------------------------
//  debug_view_sourcecode - constructor
//-------------------------------------------------

debug_view_sourcecode::debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate) :
	debug_view_disasm(machine, osdupdate, osdprivate, DVT_SOURCE),
	m_debug_info(machine.debugger().debug_info()),
	m_cur_src_index(0),
	m_displayed_src_index(-1),
	m_displayed_src_file(std::make_unique<line_indexed_file>()),
	m_line_for_cur_pc(4)
	// m_first_visible_line(1)
{
	device_t * live_cpu = machine.debugger().cpu().live_cpu();
	live_cpu->interface(m_state);
	m_supports_cursor = true;
}


//-------------------------------------------------
//  ~debug_view_sourcecode - destructor
//-------------------------------------------------

debug_view_sourcecode::~debug_view_sourcecode()
{
}


void debug_view_sourcecode::update_opened_file()
{
	if (m_cur_src_index == m_displayed_src_index)
	{
		return;
	}

	std::error_condition err = m_displayed_src_file->open(m_debug_info.file_index_to_path(m_cur_src_index));
	m_displayed_src_index = m_cur_src_index;
	if (err)
	{
		return;
	}

	m_total.y = m_displayed_src_file->num_lines();
}

//-------------------------------------------------
//  view_update - update the contents of the
//  source code view
//-------------------------------------------------

void debug_view_sourcecode::view_update()
{
	bool pc_changed = false;
	offs_t pc = 0;
	if (m_state != nullptr)
	{
		pc = m_state->pcbase();
		pc_changed = update_previous_pc(pc);
	}

	// Ensure correct file is opened

	if (pc_changed)
	{
		std::optional<file_line> file_line = debug_info().address_to_file_line(pc);
		if (file_line.has_value())
		{
			m_cur_src_index = file_line.value().file_index;
			m_line_for_cur_pc = file_line.value().line_number;
		}
	}

	update_opened_file();

	// Ensure correct set of lines to print

	if (pc_changed)
	{
		update_visible_lines(pc);
	}

	// Print

	if (m_displayed_src_file->last_open_error())
	{
		print_line(0, "Error opening file", DCA_NORMAL);
		print_line(1, m_debug_info.file_index_to_path(m_cur_src_index), DCA_NORMAL);
		print_line(2, m_displayed_src_file->last_open_error().message().c_str(), DCA_NORMAL);
		for (u32 row = 3; row < m_visible.y; row++)
		{
			print_line(row, " ", DCA_NORMAL);
		}
		return;
	}

	for (u32 row = 0; row < m_visible.y; row++)
	{
		u32 line = row + m_topleft.y + 1;
		if (line > m_displayed_src_file->num_lines())
		{
			print_line(row, " ", DCA_NORMAL);
		}
		else
		{
			u8 attrib = DCA_NORMAL;

			if (line == m_line_for_cur_pc)
			{
				// on the line with the PC: highlight
				attrib = DCA_CURRENT;
			}
			else if (exists_bp_for_line(m_cur_src_index, line))
			{
				// on a line with a breakpoint: tag it changed
				attrib = DCA_CHANGED;
			}

			if (m_cursor_visible && (line - 1) == m_cursor.y)
			{
				// We're on the cursored line and cursor is visible: highlight
				attrib |= DCA_SELECTED;
			}

			print_line(
				row,
				line,
				m_displayed_src_file->get_line_text(line),
				attrib);
		}
	}
}

bool debug_view_sourcecode::exists_bp_for_line(u32 src_index, u32 line)
{
	std::optional<debug_info_provider_base::address_range> addrs = 
		debug_info().file_line_to_address_range(m_cur_src_index, line);
	if (!addrs.has_value())
	{
		return false;
	}

	const device_debug * debug = source()->device()->debug();
	debug_info_provider_base::address_range addrs_val = addrs.value();
	for (offs_t addr = addrs_val.first; addr <= addrs_val.second; addr++)
	{
		if (debug->breakpoint_find(addr) != nullptr)
		{
			return true;
		}
	}
	return false;
}

// Center m_line_for_cur_pc vertically in view (with
// corner cases to account for file size and to minimize unnecessary
// movement).
void debug_view_sourcecode::update_visible_lines(offs_t pc)
{
	if (is_visible(m_line_for_cur_pc))
	{
		return;
	}

	if (m_displayed_src_file->num_lines() <= m_visible.y)
	{
		// Entire file fits in visible view.  Start at begining
		m_topleft.y = 0;
	}
	else if (m_line_for_cur_pc <= m_visible.y / 2)
	{
		// m_line_for_cur_pc close to top, start at top
		m_topleft.y = 0;
	}
	else if (m_line_for_cur_pc + m_visible.y / 2 > m_displayed_src_file->num_lines())
	{
		// m_line_for_cur_pc close to bottom, so bottom line at bottom
		m_topleft.y = m_displayed_src_file->num_lines() - 1 - m_visible.y;
	}
	else
	{
		// Main case, center m_line_for_cur_pc in view
		m_topleft.y = m_line_for_cur_pc - 1 - m_visible.y / 2;
	}
}

void debug_view_sourcecode::print_line(u32 row, std::optional<u32> line_number, const char * text, u8 attrib)
{
	const char LINE_NUMBER_PADDING[] = "     ";
	const s32 LINE_NUMBER_WIDTH = sizeof(LINE_NUMBER_PADDING)-1;

	// Left side shows line number (or space padding)
	std::string line_str = 	(line_number.has_value()) ? std::to_string(line_number.value()) : LINE_NUMBER_PADDING;
	for(s32 visible_col=m_topleft.x; visible_col < m_topleft.x + std::min(LINE_NUMBER_WIDTH, m_visible.x); visible_col++)
	{
		int viewdata_col = visible_col - m_topleft.x;
		m_viewdata[row * m_visible.x + viewdata_col] = 
		{
			(viewdata_col < line_str.size()) ? u8(line_str[viewdata_col]) : u8(' '),
			DCA_DISABLED
		};
	}

	// Right side shows line from file
	for(s32 visible_col=m_topleft.x + LINE_NUMBER_WIDTH; visible_col < m_topleft.x + m_visible.x; visible_col++)
	{
		s32 viewdata_col = visible_col - m_topleft.x;
		s32 text_idx = visible_col - LINE_NUMBER_WIDTH;

		if (text_idx >= strlen(text))
		{
			m_viewdata[row * m_visible.x + viewdata_col] = { ' ', attrib };
		}
		else
		{
			m_viewdata[row * m_visible.x + viewdata_col] = { u8(text[text_idx]), attrib };
		}
	}
}

void debug_view_sourcecode::set_src_index(u32 new_src_index)
{
	if (m_cur_src_index == new_src_index || 
		new_src_index >= m_debug_info.num_files())
	{
		return;
	}

	begin_update();
	m_cur_src_index = new_src_index;
	m_update_pending = true;
	// No need to call view_notify()
	end_update();
}




//-------------------------------------------------
//  view_click - handle a mouse click within the
//  current view
//-------------------------------------------------

void debug_view_sourcecode::view_click(const int button, const debug_view_xy& pos)
{
}