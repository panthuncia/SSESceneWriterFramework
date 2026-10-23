#include "hooking/Hooks.h"

#include "core/Log.h"

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

	bool VtableMatches(const Image& image, const Vtable& vtable)
	{
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

	bool AttachAll(std::span<const Detour> detours, const char* what)
	{
		if (DetourTransactionBegin() != NO_ERROR)
			return false;
		DetourUpdateThread(GetCurrentThread());
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
		}
	}

	bool PatchCallSites(const Image& image, std::span<const std::uint32_t> sites, std::uint8_t length, void* thunk)
	{
		// One stub per site (the call's rel32 must reach it): `mov rax, thunk; jmp rax`. rax is the
		// call's return register, so the original indirect call through it leaves it free.
		struct Stubs : Xbyak::CodeGenerator
		{
			Stubs(std::size_t count, void* target, std::vector<std::size_t>& entries) :
				Xbyak::CodeGenerator(count * 16 + 16)
			{
				for (std::size_t i = 0; i < count; ++i) {
					entries.push_back(getSize());
					mov(rax, reinterpret_cast<std::uintptr_t>(target));
					jmp(rax);
				}
			}
		};
		std::vector<std::size_t> entries;
		Stubs stubs(sites.size(), thunk, entries);
		std::vector<CallSite> callSites;
		for (const std::uint32_t site : sites)
			callSites.push_back({ site, length });
		return PatchCalls(image, callSites, std::span(stubs.getCode(), stubs.getSize()), entries) != nullptr;
	}
}
