// SE 1.5.97 detour targets as Address Library ids (version-1-5-97-0; the vtables are CommonLib's VTABLE_* SE ids), and the call
// sites' positions in their functions. Each vtable is checked against its RTTI name before it is used.
// Every slot index the families use (structure 0x35/0x37-0x3C, fade 0x34, lighting 0x2A/0x31,
// controllers 0x27, materials 0x02/0x08-0x0A, actors 0xE1, instance shape 0x3C/0x3D, effect
// material 0x02, NiAVObject 0x2D) holds the same function on SE as on AE.
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::tables::se_1_5_97
{
	namespace
	{
		constexpr std::array<std::uint64_t, SWF_HOOK_COUNT> Functions()
		{
			std::array<std::uint64_t, SWF_HOOK_COUNT> f{};
			f[SWF_HOOK_FADE_UPDATE] = 98849;
			f[SWF_HOOK_FADE_LOD_LEVEL] = 98850;   // the LOD-distance part is split out into FUN_141290590 on SE
			f[SWF_HOOK_FADE_SET_RANGE] = 98854;   // BSFadeNode::SetRange
			f[SWF_HOOK_FADE_SET_LOD_TYPE] = 98855;
			f[SWF_HOOK_CELL_PLACE_REFERENCE] = 19002;     // calls PLACE_REFERENCE_AT (AE inlines it)
			f[SWF_HOOK_CELL_PLACE_REFERENCE_AT] = 19036;
			f[SWF_HOOK_RIGID_BODY_MOTION_TYPE] = 60660;
			f[SWF_HOOK_RELEASE_COLLISION_OBJECT] = 76055;  // havok visitor table entry (SE 0x1e08330)
			f[SWF_HOOK_SET_COLLISION_OBJECT] = 15577;      // 5 callers on SE (AE inlines it into all but 2)
			f[SWF_HOOK_PREPEND_CONTROLLER] = 69154;
			f[SWF_HOOK_REMOVE_CONTROLLER] = 69156;         // NiObjectNET::RemoveController
			f[SWF_HOOK_HAVOK_NODE_TRANSFORM] = 76538;
			f[SWF_HOOK_DISMEMBER_PARTITION] = 15576;       // BSDismemberSkinInstance::UpdateDismemberPartion
			f[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS] = 15538;
			f[SWF_HOOK_PROPERTY_SET_FLAGS] = 98893;       // BSShaderProperty::SetFlags
			f[SWF_HOOK_PROPERTY_SET_MATERIAL] = 98897;    // BSShaderProperty::SetMaterial
			f[SWF_HOOK_PROPERTIES_SET_FLAG] = 98901;      // calls SetFlags (AE inlines it): nested PROPERTY_SET_FLAGS
			f[SWF_HOOK_PROPERTIES_SET_FADE_NODE] = 98924;
			f[SWF_HOOK_PROPERTIES_PROJECTED_UV] = 99830;  // per-geometry body in FUN_1412c4640 on SE
			f[SWF_HOOK_SET_EXTERNAL_EMITTANCE] = 99004;   // per-geometry body in FUN_141296d40 on SE
			f[SWF_HOOK_SKY_SET_COLOR] = 25691;             // Sky::SetColor
			f[SWF_HOOK_GEOMETRY_ATTACH_ALPHA] = 16967;     // BSGeometry slot 0x27 (ICF-shared by ~17 vtables, as on AE)
			f[SWF_HOOK_ALPHAS_SET_THRESHOLD] = 99824;
			f[SWF_HOOK_WEAPON_BLOOD_GEOMETRY] = 99822;    // per-geometry body in FUN_1412c43f0 on SE
			f[SWF_HOOK_LAND_SETUP_PROPERTIES] = 18368;
			f[SWF_HOOK_LAND_BLEND_PARAMS] = 18366;
			f[SWF_HOOK_FADE_SNAP] = 98856;
			f[SWF_HOOK_ACTOR_UPDATE_3D_POSITION] = 36970;  // Actor slot 0x11F (Ghidra's SE "Actor::UpdateActor3DPosition" label at 0x2a3700 is wrong)
			f[SWF_HOOK_ACTOR_SKY_CELL_SKIN] = 36296;
			f[SWF_HOOK_ACTOR_RAGDOLL_JOB] = 40360;
			f[SWF_HOOK_LOD_SEGMENT_SHOW] = 75027;
			f[SWF_HOOK_LOD_SEGMENT_HIDE] = 75028;
			f[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = 75026;
			f[SWF_HOOK_TREE_LOD_REFILL] = 30844;
			f[SWF_HOOK_TREE_ANIM_ADD] = 26493;
			f[SWF_HOOK_TREE_ANIM_REMOVE] = 26494;
			f[SWF_HOOK_TREE_ANIM_CLEAR] = 26495;
			f[SWF_HOOK_HAVOK_INIT] = 76030;                // bhkWorld slot 0x36
			f[SWF_HOOK_REFERENCE_DISABLE] = 56037;
			f[SWF_HOOK_ENABLE_COMMAND] = 21613;
			f[SWF_HOOK_FLORA_HARVEST_ALPHAS] = 14695;      // BSVisit::TraverseScenegraphGeometries<FloraAlphaTestSetter>
			return f;
		}

		constexpr std::array<CallSiteIds, SWF_HOOK_COUNT> Sites()
		{
			std::array<CallSiteIds, SWF_HOOK_COUNT> s{};
			// Every call of the segment writers (full .text rel32 scan): the active and large-reference grids'
			// segment updates (FUN_1404b5090, FUN_1404b5220), two show-all callers (BGSDistantObjectBlock::Prepare
			// FUN_1404a1b90, BGSTerrainChunk::Attach), the LOD block builder (FUN_140d7a6d0). Same callers as AE,
			// but FUN_1404b5220 calls the hide writer twice (the compiler duplicated AE's single hide branch).
			s[SWF_HOOK_LOD_SEGMENT_SHOW] = { { { { 31060, 0x205 }, { 31059, 0x116 } } }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_HIDE] = { { { { 31060, 0x1af }, { 31060, 0x1f9 }, { 31059, 0x10a } } }, 3 };
			s[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = { { { { 30746, 0x386 }, { 30934, 0x145 }, { 31060, 0x102 }, { 31059, 0x154 }, { 75682, 0x413 } } }, 5 };
			// The refill's: the block's attach (BGSDistantTreeBlock::Attach FUN_1404a76d0), its visibility
			// update (FUN_1404a8010, labelled UpdateShaderProperties in Ghidra).
			s[SWF_HOOK_TREE_LOD_REFILL] = { { { { 30832, 0x1c4 }, { 30838, 0x4f } } }, 2 };
			return s;
		}

		constexpr HookIds kIds{
			.niNode = { 285992, ".?AVNiNode@@" },
			.nodeOverrides = { {
				{ 288102, ".?AVBSParticleSystemManager@@" },
				{ 231907, ".?AVBGSDecalNode@@" },
				{ 252410, ".?AVBSFaceGenNiNode@@" },
			} },
			.removeAllChildren = 68944,  // FUN_140c58020: every child let go at once, bypassing the slots
			.niSwitchNode = { 286145, ".?AVNiSwitchNode@@" },
			.animationGraphPlace = 36373,  // FUN_1405d9ac0 (builds the placement functor AE inlines)
			.reference3DPosition = 19115,  // TESObjectREFR::Update3DPosition (slot 0x3F)
			.graphAnimation = 19726,       // FUN_1402a3e60
			.projectileUpdate3D = 42946,   // Projectile::Update3D (slot 0xAD; ArrowProjectile's tail-jumps into it)
			.selectiveUpdateCalls = { { { 18463, 0x11f }, { 18458, 0xf5 } } },  // FUN_140261090 (animated reference), FUN_1402609d0 (dynamic node)
			.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 },
			.functions = Functions(),
			.fadeNode = { 302849, ".?AVBSFadeNode@@" },
			.lightingProperty = { 304424, ".?AVBSLightingShaderProperty@@" },
			.floatController = { 305820, ".?AVBSLightingShaderPropertyFloatController@@" },
			.colorController = { 305839, ".?AVBSLightingShaderPropertyColorController@@" },
			.lightingMaterials = { {
				{ 254404, ".?AVBSLightingShaderMaterial@@" },
				{ 304554, ".?AVBSLightingShaderMaterialBase@@" },
				{ 304555, ".?AVBSLightingShaderMaterialEnvmap@@" },
				{ 304556, ".?AVBSLightingShaderMaterialEye@@" },
				{ 304557, ".?AVBSLightingShaderMaterialGlowmap@@" },
				{ 304558, ".?AVBSLightingShaderMaterialParallax@@" },
				{ 304559, ".?AVBSLightingShaderMaterialParallaxOcc@@" },
				{ 304560, ".?AVBSLightingShaderMaterialFacegen@@" },
				{ 304561, ".?AVBSLightingShaderMaterialFacegenTint@@" },
				{ 304562, ".?AVBSLightingShaderMaterialHairTint@@" },
				{ 304563, ".?AVBSLightingShaderMaterialLandscape@@" },
				{ 304564, ".?AVBSLightingShaderMaterialLODLandscape@@" },
				{ 304565, ".?AVBSLightingShaderMaterialSnow@@" },
				{ 304566, ".?AVBSLightingShaderMaterialMultiLayerParallax@@" },
			} },
			.callSites = Sites(),
			.actors = { {
				{ 260538, ".?AVActor@@" },
				{ 261397, ".?AVCharacter@@" },
				{ 261916, ".?AVPlayerCharacter@@" },
			} },
			// FUN_1401b1780 append (a 12-byte "add rcx,0x128; jmp FUN_1401b1790" thunk), FUN_1401b13f0 erase,
			// FUN_1401b1550 the geometry decal's update (also called from the two erase+detach variants on SE),
			// FUN_1401b1600 and FUN_1401b1680 erase + detach.
			.decalArrayEdits = { 15124, 15118, 15119, 15120, 15121 },
			.instanceShape = { 287975, ".?AVBSMultiStreamInstanceTriShape@@" },
			.effectFloatController = { 305844, ".?AVBSEffectShaderPropertyFloatController@@" },
			.effectColorController = { 305848, ".?AVBSEffectShaderPropertyColorController@@" },
			.effectMaterial = { 305442, ".?AVBSEffectShaderMaterial@@" },
			.scene = { .flags = 0xF4, .alphaProperty = 0x120, .shaderProperty = 0x128, .alphaFlags = 0x30, .refCount = 0x8, .deleteThisSlot = 1 },
		};
	}

	const HookIds& Ids()
	{
		return kIds;
	}
}
