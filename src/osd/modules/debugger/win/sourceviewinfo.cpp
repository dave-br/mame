// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  sourceviewinfo.cpp - Win32 debug window handling
//
//============================================================

#include "emu.h"
#include "sourceviewinfo.h"
#include "uimetrics.h"

#include "debug/dvsourcecode.h"

#include "strconv.h"

#include "winutil.h"

namespace osd::debugger::win {

// combo box styles
#define COMBO_BOX_STYLE     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL
#define COMBO_BOX_STYLE_EX  0


sourceview_info::sourceview_info(debugger_windows_interface &debugger, debugwin_info &owner, HWND parent) :
	disasmview_info(debugger, owner, parent, DVT_SOURCE)
{
	
}



HWND sourceview_info::create_source_file_combobox(HWND parent, LONG_PTR userdata)
{
	// create a combo box
	HWND const result = CreateWindowEx(COMBO_BOX_STYLE_EX, TEXT("COMBOBOX"), nullptr, COMBO_BOX_STYLE,
			0, 0, 100, 1000, parent, nullptr, GetModuleHandleUni(), nullptr);
	SetWindowLongPtr(result, GWLP_USERDATA, userdata);
	SendMessage(result, WM_SETFONT, (WPARAM)metrics().debug_font(), (LPARAM)FALSE);

	// populate the combobox with source file paths
	const debug_view_sourcecode * dv_source = view<debug_view_sourcecode>();
	const debug_info_provider_base & debug_info = dv_source->debug_info();
	std::size_t num_files = debug_info.num_files();


	size_t maxlength = 0;
	for (std::size_t i = 0; i < num_files; i++)
	{
		size_t const length = strlen(debug_info.file_index_to_path(i));
		if (length > maxlength)
		{
			maxlength = length;
		}
		auto t_name = osd::text::to_tstring(debug_info.file_index_to_path(i));
		SendMessage(result, CB_ADDSTRING, 0, (LPARAM) t_name.c_str());
	}

	SendMessage(result, CB_SETCURSEL, dv_source->cur_src_index(), 0);
	SendMessage(result, CB_SETDROPPEDWIDTH, ((maxlength + 2) * metrics().debug_font_width()) + metrics().vscroll_width(), 0);

	return result;
}


} // namespace osd::debugger::win
