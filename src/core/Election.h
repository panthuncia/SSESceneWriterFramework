// The provider election, run by every SWF copy at kPostLoad (SWF_Embed_OnPostLoad).
//
// Every copy runs this same code, so the result does not depend on which copy runs it first.
// The algorithm is frozen for ABI major 1:
//
//  1. Find every module exporting SWF_Candidate_v1.
//  2. If one candidate is already active, that is the provider. Done.
//  3. Otherwise drop candidates of another ABI major or without an address table for the
//     running game, then try Activate() in this order: pinned by SceneWriterFramework.ini
//     first, then implVersion descending, then module file name ascending (case-insensitive),
//     then full module path ascending. The first candidate whose Activate() succeeds is the
//     provider.
//
// Clients do not elect: they find the active candidate (swf/swf_loader.h).
//
// Testing overrides: SWF_RUNTIME_OVERRIDE (runtime bit mask) and SWF_INI_PATH.
#pragma once

#include "swf/swf.h"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")

namespace swf::inline SWF_NAMESPACE::election
{
	struct Candidate
	{
		const SWF_CandidateInfo* info = nullptr;
		HMODULE module = nullptr;
		std::wstring path;
	};

	struct Settings
	{
		bool disabled = false;
		std::string pinHost;
		uint64_t pinVersion = 0;  // 0 = no pin
		uint64_t pinVersionMask = 0;
	};

	struct ElectionReport
	{
		uint64_t runtime = 0;
		Settings settings;
		static constexpr size_t npos = ~size_t(0);

		std::vector<Candidate> candidates; // every module exporting SWF_Candidate_v1
		size_t provider = npos;            // index into candidates
		bool alreadyActive = false;        // provider was active before this election
		std::vector<size_t> failed;        // tried and failed, in order

		const Candidate* Provider() const { return provider == npos ? nullptr : &candidates[provider]; }
	};

	namespace detail
	{
		inline bool EnvValue(const char* name, std::string& out)
		{
			char buffer[MAX_PATH];
			const DWORD length = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
			if (length == 0 || length >= sizeof(buffer))
				return false;
			out.assign(buffer, length);
			return true;
		}

		inline std::wstring ExeDirectory()
		{
			wchar_t path[MAX_PATH];
			const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
			std::wstring result(path, length);
			const auto slash = result.find_last_of(L"\\/");
			return slash == std::wstring::npos ? std::wstring() : result.substr(0, slash + 1);
		}

		// Parses "major.minor.patch[.build]"; the mask covers the parts given.
		inline bool ParseVersion(const std::string& text, uint64_t& version, uint64_t& mask)
		{
			uint64_t parts[4] = {};
			int count = 0;
			const char* cursor = text.c_str();
			while (*cursor && count < 4) {
				char* end = nullptr;
				const unsigned long value = std::strtoul(cursor, &end, 10);
				if (end == cursor || value > 0xFFFF)
					return false;
				parts[count++] = value;
				cursor = end;
				if (*cursor == '.')
					++cursor;
				else if (*cursor)
					return false;
			}
			if (count < 3)
				return false;
			version = SWF_MAKE_VERSION(parts[0], parts[1], parts[2], parts[3]);
			mask = count == 4 ? ~0ull : ~0xFFFFull;
			return true;
		}

		inline size_t Find(const std::vector<Candidate>& candidates, uint32_t state)
		{
			for (size_t i = 0; i < candidates.size(); ++i)
				if (candidates[i].info->GetState() == state)
					return i;
			return ElectionReport::npos;
		}
	}

	/// Runtime bit of the game executable at `path`, or 0 if SWF has no table for it.
	inline uint64_t RuntimeOf(const wchar_t* path)
	{
		DWORD handle = 0;
		const DWORD size = GetFileVersionInfoSizeW(path, &handle);
		if (!size)
			return 0;
		std::vector<uint8_t> data(size);
		VS_FIXEDFILEINFO* info = nullptr;
		UINT length = 0;
		if (!GetFileVersionInfoW(path, 0, size, data.data()) ||
			!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || !info)
			return 0;

		uint32_t major = HIWORD(info->dwFileVersionMS), minor = LOWORD(info->dwFileVersionMS);
		uint32_t build = HIWORD(info->dwFileVersionLS);
		// SE 1.5.97's fixed version is 1.0.0.0: its version is only in the FileVersion string (as CommonLib reads it).
		if (major == 1 && minor == 0 && build == 0) {
			struct Translation
			{
				WORD language;
				WORD codePage;
			}* translation = nullptr;
			wchar_t* text = nullptr;
			if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translation), &length) && translation &&
				length >= sizeof(Translation)) {
				wchar_t key[64];
				swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\FileVersion", translation->language, translation->codePage);
				if (VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&text), &length) && text)
					swscanf_s(text, L"%u.%u.%u", &major, &minor, &build);
			}
		}
		if (major == 1 && minor == 6 && build == 1170)
			return SWF_RUNTIME_AE_1_6_1170;
		if (major == 1 && minor == 7 && build == 104)
			return SWF_RUNTIME_AE_1_7_104;
		if (major == 1 && minor == 5 && build == 97)
			return SWF_RUNTIME_SE_1_5_97;
		return 0;
	}

	/// Runtime bit of the running game executable, or 0 if SWF has no table for it.
	inline uint64_t RunningRuntime()
	{
		std::string overrideText;
		if (detail::EnvValue("SWF_RUNTIME_OVERRIDE", overrideText))
			return std::strtoull(overrideText.c_str(), nullptr, 0);

		wchar_t path[MAX_PATH];
		if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
			return 0;
		return RuntimeOf(path);
	}

	/// Reads [Election] from Data/SKSE/Plugins/SceneWriterFramework.ini next to the game.
	inline Settings ReadSettings()
	{
		std::string iniPath;
		if (!detail::EnvValue("SWF_INI_PATH", iniPath)) {
			const std::wstring wide = detail::ExeDirectory() + L"Data\\SKSE\\Plugins\\SceneWriterFramework.ini";
			char narrow[MAX_PATH * 2];
			const int length = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, narrow, sizeof(narrow), nullptr, nullptr);
			if (length <= 0)
				return {};
			iniPath = narrow;
		}

		Settings settings;
		char value[MAX_PATH];
		GetPrivateProfileStringA("Election", "Disable", "0", value, sizeof(value), iniPath.c_str());
		settings.disabled = value[0] == '1' || _stricmp(value, "true") == 0;
		GetPrivateProfileStringA("Election", "PinHost", "", value, sizeof(value), iniPath.c_str());
		settings.pinHost = value;
		GetPrivateProfileStringA("Election", "PinVersion", "", value, sizeof(value), iniPath.c_str());
		if (value[0] && !detail::ParseVersion(value, settings.pinVersion, settings.pinVersionMask))
			settings.pinVersion = settings.pinVersionMask = 0;
		return settings;
	}

	/// Every loaded module exporting SWF_Candidate_v1, in module-list order.
	inline std::vector<Candidate> FindCandidates()
	{
		std::vector<HMODULE> modules(256);
		DWORD needed = 0;
		for (;;) {
			const DWORD bytes = static_cast<DWORD>(modules.size() * sizeof(HMODULE));
			if (!EnumProcessModules(GetCurrentProcess(), modules.data(), bytes, &needed))
				return {};
			if (needed <= bytes)
				break;
			modules.resize(needed / sizeof(HMODULE));
		}
		modules.resize(needed / sizeof(HMODULE));

		std::vector<Candidate> candidates;
		for (const HMODULE module : modules) {
			const auto export_ = reinterpret_cast<SWF_CandidateFn>(GetProcAddress(module, SWF_CANDIDATE_EXPORT));
			if (!export_)
				continue;
			const SWF_CandidateInfo* info = export_();
			if (!info || info->size < sizeof(SWF_CandidateInfo) || !info->GetState || !info->Activate || !info->GetApi)
				continue;
			wchar_t path[MAX_PATH];
			const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
			candidates.push_back({ info, module, std::wstring(path, length) });
		}
		return candidates;
	}

	/// Elects the provider: finds the active one, or activates one if none is.
	inline ElectionReport Elect()
	{
		ElectionReport report;
		report.candidates = FindCandidates();

		report.provider = detail::Find(report.candidates, SWF_STATE_ACTIVE);
		if (report.provider != ElectionReport::npos) {
			report.alreadyActive = true;
			return report;
		}
		report.settings = ReadSettings();
		report.runtime = RunningRuntime();
		if (report.settings.disabled || report.runtime == 0)
			return report;

		const Settings& settings = report.settings;
		auto pinned = [&](const Candidate& candidate) {
			if (!settings.pinHost.empty() && _stricmp(settings.pinHost.c_str(), candidate.info->hostName) == 0)
				return true;
			return settings.pinVersion != 0 &&
			       (candidate.info->implVersion & settings.pinVersionMask) == settings.pinVersion;
		};

		std::vector<size_t> order;
		for (size_t i = 0; i < report.candidates.size(); ++i) {
			const SWF_CandidateInfo* info = report.candidates[i].info;
			if (info->abiMajor == SWF_ABI_MAJOR && (info->runtimeMask & report.runtime))
				order.push_back(i);
		}
		std::sort(order.begin(), order.end(), [&](size_t ia, size_t ib) {
			const Candidate* a = &report.candidates[ia];
			const Candidate* b = &report.candidates[ib];
			const bool pinA = pinned(*a), pinB = pinned(*b);
			if (pinA != pinB)
				return pinA;
			if (a->info->implVersion != b->info->implVersion)
				return a->info->implVersion > b->info->implVersion;
			if (const int byName = _stricmp(a->info->hostName, b->info->hostName))
				return byName < 0;
			return _wcsicmp(a->path.c_str(), b->path.c_str()) < 0;
		});

		const auto count = static_cast<uint32_t>(report.candidates.size());
		for (const size_t index : order) {
			const SWF_Result result = report.candidates[index].info->Activate(count);
			if (result == SWF_OK) {
				report.provider = index;
				return report;
			}
			if (result == SWF_E_SUPERSEDED) {
				// Another candidate activated meanwhile; it is the provider.
				report.provider = detail::Find(report.candidates, SWF_STATE_ACTIVE);
				report.alreadyActive = report.provider != ElectionReport::npos;
				return report;
			}
			report.failed.push_back(index);
		}
		return report;
	}
}
