#pragma once

#include "core/Version.h"

#include <cstdint>

namespace swf::inline SWF_NAMESPACE::writes
{
	/// SWF_Api::Write: the store of `kind` (SWF_WRITE_*) on `object`, between SWF_EVENT_SCENE_WRITE's PRE and POST.
	SWF_Result Write(uint32_t kind, void* object, uint64_t value);
}
