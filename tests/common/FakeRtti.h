#pragma once

// Fake vtables in static data carrying real MSVC x64 RTTI layouts, so VtableMatches reads them as
// it reads the game's: [locator][type descriptor][slot -1][slots...], all inside this image.
#include "hooking/Hooks.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition)                                                                       \
	do {                                                                                       \
		if (!(condition)) {                                                                    \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
			std::exit(1);                                                                      \
		}                                                                                      \
	} while (0)

namespace fake
{
	inline swf::hooking::Image image;

	struct TypeDescriptor
	{
		void* vfptr;
		void* spare;
		char name[96];
	};
	struct Locator
	{
		std::uint32_t signature, offset, cdOffset, typeDescriptor, classDescriptor, self;
	};
	struct Vtable
	{
		Locator locator;
		TypeDescriptor type;
		void* locatorPointer;  // slot -1
		void* slots[0x40];
	};
	inline Vtable vtables[48];
	inline int nextVtable = 0;

	inline std::uint32_t Rva(const void* address)
	{
		return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address) - image.base);
	}

	inline Vtable& NewVtable(const char* rtti)
	{
		Vtable& vtable = vtables[nextVtable++];
		strcpy_s(vtable.type.name, rtti);
		vtable.locator.signature = 1;
		vtable.locator.typeDescriptor = Rva(&vtable.type);
		vtable.locatorPointer = &vtable.locator;
		return vtable;
	}

	inline swf::hooking::Vtable Ref(Vtable& vtable)
	{
		return { Rva(vtable.slots), vtable.type.name };
	}

	template <class... A>
	void Call(Vtable& vtable, std::size_t slot, A... args)
	{
		reinterpret_cast<void (*)(A...)>(vtable.slots[slot])(args...);
	}

	/// Executable memory within rel32 reach of `target`.
	inline std::uint8_t* AllocateNear(const void* target, std::size_t size)
	{
		SYSTEM_INFO system{};
		GetSystemInfo(&system);
		const std::uintptr_t granularity = system.dwAllocationGranularity;
		const auto origin = reinterpret_cast<std::uintptr_t>(target) & ~(granularity - 1);
		for (std::uintptr_t distance = granularity; distance < 0x70000000; distance += granularity) {
			for (const std::uintptr_t at : { origin + distance, origin - distance })
				if (void* block = VirtualAlloc(reinterpret_cast<void*>(at), size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
					return static_cast<std::uint8_t*>(block);
		}
		return nullptr;
	}

	/// Another plugin's relay outside the image, within rel32 reach of `site`: `mov rax, to; jmp rax`, as a hook's trampoline
	/// region holds.
	inline void* ForeignRelay(const void* site, const void* to)
	{
		auto* stub = AllocateNear(site, 16);
		CHECK(stub);
		const auto address = reinterpret_cast<std::uintptr_t>(to);
		stub[0] = 0x48;
		stub[1] = 0xB8;
		std::memcpy(stub + 2, &address, sizeof(address));
		stub[10] = 0xFF;
		stub[11] = 0xE0;
		FlushInstructionCache(GetCurrentProcess(), stub, 16);
		return stub;
	}
}
