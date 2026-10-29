#include "hooking/Hooks.h"

#include "core/Log.h"
#include "hooking/Integrity.h"

#include <Windows.h>
#include <detours/detours.h>
#include <xbyak/xbyak.h>

#include <cstring>

namespace swf::inline SWF_NAMESPACE::hooking
{
	namespace
	{
		// MSVC x64 RTTI: the vtable's slot -1 points to the complete object locator, whose type
		// descriptor (image-relative) holds the decorated name after its vfptr and spare.
		struct CompleteObjectLocator
		{
			std::uint32_t signature;  // 1 on x64
			std::uint32_t offset;
			std::uint32_t cdOffset;
			std::uint32_t typeDescriptor;
			std::uint32_t classDescriptor;
			std::uint32_t self;
		};
		constexpr std::size_t kTypeDescriptorName = 0x10;
	}

	bool InImage(const Image& image, std::uintptr_t address)
	{
		return address >= image.base && address < image.base + image.size;
	}

	bool Executable(std::uintptr_t address)
	{
		MEMORY_BASIC_INFORMATION info{};
		if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT)
			return false;
		constexpr DWORD kExecute = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		return (info.Protect & kExecute) != 0 && !(info.Protect & PAGE_GUARD);
	}

	bool VtableMatches(const Image& image, const Vtable& vtable)
	{
		if (!vtable.rtti) {
			log::Warn("a vtable at +{:#x} has no address for this game version", vtable.offset);
			return false;
		}
		const std::uintptr_t at = image.base + vtable.offset;
		if (vtable.offset < sizeof(void*) || !InImage(image, at)) {
			log::Warn("vtable {} at +{:#x} is outside the image", vtable.rtti, vtable.offset);
			return false;
		}
		const auto locator = *reinterpret_cast<const std::uintptr_t*>(at - sizeof(void*));
		if (!InImage(image, locator) || !InImage(image, locator + sizeof(CompleteObjectLocator))) {
			log::Warn("vtable {} at +{:#x} has no object locator", vtable.rtti, vtable.offset);
			return false;
		}
		const auto* col = reinterpret_cast<const CompleteObjectLocator*>(locator);
		const std::uintptr_t name = image.base + col->typeDescriptor + kTypeDescriptorName;
		if (col->signature != 1 || !InImage(image, name)) {
			log::Warn("vtable {} at +{:#x} has no type descriptor", vtable.rtti, vtable.offset);
			return false;
		}
		const auto* found = reinterpret_cast<const char*>(name);
		const std::size_t length = std::strlen(vtable.rtti) + 1;
		if (!InImage(image, name + length) || std::memcmp(found, vtable.rtti, length) != 0) {
			log::Warn("vtable at +{:#x} is {:.64s}, not {}", vtable.offset, found, vtable.rtti);
			return false;
		}
		return true;
	}

	std::uintptr_t* SlotAddress(const Image& image, const Vtable& vtable, std::size_t slot)
	{
		return reinterpret_cast<std::uintptr_t*>(image.base + vtable.offset) + slot;
	}

	std::uintptr_t SlotTarget(const Image& image, const Vtable& vtable, std::size_t slot)
	{
		return *SlotAddress(image, vtable, slot);
	}

	std::uintptr_t SlotImplementation(const Image& image, const Vtable& vtable, std::size_t slot, std::uint32_t function, const char* what)
	{
		const std::uintptr_t current = SlotTarget(image, vtable, slot);
		if (!function) {
			if (InImage(image, current))
				return current;
			log::Warn("{}: {} slot {:#x} holds {:#x}, outside the image, and the table has no function for it", what, vtable.rtti, slot, current);
			return 0;
		}
		const std::uintptr_t engine = At(image, function);
		if (current == engine)
			return engine;
		if (!InImage(image, current) && Executable(current)) {
			log::Info("{}: {} slot {:#x} was swapped by {}; the engine's function behind it is hooked", what, vtable.rtti, slot, LeadsInto(current));
			return engine;
		}
		log::Warn("{}: {} slot {:#x} holds {:#x}, not the engine's +{:#x}", what, vtable.rtti, slot, current, function);
		return 0;
	}

	bool AttachAll(std::span<const Detour> detours, const char* what)
	{
		if (DetourTransactionBegin() != NO_ERROR)
			return false;
		DetourUpdateThread(GetCurrentThread());
		// The code Detours will patch: the target, past a jump it skips.
		std::vector<std::uintptr_t> starts;
		for (const Detour& detour : detours)
			starts.push_back(reinterpret_cast<std::uintptr_t>(DetourCodeFromPointer(*detour.original, nullptr)));
		for (const Detour& detour : detours) {
			if (const LONG error = DetourAttach(detour.original, detour.thunk); error != NO_ERROR) {
				DetourTransactionAbort();
				log::Warn("{}: a detour at {} could not attach (error {})", what, *detour.original, error);
				return false;
			}
		}
		if (const LONG error = DetourTransactionCommit(); error != NO_ERROR) {
			log::Warn("{}: the detours could not commit (error {})", what, error);
			return false;
		}
		for (const std::uintptr_t start : starts)
			Record(SWF_SITE_DETOUR, start, 5);
		return true;
	}

	void SwapAll(std::span<const SlotSwap> swaps)
	{
		for (const SlotSwap& swap : swaps) {
			*swap.original = reinterpret_cast<void*>(*swap.slot);
			DWORD old = 0;
			VirtualProtect(swap.slot, sizeof(*swap.slot), PAGE_READWRITE, &old);
			*swap.slot = reinterpret_cast<std::uintptr_t>(swap.thunk);
			VirtualProtect(swap.slot, sizeof(*swap.slot), old, &old);
			Record(SWF_SITE_SLOT, reinterpret_cast<std::uintptr_t>(swap.slot), sizeof(*swap.slot));
		}
	}

	PendingCalls PrepareCallSites(const Image& image, std::span<const SitePatch> sites)
	{
		// One stub per site (the call's rel32 must reach it): `mov rax, thunk; jmp rax`. rax is the call's return register, so
		// neither a direct call nor an indirect one through it needs it. An indirect site's entry is the thunk's address itself.
		struct Stubs : Xbyak::CodeGenerator
		{
			Stubs(std::span<const SitePatch> sites, std::vector<std::size_t>& entries) :
				Xbyak::CodeGenerator(sites.size() * 16 + 16)
			{
				for (const SitePatch& site : sites) {
					entries.push_back(getSize());
					if (site.indirect) {
						dq(reinterpret_cast<std::uint64_t>(site.thunk));
					} else {
						mov(rax, reinterpret_cast<std::uintptr_t>(site.thunk));
						jmp(rax);
					}
				}
			}
		};
		std::vector<std::size_t> entries;
		Stubs stubs(sites, entries);
		std::vector<CallSite> callSites;
		for (const SitePatch& site : sites)
			callSites.push_back({ site.offset, site.length, site.indirect });
		return PendingCalls(image, callSites, std::span(stubs.getCode(), stubs.getSize()), entries);
	}

	bool PatchCallSites(const Image& image, std::span<const SitePatch> sites)
	{
		PendingCalls pending = PrepareCallSites(image, sites);
		return pending && pending.Commit() != nullptr;
	}

	bool CallsTo(const Image& image, std::uint32_t site, std::uintptr_t target)
	{
		if (static_cast<std::size_t>(site) + 5 > image.size)
			return false;
		const auto* code = reinterpret_cast<const std::uint8_t*>(image.base + site);
		std::int32_t displacement = 0;
		std::memcpy(&displacement, code + 1, sizeof(displacement));
		return code[0] == 0xE8 && image.base + site + 5 + displacement == target;
	}

	std::uintptr_t CallTarget(const Image& image, std::uint32_t site)
	{
		if (!site || static_cast<std::size_t>(site) + 5 > image.size)
			return 0;
		const auto* code = reinterpret_cast<const std::uint8_t*>(image.base + site);
		if (code[0] != 0xE8)
			return 0;
		std::int32_t displacement = 0;
		std::memcpy(&displacement, code + 1, sizeof(displacement));
		return image.base + site + 5 + displacement;
	}

	std::uintptr_t ChainedCallTarget(const Image& image, std::uint32_t site, std::uintptr_t engine, const char* what)
	{
		const std::uintptr_t current = CallTarget(image, site);
		if (!current) {
			log::Warn("{}: the site at +{:#x} is not a call", what, site);
			return 0;
		}
		if (current == engine)
			return engine;
		if (!InImage(image, current) && Executable(current)) {
			log::Info("{}: the call at +{:#x} was rewritten by {}; SWF's hook calls on to it", what, site, LeadsInto(current));
			return current;
		}
		log::Warn("{}: the call at +{:#x} is not the engine's call of +{:#x}", what, site, engine ? engine - image.base : 0);
		return 0;
	}
}
