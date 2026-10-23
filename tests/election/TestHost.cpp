// A host DLL embedding one SWF copy, with hooks for the election tests.
#include "core/Provider.h"
#include "swf/swf_embed.h"

extern "C" __declspec(dllexport) const SWF_Api* TestHost_PostLoad()
{
	return SWF_Embed_OnPostLoad();
}

extern "C" __declspec(dllexport) void TestHost_SetFail(bool fail)
{
	swf::provider::testing::ForceActivateFailure(fail);
}

extern "C" __declspec(dllexport) uint32_t TestHost_Installs()
{
	return swf::provider::testing::InstallCount();
}

extern "C" __declspec(dllexport) uint32_t TestHost_State()
{
	return swf::provider::State();
}
