// The store families. Each patched engine store becomes `call stub` plus NOPs:
//  - hidden: the stub runs the original instruction and fires SWF_EVENT_HIDDEN when bit 0
//    of NiAVObject::flags changed; at the show sites it first consults SWF_FILTER_SHOW.
//  - switch index, fade reset: the stub hands (base, value, site) to a handler that performs
//    the store and fires the event.
#include "families/Stores.h"

#include "core/Dispatch.h"
#include "core/Log.h"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <cstring>

namespace swf::inline SWF_NAMESPACE::stores
{
	namespace
	{
		constexpr std::size_t kFlags = 0xF4;
		constexpr std::size_t kSwitchIndex = 0x12C;
		constexpr std::size_t kCurrentFade = 0x130;

		std::vector<std::uint32_t> hiddenOffsets;
		std::vector<std::uint32_t> switchOffsets;
		std::vector<std::uint32_t> fadeOffsets;

		template <class Event>
		void Fill(Event& event, std::uint32_t id, std::uint32_t site, const std::vector<std::uint32_t>& offsets)
		{
			event.header.size = sizeof(Event);
			event.header.id = id;
			event.site = site;
			event.siteOffset = site < offsets.size() ? offsets[site] : 0;
		}

		void OnHiddenStore(void* object, std::uint32_t site)
		{
			// After the store: bit 0 is its value (or a racing writer's, which fires its own event).
			SWF_HiddenEvent event{};
			Fill(event, SWF_EVENT_HIDDEN, site, hiddenOffsets);
			event.object = object;
			event.hidden = *reinterpret_cast<const volatile std::uint32_t*>(static_cast<std::byte*>(object) + kFlags) & 1u;
			dispatch::Fire(event.header);
		}

		bool OnShowStore(void* object, std::uint32_t site)
		{
			SWF_ShowFilterEvent event{};
			Fill(event, SWF_FILTER_SHOW, site, hiddenOffsets);
			event.object = object;
			return dispatch::Vetoed(SWF_FILTER_SHOW, event.header);
		}

		void OnSwitchStore(void* node, std::int32_t value, std::uint32_t site)
		{
			auto& index = *reinterpret_cast<std::int32_t*>(static_cast<std::byte*>(node) + kSwitchIndex);
			const std::int32_t before = index;
			index = value;
			if (before == value)
				return;
			SWF_SwitchIndexEvent event{};
			Fill(event, SWF_EVENT_SWITCH_INDEX, site, switchOffsets);
			event.node = node;
			event.before = before;
			event.after = value;
			dispatch::Fire(event.header);
		}

		void OnFadeResetStore(void* node, std::uint32_t bits, std::uint32_t site)
		{
			*reinterpret_cast<std::uint32_t*>(static_cast<std::byte*>(node) + kCurrentFade) = bits;
			SWF_FadeResetEvent event{};
			Fill(event, SWF_EVENT_FADE_RESET, site, fadeOffsets);
			event.node = node;
			std::memcpy(&event.value, &bits, sizeof(bits));
			dispatch::Fire(event.header);
		}

		// Saves rax, rdx, r8-r11, rbx and xmm0-5, aligns the stack, calls `handler`
		// (arguments already in rcx/edx), restores, and returns. rcx is the caller's to keep.
		void EmitCallBody(Xbyak::CodeGenerator& c, std::uintptr_t handler, bool answerInZf)
		{
			using namespace Xbyak::util;
			c.push(rax);
			c.push(rdx);
			c.push(r8);
			c.push(r9);
			c.push(r10);
			c.push(r11);
			c.push(rbx);
			c.mov(rbx, rsp);
			c.and_(rsp, ~std::uint32_t(0xF));
			c.sub(rsp, 0x80);
			for (int i = 0; i < 6; ++i)
				c.movdqu(c.ptr[rsp + 0x20 + 0x10 * i], Xbyak::Xmm(i));
			c.mov(rax, handler);
			c.call(rax);
			if (answerInZf)
				c.test(al, al);
			for (int i = 0; i < 6; ++i)
				c.movdqu(Xbyak::Xmm(i), c.ptr[rsp + 0x20 + 0x10 * i]);
			c.mov(rsp, rbx);
			c.pop(rbx);
			c.pop(r11);
			c.pop(r10);
			c.pop(r9);
			c.pop(r8);
			c.pop(rdx);
			c.pop(rax);
			c.ret();
		}

		/**
		 * Each hidden stub reads the dword, runs the original instruction and compares bit 0;
		 * on a change it calls OnHiddenStore(object, site) through the common body. The flags
		 * are then restored and an `or`/`and` runs once more (idempotent) to give the following
		 * code its expected flags. A `mov` sets none and is not rerun, so a concurrent writer's
		 * value is never stored over.
		 *
		 * A show stub starts with the filter: while SWF_FILTER_SHOW has entries (one dword
		 * compare) it calls OnShowStore, which leaves the answer in ZF. A vetoed store returns
		 * at once, unrun and without an event. The code after each show site reads no flags.
		 */
		struct HiddenStubs : Xbyak::CodeGenerator
		{
			HiddenStubs(std::span<const HiddenStoreSite> sites, std::span<const std::uint32_t> showSites, std::vector<std::size_t>& entries) :
				Xbyak::CodeGenerator(sites.size() * 112 + showSites.size() * 80 + 512)
			{
				using namespace Xbyak::util;
				Xbyak::Label common, vetoCommon;
				const auto* showCount = dispatch::Filters().CountWord(SWF_FILTER_SHOW);
				auto original = [&](const HiddenStoreSite& site) {
					for (std::uint8_t i = 0; i < site.length; ++i)
						db(site.bytes[i]);
				};
				for (std::uint32_t index = 0; index < sites.size(); ++index) {
					const HiddenStoreSite& site = sites[index];
					entries.push_back(getSize());
					const Xbyak::Reg64 base(site.base);
					Xbyak::Label changed, back;
					if (std::ranges::find(showSites, site.offset) != showSites.end()) {
						Xbyak::Label proceed;
						pushf();
						push(rax);
						mov(rax, reinterpret_cast<std::uintptr_t>(showCount));
						cmp(dword[rax], 0);
						pop(rax);
						je(proceed, T_NEAR);
						push(rcx);
						push(rdx);
						if (site.base != Xbyak::Operand::RCX)
							mov(rcx, base);
						mov(edx, index);
						call(vetoCommon);
						pop(rdx);
						pop(rcx);
						jz(proceed, T_NEAR);  // ZF: not vetoed
						popf();
						ret();
						L(proceed);
						popf();
					}
					pushf();
					sub(rsp, 8);
					push(rax);
					mov(eax, dword[base + kFlags]);
					mov(dword[rsp + 8], eax);
					pop(rax);
					original(site);
					push(rax);
					mov(eax, dword[base + kFlags]);
					xor_(eax, dword[rsp + 8]);
					test(al, 1);
					pop(rax);
					jnz(changed, T_NEAR);
					L(back);
					add(rsp, 8);
					popf();
					if (site.readModifyWrite)
						original(site);
					ret();
					L(changed);
					push(rcx);
					push(rdx);
					if (site.base != Xbyak::Operand::RCX)
						mov(rcx, base);
					mov(edx, index);
					call(common);
					pop(rdx);
					pop(rcx);
					jmp(back, T_NEAR);
				}
				L(common);
				EmitCallBody(*this, reinterpret_cast<std::uintptr_t>(&OnHiddenStore), false);
				L(vetoCommon);
				EmitCallBody(*this, reinterpret_cast<std::uintptr_t>(&OnShowStore), true);
			}
		};

		/**
		 * Each value stub loads (base, value, site) into rcx, edx and r8d and joins a common
		 * body that saves the volatile registers, the flags (a store sets none, so later code
		 * may test earlier flags) and xmm0-5, and calls the handler.
		 */
		struct ValueStubs : Xbyak::CodeGenerator
		{
			ValueStubs(std::span<const StoreValueSite> sites, std::uintptr_t handler, std::vector<std::size_t>& entries) :
				Xbyak::CodeGenerator(sites.size() * 48 + 256)
			{
				using namespace Xbyak::util;
				using Xbyak::Operand;
				Xbyak::Label common;
				// After the four pushes: r8 at [rsp], rdx at +8, rcx at +0x10, rax at +0x18.
				auto slotOf = [](int reg) {
					switch (reg) {
					case Operand::RAX:
						return 0x18;
					case Operand::RCX:
						return 0x10;
					case Operand::RDX:
						return 0x8;
					case Operand::R8:
						return 0x0;
					default:
						return -1;
					}
				};
				for (std::uint32_t index = 0; index < sites.size(); ++index) {
					const StoreValueSite& site = sites[index];
					entries.push_back(getSize());
					push(rax);
					push(rcx);
					push(rdx);
					push(r8);
					if (const int slot = slotOf(site.base); slot >= 0)
						mov(rcx, qword[rsp + slot]);
					else
						mov(rcx, Xbyak::Reg64(site.base));
					if (site.value == StoreValueSite::kImmediate)
						mov(edx, site.imm);
					else if (const int slot = slotOf(site.value); slot >= 0)
						mov(edx, dword[rsp + slot]);
					else
						mov(edx, Xbyak::Reg32(site.value));
					mov(r8d, index);
					jmp(common, T_NEAR);
				}
				L(common);
				push(r9);
				push(r10);
				push(r11);
				push(rbx);
				pushf();
				mov(rbx, rsp);
				and_(rsp, ~std::uint32_t(0xF));
				sub(rsp, 0x80);
				for (int i = 0; i < 6; ++i)
					movdqu(ptr[rsp + 0x20 + 0x10 * i], Xbyak::Xmm(i));
				mov(rax, handler);
				call(rax);
				for (int i = 0; i < 6; ++i)
					movdqu(Xbyak::Xmm(i), ptr[rsp + 0x20 + 0x10 * i]);
				mov(rsp, rbx);
				popf();
				pop(rbx);
				pop(r11);
				pop(r10);
				pop(r9);
				pop(r8);
				pop(rdx);
				pop(rcx);
				pop(rax);
				ret();
			}
		};

		template <class Site>
		std::vector<hooking::CallSite> CallSites(std::span<const Site> sites)
		{
			std::vector<hooking::CallSite> result;
			for (const Site& site : sites)
				result.push_back({ site.offset, site.length });
			return result;
		}

		template <class Site>
		std::vector<std::uint32_t> Offsets(std::span<const Site> sites)
		{
			std::vector<std::uint32_t> result;
			for (const Site& site : sites)
				result.push_back(site.offset);
			return result;
		}

		template <class Site>
		bool Verify(const hooking::Image& image, std::span<const Site> sites, const char* what)
		{
			for (const Site& site : sites) {
				if (!hooking::BytesMatch(image, site.offset, site.bytes.data(), site.length)) {
					log::Warn("{}: the store at +{:#x} is not the expected instruction", what, site.offset);
					return false;
				}
			}
			return true;
		}

		bool InstallValues(const hooking::Image& image, std::span<const StoreValueSite> sites, std::uintptr_t handler, std::uint32_t eventId,
			std::vector<std::uint32_t>& offsets, const char* what)
		{
			std::vector<std::size_t> entries;
			ValueStubs stubs(sites, handler, entries);
			offsets = Offsets(sites);
			const std::span code(stubs.getCode(), stubs.getSize());
			const std::uint8_t* copy = hooking::PatchCalls(image, CallSites(sites), code, entries);
			if (!copy) {
				offsets.clear();
				return false;
			}
			hooking::RegisterStubs(eventId, copy, code.size(), std::move(entries), offsets);
			log::Info("{}: {} stores patched ({} bytes of stubs)", what, sites.size(), code.size());
			return true;
		}
	}

	bool VerifyHidden(const hooking::Image& image, std::span<const HiddenStoreSite> sites)
	{
		return Verify(image, sites, "hidden");
	}

	bool InstallHidden(const hooking::Image& image, std::span<const HiddenStoreSite> sites, std::span<const std::uint32_t> showSites)
	{
		std::vector<std::size_t> entries;
		HiddenStubs stubs(sites, showSites, entries);
		hiddenOffsets = Offsets(sites);
		const std::span code(stubs.getCode(), stubs.getSize());
		const std::uint8_t* copy = hooking::PatchCalls(image, CallSites(sites), code, entries);
		if (!copy) {
			hiddenOffsets.clear();
			return false;
		}
		hooking::RegisterStubs(SWF_EVENT_HIDDEN, copy, code.size(), std::move(entries), hiddenOffsets);
		log::Info("hidden: {} stores to NiAVObject::flags patched, {} show sites filtered ({} bytes of stubs)", sites.size(), showSites.size(), code.size());
		return true;
	}

	bool VerifyValues(const hooking::Image& image, std::span<const StoreValueSite> sites)
	{
		return Verify(image, sites, "value stores");
	}

	bool InstallSwitchIndex(const hooking::Image& image, std::span<const StoreValueSite> sites)
	{
		return InstallValues(image, sites, reinterpret_cast<std::uintptr_t>(&OnSwitchStore), SWF_EVENT_SWITCH_INDEX, switchOffsets, "switch index");
	}

	bool InstallFadeReset(const hooking::Image& image, std::span<const StoreValueSite> sites)
	{
		return InstallValues(image, sites, reinterpret_cast<std::uintptr_t>(&OnFadeResetStore), SWF_EVENT_FADE_RESET, fadeOffsets, "fade reset");
	}

	std::uint32_t SiteCount(std::uint32_t eventId)
	{
		switch (eventId) {
		case SWF_EVENT_HIDDEN:
			return static_cast<std::uint32_t>(hiddenOffsets.size());
		case SWF_EVENT_SWITCH_INDEX:
			return static_cast<std::uint32_t>(switchOffsets.size());
		case SWF_EVENT_FADE_RESET:
			return static_cast<std::uint32_t>(fadeOffsets.size());
		default:
			return 0;
		}
	}
}
