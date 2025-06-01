// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider_aggregator.h

    TODO

***************************************************************************/


#ifndef MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H
#define MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H

#pragma once

#include "srcdbg_provider.h"

class srcdbg_provider_aggregator : public srcdbg_provider_base
{
public:
	srcdbg_provider_aggregator(const running_machine& machine);
	~srcdbg_provider_aggregator() { }

	// robin all, change params so caller creates the tables,
	// and callees just populate them
	virtual void get_srcdbg_symbols(
		symbol_table ** symtable_srcdbg_globals,
		symbol_table ** symtable_srcdbg_locals,
		symbol_table * parent,
		device_t * device,
		const device_state_interface * state) const;

	// robin all
	virtual void complete_local_relative_initialization() override;

	// Sum
	virtual u32 num_files() const override { return m_source_file_paths.size(); }

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
	virtual const source_file_path & file_index_to_path(u32 file_index) const override { return m_source_file_paths[file_index]; };

	// robin to first successful
	virtual std::optional<u32> file_path_to_index(const char * file_path) const override;

	// use num_files successively to determine which provider owns
	// this, use remainder on that provider
	virtual void file_line_to_address_ranges(u32 file_index, u32 line_number, std::vector<address_range> & ranges) const override;

	// robin to first successful
	virtual bool address_to_file_line (offs_t address, file_line & loc) const override;

	// own offset and remove from base class
	virtual void set_offset(s32 offset) override { m_offset = offset; }
	// virtual s32 get_offset() const override { return m_offset; }

private:
	std::vector<srcdbg_provider_base> m_providers;
};


#endif // MAME_EMU_DEBUG_SRCDBG_PROVIDER_AGGREGATOR_H
