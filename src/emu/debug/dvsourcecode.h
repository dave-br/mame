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
#include "dvdisasm.h"

#include "dbginfo/mdisimple.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class line_indexed_file
{
public:
	line_indexed_file();
	~line_indexed_file() { };
	const std::error_condition & open(const char * file_path);
	const std::error_condition & last_open_error() { return m_err; };
	u32 num_lines() { return m_line_starts.size(); };
	const char * get_line_text(u32 n) { return (const char *) &m_data[m_line_starts[n-1]]; };

private:
	std::error_condition m_err;
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
	// TODO: MAKE FRIEND CLASS, AND ONLY GETTERS PUBLIC
	class source_file_path
	{
	public:
		source_file_path(std::string & built_p, std::string & local_p) :
			m_built(std::move(built_p)),
			m_local(std::move(local_p))
		{
		}

		// TODO: VERIFY THIS IS CALLED WHEN SFP IS PLACED IN VECTOR
		// source_file_path(source_file_path && sfp) :
		// 	m_built(sfp.m_built),
		// 	m_local(sfp.m_local)
		// {
		// }
		
		const char * built() const { return m_built.c_str(); }
		const char * local() const { return m_local.c_str(); }  // TODO: WORKS IF LOCAL EMPTY?

	private:
		std::string m_built;
		std::string m_local;
	};

	class symbol
	{
	public:
		const char * name();
		offs_t value();
	};

	typedef std::pair<offs_t,offs_t> address_range;
	static std::unique_ptr<debug_info_provider_base> create_debug_info(running_machine &machine);
	virtual ~debug_info_provider_base() {};
	virtual std::size_t num_files() const = 0;
	virtual const source_file_path & file_index_to_path(u16 file_index) const = 0;
	virtual std::optional<int> file_path_to_index(const char * file_path) const = 0;
	virtual std::optional<address_range> file_line_to_address_range (u16 file_index, u32 line_number) const = 0;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const = 0;
	virtual const std::vector<symbol> & global_symbols() const = 0;
};

// debug-info provider for the simple format
// TODO: NAME?  MOVE TO ANOTHER FILE?

class debug_info_simple : public debug_info_provider_base
{
public:
	debug_info_simple(running_machine& machine, std::vector<uint8_t>& data);
	~debug_info_simple() { }
	virtual std::size_t num_files() const override { return m_source_file_paths.size(); }
	virtual const source_file_path & file_index_to_path(u16 file_index) const override { return m_source_file_paths[file_index]; };
	virtual std::optional<int> file_path_to_index(const char * file_path) const override;
	virtual std::optional<address_range> file_line_to_address_range (u16 file_index, u32 line_number) const override;
	virtual std::optional<file_line> address_to_file_line (offs_t address) const override;

private:
	struct address_line
	{
		u16 address_first;
		u16 address_last;
		u32 line_number;
	};

	void generate_local_path(running_machine& machine, const std::string & built, std::string & local);
	void apply_source_map(running_machine& machine, std::string & local);

	// std::vector<char>                        m_source_file_path_chars; // Storage for source file path characters
	std::vector<source_file_path>            m_source_file_paths;      // Starting points for source file path strings
	std::vector<mdi_line_mapping>            m_linemaps_by_address;    // a list of mdi_line_mappings, sorted by address
	std::vector<std::vector<address_line>>   m_linemaps_by_line;       // m_linemaps_by_line[i] is a list of address/line pairs,
	                                                                   // sorted by line, from file #i
};

// debug view for source-level debugging
class debug_view_sourcecode : public debug_view_disasm
{
	friend class debug_view_manager;

public:
	// getters
	const debug_info_provider_base & debug_info() const { return m_debug_info; }
	u16 cur_src_index() const { return m_cur_src_index; }
	virtual std::optional<offs_t> selected_address() override;

	// setters
	void set_src_index(u16 new_src_index);

protected:
	// construction/destruction
	debug_view_sourcecode(running_machine &machine, debug_view_osd_update_func osdupdate, void *osdprivate);
	virtual ~debug_view_sourcecode();

	// view overrides
	virtual void set_source(const debug_view_source &source) override;
	virtual void view_update() override;
	// virtual void view_click(const int button, const debug_view_xy& pos) override;

private:
	void print_line(u32 row, const char * text, u8 attrib) { print_line( row, std::optional<u32>(), text, attrib); };
	void print_line(u32 row, std::optional<u32> line_number, const char * text, u8 attrib);

	u32 first_visible_line() { return m_topleft.y + 1; }
	bool is_visible(u32 line) { return (first_visible_line() <= line && line < first_visible_line() + m_visible.y); }
	void update_opened_file();
	void update_visible_lines(offs_t pc);
	bool exists_bp_for_line(u16 src_index, u32 line);

	device_state_interface *                          m_state;                 // state interface, if present
	const debug_info_provider_base &                  m_debug_info;            // Interface to the loaded debugging info file
	u16                                               m_cur_src_index;         // Identifies which source file we should now show / switch to
	u16                                               m_displayed_src_index;   // Identifies which source file is currently shown
	std::unique_ptr<line_indexed_file>                m_displayed_src_file;    // File object currently printed to the view
	std::vector<std::pair<std::string, std::string>>  m_src_path_map;
	u32                                               m_line_for_cur_pc;       // Line number to be highlighted
	// u32                                 m_first_visible_line;    // Line number to show at top of scrolled view
};

#endif // MAME_EMU_DEBUG_DVSOURCE_H
