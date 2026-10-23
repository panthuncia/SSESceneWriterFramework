// The call families: engine functions hooked as SWF_EVENT_CALL(hook), PRE and POST with their
// arguments. Signatures are the engine's, with engine types as pointers.
#include "families/CallHook.h"
#include "families/DetourFamilies.h"

#include "core/Log.h"
#include "hooking/Hooks.h"

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

		/// What a family hooks: detours at function starts, and vtable slots swapped.
		struct Plan
		{
			std::vector<hooking::Detour> detours;
			std::vector<hooking::SlotSwap> swaps;
			std::vector<std::uintptr_t> targets;  // every function a detour starts at
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

		template <std::uint32_t... V>
		void SwapMaterials(Plan& plan, const hooking::Image& image, const HookTargets& targets, std::integer_sequence<std::uint32_t, V...>)
		{
			((SwapSlot<MaterialCopyMembers<V>>(plan, image, targets.lightingMaterials[V], 0x02),
				 SwapSlot<MaterialLoadTextureSet<V>>(plan, image, targets.lightingMaterials[V], 0x08),
				 SwapSlot<MaterialClearTextures<V>>(plan, image, targets.lightingMaterials[V], 0x09),
				 SwapSlot<MaterialReceiveValues<V>>(plan, image, targets.lightingMaterials[V], 0x0A)),
				...);
		}

		// Builds the plan without touching memory beyond reading vtable slots, which only happens once
		// every vtable it reads has matched its RTTI (the verify pass).
		bool Build(SWF_Caps family, const hooking::Image& image, const HookTargets& targets, Plan& plan, bool readSlots)
		{
			switch (family) {
			case SWF_CAP_FADE_CALLS:
				plan.vtables.push_back(targets.fadeNode);
				if (readSlots)
					DetourAt<FadeOnVisible>(plan, hooking::SlotTarget(image, targets.fadeNode, 0x34));
				DetourAt<FadeUpdate>(plan, image.base + targets.functions[SWF_HOOK_FADE_UPDATE]);
				DetourAt<FadeLodLevel>(plan, image.base + targets.functions[SWF_HOOK_FADE_LOD_LEVEL]);
				DetourAt<FadeSetRange>(plan, image.base + targets.functions[SWF_HOOK_FADE_SET_RANGE]);
				DetourAt<FadeSetLodType>(plan, image.base + targets.functions[SWF_HOOK_FADE_SET_LOD_TYPE]);
				return true;
			case SWF_CAP_NODE_CALLS:
				DetourAt<CellPlaceReference>(plan, image.base + targets.functions[SWF_HOOK_CELL_PLACE_REFERENCE]);
				DetourAt<CellPlaceReferenceAt>(plan, image.base + targets.functions[SWF_HOOK_CELL_PLACE_REFERENCE_AT]);
				DetourAt<RigidBodyMotionType>(plan, image.base + targets.functions[SWF_HOOK_RIGID_BODY_MOTION_TYPE]);
				DetourAt<ReleaseCollisionObject>(plan, image.base + targets.functions[SWF_HOOK_RELEASE_COLLISION_OBJECT]);
				DetourAt<SetCollisionObject>(plan, image.base + targets.functions[SWF_HOOK_SET_COLLISION_OBJECT]);
				DetourAt<PrependController>(plan, image.base + targets.functions[SWF_HOOK_PREPEND_CONTROLLER]);
				DetourAt<RemoveController>(plan, image.base + targets.functions[SWF_HOOK_REMOVE_CONTROLLER]);
				DetourAt<HavokNodeTransform>(plan, image.base + targets.functions[SWF_HOOK_HAVOK_NODE_TRANSFORM]);
				DetourAt<DismemberPartition>(plan, image.base + targets.functions[SWF_HOOK_DISMEMBER_PARTITION]);
				DetourAt<BipedDismemberPartitions>(plan, image.base + targets.functions[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS]);
				return true;
			case SWF_CAP_PROPERTY_CALLS:
				DetourAt<PropertySetFlags>(plan, image.base + targets.functions[SWF_HOOK_PROPERTY_SET_FLAGS]);
				DetourAt<PropertySetMaterial>(plan, image.base + targets.functions[SWF_HOOK_PROPERTY_SET_MATERIAL]);
				DetourAt<PropertiesSetFlag>(plan, image.base + targets.functions[SWF_HOOK_PROPERTIES_SET_FLAG]);
				DetourAt<PropertiesSetFadeNode>(plan, image.base + targets.functions[SWF_HOOK_PROPERTIES_SET_FADE_NODE]);
				DetourAt<PropertiesProjectedUV>(plan, image.base + targets.functions[SWF_HOOK_PROPERTIES_PROJECTED_UV]);
				DetourAt<SetExternalEmittance>(plan, image.base + targets.functions[SWF_HOOK_SET_EXTERNAL_EMITTANCE]);
				DetourAt<SkySetColor>(plan, image.base + targets.functions[SWF_HOOK_SKY_SET_COLOR]);
				SwapSlot<LightingSetMaterialAlpha>(plan, image, targets.lightingProperty, 0x31);
				SwapSlot<LightingGetRenderPasses>(plan, image, targets.lightingProperty, 0x2A);
				return true;
			case SWF_CAP_GEOMETRY_CALLS:
				DetourAt<GeometryAttachAlpha>(plan, image.base + targets.functions[SWF_HOOK_GEOMETRY_ATTACH_ALPHA]);
				DetourAt<AlphasSetThreshold>(plan, image.base + targets.functions[SWF_HOOK_ALPHAS_SET_THRESHOLD]);
				DetourAt<WeaponBloodGeometry>(plan, image.base + targets.functions[SWF_HOOK_WEAPON_BLOOD_GEOMETRY]);
				DetourAt<LandSetupProperties>(plan, image.base + targets.functions[SWF_HOOK_LAND_SETUP_PROPERTIES]);
				DetourAt<LandBlendParams>(plan, image.base + targets.functions[SWF_HOOK_LAND_BLEND_PARAMS]);
				return true;
			case SWF_CAP_MATERIAL_CALLS:
				SwapSlot<FloatControllerUpdate>(plan, image, targets.floatController, 0x27);
				SwapSlot<ColorControllerUpdate>(plan, image, targets.colorController, 0x27);
				SwapMaterials(plan, image, targets, std::make_integer_sequence<std::uint32_t, 14>{});
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
		return true;
	}

	bool InstallCalls(SWF_Caps family, const hooking::Image& image, const HookTargets& targets)
	{
		Plan plan;
		if (!Build(family, image, targets, plan, true))
			return false;
		for (const std::uintptr_t target : plan.targets)
			if (!hooking::InImage(image, target))
				return false;
		if (!hooking::AttachAll(plan.detours, NameOf(family)))
			return false;
		hooking::SwapAll(plan.swaps);
		log::Info("{}: {} detours, {} vtable slots", NameOf(family), plan.detours.size(), plan.swaps.size());
		return true;
	}

	SWF_Caps CallFamilyOf(std::uint32_t hook)
	{
		if (hook >= SWF_HOOK_FADE_ON_VISIBLE && hook <= SWF_HOOK_FADE_SET_LOD_TYPE)
			return SWF_CAP_FADE_CALLS;
		if (hook >= SWF_HOOK_CELL_PLACE_REFERENCE && hook <= SWF_HOOK_BIPED_DISMEMBER_PARTITIONS)
			return SWF_CAP_NODE_CALLS;
		if (hook >= SWF_HOOK_PROPERTY_SET_FLAGS && hook <= SWF_HOOK_SKY_SET_COLOR)
			return SWF_CAP_PROPERTY_CALLS;
		if (hook >= SWF_HOOK_GEOMETRY_ATTACH_ALPHA && hook <= SWF_HOOK_LAND_BLEND_PARAMS)
			return SWF_CAP_GEOMETRY_CALLS;
		if (hook >= SWF_HOOK_FLOAT_CONTROLLER_UPDATE && hook <= SWF_HOOK_MATERIAL_RECEIVE_VALUES)
			return SWF_CAP_MATERIAL_CALLS;
		return 0;
	}
}
