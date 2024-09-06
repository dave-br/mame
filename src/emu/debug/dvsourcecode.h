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
#include "dvdisasm.h"
// #include "mdisimple.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class line_indexed_file
{
public:
	line_indexed_file();
	~line_indexed_file() { };
	std::error_condition open(const char * file_path);
	u32 num_lines() { return m_line_starts.size(); };
	const char * get_line_text(u32 n) { return (const char *) &m_data[m_line_starts[n-1]]; };

private:
	std::vector<uint8_t> m_data;
	std::vector<u32> m_line_starts;
};

struct file_line
{
	u16 file_index;
	u32 line_number;
};

// abstract base class for debug-info (symbols) file readers
class debug_info_provider_base
{
public:
	static std::unique_ptr<debug_info_provider_base> create_debug_info(running_machine &machine);
	virtual ~debug_info_provider_base() {};
	virtual const char * file_index_to_path(int file_index) const = 0;
	virtual std::optional<u16> file_line_to_address (u16 file_index, u32 line_number) const = 0;
	virtual std::optional<file_line> address_to_file_line (u16 address) const = 0;

// protected:
// 	std::vector<uint8_t> m_data;
};

// debug-info provider for the simple format
// TODO: NAME?  MOVE TO ANOTHER FILE?
class debug_info_simple : public debug_info_provider_base
{
public:
	debug_info_simple(running_machine& machine, std::vector<uint8_t>& data);
	~debug_info_simple() { }
	virtual const char * file_index_to_path(int file_index) const override { return m_source_file_paths[file_index]; };
	virtual std::optional<u16> file_line_to_address (u16 file_index, u32 line_number) const override;
	virtual std::optional<file_line> address_to_file_line (u16 address) const override;

private:
	std::vector<char> m_source_file_path_chars;       // Storage for source file path characters
	std::vector<const char*> m_source_file_paths;     // Starting points for source file path strings
	std::unordered_map<file_line, u16> m_file_line_to_address;
	std::unordered_map<u16, file_line> m_address_to_file_line;
};

// debug view for source-level debugging
class debug_view_sourcecode : public debug_view_disasm
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

	void print_line(u32 row, const char * text, u8 attrib);
	bool is_visible(u32 line) { return (m_first_visible_line <= line && line < m_first_visible_line + m_visible.y); }
	void adjust_visible_lines();

	const debug_info_provider_base &    m_debug_info;		     // Interface to the loaded debugging info file
	u32                                 m_cur_src_index;         // Identifies which source file we show show now
	u32                                 m_displayed_src_index;   // Identifies which source file is currently shown
	std::unique_ptr<line_indexed_file>  m_displayed_src_file;    // File object currently printed to the view
	u32                                 m_highlighted_line;      // Line number to be highlighted
	u32                                 m_first_visible_line;    // Line number to show at top of scrolled view

// 	// internal helpers
// 	void enumerate_sources();
// 	void pad_ostream_to_length(std::ostream& str, int len);
// 	void gather_watchpoints();


// 	// internal state
// 	bool (*m_sortType)(const debug_watchpoint *, const debug_watchpoint *);
// 	std::vector<debug_watchpoint *> m_buffer;
};

#endif // MAME_EMU_DEBUG_DVSOURCE_H
