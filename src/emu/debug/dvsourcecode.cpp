// license:BSD-3-Clause
// copyright-holders: TODO
/*********************************************************************

    dvsourcecode.cpp

    Source code debugger view.

***************************************************************************/

#include "emu.h"
#include "dvsourcecode.h"


//-------------------------------------------------
//  debug_view_sourcecode - constructor
//-------------------------------------------------

debug_view_sourcecode::debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate)
	: debug_view(machine, DVT_SOURCE, osdupdate, osdprivate)
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