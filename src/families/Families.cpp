#include "families/Families.h"

#include "families/Stores.h"
#include "tables/StoreSites.h"

namespace swf::inline SWF_NAMESPACE
{
	namespace
	{
		namespace table = tables::ae_1_6_1170;

		bool VerifyHidden() { return stores::VerifyHidden(hooking::GameImage(), table::HiddenStores()); }
		bool InstallHidden() { return stores::InstallHidden(hooking::GameImage(), table::HiddenStores(), table::ShowSites()); }
		bool VerifySwitch() { return stores::VerifyValues(hooking::GameImage(), table::SwitchStores()); }
		bool InstallSwitch() { return stores::InstallSwitchIndex(hooking::GameImage(), table::SwitchStores()); }
		bool VerifyFadeReset() { return stores::VerifyValues(hooking::GameImage(), table::FadeResetStores()); }
		bool InstallFadeReset() { return stores::InstallFadeReset(hooking::GameImage(), table::FadeResetStores()); }

		// No family is required: a provider with some families missing still serves the rest,
		// and each client decides from the capability bits whether it can run.
		constexpr Family kFamilies[] = {
			{ "hidden", SWF_CAP_HIDDEN, false, &VerifyHidden, &InstallHidden },
			{ "switch-index", SWF_CAP_SWITCH_INDEX, false, &VerifySwitch, &InstallSwitch },
			{ "fade-reset", SWF_CAP_FADE_RESET, false, &VerifyFadeReset, &InstallFadeReset },
		};
	}

	std::span<const Family> Families()
	{
		return kFamilies;
	}

	SWF_Caps CapFor(uint32_t id, bool filter)
	{
		if (filter)
			return id == SWF_FILTER_SHOW ? SWF_CAP_HIDDEN : ~0ull;
		switch (id) {
		case SWF_EVENT_WRITTEN:
			return 0;
		case SWF_EVENT_HIDDEN:
			return SWF_CAP_HIDDEN;
		case SWF_EVENT_SWITCH_INDEX:
			return SWF_CAP_SWITCH_INDEX;
		case SWF_EVENT_FADE_RESET:
			return SWF_CAP_FADE_RESET;
		default:
			return ~0ull;
		}
	}
}
