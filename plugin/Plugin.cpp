// Standalone SceneWriterFramework.dll: an SKSE plugin whose only job is to be a candidate
// and to run the election at kPostLoad. It declares the few SKSE structs it needs itself
// (SKSE PluginAPI.h layouts) so the plugin does not depend on CommonLib.
#include "core/Log.h"
#include "swf/swf_client.h"
#include "swf/swf_embed.h"

#include <cstdint>

namespace
{
	using PluginHandle = uint32_t;

	struct SKSEInterface
	{
		uint32_t skseVersion;
		uint32_t runtimeVersion;
		uint32_t editorVersion;
		uint32_t isEditor;
		void* (*QueryInterface)(uint32_t id);
		PluginHandle (*GetPluginHandle)();
		uint32_t (*GetReleaseIndex)();
	};

	struct SKSEMessagingInterface
	{
		struct Message
		{
			const char* sender;
			uint32_t type;
			uint32_t dataLen;
			void* data;
		};
		using EventCallback = void (*)(Message* msg);

		uint32_t interfaceVersion;
		bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
		bool (*Dispatch)(PluginHandle sender, uint32_t messageType, void* data, uint32_t dataLen, const char* receiver);
		void* (*GetEventDispatcher)(uint32_t dispatcherId);
	};

	constexpr uint32_t kInterface_Messaging = 5;
	constexpr uint32_t kMessage_PostLoad = 0;
	constexpr uint32_t kMessage_DataLoaded = 8;

	constexpr uint32_t MakeExeVersion(uint32_t major, uint32_t minor, uint32_t build)
	{
		return ((major & 0xFF) << 24) | ((minor & 0xFF) << 16) | ((build & 0xFFF) << 4);
	}

	void OnMessage(SKSEMessagingInterface::Message* message)
	{
		if (!message)
			return;
		if (message->type == kMessage_PostLoad)
			SWF_Embed_OnPostLoad();
		// Every plugin has installed by now: SWF's patches that another rewrote since are logged, with the module (whichever
		// copy is the provider checks; a host that embeds SWF may call it again).
		if (message->type == kMessage_DataLoaded)
			if (const SWF_Api* api = swf::client::Connect())
				api->CheckIntegrity(nullptr, nullptr);
	}
}

struct SKSEPluginVersionData
{
	uint32_t dataVersion;
	uint32_t pluginVersion;
	char name[256];
	char author[256];
	char supportEmail[252];
	uint32_t versionIndependenceEx;
	uint32_t versionIndependence;
	uint32_t compatibleVersions[16];
	uint32_t seVersionRequired;
};
static_assert(sizeof(SKSEPluginVersionData) == 0x350);

extern "C" __declspec(dllexport) constinit SKSEPluginVersionData SKSEPlugin_Version{
	1,
	(SWF_VERSION_MAJOR << 24) | (SWF_VERSION_MINOR << 16) | (SWF_VERSION_PATCH << 4),
	"SceneWriterFramework",
	"Panthuncia",
	"",
	1,  // kVersionIndependentEx_NoStructUse
	0,
	{ MakeExeVersion(1, 6, 1170), 0 },
	0,
};

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface* skse)
{
	if (!skse || skse->isEditor)
		return false;
	auto* messaging = static_cast<SKSEMessagingInterface*>(skse->QueryInterface(kInterface_Messaging));
	if (!messaging || !messaging->RegisterListener(skse->GetPluginHandle(), "SKSE", &OnMessage)) {
		swf::log::Error("standalone plugin: SKSE messaging unavailable");
		return false;
	}
	swf::log::Info("standalone plugin loaded");
	return true;
}
