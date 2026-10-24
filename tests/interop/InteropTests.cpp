// SWF's hooks beside other plugins' hooks on the same targets, installed before and after SWF:
//
//   - a Detours detour (another plugin's Detours copy, or CommonLib's stl::detour_thunk);
//   - a hand-rolled prologue hook: whole instructions copied to a stub that jumps back, then a
//     `jmp rel32` written over the function start (MinHook, xbyak-built hooks, write_branch<5>);
//   - a vtable slot swap that keeps the previous implementation (CommonLib's write_vfunc);
//   - a call-site rewrite after SWF's that keeps the previous call target (write_call<5>).
//
// Each case checks every hook ran, once, in the order the install order implies: the later hook
// is the outer one. The integrity check then reports exactly the patches rewritten after SWF's,
// each traced to this executable, whose functions the foreign hooks are.
#include "../common/FakeRtti.h"
#include "core/Dispatch.h"
#include "families/DetourFamilies.h"
#include "hooking/Hooks.h"
#include "hooking/Integrity.h"

#include <Windows.h>
#include <detours/detours.h>

#include <string>
#include <vector>

namespace
{
	using namespace swf;
	using namespace fake;
	using FakeVtable = fake::Vtable;

	std::vector<std::string> trace;

	// ---- The "engine": the fade calls and the property calls, in this image.
	__declspec(noinline) void EngineOnVisible(void*, void*, std::int32_t) { trace.push_back("engine onVisible"); }
	__declspec(noinline) std::uint64_t EngineUpdate(void*, float amount, void*)
	{
		trace.push_back("engine update");
		return static_cast<std::uint64_t>(amount * 100.0f);
	}
	__declspec(noinline) void EngineLodLevel(void*, float level) { trace.push_back("engine lodLevel " + std::to_string(static_cast<int>(level))); }
	__declspec(noinline) void EngineSetRange(void*, float, float farDistance) { trace.push_back("engine setRange " + std::to_string(static_cast<int>(farDistance))); }
	__declspec(noinline) void EngineSetLodType(void*, std::uint8_t type) { trace.push_back("engine lodType " + std::to_string(type)); }
	__declspec(noinline) std::uint64_t EngineSnap(void*, void*) { trace.push_back("engine snap"); return 0; }

	template <int V>
	__declspec(noinline) void EngineProperty() { trace.push_back("engine property " + std::to_string(V)); }
	__declspec(noinline) void* EngineRenderPasses(void* property, void*, std::uint32_t, void*)
	{
		trace.push_back("engine renderPasses");
		return property;
	}
	__declspec(noinline) void EngineMaterialAlpha(void*, float alpha) { trace.push_back("engine alpha " + std::to_string(static_cast<int>(alpha * 10))); }

	// ---- SWF's subscriber: every call event, both phases.
	void Record(const SWF_EventHeader* header, std::uintptr_t*, void*)
	{
		const auto& event = *reinterpret_cast<const SWF_CallEvent*>(header);
		trace.push_back(std::string(header->phase == SWF_PHASE_PRE ? "swf pre " : "swf post ") + std::to_string(event.hook));
	}

	void Subscribe(std::uint32_t hook)
	{
		dispatch::Entry entry;
		entry.event = &Record;
		entry.phases = SWF_PHASE_PRE | SWF_PHASE_POST;
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Events().Add(SWF_EVENT_CALL(hook), entry, &subscription) == SWF_OK);
	}

	void Expect(std::initializer_list<const char*> expected)
	{
		bool same = trace.size() == expected.size();
		for (std::size_t i = 0; same && i < trace.size(); ++i)
			same = trace[i] == expected.begin()[i];
		if (!same) {
			std::fprintf(stderr, "trace:\n");
			for (const auto& line : trace)
				std::fprintf(stderr, "  %s\n", line.c_str());
		}
		CHECK(same);
		trace.clear();
	}

	// Executable memory within rel32 reach of `target`.
	std::uint8_t* AllocateNear(const void* target, std::size_t size)
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

	void WriteCode(void* at, const void* bytes, std::size_t size)
	{
		DWORD old = 0;
		VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old);
		std::memcpy(at, bytes, size);
		VirtualProtect(at, size, old, &old);
		FlushInstructionCache(GetCurrentProcess(), at, size);
	}

	void WriteRel32(std::uint8_t opcode, void* at, const void* to)
	{
		std::uint8_t bytes[5] = { opcode };
		const auto displacement = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(to) - (reinterpret_cast<std::intptr_t>(at) + 5));
		std::memcpy(bytes + 1, &displacement, sizeof(displacement));
		WriteCode(at, bytes, sizeof(bytes));
	}

	/// A hand-rolled prologue hook on `target`: copies whole instructions covering 5 bytes to a
	/// stub that jumps back after them, then writes `jmp rel32` to `hook` over the start. Returns
	/// the stub, which calls on to the target. Detours' disassembler finds the instruction
	/// boundaries and relocates relative operands, as such hooks do with their own.
	void* HandRolled(void* target, void* hook)
	{
		auto* stub = AllocateNear(target, 64);
		CHECK(stub);
		auto* from = static_cast<std::uint8_t*>(target);
		auto* to = stub;
		while (from - static_cast<std::uint8_t*>(target) < 5) {
			auto* next = static_cast<std::uint8_t*>(DetourCopyInstruction(to, nullptr, from, nullptr, nullptr));
			to += next - from;
			from = next;
		}
		// jmp [rip + 0]; dq from
		const std::uint8_t jump[] = { 0xFF, 0x25, 0, 0, 0, 0 };
		std::memcpy(to, jump, sizeof(jump));
		std::memcpy(to + sizeof(jump), &from, sizeof(from));
		FlushInstructionCache(GetCurrentProcess(), stub, 64);
		WriteRel32(0xE9, target, hook);
		return stub;
	}

	/// Another plugin's Detours detour on `*target`; `*target` becomes its trampoline.
	void ForeignDetour(void** target, void* hook)
	{
		CHECK(DetourTransactionBegin() == NO_ERROR);
		DetourUpdateThread(GetCurrentThread());
		CHECK(DetourAttach(target, hook) == NO_ERROR);
		CHECK(DetourTransactionCommit() == NO_ERROR);
	}

	/// A write_vfunc-style swap: the slot's previous implementation becomes `*original`.
	void ForeignSwap(void** slot, void* hook, void** original)
	{
		*original = *slot;
		WriteCode(slot, &hook, sizeof(hook));
	}

	// ---- The foreign hooks: each traces, then calls on.
	std::uint64_t (*foreignUpdate)(void*, float, void*) = nullptr;
	std::uint64_t ForeignUpdate(void* fade, float amount, void* camera)
	{
		trace.push_back("foreign update");
		return foreignUpdate(fade, amount, camera);
	}
	void (*foreignLodLevel)(void*, float) = nullptr;
	void ForeignLodLevel(void* fade, float level)
	{
		trace.push_back("foreign lodLevel");
		foreignLodLevel(fade, level + 1.0f);  // a wrapper may change the arguments it passes on
	}
	void (*foreignSetRange)(void*, float, float) = nullptr;
	void ForeignSetRange(void* fade, float nearDistance, float farDistance)
	{
		trace.push_back("foreign setRange");
		foreignSetRange(fade, nearDistance, farDistance);
	}
	void (*foreignLodType)(void*, std::uint8_t) = nullptr;
	void ForeignLodType(void* fade, std::uint8_t type)
	{
		trace.push_back("foreign lodType");
		foreignLodType(fade, type);
	}
	void* (*foreignRenderPasses)(void*, void*, std::uint32_t, void*) = nullptr;
	void* ForeignRenderPasses(void* property, void* geometry, std::uint32_t flags, void* accumulator)
	{
		trace.push_back("foreign renderPasses");
		return foreignRenderPasses(property, geometry, flags, accumulator);
	}
	void (*foreignAlpha)(void*, float) = nullptr;
	void ForeignAlpha(void* property, float alpha)
	{
		trace.push_back("foreign alpha");
		foreignAlpha(property, alpha);
	}
	void (*foreignSelectiveUpdate)(void*, void*, std::uint32_t) = nullptr;
	void ForeignSelectiveUpdate(void* node, void* data, std::uint32_t arg)
	{
		trace.push_back("foreign update pass");
		foreignSelectiveUpdate(node, data, arg);
	}

	void RecordMoved(const SWF_EventHeader* header, std::uintptr_t*, void*)
	{
		trace.push_back("swf moved " + std::to_string(reinterpret_cast<const SWF_MovedEvent*>(header)->writer));
	}

	std::string Hook(const char* phase, std::uint32_t hook) { return std::string("swf ") + phase + " " + std::to_string(hook); }
}

int main()
{
	SetEnvironmentVariableW(L"SWF_LOG_PATH", L"NUL");
	image = hooking::GameImage();

	FakeVtable& fadeNode = NewVtable(".?AVBSFadeNode@@");
	fadeNode.slots[0x34] = reinterpret_cast<void*>(&EngineOnVisible);
	FakeVtable& lighting = NewVtable(".?AVBSLightingShaderProperty@@");
	lighting.slots[0x2A] = reinterpret_cast<void*>(&EngineRenderPasses);
	lighting.slots[0x31] = reinterpret_cast<void*>(&EngineMaterialAlpha);

	HookTargets targets{};
	targets.fadeNode = Ref(fadeNode);
	targets.lightingProperty = Ref(lighting);
	targets.functions[SWF_HOOK_FADE_UPDATE] = Rva(reinterpret_cast<const void*>(&EngineUpdate));
	targets.functions[SWF_HOOK_FADE_LOD_LEVEL] = Rva(reinterpret_cast<const void*>(&EngineLodLevel));
	targets.functions[SWF_HOOK_FADE_SET_RANGE] = Rva(reinterpret_cast<const void*>(&EngineSetRange));
	targets.functions[SWF_HOOK_FADE_SET_LOD_TYPE] = Rva(reinterpret_cast<const void*>(&EngineSetLodType));
	targets.functions[SWF_HOOK_FADE_SNAP] = Rva(reinterpret_cast<const void*>(&EngineSnap));
	targets.functions[SWF_HOOK_PROPERTY_SET_FLAGS] = Rva(reinterpret_cast<const void*>(&EngineProperty<0>));
	targets.functions[SWF_HOOK_PROPERTY_SET_MATERIAL] = Rva(reinterpret_cast<const void*>(&EngineProperty<1>));
	targets.functions[SWF_HOOK_PROPERTIES_SET_FLAG] = Rva(reinterpret_cast<const void*>(&EngineProperty<2>));
	targets.functions[SWF_HOOK_PROPERTIES_SET_FADE_NODE] = Rva(reinterpret_cast<const void*>(&EngineProperty<3>));
	targets.functions[SWF_HOOK_PROPERTIES_PROJECTED_UV] = Rva(reinterpret_cast<const void*>(&EngineProperty<4>));
	targets.functions[SWF_HOOK_SET_EXTERNAL_EMITTANCE] = Rva(reinterpret_cast<const void*>(&EngineProperty<5>));
	targets.functions[SWF_HOOK_SKY_SET_COLOR] = Rva(reinterpret_cast<const void*>(&EngineProperty<6>));

	// ---- Before SWF: a Detours detour on FadeUpdate, a hand-rolled hook on FadeLodLevel, a slot
	// swap on GetRenderPasses (as CS's own engine fixes do).
	foreignUpdate = &EngineUpdate;
	ForeignDetour(reinterpret_cast<void**>(&foreignUpdate), reinterpret_cast<void*>(&ForeignUpdate));
	foreignLodLevel = reinterpret_cast<decltype(foreignLodLevel)>(HandRolled(reinterpret_cast<void*>(&EngineLodLevel), reinterpret_cast<void*>(&ForeignLodLevel)));
	ForeignSwap(&lighting.slots[0x2A], reinterpret_cast<void*>(&ForeignRenderPasses), reinterpret_cast<void**>(&foreignRenderPasses));

	CHECK(detours::VerifyCalls(SWF_CAP_FADE_CALLS, image, targets));
	CHECK(detours::InstallCalls(SWF_CAP_FADE_CALLS, image, targets));
	CHECK(detours::VerifyCalls(SWF_CAP_PROPERTY_CALLS, image, targets));
	CHECK(detours::InstallCalls(SWF_CAP_PROPERTY_CALLS, image, targets));
	for (const std::uint32_t hook : { SWF_HOOK_FADE_ON_VISIBLE, SWF_HOOK_FADE_UPDATE, SWF_HOOK_FADE_LOD_LEVEL, SWF_HOOK_FADE_SET_RANGE, SWF_HOOK_FADE_SET_LOD_TYPE,
			 SWF_HOOK_LIGHTING_GET_RENDER_PASSES, SWF_HOOK_LIGHTING_SET_MATERIAL_ALPHA })
		Subscribe(hook);

	// ---- After SWF: a Detours detour on FadeSetRange, a hand-rolled hook on FadeSetLodType (over
	// SWF's detour jump), a slot swap on SetMaterialAlpha.
	foreignSetRange = &EngineSetRange;
	ForeignDetour(reinterpret_cast<void**>(&foreignSetRange), reinterpret_cast<void*>(&ForeignSetRange));
	foreignLodType = reinterpret_cast<decltype(foreignLodType)>(HandRolled(reinterpret_cast<void*>(&EngineSetLodType), reinterpret_cast<void*>(&ForeignLodType)));
	ForeignSwap(&lighting.slots[0x31], reinterpret_cast<void*>(&ForeignAlpha), reinterpret_cast<void**>(&foreignAlpha));

	int fade = 0;
	// Hooked before SWF: SWF is outer, the foreign hook inner.
	CHECK(EngineUpdate(&fade, 0.5f, nullptr) == 50);
	Expect({ Hook("pre", SWF_HOOK_FADE_UPDATE).c_str(), "foreign update", "engine update", Hook("post", SWF_HOOK_FADE_UPDATE).c_str() });
	EngineLodLevel(&fade, 3.0f);
	// SWF sees the arguments the caller passed; the inner foreign hook's change reaches the engine.
	Expect({ Hook("pre", SWF_HOOK_FADE_LOD_LEVEL).c_str(), "foreign lodLevel", "engine lodLevel 4", Hook("post", SWF_HOOK_FADE_LOD_LEVEL).c_str() });
	Call(lighting, 0x2A, static_cast<void*>(&fade), static_cast<void*>(nullptr), std::uint32_t(0), static_cast<void*>(nullptr));
	Expect({ Hook("pre", SWF_HOOK_LIGHTING_GET_RENDER_PASSES).c_str(), "foreign renderPasses", "engine renderPasses",
		Hook("post", SWF_HOOK_LIGHTING_GET_RENDER_PASSES).c_str() });

	// Hooked after SWF: the foreign hook is outer.
	EngineSetRange(&fade, 1.0f, 9.0f);
	Expect({ "foreign setRange", Hook("pre", SWF_HOOK_FADE_SET_RANGE).c_str(), "engine setRange 9", Hook("post", SWF_HOOK_FADE_SET_RANGE).c_str() });
	EngineSetLodType(&fade, 6);
	Expect({ "foreign lodType", Hook("pre", SWF_HOOK_FADE_SET_LOD_TYPE).c_str(), "engine lodType 6", Hook("post", SWF_HOOK_FADE_SET_LOD_TYPE).c_str() });
	Call(lighting, 0x31, static_cast<void*>(&fade), 0.5f);
	Expect({ "foreign alpha", Hook("pre", SWF_HOOK_LIGHTING_SET_MATERIAL_ALPHA).c_str(), "engine alpha 5",
		Hook("post", SWF_HOOK_LIGHTING_SET_MATERIAL_ALPHA).c_str() });

	// Untouched by anyone else: SWF alone.
	Call(fadeNode, 0x34, static_cast<void*>(&fade), static_cast<void*>(nullptr), std::int32_t(1));
	Expect({ Hook("pre", SWF_HOOK_FADE_ON_VISIBLE).c_str(), "engine onVisible", Hook("post", SWF_HOOK_FADE_ON_VISIBLE).c_str() });

	// ---- Call sites: a site another plugin rewrote first is refused (the family is left out); a
	// rewrite after SWF's that keeps SWF's call target as its original chains.
	{
		auto* region = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
		CHECK(region);
		const std::uint8_t writer[] = { 0xB0, 0x01, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3 };  // mov al, 1; nop x5; ret
		for (int i = 0; i < 4; ++i)  // from 0x100: offset 0 is a table's "no address"; the fourth at 0x600
			std::memcpy(region + (i < 3 ? 0x100 * (i + 1) : 0x600), writer, sizeof(writer));
		// sub rsp, 0x28; mov rax, [rcx]; call [rax + 0x168]; add rsp, 0x28; ret
		const std::uint8_t pass[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
		std::memcpy(region + 0x400, pass, sizeof(pass));
		std::memcpy(region + 0x500, pass, sizeof(pass));
		const hooking::Image regionImage{ reinterpret_cast<std::uintptr_t>(region), 0x1000 };
		HookTargets moves{};
		moves.animationGraphPlace = 0x100;
		moves.reference3DPosition = 0x200;
		moves.graphAnimation = 0x300;
		moves.projectileUpdate3D = 0x600;
		moves.selectiveUpdateCalls = { 0x407, 0x507 };
		moves.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 };

		// Before SWF: a plugin's `call rel32; nop` over the second site.
		std::uint8_t saved[6];
		std::memcpy(saved, region + 0x507, sizeof(saved));
		WriteRel32(0xE8, region + 0x507, region + 0x700);
		CHECK(!detours::VerifyMoves(regionImage, moves));
		std::memcpy(region + 0x507, saved, sizeof(saved));
		CHECK(detours::VerifyMoves(regionImage, moves));
		CHECK(detours::InstallMoves(regionImage, moves));
		dispatch::Entry entry;
		entry.event = &RecordMoved;
		entry.phases = SWF_PHASE_POST;
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Events().Add(SWF_EVENT_MOVED, entry, &subscription) == SWF_OK);

		// After SWF: a write_call<5>-style rewrite of the first site, through a stub of its own
		// within reach, keeping the site's current target (SWF's stub) as its original.
		auto* site = region + 0x407;
		CHECK(site[0] == 0xE8);
		std::int32_t displacement;
		std::memcpy(&displacement, site + 1, sizeof(displacement));
		foreignSelectiveUpdate = reinterpret_cast<decltype(foreignSelectiveUpdate)>(site + 5 + displacement);
		std::uint8_t* stub = region + 0x700;
		const std::uint8_t movRax[] = { 0x48, 0xB8 };
		const auto hook = reinterpret_cast<std::uintptr_t>(&ForeignSelectiveUpdate);
		std::memcpy(stub, movRax, sizeof(movRax));
		std::memcpy(stub + 2, &hook, sizeof(hook));
		const std::uint8_t jmpRax[] = { 0xFF, 0xE0 };
		std::memcpy(stub + 10, jmpRax, sizeof(jmpRax));
		WriteRel32(0xE8, site, stub);

		static int updates = 0;
		FakeVtable& updatable = NewVtable(".?AVUpdatable@@");
		updatable.slots[0x2D] = reinterpret_cast<void*>(+[](void*, void*, std::uint32_t) { trace.push_back("engine update pass"), ++updates; });
		alignas(16) std::byte node[0x200]{};
		*reinterpret_cast<void**>(node) = updatable.slots;
		reinterpret_cast<void (*)(void*)>(region + 0x400)(node);
		const std::string moved = "swf moved " + std::to_string(SWF_MOVE_SELECTIVE_UPDATE);
		Expect({ "foreign update pass", "engine update pass", moved.c_str() });
		reinterpret_cast<void (*)(void*)>(region + 0x500)(node);
		Expect({ "engine update pass", moved.c_str() });
		CHECK(updates == 2);
	}

	// ---- The integrity check: the four rewritten after SWF (FadeSetRange's and FadeSetLodType's
	// detour jumps, slot 0x31, the first update pass's call), each leading into this executable.
	{
		struct Found
		{
			std::uint32_t kind;
			std::uintptr_t address;
			std::string module;
		};
		static std::vector<Found> found;
		const std::uint32_t changed = hooking::CheckIntegrity(
			[](const SWF_IntegrityEntry* entry, void*) { found.push_back({ entry->kind, entry->address, entry->module ? entry->module : "" }); }, nullptr);
		char path[MAX_PATH]{};
		std::string self(path, GetModuleFileNameA(nullptr, path, MAX_PATH));
		self = self.substr(self.find_last_of("\\/") + 1);
		CHECK(changed == 4 && found.size() == 4);
		std::uint32_t detours = 0, slots = 0, sites = 0;
		for (const Found& entry : found) {
			if (entry.module != self)
				std::fprintf(stderr, "kind %u at %p leads to '%s'\n", entry.kind, reinterpret_cast<void*>(entry.address), entry.module.c_str());
			CHECK(entry.module == self);
			detours += entry.kind == SWF_SITE_DETOUR;
			slots += entry.kind == SWF_SITE_SLOT && entry.address == reinterpret_cast<std::uintptr_t>(&lighting.slots[0x31]);
			sites += entry.kind == SWF_SITE_CALL;
		}
		CHECK(detours == 2 && slots == 1 && sites == 1);
	}

	std::puts("interop: all checks passed");
	return 0;
}
