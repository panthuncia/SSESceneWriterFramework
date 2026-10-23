#pragma once

#include "core/Version.h"

#include <span>

namespace swf::inline SWF_NAMESPACE
{
	/// One group of engine hooks that installs as a unit and reports one capability bit.
	struct Family
	{
		const char* name;
		SWF_Caps cap;
		bool required;          // activation fails if this family cannot install
		bool (*verify)();       // checks every site; writes nothing
		bool (*install)();      // called only after verify() succeeded
	};

	/// Every hook family, in install order.
	std::span<const Family> Families();

	/// The capability an event or filter id needs: 0 for none, ~0 for an unknown id.
	SWF_Caps CapFor(uint32_t id, bool filter);
}
