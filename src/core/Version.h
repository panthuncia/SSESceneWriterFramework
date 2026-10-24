#pragma once

#include "swf/swf.h"

// SWF_VERSION_* and SWF_NAMESPACE come from cmake/SWFEmbed.cmake (cmake/SWFVersion.cmake).
#if !defined(SWF_VERSION_MAJOR) || !defined(SWF_VERSION_MINOR) || !defined(SWF_VERSION_PATCH) || !defined(SWF_VERSION_BUILD) || !defined(SWF_NAMESPACE)
#	error "Build SWF through cmake/SWFEmbed.cmake so the version defines are set"
#endif

#define SWF_STRINGIZE_(x) #x
#define SWF_STRINGIZE(x) SWF_STRINGIZE_(x)
#define SWF_VERSION_STRING                                                                    \
	SWF_STRINGIZE(SWF_VERSION_MAJOR) "." SWF_STRINGIZE(SWF_VERSION_MINOR) "." SWF_STRINGIZE(SWF_VERSION_PATCH) "." \
		SWF_STRINGIZE(SWF_VERSION_BUILD)

namespace swf::inline SWF_NAMESPACE
{
	inline constexpr uint64_t kImplVersion = SWF_MAKE_VERSION(SWF_VERSION_MAJOR, SWF_VERSION_MINOR, SWF_VERSION_PATCH, SWF_VERSION_BUILD);

	/// Runtimes this copy carries address tables for.
	inline constexpr uint64_t kRuntimeMask = SWF_RUNTIME_AE_1_6_1170 | SWF_RUNTIME_SE_1_5_97 | SWF_RUNTIME_AE_1_7_104;
}
