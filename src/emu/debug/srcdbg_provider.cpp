// license:BSD-3-Clause
// copyright-holders:David Broman
/*********************************************************************

    srcdbg_provider.cpp

    Factory to create format-specific implementation of
	format-agnostic interface to source-debugging info

***************************************************************************/


#include "emu.h"
#include "srcdbg_provider_simple.h"		// TODO: BREAK UP BETTER SO THIS IS UNNECESSARY
#include "srcdbg_provider.h"

#include "emuopts.h"


// static
std::unique_ptr<srcdbg_provider_base> srcdbg_provider_base::create_debug_info(running_machine &machine)
{
	const char * di_path = machine.options().debug_info();
	if (di_path[0] == 0)
	{
		return nullptr;
	}

	std::string error;
	srcdbg_format format;
	if (!srcdbg_format_header_read(di_path, format, error))
	{
		throw emu_fatalerror("Error reading source-level debugging information file\n%s\n\n%s", di_path, error.c_str());
	}

	switch (format)
	{
	case SRCDBG_FORMAT_SIMPLE:
	{
		std::unique_ptr<srcdbg_provider_simple> ret = std::make_unique<srcdbg_provider_simple>(machine);
		srcdbg_import importer(*ret);
		if (!srcdbg_format_simp_read(di_path, importer, error))
		{
			if (!error.empty())
			{
				throw emu_fatalerror("Error reading source-level debugging information file\n%s\n\n%s", di_path, error.c_str());
			}
			return nullptr;
		}
		return ret;
	}
	default:
		assert(!"Unexpected source-level debugging information file format");
		return nullptr;
	}

	assert (!"This code should be unreachable");
	return nullptr;
}

