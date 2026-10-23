#include "tables/Tables.h"

#include "swf/swf.h"

namespace swf::inline SWF_NAMESPACE::tables
{
	namespace
	{
		const Runtime* active = nullptr;

		const Runtime& Ae()
		{
			namespace t = ae_1_6_1170;
			static const Runtime runtime{ "AE 1.6.1170", &t::Targets(), t::HiddenStores(), t::ShowSites(), t::SwitchStores(), t::FadeResetStores() };
			return runtime;
		}

		const Runtime& Se()
		{
			namespace t = se_1_5_97;
			static const Runtime runtime{ "SE 1.5.97", &t::Targets(), t::HiddenStores(), t::ShowSites(), t::SwitchStores(), t::FadeResetStores() };
			return runtime;
		}
	}

	bool Select(std::uint64_t runtime)
	{
		switch (runtime) {
		case SWF_RUNTIME_AE_1_6_1170:
			active = &Ae();
			return true;
		case SWF_RUNTIME_SE_1_5_97:
			active = &Se();
			return true;
		default:
			return false;
		}
	}

	const Runtime& Active()
	{
		return *active;
	}
}
