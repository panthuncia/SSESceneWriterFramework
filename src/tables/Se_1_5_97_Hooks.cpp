// SE 1.5.97 detour targets. Vtable offsets come from Address Library's version-1-5-97-0
// (CommonLib's VTABLE_* SE ids), and each is checked against its RTTI name before it is used.
// Every slot index the families use (structure 0x35/0x37-0x3C, fade 0x34, lighting 0x2A/0x31,
// controllers 0x27, materials 0x02/0x08-0x0A, actors 0xE1, instance shape 0x3C/0x3D, effect
// material 0x02, NiAVObject 0x2D) holds the same function on SE as on AE.
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::tables::se_1_5_97
{
	namespace
	{
		constexpr std::array<std::uint32_t, SWF_HOOK_COUNT> Functions()
		{
			std::array<std::uint32_t, SWF_HOOK_COUNT> f{};
			f[SWF_HOOK_FADE_UPDATE] = 0x1290050;
			f[SWF_HOOK_FADE_LOD_LEVEL] = 0x1290370;   // the LOD-distance part is split out into FUN_141290590 on SE
			f[SWF_HOOK_FADE_SET_RANGE] = 0x1290820;   // BSFadeNode::SetRange
			f[SWF_HOOK_FADE_SET_LOD_TYPE] = 0x1290870;
			f[SWF_HOOK_CELL_PLACE_REFERENCE] = 0x27ddc0;     // calls PLACE_REFERENCE_AT (AE inlines it)
			f[SWF_HOOK_CELL_PLACE_REFERENCE_AT] = 0x2811c0;
			f[SWF_HOOK_RIGID_BODY_MOTION_TYPE] = 0xa828c0;
			f[SWF_HOOK_RELEASE_COLLISION_OBJECT] = 0xda8fb0;  // havok visitor table entry (SE 0x1e08330)
			f[SWF_HOOK_SET_COLLISION_OBJECT] = 0x1cdda0;      // 5 callers on SE (AE inlines it into all but 2)
			f[SWF_HOOK_PREPEND_CONTROLLER] = 0xc60ee0;
			f[SWF_HOOK_REMOVE_CONTROLLER] = 0xc60fb0;         // NiObjectNET::RemoveController
			f[SWF_HOOK_HAVOK_NODE_TRANSFORM] = 0xdc5d20;
			f[SWF_HOOK_DISMEMBER_PARTITION] = 0x1cdd40;       // BSDismemberSkinInstance::UpdateDismemberPartion
			f[SWF_HOOK_BIPED_DISMEMBER_PARTITIONS] = 0x1cb980;
			f[SWF_HOOK_PROPERTY_SET_FLAGS] = 0x1291c30;       // BSShaderProperty::SetFlags
			f[SWF_HOOK_PROPERTY_SET_MATERIAL] = 0x1291d40;    // BSShaderProperty::SetMaterial
			f[SWF_HOOK_PROPERTIES_SET_FLAG] = 0x1291f30;      // calls SetFlags (AE inlines it): nested PROPERTY_SET_FLAGS
			f[SWF_HOOK_PROPERTIES_SET_FADE_NODE] = 0x12923f0;
			f[SWF_HOOK_PROPERTIES_PROJECTED_UV] = 0x12c4050;  // per-geometry body in FUN_1412c4640 on SE
			f[SWF_HOOK_SET_EXTERNAL_EMITTANCE] = 0x1296a60;   // per-geometry body in FUN_141296d40 on SE
			f[SWF_HOOK_SKY_SET_COLOR] = 0x3b4230;             // Sky::SetColor
			f[SWF_HOOK_GEOMETRY_ATTACH_ALPHA] = 0x218820;     // BSGeometry slot 0x27 (ICF-shared by ~17 vtables, as on AE)
			f[SWF_HOOK_ALPHAS_SET_THRESHOLD] = 0x12c3ba0;
			f[SWF_HOOK_WEAPON_BLOOD_GEOMETRY] = 0x12c3a00;    // per-geometry body in FUN_1412c43f0 on SE
			f[SWF_HOOK_LAND_SETUP_PROPERTIES] = 0x25b350;
			f[SWF_HOOK_LAND_BLEND_PARAMS] = 0x25ac20;
			f[SWF_HOOK_FADE_SNAP] = 0x1290890;
			f[SWF_HOOK_ACTOR_UPDATE_3D_POSITION] = 0x60abf0;  // Actor slot 0x11F (Ghidra's SE "Actor::UpdateActor3DPosition" label at 0x2a3700 is wrong)
			f[SWF_HOOK_ACTOR_SKY_CELL_SKIN] = 0x5d2cd0;
			f[SWF_HOOK_ACTOR_RAGDOLL_JOB] = 0x6dc210;
			f[SWF_HOOK_LOD_SEGMENT_SHOW] = 0xd593e0;
			f[SWF_HOOK_LOD_SEGMENT_HIDE] = 0xd59410;
			f[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = 0xd59360;
			f[SWF_HOOK_TREE_LOD_REFILL] = 0x4a8a60;
			f[SWF_HOOK_TREE_ANIM_ADD] = 0x3dd7b0;
			f[SWF_HOOK_TREE_ANIM_REMOVE] = 0x3dd8d0;
			f[SWF_HOOK_TREE_ANIM_CLEAR] = 0x3dda30;
			f[SWF_HOOK_HAVOK_INIT] = 0xda7af0;                // bhkWorld slot 0x36
			f[SWF_HOOK_REFERENCE_DISABLE] = 0x9a93e0;
			f[SWF_HOOK_ENABLE_COMMAND] = 0x2f6210;
			f[SWF_HOOK_FLORA_HARVEST_ALPHAS] = 0x19cf90;      // BSVisit::TraverseScenegraphGeometries<FloraAlphaTestSetter>
			return f;
		}

		constexpr std::array<CallSites, SWF_HOOK_COUNT> Sites()
		{
			std::array<CallSites, SWF_HOOK_COUNT> s{};
			// Every call of the segment writers (full .text rel32 scan): the active and large-reference grids'
			// segment updates (FUN_1404b5090, FUN_1404b5220), two show-all callers (BGSDistantObjectBlock::Prepare
			// FUN_1404a1b90, BGSTerrainChunk::Attach), the LOD block builder (FUN_140d7a6d0). Same callers as AE,
			// but FUN_1404b5220 calls the hide writer twice (the compiler duplicated AE's single hide branch).
			s[SWF_HOOK_LOD_SEGMENT_SHOW] = { { 0x4b5425, 0x4b51a6 }, 2 };
			s[SWF_HOOK_LOD_SEGMENT_HIDE] = { { 0x4b53cf, 0x4b5419, 0x4b519a }, 3 };
			s[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = { { 0x4a1f16, 0x4ada85, 0x4b5322, 0x4b51e4, 0xd7aae3 }, 5 };
			// The refill's: the block's attach (BGSDistantTreeBlock::Attach FUN_1404a76d0), its visibility
			// update (FUN_1404a8010, labelled UpdateShaderProperties in Ghidra).
			s[SWF_HOOK_TREE_LOD_REFILL] = { { 0x4a7894, 0x4a805f }, 2 };
			return s;
		}

		constexpr HookTargets kTargets{
			.niNode = { 0x1764d50, ".?AVNiNode@@" },
			.nodeOverrides = { {
				{ 0x178bde0, ".?AVBSParticleSystemManager@@" },
				{ 0x15407c8, ".?AVBGSDecalNode@@" },
				{ 0x16018c8, ".?AVBSFaceGenNiNode@@" },
			} },
			.removeAllChildren = 0xc58020,  // FUN_140c58020: every child let go at once, bypassing the slots
			.niSwitchNode = { 0x17668a8, ".?AVNiSwitchNode@@" },
			.animationGraphPlace = 0x5d9ac0,  // FUN_1405d9ac0 (builds the placement functor AE inlines)
			.reference3DPosition = 0x286130,  // TESObjectREFR::Update3DPosition (slot 0x3F)
			.graphAnimation = 0x2a3e60,       // FUN_1402a3e60
			.selectiveUpdateCalls = { 0x2611af, 0x260ac5 },  // FUN_140261090 (animated reference), FUN_1402609d0 (dynamic node)
			.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 },
			.functions = Functions(),
			.fadeNode = { 0x184cb50, ".?AVBSFadeNode@@" },
			.lightingProperty = { 0x185c600, ".?AVBSLightingShaderProperty@@" },
			.floatController = { 0x186a5f8, ".?AVBSLightingShaderPropertyFloatController@@" },
			.colorController = { 0x186ab28, ".?AVBSLightingShaderPropertyColorController@@" },
			.lightingMaterials = { {
				{ 0x1619298, ".?AVBSLightingShaderMaterial@@" },
				{ 0x185dbc8, ".?AVBSLightingShaderMaterialBase@@" },
				{ 0x185dc40, ".?AVBSLightingShaderMaterialEnvmap@@" },
				{ 0x185dcb8, ".?AVBSLightingShaderMaterialEye@@" },
				{ 0x185dd30, ".?AVBSLightingShaderMaterialGlowmap@@" },
				{ 0x185dda8, ".?AVBSLightingShaderMaterialParallax@@" },
				{ 0x185de20, ".?AVBSLightingShaderMaterialParallaxOcc@@" },
				{ 0x185de98, ".?AVBSLightingShaderMaterialFacegen@@" },
				{ 0x185df10, ".?AVBSLightingShaderMaterialFacegenTint@@" },
				{ 0x185df88, ".?AVBSLightingShaderMaterialHairTint@@" },
				{ 0x185e000, ".?AVBSLightingShaderMaterialLandscape@@" },
				{ 0x185e078, ".?AVBSLightingShaderMaterialLODLandscape@@" },
				{ 0x185e0f0, ".?AVBSLightingShaderMaterialSnow@@" },
				{ 0x185e168, ".?AVBSLightingShaderMaterialMultiLayerParallax@@" },
			} },
			.callSites = Sites(),
			.actors = { {
				{ 0x1655640, ".?AVActor@@" },
				{ 0x165da40, ".?AVCharacter@@" },
				{ 0x16635e0, ".?AVPlayerCharacter@@" },
			} },
			// FUN_1401b1780 append (a 12-byte "add rcx,0x128; jmp FUN_1401b1790" thunk), FUN_1401b13f0 erase,
			// FUN_1401b1550 the geometry decal's update (also called from the two erase+detach variants on SE),
			// FUN_1401b1600 and FUN_1401b1680 erase + detach.
			.decalArrayEdits = { 0x1b1780, 0x1b13f0, 0x1b1550, 0x1b1600, 0x1b1680 },
			.instanceShape = { 0x1789ef0, ".?AVBSMultiStreamInstanceTriShape@@" },
			.effectFloatController = { 0x186adc0, ".?AVBSEffectShaderPropertyFloatController@@" },
			.effectColorController = { 0x186b030, ".?AVBSEffectShaderPropertyColorController@@" },
			.effectMaterial = { 0x18664f8, ".?AVBSEffectShaderMaterial@@" },
			.scene = { .flags = 0xF4, .alphaProperty = 0x120, .shaderProperty = 0x128, .alphaFlags = 0x30, .refCount = 0x8, .deleteThisSlot = 1 },
		};
	}

	const HookTargets& Targets()
	{
		return kTargets;
	}
}
