#pragma once

#include "hooking/Memory.h"
#include "tables/StoreSites.h"

namespace swf::inline SWF_NAMESPACE::stores
{
	bool VerifyHidden(const hooking::Image& image, std::span<const HiddenStoreSite> sites);
	/// `showSites` are the offsets of the sites SWF_FILTER_SHOW may veto.
	bool InstallHidden(const hooking::Image& image, std::span<const HiddenStoreSite> sites, std::span<const std::uint32_t> showSites);

	bool VerifyValues(const hooking::Image& image, std::span<const StoreValueSite> sites);
	bool InstallSwitchIndex(const hooking::Image& image, std::span<const StoreValueSite> sites);
	bool InstallFadeReset(const hooking::Image& image, std::span<const StoreValueSite> sites);

	/// Installed sites behind a store event id.
	std::uint32_t SiteCount(std::uint32_t eventId);
}
