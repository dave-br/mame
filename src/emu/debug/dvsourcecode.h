// license:BSD-3-Clause
// copyright-holders:Andrew Gardner, Vas Crabb
/*********************************************************************

    dvsourcecode.h

    Source code debugger view.

***************************************************************************/
#ifndef MAME_EMU_DEBUG_DVSOURCE_H
#define MAME_EMU_DEBUG_DVSOURCE_H

#pragma once

#include "emu.h"
//#include "debugcpu.h"
#include "debugvw.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// abstract base class for debug-info (symbols) file readers
class debug_info_provider_base
{
public:
	virtual ~debug_info_provider_base() {};
	virtual u16 file_line_to_address(const char * file_path, int line_number) = 0;
	
};

// debug-info provider for the simple format
// TODO: NAME?  MOVE TO ANOTHER FILE?
class debug_info_simple : public debug_info_provider_base
{
public:
	debug_info_simple(running_machine & machine) {}
	~debug_info_simple() { }
	virtual u16 file_line_to_address(const char * file_path, int line_number) override { return 44; };
};

// debug view for source-level debugging
class debug_view_sourcecode : public debug_view
{
	friend class debug_view_manager;

protected:
	// view overrides
	virtual void view_update() override;
	virtual void view_click(const int button, const debug_view_xy& pos) override;

private:
	// construction/destruction
	debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate);
	virtual ~debug_view_sourcecode();
	void print_line(u32 row, std::string text);
	const debug_info_provider_base & 		m_debug_info;		// Interface to the loaded debugging info file

// 	// internal helpers
// 	void enumerate_sources();
// 	void pad_ostream_to_length(std::ostream& str, int len);
// 	void gather_watchpoints();


// 	// internal state
// 	bool (*m_sortType)(const debug_watchpoint *, const debug_watchpoint *);
// 	std::vector<debug_watchpoint *> m_buffer;
};

#endif // MAME_EMU_DEBUG_DVSOURCE_H
