#pragma once

#include "hooking/Hooks.h"

#include <array>
#include <cstdint>

namespace swf::inline SWF_NAMESPACE
{
	/// Call sites of one function, hooked there instead of at its start.
	struct CallSites
	{
		std::array<std::uint32_t, 8> offsets;  // `call rel32` to the hook's function
		std::uint32_t count;
	};

	/// The fields SWF_Api::Write stores, for one runtime.
	struct SceneLayout
	{
		std::uint32_t flags;           // NiAVObject::flags (kHidden is bit 0)
		std::uint32_t alphaProperty;   // BSGeometry's NiPointer<NiAlphaProperty>
		std::uint32_t shaderProperty;  // BSGeometry's NiPointer<BSShaderProperty>
		std::uint32_t alphaFlags;      // NiAlphaProperty's 16-bit flags
		std::uint32_t refCount;        // NiRefObject's count
		std::uint32_t deleteThisSlot;  // NiRefObject's DeleteThis, run when the count reaches 0
	};

	/// The engine functions and vtables the detour families hook, for one runtime.
	struct HookTargets
	{
		// Structure: NiNode's child-edit slots (0x35, 0x37-0x3C) and the classes overriding them.
		hooking::Vtable niNode;
		std::array<hooking::Vtable, 3> nodeOverrides;  // BSParticleSystemManager, BGSDecalNode, BSFaceGenNiNode
		std::uint32_t removeAllChildren;
		hooking::Vtable niSwitchNode;
		// The engine functions behind those slots (0x35, 0x37-0x3C), detoured even where another plugin swapped a slot first; 0 where
		// the table has none (the slot's implementation is detoured).
		std::array<std::uint32_t, 7> niNodeEdits;
		std::array<std::array<std::uint32_t, 7>, 3> overrideEdits;
		std::array<std::uint32_t, 7> switchEdits;

		// Moves.
		std::uint32_t animationGraphPlace;
		std::uint32_t reference3DPosition;
		std::uint32_t graphAnimation;
		std::uint32_t projectileUpdate3D;
		std::array<std::uint32_t, 2> selectiveUpdateCalls;  // `call [rax + 0x168]` in the cells' update passes
		std::array<std::uint8_t, 6> selectiveUpdateBytes;

		// Calls: a function offset per SWF_HOOK_* (0 for the slot hooks below).
		std::array<std::uint32_t, SWF_HOOK_COUNT> functions;
		hooking::Vtable fadeNode;            // SWF_HOOK_FADE_ON_VISIBLE: slot 0x34's implementation
		std::uint32_t fadeOnVisible;         // that implementation (0: read from the slot)
		hooking::Vtable lightingProperty;    // slots 0x2A, 0x31
		hooking::Vtable floatController;     // slot 0x27
		hooking::Vtable colorController;     // slot 0x27
		std::array<hooking::Vtable, 14> lightingMaterials;  // slots 0x02, 0x08-0x0A

		// Feature families.
		std::array<CallSites, SWF_HOOK_COUNT> callSites;  // per site-hooked SWF_HOOK_*: the calls of functions[hook]
		std::array<hooking::Vtable, 3> actors;            // Actor, Character, PlayerCharacter: slot 0xE1
		std::array<std::uint32_t, 5> decalArrayEdits;     // SWF_HOOK_DECAL_ARRAY_EDIT's functions
		hooking::Vtable instanceShape;                    // BSMultiStreamInstanceTriShape: slots 0x3C, 0x3D
		hooking::Vtable effectFloatController;            // slot 0x27
		hooking::Vtable effectColorController;            // slot 0x27
		hooking::Vtable effectMaterial;                   // slot 0x02

		SceneLayout scene;
	};

	/// A place inside the function an Address Library id names: a call site, `at` bytes past its start.
	struct SiteId
	{
		std::uint64_t id;
		std::uint32_t at;
	};

	/// A vtable as Address Library names it (CommonLib's VTABLE_* ids), with its RTTI type descriptor's name.
	struct VtableId
	{
		std::uint64_t id;
		const char* rtti;
	};

	struct CallSiteIds
	{
		std::array<SiteId, 8> sites;
		std::uint32_t count;
	};

	/// HookTargets as one runtime's Address Library ids, which tables::Select resolves against that runtime's database.
	/// The functions and vtables are the ids the database gives them; only the call sites inside a function are positioned per
	/// game version.
	struct HookIds
	{
		VtableId niNode;
		std::array<VtableId, 3> nodeOverrides;
		std::uint64_t removeAllChildren;
		VtableId niSwitchNode;
		std::array<std::uint64_t, 7> niNodeEdits;
		std::array<std::array<std::uint64_t, 7>, 3> overrideEdits;
		std::array<std::uint64_t, 7> switchEdits;

		std::uint64_t animationGraphPlace;
		std::uint64_t reference3DPosition;
		std::uint64_t graphAnimation;
		std::uint64_t projectileUpdate3D;
		std::array<SiteId, 2> selectiveUpdateCalls;
		std::array<std::uint8_t, 6> selectiveUpdateBytes;

		std::array<std::uint64_t, SWF_HOOK_COUNT> functions;
		VtableId fadeNode;
		std::uint64_t fadeOnVisible;
		VtableId lightingProperty;
		VtableId floatController;
		VtableId colorController;
		std::array<VtableId, 14> lightingMaterials;

		std::array<CallSiteIds, SWF_HOOK_COUNT> callSites;
		std::array<VtableId, 3> actors;
		std::array<std::uint64_t, 5> decalArrayEdits;
		VtableId instanceShape;
		VtableId effectFloatController;
		VtableId effectColorController;
		VtableId effectMaterial;

		SceneLayout scene;
	};

	namespace tables::ae_1_6_1170
	{
		const HookIds& Ids();
	}

	namespace tables::ae_1_7_104
	{
		const HookIds& Ids();
	}

	namespace tables::se_1_5_97
	{
		const HookIds& Ids();
	}
}
