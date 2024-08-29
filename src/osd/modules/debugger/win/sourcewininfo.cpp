// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  sourcewininfo.cpp - Win32 debug window handling
//
//============================================================

#include "emu.h"
#include "sourcewininfo.h"

#include "debugviewinfo.h"

namespace osd::debugger::win {

sourcewin_info::sourcewin_info(debugger_windows_interface &debugger) :
	disasmbasewin_info(debugger, false, std::string("Source").c_str(), nullptr)
{
	if (!window())
		return;

	// TODO: WILL NEED THIS
	// m_views[0].reset(new debugview_info(debugger, *this, window(), DVT_SOURCE));
	// if ((m_views[0] == nullptr) || !m_views[0]->is_valid())
	// {
	// 	m_views[0].reset();
	// 	return;
	// }

	// create the options menu
	// HMENU const optionsmenu = CreatePopupMenu();
	// AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_BREAKPOINTS, TEXT("Breakpoints\tCtrl+1"));
	// AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_WATCHPOINTS, TEXT("Watchpoints\tCtrl+2"));
	// AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_REGISTERPOINTS, TEXT("Registerpoints\tCtrl+3"));
	// AppendMenu(GetMenu(window()), MF_ENABLED | MF_POPUP, (UINT_PTR)optionsmenu, TEXT("Options"));

	// compute a client rect
	RECT bounds;
	bounds.top = bounds.left = 0;
	bounds.right = m_views[0]->maxwidth() + (2 * EDGE_WIDTH);
	bounds.bottom = 200;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);

	// clamp the min/max size
	set_maxwidth(bounds.right - bounds.left);

	// position the window and recompute children
	debugger.stagger_window(window(), bounds.right - bounds.left, bounds.bottom - bounds.top);
	recompute_children();
}


sourcewin_info::~sourcewin_info()
{
}


// bool sourcewin_info::handle_key(WPARAM wparam, LPARAM lparam)
// {
// 	switch (wparam)
// 	{
// 	// ajg - steals the F4 from the global key handler - but ALT+F4 didn't work anyways ;)
// 	case VK_F4:
// 		SendMessage(window(), WM_COMMAND, ID_RUN_TO_CURSOR, 0);
// 		return true;

// 	case VK_F9:
// 		if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
// 			SendMessage(window(), WM_COMMAND, ID_DISABLE_BREAKPOINT, 0);
// 		else
// 			SendMessage(window(), WM_COMMAND, ID_TOGGLE_BREAKPOINT, 0);
// 		return true;

// 	case VK_RETURN:
// 		if (m_views[0]->cursor_visible() && m_views[0]->source_is_visible_cpu())
// 		{
// 			SendMessage(window(), WM_COMMAND, ID_STEP, 0);
// 			return true;
// 		}
// 		break;
// 	}

// 	return debugwin_info::handle_key(wparam, lparam);
// }

// bool sourcewin_info::handle_command(WPARAM wparam, LPARAM lparam)
// {
// 	// auto *const dasmview = downcast<disasmview_info *>(m_views[0].get());

// 	switch (HIWORD(wparam))
// 	{
// 	// menu selections
// 	case 0:
// 		switch (LOWORD(wparam))
// 		{
// 		case ID_TOGGLE_BREAKPOINT:
// 			if (dasmview->cursor_visible())
// 			{
// 				offs_t const address = dasmview->selected_address();
// 				device_debug *const debug = dasmview->source_device()->debug();

// 				// first find an existing breakpoint at this address
// 				const debug_breakpoint *bp = debug->breakpoint_find(address);

// 				// if it doesn't exist, add a new one
// 				if (!is_main_console())
// 				{
// 					if (bp == nullptr)
// 					{
// 						int32_t bpindex = debug->breakpoint_set(address);
// 						machine().debugger().console().printf("Breakpoint %X set\n", bpindex);
// 					}
// 					else
// 					{
// 						int32_t bpindex = bp->index();
// 						debug->breakpoint_clear(bpindex);
// 						machine().debugger().console().printf("Breakpoint %X cleared\n", bpindex);
// 					}
// 					machine().debug_view().update_all();
// 					machine().debugger().refresh_display();
// 				}
// 				else if (dasmview->source_is_visible_cpu())
// 				{
// 					std::string command;
// 					if (bp == nullptr)
// 						command = string_format("bpset 0x%X", address);
// 					else
// 						command = string_format("bpclear 0x%X", bp->index());
// 					machine().debugger().console().execute_command(command, true);
// 				}
// 			}
// 			return true;

// 		case ID_DISABLE_BREAKPOINT:
// 			if (dasmview->cursor_visible())
// 			{
// 				offs_t const address = dasmview->selected_address();
// 				device_debug *const debug = dasmview->source_device()->debug();

// 				// first find an existing breakpoint at this address
// 				const debug_breakpoint *bp = debug->breakpoint_find(address);

// 				// if it doesn't exist, add a new one
// 				if (bp != nullptr)
// 				{
// 					if (!is_main_console())
// 					{
// 						debug->breakpoint_enable(bp->index(), !bp->enabled());
// 						machine().debugger().console().printf("Breakpoint %X %s\n", (uint32_t)bp->index(), bp->enabled() ? "enabled" : "disabled");
// 						machine().debug_view().update_all();
// 						machine().debugger().refresh_display();
// 					}
// 					else if (dasmview->source_is_visible_cpu())
// 					{
// 						std::string command;
// 						command = string_format(bp->enabled() ? "bpdisable 0x%X" : "bpenable 0x%X", (uint32_t)bp->index());
// 						machine().debugger().console().execute_command(command, true);
// 					}
// 				}
// 			}
// 			return true;

// 		case ID_RUN_TO_CURSOR:
// 			if (dasmview->cursor_visible())
// 			{
// 				offs_t const address = dasmview->selected_address();
// 				if (dasmview->source_is_visible_cpu())
// 				{
// 					std::string command;
// 					command = string_format("go 0x%X", address);
// 					machine().debugger().console().execute_command(command, true);
// 				}
// 				else
// 				{
// 					dasmview->source_device()->debug()->go(address);
// 				}
// 			}
// 			return true;

// 		case ID_SHOW_RAW:
// 			dasmview->set_right_column(DASM_RIGHTCOL_RAW);
// 			recompute_children();
// 			return true;

// 		case ID_SHOW_ENCRYPTED:
// 			dasmview->set_right_column(DASM_RIGHTCOL_ENCRYPTED);
// 			recompute_children();
// 			return true;

// 		case ID_SHOW_COMMENTS:
// 			dasmview->set_right_column(DASM_RIGHTCOL_COMMENTS);
// 			recompute_children();
// 			return true;
// 		}
// 		break;
// 	}
// 	return editwin_info::handle_command(wparam, lparam);
// }


// void sourcewin_info::update_menu()
// {
// 	debugwin_info::update_menu();

// 	HMENU const menu = GetMenu(window());
// 	CheckMenuItem(menu, ID_SHOW_BREAKPOINTS, MF_BYCOMMAND | (m_views[0]->type() == DVT_BREAK_POINTS ? MF_CHECKED : MF_UNCHECKED));
// 	CheckMenuItem(menu, ID_SHOW_WATCHPOINTS, MF_BYCOMMAND | (m_views[0]->type() == DVT_WATCH_POINTS ? MF_CHECKED : MF_UNCHECKED));
// 	CheckMenuItem(menu, ID_SHOW_REGISTERPOINTS, MF_BYCOMMAND | (m_views[0]->type() == DVT_REGISTER_POINTS ? MF_CHECKED : MF_UNCHECKED));
// }


// bool sourcewin_info::handle_command(WPARAM wparam, LPARAM lparam)
// {
// 	switch (HIWORD(wparam))
// 	{
// 	// menu selections
// 	case 0:
// 		switch (LOWORD(wparam))
// 		{
// 		case ID_SHOW_BREAKPOINTS:
// 			m_views[0].reset();
// 			m_views[0].reset(new debugview_info(debugger(), *this, window(), DVT_BREAK_POINTS));
// 			if (!m_views[0]->is_valid())
// 				m_views[0].reset();
// 			win_set_window_text_utf8(window(), "All Breakpoints");
// 			recompute_children();
// 			return true;

// 		case ID_SHOW_WATCHPOINTS:
// 			m_views[0].reset();
// 			m_views[0].reset(new debugview_info(debugger(), *this, window(), DVT_WATCH_POINTS));
// 			if (!m_views[0]->is_valid())
// 				m_views[0].reset();
// 			win_set_window_text_utf8(window(), "All Watchpoints");
// 			recompute_children();
// 			return true;

// 		case ID_SHOW_REGISTERPOINTS:
// 			m_views[0].reset();
// 			m_views[0].reset(new debugview_info(debugger(), *this, window(), DVT_REGISTER_POINTS));
// 			if (!m_views[0]->is_valid())
// 				m_views[0].reset();
// 			win_set_window_text_utf8(window(), "All Registerpoints");
// 			recompute_children();
// 			return true;
// 		}
// 		break;
// 	}
// 	return debugwin_info::handle_command(wparam, lparam);
// }


// void sourcewin_info::restore_configuration_from_node(util::xml::data_node const &node)
// {
// 	switch (node.get_attribute_int(ATTR_WINDOW_POINTS_TYPE, -1))
// 	{
// 	case 0:
// 		SendMessage(window(), WM_COMMAND, ID_SHOW_BREAKPOINTS, 0);
// 		break;
// 	case 1:
// 		SendMessage(window(), WM_COMMAND, ID_SHOW_WATCHPOINTS, 0);
// 		break;
// 	case 2:
// 		SendMessage(window(), WM_COMMAND, ID_SHOW_REGISTERPOINTS, 0);
// 		break;
// 	}

// 	debugwin_info::restore_configuration_from_node(node);
// }


// void sourcewin_info::save_configuration_to_node(util::xml::data_node &node)
// {
// 	debugwin_info::save_configuration_to_node(node);

// 	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_POINTS_VIEWER);
// 	switch (m_views[0]->type())
// 	{
// 	case DVT_BREAK_POINTS:
// 		node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 0);
// 		break;
// 	case DVT_WATCH_POINTS:
// 		node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 1);
// 		break;
// 	case DVT_REGISTER_POINTS:
// 		node.set_attribute_int(ATTR_WINDOW_POINTS_TYPE, 2);
// 		break;
// 	default:
// 		break;
// 	}
// }

} // namespace osd::debugger::win
