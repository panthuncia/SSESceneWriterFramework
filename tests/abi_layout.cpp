// Freezes the public ABI layout. A failure here means a struct changed in a way that
// breaks binaries built against an earlier SWF; add fields only at the end.
#include "swf/swf.h"

#include <cstddef>
#include <cstring>

#define FROZEN(type, field, offset) static_assert(offsetof(type, field) == offset, #type "::" #field " moved")

FROZEN(SWF_CandidateInfo, size, 0);
FROZEN(SWF_CandidateInfo, abiMajor, 4);
FROZEN(SWF_CandidateInfo, abiMinor, 8);
FROZEN(SWF_CandidateInfo, implVersion, 16);
FROZEN(SWF_CandidateInfo, runtimeMask, 24);
FROZEN(SWF_CandidateInfo, hostName, 32);
FROZEN(SWF_CandidateInfo, GetState, 40);
FROZEN(SWF_CandidateInfo, Activate, 48);
FROZEN(SWF_CandidateInfo, GetApi, 56);
static_assert(sizeof(SWF_CandidateInfo) >= 64);

FROZEN(SWF_Api, size, 0);
FROZEN(SWF_Api, abiMinor, 4);
FROZEN(SWF_Api, GetProviderInfo, 8);
FROZEN(SWF_Api, GetCaps, 16);
FROZEN(SWF_Api, Subscribe, 24);
FROZEN(SWF_Api, Unsubscribe, 32);
FROZEN(SWF_Api, AddFilter, 40);
FROZEN(SWF_Api, RemoveFilter, 48);
FROZEN(SWF_Api, NotifyWritten, 56);
FROZEN(SWF_Api, SiteCount, 64);
FROZEN(SWF_Api, ResolveSite, 72);
FROZEN(SWF_Api, CheckIntegrity, 80);
FROZEN(SWF_Api, Write, 88);
static_assert(sizeof(SWF_Api) >= 96);

FROZEN(SWF_IntegrityEntry, size, 0);
FROZEN(SWF_IntegrityEntry, kind, 4);
FROZEN(SWF_IntegrityEntry, address, 8);
FROZEN(SWF_IntegrityEntry, offset, 16);
FROZEN(SWF_IntegrityEntry, family, 24);
FROZEN(SWF_IntegrityEntry, module, 32);
FROZEN(SWF_IntegrityEntry, target, 40);

FROZEN(SWF_EventHeader, size, 0);
FROZEN(SWF_EventHeader, id, 4);
FROZEN(SWF_EventHeader, phase, 8);
FROZEN(SWF_EventHeader, threadId, 12);
FROZEN(SWF_EventHeader, sequence, 16);
static_assert(sizeof(SWF_EventHeader) == 24);

FROZEN(SWF_WrittenEvent, object, 24);
FROZEN(SWF_WrittenEvent, fields, 32);
FROZEN(SWF_SceneWriteEvent, kind, 24);
FROZEN(SWF_SceneWriteEvent, object, 32);
FROZEN(SWF_SceneWriteEvent, before, 40);
FROZEN(SWF_SceneWriteEvent, after, 48);
static_assert(sizeof(SWF_SceneWriteEvent) == 56);

FROZEN(SWF_HiddenEvent, object, 24);
FROZEN(SWF_HiddenEvent, site, 32);
FROZEN(SWF_HiddenEvent, siteOffset, 36);
FROZEN(SWF_HiddenEvent, hidden, 40);
static_assert(sizeof(SWF_HiddenEvent) == 48);

FROZEN(SWF_SwitchIndexEvent, node, 24);
FROZEN(SWF_SwitchIndexEvent, before, 32);
FROZEN(SWF_SwitchIndexEvent, after, 36);
FROZEN(SWF_SwitchIndexEvent, site, 40);
FROZEN(SWF_SwitchIndexEvent, siteOffset, 44);

FROZEN(SWF_FadeResetEvent, node, 24);
FROZEN(SWF_FadeResetEvent, value, 32);
FROZEN(SWF_FadeResetEvent, site, 36);
FROZEN(SWF_FadeResetEvent, siteOffset, 40);

FROZEN(SWF_ShowFilterEvent, object, 24);
FROZEN(SWF_ShowFilterEvent, site, 32);
FROZEN(SWF_ShowFilterEvent, siteOffset, 36);

FROZEN(SWF_ProviderInfo, size, 0);
FROZEN(SWF_ProviderInfo, abiMajor, 4);
FROZEN(SWF_ProviderInfo, abiMinor, 8);
FROZEN(SWF_ProviderInfo, candidateCount, 12);
FROZEN(SWF_ProviderInfo, implVersion, 16);
FROZEN(SWF_ProviderInfo, runtimeMask, 24);
FROZEN(SWF_ProviderInfo, caps, 32);
FROZEN(SWF_ProviderInfo, hostName, 40);

FROZEN(SWF_ChildEvent, parent, 24);
FROZEN(SWF_ChildEvent, child, 32);
FROZEN(SWF_ChildEvent, unchanged, 40);
FROZEN(SWF_NodeEvent, node, 24);
FROZEN(SWF_MovedEvent, key, 24);
FROZEN(SWF_MovedEvent, writer, 32);
FROZEN(SWF_CallEvent, hook, 24);
FROZEN(SWF_CallEvent, argCount, 28);
FROZEN(SWF_CallEvent, args, 32);
FROZEN(SWF_CallEvent, result, 32 + 8 * SWF_CALL_MAX_ARGS);

static_assert(SWF_ABI_MAJOR == 1);
static_assert(SWF_EVENT_CHILD_ATTACHED == 5 && SWF_EVENT_CHILD_DETACHING == 6 && SWF_EVENT_SWITCH_STRUCTURAL == 7 && SWF_EVENT_MOVED == 8);
static_assert(SWF_EVENT_SCENE_WRITE == 9 && SWF_WRITE_HIDDEN == 1 && SWF_WRITE_SHADER_PROPERTY == 2 && SWF_WRITE_ALPHA_PROPERTY == 3 && SWF_WRITE_ALPHA_FLAGS == 4);
static_assert(SWF_EVENT_CALL_BASE == 64 && SWF_HOOK_FADE_ON_VISIBLE == 1 && SWF_HOOK_MATERIAL_RECEIVE_VALUES == 35 && SWF_HOOK_EFFECT_MATERIAL_COPY_MEMBERS == 53 && SWF_HOOK_HAVOK_INIT == 54 && SWF_HOOK_REFERENCE_DISABLE == 55 && SWF_HOOK_ENABLE_COMMAND == 56 && SWF_HOOK_FLORA_HARVEST_ALPHAS == 57 && SWF_HOOK_COUNT == 58);
static_assert(SWF_FILTER_CALL_BASE == 64 && SWF_FILTER_CALL(SWF_HOOK_TREE_ANIM_REMOVE) == 113);
static_assert(SWF_SCRATCH_WORDS == 8 && SWF_CALL_MAX_ARGS == 8);
static_assert(SWF_EVENT_WRITTEN == 1 && SWF_EVENT_HIDDEN == 2 && SWF_EVENT_SWITCH_INDEX == 3 && SWF_EVENT_FADE_RESET == 4);
static_assert(SWF_FILTER_SHOW == 1);
static_assert(SWF_CAP_HIDDEN == 1 && SWF_CAP_SWITCH_INDEX == 2 && SWF_CAP_FADE_RESET == 4);
static_assert(SWF_RUNTIME_AE_1_6_1170 == 1);
static_assert(SWF_RUNTIME_SE_1_5_97 == 2);
static_assert(SWF_MAKE_VERSION(1, 2, 3, 4) == 0x0001000200030004ull);

int main()
{
	return std::strcmp(SWF_CANDIDATE_EXPORT, "SWF_Candidate_v1") == 0 ? 0 : 1;
}
