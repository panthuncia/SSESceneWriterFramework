// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Panthuncia. The MIT License: see LICENSE in this directory.
//
// SceneWriterFramework for C++: a header-only layer over the C ABI (swf.h) and the loader
// (swf_loader.h). Engine objects stay void*; swf_commonlib.h, under the GPL, types them as
// CommonLib's RE:: classes.
//
//   const swf::Api api = swf::Api::Connect();  // at kPostPostLoad or later
//   if (api.Has(SWF_CAP_STRUCTURE))
//       api.Subscribe<SWF_EVENT_CHILD_ATTACHED>(SWF_PHASE_POST, 0, [](const SWF_ChildEvent& event) { ... });
//
// A callback may take the event alone, or the event and its scratch words (swf::Scratch).
// SWF dispatches on the writing thread, inside the engine's call.
#pragma once

#include "swf/swf.h"
#include "swf/swf_loader.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace swf
{
	/// A subscription's scratch words for one engine call (see SWF_EventFn).
	using Scratch = std::span<uintptr_t, SWF_SCRATCH_WORDS>;

	/// The event struct an event id dispatches: SWF_CallEvent for SWF_EVENT_CALL(hook).
	template <uint32_t Id>
	struct EventOf
	{
		static_assert(Id >= SWF_EVENT_CALL_BASE && Id < SWF_EVENT_CALL_BASE + SWF_HOOK_COUNT, "not an SWF event id");
		using type = SWF_CallEvent;
	};
	template <>
	struct EventOf<SWF_EVENT_WRITTEN> { using type = SWF_WrittenEvent; };
	template <>
	struct EventOf<SWF_EVENT_HIDDEN> { using type = SWF_HiddenEvent; };
	template <>
	struct EventOf<SWF_EVENT_SWITCH_INDEX> { using type = SWF_SwitchIndexEvent; };
	template <>
	struct EventOf<SWF_EVENT_FADE_RESET> { using type = SWF_FadeResetEvent; };
	template <>
	struct EventOf<SWF_EVENT_CHILD_ATTACHED> { using type = SWF_ChildEvent; };
	template <>
	struct EventOf<SWF_EVENT_CHILD_DETACHING> { using type = SWF_ChildEvent; };
	template <>
	struct EventOf<SWF_EVENT_SWITCH_STRUCTURAL> { using type = SWF_NodeEvent; };
	template <>
	struct EventOf<SWF_EVENT_MOVED> { using type = SWF_MovedEvent; };
	template <>
	struct EventOf<SWF_EVENT_SCENE_WRITE> { using type = SWF_SceneWriteEvent; };
	template <uint32_t Id>
	using Event = typename EventOf<Id>::type;

	/// The event struct a filter id passes: SWF_CallEvent for SWF_FILTER_CALL(hook).
	template <uint32_t Id>
	struct FilterOf
	{
		static_assert(Id >= SWF_FILTER_CALL_BASE && Id < SWF_FILTER_CALL_BASE + SWF_HOOK_COUNT, "not an SWF filter id");
		using type = SWF_CallEvent;
	};
	template <>
	struct FilterOf<SWF_FILTER_SHOW> { using type = SWF_ShowFilterEvent; };
	template <uint32_t Id>
	using FilterEvent = typename FilterOf<Id>::type;

	/// A call event's argument or result word as T, as SWF_CallEvent stores it: pointers and integers
	/// as they are, bools as 0 or 1, floats from their IEEE bits in the low 32 bits.
	template <class T>
	T As(uint64_t word)
	{
		if constexpr (std::is_pointer_v<T>)
			return reinterpret_cast<T>(static_cast<uintptr_t>(word));
		else if constexpr (std::is_same_v<T, bool>)
			return word != 0;
		else if constexpr (std::is_same_v<T, float>)
			return std::bit_cast<float>(static_cast<uint32_t>(word));
		else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
			return static_cast<T>(word);
		else
			static_assert(sizeof(T) == 0, "swf::As: a pointer, integer, enum, bool or float");
	}

	/// Argument `index` of a call event as T (0 past its argCount).
	template <class T>
	T Arg(const SWF_CallEvent& event, uint32_t index)
	{
		return As<T>(index < event.argCount && index < SWF_CALL_MAX_ARGS ? event.args[index] : 0);
	}

	/// A call event's result as T (POST only; 0 at PRE and for void functions).
	template <class T>
	T Result(const SWF_CallEvent& event)
	{
		return As<T>(event.result);
	}

	/// A packed implVersion as "major.minor.patch.build".
	inline std::string VersionString(uint64_t version)
	{
		char text[32];
		return SWF_FormatVersion(version, text, sizeof(text));
	}

	/// An engine site behind a store stub (Api::ResolveSite).
	struct Site
	{
		uint32_t eventId = 0;
		uint32_t site = 0;
		uint32_t siteOffset = 0;
	};

	namespace detail
	{
		// A callable kept for the process: SWF publishes subscriber lists without waiting for dispatches
		// already running, so one may still call a callable after its Unsubscribe returns. A stateless
		// callable (a function, or a lambda without captures) needs no storage.
		template <class Fn>
		inline constexpr bool kStateless = std::is_empty_v<Fn> && std::is_default_constructible_v<Fn>;

		template <class Fn>
		void* Keep(Fn&& fn)
		{
			if constexpr (kStateless<std::decay_t<Fn>>)
				return nullptr;
			else
				return new std::decay_t<Fn>(std::forward<Fn>(fn));
		}

		template <class Fn>
		void Drop(void* user)
		{
			if constexpr (!kStateless<Fn>)
				delete static_cast<Fn*>(user);
		}

		template <class Fn>
		Fn& Callable(void* user)
		{
			if constexpr (kStateless<Fn>) {
				static Fn fn{};
				return fn;
			} else
				return *static_cast<Fn*>(user);
		}

		template <class Raw, class View, class Fn>
		void Event(const SWF_EventHeader* header, uintptr_t* scratch, void* user)
		{
			View view(*reinterpret_cast<const Raw*>(header));
			Fn& fn = Callable<Fn>(user);
			if constexpr (std::is_invocable_v<Fn&, View, Scratch>)
				fn(view, Scratch(scratch, SWF_SCRATCH_WORDS));
			else
				fn(view);
		}

		template <class Raw, class View, class Fn>
		int32_t Filter(const SWF_EventHeader* header, void* user)
		{
			View view(*reinterpret_cast<const Raw*>(header));
			return Callable<Fn>(user)(view) ? 1 : 0;
		}
	}

	/// The provider's API. A default or failed Api is empty: every call returns SWF_E_NOT_READY (or 0).
	class Api
	{
	public:
		Api() = default;
		explicit Api(const SWF_Api* api) :
			api(api) {}

		/// Connects to the provider (SWF_Connect): at kPostPostLoad or later. Empty if there is none
		/// or it is older than `abiMinor`.
		static Api Connect(uint32_t abiMinor = SWF_ABI_MINOR) { return Api(SWF_Connect(abiMinor)); }

		explicit operator bool() const { return api != nullptr; }
		const SWF_Api* Raw() const { return api; }
		uint32_t AbiMinor() const { return api ? api->abiMinor : 0; }

		SWF_Caps Caps() const { return api ? api->GetCaps() : 0; }
		/// Whether every family in `caps` installed.
		bool Has(SWF_Caps caps) const { return (Caps() & caps) == caps; }

		std::optional<SWF_ProviderInfo> ProviderInfo() const
		{
			SWF_ProviderInfo info{};
			info.size = sizeof(info);
			if (!api || api->GetProviderInfo(&info) != SWF_OK)
				return std::nullopt;
			return info;
		}

		/// Subscribes `fn` to event `Id`: fn(const Event<Id>&) or fn(const Event<Id>&, Scratch). Lower
		/// priorities run first. A callable with state is copied and kept for the process, since a
		/// dispatch already running may call it after Unsubscribe.
		template <uint32_t Id, class Fn>
		SWF_Result Subscribe(uint32_t phases, int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			return SubscribeAs<Id, const Event<Id>&>(phases, priority, std::forward<Fn>(fn), out);
		}

		/// Subscribe, with the event passed as View, constructed from the event struct (swf_commonlib.h's
		/// typed events).
		template <uint32_t Id, class View, class Fn>
		SWF_Result SubscribeAs(uint32_t phases, int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			using Callable = std::decay_t<Fn>;
			static_assert(std::is_invocable_v<Callable&, View> || std::is_invocable_v<Callable&, View, Scratch>,
				"swf::Api::Subscribe: fn(event) or fn(event, swf::Scratch)");
			if (!api)
				return SWF_E_NOT_READY;
			void* user = detail::Keep(std::forward<Fn>(fn));
			SWF_Subscription subscription = 0;
			const SWF_Result result = api->Subscribe(Id, phases, priority, &detail::Event<Event<Id>, View, Callable>, user, &subscription);
			if (result != SWF_OK)
				detail::Drop<Callable>(user);
			else if (out)
				*out = subscription;
			return result;
		}

		SWF_Result Unsubscribe(SWF_Subscription subscription) const { return api ? api->Unsubscribe(subscription) : SWF_E_NOT_READY; }

		/// Adds `fn` as filter `Id`: fn(const FilterEvent<Id>&) returns true to veto the write. Kept as
		/// Subscribe keeps its callable.
		template <uint32_t Id, class Fn>
		SWF_Result AddFilter(int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			return AddFilterAs<Id, const FilterEvent<Id>&>(priority, std::forward<Fn>(fn), out);
		}

		template <uint32_t Id, class View, class Fn>
		SWF_Result AddFilterAs(int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			using Callable = std::decay_t<Fn>;
			static_assert(std::is_invocable_r_v<bool, Callable&, View>, "swf::Api::AddFilter: fn(event) -> bool (true vetoes)");
			if (!api)
				return SWF_E_NOT_READY;
			void* user = detail::Keep(std::forward<Fn>(fn));
			SWF_Subscription filter = 0;
			const SWF_Result result = api->AddFilter(Id, priority, &detail::Filter<FilterEvent<Id>, View, Callable>, user, &filter);
			if (result != SWF_OK)
				detail::Drop<Callable>(user);
			else if (out)
				*out = filter;
			return result;
		}

		SWF_Result RemoveFilter(SWF_Subscription filter) const { return api ? api->RemoveFilter(filter) : SWF_E_NOT_READY; }

		/// Reports a direct write SWF could not observe (SWF_EVENT_WRITTEN).
		SWF_Result NotifyWritten(void* object, SWF_Fields fields) const { return api ? api->NotifyWritten(object, fields) : SWF_E_NOT_READY; }

		/// Writes through SWF (SWF_WRITE_*), around SWF_EVENT_SCENE_WRITE. abiMinor >= 2.
		SWF_Result Write(uint32_t kind, void* object, uint64_t value) const
		{
			if (!api)
				return SWF_E_NOT_READY;
			return api->abiMinor >= 2 ? api->Write(kind, object, value) : SWF_E_UNSUPPORTED;
		}
		/// An NiAVObject's hidden bit.
		SWF_Result SetHidden(void* object, bool hidden) const { return Write(SWF_WRITE_HIDDEN, object, hidden ? 1 : 0); }
		/// A BSGeometry's shader property (null clears it).
		SWF_Result SetShaderProperty(void* geometry, void* property) const { return Write(SWF_WRITE_SHADER_PROPERTY, geometry, reinterpret_cast<uintptr_t>(property)); }
		/// A BSGeometry's alpha property (null clears it).
		SWF_Result SetAlphaProperty(void* geometry, void* property) const { return Write(SWF_WRITE_ALPHA_PROPERTY, geometry, reinterpret_cast<uintptr_t>(property)); }
		/// An NiAlphaProperty's flags.
		SWF_Result SetAlphaFlags(void* property, uint16_t flags) const { return Write(SWF_WRITE_ALPHA_FLAGS, property, flags); }

		uint32_t SiteCount(uint32_t eventId) const { return api ? api->SiteCount(eventId) : 0; }

		/// The engine store behind an address inside one of SWF's store stubs.
		std::optional<Site> ResolveSite(uintptr_t address) const
		{
			Site site;
			if (!api || api->ResolveSite(address, &site.eventId, &site.site, &site.siteOffset) != SWF_OK)
				return std::nullopt;
			return site;
		}

		/// Re-reads SWF's patches; fn(const SWF_IntegrityEntry&) runs for each one rewritten since, before
		/// this returns. Returns their count. abiMinor >= 1.
		template <class Fn>
		uint32_t CheckIntegrity(Fn&& fn) const
		{
			if (!api || api->abiMinor < 1)
				return 0;
			return api->CheckIntegrity([](const SWF_IntegrityEntry* entry, void* user) { (*static_cast<std::remove_reference_t<Fn>*>(user))(*entry); },
				const_cast<void*>(static_cast<const void*>(std::addressof(fn))));
		}
		uint32_t CheckIntegrity() const { return api && api->abiMinor >= 1 ? api->CheckIntegrity(nullptr, nullptr) : 0; }

	private:
		const SWF_Api* api = nullptr;
	};
}
