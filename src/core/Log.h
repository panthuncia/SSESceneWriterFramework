#pragma once

#include "core/Version.h"
#include "swf/swf_embed.h"

#include <format>
#include <string_view>

namespace swf::inline SWF_NAMESPACE::log
{
	void SetSink(SWF_LogSink sink);

	/// Writes one line, prefixed with this copy's host module and SWF version so lines
	/// from different copies in one log can be told apart.
	void Write(int32_t level, std::string_view message);

	/// File name of the module hosting this copy of SWF.
	const char* HostName();

	template <class... Args>
	void Debug(std::format_string<Args...> format, Args&&... args)
	{
		Write(SWF_LOG_DEBUG, std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args>
	void Info(std::format_string<Args...> format, Args&&... args)
	{
		Write(SWF_LOG_INFO, std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args>
	void Warn(std::format_string<Args...> format, Args&&... args)
	{
		Write(SWF_LOG_WARN, std::format(format, std::forward<Args>(args)...));
	}

	template <class... Args>
	void Error(std::format_string<Args...> format, Args&&... args)
	{
		Write(SWF_LOG_ERROR, std::format(format, std::forward<Args>(args)...));
	}
}
