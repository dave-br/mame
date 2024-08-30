// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  sourcewininfo.h - Win32 debug window handling
//
//============================================================
#ifndef MAME_DEBUGGER_WIN_SOURCEVIEWINFO_H
#define MAME_DEBUGGER_WIN_SOURCEVIEWINFO_H

#pragma once

#include "disasmviewinfo.h"


namespace osd::debugger::win {

class sourceview_info : public disasmview_info
{
public:
	sourceview_info(debugger_windows_interface &debugger, debugwin_info &owner, HWND parent);
	virtual ~sourceview_info() {}
};

} // namespace osd::debugger::win

#endif // MAME_DEBUGGER_WIN_SOURCEVIEWINFO_H
