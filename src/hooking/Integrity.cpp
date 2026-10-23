// The integrity check: every byte SWF patched, read again later, and each change traced to the module
// whose code it now leads into. It repairs nothing: another plugin's later hook may still chain to SWF's.
#include "hooking/Integrity.h"

#include "core/Log.h"
#include "hooking/Memory.h"

#include <Windows.h>

#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace swf::inline SWF_NAMESPACE::hooking
{
	namespace
	{
		struct Patch
		{
			std::uint32_t kind;
			std::uintptr_t address;
			std::array<std::uint8_t, 16> bytes;
			std::uint8_t length;
			const char* family;
		};

		std::mutex mutex;
		std::vector<Patch> patches;
		const char* currentFamily = nullptr;

		bool Readable(std::uintptr_t address, std::size_t length)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT)
				return false;
			if (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))
				return false;
			return address + length <= reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
		}

		HMODULE ModuleAt(std::uintptr_t address)
		{
			HMODULE module = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(address), &module);
			return module;
		}

		// One jump's destination, for the forms hooks are made of; 0 for anything else.
		std::uintptr_t JumpTarget(std::uintptr_t at)
		{
			if (!Readable(at, 14))
				return 0;
			const auto* code = reinterpret_cast<const std::uint8_t*>(at);
			std::int32_t rel = 0;
			if (code[0] == 0xE8 || code[0] == 0xE9) {  // call / jmp rel32
				std::memcpy(&rel, code + 1, sizeof(rel));
				return at + 5 + rel;
			}
			if (code[0] == 0xEB)  // jmp rel8
				return at + 2 + static_cast<std::int8_t>(code[1]);
			if ((code[0] == 0xFF && (code[1] == 0x25 || code[1] == 0x15))) {  // jmp / call [rip + rel32]
				std::memcpy(&rel, code + 2, sizeof(rel));
				const std::uintptr_t slot = at + 6 + rel;
				return Readable(slot, sizeof(std::uintptr_t)) ? *reinterpret_cast<const std::uintptr_t*>(slot) : 0;
			}
			std::uintptr_t imm = 0;
			if (code[0] == 0x48 && code[1] == 0xB8 && code[10] == 0xFF && code[11] == 0xE0) {  // mov rax, imm64; jmp rax
				std::memcpy(&imm, code + 2, sizeof(imm));
				return imm;
			}
			if (code[0] == 0x49 && code[1] == 0xBB && code[10] == 0x41 && code[11] == 0xFF && code[12] == 0xE3) {  // mov r11, imm64; jmp r11
				std::memcpy(&imm, code + 2, sizeof(imm));
				return imm;
			}
			return 0;
		}

		// Where a changed patch now leads: through relays outside any module (stub blocks, trampolines'
		// jump regions) to the first address inside one, or the last address reached.
		std::uintptr_t Lead(const Patch& patch)
		{
			std::uintptr_t at = patch.kind == SWF_SITE_SLOT ? *reinterpret_cast<const std::uintptr_t*>(patch.address) : JumpTarget(patch.address);
			for (int hop = 0; at && hop < 4 && !ModuleAt(at); ++hop) {
				const std::uintptr_t next = JumpTarget(at);
				if (!next)
					break;
				at = next;
			}
			return at;
		}

		std::string ModuleName(std::uintptr_t address)
		{
			const HMODULE module = address ? ModuleAt(address) : nullptr;
			if (!module)
				return {};
			char path[MAX_PATH]{};
			const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
			std::string name(path, length);
			const auto slash = name.find_last_of("\\/");
			return slash == std::string::npos ? name : name.substr(slash + 1);
		}

		const char* KindName(std::uint32_t kind)
		{
			switch (kind) {
			case SWF_SITE_DETOUR:
				return "detour";
			case SWF_SITE_CALL:
				return "call site";
			case SWF_SITE_SLOT:
				return "vtable slot";
			default:
				return "patch";
			}
		}
	}

	void SetFamily(const char* name)
	{
		std::lock_guard lock(mutex);
		currentFamily = name;
	}

	void Record(std::uint32_t kind, std::uintptr_t address, std::size_t length)
	{
		Patch patch{ kind, address, {}, static_cast<std::uint8_t>(length < 16 ? length : 16), nullptr };
		std::memcpy(patch.bytes.data(), reinterpret_cast<const void*>(address), patch.length);
		std::lock_guard lock(mutex);
		patch.family = currentFamily;
		for (Patch& existing : patches) {
			if (existing.address == address) {
				existing = patch;
				return;
			}
		}
		patches.push_back(patch);
	}

	std::uint32_t CheckIntegrity(SWF_IntegrityFn fn, void* user)
	{
		std::vector<Patch> snapshot;
		{
			std::lock_guard lock(mutex);
			snapshot = patches;
		}
		const Image image = GameImage();
		std::uint32_t changed = 0;
		for (const Patch& patch : snapshot) {
			if (std::memcmp(reinterpret_cast<const void*>(patch.address), patch.bytes.data(), patch.length) == 0)
				continue;
			++changed;
			const std::uintptr_t target = Lead(patch);
			const std::string module = ModuleName(target);
			const bool inImage = patch.address >= image.base && patch.address < image.base + image.size;
			log::Warn("integrity: the {} at {}{:#x} ({}) was rewritten; it now leads to {} ({:#x})", KindName(patch.kind), inImage ? "+" : "",
				inImage ? patch.address - image.base : patch.address, patch.family ? patch.family : "?", module.empty() ? "no module" : module, target);
			if (fn) {
				SWF_IntegrityEntry entry{};
				entry.size = sizeof(entry);
				entry.kind = patch.kind;
				entry.address = patch.address;
				entry.offset = inImage ? static_cast<std::uint32_t>(patch.address - image.base) : 0;
				entry.family = patch.family;
				entry.module = module.empty() ? nullptr : module.c_str();
				entry.target = target;
				fn(&entry, user);
			}
		}
		log::Info("integrity: {} of {} patches unchanged", snapshot.size() - changed, snapshot.size());
		return changed;
	}
}
