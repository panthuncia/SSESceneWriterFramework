#pragma once

#include "core/Version.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace swf::inline SWF_NAMESPACE::dispatch
{
	inline constexpr uint32_t kMaxIds = 256;

	struct Entry
	{
		SWF_EventFn event = nullptr;
		SWF_FilterFn filter = nullptr;
		void* user = nullptr;
		int32_t priority = 0;
		uint32_t phases = 0;
		uint64_t serial = 0;
		uint32_t scratchSlot = 0;  // index among the PRE|POST entries; kNoSlot for single-phase ones
		static constexpr uint32_t kNoSlot = ~0u;
	};

	/// Immutable once published, sized to its entries; replaced copy-on-write and never freed,
	/// so dispatch reads it without locks while subscriptions change.
	struct List
	{
		uint32_t count = 0;
		uint32_t scratchSlots = 0;  // PRE|POST entries, each owning SWF_SCRATCH_WORDS per call
		std::unique_ptr<Entry[]> entries;
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
		void Publish(uint32_t id, List* next);

		std::array<std::atomic<const List*>, kMaxIds> lists{};
		std::array<std::atomic<uint32_t>, kMaxIds> counts{};
		std::mutex mutex;
		uint64_t nextSerial = 1;
	};

	Table& Events();
	Table& Filters();

	/// Stamps phase and writer thread into `header`, and a sequence number unless it has one (the
	/// POST of a call keeps its PRE's).
	void Stamp(SWF_EventHeader& header, uint32_t phase);

	/// Fires a POST-only event (store events, notifications).
	void Fire(SWF_EventHeader& header);

	/// True if a filter registered for `filterId` vetoes the write `header` describes.
	bool Vetoed(uint32_t filterId, SWF_EventHeader& header);

	/// PRE/POST dispatch around one engine call. Holds the subscriber snapshot taken at
	/// construction, so every subscriber sees both phases, and the PRE|POST subscribers' scratch,
	/// taken from this thread's scratch arena and given back on destruction (nested calls stack).
	class Call
	{
	public:
		explicit Call(uint32_t id);
		~Call();
		Call(const Call&) = delete;
		Call& operator=(const Call&) = delete;

		bool Empty() const { return !list || list->count == 0; }

		void Pre(SWF_EventHeader& header) { Run(header, SWF_PHASE_PRE); }
		void Post(SWF_EventHeader& header) { Run(header, SWF_PHASE_POST); }

	private:
		void Run(SWF_EventHeader& header, uint32_t phase);

		const List* list;
		uintptr_t* scratch = nullptr;
		size_t markBlock = 0;
		size_t markTop = 0;
	};
}
