#include "families/Families.h"

#include "families/CallHook.h"
#include "families/DetourFamilies.h"
#include "families/Stores.h"
#include "tables/HookTargets.h"
#include "tables/StoreSites.h"

namespace swf::inline SWF_NAMESPACE
{
	namespace
	{
		namespace table = tables::ae_1_6_1170;

		bool VerifyHidden() { return stores::VerifyHidden(hooking::GameImage(), table::HiddenStores()); }
		bool InstallHidden() { return stores::InstallHidden(hooking::GameImage(), table::HiddenStores(), table::ShowSites()); }
		bool VerifySwitch() { return stores::VerifyValues(hooking::GameImage(), table::SwitchStores()); }
		bool InstallSwitch() { return stores::InstallSwitchIndex(hooking::GameImage(), table::SwitchStores()); }
		bool VerifyFadeReset() { return stores::VerifyValues(hooking::GameImage(), table::FadeResetStores()); }
		bool InstallFadeReset() { return stores::InstallFadeReset(hooking::GameImage(), table::FadeResetStores()); }

		bool VerifyStructure() { return detours::VerifyStructure(hooking::GameImage(), table::Targets()); }
		bool InstallStructure() { return detours::InstallStructure(hooking::GameImage(), table::Targets()); }
		bool VerifySwitchStructural() { return detours::VerifySwitchStructural(hooking::GameImage(), table::Targets()); }
		bool InstallSwitchStructural() { return detours::InstallSwitchStructural(hooking::GameImage(), table::Targets()); }
		bool VerifyMoves() { return detours::VerifyMoves(hooking::GameImage(), table::Targets()); }
		bool InstallMoves() { return detours::InstallMoves(hooking::GameImage(), table::Targets()); }

		template <SWF_Caps Family>
		bool VerifyCalls() { return detours::VerifyCalls(Family, hooking::GameImage(), table::Targets()); }
		template <SWF_Caps Family>
		bool InstallCalls() { return detours::InstallCalls(Family, hooking::GameImage(), table::Targets()); }

		// No family is required: a provider with some families missing still serves the rest,
		// and each client decides from the capability bits whether it can run.
		constexpr Family kFamilies[] = {
			{ "hidden", SWF_CAP_HIDDEN, false, &VerifyHidden, &InstallHidden },
			{ "switch-index", SWF_CAP_SWITCH_INDEX, false, &VerifySwitch, &InstallSwitch },
			{ "fade-reset", SWF_CAP_FADE_RESET, false, &VerifyFadeReset, &InstallFadeReset },
			{ "structure", SWF_CAP_STRUCTURE, false, &VerifyStructure, &InstallStructure },
			{ "switch-structural", SWF_CAP_SWITCH_STRUCTURAL, false, &VerifySwitchStructural, &InstallSwitchStructural },
			{ "moves", SWF_CAP_MOVES, false, &VerifyMoves, &InstallMoves },
			{ "fade-calls", SWF_CAP_FADE_CALLS, false, &VerifyCalls<SWF_CAP_FADE_CALLS>, &InstallCalls<SWF_CAP_FADE_CALLS> },
			{ "node-calls", SWF_CAP_NODE_CALLS, false, &VerifyCalls<SWF_CAP_NODE_CALLS>, &InstallCalls<SWF_CAP_NODE_CALLS> },
			{ "property-calls", SWF_CAP_PROPERTY_CALLS, false, &VerifyCalls<SWF_CAP_PROPERTY_CALLS>, &InstallCalls<SWF_CAP_PROPERTY_CALLS> },
			{ "geometry-calls", SWF_CAP_GEOMETRY_CALLS, false, &VerifyCalls<SWF_CAP_GEOMETRY_CALLS>, &InstallCalls<SWF_CAP_GEOMETRY_CALLS> },
			{ "material-calls", SWF_CAP_MATERIAL_CALLS, false, &VerifyCalls<SWF_CAP_MATERIAL_CALLS>, &InstallCalls<SWF_CAP_MATERIAL_CALLS> },
			{ "actor-calls", SWF_CAP_ACTOR_CALLS, false, &VerifyCalls<SWF_CAP_ACTOR_CALLS>, &InstallCalls<SWF_CAP_ACTOR_CALLS> },
			{ "decal-calls", SWF_CAP_DECAL_CALLS, false, &VerifyCalls<SWF_CAP_DECAL_CALLS>, &InstallCalls<SWF_CAP_DECAL_CALLS> },
			{ "lod-segment-calls", SWF_CAP_LOD_SEGMENT_CALLS, false, &VerifyCalls<SWF_CAP_LOD_SEGMENT_CALLS>, &InstallCalls<SWF_CAP_LOD_SEGMENT_CALLS> },
			{ "tree-lod-calls", SWF_CAP_TREE_LOD_CALLS, false, &VerifyCalls<SWF_CAP_TREE_LOD_CALLS>, &InstallCalls<SWF_CAP_TREE_LOD_CALLS> },
			{ "tree-anim-calls", SWF_CAP_TREE_ANIM_CALLS, false, &VerifyCalls<SWF_CAP_TREE_ANIM_CALLS>, &InstallCalls<SWF_CAP_TREE_ANIM_CALLS> },
			{ "effect-calls", SWF_CAP_EFFECT_CALLS, false, &VerifyCalls<SWF_CAP_EFFECT_CALLS>, &InstallCalls<SWF_CAP_EFFECT_CALLS> },
		};
	}

	std::span<const Family> Families()
	{
		return kFamilies;
	}

	SWF_Caps CapFor(uint32_t id, bool filter)
	{
		if (filter) {
			if (id == SWF_FILTER_SHOW)
				return SWF_CAP_HIDDEN;
			if (id > SWF_FILTER_CALL_BASE && id < SWF_FILTER_CALL(SWF_HOOK_COUNT) && calls::Vetoable(id - SWF_FILTER_CALL_BASE))
				return detours::CallFamilyOf(id - SWF_FILTER_CALL_BASE);
			return ~0ull;
		}
		if (id > SWF_EVENT_CALL_BASE && id < SWF_EVENT_CALL(SWF_HOOK_COUNT)) {
			const SWF_Caps family = detours::CallFamilyOf(id - SWF_EVENT_CALL_BASE);
			return family ? family : ~0ull;
		}
		switch (id) {
		case SWF_EVENT_WRITTEN:
		case SWF_EVENT_SCENE_WRITE:
			return 0;
		case SWF_EVENT_HIDDEN:
			return SWF_CAP_HIDDEN;
		case SWF_EVENT_SWITCH_INDEX:
			return SWF_CAP_SWITCH_INDEX;
		case SWF_EVENT_FADE_RESET:
			return SWF_CAP_FADE_RESET;
		case SWF_EVENT_CHILD_ATTACHED:
		case SWF_EVENT_CHILD_DETACHING:
			return SWF_CAP_STRUCTURE;
		case SWF_EVENT_SWITCH_STRUCTURAL:
			return SWF_CAP_SWITCH_STRUCTURAL;
		case SWF_EVENT_MOVED:
			return SWF_CAP_MOVES;
		default:
			return ~0ull;
		}
	}
}
