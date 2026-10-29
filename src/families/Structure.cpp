// Structure: NiNode's child edits as SWF_EVENT_CHILD_DETACHING (before the engine's detach, the
// child still attached) and SWF_EVENT_CHILD_ATTACHED (after the attach). NiNode's implementations
// are detoured rather than its vtable, so every class inheriting them is covered, and so are the
// classes that override them (BSParticleSystemManager moves world-space particle systems,
// BGSDecalNode detaches and sets, BSFaceGenNiNode attaches). An override that calls NiNode's
// reports the edit twice. NiSwitchNode's own edits are SWF_EVENT_SWITCH_STRUCTURAL.
#include "families/DetourFamilies.h"

#include "core/Dispatch.h"
#include "core/Log.h"
#include "hooking/Hooks.h"

#include <algorithm>
#include <vector>

namespace swf::inline SWF_NAMESPACE::detours
{
	namespace
	{
		// NiNode's child-edit slots (SE/AE).
		constexpr std::size_t kAttachChild = 0x35;
		constexpr std::size_t kDetachChild1 = 0x37;
		constexpr std::size_t kDetachChild2 = 0x38;
		constexpr std::size_t kDetachChildAt1 = 0x39;
		constexpr std::size_t kDetachChildAt2 = 0x3A;
		constexpr std::size_t kSetAt1 = 0x3B;
		constexpr std::size_t kSetAt2 = 0x3C;
		constexpr std::size_t kSlots[] = { kAttachChild, kDetachChild1, kDetachChild2, kDetachChildAt1, kDetachChildAt2, kSetAt1, kSetAt2 };

		// NiNode::children (+0x110, an NiTArray: data at +0x08, freeIdx at +0x12).
		constexpr std::size_t kChildrenData = 0x118;
		constexpr std::size_t kChildrenFreeIdx = 0x122;

		void* ChildAt(void* node, std::uint32_t index)
		{
			auto* bytes = static_cast<std::byte*>(node);
			auto* const* data = *reinterpret_cast<void* const* const*>(bytes + kChildrenData);
			const auto freeIdx = *reinterpret_cast<const std::uint16_t*>(bytes + kChildrenFreeIdx);
			return data && index < freeIdx ? data[index] : nullptr;
		}

		void FireChild(std::uint32_t id, void* parent, void* child, bool unchanged = false)
		{
			if (!*dispatch::Events().CountWord(id))
				return;
			SWF_ChildEvent event{};
			event.header.size = sizeof(event);
			event.header.id = id;
			event.parent = parent;
			event.child = child;
			event.unchanged = unchanged ? 1 : 0;
			dispatch::Fire(event.header);
		}

		void Detaching(void* parent, void* child) { FireChild(SWF_EVENT_CHILD_DETACHING, parent, child); }
		void Attached(void* parent, void* child, bool unchanged = false) { FireChild(SWF_EVENT_CHILD_ATTACHED, parent, child, unchanged); }

		// N: 0 NiNode's implementations, 1-3 the overriding classes'.
		template <int N>
		struct Edits
		{
			static inline void (*attach)(void*, void*, bool) = nullptr;
			static void Attach(void* node, void* child, bool firstAvail)
			{
				attach(node, child, firstAvail);
				Attached(node, child);
			}

			static inline void (*detach1)(void*, void*, void*) = nullptr;
			static void Detach1(void* node, void* child, void* out)
			{
				Detaching(node, child);
				detach1(node, child, out);
			}

			static inline void (*detach2)(void*, void*) = nullptr;
			static void Detach2(void* node, void* child)
			{
				Detaching(node, child);
				detach2(node, child);
			}

			static inline void (*detachAt1)(void*, std::uint32_t, void*) = nullptr;
			static void DetachAt1(void* node, std::uint32_t index, void* out)
			{
				Detaching(node, ChildAt(node, index));
				detachAt1(node, index, out);
			}

			static inline void (*detachAt2)(void*, std::uint32_t) = nullptr;
			static void DetachAt2(void* node, std::uint32_t index)
			{
				Detaching(node, ChildAt(node, index));
				detachAt2(node, index);
			}

			static inline void (*setAt1)(void*, std::uint32_t, void*, void*) = nullptr;
			static void SetAt1(void* node, std::uint32_t index, void* child, void* out)
			{
				void* previous = ChildAt(node, index);
				if (previous != child)
					Detaching(node, previous);
				setAt1(node, index, child, out);
				Attached(node, child, previous == child);
			}

			static inline void (*setAt2)(void*, std::uint32_t, void*) = nullptr;
			static void SetAt2(void* node, std::uint32_t index, void* child)
			{
				void* previous = ChildAt(node, index);
				if (previous != child)
					Detaching(node, previous);
				setAt2(node, index, child);
				Attached(node, child, previous == child);
			}

			// The detour for slot `slot`, its original set to `target`.
			static hooking::Detour For(std::size_t slot, std::uintptr_t target)
			{
				auto make = [&](auto& original, auto* thunk) {
					original = reinterpret_cast<std::remove_reference_t<decltype(original)>>(target);
					return hooking::Detour{ reinterpret_cast<void**>(&original), reinterpret_cast<void*>(thunk) };
				};
				switch (slot) {
				case kAttachChild:
					return make(attach, &Attach);
				case kDetachChild1:
					return make(detach1, &Detach1);
				case kDetachChild2:
					return make(detach2, &Detach2);
				case kDetachChildAt1:
					return make(detachAt1, &DetachAt1);
				case kDetachChildAt2:
					return make(detachAt2, &DetachAt2);
				case kSetAt1:
					return make(setAt1, &SetAt1);
				default:
					return make(setAt2, &SetAt2);
				}
			}
		};

		void (*removeAll)(void*) = nullptr;
		void RemoveAllChildren(void* node)
		{
			if (node) {
				const auto freeIdx = *reinterpret_cast<const std::uint16_t*>(static_cast<std::byte*>(node) + kChildrenFreeIdx);
				for (std::uint32_t i = 0; i < freeIdx; ++i)
					if (void* child = ChildAt(node, i))
						Detaching(node, child);
			}
			removeAll(node);
		}

		hooking::Detour EditFor(int n, std::size_t slot, std::uintptr_t target)
		{
			switch (n) {
			case 0:
				return Edits<0>::For(slot, target);
			case 1:
				return Edits<1>::For(slot, target);
			case 2:
				return Edits<2>::For(slot, target);
			default:
				return Edits<3>::For(slot, target);
			}
		}

		// NiSwitchNode's own implementations: the structural event after each.
		void SwitchStructural(void* node)
		{
			if (!*dispatch::Events().CountWord(SWF_EVENT_SWITCH_STRUCTURAL))
				return;
			SWF_NodeEvent event{};
			event.header.size = sizeof(event);
			event.header.id = SWF_EVENT_SWITCH_STRUCTURAL;
			event.node = node;
			dispatch::Fire(event.header);
		}

		struct SwitchEdits
		{
			static inline void (*attach)(void*, void*, bool) = nullptr;
			static void Attach(void* node, void* child, bool firstAvail) { attach(node, child, firstAvail), SwitchStructural(node); }
			static inline void (*detach1)(void*, void*, void*) = nullptr;
			static void Detach1(void* node, void* child, void* out) { detach1(node, child, out), SwitchStructural(node); }
			static inline void (*detach2)(void*, void*) = nullptr;
			static void Detach2(void* node, void* child) { detach2(node, child), SwitchStructural(node); }
			static inline void (*detachAt1)(void*, std::uint32_t, void*) = nullptr;
			static void DetachAt1(void* node, std::uint32_t index, void* out) { detachAt1(node, index, out), SwitchStructural(node); }
			static inline void (*detachAt2)(void*, std::uint32_t) = nullptr;
			static void DetachAt2(void* node, std::uint32_t index) { detachAt2(node, index), SwitchStructural(node); }
			static inline void (*setAt1)(void*, std::uint32_t, void*, void*) = nullptr;
			static void SetAt1(void* node, std::uint32_t index, void* child, void* out) { setAt1(node, index, child, out), SwitchStructural(node); }
			static inline void (*setAt2)(void*, std::uint32_t, void*) = nullptr;
			static void SetAt2(void* node, std::uint32_t index, void* child) { setAt2(node, index, child), SwitchStructural(node); }

			static hooking::Detour For(std::size_t slot, std::uintptr_t target)
			{
				auto make = [&](auto& original, auto* thunk) {
					original = reinterpret_cast<std::remove_reference_t<decltype(original)>>(target);
					return hooking::Detour{ reinterpret_cast<void**>(&original), reinterpret_cast<void*>(thunk) };
				};
				switch (slot) {
				case kAttachChild:
					return make(attach, &Attach);
				case kDetachChild1:
					return make(detach1, &Detach1);
				case kDetachChild2:
					return make(detach2, &Detach2);
				case kDetachChildAt1:
					return make(detachAt1, &DetachAt1);
				case kDetachChildAt2:
					return make(detachAt2, &DetachAt2);
				case kSetAt1:
					return make(setAt1, &SetAt1);
				default:
					return make(setAt2, &SetAt2);
				}
			}
		};

		bool VtablesMatch(const hooking::Image& image, std::initializer_list<hooking::Vtable> vtables)
		{
			return std::ranges::all_of(vtables, [&](const hooking::Vtable& vtable) { return hooking::VtableMatches(image, vtable); });
		}

		// The structure family's detours: NiNode's implementations, the overrides' that differ from them and from each other, and the
		// remove-all. The implementations come from the table, so a slot another plugin swapped first still leads to the engine's
		// (a 0 for one that does not match leaves the family out).
		std::vector<hooking::Detour> StructureDetours(const hooking::Image& image, const HookTargets& targets, std::vector<std::uintptr_t>& hooked)
		{
			std::vector<hooking::Detour> detours;
			for (std::size_t i = 0; i < std::size(kSlots); ++i) {
				const std::uintptr_t target = hooking::SlotImplementation(image, targets.niNode, kSlots[i], targets.niNodeEdits[i], "structure");
				hooked.push_back(target);
				detours.push_back(EditFor(0, kSlots[i], target));
			}
			for (int n = 0; n < 3; ++n) {
				for (std::size_t i = 0; i < std::size(kSlots); ++i) {
					const std::size_t slot = kSlots[i];
					const std::uintptr_t target = hooking::SlotImplementation(image, targets.nodeOverrides[n], slot, targets.overrideEdits[n][i], "structure");
					if (std::ranges::find(hooked, target) != hooked.end())
						continue;
					hooked.push_back(target);
					detours.push_back(EditFor(n + 1, slot, target));
				}
			}
			const std::uintptr_t all = hooking::At(image, targets.removeAllChildren);
			removeAll = reinterpret_cast<decltype(removeAll)>(all);
			hooked.push_back(all);
			detours.push_back({ reinterpret_cast<void**>(&removeAll), reinterpret_cast<void*>(&RemoveAllChildren) });
			return detours;
		}

		std::vector<hooking::Detour> SwitchDetours(const hooking::Image& image, const HookTargets& targets, std::vector<std::uintptr_t>& hooked)
		{
			std::vector<hooking::Detour> detours;
			for (std::size_t i = 0; i < std::size(kSlots); ++i) {
				const std::size_t slot = kSlots[i];
				const std::uintptr_t target = hooking::SlotImplementation(image, targets.niSwitchNode, slot, targets.switchEdits[i], "switch structural");
				// An implementation NiSwitchNode inherits is NiNode's: detouring it would make every
				// node's edit a switch's.
				if (target && target == hooking::SlotImplementation(image, targets.niNode, slot, targets.niNodeEdits[i], "switch structural")) {
					log::Warn("switch structural: NiSwitchNode slot {:#x} is NiNode's; not hooked", slot);
					continue;
				}
				hooked.push_back(target);
				detours.push_back(SwitchEdits::For(slot, target));
			}
			return detours;
		}

		bool AllInImage(const hooking::Image& image, const std::vector<std::uintptr_t>& targets, const char* what)
		{
			for (const std::uintptr_t target : targets) {
				if (!hooking::InImage(image, target)) {
					log::Warn("{}: a target at {:#x} is outside the image", what, target);
					return false;
				}
			}
			return true;
		}
	}

	bool VerifyStructure(const hooking::Image& image, const HookTargets& targets)
	{
		if (!VtablesMatch(image, { targets.niNode, targets.nodeOverrides[0], targets.nodeOverrides[1], targets.nodeOverrides[2] }))
			return false;
		std::vector<std::uintptr_t> hooked;
		StructureDetours(image, targets, hooked);
		return AllInImage(image, hooked, "structure");
	}

	bool InstallStructure(const hooking::Image& image, const HookTargets& targets)
	{
		std::vector<std::uintptr_t> hooked;
		const auto detours = StructureDetours(image, targets, hooked);
		if (!AllInImage(image, hooked, "structure") || !hooking::AttachAll(detours, "structure"))
			return false;
		log::Info("structure: {} child-edit implementations detoured", detours.size());
		return true;
	}

	bool VerifySwitchStructural(const hooking::Image& image, const HookTargets& targets)
	{
		if (!VtablesMatch(image, { targets.niNode, targets.niSwitchNode }))
			return false;
		std::vector<std::uintptr_t> hooked;
		SwitchDetours(image, targets, hooked);
		return AllInImage(image, hooked, "switch structural");
	}

	bool InstallSwitchStructural(const hooking::Image& image, const HookTargets& targets)
	{
		std::vector<std::uintptr_t> hooked;
		const auto detours = SwitchDetours(image, targets, hooked);
		if (!AllInImage(image, hooked, "switch structural") || !hooking::AttachAll(detours, "switch structural"))
			return false;
		log::Info("switch structural: {} NiSwitchNode child edits detoured", detours.size());
		return true;
	}
}
