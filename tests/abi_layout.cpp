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
static_assert(sizeof(SWF_Api) >= 64);

FROZEN(SWF_EventHeader, size, 0);
FROZEN(SWF_EventHeader, id, 4);
FROZEN(SWF_EventHeader, phase, 8);
FROZEN(SWF_EventHeader, threadId, 12);
FROZEN(SWF_EventHeader, sequence, 16);
static_assert(sizeof(SWF_EventHeader) == 24);

FROZEN(SWF_WrittenEvent, object, 24);
FROZEN(SWF_WrittenEvent, fields, 32);

FROZEN(SWF_ProviderInfo, size, 0);
FROZEN(SWF_ProviderInfo, abiMajor, 4);
FROZEN(SWF_ProviderInfo, abiMinor, 8);
FROZEN(SWF_ProviderInfo, candidateCount, 12);
FROZEN(SWF_ProviderInfo, implVersion, 16);
FROZEN(SWF_ProviderInfo, runtimeMask, 24);
FROZEN(SWF_ProviderInfo, caps, 32);
FROZEN(SWF_ProviderInfo, hostName, 40);

static_assert(SWF_ABI_MAJOR == 1);
static_assert(SWF_EVENT_WRITTEN == 1);
static_assert(SWF_RUNTIME_AE_1_6_1170 == 1);
static_assert(SWF_MAKE_VERSION(1, 2, 3, 4) == 0x0001000200030004ull);

int main()
{
	return std::strcmp(SWF_CANDIDATE_EXPORT, "SWF_Candidate_v1") == 0 ? 0 : 1;
}
