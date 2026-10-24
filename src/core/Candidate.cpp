// The SWF_Candidate_v1 export and the host entry points. They share a translation unit so
// that a host referencing SWF_Embed_OnPostLoad always links the export.
#include "core/Election.h"
#include "core/Log.h"
#include "core/Provider.h"
#include "swf/swf.hpp"
#include "swf/swf_embed.h"

#include <string>

namespace swf::inline SWF_NAMESPACE
{
	namespace
	{
		std::string Narrow(const std::wstring& wide)
		{
			const int length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
			std::string result(static_cast<size_t>(length), '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), result.data(), length, nullptr, nullptr);
			return result;
		}

		void LogElection(const election::ElectionReport& report)
		{
			const election::Candidate* provider = report.Provider();
			if (report.alreadyActive) {
				log::Debug("provider already elected: {} {}", provider->info->hostName, VersionString(provider->info->implVersion));
				return;
			}

			std::string list;
			for (const auto& candidate : report.candidates)
				list += std::format("{}{} {} (abi {}.{}, runtimes {:#x}) at {}", list.empty() ? "" : "; ",
					candidate.info->hostName, VersionString(candidate.info->implVersion), candidate.info->abiMajor,
					candidate.info->abiMinor, candidate.info->runtimeMask, Narrow(candidate.path));
			log::Info("election: {} candidate(s): {}", report.candidates.size(), list);

			for (const size_t index : report.failed)
				log::Warn("election: {} failed to activate", report.candidates[index].info->hostName);

			if (provider)
				log::Info("election: provider is {} {}", provider->info->hostName, VersionString(provider->info->implVersion));
			else if (report.settings.disabled)
				log::Warn("election: disabled by SceneWriterFramework.ini");
			else if (report.runtime == 0)
				log::Warn("election: no SWF address tables for this game version");
			else
				log::Error("election: no candidate could activate; scene writer events are unavailable");
		}
	}
}

extern "C" __declspec(dllexport) const SWF_CandidateInfo* SWF_Candidate_v1()
{
	namespace provider = swf::provider;
	static const SWF_CandidateInfo info{
		sizeof(SWF_CandidateInfo),
		SWF_ABI_MAJOR,
		SWF_ABI_MINOR,
		0,
		swf::kImplVersion,
		swf::kRuntimeMask,
		swf::log::HostName(),
		&provider::State,
		&provider::Activate,
		&provider::GetApi,
	};
	return &info;
}

extern "C" void SWF_Embed_SetLogSink(SWF_LogSink sink)
{
	swf::log::SetSink(sink);
}

extern "C" const SWF_Api* SWF_Embed_OnPostLoad(void)
{
	const swf::election::ElectionReport report = swf::election::Elect();
	swf::LogElection(report);
	const swf::election::Candidate* provider = report.Provider();
	return provider ? provider->info->GetApi(SWF_ABI_MINOR) : nullptr;
}
