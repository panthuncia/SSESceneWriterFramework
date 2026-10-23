// The dispatch has no subscriber limit: exact-size lists, and PRE|POST scratch from a per-thread
// arena that nested hooked calls stack on.
#include "core/Dispatch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

	constexpr uint32_t kOuter = 10;
	constexpr uint32_t kInner = 11;
	constexpr uint32_t kPost = 12;
	constexpr int kOuterCount = 300;
	constexpr int kInnerCount = 2000;  // more scratch than one arena block holds
	constexpr int kPostCount = 500;

	std::vector<intptr_t> order;
	std::vector<uintptr_t*> preScratch;
	int innerPre = 0, innerPost = 0, outerChecked = 0;

	SWF_Subscription Add(uint32_t id, uint32_t phases, int32_t priority, SWF_EventFn fn, intptr_t tag)
	{
		dispatch::Entry entry;
		entry.event = fn;
		entry.phases = phases;
		entry.priority = priority;
		entry.user = reinterpret_cast<void*>(tag);
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Events().Add(id, entry, &subscription) == SWF_OK);
		return subscription;
	}

	void FireNested()
	{
		SWF_EventHeader header{ sizeof(header), kInner };
		dispatch::Call call(kInner);
		call.Pre(header);
		call.Post(header);
	}

	void Inner(const SWF_EventHeader* header, uintptr_t* scratch, void* user)
	{
		const auto tag = reinterpret_cast<uintptr_t>(user);
		if (header->phase == SWF_PHASE_PRE) {
			CHECK(scratch[0] == 0 && scratch[3] == 0);
			scratch[0] = tag;
			scratch[3] = ~tag;
			++innerPre;
		} else {
			CHECK(scratch[0] == tag && scratch[3] == ~tag);
			++innerPost;
		}
	}

	void Outer(const SWF_EventHeader* header, uintptr_t* scratch, void* user)
	{
		const auto tag = reinterpret_cast<uintptr_t>(user);
		if (header->phase == SWF_PHASE_PRE) {
			for (int w = 0; w < SWF_SCRATCH_WORDS; ++w) {
				CHECK(scratch[w] == 0);
				scratch[w] = tag * 10 + w;
			}
			preScratch.push_back(scratch);
			// An engine call made from inside a callback: a nested hooked call takes more scratch.
			if (tag == 0)
				FireNested();
		} else {
			CHECK(preScratch[tag] == scratch);
			for (int w = 0; w < SWF_SCRATCH_WORDS; ++w)
				CHECK(scratch[w] == tag * 10 + w);
			++outerChecked;
		}
	}

	void PostOnly(const SWF_EventHeader*, uintptr_t* scratch, void* user)
	{
		for (int w = 0; w < SWF_SCRATCH_WORDS; ++w)
			CHECK(scratch[w] == 0);
		scratch[0] = 1;  // never seen by anyone else
		order.push_back(reinterpret_cast<intptr_t>(user));
	}
}

int main()
{
	// Many PRE|POST subscribers; equal priorities keep subscription order.
	for (int i = 0; i < kOuterCount; ++i)
		Add(kOuter, SWF_PHASE_PRE | SWF_PHASE_POST, 0, &Outer, i);
	for (int i = 0; i < kInnerCount; ++i)
		Add(kInner, SWF_PHASE_PRE | SWF_PHASE_POST, 0, &Inner, i + 1);
	CHECK(dispatch::Events().Get(kOuter)->count == kOuterCount && dispatch::Events().Get(kOuter)->scratchSlots == kOuterCount);

	for (int round = 0; round < 3; ++round) {
		preScratch.clear();
		innerPre = innerPost = outerChecked = 0;
		SWF_EventHeader header{ sizeof(header), kOuter };
		{
			dispatch::Call call(kOuter);
			call.Pre(header);
			call.Post(header);
		}
		CHECK(outerChecked == kOuterCount && innerPre == kInnerCount && innerPost == kInnerCount);
	}

	// Post-only subscribers, added in reverse priority, run in priority order with fresh scratch.
	std::vector<SWF_Subscription> handles;
	for (int i = kPostCount - 1; i >= 0; --i)
		handles.push_back(Add(kPost, SWF_PHASE_POST, i, &PostOnly, i));
	CHECK(dispatch::Events().Get(kPost)->scratchSlots == 0);
	SWF_EventHeader post{ sizeof(post), kPost };
	dispatch::Fire(post);
	CHECK(order.size() == kPostCount);
	for (int i = 0; i < kPostCount; ++i)
		CHECK(order[i] == i);

	// Removing from the middle keeps the rest in order.
	CHECK(dispatch::Events().Remove(handles[kPostCount - 1 - 250]) == SWF_OK);  // priority 250
	CHECK(dispatch::Events().Remove(handles[kPostCount - 1 - 250]) == SWF_E_INVALID);
	order.clear();
	dispatch::Fire(post);
	CHECK(order.size() == kPostCount - 1 && order[249] == 249 && order[250] == 251);
	CHECK(*dispatch::Events().CountWord(kPost) == kPostCount - 1);

	std::puts("dispatch: all checks passed");
	return 0;
}
