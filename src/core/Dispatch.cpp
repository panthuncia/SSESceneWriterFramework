#include "core/Dispatch.h"

#include <Windows.h>

namespace swf::inline SWF_NAMESPACE::dispatch
{
	namespace
	{
		std::atomic<uint64_t> sequence{ 0 };

		constexpr int kIdShift = 48;
		constexpr uint64_t kSerialMask = (1ull << kIdShift) - 1;
	}

	SWF_Result Table::Add(uint32_t id, const Entry& entry, SWF_Subscription* out)
	{
		if (id == 0 || id >= kMaxIds || !out)
			return SWF_E_INVALID;

		std::lock_guard lock(mutex);
		const List* current = lists[id].load(std::memory_order_relaxed);
		if (current && current->count >= kMaxSubscribers)
			return SWF_E_FULL;

		auto* next = new List(current ? *current : List{});
		Entry added = entry;
		added.serial = nextSerial++;

		// Stable insert: after every entry with priority <= the new one.
		uint32_t at = next->count;
		while (at > 0 && next->entries[at - 1].priority > added.priority) {
			next->entries[at] = next->entries[at - 1];
			--at;
		}
		next->entries[at] = added;
		++next->count;

		lists[id].store(next, std::memory_order_release);
		*out = (static_cast<uint64_t>(id) << kIdShift) | added.serial;
		return SWF_OK;
	}

	SWF_Result Table::Remove(SWF_Subscription subscription)
	{
		const auto id = static_cast<uint32_t>(subscription >> kIdShift);
		const uint64_t serial = subscription & kSerialMask;
		if (id == 0 || id >= kMaxIds)
			return SWF_E_INVALID;

		std::lock_guard lock(mutex);
		const List* current = lists[id].load(std::memory_order_relaxed);
		if (!current)
			return SWF_E_INVALID;

		auto* next = new List{};
		bool found = false;
		for (uint32_t i = 0; i < current->count; ++i) {
			if (current->entries[i].serial == serial)
				found = true;
			else
				next->entries[next->count++] = current->entries[i];
		}
		if (!found) {
			delete next;
			return SWF_E_INVALID;
		}
		lists[id].store(next, std::memory_order_release);
		return SWF_OK;
	}

	Table& Events()
	{
		static Table table;
		return table;
	}

	Table& Filters()
	{
		static Table table;
		return table;
	}

	void Stamp(SWF_EventHeader& header, uint32_t phase)
	{
		header.phase = phase;
		header.threadId = GetCurrentThreadId();
		header.sequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
	}

	void Fire(SWF_EventHeader& header)
	{
		const List* list = Events().Get(header.id);
		if (!list || list->count == 0)
			return;
		Stamp(header, SWF_PHASE_POST);
		for (uint32_t i = 0; i < list->count; ++i) {
			const Entry& entry = list->entries[i];
			if (entry.phases & SWF_PHASE_POST) {
				uintptr_t scratch = 0;
				entry.event(&header, &scratch, entry.user);
			}
		}
	}

	bool Vetoed(uint32_t filterId, SWF_EventHeader& header)
	{
		const List* list = Filters().Get(filterId);
		if (!list || list->count == 0)
			return false;
		Stamp(header, SWF_PHASE_PRE);
		for (uint32_t i = 0; i < list->count; ++i) {
			const Entry& entry = list->entries[i];
			if (entry.filter(&header, entry.user))
				return true;
		}
		return false;
	}

	void Call::Run(SWF_EventHeader& header, uint32_t phase)
	{
		if (Empty())
			return;
		Stamp(header, phase);
		for (uint32_t i = 0; i < list->count; ++i) {
			const Entry& entry = list->entries[i];
			if (entry.phases & phase)
				entry.event(&header, &scratch[i], entry.user);
		}
	}
}
