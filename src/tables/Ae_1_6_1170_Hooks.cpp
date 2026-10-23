// AE 1.6.1170 detour targets. Vtable offsets come from Address Library's versionlib-1-6-1170-0
// (CommonLib's VTABLE_* AE ids), and each is checked against its RTTI name before it is used.
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::tables::ae_1_6_1170
{
	namespace
	{
		constexpr std::array<std::uint32_t, SWF_HOOK_COUNT> Functions()
		{
			std::array<std::uint32_t, SWF_HOOK_COUNT> f{};
			f[SWF_HOOK_FADE_UPDATE] = 0x147a160;
			f[SWF_HOOK_FADE_LOD_LEVEL] = 0x147a430;
			f[SWF_HOOK_FADE_SET_RANGE] = 0x147a9b0;
			f[SWF_HOOK_FADE_SET_LOD_TYPE] = 0x147aa00;
			f[SWF_HOOK_CELL_PLACE_REFERENCE] = 0x2d1280;
			f[SWF_HOOK_CELL_PLACE_REFERENCE_AT] = 0x2d5090;
			f[SWF_HOOK_RIGID_BODY_MOTION_TYPE] = 0xb44490;
			f[SWF_HOOK_RELEASE_COLLISION_OBJECT] = 0xe880e0;
			f[SWF_HOOK_SET_COLLISION_OBJECT] = 0xe8bd40;
			f[SWF_HOOK_PREPEND_CONTROLLER] = 0xd268d0;
			f[SWF_HOOK_REMOVE_CONTROLLER] = 0xd269a0;
			f[SWF_HOOK_HAVOK_NODE_TRANSFORM] = 0xea55a0;
			f[SWF_HOOK_DISMEMBER_PARTITION] = 0x21a530;
			f[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS] = 0x218200;
			f[SWF_HOOK_PROPERTY_SET_FLAGS] = 0x147bee0;
			f[SWF_HOOK_PROPERTY_SET_MATERIAL] = 0x147bff0;
			f[SWF_HOOK_PROPERTIES_SET_FLAG] = 0x147c1e0;
			f[SWF_HOOK_PROPERTIES_SET_FADE_NODE] = 0x147c690;
			f[SWF_HOOK_PROPERTIES_PROJECTED_UV] = 0x14abd10;
			f[SWF_HOOK_SET_EXTERNAL_EMITTANCE] = 0x1480fc0;
			f[SWF_HOOK_SKY_SET_COLOR] = 0x40d970;
			f[SWF_HOOK_GEOMETRY_ATTACH_ALPHA] = 0x267cf0;
			f[SWF_HOOK_ALPHAS_SET_THRESHOLD] = 0x14ab770;
			f[SWF_HOOK_WEAPON_BLOOD_GEOMETRY] = 0x14ab580;
			f[SWF_HOOK_LAND_SETUP_PROPERTIES] = 0x2ad800;
			f[SWF_HOOK_LAND_BLEND_PARAMS] = 0x2ad0e0;
			f[SWF_HOOK_FADE_SNAP] = 0x147aa20;
			f[SWF_HOOK_ACTOR_UPDATE_3D_POSITION] = 0x69eb80;
			f[SWF_HOOK_ACTOR_SKY_CELL_SKIN] = 0x663430;
			f[SWF_HOOK_ACTOR_RAGDOLL_JOB] = 0x770dc0;
			f[SWF_HOOK_LOD_SEGMENT_SHOW] = 0xe31130;
			f[SWF_HOOK_LOD_SEGMENT_HIDE] = 0xe31160;
			f[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = 0xe310b0;
			f[SWF_HOOK_TREE_LOD_REFILL] = 0x504a40;
			f[SWF_HOOK_TREE_ANIM_ADD] = 0x4376f0;
			f[SWF_HOOK_TREE_ANIM_REMOVE] = 0x437840;
			f[SWF_HOOK_TREE_ANIM_CLEAR] = 0x437ae0;
			return f;
		}

		constexpr std::array<CallSites, SWF_HOOK_COUNT> Sites()
		{
			std::array<CallSites, SWF_HOOK_COUNT> s{};
			// Every call of the segment writers: the active and large-reference grids' segment updates
			// (FUN_1405110b0, FUN_1405112f0), two show-all callers (FUN_1404fdcf0, FUN_140509550), the
			// LOD block builder (FUN_140e55790).
			s[SWF_HOOK_LOD_SEGMENT_SHOW] = { { 0x511550, 0x511269 }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_HIDE] = { { 0x511544, 0x51125c }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = { { 0x4fde80, 0x509691, 0x511402, 0x5112b4, 0xe55ba3 }, 5 };
			// The refill's: the block's attach (FUN_140503630), its visibility update (FUN_140503f70).
			s[SWF_HOOK_TREE_LOD_REFILL] = { { 0x5037f6, 0x503fbf }, 2 };
			return s;
		}

		constexpr HookTargets kTargets{
			.niNode = { 0x19ab150, ".?AVNiNode@@" },
			.nodeOverrides = { {
				{ 0x19d4020, ".?AVBSParticleSystemManager@@" },
				{ 0x1784bb0, ".?AVBGSDecalNode@@" },
				{ 0x18466c0, ".?AVBSFaceGenNiNode@@" },
			} },
			.removeAllChildren = 0xd1d630,  // FUN_140d1d630: every child let go at once, bypassing the slots
			.niSwitchNode = { 0x19accd8, ".?AVNiSwitchNode@@" },
			.animationGraphPlace = 0x66afa0,  // FUN_14066afa0
			.reference3DPosition = 0x2d9800,  // TESObjectREFR::Update3DPosition
			.graphAnimation = 0x2f75a0,       // FUN_1402f75a0
			.selectiveUpdateCalls = { 0x2b42ba, 0x2b3bd5 },  // FUN_1402b41a0 (animated reference), FUN_1402b3ae0 (dynamic node)
			.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 },
			.functions = Functions(),
			.fadeNode = { 0x1aa60f0, ".?AVBSFadeNode@@" },
			.lightingProperty = { 0x1ab5938, ".?AVBSLightingShaderProperty@@" },
			.floatController = { 0x1ac3be8, ".?AVBSLightingShaderPropertyFloatController@@" },
			.colorController = { 0x1ac4168, ".?AVBSLightingShaderPropertyColorController@@" },
			.lightingMaterials = { {
				{ 0x185dc08, ".?AVBSLightingShaderMaterial@@" },
				{ 0x1ab6eb8, ".?AVBSLightingShaderMaterialBase@@" },
				{ 0x1ab6f30, ".?AVBSLightingShaderMaterialEnvmap@@" },
				{ 0x1ab6fa8, ".?AVBSLightingShaderMaterialEye@@" },
				{ 0x1ab7020, ".?AVBSLightingShaderMaterialGlowmap@@" },
				{ 0x1ab7098, ".?AVBSLightingShaderMaterialParallax@@" },
				{ 0x1ab7110, ".?AVBSLightingShaderMaterialParallaxOcc@@" },
				{ 0x1ab7188, ".?AVBSLightingShaderMaterialFacegen@@" },
				{ 0x1ab7200, ".?AVBSLightingShaderMaterialFacegenTint@@" },
				{ 0x1ab7278, ".?AVBSLightingShaderMaterialHairTint@@" },
				{ 0x1ab72f0, ".?AVBSLightingShaderMaterialLandscape@@" },
				{ 0x1ab7368, ".?AVBSLightingShaderMaterialLODLandscape@@" },
				{ 0x1ab73e0, ".?AVBSLightingShaderMaterialSnow@@" },
				{ 0x1ab7458, ".?AVBSLightingShaderMaterialMultiLayerParallax@@" },
			} },
			.callSites = Sites(),
			.actors = { {
				{ 0x189d870, ".?AVActor@@" },
				{ 0x18a5558, ".?AVCharacter@@" },
				{ 0x18ab9c0, ".?AVPlayerCharacter@@" },
			} },
			.decalArrayEdits = { 0x1fdfb0, 0x1fdc80, 0x1fdcb0, 0x1fdd50, 0x1fde40 },
			.instanceShape = { 0x19d2000, ".?AVBSMultiStreamInstanceTriShape@@" },
			.effectFloatController = { 0x1ac4428, ".?AVBSEffectShaderPropertyFloatController@@" },
			.effectColorController = { 0x1ac46c0, ".?AVBSEffectShaderPropertyColorController@@" },
			.effectMaterial = { 0x1abfa18, ".?AVBSEffectShaderMaterial@@" },
		};
	}

	const HookTargets& Targets()
	{
		return kTargets;
	}
}
