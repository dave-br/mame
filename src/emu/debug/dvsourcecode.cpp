// license:BSD-3-Clause
// copyright-holders: TODO
/*********************************************************************

    dvsourcecode.cpp

    Source code debugger view.

***************************************************************************/

#include "emu.h"
#include "dvsourcecode.h"
#include "debugger.h"
#include "mdisimple.h"
#include "dbginfo_provider.h"
#include "emuopts.h"


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


//-------------------------------------------------
//  debug_view_sourcecode - constructor
//-------------------------------------------------

debug_view_sourcecode::debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate) :
	debug_view_disasm(machine, osdupdate, osdprivate, true /* source_code_debugging */),
	m_state(nullptr),
	m_debug_info(machine.debugger().debug_info()),
	m_cur_src_index(0),
	m_displayed_src_index(-1),
	m_displayed_src_file(std::make_unique<line_indexed_file>()),
	m_line_for_cur_pc()
	// m_first_visible_line(1)
{
	// device_t * live_cpu = machine.debugger().cpu().live_cpu();
	// live_cpu->interface(m_state);
	m_supports_cursor = true;
}


//-------------------------------------------------
//  ~debug_view_sourcecode - destructor
//-------------------------------------------------

debug_view_sourcecode::~debug_view_sourcecode()
{
}

//-------------------------------------------------
//  selected_address - return the PC of the
//  currently selected address in the view
//-------------------------------------------------

std::optional<offs_t> debug_view_sourcecode::selected_address()
{
	flush_updates();
	u32 line = m_cursor.y + 1;

	std::vector<debug_info_provider_base::address_range> ranges;
	m_debug_info.file_line_to_address_ranges(m_cur_src_index, line, ranges);

	if (ranges.size() == 0)
	{
		return std::optional<offs_t>();
	}

	return std::optional<offs_t>(ranges[0].first);
}


void debug_view_sourcecode::update_opened_file()
{
	if (m_cur_src_index == m_displayed_src_index)
	{
		return;
	}

	const char * local_path = debug_info().file_index_to_path(m_cur_src_index).local();
	if (local_path == nullptr)
	{
		return;
	}

	std::error_condition err = m_displayed_src_file->open(local_path);
	m_displayed_src_index = m_cur_src_index;
	if (err)
	{
		return;
	}

	m_total.y = m_displayed_src_file->num_lines();
}


void debug_view_sourcecode::set_source(const debug_view_source &source)
{
	source.device()->interface(m_state);
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
			m_cur_src_index = file_line.value().file_index();
			m_line_for_cur_pc = file_line.value().line_number();
		}
	}

	update_opened_file();

	// Ensure correct set of lines to print

	// TODO: MOVE THIS AFTER ERROR PRINT?
	if (pc_changed)
	{
		update_visible_lines(pc);
	}

	// Print
	// const char * local_path = debug_info().file_index_to_path(m_cur_src_index).local();
	const debug_info_provider_base::source_file_path & path = m_debug_info.file_index_to_path(m_cur_src_index);
	if (path.local() == nullptr || m_displayed_src_file->last_open_error())
	{
		print_line(0, "Error opening file", DCA_NORMAL);
		if (path.local() == nullptr)
		{
			print_line(1, "Could not find local file matching originally built source", DCA_NORMAL);
		}
		else
		{
			print_line(1, path.local(), DCA_NORMAL);
			print_line(2, m_displayed_src_file->last_open_error().message().c_str(), DCA_NORMAL);
		}
		std::string s("Originally built source: ");
		s += path.built();
		print_line(3, s.c_str(), DCA_NORMAL);
		s = "Source search path (";
		s += OPTION_DEBUGSRCPATH;
		s += "): ";
		s += machine().options().debug_source_path();
		print_line(4, s.c_str(), DCA_NORMAL);
		s = "Source path prefix map (";
		s += OPTION_DEBUGSRCPATHMAP;
		s += "): ";
		s += machine().options().debug_source_path_map();
		print_line(5, s.c_str(), DCA_NORMAL);
		for (u32 row = 6; row < m_visible.y; row++)
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

			if (m_line_for_cur_pc.has_value() && line == m_line_for_cur_pc.value())
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

bool debug_view_sourcecode::exists_bp_for_line(u16 src_index, u32 line)
{
	std::vector<debug_info_provider_base::address_range> ranges;
	m_debug_info.file_line_to_address_ranges(m_cur_src_index, line, ranges);
	const device_debug * debug = source()->device()->debug();
	for (offs_t i = 0; i < ranges.size(); i++)
	{
		for (offs_t addr = ranges[i].first; addr <= ranges[i].second; addr++)
		{
			if (debug->breakpoint_find(addr) != nullptr)
			{
				return true;
			}
		}
	}
	return false;
}

// Center m_line_for_cur_pc vertically in view (with
// corner cases to account for file size and to minimize unnecessary
// movement).
void debug_view_sourcecode::update_visible_lines(offs_t pc)
{
	if (!m_line_for_cur_pc.has_value() || is_visible(m_line_for_cur_pc.value()))
	{
		return;
	}

	u32 line_for_cur_pc = m_line_for_cur_pc.value();

	if (m_displayed_src_file->num_lines() <= m_visible.y)
	{
		// Entire file fits in visible view.  Start at begining
		m_topleft.y = 0;
	}
	else if (line_for_cur_pc <= m_visible.y / 2)
	{
		// line_for_cur_pc close to top, start at top
		m_topleft.y = 0;
	}
	else if (line_for_cur_pc + m_visible.y / 2 > m_displayed_src_file->num_lines())
	{
		// line_for_cur_pc close to bottom, so bottom line at bottom
		m_topleft.y = m_displayed_src_file->num_lines() - 1 - m_visible.y;
	}
	else
	{
		// Main case, center line_for_cur_pc in view
		m_topleft.y = line_for_cur_pc - 1 - m_visible.y / 2;
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
		s32 viewdata_col = visible_col - m_topleft.x;
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


void debug_view_sourcecode::set_src_index(u16 new_src_index)
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

// void debug_view_sourcecode::view_click(const int button, const debug_view_xy& pos)
// {
// }