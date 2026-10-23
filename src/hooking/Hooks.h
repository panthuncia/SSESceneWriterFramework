#pragma once

#include "hooking/Memory.h"

#include <cstdint>
#include <span>
#include <vector>

namespace swf::inline SWF_NAMESPACE::hooking
{
	/// A vtable in the image, named by its RTTI type descriptor (".?AVNiNode@@").
	struct Vtable
	{
		std::uint32_t offset;  // from the image base
		const char* rtti;
	};

	/// True if `vtable` lies in the image and its complete object locator names `vtable.rtti`.
	/// Logs the name found when it differs.
	bool VtableMatches(const Image& image, const Vtable& vtable);

	/// The implementation in slot `slot` of `vtable` (call VtableMatches first).
	std::uintptr_t SlotTarget(const Image& image, const Vtable& vtable, std::size_t slot);

	/// True if `address` lies inside the image.
	bool InImage(const Image& image, std::uintptr_t address);

	/// One Detours detour: `*original` holds the target on entry and the trampoline after.
	struct Detour
	{
		void** original;
		void* thunk;
	};

	/// One vtable slot swap: `*original` receives the slot's implementation.
	struct SlotSwap
	{
		std::uintptr_t* slot;
		void** original;
		void* thunk;
	};

	/// Attaches every detour in one Detours transaction: all or none.
	bool AttachAll(std::span<const Detour> detours, const char* what);

	/// Swaps every slot (after AttachAll, which may already have hooked the same implementation).
	void SwapAll(std::span<const SlotSwap> swaps);

	/// Address of slot `slot` in `vtable`.
	std::uintptr_t* SlotAddress(const Image& image, const Vtable& vtable, std::size_t slot);

	/// Rewrites each 6-byte site (verified by the caller) as `call` to a stub that jumps to `thunk`.
	bool PatchCallSites(const Image& image, std::span<const std::uint32_t> sites, std::uint8_t length, void* thunk);
}
