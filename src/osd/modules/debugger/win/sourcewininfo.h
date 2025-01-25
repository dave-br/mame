// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  sourcewininfo.h - Win32 debug window handling
//
//============================================================
#ifndef MAME_DEBUGGER_WIN_SOURCEWININFO_H
#define MAME_DEBUGGER_WIN_SOURCEWININFO_H

#pragma once

#include "disasmbasewininfo.h"
#include "debugwininfo.h"


namespace osd::debugger::win {

class sourcewin_info : public disasmbasewin_info
{
public:
	sourcewin_info(debugger_windows_interface &debugger);
	virtual ~sourcewin_info();

	// virtual bool handle_key(WPARAM wparam, LPARAM lparam) override;
	// virtual void restore_configuration_from_node(util::xml::data_node const &node) override;

protected:
	virtual void recompute_children() override;
	virtual void draw_contents(HDC dc) override;
	virtual bool handle_command(WPARAM wparam, LPARAM lparam) override;
	// virtual void update_menu() override;
	// virtual void save_configuration_to_node(util::xml::data_node &node) override;

private:
	virtual void process_string(const std::string &string) override {};

	HWND    m_combownd;
};

} // namespace osd::debugger::win

#endif // MAME_DEBUGGER_WIN_SOURCEWININFO_H
