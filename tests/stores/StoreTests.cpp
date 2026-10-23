// Runs the store families' stubs against a synthetic image: real engine store instructions
// (copied from the AE table) each followed by `ret`, patched exactly as the game's would be,
// then executed through a harness that loads the registers the instruction uses.
#include "core/Dispatch.h"
#include "families/Stores.h"
#include "hooking/Memory.h"

#include <Windows.h>
#include <xbyak/xbyak.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(condition)                                                                       \
	do {                                                                                       \
		if (!(condition)) {                                                                    \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
			std::exit(1);                                                                      \
		}                                                                                      \
	} while (0)

namespace
{
	using namespace swf;
	using Xbyak::Operand;

	constexpr std::uint32_t kOne = 0x3f800000;

	// Offsets in the synthetic image; the instructions are real AE sites.
	constexpr HiddenStoreSite kHidden[] = {
		{ 0x100, { 0x89, 0x85, 0xf4, 0x00, 0x00, 0x00 }, 6, Operand::RBP, false },        // MOV [RBP+0xf4], EAX
		{ 0x200, { 0x83, 0x8e, 0xf4, 0x00, 0x00, 0x00, 0x01 }, 7, Operand::RSI, true },   // OR [RSI+0xf4], 0x1
		{ 0x300, { 0x83, 0xa0, 0xf4, 0x00, 0x00, 0x00, 0xfe }, 7, Operand::RAX, true },   // AND [RAX+0xf4], 0xfffffffe (show)
		{ 0x400, { 0x44, 0x09, 0xa0, 0xf4, 0x00, 0x00, 0x00 }, 7, Operand::RAX, true },   // OR [RAX+0xf4], R12D
	};
	constexpr std::uint32_t kShow[] = { 0x300 };

	constexpr StoreValueSite kSwitch[] = {
		{ 0x500, { 0x44, 0x89, 0x82, 0x2c, 0x01, 0x00, 0x00 }, 7, Operand::RDX, Operand::R8 },  // MOV [RDX+0x12c], R8D
		{ 0x580, { 0x89, 0x81, 0x2c, 0x01, 0x00, 0x00 }, 6, Operand::RCX, Operand::RAX },        // MOV [RCX+0x12c], EAX
	};
	constexpr StoreValueSite kFade[] = {
		{ 0x600, { 0xc7, 0x80, 0x30, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f }, 10, Operand::RAX, StoreValueSite::kImmediate, kOne },
		{ 0x680, { 0x44, 0x89, 0xa0, 0x30, 0x01, 0x00, 0x00 }, 7, Operand::RAX, Operand::R12 },  // MOV [RAX+0x130], R12D
	};

	hooking::Image image;

	template <class Site>
	void Place(const Site& site)
	{
		auto* at = reinterpret_cast<std::uint8_t*>(image.base + site.offset);
		std::memcpy(at, site.bytes.data(), site.length);
		at[site.length] = 0xC3;  // ret
	}

	// void(object, value): loads `value` into `valueReg` (unless none) and `object` into
	// `baseReg`, then calls the site. Callee-saved registers are restored.
	using Invoke = void (*)(void* object, std::uint64_t value);

	struct Harness : Xbyak::CodeGenerator
	{
		Harness(std::uintptr_t target, int baseReg, int valueReg)
		{
			using namespace Xbyak::util;
			const Xbyak::Reg64 saved[] = { rbx, rbp, rdi, rsi, r12, r13, r14, r15 };
			for (const auto& reg : saved)
				push(reg);
			sub(rsp, 0x28);
			mov(r10, rcx);
			mov(r11, rdx);
			if (valueReg >= 0)
				mov(Xbyak::Reg64(valueReg), r11);
			mov(Xbyak::Reg64(baseReg), r10);
			Xbyak::Label address;
			call(ptr[rip + address]);
			add(rsp, 0x28);
			for (int i = 7; i >= 0; --i)
				pop(saved[i]);
			ret();
			L(address);
			dq(target);
		}

		Invoke Get() const { return getCode<Invoke>(); }
	};

	std::vector<Harness*> harnesses;

	Invoke HarnessFor(std::uint32_t offset, int baseReg, int valueReg)
	{
		harnesses.push_back(new Harness(image.base + offset, baseReg, valueReg));
		return harnesses.back()->Get();
	}

	struct Object
	{
		alignas(16) std::uint8_t bytes[0x200]{};
		std::uint32_t& Flags() { return *reinterpret_cast<std::uint32_t*>(bytes + 0xF4); }
		std::int32_t& Index() { return *reinterpret_cast<std::int32_t*>(bytes + 0x12C); }
		std::uint32_t& Fade() { return *reinterpret_cast<std::uint32_t*>(bytes + 0x130); }
	};

	std::vector<SWF_HiddenEvent> hiddenEvents;
	std::vector<SWF_SwitchIndexEvent> switchEvents;
	std::vector<SWF_FadeResetEvent> fadeEvents;
	int vetoCalls = 0;
	bool vetoAnswer = false;

	void Subscribe(uint32_t id, SWF_EventFn fn)
	{
		dispatch::Entry entry;
		entry.event = fn;
		entry.phases = SWF_PHASE_POST;
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Events().Add(id, entry, &subscription) == SWF_OK);
	}

	std::uintptr_t StubOf(std::uint32_t offset)
	{
		const auto at = image.base + offset;
		CHECK(*reinterpret_cast<const std::uint8_t*>(at) == 0xE8);
		std::int32_t displacement;
		std::memcpy(&displacement, reinterpret_cast<const void*>(at + 1), sizeof(displacement));
		return at + 5 + displacement;
	}
}

int main()
{
	SetEnvironmentVariableW(L"SWF_LOG_PATH", L"NUL");
	image.base = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	image.size = 0x10000;
	CHECK(image.base != 0);
	for (const auto& site : kHidden)
		Place(site);
	for (const auto& site : kSwitch)
		Place(site);
	for (const auto& site : kFade)
		Place(site);

	// Verification: exact bytes, a changed byte, a site past the image.
	CHECK(stores::VerifyHidden(image, kHidden));
	reinterpret_cast<std::uint8_t*>(image.base + kHidden[1].offset)[2] ^= 0xFF;
	CHECK(!stores::VerifyHidden(image, kHidden));
	reinterpret_cast<std::uint8_t*>(image.base + kHidden[1].offset)[2] ^= 0xFF;
	const HiddenStoreSite outside[] = { { 0xFFFF, { 0x89 }, 6, Operand::RBP, false } };
	CHECK(!stores::VerifyHidden(image, outside));
	CHECK(stores::VerifyValues(image, kSwitch) && stores::VerifyValues(image, kFade));

	CHECK(stores::InstallHidden(image, kHidden, kShow));
	CHECK(stores::InstallSwitchIndex(image, kSwitch));
	CHECK(stores::InstallFadeReset(image, kFade));
	CHECK(stores::SiteCount(SWF_EVENT_HIDDEN) == 4 && stores::SiteCount(SWF_EVENT_SWITCH_INDEX) == 2 && stores::SiteCount(SWF_EVENT_FADE_RESET) == 2);

	Subscribe(SWF_EVENT_HIDDEN, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		hiddenEvents.push_back(*reinterpret_cast<const SWF_HiddenEvent*>(header));
	});
	Subscribe(SWF_EVENT_SWITCH_INDEX, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		switchEvents.push_back(*reinterpret_cast<const SWF_SwitchIndexEvent*>(header));
	});
	Subscribe(SWF_EVENT_FADE_RESET, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		fadeEvents.push_back(*reinterpret_cast<const SWF_FadeResetEvent*>(header));
	});

	Object object;

	// MOV [RBP+0xf4], EAX: an event only when bit 0 changes; other bits stored as given.
	const Invoke movStore = HarnessFor(kHidden[0].offset, Operand::RBP, Operand::RAX);
	movStore(&object, 0x11);
	CHECK(object.Flags() == 0x11 && hiddenEvents.size() == 1);
	CHECK(hiddenEvents[0].object == &object && hiddenEvents[0].hidden == 1 && hiddenEvents[0].site == 0 && hiddenEvents[0].siteOffset == 0x100);
	movStore(&object, 0x31);
	CHECK(object.Flags() == 0x31 && hiddenEvents.size() == 1);
	movStore(&object, 0x30);
	CHECK(object.Flags() == 0x30 && hiddenEvents.size() == 2 && hiddenEvents[1].hidden == 0);

	// OR [RSI+0xf4], 1 (read-modify-write, rerun after the flags are restored).
	const Invoke orStore = HarnessFor(kHidden[1].offset, Operand::RSI, -1);
	orStore(&object, 0);
	CHECK(object.Flags() == 0x31 && hiddenEvents.size() == 3 && hiddenEvents[2].site == 1 && hiddenEvents[2].hidden == 1);
	orStore(&object, 0);
	CHECK(object.Flags() == 0x31 && hiddenEvents.size() == 3);

	// OR [RAX+0xf4], R12D: the value register is callee-saved and untouched by the stub.
	const Invoke orRegister = HarnessFor(kHidden[3].offset, Operand::RAX, Operand::R12);
	object.Flags() = 0x30;
	orRegister(&object, 0x101);
	CHECK(object.Flags() == 0x131 && hiddenEvents.size() == 4 && hiddenEvents[3].site == 3);

	// The show site: no filter registered, then a filter that passes, then one that vetoes.
	const Invoke show = HarnessFor(kHidden[2].offset, Operand::RAX, -1);
	object.Flags() = 0x31;
	show(&object, 0);
	CHECK(object.Flags() == 0x30 && hiddenEvents.size() == 5 && hiddenEvents[4].site == 2 && hiddenEvents[4].hidden == 0);

	dispatch::Entry filter;
	filter.filter = [](const SWF_EventHeader* header, void*) -> int32_t {
		const auto* event = reinterpret_cast<const SWF_ShowFilterEvent*>(header);
		CHECK(header->id == SWF_FILTER_SHOW && event->site == 2 && event->siteOffset == 0x300);
		++vetoCalls;
		return vetoAnswer ? 1 : 0;
	};
	SWF_Subscription filterHandle = 0;
	CHECK(dispatch::Filters().Add(SWF_FILTER_SHOW, filter, &filterHandle) == SWF_OK);

	object.Flags() = 0x31;
	vetoAnswer = false;
	show(&object, 0);
	CHECK(vetoCalls == 1 && object.Flags() == 0x30 && hiddenEvents.size() == 6);

	object.Flags() = 0x31;
	vetoAnswer = true;
	show(&object, 0);
	CHECK(vetoCalls == 2 && object.Flags() == 0x31 && hiddenEvents.size() == 6);  // unrun, no event

	CHECK(dispatch::Filters().Remove(filterHandle) == SWF_OK);
	show(&object, 0);
	CHECK(vetoCalls == 2 && object.Flags() == 0x30 && hiddenEvents.size() == 7);

	// Switch index: MOV [RDX+0x12c], R8D (both volatile, read from the stub's saved slots).
	const Invoke switchR8 = HarnessFor(kSwitch[0].offset, Operand::RDX, Operand::R8);
	object.Index() = -1;
	switchR8(&object, 3);
	CHECK(object.Index() == 3 && switchEvents.size() == 1);
	CHECK(switchEvents[0].node == &object && switchEvents[0].before == -1 && switchEvents[0].after == 3 && switchEvents[0].site == 0 && switchEvents[0].siteOffset == 0x500);
	switchR8(&object, 3);
	CHECK(switchEvents.size() == 1);

	const Invoke switchRax = HarnessFor(kSwitch[1].offset, Operand::RCX, Operand::RAX);
	switchRax(&object, 1);
	CHECK(object.Index() == 1 && switchEvents.size() == 2 && switchEvents[1].before == 3 && switchEvents[1].site == 1);

	// Fade resets: an immediate 1.0 and a register; an event every time.
	const Invoke fadeImm = HarnessFor(kFade[0].offset, Operand::RAX, -1);
	fadeImm(&object, 0);
	CHECK(object.Fade() == kOne && fadeEvents.size() == 1 && fadeEvents[0].value == 1.0f && fadeEvents[0].site == 0);
	fadeImm(&object, 0);
	CHECK(fadeEvents.size() == 2);

	const Invoke fadeR12 = HarnessFor(kFade[1].offset, Operand::RAX, Operand::R12);
	fadeR12(&object, 0);
	CHECK(object.Fade() == 0 && fadeEvents.size() == 3 && fadeEvents[2].value == 0.0f && fadeEvents[2].siteOffset == 0x680);

	// Stub addresses resolve back to their engine sites.
	uint32_t id = 0, site = 0, offset = 0;
	CHECK(hooking::ResolveStub(StubOf(kHidden[2].offset), id, site, offset) && id == SWF_EVENT_HIDDEN && site == 2 && offset == 0x300);
	CHECK(hooking::ResolveStub(StubOf(kHidden[2].offset) + 3, id, site, offset) && site == 2);
	CHECK(hooking::ResolveStub(StubOf(kSwitch[1].offset), id, site, offset) && id == SWF_EVENT_SWITCH_INDEX && site == 1);
	CHECK(hooking::ResolveStub(StubOf(kFade[0].offset), id, site, offset) && id == SWF_EVENT_FADE_RESET && site == 0);
	CHECK(!hooking::ResolveStub(image.base, id, site, offset));

	std::puts("store stubs: all checks passed");
	return 0;
}
