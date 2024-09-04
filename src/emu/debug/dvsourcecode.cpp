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

	const char * source_file_paths[] = { "D:\\coco\\asm\\moon\\Final\\mpE.asm", "test" };
	mame_debug_info_simple_header hdr;
	strncpy(&hdr.magic[0], "MDbI", 4);
	strncpy(&hdr.type[0], "simp", 4);
	hdr.version = 1;
	hdr.source_file_paths_size = 0;
	for (int i=0; i < _countof(source_file_paths); i++)
	{
		hdr.source_file_paths_size += strlen(source_file_paths[i]) + 1;
	}
	hdr.num_line_mappings = 0;
	fwrite(&hdr, sizeof(mame_debug_info_simple_header), 1, testfptr);
	for (int i=0; i < _countof(source_file_paths); i++)
	{
		fwrite(source_file_paths[i], strlen(source_file_paths[i])+1, 1, testfptr);
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

debug_info_simple::debug_info_simple(running_machine & machine, std::vector<uint8_t> & data) :
	debug_info_provider_base(),
	source_file_paths()
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
	// Ensuring this now makes it safe to read the strings later without going past the end.
	if (data[first_line_mapping - 1] != 0)
	{
		// TODO ERROR
		assert(false);		
	}

	// Read the starting char address for each source file path into vector
	source_file_paths.push_back((const char*) &data[i++]);     // First string starts immediately
	for (; i < first_line_mapping - 1; i++)                    // Exclude first_line_mapping - 1; it is final null-terminator
	{
		if (data[i-1] == 0)
		{
			// i points to character immediately following null terminator, so
			// i begins a new string
			source_file_paths.push_back((const char*) &data[i]);
		}
	}
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