#pragma once

#include "core/Version.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace swf::inline SWF_NAMESPACE::dispatch
{
	inline constexpr uint32_t kMaxIds = 256;
	inline constexpr uint32_t kMaxSubscribers = 32;

	struct Entry
	{
		SWF_EventFn event = nullptr;
		SWF_FilterFn filter = nullptr;
		void* user = nullptr;
		int32_t priority = 0;
		uint32_t phases = 0;
		uint64_t serial = 0;
	};

	/// Immutable once published; replaced copy-on-write and never freed, so dispatch reads
	/// it without locks while subscriptions change.
	struct List
	{
		uint32_t count = 0;
		Entry entries[kMaxSubscribers];
	};

	class Table
	{
	public:
		SWF_Result Add(uint32_t id, const Entry& entry, SWF_Subscription* out);
		SWF_Result Remove(SWF_Subscription subscription);

		const List* Get(uint32_t id) const
		{
			return id < kMaxIds ? lists[id].load(std::memory_order_acquire) : nullptr;
		}

		/// A word that is nonzero while `id` has entries, at a fixed address for the process;
		/// store stubs test it inline before calling into the dispatch.
		const std::atomic<uint32_t>* CountWord(uint32_t id) const { return &counts[id]; }

	private:
		std::array<std::atomic<const List*>, kMaxIds> lists{};
		std::array<std::atomic<uint32_t>, kMaxIds> counts{};
		std::mutex mutex;
		uint64_t nextSerial = 1;
	};

	Table& Events();
	Table& Filters();

	/// Stamps phase, writer thread and sequence into `header`.
	void Stamp(SWF_EventHeader& header, uint32_t phase);

	/// Fires a POST-only event (store events, notifications).
	void Fire(SWF_EventHeader& header);

	/// True if a filter registered for `filterId` vetoes the write `header` describes.
	bool Vetoed(uint32_t filterId, SWF_EventHeader& header);

	/// PRE/POST dispatch around one engine call. Holds the subscriber snapshot taken at
	/// Pre() so every subscriber sees both phases with its own scratch word.
	class Call
	{
	public:
		explicit Call(uint32_t id) :
			list(Events().Get(id)) {}

		bool Empty() const { return !list || list->count == 0; }

		void Pre(SWF_EventHeader& header) { Run(header, SWF_PHASE_PRE); }
		void Post(SWF_EventHeader& header) { Run(header, SWF_PHASE_POST); }

	private:
		void Run(SWF_EventHeader& header, uint32_t phase);

		const List* list;
		uintptr_t scratch[kMaxSubscribers]{};
	};
}
