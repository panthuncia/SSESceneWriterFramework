#pragma once

#include "core/Version.h"

namespace swf::inline SWF_NAMESPACE::provider
{
	uint32_t State();

	/// Verifies and installs every family once; later calls return the cached result.
	/// Returns SWF_E_SUPERSEDED, without caching, if another candidate is already active.
	SWF_Result Activate(uint32_t candidateCount);

	/// The API table if this copy is the active provider and serves `abiMinorWanted`.
	const SWF_Api* GetApi(uint32_t abiMinorWanted);

#ifdef SWF_TESTING
	namespace testing
	{
		void ForceActivateFailure(bool fail);
		uint32_t InstallCount();
	}
#endif
}
