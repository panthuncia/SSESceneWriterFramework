// The call families: engine functions hooked as SWF_EVENT_CALL(hook), PRE and POST with their
// arguments. Signatures are the engine's, with engine types as pointers.
#include "families/CallHook.h"
#include "families/DetourFamilies.h"

#include "core/Log.h"
#include "hooking/Hooks.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace swf::inline SWF_NAMESPACE::detours
{
	namespace
	{
		template <std::uint32_t H, class Signature, std::uint32_t Instance = 0>
		using Hook = calls::CallHook<H, Instance, Signature>;

		using FadeOnVisible = Hook<SWF_HOOK_FADE_ON_VISIBLE, void(void*, void*, std::int32_t)>;
		using FadeUpdate = Hook<SWF_HOOK_FADE_UPDATE, std::uint64_t(void*, float, void*)>;
		using FadeLodLevel = Hook<SWF_HOOK_FADE_LOD_LEVEL, void(void*, float)>;
		using FadeSetRange = Hook<SWF_HOOK_FADE_SET_RANGE, void(void*, float, float)>;
		using FadeSetLodType = Hook<SWF_HOOK_FADE_SET_LOD_TYPE, void(void*, std::uint8_t)>;
		using FadeSnap = Hook<SWF_HOOK_FADE_SNAP, std::uint64_t(void*, void*)>;

		using CellPlaceReference = Hook<SWF_HOOK_CELL_PLACE_REFERENCE, void(void*, void*)>;
		using CellPlaceReferenceAt = Hook<SWF_HOOK_CELL_PLACE_REFERENCE_AT, void*(void*, void*, bool)>;
		using RigidBodyMotionType = Hook<SWF_HOOK_RIGID_BODY_MOTION_TYPE, void(void*, std::uint32_t, std::uint32_t, std::uint32_t)>;
		using ReleaseCollisionObject = Hook<SWF_HOOK_RELEASE_COLLISION_OBJECT, void(void*)>;
		using SetCollisionObject = Hook<SWF_HOOK_SET_COLLISION_OBJECT, void(void*, void*)>;
		using PrependController = Hook<SWF_HOOK_PREPEND_CONTROLLER, void(void*, void*)>;
		using RemoveController = Hook<SWF_HOOK_REMOVE_CONTROLLER, void(void*, void*)>;
		using HavokNodeTransform = Hook<SWF_HOOK_HAVOK_NODE_TRANSFORM, void(void*, const void*)>;
		using DismemberPartition = Hook<SWF_HOOK_DISMEMBER_PARTITION, void(void*, std::uint16_t, bool)>;
		using BipedDismemberPartitions = Hook<SWF_HOOK_BIPED_DISMEMBER_PARTITIONS, void(void*, void*, void*, std::int32_t, bool)>;
		using HavokInit = Hook<SWF_HOOK_HAVOK_INIT, void(void*, void*, void*)>;
		using ReferenceDisable = Hook<SWF_HOOK_REFERENCE_DISABLE, bool(void*, bool, bool, void*, std::uint32_t)>;
		using EnableCommand = Hook<SWF_HOOK_ENABLE_COMMAND, bool(void*, void*, void*, void*, void*, void*, void*, void*)>;

		using PropertySetFlags = Hook<SWF_HOOK_PROPERTY_SET_FLAGS, void(void*, std::uint8_t, bool)>;
		using PropertySetMaterial = Hook<SWF_HOOK_PROPERTY_SET_MATERIAL, void(void*, void*, bool)>;
		using LightingSetMaterialAlpha = Hook<SWF_HOOK_LIGHTING_SET_MATERIAL_ALPHA, void(void*, float)>;
		using LightingGetRenderPasses = Hook<SWF_HOOK_LIGHTING_GET_RENDER_PASSES, void*(void*, void*, std::uint32_t, void*)>;
		using PropertiesSetFlag = Hook<SWF_HOOK_PROPERTIES_SET_FLAG, std::uint64_t(void*, std::uint32_t, bool, bool)>;
		using PropertiesSetFadeNode = Hook<SWF_HOOK_PROPERTIES_SET_FADE_NODE, void(void*, void*)>;
		using PropertiesProjectedUV = Hook<SWF_HOOK_PROPERTIES_PROJECTED_UV, std::uint64_t(void*, void*)>;
		using SetExternalEmittance = Hook<SWF_HOOK_SET_EXTERNAL_EMITTANCE, std::uint64_t(void*, void*)>;
		using SkySetColor = Hook<SWF_HOOK_SKY_SET_COLOR, void(void*, void*, void*, float)>;

		using GeometryAttachAlpha = Hook<SWF_HOOK_GEOMETRY_ATTACH_ALPHA, void(void*, void*)>;
		using AlphasSetThreshold = Hook<SWF_HOOK_ALPHAS_SET_THRESHOLD, std::uint64_t(void*, void*)>;
		using FloraHarvestAlphas = Hook<SWF_HOOK_FLORA_HARVEST_ALPHAS, std::uint64_t(void*, const bool*)>;
		using WeaponBloodGeometry = Hook<SWF_HOOK_WEAPON_BLOOD_GEOMETRY, std::uint64_t(void*, void*)>;
		using LandSetupProperties = Hook<SWF_HOOK_LAND_SETUP_PROPERTIES, std::uint64_t(void*)>;
		using LandBlendParams = Hook<SWF_HOOK_LAND_BLEND_PARAMS, void(void*)>;

		using FloatControllerUpdate = Hook<SWF_HOOK_FLOAT_CONTROLLER_UPDATE, void(void*, void*)>;
		using ColorControllerUpdate = Hook<SWF_HOOK_COLOR_CONTROLLER_UPDATE, void(void*, void*)>;
		template <std::uint32_t V>
		using MaterialCopyMembers = Hook<SWF_HOOK_MATERIAL_COPY_MEMBERS, void(void*, void*), V>;
		template <std::uint32_t V>
		using MaterialLoadTextureSet = Hook<SWF_HOOK_MATERIAL_LOAD_TEXTURE_SET, void(void*, std::uint64_t, void*), V>;
		template <std::uint32_t V>
		using MaterialClearTextures = Hook<SWF_HOOK_MATERIAL_CLEAR_TEXTURES, void(void*), V>;
		template <std::uint32_t V>
		using MaterialReceiveValues = Hook<SWF_HOOK_MATERIAL_RECEIVE_VALUES, void(void*, bool, bool, bool, bool, bool), V>;

		template <std::uint32_t V>
		using ActorSetAlpha = Hook<SWF_HOOK_ACTOR_SET_ALPHA, void(void*, float), V>;
		using ActorUpdate3DPosition = Hook<SWF_HOOK_ACTOR_UPDATE_3D_POSITION, void(void*)>;
		using ActorSkyCellSkin = Hook<SWF_HOOK_ACTOR_SKY_CELL_SKIN, std::uint64_t(void*)>;
		using ActorRagdollJob = Hook<SWF_HOOK_ACTOR_RAGDOLL_JOB, void(void*)>;

		template <std::uint32_t V>
		using DecalArrayEdit = Hook<SWF_HOOK_DECAL_ARRAY_EDIT, void(void*, void*, void*, void*), V>;

		// The site-hooked ones: one instance per call site, each calling on to what its site called.
		template <std::uint32_t V>
		using LodSegmentShow = Hook<SWF_HOOK_LOD_SEGMENT_SHOW, void(void*, std::uint64_t), V>;
		template <std::uint32_t V>
		using LodSegmentHide = Hook<SWF_HOOK_LOD_SEGMENT_HIDE, void(void*, std::uint64_t), V>;
		template <std::uint32_t V>
		using LodSegmentShowAll = Hook<SWF_HOOK_LOD_SEGMENT_SHOW_ALL, void(void*), V>;

		template <std::uint32_t V>
		using TreeLodRefill = Hook<SWF_HOOK_TREE_LOD_REFILL, void(void*, void*), V>;
		using InstanceAddGroup = Hook<SWF_HOOK_INSTANCE_ADD_GROUP, std::uint32_t(void*, std::uint32_t, const std::uint16_t*, std::uint32_t, float)>;
		using InstanceRemoveGroup = Hook<SWF_HOOK_INSTANCE_REMOVE_GROUP, void(void*, std::uint32_t)>;

		using TreeAnimAdd = Hook<SWF_HOOK_TREE_ANIM_ADD, void(void*, void*, std::uint64_t, std::uint64_t)>;
		using TreeAnimRemove = Hook<SWF_HOOK_TREE_ANIM_REMOVE, std::uint64_t(void*, void*)>;
		using TreeAnimClear = Hook<SWF_HOOK_TREE_ANIM_CLEAR, std::uint64_t(void*, std::uint64_t, std::uint64_t, std::uint64_t)>;

		using EffectFloatControllerUpdate = Hook<SWF_HOOK_EFFECT_FLOAT_CONTROLLER_UPDATE, void(void*, void*)>;
		using EffectColorControllerUpdate = Hook<SWF_HOOK_EFFECT_COLOR_CONTROLLER_UPDATE, void(void*, void*)>;
		using EffectMaterialCopyMembers = Hook<SWF_HOOK_EFFECT_MATERIAL_COPY_MEMBERS, void(void*, void*)>;

		/// What a family hooks: detours at function starts, vtable slots swapped, and call sites.
		struct Plan
		{
			std::vector<hooking::Detour> detours;
			std::vector<hooking::SlotSwap> swaps;
			std::vector<hooking::SitePatch> sites;
			std::vector<std::uintptr_t> siteTargets;  // what each site calls now (0: not the engine's call, nor a hook chaining to it)
			std::vector<std::uintptr_t> targets;      // every engine function a detour starts at, or a site calls
			std::vector<hooking::Vtable> vtables;
		};

		template <class H>
		void DetourAt(Plan& plan, std::uintptr_t target)
		{
			H::original = reinterpret_cast<decltype(H::original)>(target);
			plan.detours.push_back({ H::OriginalSlot(), H::ThunkAddress() });
			plan.targets.push_back(target);
		}

		template <class H>
		void SwapSlot(Plan& plan, const hooking::Image& image, const hooking::Vtable& vtable, std::size_t slot)
		{
			plan.swaps.push_back({ hooking::SlotAddress(image, vtable, slot), H::OriginalSlot(), H::ThunkAddress() });
			plan.vtables.push_back(vtable);
		}

		const char* NameOf(SWF_Caps family);

		// Every call site of the hook's function, rewritten to call a thunk of its own (H<site>), which calls on to what the site
		// called: the engine function, or another plugin's hook there that calls on to it (as write_call<5> keeps the call it took).
		template <template <std::uint32_t> class H, std::uint32_t... V>
		void PatchAt(Plan& plan, const hooking::Image& image, const HookTargets& targets, SWF_Caps family, std::uint32_t hook,
			std::integer_sequence<std::uint32_t, V...>)
		{
			static_assert(sizeof...(V) == std::tuple_size_v<decltype(CallSites::offsets)>);
			const std::uintptr_t engine = hooking::At(image, targets.functions[hook]);
			const CallSites& sites = targets.callSites[hook];
			auto patch = [&]<class S>(std::uint32_t i) {
				if (i >= sites.count)
					return;
				const std::uintptr_t current = hooking::ChainedCallTarget(image, sites.offsets[i], engine, NameOf(family));
				S::original = reinterpret_cast<decltype(S::original)>(current);
				plan.sites.push_back({ sites.offsets[i], 5, S::ThunkAddress() });
				plan.siteTargets.push_back(current);
			};
			(patch.template operator()<H<V>>(V), ...);
			plan.targets.push_back(engine);
		}

		template <template <std::uint32_t> class H>
		void PatchAt(Plan& plan, const hooking::Image& image, const HookTargets& targets, SWF_Caps family, std::uint32_t hook)
		{
			PatchAt<H>(plan, image, targets, family, hook, std::make_integer_sequence<std::uint32_t, 8>{});
		}

		template <std::uint32_t... V>
		void DetourDecals(Plan& plan, const hooking::Image& image, const HookTargets& targets, std::integer_sequence<std::uint32_t, V...>)
		{
			(DetourAt<DecalArrayEdit<V>>(plan, hooking::At(image, targets.decalArrayEdits[V])), ...);
		}

		template <std::uint32_t... V>
		void SwapMaterials(Plan& plan, const hooking::Image& image, const HookTargets& targets, std::integer_sequence<std::uint32_t, V...>)
		{
			((SwapSlot<MaterialCopyMembers<V>>(plan, image, targets.lightingMaterials[V], 0x02),
				 SwapSlot<MaterialLoadTextureSet<V>>(plan, image, targets.lightingMaterials[V], 0x08),
				 SwapSlot<MaterialClearTextures<V>>(plan, image, targets.lightingMaterials[V], 0x09),
				 SwapSlot<MaterialReceiveValues<V>>(plan, image, targets.lightingMaterials[V], 0x0A)),
				...);
		}

		// Builds the plan without touching memory beyond reading vtable slots and call sites, which only happens once every vtable
		// it reads has matched its RTTI (the verify pass).
		bool Build(SWF_Caps family, const hooking::Image& image, const HookTargets& targets, Plan& plan, bool readSlots)
		{
			switch (family) {
			case SWF_CAP_FADE_CALLS:
				plan.vtables.push_back(targets.fadeNode);
				if (readSlots)
					DetourAt<FadeOnVisible>(plan, hooking::SlotImplementation(image, targets.fadeNode, 0x34, targets.fadeOnVisible, "fade calls"));
				DetourAt<FadeUpdate>(plan, hooking::At(image, targets.functions[SWF_HOOK_FADE_UPDATE]));
				DetourAt<FadeLodLevel>(plan, hooking::At(image, targets.functions[SWF_HOOK_FADE_LOD_LEVEL]));
				DetourAt<FadeSetRange>(plan, hooking::At(image, targets.functions[SWF_HOOK_FADE_SET_RANGE]));
				DetourAt<FadeSetLodType>(plan, hooking::At(image, targets.functions[SWF_HOOK_FADE_SET_LOD_TYPE]));
				DetourAt<FadeSnap>(plan, hooking::At(image, targets.functions[SWF_HOOK_FADE_SNAP]));
				return true;
			case SWF_CAP_NODE_CALLS:
				DetourAt<CellPlaceReference>(plan, hooking::At(image, targets.functions[SWF_HOOK_CELL_PLACE_REFERENCE]));
				DetourAt<CellPlaceReferenceAt>(plan, hooking::At(image, targets.functions[SWF_HOOK_CELL_PLACE_REFERENCE_AT]));
				DetourAt<RigidBodyMotionType>(plan, hooking::At(image, targets.functions[SWF_HOOK_RIGID_BODY_MOTION_TYPE]));
				DetourAt<ReleaseCollisionObject>(plan, hooking::At(image, targets.functions[SWF_HOOK_RELEASE_COLLISION_OBJECT]));
				DetourAt<SetCollisionObject>(plan, hooking::At(image, targets.functions[SWF_HOOK_SET_COLLISION_OBJECT]));
				DetourAt<PrependController>(plan, hooking::At(image, targets.functions[SWF_HOOK_PREPEND_CONTROLLER]));
				DetourAt<RemoveController>(plan, hooking::At(image, targets.functions[SWF_HOOK_REMOVE_CONTROLLER]));
				DetourAt<HavokNodeTransform>(plan, hooking::At(image, targets.functions[SWF_HOOK_HAVOK_NODE_TRANSFORM]));
				DetourAt<DismemberPartition>(plan, hooking::At(image, targets.functions[SWF_HOOK_DISMEMBER_PARTITION]));
				DetourAt<BipedDismemberPartitions>(plan, hooking::At(image, targets.functions[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS]));
				DetourAt<HavokInit>(plan, hooking::At(image, targets.functions[SWF_HOOK_HAVOK_INIT]));
				DetourAt<ReferenceDisable>(plan, hooking::At(image, targets.functions[SWF_HOOK_REFERENCE_DISABLE]));
				DetourAt<EnableCommand>(plan, hooking::At(image, targets.functions[SWF_HOOK_ENABLE_COMMAND]));
				return true;
			case SWF_CAP_PROPERTY_CALLS:
				DetourAt<PropertySetFlags>(plan, hooking::At(image, targets.functions[SWF_HOOK_PROPERTY_SET_FLAGS]));
				DetourAt<PropertySetMaterial>(plan, hooking::At(image, targets.functions[SWF_HOOK_PROPERTY_SET_MATERIAL]));
				DetourAt<PropertiesSetFlag>(plan, hooking::At(image, targets.functions[SWF_HOOK_PROPERTIES_SET_FLAG]));
				DetourAt<PropertiesSetFadeNode>(plan, hooking::At(image, targets.functions[SWF_HOOK_PROPERTIES_SET_FADE_NODE]));
				DetourAt<PropertiesProjectedUV>(plan, hooking::At(image, targets.functions[SWF_HOOK_PROPERTIES_PROJECTED_UV]));
				DetourAt<SetExternalEmittance>(plan, hooking::At(image, targets.functions[SWF_HOOK_SET_EXTERNAL_EMITTANCE]));
				DetourAt<SkySetColor>(plan, hooking::At(image, targets.functions[SWF_HOOK_SKY_SET_COLOR]));
				SwapSlot<LightingSetMaterialAlpha>(plan, image, targets.lightingProperty, 0x31);
				SwapSlot<LightingGetRenderPasses>(plan, image, targets.lightingProperty, 0x2A);
				return true;
			case SWF_CAP_GEOMETRY_CALLS:
				DetourAt<GeometryAttachAlpha>(plan, hooking::At(image, targets.functions[SWF_HOOK_GEOMETRY_ATTACH_ALPHA]));
				DetourAt<AlphasSetThreshold>(plan, hooking::At(image, targets.functions[SWF_HOOK_ALPHAS_SET_THRESHOLD]));
				DetourAt<FloraHarvestAlphas>(plan, hooking::At(image, targets.functions[SWF_HOOK_FLORA_HARVEST_ALPHAS]));
				DetourAt<WeaponBloodGeometry>(plan, hooking::At(image, targets.functions[SWF_HOOK_WEAPON_BLOOD_GEOMETRY]));
				DetourAt<LandSetupProperties>(plan, hooking::At(image, targets.functions[SWF_HOOK_LAND_SETUP_PROPERTIES]));
				DetourAt<LandBlendParams>(plan, hooking::At(image, targets.functions[SWF_HOOK_LAND_BLEND_PARAMS]));
				return true;
			case SWF_CAP_MATERIAL_CALLS:
				SwapSlot<FloatControllerUpdate>(plan, image, targets.floatController, 0x27);
				SwapSlot<ColorControllerUpdate>(plan, image, targets.colorController, 0x27);
				SwapMaterials(plan, image, targets, std::make_integer_sequence<std::uint32_t, 14>{});
				return true;
			case SWF_CAP_ACTOR_CALLS:
				SwapSlot<ActorSetAlpha<0>>(plan, image, targets.actors[0], 0xE1);
				SwapSlot<ActorSetAlpha<1>>(plan, image, targets.actors[1], 0xE1);
				SwapSlot<ActorSetAlpha<2>>(plan, image, targets.actors[2], 0xE1);
				DetourAt<ActorUpdate3DPosition>(plan, hooking::At(image, targets.functions[SWF_HOOK_ACTOR_UPDATE_3D_POSITION]));
				DetourAt<ActorSkyCellSkin>(plan, hooking::At(image, targets.functions[SWF_HOOK_ACTOR_SKY_CELL_SKIN]));
				DetourAt<ActorRagdollJob>(plan, hooking::At(image, targets.functions[SWF_HOOK_ACTOR_RAGDOLL_JOB]));
				return true;
			case SWF_CAP_DECAL_CALLS:
				DetourDecals(plan, image, targets, std::make_integer_sequence<std::uint32_t, 5>{});
				return true;
			case SWF_CAP_LOD_SEGMENT_CALLS:
				if (readSlots) {
					PatchAt<LodSegmentShow>(plan, image, targets, family, SWF_HOOK_LOD_SEGMENT_SHOW);
					PatchAt<LodSegmentHide>(plan, image, targets, family, SWF_HOOK_LOD_SEGMENT_HIDE);
					PatchAt<LodSegmentShowAll>(plan, image, targets, family, SWF_HOOK_LOD_SEGMENT_SHOW_ALL);
				}
				return true;
			case SWF_CAP_TREE_LOD_CALLS:
				if (readSlots)
					PatchAt<TreeLodRefill>(plan, image, targets, family, SWF_HOOK_TREE_LOD_REFILL);
				SwapSlot<InstanceAddGroup>(plan, image, targets.instanceShape, 0x3C);
				SwapSlot<InstanceRemoveGroup>(plan, image, targets.instanceShape, 0x3D);
				return true;
			case SWF_CAP_TREE_ANIM_CALLS:
				DetourAt<TreeAnimAdd>(plan, hooking::At(image, targets.functions[SWF_HOOK_TREE_ANIM_ADD]));
				DetourAt<TreeAnimRemove>(plan, hooking::At(image, targets.functions[SWF_HOOK_TREE_ANIM_REMOVE]));
				DetourAt<TreeAnimClear>(plan, hooking::At(image, targets.functions[SWF_HOOK_TREE_ANIM_CLEAR]));
				return true;
			case SWF_CAP_EFFECT_CALLS:
				SwapSlot<EffectFloatControllerUpdate>(plan, image, targets.effectFloatController, 0x27);
				SwapSlot<EffectColorControllerUpdate>(plan, image, targets.effectColorController, 0x27);
				SwapSlot<EffectMaterialCopyMembers>(plan, image, targets.effectMaterial, 0x02);
				return true;
			default:
				return false;
			}
		}

		const char* NameOf(SWF_Caps family)
		{
			switch (family) {
			case SWF_CAP_FADE_CALLS:
				return "fade calls";
			case SWF_CAP_NODE_CALLS:
				return "node calls";
			case SWF_CAP_PROPERTY_CALLS:
				return "property calls";
			case SWF_CAP_GEOMETRY_CALLS:
				return "geometry calls";
			case SWF_CAP_MATERIAL_CALLS:
				return "material calls";
			case SWF_CAP_ACTOR_CALLS:
				return "actor calls";
			case SWF_CAP_DECAL_CALLS:
				return "decal calls";
			case SWF_CAP_LOD_SEGMENT_CALLS:
				return "LOD segment calls";
			case SWF_CAP_TREE_LOD_CALLS:
				return "tree LOD calls";
			case SWF_CAP_TREE_ANIM_CALLS:
				return "tree animation calls";
			case SWF_CAP_EFFECT_CALLS:
				return "effect calls";
			default:
				return "calls";
			}
		}
	}

	bool VerifyCalls(SWF_Caps family, const hooking::Image& image, const HookTargets& targets)
	{
		Plan vtables;
		if (!Build(family, image, targets, vtables, false))
			return false;
		for (const hooking::Vtable& vtable : vtables.vtables)
			if (!hooking::VtableMatches(image, vtable))
				return false;
		// The vtables are the expected classes: their slots can be read for the detour targets.
		Plan plan;
		Build(family, image, targets, plan, true);
		for (const std::uintptr_t target : plan.targets) {
			if (!hooking::InImage(image, target)) {
				log::Warn("{}: a target at {:#x} is outside the image", NameOf(family), target);
				return false;
			}
		}
		// A call site that calls neither the engine function nor another plugin's hook (ChainedCallTarget logged it): the family is
		// left out.
		return std::ranges::find(plan.siteTargets, std::uintptr_t(0)) == plan.siteTargets.end();
	}

	bool InstallCalls(SWF_Caps family, const hooking::Image& image, const HookTargets& targets)
	{
		Plan plan;
		if (!Build(family, image, targets, plan, true))
			return false;
		for (const std::uintptr_t target : plan.targets)
			if (!hooking::InImage(image, target))
				return false;
		if (std::ranges::find(plan.siteTargets, std::uintptr_t(0)) != plan.siteTargets.end())
			return false;
		// The sites' stubs are placed first and written last: a family whose sites or detours fail installs nothing.
		std::optional<hooking::PendingCalls> sites;
		if (!plan.sites.empty()) {
			sites.emplace(hooking::PrepareCallSites(image, plan.sites));
			if (!*sites) {
				log::Warn("{}: no room for the call sites' stubs", NameOf(family));
				return false;
			}
		}
		if (!hooking::AttachAll(plan.detours, NameOf(family)))
			return false;
		if (sites)
			sites->Commit();
		hooking::SwapAll(plan.swaps);
		log::Info("{}: {} detours, {} vtable slots, {} call sites", NameOf(family), plan.detours.size(), plan.swaps.size(), plan.sites.size());
		return true;
	}

	SWF_Caps CallFamilyOf(std::uint32_t hook)
	{
		if (hook >= SWF_HOOK_FADE_ON_VISIBLE && hook <= SWF_HOOK_FADE_SET_LOD_TYPE)
			return SWF_CAP_FADE_CALLS;
		if ((hook >= SWF_HOOK_CELL_PLACE_REFERENCE && hook <= SWF_HOOK_BIPED_DISMEMBER_PARTITIONS) || (hook >= SWF_HOOK_HAVOK_INIT && hook <= SWF_HOOK_ENABLE_COMMAND))
			return SWF_CAP_NODE_CALLS;
		if (hook >= SWF_HOOK_PROPERTY_SET_FLAGS && hook <= SWF_HOOK_SKY_SET_COLOR)
			return SWF_CAP_PROPERTY_CALLS;
		if ((hook >= SWF_HOOK_GEOMETRY_ATTACH_ALPHA && hook <= SWF_HOOK_LAND_BLEND_PARAMS) || hook == SWF_HOOK_FLORA_HARVEST_ALPHAS)
			return SWF_CAP_GEOMETRY_CALLS;
		if (hook >= SWF_HOOK_FLOAT_CONTROLLER_UPDATE && hook <= SWF_HOOK_MATERIAL_RECEIVE_VALUES)
			return SWF_CAP_MATERIAL_CALLS;
		if (hook == SWF_HOOK_FADE_SNAP)
			return SWF_CAP_FADE_CALLS;
		if (hook >= SWF_HOOK_ACTOR_SET_ALPHA && hook <= SWF_HOOK_ACTOR_RAGDOLL_JOB)
			return SWF_CAP_ACTOR_CALLS;
		if (hook == SWF_HOOK_DECAL_ARRAY_EDIT)
			return SWF_CAP_DECAL_CALLS;
		if (hook >= SWF_HOOK_LOD_SEGMENT_SHOW && hook <= SWF_HOOK_LOD_SEGMENT_SHOW_ALL)
			return SWF_CAP_LOD_SEGMENT_CALLS;
		if (hook >= SWF_HOOK_TREE_LOD_REFILL && hook <= SWF_HOOK_INSTANCE_REMOVE_GROUP)
			return SWF_CAP_TREE_LOD_CALLS;
		if (hook >= SWF_HOOK_TREE_ANIM_ADD && hook <= SWF_HOOK_TREE_ANIM_CLEAR)
			return SWF_CAP_TREE_ANIM_CALLS;
		if (hook >= SWF_HOOK_EFFECT_FLOAT_CONTROLLER_UPDATE && hook <= SWF_HOOK_EFFECT_MATERIAL_COPY_MEMBERS)
			return SWF_CAP_EFFECT_CALLS;
		return 0;
	}
}
