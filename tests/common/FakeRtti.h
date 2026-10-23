#pragma once

// Fake vtables in static data carrying real MSVC x64 RTTI layouts, so VtableMatches reads them as
// it reads the game's: [locator][type descriptor][slot -1][slots...], all inside this image.
#include "hooking/Hooks.h"

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
	inline Vtable vtables[24];
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
}
