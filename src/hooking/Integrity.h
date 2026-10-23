#pragma once

#include "core/Version.h"

#include <cstddef>
#include <cstdint>

namespace swf::inline SWF_NAMESPACE::hooking
{
	/// The family whose patches the next Record calls belong to (a static name; null for none).
	void SetFamily(const char* name);

	/// Snapshots the `length` bytes SWF just wrote at `address` (a detour's jump, a call site, a
	/// vtable slot), replacing any earlier record of the same address.
	void Record(std::uint32_t kind, std::uintptr_t address, std::size_t length);

	/// Re-reads every recorded patch; for each that changed, follows the new bytes to the code they
	/// lead to and reports it (and logs it). Returns the number that changed.
	std::uint32_t CheckIntegrity(SWF_IntegrityFn fn, void* user);
}
