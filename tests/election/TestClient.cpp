// A client-only mod: no embedded SWF, just the header-only discovery.
#include "swf/swf_client.h"

extern "C" __declspec(dllexport) const SWF_Api* TestClient_Connect()
{
	return swf::client::Connect();
}
