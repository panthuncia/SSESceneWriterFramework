/* A client-only mod in C: no embedded SWF, just the header-only loader. */
#include "swf/swf_loader.h"

__declspec(dllexport) const SWF_Api* TestClient_Connect(void)
{
	return SWF_Connect(SWF_ABI_MINOR);
}

/* A provider newer than any: none connects. */
__declspec(dllexport) const SWF_Api* TestClient_ConnectFuture(void)
{
	return SWF_Connect(SWF_ABI_MINOR + 1);
}
