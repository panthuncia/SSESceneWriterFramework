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
#define SWF_ABI_MINOR 0

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
#define SWF_E_FULL 7        /* subscriber limit reached for this id */

/* Game runtimes a candidate has address tables for (bit per runtime). */
#define SWF_RUNTIME_AE_1_6_1170 (1ull << 0)

/* Candidate states reported by SWF_CandidateInfo::GetState. */
#define SWF_STATE_IDLE 0
#define SWF_STATE_ACTIVE 1
#define SWF_STATE_FAILED 2

/* Capability bits: one per hook family that installed successfully. */
typedef uint64_t SWF_Caps;

/* Subscription phases. Function-hook events fire PRE and POST around the engine call;
 * store events fire POST only. A subscription may ask for both. */
#define SWF_PHASE_PRE (1u << 0)
#define SWF_PHASE_POST (1u << 1)

/* Event ids. */
#define SWF_EVENT_WRITTEN 1 /* SWF_WrittenEvent: a client reported a direct write */

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
	uint64_t sequence; /* process-wide, increasing */
} SWF_EventHeader;

typedef struct SWF_WrittenEvent
{
	SWF_EventHeader header;
	void* object; /* engine object written (NiAVObject*, BSShaderProperty*, material...) */
	SWF_Fields fields;
} SWF_WrittenEvent;

/* Event callback. `scratch` is one word private to this subscription that survives from
 * the PRE to the POST dispatch of the same engine call; it starts at 0. */
typedef void (*SWF_EventFn)(const SWF_EventHeader* event, uintptr_t* scratch, void* user);

/* Filter callback: return nonzero to veto the write. Filters run in priority order and
 * stop at the first veto. */
typedef int32_t (*SWF_FilterFn)(const SWF_EventHeader* event, void* user);

typedef uint64_t SWF_Subscription;

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
