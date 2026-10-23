// Election scenarios, one per process (ctest runs each by name): a provider pins its
// module, so state cannot be reset by unloading.
//
// Hosts: swf_host_a 0.1.0, swf_host_b 0.2.0, swf_host_c 0.2.0, swf_host_d 0.3.0 (late).
#include "swf/swf.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(condition)                                                                       \
	do {                                                                                       \
		if (!(condition)) {                                                                    \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
			std::exit(1);                                                                      \
		}                                                                                      \
	} while (0)

namespace
{
	struct Host
	{
		HMODULE module = nullptr;
		const SWF_Api* (*PostLoad)() = nullptr;
		void (*SetFail)(bool) = nullptr;
		uint32_t (*Installs)() = nullptr;
		uint32_t (*State)() = nullptr;
	};

	template <class T>
	T Export(HMODULE module, const char* name)
	{
		auto proc = reinterpret_cast<T>(GetProcAddress(module, name));
		CHECK(proc != nullptr);
		return proc;
	}

	Host Load(const char* name)
	{
		Host host;
		host.module = LoadLibraryA(name);
		CHECK(host.module != nullptr);
		host.PostLoad = Export<decltype(host.PostLoad)>(host.module, "TestHost_PostLoad");
		host.SetFail = Export<decltype(host.SetFail)>(host.module, "TestHost_SetFail");
		host.Installs = Export<decltype(host.Installs)>(host.module, "TestHost_Installs");
		host.State = Export<decltype(host.State)>(host.module, "TestHost_State");
		return host;
	}

	struct Abc
	{
		Host a = Load("swf_host_a.dll");
		Host b = Load("swf_host_b.dll");
		Host c = Load("swf_host_c.dll");
	};

	std::string TempPath(const char* name)
	{
		char directory[MAX_PATH];
		GetTempPathA(MAX_PATH, directory);
		return std::string(directory) + "swf_test_" + std::to_string(GetCurrentProcessId()) + "_" + name;
	}

	void WriteIni(const char* body)
	{
		const std::string path = TempPath("election.ini");
		FILE* file = nullptr;
		fopen_s(&file, path.c_str(), "w");
		CHECK(file != nullptr);
		std::fprintf(file, "[Election]\n%s\n", body);
		std::fclose(file);
		SetEnvironmentVariableA("SWF_INI_PATH", path.c_str());
	}

	const char* ProviderHost(const SWF_Api* api)
	{
		CHECK(api != nullptr);
		SWF_ProviderInfo info{};
		info.size = sizeof(info);
		CHECK(api->GetProviderInfo(&info) == SWF_OK);
		return info.hostName;
	}

	// Every host returned the same table, and only `winner` installed.
	void ExpectProvider(const Abc& hosts, const Host& winner, const char* winnerName, std::initializer_list<const SWF_Api*> apis)
	{
		for (const SWF_Api* api : apis)
			CHECK(api == *apis.begin());
		CHECK(std::strcmp(ProviderHost(*apis.begin()), winnerName) == 0);
		CHECK(winner.State() == SWF_STATE_ACTIVE);
		CHECK(hosts.a.Installs() + hosts.b.Installs() + hosts.c.Installs() == 1);
		CHECK(winner.Installs() == 1);
	}

	int OrderAbc()
	{
		Abc hosts;
		const SWF_Api* a = hosts.a.PostLoad();
		const SWF_Api* b = hosts.b.PostLoad();
		const SWF_Api* c = hosts.c.PostLoad();
		ExpectProvider(hosts, hosts.b, "swf_host_b.dll", { a, b, c });
		return 0;
	}

	int OrderCba()
	{
		Abc hosts;
		const SWF_Api* c = hosts.c.PostLoad();
		const SWF_Api* b = hosts.b.PostLoad();
		const SWF_Api* a = hosts.a.PostLoad();
		ExpectProvider(hosts, hosts.b, "swf_host_b.dll", { a, b, c });
		return 0;
	}

	int Fallback()
	{
		Abc hosts;
		hosts.b.SetFail(true);
		const SWF_Api* a = hosts.a.PostLoad();
		const SWF_Api* c = hosts.c.PostLoad();
		CHECK(hosts.b.State() == SWF_STATE_FAILED);
		ExpectProvider(hosts, hosts.c, "swf_host_c.dll", { a, c });
		return 0;
	}

	int AllFail()
	{
		Abc hosts;
		hosts.a.SetFail(true);
		hosts.b.SetFail(true);
		hosts.c.SetFail(true);
		CHECK(hosts.a.PostLoad() == nullptr);
		CHECK(hosts.b.PostLoad() == nullptr);
		CHECK(hosts.a.Installs() + hosts.b.Installs() + hosts.c.Installs() == 0);
		return 0;
	}

	int PinHost()
	{
		WriteIni("PinHost=SWF_HOST_A.dll");
		Abc hosts;
		const SWF_Api* c = hosts.c.PostLoad();
		const SWF_Api* a = hosts.a.PostLoad();
		ExpectProvider(hosts, hosts.a, "swf_host_a.dll", { a, c });
		return 0;
	}

	int PinVersion()
	{
		WriteIni("PinVersion=0.1.0");
		Abc hosts;
		const SWF_Api* b = hosts.b.PostLoad();
		ExpectProvider(hosts, hosts.a, "swf_host_a.dll", { b });
		return 0;
	}

	int Disabled()
	{
		WriteIni("Disable=1");
		Abc hosts;
		CHECK(hosts.a.PostLoad() == nullptr);
		CHECK(hosts.b.PostLoad() == nullptr);
		CHECK(hosts.a.Installs() + hosts.b.Installs() + hosts.c.Installs() == 0);
		return 0;
	}

	int UnsupportedRuntime()
	{
		SetEnvironmentVariableA("SWF_RUNTIME_OVERRIDE", "0x2");
		Abc hosts;
		CHECK(hosts.a.PostLoad() == nullptr);
		CHECK(hosts.a.Installs() + hosts.b.Installs() + hosts.c.Installs() == 0);
		return 0;
	}

	int LateCandidate()
	{
		Abc hosts;
		const SWF_Api* b = hosts.b.PostLoad();
		const Host d = Load("swf_host_d.dll");
		const SWF_Api* fromD = d.PostLoad();
		CHECK(fromD == b);
		CHECK(d.Installs() == 0);
		CHECK(std::strcmp(ProviderHost(fromD), "swf_host_b.dll") == 0);
		return 0;
	}

	int ClientOnly()
	{
		const HMODULE client = LoadLibraryA("swf_client_only.dll");
		CHECK(client != nullptr);
		const auto connect = Export<const SWF_Api* (*)()>(client, "TestClient_Connect");
		Abc hosts;
		CHECK(connect() == nullptr);  // no provider before the election
		const SWF_Api* a = hosts.a.PostLoad();
		CHECK(connect() == a);
		CHECK(hosts.a.Installs() + hosts.b.Installs() + hosts.c.Installs() == 1);
		return 0;
	}

	struct Seen
	{
		std::vector<int> order;
		std::vector<uint64_t> sequences;
		void* object = nullptr;
		SWF_Fields fields = 0;
	} seen;

	void Record(const SWF_EventHeader* header, uintptr_t* scratch, void* user)
	{
		CHECK(header->id == SWF_EVENT_WRITTEN && header->phase == SWF_PHASE_POST);
		CHECK(header->threadId == GetCurrentThreadId() && *scratch == 0);
		const auto* event = reinterpret_cast<const SWF_WrittenEvent*>(header);
		seen.order.push_back(static_cast<int>(reinterpret_cast<intptr_t>(user)));
		seen.sequences.push_back(header->sequence);
		seen.object = event->object;
		seen.fields = event->fields;
	}

	int Dispatch()
	{
		Abc hosts;
		const SWF_Api* api = hosts.a.PostLoad();
		CHECK(api != nullptr);
		CHECK(api->size >= sizeof(SWF_Api));

		SWF_ProviderInfo info{};
		info.size = sizeof(info);
		CHECK(api->GetProviderInfo(&info) == SWF_OK);
		CHECK(info.candidateCount == 3);
		CHECK(info.implVersion == SWF_MAKE_VERSION(0, 2, 0, 0));

		const SWF_EventFn fn = &Record;

		SWF_Subscription late = 0, early = 0, same = 0;
		CHECK(api->Subscribe(SWF_EVENT_WRITTEN, SWF_PHASE_POST, 10, fn, reinterpret_cast<void*>(2), &late) == SWF_OK);
		CHECK(api->Subscribe(SWF_EVENT_WRITTEN, SWF_PHASE_POST, -5, fn, reinterpret_cast<void*>(1), &early) == SWF_OK);
		CHECK(api->Subscribe(SWF_EVENT_WRITTEN, SWF_PHASE_POST, 10, fn, reinterpret_cast<void*>(3), &same) == SWF_OK);
		CHECK(api->Subscribe(0, SWF_PHASE_POST, 0, fn, nullptr, &same) == SWF_E_INVALID);
		CHECK(api->Subscribe(SWF_EVENT_WRITTEN, 0, 0, fn, nullptr, &same) == SWF_E_INVALID);

		int object = 0;
		CHECK(api->NotifyWritten(&object, SWF_FIELD_HIDDEN | SWF_FIELD_FADE) == SWF_OK);
		CHECK((seen.order == std::vector<int>{ 1, 2, 3 }));
		CHECK(seen.object == &object && seen.fields == (SWF_FIELD_HIDDEN | SWF_FIELD_FADE));
		CHECK(seen.sequences[0] == seen.sequences[2]);  // one event, one sequence number

		const uint64_t firstSequence = seen.sequences[0];
		seen = {};
		CHECK(api->Unsubscribe(late) == SWF_OK);
		CHECK(api->Unsubscribe(late) == SWF_E_INVALID);
		CHECK(api->NotifyWritten(&object, SWF_FIELD_TRANSFORM) == SWF_OK);
		CHECK((seen.order == std::vector<int>{ 1, 3 }));
		CHECK(seen.sequences[0] > firstSequence);
		CHECK(api->NotifyWritten(nullptr, SWF_FIELD_TRANSFORM) == SWF_E_INVALID);
		return 0;
	}
}

int main(int argc, char** argv)
{
	CHECK(argc == 2);
	SetEnvironmentVariableA("SWF_RUNTIME_OVERRIDE", "1");
	SetEnvironmentVariableA("SWF_INI_PATH", TempPath("missing.ini").c_str());
	SetEnvironmentVariableA("SWF_LOG_PATH", TempPath(argv[1]).append(".log").c_str());

	struct Scenario
	{
		const char* name;
		int (*run)();
	};
	static constexpr Scenario kScenarios[] = {
		{ "order_abc", &OrderAbc },
		{ "order_cba", &OrderCba },
		{ "fallback", &Fallback },
		{ "all_fail", &AllFail },
		{ "pin_host", &PinHost },
		{ "pin_version", &PinVersion },
		{ "disabled", &Disabled },
		{ "unsupported_runtime", &UnsupportedRuntime },
		{ "late_candidate", &LateCandidate },
		{ "client_only", &ClientOnly },
		{ "dispatch", &Dispatch },
	};
	for (const Scenario& scenario : kScenarios)
		if (std::strcmp(scenario.name, argv[1]) == 0)
			return scenario.run();
	std::fprintf(stderr, "unknown scenario %s\n", argv[1]);
	return 2;
}
