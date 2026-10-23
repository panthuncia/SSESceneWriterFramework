#pragma once

#include "hooking/Memory.h"
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::detours
{
	// Each family verifies every target it would hook (writing nothing), then installs as one
	// Detours transaction plus its vtable slot swaps.

	bool VerifyStructure(const hooking::Image& image, const HookTargets& targets);
	bool InstallStructure(const hooking::Image& image, const HookTargets& targets);

	bool VerifySwitchStructural(const hooking::Image& image, const HookTargets& targets);
	bool InstallSwitchStructural(const hooking::Image& image, const HookTargets& targets);

	bool VerifyMoves(const hooking::Image& image, const HookTargets& targets);
	bool InstallMoves(const hooking::Image& image, const HookTargets& targets);

	/// The call families, by capability (SWF_CAP_FADE_CALLS ... SWF_CAP_MATERIAL_CALLS).
	bool VerifyCalls(SWF_Caps family, const hooking::Image& image, const HookTargets& targets);
	bool InstallCalls(SWF_Caps family, const hooking::Image& image, const HookTargets& targets);

	/// The call family a SWF_HOOK_* belongs to (0 for an unknown hook).
	SWF_Caps CallFamilyOf(std::uint32_t hook);
}
