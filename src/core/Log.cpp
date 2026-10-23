#include "core/Log.h"

#include <Windows.h>
#include <ShlObj.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace swf::inline SWF_NAMESPACE::log
{
	namespace
	{
		std::atomic<SWF_LogSink> sink{ nullptr };
		std::mutex fileMutex;

		std::filesystem::path DefaultPath()
		{
			if (wchar_t buffer[MAX_PATH]; GetEnvironmentVariableW(L"SWF_LOG_PATH", buffer, MAX_PATH))
				return buffer;
			PWSTR documents = nullptr;
			if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)))
				return {};
			std::filesystem::path path(documents);
			CoTaskMemFree(documents);
			return path / L"My Games" / L"Skyrim Special Edition" / L"SKSE" / L"SceneWriterFramework.log";
		}

		// Truncates a log left by an earlier process, so copies in this process share one file.
		bool FileIsFromThisProcess(const std::filesystem::path& path)
		{
			WIN32_FILE_ATTRIBUTE_DATA attributes{};
			if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
				return false;
			FILETIME creation{}, exit{}, kernel{}, user{};
			if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
				return true;
			return CompareFileTime(&attributes.ftLastWriteTime, &creation) >= 0;
		}

		void WriteFile(int32_t level, const std::string& line)
		{
			static const std::filesystem::path path = DefaultPath();
			if (path.empty())
				return;
			static constexpr const char* kLevels[] = { "debug", "info", "warn", "error" };
			const char* levelName = level >= 0 && level <= 3 ? kLevels[level] : "info";

			std::lock_guard lock(fileMutex);
			static bool appendChecked = false;
			const bool append = appendChecked || FileIsFromThisProcess(path);
			appendChecked = true;
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			FILE* file = _wfsopen(path.c_str(), append ? L"ab" : L"wb", _SH_DENYNO);
			if (!file)
				return;
			std::fprintf(file, "[%s] %s\n", levelName, line.c_str());
			std::fclose(file);
		}
	}

	void SetSink(SWF_LogSink a_sink)
	{
		sink.store(a_sink);
	}

	const char* HostName()
	{
		static const std::string name = [] {
			HMODULE module = nullptr;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(&HostName), &module);
			char path[MAX_PATH];
			const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
			std::string full(path, length);
			const auto slash = full.find_last_of("\\/");
			return slash == std::string::npos ? full : full.substr(slash + 1);
		}();
		return name.c_str();
	}

	void Write(int32_t level, std::string_view message)
	{
		const std::string line = std::format("[{} swf {}] {}", HostName(), SWF_VERSION_STRING, message);
		if (const SWF_LogSink current = sink.load())
			current(level, line.c_str());
		else
			WriteFile(level, line);
	}
}
