#pragma once

#include "core/Version.h"

#include <array>
#include <cstdint>
#include <span>

namespace swf::inline SWF_NAMESPACE
{
	/// A store to NiAVObject::flags (+0xF4) that can change kHidden (bit 0). The stub runs the
	/// original instruction itself and compares bit 0 before and after.
	struct HiddenStoreSite
	{
		std::uint32_t offset;  // from the image base
		std::array<std::uint8_t, 12> bytes;
		std::uint8_t length;
		int base;              // Xbyak::Operand register index
		bool readModifyWrite;  // `or`/`and`: rerun after the flags are restored; a `mov` runs once
	};

	/// A `mov dword ptr [base + disp], value` that SWF takes over: the stub hands
	/// (base, value) to a handler, which performs the store and fires the event.
	struct StoreValueSite
	{
		std::uint32_t offset;  // from the image base
		std::array<std::uint8_t, 10> bytes;
		std::uint8_t length;
		int base;   // Xbyak::Operand register index
		int value;  // register index, or kImmediate for `imm`
		std::uint32_t imm = 0;
		static constexpr int kImmediate = -1;
	};

	namespace tables::ae_1_6_1170
	{
		std::span<const HiddenStoreSite> HiddenStores();
		std::span<const std::uint32_t> ShowSites();
		std::span<const StoreValueSite> SwitchStores();
		std::span<const StoreValueSite> FadeResetStores();
	}

	namespace tables::ae_1_7_104
	{
		std::span<const HiddenStoreSite> HiddenStores();
		std::span<const std::uint32_t> ShowSites();
		std::span<const StoreValueSite> SwitchStores();
		std::span<const StoreValueSite> FadeResetStores();
	}

	namespace tables::se_1_5_97
	{
		std::span<const HiddenStoreSite> HiddenStores();
		std::span<const std::uint32_t> ShowSites();
		std::span<const StoreValueSite> SwitchStores();
		std::span<const StoreValueSite> FadeResetStores();
	}
}
