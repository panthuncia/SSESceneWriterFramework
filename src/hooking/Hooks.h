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

	/**
	 * The engine function behind slot `slot` of `vtable` (call VtableMatches first), which a family detours: `function` (an offset
	 * from its table) when the slot still holds it, or holds code outside the image (another plugin's swap, whose thunk calls on to
	 * it). With no `function` (0: the table has none for this runtime) it is the slot's implementation, which must be in the image.
	 * 0 when the slot holds a different function of the image (the table does not match the game) or nothing executable.
	 */
	std::uintptr_t SlotImplementation(const Image& image, const Vtable& vtable, std::size_t slot, std::uint32_t function, const char* what);

	/// True if `address` lies inside the image.
	bool InImage(const Image& image, std::uintptr_t address);

	/// True if `address` is in committed executable memory.
	bool Executable(std::uintptr_t address);

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

	/// One call site to rewrite as `call` to a stub that jumps to `thunk`, or as `call [rip + rel32]` through an address holding it.
	struct SitePatch
	{
		std::uint32_t offset;  // from the image base
		std::uint8_t length;   // >= 5 (>= 6 when indirect)
		void* thunk;
		bool indirect = false;
	};

	/// The stubs for every site (verified by the caller), in one block, written to the sites when committed: all or none.
	PendingCalls PrepareCallSites(const Image& image, std::span<const SitePatch> sites);

	/// PrepareCallSites, committed at once.
	bool PatchCallSites(const Image& image, std::span<const SitePatch> sites);

	/// True if the 5 bytes at `site` are `call rel32` to `target`.
	bool CallsTo(const Image& image, std::uint32_t site, std::uintptr_t target);

	/// The target of the `call rel32` at `site`; 0 if it is not one.
	std::uintptr_t CallTarget(const Image& image, std::uint32_t site);

	/**
	 * What a call site holds now, for a hook that calls on to it: `engine` when the site still calls it, or the site's current target
	 * when that is executable code outside the image (another plugin's hook there, which calls on to the engine's). 0 for anything
	 * else: not a `call rel32`, or a call to a different function of the image (the table does not match the game). `what` names the
	 * family in the log, which also names the module a chained site leads into.
	 */
	std::uintptr_t ChainedCallTarget(const Image& image, std::uint32_t site, std::uintptr_t engine, const char* what);
}
