// AE detour targets as Address Library ids (versionlib-1-6-1170-0; the vtables are CommonLib's VTABLE_* AE ids), and the call
// sites' positions in 1.6.1170's functions (Ae_1_7_104_Hooks.cpp moves the ones 1.7.104 moved). Each vtable is checked against
// its RTTI name before it is used.
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::tables::ae_1_6_1170
{
	namespace
	{
		constexpr std::array<std::uint64_t, SWF_HOOK_COUNT> Functions()
		{
			std::array<std::uint64_t, SWF_HOOK_COUNT> f{};
			f[SWF_HOOK_FADE_UPDATE] = 105486;
			f[SWF_HOOK_FADE_LOD_LEVEL] = 105487;
			f[SWF_HOOK_FADE_SET_RANGE] = 105491;
			f[SWF_HOOK_FADE_SET_LOD_TYPE] = 105492;
			f[SWF_HOOK_CELL_PLACE_REFERENCE] = 19413;
			f[SWF_HOOK_CELL_PLACE_REFERENCE_AT] = 19450;
			f[SWF_HOOK_RIGID_BODY_MOTION_TYPE] = 61518;
			f[SWF_HOOK_RELEASE_COLLISION_OBJECT] = 77888;
			f[SWF_HOOK_SET_COLLISION_OBJECT] = 77979;
			f[SWF_HOOK_PREPEND_CONTROLLER] = 70515;
			f[SWF_HOOK_REMOVE_CONTROLLER] = 70517;
			f[SWF_HOOK_HAVOK_NODE_TRANSFORM] = 78382;
			f[SWF_HOOK_DISMEMBER_PARTITION] = 15753;
			f[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS] = 15715;
			f[SWF_HOOK_PROPERTY_SET_FLAGS] = 105540;
			f[SWF_HOOK_PROPERTY_SET_MATERIAL] = 105544;
			f[SWF_HOOK_PROPERTIES_SET_FLAG] = 105548;
			f[SWF_HOOK_PROPERTIES_SET_FADE_NODE] = 105575;
			f[SWF_HOOK_PROPERTIES_PROJECTED_UV] = 106480;
			f[SWF_HOOK_SET_EXTERNAL_EMITTANCE] = 105659;
			f[SWF_HOOK_SKY_SET_COLOR] = 26238;
			f[SWF_HOOK_GEOMETRY_ATTACH_ALPHA] = 17348;
			f[SWF_HOOK_ALPHAS_SET_THRESHOLD] = 106474;
			f[SWF_HOOK_WEAPON_BLOOD_GEOMETRY] = 106472;
			f[SWF_HOOK_LAND_SETUP_PROPERTIES] = 18791;
			f[SWF_HOOK_LAND_BLEND_PARAMS] = 18789;
			f[SWF_HOOK_FADE_SNAP] = 105493;
			f[SWF_HOOK_ACTOR_UPDATE_3D_POSITION] = 37995;
			f[SWF_HOOK_ACTOR_SKY_CELL_SKIN] = 37286;
			f[SWF_HOOK_ACTOR_RAGDOLL_JOB] = 41373;
			f[SWF_HOOK_LOD_SEGMENT_SHOW] = 76789;
			f[SWF_HOOK_LOD_SEGMENT_HIDE] = 76790;
			f[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = 76788;
			f[SWF_HOOK_TREE_LOD_REFILL] = 31665;
			f[SWF_HOOK_TREE_ANIM_ADD] = 27088;
			f[SWF_HOOK_TREE_ANIM_REMOVE] = 27089;
			f[SWF_HOOK_TREE_ANIM_CLEAR] = 27090;
			f[SWF_HOOK_HAVOK_INIT] = 77863;
			f[SWF_HOOK_REFERENCE_DISABLE] = 56491;
			f[SWF_HOOK_ENABLE_COMMAND] = 22095;
			f[SWF_HOOK_FLORA_HARVEST_ALPHAS] = 14867;
			return f;
		}

		constexpr std::array<CallSiteIds, SWF_HOOK_COUNT> Sites()
		{
			std::array<CallSiteIds, SWF_HOOK_COUNT> s{};
			// Every call of the segment writers: the active and large-reference grids' segment updates
			// (FUN_1405110b0, FUN_1405112f0), two show-all callers (FUN_1404fdcf0, FUN_140509550), the
			// LOD block builder (FUN_140e55790).
			s[SWF_HOOK_LOD_SEGMENT_SHOW] = { { { { 31847, 0x260 }, { 31846, 0x1b9 } } }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_HIDE] = { { { { 31847, 0x254 }, { 31846, 0x1ac } } }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = { { { { 31587, 0x190 }, { 31737, 0x141 }, { 31847, 0x112 }, { 31846, 0x204 }, { 77492, 0x413 } } }, 5 };
			// The refill's: the block's attach (FUN_140503630), its visibility update (FUN_140503f70).
			s[SWF_HOOK_TREE_LOD_REFILL] = { { { { 31653, 0x1c6 }, { 31659, 0x4f } } }, 2 };
			return s;
		}

		constexpr HookIds kIds{
			.niNode = { 237098, ".?AVNiNode@@" },
			.nodeOverrides = { {
				{ 238426, ".?AVBSParticleSystemManager@@" },
				{ 188109, ".?AVBGSDecalNode@@" },
				{ 200333, ".?AVBSFaceGenNiNode@@" },
			} },
			.removeAllChildren = 70295,  // FUN_140d1d630: every child let go at once, bypassing the slots
			.niSwitchNode = { 237185, ".?AVNiSwitchNode@@" },
			.animationGraphPlace = 37364,  // FUN_14066afa0
			.reference3DPosition = 19517,  // TESObjectREFR::Update3DPosition
			.graphAnimation = 20120,       // FUN_1402f75a0
			.projectileUpdate3D = 44126,   // Projectile::Update3D (slot 0xAD; ArrowProjectile's tail-jumps into it)
			.selectiveUpdateCalls = { { { 18894, 0x11a }, { 18889, 0xf5 } } },  // FUN_1402b41a0 (animated reference), FUN_1402b3ae0 (dynamic node)
			.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 },
			.functions = Functions(),
			.fadeNode = { 252901, ".?AVBSFadeNode@@" },
			.lightingProperty = { 254668, ".?AVBSLightingShaderProperty@@" },
			.floatController = { 255429, ".?AVBSLightingShaderPropertyFloatController@@" },
			.colorController = { 255443, ".?AVBSLightingShaderPropertyColorController@@" },
			.lightingMaterials = { {
				{ 204134, ".?AVBSLightingShaderMaterial@@" },
				{ 254737, ".?AVBSLightingShaderMaterialBase@@" },
				{ 254739, ".?AVBSLightingShaderMaterialEnvmap@@" },
				{ 254741, ".?AVBSLightingShaderMaterialEye@@" },
				{ 254743, ".?AVBSLightingShaderMaterialGlowmap@@" },
				{ 254745, ".?AVBSLightingShaderMaterialParallax@@" },
				{ 254747, ".?AVBSLightingShaderMaterialParallaxOcc@@" },
				{ 254749, ".?AVBSLightingShaderMaterialFacegen@@" },
				{ 254751, ".?AVBSLightingShaderMaterialFacegenTint@@" },
				{ 254753, ".?AVBSLightingShaderMaterialHairTint@@" },
				{ 254755, ".?AVBSLightingShaderMaterialLandscape@@" },
				{ 254757, ".?AVBSLightingShaderMaterialLODLandscape@@" },
				{ 254759, ".?AVBSLightingShaderMaterialSnow@@" },
				{ 254761, ".?AVBSLightingShaderMaterialMultiLayerParallax@@" },
			} },
			.callSites = Sites(),
			.actors = { {
				{ 207511, ".?AVActor@@" },
				{ 207886, ".?AVCharacter@@" },
				{ 208040, ".?AVPlayerCharacter@@" },
			} },
			.decalArrayEdits = { 15294, 15288, 15289, 15290, 15291 },
			.instanceShape = { 238363, ".?AVBSMultiStreamInstanceTriShape@@" },
			.effectFloatController = { 255445, ".?AVBSEffectShaderPropertyFloatController@@" },
			.effectColorController = { 255447, ".?AVBSEffectShaderPropertyColorController@@" },
			.effectMaterial = { 255190, ".?AVBSEffectShaderMaterial@@" },
			.scene = { .flags = 0xF4, .alphaProperty = 0x120, .shaderProperty = 0x128, .alphaFlags = 0x30, .refCount = 0x8, .deleteThisSlot = 1 },
		};
	}

	const HookIds& Ids()
	{
		return kIds;
	}
}
