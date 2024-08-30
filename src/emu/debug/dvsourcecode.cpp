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


// static
std::unique_ptr<debug_info_provider_base> debug_info_provider_base::create_debug_info(running_machine &machine)
{
	const char* di_path = machine.options().debug_info();
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
}


//-------------------------------------------------
//  debug_view_sourcecode - constructor
//-------------------------------------------------

debug_view_sourcecode::debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate) :
	// debug_view(machine, DVT_SOURCE, osdupdate, osdprivate),
	debug_view_disasm(machine, osdupdate, osdprivate, DVT_SOURCE),
	m_debug_info(machine.debugger().debug_info())
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
	std::string textE = "I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     I am the ZA.  abcdefghijklmnopqrstuvwxyz 0123456789     ";
	std::string textO = " ";
	for(u32 row = 0; row < m_visible.y; row++)
	{
		if (row % 2 == 0)
			print_line(row, textE);
		else
			print_line(row, textO);
	}

}

void debug_view_sourcecode::print_line(u32 row, std::string text)
{
	for(s32 visible_col=m_topleft.x; visible_col < m_topleft.x + m_visible.x; visible_col++)
	{
		int viewdata_col = visible_col - m_topleft.x;
		if (visible_col >= int(text.size()))
			m_viewdata[row * m_visible.x + viewdata_col] = { ' ', DCA_NORMAL };
		else
			m_viewdata[row * m_visible.x + viewdata_col] = { u8(text[visible_col]), DCA_NORMAL };
	}
}



//-------------------------------------------------
//  view_click - handle a mouse click within the
//  current view
//-------------------------------------------------

void debug_view_sourcecode::view_click(const int button, const debug_view_xy& pos)
{
}