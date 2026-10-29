#pragma once

#include "core/Version.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace swf::inline SWF_NAMESPACE::hooking
{
	/// A code image the families patch: the game executable, or a test's synthetic image.
	struct Image
	{
		std::uintptr_t base = 0;
		std::size_t size = 0;
	};

	Image GameImage();

	/// The address of `offset` in `image`. A table holds 0 for an address it does not have for its
	/// runtime: that gives 0, which no image contains, so the family using it does not verify.
	inline std::uintptr_t At(const Image& image, std::uint32_t offset)
	{
		return offset ? image.base + offset : 0;
	}

	/// True if `length` bytes at `address` are committed, readable memory in one region.
	bool Readable(std::uintptr_t address, std::size_t length);

	/// True if `length` bytes at `offset` lie inside the image and equal `bytes`.
	bool BytesMatch(const Image& image, std::uint32_t offset, const std::uint8_t* bytes, std::size_t length);

	struct CallSite
	{
		std::uint32_t offset;   // from the image base
		std::uint8_t length;    // >= 5: `call rel32` to the entry, then NOPs; >= 6 when indirect
		bool indirect = false;  // `call [rip + rel32]` through the entry, an 8-byte address (write_call<6>'s form)
	};

	/**
	 * Stubs copied into memory just past the image, not yet called from anywhere: Commit rewrites each site as a call to its entry
	 * (`entries[i]`, an offset into the stubs). False when no memory within rel32 reach of every site is free, with nothing written.
	 * Dropped uncommitted, the copy is freed. A family that also detours prepares its sites first, so a failure to place them leaves
	 * nothing installed.
	 */
	class PendingCalls
	{
	public:
		PendingCalls(const Image& image, std::span<const CallSite> sites, std::span<const std::uint8_t> stubs, std::span<const std::size_t> entries);
		PendingCalls(PendingCalls&& other) noexcept;
		PendingCalls(const PendingCalls&) = delete;
		PendingCalls& operator=(const PendingCalls&) = delete;
		PendingCalls& operator=(PendingCalls&&) = delete;
		~PendingCalls();

		explicit operator bool() const { return code != nullptr; }
		/// Rewrites the sites. Returns the stub copy.
		std::uint8_t* Commit();

	private:
		Image image;
		std::vector<CallSite> sites;
		std::vector<std::size_t> entries;
		std::uint8_t* code = nullptr;
		bool committed = false;
	};

	/// PendingCalls, committed at once: returns the stub copy, or null with nothing written.
	std::uint8_t* PatchCalls(const Image& image, std::span<const CallSite> sites, std::span<const std::uint8_t> stubs, std::span<const std::size_t> entries);

	/// Records a patched family's stubs, so ResolveStub can map an address back to its site.
	void RegisterStubs(std::uint32_t eventId, const std::uint8_t* code, std::size_t codeSize, std::vector<std::size_t> entries, std::vector<std::uint32_t> siteOffsets);

	/// Maps an address inside registered stubs to (event id, site index, site offset).
	bool ResolveStub(std::uintptr_t address, std::uint32_t& eventId, std::uint32_t& site, std::uint32_t& siteOffset);
}
