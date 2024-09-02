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
	m_data[m_data.size()] = '\0';
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

	// FUTURE: Insert code here that validates di_path is a path to a MAME
	// debug info file, reads the header, and instantiates the corresponding
	// debug_info_* class to read it.  For now, only debug_info_simple
	// is supported

	return std::make_unique<debug_info_simple>(machine, di_path);

}

debug_info_simple::debug_info_simple(running_machine & machine, const char * di_path)
{
	// TODO	
	// util::core_file (verify it uses osd winfile and sets share mode to read)
}


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