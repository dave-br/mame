// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  sourceviewinfo.cpp - Win32 debug window handling
//
//============================================================

#include "emu.h"
#include "sourceviewinfo.h"

namespace osd::debugger::win {

sourceview_info::sourceview_info(debugger_windows_interface &debugger, debugwin_info &owner, HWND parent) :
	disasmview_info(debugger, owner, parent, DVT_SOURCE)
{
	
}


} // namespace osd::debugger::win
