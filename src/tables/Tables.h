#pragma once

#include "tables/HookTargets.h"
#include "tables/StoreSites.h"

#include <cstdint>
#include <span>

namespace swf::inline SWF_NAMESPACE::tables
{
	/// One runtime's address tables. A value the runtime's table does not have is 0 (an empty span for
	/// a store table): the family using it does not verify, and its SWF_CAP_* bit stays clear.
	struct Runtime
	{
		const char* name;
		const HookTargets* targets;
		std::span<const HiddenStoreSite> hiddenStores;
		std::span<const std::uint32_t> showSites;
		std::span<const StoreValueSite> switchStores;
		std::span<const StoreValueSite> fadeResetStores;
	};

	/// Selects the tables for one SWF_RUNTIME_* bit. False if this copy has none for it.
	bool Select(std::uint64_t runtime);

	/// The selected runtime's tables (Select must have succeeded).
	const Runtime& Active();
}
