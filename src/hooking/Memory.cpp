#include "hooking/Memory.h"

#include "core/Log.h"
#include "hooking/Integrity.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace swf::inline SWF_NAMESPACE::hooking
{
	namespace
	{
		struct Stubs
		{
			std::uint32_t eventId;
			std::uintptr_t begin;
			std::uintptr_t end;
			std::vector<std::size_t> entries;
			std::vector<std::uint32_t> siteOffsets;
		};

		std::mutex stubsMutex;
		std::vector<Stubs> registered;

		void WriteCode(std::uintptr_t at, const void* data, std::size_t size)
		{
			DWORD old = 0;
			VirtualProtect(reinterpret_cast<void*>(at), size, PAGE_EXECUTE_READWRITE, &old);
			std::memcpy(reinterpret_cast<void*>(at), data, size);
			VirtualProtect(reinterpret_cast<void*>(at), size, old, &old);
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), size);
		}

		bool CallReaches(std::uintptr_t from, std::uintptr_t to)
		{
			const auto distance = static_cast<std::intptr_t>(to) - static_cast<std::intptr_t>(from + 5);
			return distance == static_cast<std::int32_t>(distance);
		}
	}

	Image GameImage()
	{
		const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		return { base, nt->OptionalHeader.SizeOfImage };
	}

	bool BytesMatch(const Image& image, std::uint32_t offset, const std::uint8_t* bytes, std::size_t length)
	{
		if (static_cast<std::size_t>(offset) + length > image.size)
			return false;
		return std::memcmp(reinterpret_cast<const void*>(image.base + offset), bytes, length) == 0;
	}

	std::uint8_t* PatchCalls(const Image& image, std::span<const CallSite> sites, std::span<const std::uint8_t> stubs, std::span<const std::size_t> entries)
	{
		// A block of its own just past the image: every `call rel32` must reach it.
		SYSTEM_INFO system{};
		GetSystemInfo(&system);
		const std::uintptr_t granularity = system.dwAllocationGranularity;
		const std::uintptr_t imageEnd = image.base + image.size;
		std::uint8_t* code = nullptr;
		for (std::uintptr_t at = (imageEnd + granularity - 1) & ~(granularity - 1); !code && at < imageEnd + 0x10000000; at += granularity)
			code = static_cast<std::uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(at), stubs.size(), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
		if (!code) {
			log::Warn("no free memory within reach of the image for {} bytes of stubs", stubs.size());
			return nullptr;
		}
		for (std::size_t i = 0; i < sites.size(); ++i) {
			if (!CallReaches(image.base + sites[i].offset, reinterpret_cast<std::uintptr_t>(code + entries[i]))) {
				VirtualFree(code, 0, MEM_RELEASE);
				log::Warn("stubs at {} are out of reach of the site at +{:#x}", static_cast<void*>(code), sites[i].offset);
				return nullptr;
			}
		}

		std::memcpy(code, stubs.data(), stubs.size());
		FlushInstructionCache(GetCurrentProcess(), code, stubs.size());
		for (std::size_t i = 0; i < sites.size(); ++i) {
			const std::uintptr_t at = image.base + sites[i].offset;
			std::array<std::uint8_t, 16> patch;
			patch.fill(0x90);
			patch[0] = 0xE8;
			const auto displacement = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(code + entries[i]) - static_cast<std::intptr_t>(at + 5));
			std::memcpy(patch.data() + 1, &displacement, sizeof(displacement));
			WriteCode(at, patch.data(), sites[i].length);
			Record(SWF_SITE_CALL, at, sites[i].length);
		}
		return code;
	}

	void RegisterStubs(std::uint32_t eventId, const std::uint8_t* code, std::size_t codeSize, std::vector<std::size_t> entries, std::vector<std::uint32_t> siteOffsets)
	{
		std::lock_guard lock(stubsMutex);
		const auto begin = reinterpret_cast<std::uintptr_t>(code);
		registered.push_back({ eventId, begin, begin + codeSize, std::move(entries), std::move(siteOffsets) });
	}

	bool ResolveStub(std::uintptr_t address, std::uint32_t& eventId, std::uint32_t& site, std::uint32_t& siteOffset)
	{
		std::lock_guard lock(stubsMutex);
		for (const Stubs& stubs : registered) {
			if (address < stubs.begin || address >= stubs.end)
				continue;
			// Entries are ascending; an address in a shared body after the last entry resolves to
			// the last site.
			const auto offset = address - stubs.begin;
			const auto it = std::upper_bound(stubs.entries.begin(), stubs.entries.end(), offset);
			if (it == stubs.entries.begin())
				return false;
			const auto index = static_cast<std::size_t>(it - stubs.entries.begin()) - 1;
			if (index >= stubs.siteOffsets.size())
				return false;
			eventId = stubs.eventId;
			site = static_cast<std::uint32_t>(index);
			siteOffset = stubs.siteOffsets[index];
			return true;
		}
		return false;
	}
}
