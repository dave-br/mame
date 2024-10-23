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
		// TODO: Or, do something like...
		// return std::make_unique<debug_info_empty>(machine);
	}

	// TODO: before full-blown reading, something should just check magic & type, and then
	// defer to srcdbg_provider_simple, etc.
	std::unique_ptr<srcdbg_provider_simple> ret = std::make_unique<srcdbg_provider_simple>(machine);
	srcdbg_import importer(*ret);
	std::string error;

	// TODO: ERROR HANDLING
	if (!srcdbg_format_read(di_path, importer, error))
	{
		if (!error.empty())
		{
		}
		return nullptr;
	}

	return ret;
}

