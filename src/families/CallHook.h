#pragma once

#include "core/Dispatch.h"

#include <bit>
#include <cstdint>
#include <type_traits>

namespace swf::inline SWF_NAMESPACE::calls
{
	/// An argument or return value as SWF_CallEvent carries it.
	template <class T>
	std::uint64_t Word(T value)
	{
		if constexpr (std::is_same_v<T, float>)
			return std::bit_cast<std::uint32_t>(value);
		else if constexpr (std::is_pointer_v<T>)
			return reinterpret_cast<std::uintptr_t>(value);
		else if constexpr (std::is_same_v<T, bool>)
			return value ? 1 : 0;
		else if constexpr (std::is_signed_v<T>)
			return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
		else
			return static_cast<std::uint64_t>(value);
	}

	/// True for the hooks SWF_FILTER_CALL(hook) may veto.
	constexpr bool Vetoable(std::uint32_t hook)
	{
		return hook == SWF_HOOK_TREE_ANIM_REMOVE;
	}

	/**
	 * One engine function's hook: Thunk replaces it (a detour, or a vtable slot), and `original`
	 * calls on. With no subscriber a call costs one list load. `Instance` tells apart hooks that
	 * share an id but call different originals (one per vtable).
	 */
	template <std::uint32_t Hook, std::uint32_t Instance, class Signature>
	struct CallHook;

	template <std::uint32_t Hook, std::uint32_t Instance, class R, class... Args>
	struct CallHook<Hook, Instance, R(Args...)>
	{
		static_assert(sizeof...(Args) <= SWF_CALL_MAX_ARGS);
		static inline R (*original)(Args...) = nullptr;

		static R Thunk(Args... args)
		{
			dispatch::Call call(SWF_EVENT_CALL(Hook));
			bool filtered = false;
			if constexpr (Vetoable(Hook))
				filtered = *dispatch::Filters().CountWord(SWF_FILTER_CALL(Hook)) != 0;
			if (call.Empty() && !filtered)
				return original(args...);
			SWF_CallEvent event{};
			event.header.size = sizeof(event);
			event.header.id = SWF_EVENT_CALL(Hook);
			event.hook = Hook;
			event.argCount = sizeof...(Args);
			std::uint32_t i = 0;
			((event.args[i++] = Word(args)), ...);
			if (filtered) {
				// The filters see the call event as the filter id; a veto skips the call and its events.
				event.header.id = SWF_FILTER_CALL(Hook);
				if (dispatch::Vetoed(SWF_FILTER_CALL(Hook), event.header))
					return R();
				event.header.id = SWF_EVENT_CALL(Hook);
			}
			call.Pre(event.header);
			if constexpr (std::is_void_v<R>) {
				original(args...);
				call.Post(event.header);
			} else {
				R result = original(args...);
				event.result = Word(result);
				call.Post(event.header);
				return result;
			}
		}

		/// For hooking::AttachAll: `original` must hold the target first.
		static void** OriginalSlot() { return reinterpret_cast<void**>(&original); }
		static void* ThunkAddress() { return reinterpret_cast<void*>(&Thunk); }
	};
}
