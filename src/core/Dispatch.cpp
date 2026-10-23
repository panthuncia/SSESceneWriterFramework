#include "core/Dispatch.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace swf::inline SWF_NAMESPACE::dispatch
{
	namespace
	{
		// Per writer thread: a shared counter would put every hooked call on one contended cache line.
		thread_local uint64_t sequence = 0;

		constexpr int kIdShift = 48;
		constexpr uint64_t kSerialMask = (1ull << kIdShift) - 1;

		/// A thread's scratch for PRE|POST subscriptions: blocks that never move, used as a stack.
		/// A call takes its words on entry and gives them back on exit, so nested hooked calls stack
		/// and a scratch pointer stays valid while deeper calls take more. Blocks are kept, so a
		/// thread allocates only when a call needs more than it ever has.
		class Arena
		{
		public:
			uintptr_t* Take(size_t words, size_t& markBlock, size_t& markTop)
			{
				if (blocks.empty())
					blocks.push_back(Block(std::max(words, kBlockWords)));
				markBlock = current;
				markTop = blocks[current].top;
				while (blocks[current].size - blocks[current].top < words) {
					if (++current == blocks.size())
						blocks.push_back(Block(std::max(words, kBlockWords)));
				}
				Block& block = blocks[current];
				uintptr_t* taken = block.words.get() + block.top;
				block.top += words;
				std::memset(taken, 0, words * sizeof(uintptr_t));
				return taken;
			}

			void Release(size_t markBlock, size_t markTop)
			{
				for (size_t i = markBlock + 1; i <= current; ++i)
					blocks[i].top = 0;
				current = markBlock;
				blocks[current].top = markTop;
			}

		private:
			static constexpr size_t kBlockWords = 1024;

			struct Block
			{
				explicit Block(size_t a_size) :
					words(std::make_unique<uintptr_t[]>(a_size)), size(a_size) {}
				std::unique_ptr<uintptr_t[]> words;
				size_t size;
				size_t top = 0;
			};

			std::vector<Block> blocks;
			size_t current = 0;
		};

		thread_local Arena arena;

		/// Builds an exact-size list from `entries`, numbering the PRE|POST entries' scratch slots.
		List* MakeList(const std::vector<Entry>& entries)
		{
			auto* list = new List{};
			list->count = static_cast<uint32_t>(entries.size());
			list->entries = std::make_unique<Entry[]>(entries.size());
			for (uint32_t i = 0; i < list->count; ++i) {
				Entry entry = entries[i];
				const bool both = (entry.phases & (SWF_PHASE_PRE | SWF_PHASE_POST)) == (SWF_PHASE_PRE | SWF_PHASE_POST);
				entry.scratchSlot = both ? list->scratchSlots++ : Entry::kNoSlot;
				list->entries[i] = entry;
			}
			return list;
		}

		std::vector<Entry> EntriesOf(const List* list)
		{
			return list ? std::vector<Entry>(list->entries.get(), list->entries.get() + list->count) : std::vector<Entry>{};
		}

		void Invoke(const Entry& entry, SWF_EventHeader& header)
		{
			uintptr_t fresh[SWF_SCRATCH_WORDS]{};
			entry.event(&header, fresh, entry.user);
		}
	}

	void Table::Publish(uint32_t id, List* next)
	{
		lists[id].store(next, std::memory_order_release);
		counts[id].store(next->count, std::memory_order_release);
	}

	SWF_Result Table::Add(uint32_t id, const Entry& entry, SWF_Subscription* out)
	{
		if (id == 0 || id >= kMaxIds || !out)
			return SWF_E_INVALID;

		std::lock_guard lock(mutex);
		std::vector<Entry> entries = EntriesOf(lists[id].load(std::memory_order_relaxed));
		Entry added = entry;
		added.serial = nextSerial++;
		// Stable: after every entry with priority <= the new one.
		const auto at = std::upper_bound(entries.begin(), entries.end(), added.priority,
			[](int32_t priority, const Entry& existing) { return priority < existing.priority; });
		entries.insert(at, added);

		Publish(id, MakeList(entries));
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
		std::vector<Entry> entries = EntriesOf(lists[id].load(std::memory_order_relaxed));
		const auto it = std::ranges::find(entries, serial, &Entry::serial);
		if (it == entries.end())
			return SWF_E_INVALID;
		entries.erase(it);
		Publish(id, MakeList(entries));
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
		if (!header.sequence)
			header.sequence = ++sequence;
	}

	void Fire(SWF_EventHeader& header)
	{
		const List* list = Events().Get(header.id);
		if (!list || list->count == 0)
			return;
		Stamp(header, SWF_PHASE_POST);
		for (uint32_t i = 0; i < list->count; ++i) {
			const Entry& entry = list->entries[i];
			if (entry.phases & SWF_PHASE_POST)
				Invoke(entry, header);
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

	Call::Call(uint32_t id) :
		list(Events().Get(id))
	{
		if (list && list->scratchSlots)
			scratch = arena.Take(size_t(list->scratchSlots) * SWF_SCRATCH_WORDS, markBlock, markTop);
	}

	Call::~Call()
	{
		if (scratch)
			arena.Release(markBlock, markTop);
	}

	void Call::Run(SWF_EventHeader& header, uint32_t phase)
	{
		if (Empty())
			return;
		Stamp(header, phase);
		for (uint32_t i = 0; i < list->count; ++i) {
			const Entry& entry = list->entries[i];
			if (!(entry.phases & phase))
				continue;
			if (entry.scratchSlot != Entry::kNoSlot)
				entry.event(&header, scratch + size_t(entry.scratchSlot) * SWF_SCRATCH_WORDS, entry.user);
			else
				Invoke(entry, header);
		}
	}
}
