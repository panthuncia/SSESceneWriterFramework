/*
 * SceneWriterFramework (SWF) public C ABI.
 *
 * Every copy of SWF in the process (the standalone SceneWriterFramework.dll, or a copy
 * embedded in another mod) is a candidate. At SKSE kPostLoad the copies elect one
 * provider, which hooks the engine's scene writers and serves this API to everyone.
 *
 * ABI rules (enforced by tests/abi_layout.cpp):
 *  - Within an ABI major, structs and the SWF_Api table only grow at the end.
 *  - Every struct begins with a uint32_t size; readers use only fields inside it.
 *  - Event, filter, capability and field ids are append-only; unknown ids are ignored.
 *  - Nothing here uses C++ types, CRT ownership or exceptions across the boundary.
 *
 * Until SWF 1.0.0 is tagged the ABI is pre-release and may still change.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWF_ABI_MAJOR 1
#define SWF_ABI_MINOR 2 /* 1: CheckIntegrity; 2: Write */

/* Frozen: name of the export every candidate provides. */
#define SWF_CANDIDATE_EXPORT "SWF_Candidate_v1"

/* Packed implementation version: major.minor.patch.build, 16 bits each. */
#define SWF_MAKE_VERSION(major, minor, patch, build) \
	((((uint64_t)(major)) << 48) | (((uint64_t)(minor)) << 32) | (((uint64_t)(patch)) << 16) | ((uint64_t)(build)))

typedef int32_t SWF_Result;
#define SWF_OK 0
#define SWF_E_NOT_READY 1   /* no provider elected yet */
#define SWF_E_INVALID 2     /* bad argument */
#define SWF_E_UNSUPPORTED 3 /* id or runtime this provider does not support */
#define SWF_E_FAILED 4      /* activation or install failed */
#define SWF_E_DISABLED 5    /* disabled by SceneWriterFramework.ini */
#define SWF_E_SUPERSEDED 6  /* another candidate is already the provider */
#define SWF_E_FULL 7        /* reserved: SWF has no subscriber limit */

/* Game runtimes a candidate has address tables for (bit per runtime). */
#define SWF_RUNTIME_AE_1_6_1170 (1ull << 0)

/* Candidate states reported by SWF_CandidateInfo::GetState. */
#define SWF_STATE_IDLE 0
#define SWF_STATE_ACTIVE 1
#define SWF_STATE_FAILED 2

/* Capability bits: one per hook family that installed successfully. */
typedef uint64_t SWF_Caps;
#define SWF_CAP_HIDDEN (1ull << 0)       /* SWF_EVENT_HIDDEN, SWF_FILTER_SHOW */
#define SWF_CAP_SWITCH_INDEX (1ull << 1) /* SWF_EVENT_SWITCH_INDEX */
#define SWF_CAP_FADE_RESET (1ull << 2)   /* SWF_EVENT_FADE_RESET */
#define SWF_CAP_STRUCTURE (1ull << 3)          /* SWF_EVENT_CHILD_ATTACHED, SWF_EVENT_CHILD_DETACHING */
#define SWF_CAP_SWITCH_STRUCTURAL (1ull << 4)  /* SWF_EVENT_SWITCH_STRUCTURAL */
#define SWF_CAP_MOVES (1ull << 5)              /* SWF_EVENT_MOVED */
#define SWF_CAP_FADE_CALLS (1ull << 6)         /* SWF_HOOK_FADE_* */
#define SWF_CAP_NODE_CALLS (1ull << 7)         /* SWF_HOOK_CELL_*, collision (and SWF_HOOK_HAVOK_INIT), controllers, dismember */
#define SWF_CAP_PROPERTY_CALLS (1ull << 8)     /* SWF_HOOK_PROPERTY_*, SWF_HOOK_LIGHTING_*, emittance */
#define SWF_CAP_GEOMETRY_CALLS (1ull << 9)     /* alpha, weapon blood, land */
#define SWF_CAP_MATERIAL_CALLS (1ull << 10)    /* controllers' and materials' in-place writes */
#define SWF_CAP_ACTOR_CALLS (1ull << 11)       /* SWF_HOOK_ACTOR_* */
#define SWF_CAP_DECAL_CALLS (1ull << 12)       /* SWF_HOOK_DECAL_ARRAY_EDIT */
#define SWF_CAP_LOD_SEGMENT_CALLS (1ull << 13) /* SWF_HOOK_LOD_SEGMENT_* */
#define SWF_CAP_TREE_LOD_CALLS (1ull << 14)    /* SWF_HOOK_TREE_LOD_REFILL, SWF_HOOK_INSTANCE_* */
#define SWF_CAP_TREE_ANIM_CALLS (1ull << 15)   /* SWF_HOOK_TREE_ANIM_*, SWF_FILTER_CALL(SWF_HOOK_TREE_ANIM_REMOVE) */
#define SWF_CAP_EFFECT_CALLS (1ull << 16)      /* SWF_HOOK_EFFECT_* */

/* Subscription phases. Function-hook events fire PRE and POST around the engine call;
 * store events fire POST only. A subscription may ask for both. */
#define SWF_PHASE_PRE (1u << 0)
#define SWF_PHASE_POST (1u << 1)

/* Event ids. */
#define SWF_EVENT_WRITTEN 1      /* SWF_WrittenEvent: a client reported a direct write */
#define SWF_EVENT_HIDDEN 2       /* SWF_HiddenEvent: an engine store changed NiAVObject::flags bit 0 */
#define SWF_EVENT_SWITCH_INDEX 3 /* SWF_SwitchIndexEvent: an engine store changed NiSwitchNode::index */
#define SWF_EVENT_FADE_RESET 4   /* SWF_FadeResetEvent: an engine store reset BSFadeNode::currentFade */
#define SWF_EVENT_CHILD_ATTACHED 5    /* SWF_ChildEvent, after an NiNode child edit attached `child` */
#define SWF_EVENT_CHILD_DETACHING 6   /* SWF_ChildEvent, before an NiNode child edit detaches `child` */
#define SWF_EVENT_SWITCH_STRUCTURAL 7 /* SWF_NodeEvent, after one of NiSwitchNode's own child edits */
#define SWF_EVENT_MOVED 8             /* SWF_MovedEvent, after an engine writer moved a reference or node */
#define SWF_EVENT_SCENE_WRITE 9       /* SWF_SceneWriteEvent, PRE and POST around a plugin's SWF_Api::Write */

/* Filter ids. A filter runs before the write and may veto it. */
#define SWF_FILTER_SHOW 1 /* SWF_ShowFilterEvent: an engine LOD store about to clear kHidden */
/* SWF_CallEvent before a vetoable engine call (marked "vetoable" in the hook list), with its
 * arguments. A veto skips the call and both its events; the caller gets 0 back. */
#define SWF_FILTER_CALL_BASE 64
#define SWF_FILTER_CALL(hook) (SWF_FILTER_CALL_BASE + (hook))

/* Field bits shared by events that describe which fields of an object changed. */
typedef uint64_t SWF_Fields;
#define SWF_FIELD_HIDDEN (1ull << 0)
#define SWF_FIELD_TRANSFORM (1ull << 1)
#define SWF_FIELD_FADE (1ull << 2)
#define SWF_FIELD_SWITCH_INDEX (1ull << 3)
#define SWF_FIELD_CHILDREN (1ull << 4)
#define SWF_FIELD_PROPERTIES (1ull << 5)
#define SWF_FIELD_SHADER_FLAGS (1ull << 6)
#define SWF_FIELD_MATERIAL (1ull << 7)

typedef struct SWF_EventHeader
{
	uint32_t size;
	uint32_t id;
	uint32_t phase;    /* SWF_PHASE_PRE or SWF_PHASE_POST */
	uint32_t threadId; /* writer thread */
	uint64_t sequence; /* increasing per writer thread; PRE and POST of one call share it */
} SWF_EventHeader;

typedef struct SWF_WrittenEvent
{
	SWF_EventHeader header;
	void* object; /* engine object written (NiAVObject*, BSShaderProperty*, material...) */
	SWF_Fields fields;
} SWF_WrittenEvent;

/* Scene writes (SWF_Api::Write, abiMinor >= 2): fields a participating plugin writes through SWF instead of
 * storing them itself. SWF makes the store on the calling thread between SWF_EVENT_SCENE_WRITE's PRE and POST,
 * so every subscriber sees the plugin's write as it sees the engine's own writers'. A property is reference
 * counted as the engine's NiPointer counts it: the new one gains a reference before the store, and the old one
 * loses the field's after POST, so subscribers may read either during both phases. A write that changes
 * nothing stores nothing and fires nothing. */
#define SWF_WRITE_HIDDEN 1          /* NiAVObject*: flags bit 0 (kHidden); value 0 or 1 */
#define SWF_WRITE_SHADER_PROPERTY 2 /* BSGeometry*: its shader property; value a BSShaderProperty* (0 clears) */
#define SWF_WRITE_ALPHA_PROPERTY 3  /* BSGeometry*: its alpha property; value an NiAlphaProperty* (0 clears) */
#define SWF_WRITE_ALPHA_FLAGS 4     /* NiAlphaProperty*: its flags (blend, test and their modes); value the 16 bits */

typedef struct SWF_SceneWriteEvent
{
	SWF_EventHeader header;
	uint32_t kind; /* SWF_WRITE_* */
	uint32_t reserved;
	void* object;
	uint64_t before; /* the field's value before the store: a pointer, or the bit */
	uint64_t after;  /* the value written */
} SWF_SceneWriteEvent;

/* Engine stores. `site` indexes the provider's table for that event (stable within one
 * implVersion; SiteCount gives the count); `siteOffset` is the store's offset from the
 * game executable's image base. */
typedef struct SWF_HiddenEvent
{
	SWF_EventHeader header;
	void* object; /* NiAVObject*, after the store */
	uint32_t site;
	uint32_t siteOffset;
	uint32_t hidden; /* bit 0's value after the store */
	uint32_t reserved;
} SWF_HiddenEvent;

typedef struct SWF_SwitchIndexEvent
{
	SWF_EventHeader header;
	void* node; /* NiSwitchNode*, after the store */
	int32_t before;
	int32_t after;
	uint32_t site;
	uint32_t siteOffset;
} SWF_SwitchIndexEvent;

typedef struct SWF_FadeResetEvent
{
	SWF_EventHeader header;
	void* node; /* BSFadeNode*, after the store */
	float value; /* currentFade stored: 0 or 1 */
	uint32_t site;
	uint32_t siteOffset;
	uint32_t reserved;
} SWF_FadeResetEvent;

typedef struct SWF_ShowFilterEvent
{
	SWF_EventHeader header; /* header.id is SWF_FILTER_SHOW */
	void* object; /* NiAVObject* about to be shown; a veto leaves it hidden, with no event */
	uint32_t site;
	uint32_t siteOffset;
} SWF_ShowFilterEvent;

typedef struct SWF_ChildEvent
{
	SWF_EventHeader header;
	void* parent; /* NiNode* */
	void* child;  /* NiAVObject*; may be null (an empty slot set or detached) */
	uint32_t unchanged; /* CHILD_ATTACHED from SetAt: the slot already held `child` */
	uint32_t reserved;
} SWF_ChildEvent;

typedef struct SWF_NodeEvent
{
	SWF_EventHeader header;
	void* node;
} SWF_NodeEvent;

/* Writers of SWF_EVENT_MOVED. */
#define SWF_MOVE_ANIMATION_GRAPH_PLACE 0  /* FUN_14066afa0: the animation graph placed a reference */
#define SWF_MOVE_REFERENCE_3D_POSITION 1  /* TESObjectREFR::Update3DPosition */
#define SWF_MOVE_GRAPH_ANIMATION 2        /* FUN_1402f75a0: a reference's graph animation */
#define SWF_MOVE_SELECTIVE_UPDATE 3       /* the cells' update passes: UpdateSelectedDownwardPass */

typedef struct SWF_MovedEvent
{
	SWF_EventHeader header;
	const void* key; /* TESObjectREFR*, or the NiAVObject* moved when it has no reference */
	uint32_t writer; /* SWF_MOVE_* */
	uint32_t reserved;
} SWF_MovedEvent;

/* Engine call hooks. SWF_EVENT_CALL(hook) fires PRE before and POST after the engine function,
 * on the calling thread. `args` holds its arguments in order: pointers and unsigned integers
 * zero-extended, signed integers sign-extended, bools as 0 or 1, floats as their IEEE bits in
 * the low 32 bits. At POST `result` holds the return value the same way (0 for void). The
 * signatures below are the engine's (AE offsets from the image base; "slot" is a vtable slot
 * whose implementation SWF hooks). */
#define SWF_EVENT_CALL_BASE 64
#define SWF_EVENT_CALL(hook) (SWF_EVENT_CALL_BASE + (hook))
#define SWF_CALL_MAX_ARGS 8

typedef struct SWF_CallEvent
{
	SWF_EventHeader header;
	uint32_t hook;
	uint32_t argCount;
	uint64_t args[SWF_CALL_MAX_ARGS];
	uint64_t result;
} SWF_CallEvent;

/* SWF_CAP_FADE_CALLS */
#define SWF_HOOK_FADE_ON_VISIBLE 1    /* BSFadeNode slot 0x34 (fade, NiCullingProcess*, int32 alphaGroup) */
#define SWF_HOOK_FADE_UPDATE 2        /* 0x147a160 (fade, float fadeAmount, camera) -> uint64 */
#define SWF_HOOK_FADE_LOD_LEVEL 3     /* 0x147a430 (fade, float level): the LOD level (+0x152 & 0xF) */
#define SWF_HOOK_FADE_SET_RANGE 4     /* 0x147a9b0 (fade, float near, float far) */
#define SWF_HOOK_FADE_SET_LOD_TYPE 5  /* 0x147aa00 (fade, uint8 type) */
/* SWF_CAP_NODE_CALLS */
#define SWF_HOOK_CELL_PLACE_REFERENCE 6       /* 0x2d1280 (TESObjectCELL*, TESObjectREFR*) */
#define SWF_HOOK_CELL_PLACE_REFERENCE_AT 7    /* 0x2d5090 (TESObjectCELL*, TESObjectREFR*, bool snap) -> void* */
#define SWF_HOOK_RIGID_BODY_MOTION_TYPE 8     /* 0xb44490 (hkpRigidBody*, uint32 type, uint32, uint32) */
#define SWF_HOOK_RELEASE_COLLISION_OBJECT 9   /* 0xe880e0 (visit*): the node is at visit +0x10 */
#define SWF_HOOK_SET_COLLISION_OBJECT 10      /* 0xe8bd40 (NiAVObject*, NiCollisionObject*) */
#define SWF_HOOK_PREPEND_CONTROLLER 11        /* 0xd268d0 (NiObjectNET*, NiTimeController*) */
#define SWF_HOOK_REMOVE_CONTROLLER 12         /* 0xd269a0 (NiObjectNET*, NiTimeController*) */
#define SWF_HOOK_HAVOK_NODE_TRANSFORM 13      /* 0xea55a0 (NiCollisionObject*, const transform*) */
#define SWF_HOOK_DISMEMBER_PARTITION 14       /* 0x21a530 (BSDismemberSkinInstance*, uint16 slot, bool visible) */
#define SWF_HOOK_BIPED_DISMEMBER_PARTITIONS 15 /* 0x218200 (biped, object, BSDismemberSkinInstance*, int32 slot, bool) */
/* SWF_CAP_PROPERTY_CALLS */
#define SWF_HOOK_PROPERTY_SET_FLAGS 16        /* 0x147bee0 BSShaderProperty::SetFlags (property, uint8 flag, bool set) */
#define SWF_HOOK_PROPERTY_SET_MATERIAL 17     /* 0x147bff0 BSShaderProperty::SetMaterial (property, material, bool unique) */
#define SWF_HOOK_LIGHTING_SET_MATERIAL_ALPHA 18 /* BSLightingShaderProperty slot 0x31 (property, float alpha) */
#define SWF_HOOK_LIGHTING_GET_RENDER_PASSES 19  /* BSLightingShaderProperty slot 0x2A (property, geometry, uint32 flags, accumulator) -> passes */
#define SWF_HOOK_PROPERTIES_SET_FLAG 20       /* 0x147c1e0 (object, uint32 flag, bool set, bool lightingOnly) -> uint64 */
#define SWF_HOOK_PROPERTIES_SET_FADE_NODE 21  /* 0x147c690 (BSFadeNode**, object) */
#define SWF_HOOK_PROPERTIES_PROJECTED_UV 22   /* 0x14abd10 (object, visitor) -> uint64 */
#define SWF_HOOK_SET_EXTERNAL_EMITTANCE 23    /* 0x1480fc0 (object, NiColor**) -> uint64 */
#define SWF_HOOK_SKY_SET_COLOR 24             /* 0x40d970 Sky::SetColor (sky, NiColor*, blend, float flash) */
/* SWF_CAP_GEOMETRY_CALLS */
#define SWF_HOOK_GEOMETRY_ATTACH_ALPHA 25     /* 0x267cf0 BSGeometry::AttachProperty (geometry, NiAlphaProperty*) */
#define SWF_HOOK_ALPHAS_SET_THRESHOLD 26      /* 0x14ab770 (object, visitor) -> uint64 */
#define SWF_HOOK_WEAPON_BLOOD_GEOMETRY 27     /* 0x14ab580 (object, visitor) -> uint64 */
#define SWF_HOOK_LAND_SETUP_PROPERTIES 28     /* 0x2ad800 (TESObjectLAND*) -> uint64 */
#define SWF_HOOK_LAND_BLEND_PARAMS 29         /* 0x2ad0e0 (TESObjectLAND*) */
/* SWF_CAP_MATERIAL_CALLS */
#define SWF_HOOK_FLOAT_CONTROLLER_UPDATE 30   /* BSLightingShaderPropertyFloatController slot 0x27 (controller, data) */
#define SWF_HOOK_COLOR_CONTROLLER_UPDATE 31   /* BSLightingShaderPropertyColorController slot 0x27 (controller, data) */
#define SWF_HOOK_MATERIAL_COPY_MEMBERS 32     /* the 14 Lighting material classes, slot 0x02 (material, source) */
#define SWF_HOOK_MATERIAL_LOAD_TEXTURE_SET 33 /* slot 0x08 (material, uint64, BSTextureSet*) */
#define SWF_HOOK_MATERIAL_CLEAR_TEXTURES 34   /* slot 0x09 (material) */
#define SWF_HOOK_MATERIAL_RECEIVE_VALUES 35   /* slot 0x0A (material, bool, bool, bool, bool, bool) */
/* SWF_CAP_FADE_CALLS */
#define SWF_HOOK_FADE_SNAP 36                 /* 0x147aa20 (node, camera) -> uint64: a fade node's placement snap */
/* SWF_CAP_ACTOR_CALLS */
#define SWF_HOOK_ACTOR_SET_ALPHA 37           /* Actor, Character, PlayerCharacter slot 0xE1 (actor, float alpha) */
#define SWF_HOOK_ACTOR_UPDATE_3D_POSITION 38  /* 0x69eb80 Actor::UpdateActor3DPosition (actor) */
#define SWF_HOOK_ACTOR_SKY_CELL_SKIN 39       /* 0x663430 (actor) -> uint64 */
#define SWF_HOOK_ACTOR_RAGDOLL_JOB 40         /* 0x770dc0 (list): actor handles at list +0x30, their count at +0x40 */
/* SWF_CAP_DECAL_CALLS */
#define SWF_HOOK_DECAL_ARRAY_EDIT 41          /* a BGSDecalNode's decal array edits (node, ...): 0x1fdfb0 append, 0x1fdc80 and
                                               * 0x1fdd50, 0x1fde40 erase, 0x1fdcb0 the geometry decal's update; args 1-3 are the
                                               * argument registers as passed, meaningful only per function */
/* SWF_CAP_LOD_SEGMENT_CALLS: every call of the three, at its call sites */
#define SWF_HOOK_LOD_SEGMENT_SHOW 42          /* 0xe31130 (BSSubIndexTriShape*, uint64 segment) */
#define SWF_HOOK_LOD_SEGMENT_HIDE 43          /* 0xe31160 (BSSubIndexTriShape*, uint64 segment) */
#define SWF_HOOK_LOD_SEGMENT_SHOW_ALL 44      /* 0xe310b0 (BSSubIndexTriShape*) */
/* SWF_CAP_TREE_LOD_CALLS */
#define SWF_HOOK_TREE_LOD_REFILL 45           /* 0x504a40 at its two call sites (BGSDistantTreeBlock*, TreeGroup*) */
#define SWF_HOOK_INSTANCE_ADD_GROUP 46        /* BSMultiStreamInstanceTriShape slot 0x3C (shape, uint32 count, const uint16* data,
                                               * uint32 shorts, float radius) -> uint32 index */
#define SWF_HOOK_INSTANCE_REMOVE_GROUP 47     /* slot 0x3D (shape, uint32 index) */
/* SWF_CAP_TREE_ANIM_CALLS: the tree manager's animation list */
#define SWF_HOOK_TREE_ANIM_ADD 48             /* 0x4376f0 (manager, BSLeafAnimNode*, uint64, uint64) */
#define SWF_HOOK_TREE_ANIM_REMOVE 49          /* 0x437840 (manager, node) -> uint64; vetoable */
#define SWF_HOOK_TREE_ANIM_CLEAR 50           /* 0x437ae0 (manager, uint64, uint64, uint64) -> uint64 */
/* SWF_CAP_EFFECT_CALLS */
#define SWF_HOOK_EFFECT_FLOAT_CONTROLLER_UPDATE 51 /* BSEffectShaderPropertyFloatController slot 0x27 (controller, data) */
#define SWF_HOOK_EFFECT_COLOR_CONTROLLER_UPDATE 52 /* BSEffectShaderPropertyColorController slot 0x27 (controller, data) */
#define SWF_HOOK_EFFECT_MATERIAL_COPY_MEMBERS 53   /* BSEffectShaderMaterial slot 0x02 (material, source) */
/* SWF_CAP_NODE_CALLS */
#define SWF_HOOK_HAVOK_INIT 54                /* 0xe86ad0 bhkWorld::InitHavok (world, NiAVObject* from, NiAVObject* to): moves a
                                               * collision object from `from` to `to` (SetCollisionObject on `to`, `from`'s
                                               * +0x40 cleared inline) */
#define SWF_HOOK_REFERENCE_DISABLE 55         /* 0xa3db90 (TESObjectREFR*, bool fadeOut, bool, handle*, uint32) -> bool: Papyrus
                                               * Disable's; with a fade, the 3D's LOD type set to 9 (+0x153) inline */
#define SWF_HOOK_ENABLE_COMMAND 56            /* 0x34c9f0, the script command Enable (8 arguments, the reference third) -> bool:
                                               * a LOD type of 9 reset to the model's, inline */
#define SWF_HOOK_COUNT 57

/* Event callback. `scratch` points to SWF_SCRATCH_WORDS words private to this subscription
 * and this engine call, zeroed before the first dispatch. For a PRE|POST subscription they
 * survive from the PRE to the POST dispatch of the same call; otherwise they are fresh on
 * every dispatch. The pointer is valid until the callback returns. */
#define SWF_SCRATCH_WORDS 8
typedef void (*SWF_EventFn)(const SWF_EventHeader* event, uintptr_t* scratch, void* user);

/* Filter callback: return nonzero to veto the write. Filters run in priority order and
 * stop at the first veto. */
typedef int32_t (*SWF_FilterFn)(const SWF_EventHeader* event, void* user);

typedef uint64_t SWF_Subscription;

/* Kinds of patch the integrity check re-reads. */
#define SWF_SITE_DETOUR 1 /* the jump a detour wrote at a function's start */
#define SWF_SITE_CALL 2   /* a rewritten call site (a store, or a hooked call) */
#define SWF_SITE_SLOT 3   /* a swapped vtable slot */

/* One patch of SWF's that something rewrote after SWF installed it. */
typedef struct SWF_IntegrityEntry
{
	uint32_t size;
	uint32_t kind;     /* SWF_SITE_* */
	uintptr_t address; /* the patched bytes */
	uint32_t offset;   /* from the game executable's image base (0 outside it) */
	uint32_t reserved;
	const char* family; /* the hook family's name */
	const char* module; /* file name of the module the new bytes lead into, or null; valid during the callback */
	uintptr_t target;   /* the address the new bytes lead to (through relays outside any module) */
} SWF_IntegrityEntry;
typedef void (*SWF_IntegrityFn)(const SWF_IntegrityEntry* entry, void* user);

typedef struct SWF_ProviderInfo
{
	uint32_t size; /* set by the caller to sizeof(SWF_ProviderInfo) */
	uint32_t abiMajor;
	uint32_t abiMinor;
	uint32_t candidateCount; /* candidates seen at election */
	uint64_t implVersion;
	uint64_t runtimeMask;
	SWF_Caps caps;
	const char* hostName; /* module file name of the DLL hosting the provider */
} SWF_ProviderInfo;

typedef struct SWF_Api
{
	uint32_t size;
	uint32_t abiMinor;

	SWF_Result (*GetProviderInfo)(SWF_ProviderInfo* out);
	SWF_Caps (*GetCaps)(void);

	/* Lower priority runs first; equal priorities run in subscription order. */
	SWF_Result (*Subscribe)(uint32_t eventId, uint32_t phases, int32_t priority, SWF_EventFn fn, void* user, SWF_Subscription* out);
	SWF_Result (*Unsubscribe)(SWF_Subscription subscription);

	SWF_Result (*AddFilter)(uint32_t filterId, int32_t priority, SWF_FilterFn fn, void* user, SWF_Subscription* out);
	SWF_Result (*RemoveFilter)(SWF_Subscription filter);

	/* Report a direct write to scene state SWF could not observe. Fires SWF_EVENT_WRITTEN. */
	SWF_Result (*NotifyWritten)(void* object, SWF_Fields fields);

	/* Number of engine sites behind an event id (0 if not installed or not a store event). */
	uint32_t (*SiteCount)(uint32_t eventId);

	/* Maps an instruction address inside one of SWF's store stubs back to the engine store it
	 * replaced (for hardware-watch diagnostics). Any output pointer may be null. */
	SWF_Result (*ResolveSite)(uintptr_t address, uint32_t* eventId, uint32_t* site, uint32_t* siteOffset);

	/* abiMinor >= 1. Re-reads every patch SWF installed and reports each one rewritten since (by a plugin
	 * hooking the same code after SWF), with the module its new bytes lead into; also logs them. Repairs
	 * nothing: a later hook that calls on still reaches SWF's. `fn` may be null. Returns the count. Hosts
	 * call it once the game's data has loaded (SKSE kDataLoaded), when every plugin has installed. */
	uint32_t (*CheckIntegrity)(SWF_IntegrityFn fn, void* user);

	/* abiMinor >= 2. Writes `value` into `object`'s field `kind` (SWF_WRITE_*) and fires SWF_EVENT_SCENE_WRITE
	 * around the store. On any thread the engine allows that write on; SWF adds no locking of its own. */
	SWF_Result (*Write)(uint32_t kind, void* object, uint64_t value);
} SWF_Api;

typedef struct SWF_CandidateInfo
{
	uint32_t size;
	uint32_t abiMajor;
	uint32_t abiMinor;
	uint32_t reserved;
	uint64_t implVersion;
	uint64_t runtimeMask;
	const char* hostName;

	uint32_t (*GetState)(void);
	/* Idempotent: installs on the first successful call, then returns the cached result.
	 * `candidateCount` is recorded for SWF_ProviderInfo. */
	SWF_Result (*Activate)(uint32_t candidateCount);
	/* Returns the table if this candidate is active and serves `abiMinorWanted`. */
	const SWF_Api* (*GetApi)(uint32_t abiMinorWanted);
} SWF_CandidateInfo;

typedef const SWF_CandidateInfo* (*SWF_CandidateFn)(void);

#ifdef __cplusplus
}
#endif
