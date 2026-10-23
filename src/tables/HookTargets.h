#pragma once

#include "hooking/Hooks.h"

#include <array>
#include <cstdint>

namespace swf::inline SWF_NAMESPACE
{
	/// The engine functions and vtables the detour families hook, for one runtime.
	struct HookTargets
	{
		// Structure: NiNode's child-edit slots (0x35, 0x37-0x3C) and the classes overriding them.
		hooking::Vtable niNode;
		std::array<hooking::Vtable, 3> nodeOverrides;  // BSParticleSystemManager, BGSDecalNode, BSFaceGenNiNode
		std::uint32_t removeAllChildren;
		hooking::Vtable niSwitchNode;

		// Moves.
		std::uint32_t animationGraphPlace;
		std::uint32_t reference3DPosition;
		std::uint32_t graphAnimation;
		std::array<std::uint32_t, 2> selectiveUpdateCalls;  // `call [rax + 0x168]` in the cells' update passes
		std::array<std::uint8_t, 6> selectiveUpdateBytes;

		// Calls: a function offset per SWF_HOOK_* (0 for the slot hooks below).
		std::array<std::uint32_t, SWF_HOOK_COUNT> functions;
		hooking::Vtable fadeNode;            // SWF_HOOK_FADE_ON_VISIBLE: slot 0x34's implementation
		hooking::Vtable lightingProperty;    // slots 0x2A, 0x31
		hooking::Vtable floatController;     // slot 0x27
		hooking::Vtable colorController;     // slot 0x27
		std::array<hooking::Vtable, 14> lightingMaterials;  // slots 0x02, 0x08-0x0A
	};

	namespace tables::ae_1_6_1170
	{
		const HookTargets& Targets();
	}
}
