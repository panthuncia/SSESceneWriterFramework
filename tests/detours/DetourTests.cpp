// Runs the detour families end to end against code in this executable: fake vtables in static
// data carrying real MSVC RTTI layouts (so VtableMatches reads them as it reads the game's), real
// functions detoured by Detours, fake nodes with NiNode's child array at its AE offsets, and a
// synthetic code region for the moves' call sites.
#include "../common/FakeRtti.h"
#include "core/Dispatch.h"
#include "families/CallHook.h"
#include "families/DetourFamilies.h"
#include "hooking/Hooks.h"

#include <Windows.h>

#include <bit>
#include <string>
#include <vector>

namespace
{
	using namespace swf;
	using namespace fake;
	using FakeVtable = fake::Vtable;

	std::vector<std::string> trace;  // what the "engine" functions did

	// ---- Fake nodes: vtable at +0, children data at +0x118, freeIdx at +0x122, userData at +0xF8.
	struct FakeNode
	{
		alignas(16) std::byte bytes[0x200]{};
		void* children[8]{};

		explicit FakeNode(FakeVtable* vtable = nullptr)
		{
			*reinterpret_cast<void**>(bytes) = vtable ? vtable->slots : nullptr;
			*reinterpret_cast<void***>(bytes + 0x118) = children;
		}
		std::uint16_t& FreeIdx() { return *reinterpret_cast<std::uint16_t*>(bytes + 0x122); }
		void*& UserData() { return *reinterpret_cast<void**>(bytes + 0xF8); }
		void* Self() { return bytes; }
	};

	FakeNode* NodeOf(void* self) { return reinterpret_cast<FakeNode*>(self); }

	// ---- The "engine" child edits (distinct bodies so the linker does not fold them).
	__declspec(noinline) void NodeAttach(void* self, void* child, bool)
	{
		trace.push_back("attach");
		auto* node = NodeOf(self);
		node->children[node->FreeIdx()++] = child;
	}
	__declspec(noinline) void NodeDetach1(void* self, void* child, void*)
	{
		trace.push_back("detach1");
		auto* node = NodeOf(self);
		for (auto& slot : node->children)
			if (slot == child)
				slot = nullptr;
	}
	__declspec(noinline) void NodeDetach2(void* self, void* child) { trace.push_back("detach2"), NodeDetach1(self, child, nullptr); }
	__declspec(noinline) void NodeDetachAt1(void* self, std::uint32_t index, void*) { trace.push_back("detachAt1"), NodeOf(self)->children[index] = nullptr; }
	__declspec(noinline) void NodeDetachAt2(void* self, std::uint32_t index) { trace.push_back("detachAt2"), NodeOf(self)->children[index] = nullptr; }
	__declspec(noinline) void NodeSetAt1(void* self, std::uint32_t index, void* child, void*) { trace.push_back("setAt1"), NodeOf(self)->children[index] = child; }
	__declspec(noinline) void NodeSetAt2(void* self, std::uint32_t index, void* child) { trace.push_back("setAt2"), NodeOf(self)->children[index] = child; }
	__declspec(noinline) void NodeRemoveAll(void* self)
	{
		trace.push_back("removeAll");
		auto* node = NodeOf(self);
		for (auto& slot : node->children)
			slot = nullptr;
		node->FreeIdx() = 0;
	}
	__declspec(noinline) void DecalDetach2(void* self, void* child) { trace.push_back("decal-detach2"), NodeDetach1(self, child, nullptr); }
	__declspec(noinline) void SwitchAttach(void* self, void* child, bool first) { trace.push_back("switch-attach"), NodeAttach(self, child, first); }
	__declspec(noinline) void SwitchOther(void* self, void*) { trace.push_back("switch-other"), (void)self; }

	void FillNodeSlots(FakeVtable& vtable)
	{
		vtable.slots[0x35] = reinterpret_cast<void*>(&NodeAttach);
		vtable.slots[0x37] = reinterpret_cast<void*>(&NodeDetach1);
		vtable.slots[0x38] = reinterpret_cast<void*>(&NodeDetach2);
		vtable.slots[0x39] = reinterpret_cast<void*>(&NodeDetachAt1);
		vtable.slots[0x3A] = reinterpret_cast<void*>(&NodeDetachAt2);
		vtable.slots[0x3B] = reinterpret_cast<void*>(&NodeSetAt1);
		vtable.slots[0x3C] = reinterpret_cast<void*>(&NodeSetAt2);
	}

	// ---- Event capture.
	std::vector<SWF_ChildEvent> childEvents;
	std::vector<void*> switchEvents;
	std::vector<SWF_MovedEvent> movedEvents;
	std::vector<SWF_CallEvent> callEvents;

	void Subscribe(std::uint32_t id, std::uint32_t phases, SWF_EventFn fn)
	{
		dispatch::Entry entry;
		entry.event = fn;
		entry.phases = phases;
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Events().Add(id, entry, &subscription) == SWF_OK);
	}

	// ---- Fade and material "engine" functions.
	__declspec(noinline) void FadeOnVisibleImpl(void*, void*, std::int32_t group) { trace.push_back("onVisible " + std::to_string(group)); }
	__declspec(noinline) std::uint64_t FadeUpdateImpl(void*, float amount, void*)
	{
		trace.push_back("fadeUpdate");
		return static_cast<std::uint64_t>(amount * 100.0f);
	}
	__declspec(noinline) void FadeLodLevelImpl(void*, float) { trace.push_back("lodLevel"); }
	__declspec(noinline) void FadeSetRangeImpl(void*, float, float) { trace.push_back("setRange"); }
	__declspec(noinline) void FadeSetLodTypeImpl(void*, std::uint8_t type) { trace.push_back("lodType " + std::to_string(type)); }
	__declspec(noinline) std::uint64_t FadeSnapImpl(void*, void*) { trace.push_back("snap"); return 0; }

	template <int V>
	__declspec(noinline) void MaterialCopy(void*, void*) { trace.push_back("copy " + std::to_string(V)); }
	template <int V>
	__declspec(noinline) void MaterialLoad(void*, std::uint64_t, void*) { trace.push_back("load " + std::to_string(V)); }
	template <int V>
	__declspec(noinline) void MaterialClear(void*) { trace.push_back("clear " + std::to_string(V)); }
	template <int V>
	__declspec(noinline) void MaterialReceive(void*, bool, bool, bool, bool, bool e) { trace.push_back("receive " + std::to_string(V) + (e ? "e" : "")); }
	__declspec(noinline) void FloatUpdate(void*, void*) { trace.push_back("float"); }
	__declspec(noinline) void ColorUpdate(void*, void*) { trace.push_back("color"); }

	template <int... V>
	void FillMaterials(HookTargets& targets, std::integer_sequence<int, V...>)
	{
		((targets.lightingMaterials[V] = [] {
			static char name[64];
			std::snprintf(name, sizeof(name), ".?AVMaterial%d@@", V);
			FakeVtable& vtable = NewVtable(name);
			vtable.slots[0x02] = reinterpret_cast<void*>(&MaterialCopy<V>);
			vtable.slots[0x08] = reinterpret_cast<void*>(&MaterialLoad<V>);
			vtable.slots[0x09] = reinterpret_cast<void*>(&MaterialClear<V>);
			vtable.slots[0x0A] = reinterpret_cast<void*>(&MaterialReceive<V>);
			return Ref(vtable);
		}()),
			...);
	}

	// ---- The tree animation list: add, a vetoable remove, clear.
	__declspec(noinline) void AnimAdd(void*, void* node, std::uint64_t, std::uint64_t) { trace.push_back("anim add " + std::to_string(node != nullptr)); }
	__declspec(noinline) std::uint64_t AnimRemove(void*, void*)
	{
		trace.push_back("anim remove");
		return 7;
	}
	__declspec(noinline) std::uint64_t AnimClear(void*, std::uint64_t, std::uint64_t, std::uint64_t)
	{
		trace.push_back("anim clear");
		return 9;
	}

	// ---- A standalone CallHook on a function with every argument kind.
	__declspec(noinline) double Mixed(std::int32_t a, float b, void* c, bool d, std::uint8_t e, float f, std::int64_t g)
	{
		trace.push_back("mixed");
		return a + b + (c ? 1 : 0) + (d ? 10 : 0) + e + f + static_cast<double>(g);
	}
}

int main()
{
	SetEnvironmentVariableW(L"SWF_LOG_PATH", L"NUL");
	image = hooking::GameImage();

	// VtableMatches: a match, a wrong name, an address outside the image.
	FakeVtable& probe = NewVtable(".?AVProbe@@");
	CHECK(hooking::VtableMatches(image, Ref(probe)));
	CHECK(!hooking::VtableMatches(image, { Rva(probe.slots), ".?AVOther@@" }));
	CHECK(!hooking::VtableMatches(image, { static_cast<std::uint32_t>(image.size + 0x1000), ".?AVProbe@@" }));

	// ---- CallHook: arguments as words, the result, PRE/POST scratch.
	{
		using Hook = calls::CallHook<SWF_HOOK_FADE_UPDATE, 77, double(std::int32_t, float, void*, bool, std::uint8_t, float, std::int64_t)>;
		Hook::original = &Mixed;
		Subscribe(SWF_EVENT_CALL(SWF_HOOK_FADE_UPDATE), SWF_PHASE_PRE | SWF_PHASE_POST, [](const SWF_EventHeader* header, uintptr_t* scratch, void*) {
			const auto& event = *reinterpret_cast<const SWF_CallEvent*>(header);
			if (header->phase == SWF_PHASE_PRE)
				scratch[2] = 0xABCD;
			else
				CHECK(scratch[2] == 0xABCD);
			callEvents.push_back(event);
		});
		int marker = 0;
		const double result = Hook::Thunk(-3, 1.5f, &marker, true, 200, -2.0f, -7);
		CHECK(result == -3 + 1.5 + 1 + 10 + 200 - 2.0 - 7);
		CHECK(callEvents.size() == 2 && trace.back() == "mixed");
		const auto& pre = callEvents[0];
		CHECK(pre.header.phase == SWF_PHASE_PRE && pre.hook == SWF_HOOK_FADE_UPDATE && pre.argCount == 7);
		CHECK(pre.args[0] == static_cast<std::uint64_t>(-3ll) && pre.args[1] == std::bit_cast<std::uint32_t>(1.5f));
		CHECK(pre.args[2] == reinterpret_cast<std::uintptr_t>(&marker) && pre.args[3] == 1 && pre.args[4] == 200);
		CHECK(pre.args[5] == std::bit_cast<std::uint32_t>(-2.0f) && pre.args[6] == static_cast<std::uint64_t>(-7ll));
		CHECK(callEvents[1].header.phase == SWF_PHASE_POST && callEvents[1].header.sequence == pre.header.sequence);
		callEvents.clear();
	}

	// ---- Structure: NiNode's slots, one override sharing NiNode's attach, the remove-all.
	FakeVtable& niNode = NewVtable(".?AVNiNode@@");
	FillNodeSlots(niNode);
	FakeVtable& decal = NewVtable(".?AVBGSDecalNode@@");
	FillNodeSlots(decal);
	decal.slots[0x38] = reinterpret_cast<void*>(&DecalDetach2);  // its own detach; the rest are NiNode's
	FakeVtable& particles = NewVtable(".?AVBSParticleSystemManager@@");
	FillNodeSlots(particles);
	FakeVtable& faceGen = NewVtable(".?AVBSFaceGenNiNode@@");
	FillNodeSlots(faceGen);
	FakeVtable& niSwitch = NewVtable(".?AVNiSwitchNode@@");
	FillNodeSlots(niSwitch);
	niSwitch.slots[0x35] = reinterpret_cast<void*>(&SwitchAttach);
	for (std::size_t slot = 0x37; slot <= 0x3C; ++slot)
		niSwitch.slots[slot] = reinterpret_cast<void*>(&SwitchOther);  // stands in for its own edits (one body)

	HookTargets targets{};
	targets.niNode = Ref(niNode);
	targets.nodeOverrides = { Ref(particles), Ref(decal), Ref(faceGen) };
	targets.removeAllChildren = Rva(reinterpret_cast<const void*>(&NodeRemoveAll));
	targets.niSwitchNode = Ref(niSwitch);

	CHECK(detours::VerifyStructure(image, targets));
	CHECK(detours::InstallStructure(image, targets));
	CHECK(detours::VerifySwitchStructural(image, targets));
	CHECK(detours::InstallSwitchStructural(image, targets));
	Subscribe(SWF_EVENT_CHILD_ATTACHED, SWF_PHASE_POST, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		childEvents.push_back(*reinterpret_cast<const SWF_ChildEvent*>(header));
	});
	Subscribe(SWF_EVENT_CHILD_DETACHING, SWF_PHASE_POST, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		const auto& event = *reinterpret_cast<const SWF_ChildEvent*>(header);
		// Before the engine's detach: the child is still in the array (or the slot is empty).
		auto* node = NodeOf(event.parent);
		bool present = event.child == nullptr;
		for (void* child : node->children)
			present |= child == event.child;
		CHECK(present);
		childEvents.push_back(event);
	});
	Subscribe(SWF_EVENT_SWITCH_STRUCTURAL, SWF_PHASE_POST, [](const SWF_EventHeader* header, uintptr_t*, void*) {
		switchEvents.push_back(reinterpret_cast<const SWF_NodeEvent*>(header)->node);
	});

	FakeNode node(&niNode);
	int a = 0, b = 0, c = 0;
	Call(niNode, 0x35, node.Self(), static_cast<void*>(&a), false);
	Call(niNode, 0x35, node.Self(), static_cast<void*>(&b), false);
	CHECK(childEvents.size() == 2 && childEvents[0].header.id == SWF_EVENT_CHILD_ATTACHED && childEvents[1].child == &b && node.children[1] == &b);
	Call(niNode, 0x39, node.Self(), std::uint32_t(0), static_cast<void*>(nullptr));  // DetachChildAt(0): a
	CHECK(childEvents.size() == 3 && childEvents[2].header.id == SWF_EVENT_CHILD_DETACHING && childEvents[2].child == &a && !node.children[0]);
	Call(niNode, 0x3C, node.Self(), std::uint32_t(1), static_cast<void*>(&c));  // SetAt(1, c) over b
	CHECK(childEvents.size() == 5 && childEvents[3].child == &b && childEvents[4].child == &c && !childEvents[4].unchanged);
	Call(niNode, 0x3C, node.Self(), std::uint32_t(1), static_cast<void*>(&c));  // SetAt(1, c) again
	CHECK(childEvents.size() == 6 && childEvents[5].header.id == SWF_EVENT_CHILD_ATTACHED && childEvents[5].unchanged == 1);
	reinterpret_cast<void (*)(void*)>(&NodeRemoveAll)(node.Self());  // detours the function itself
	CHECK(childEvents.size() == 7 && childEvents[6].child == &c && trace.back() == "removeAll");

	// The decal node's own detach is detoured once; its inherited slots are NiNode's (already hooked).
	childEvents.clear();
	FakeNode decalNode(&decal);
	Call(decal, 0x35, decalNode.Self(), static_cast<void*>(&a), false);
	CHECK(childEvents.size() == 1);
	Call(decal, 0x38, decalNode.Self(), static_cast<void*>(&a));
	// The override's detach, then NiNode's inside it: an override that calls NiNode's reports twice.
	CHECK(childEvents.size() == 3 && childEvents[1].header.id == SWF_EVENT_CHILD_DETACHING && childEvents[2].header.id == SWF_EVENT_CHILD_DETACHING);
	CHECK(trace.back() == "detach1");

	// NiSwitchNode's own edits: the switch event after the inner NiNode edit's.
	childEvents.clear();
	FakeNode switchNode(&niSwitch);
	Call(niSwitch, 0x35, switchNode.Self(), static_cast<void*>(&a), true);
	CHECK(switchEvents.size() == 1 && switchEvents[0] == switchNode.Self() && childEvents.size() == 1);
	Call(niSwitch, 0x38, switchNode.Self(), static_cast<void*>(&a));
	CHECK(switchEvents.size() == 2);

	// ---- Moves: three detoured writers and a patched `call [rax + 0x168]`, in a synthetic region
	// inside an image that also covers this executable (the writers are its functions).
	{
		auto* region = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
		CHECK(region);
		// writer stubs: `mov al, 1; nop x5; ret` (offset 0 is a table's "no address", so they start at 0x100)
		for (int i = 0; i < 3; ++i) {
			std::uint8_t* at = region + 0x100 * (i + 1);
			const std::uint8_t body[] = { 0xB0, 0x01, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3 };
			std::memcpy(at, body, sizeof(body));
		}
		// Two update passes: `sub rsp, 0x28; mov rax, [rcx]; call [rax + 0x168]; add rsp, 0x28; ret`
		const std::uint8_t pass[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
		std::memcpy(region + 0x400, pass, sizeof(pass));
		std::memcpy(region + 0x500, pass, sizeof(pass));
		const hooking::Image regionImage{ reinterpret_cast<std::uintptr_t>(region), 0x1000 };
		HookTargets moves = targets;
		moves.animationGraphPlace = 0x100;
		moves.reference3DPosition = 0x200;
		moves.graphAnimation = 0x300;
		moves.selectiveUpdateCalls = { 0x407, 0x507 };
		moves.selectiveUpdateBytes = { 0xFF, 0x90, 0x68, 0x01, 0x00, 0x00 };
		CHECK(detours::VerifyMoves(regionImage, moves));
		region[0x507] ^= 0xFF;
		CHECK(!detours::VerifyMoves(regionImage, moves));
		region[0x507] ^= 0xFF;
		Subscribe(SWF_EVENT_MOVED, SWF_PHASE_POST, [](const SWF_EventHeader* header, uintptr_t*, void*) {
			movedEvents.push_back(*reinterpret_cast<const SWF_MovedEvent*>(header));
		});
		CHECK(detours::InstallMoves(regionImage, moves));
		int reference = 0;
		CHECK(reinterpret_cast<bool (*)(void*)>(region + 0x100)(&reference));
		reinterpret_cast<void (*)(void*, bool)>(region + 0x200)(&reference, true);
		reinterpret_cast<void (*)(void*)>(region + 0x300)(&reference);
		CHECK(movedEvents.size() == 3 && movedEvents[0].writer == SWF_MOVE_ANIMATION_GRAPH_PLACE && movedEvents[1].writer == SWF_MOVE_REFERENCE_3D_POSITION &&
			  movedEvents[2].writer == SWF_MOVE_GRAPH_ANIMATION && movedEvents[2].key == &reference);

		// The update pass: the node's slot 0x2D runs, then its reference (userData) is the key.
		static int updates = 0;
		FakeVtable& updatable = NewVtable(".?AVUpdatable@@");
		updatable.slots[0x2D] = reinterpret_cast<void*>(+[](void*, void*, std::uint32_t) { ++updates; });
		FakeNode mover(&updatable);
		mover.UserData() = &reference;
		reinterpret_cast<void (*)(void*)>(region + 0x400)(mover.Self());
		CHECK(updates == 1 && movedEvents.size() == 4 && movedEvents[3].writer == SWF_MOVE_SELECTIVE_UPDATE && movedEvents[3].key == &reference);
		mover.UserData() = nullptr;
		reinterpret_cast<void (*)(void*)>(region + 0x400)(mover.Self());
		CHECK(updates == 2 && movedEvents.size() == 5 && movedEvents[4].key == mover.Self());
		reinterpret_cast<void (*)(void*)>(region + 0x500)(mover.Self());
		CHECK(updates == 3 && movedEvents.size() == 6 && movedEvents[5].writer == SWF_MOVE_SELECTIVE_UPDATE);
	}

	// ---- Fade calls: a detoured slot implementation and four functions.
	FakeVtable& fadeNode = NewVtable(".?AVBSFadeNode@@");
	fadeNode.slots[0x34] = reinterpret_cast<void*>(&FadeOnVisibleImpl);
	targets.fadeNode = Ref(fadeNode);
	targets.functions[SWF_HOOK_FADE_UPDATE] = Rva(reinterpret_cast<const void*>(&FadeUpdateImpl));
	targets.functions[SWF_HOOK_FADE_LOD_LEVEL] = Rva(reinterpret_cast<const void*>(&FadeLodLevelImpl));
	targets.functions[SWF_HOOK_FADE_SET_RANGE] = Rva(reinterpret_cast<const void*>(&FadeSetRangeImpl));
	targets.functions[SWF_HOOK_FADE_SET_LOD_TYPE] = Rva(reinterpret_cast<const void*>(&FadeSetLodTypeImpl));
	targets.functions[SWF_HOOK_FADE_SNAP] = Rva(reinterpret_cast<const void*>(&FadeSnapImpl));
	CHECK(detours::VerifyCalls(SWF_CAP_FADE_CALLS, image, targets));
	CHECK(detours::InstallCalls(SWF_CAP_FADE_CALLS, image, targets));
	callEvents.clear();
	auto record = [](const SWF_EventHeader* header, uintptr_t*, void*) { callEvents.push_back(*reinterpret_cast<const SWF_CallEvent*>(header)); };
	for (std::uint32_t hook = SWF_HOOK_FADE_ON_VISIBLE; hook <= SWF_HOOK_FADE_SET_LOD_TYPE; ++hook)
		if (hook != SWF_HOOK_FADE_UPDATE)  // already subscribed (both phases) above
			Subscribe(SWF_EVENT_CALL(hook), SWF_PHASE_POST, record);
	FakeNode fade(&fadeNode);
	Call(fadeNode, 0x34, fade.Self(), static_cast<void*>(nullptr), std::int32_t(-2));
	CHECK(trace.back() == "onVisible -2" && callEvents.size() == 1 && callEvents[0].hook == SWF_HOOK_FADE_ON_VISIBLE && callEvents[0].args[2] == static_cast<std::uint64_t>(-2ll));
	const auto update = reinterpret_cast<std::uint64_t (*)(void*, float, void*)>(&FadeUpdateImpl);
	CHECK(update(fade.Self(), 0.25f, nullptr) == 25);
	CHECK(callEvents.size() == 3 && callEvents[2].hook == SWF_HOOK_FADE_UPDATE && callEvents[2].result == 25 &&
		  callEvents[2].args[1] == std::bit_cast<std::uint32_t>(0.25f));
	reinterpret_cast<void (*)(void*, std::uint8_t)>(&FadeSetLodTypeImpl)(fade.Self(), 6);
	CHECK(trace.back() == "lodType 6" && callEvents.back().hook == SWF_HOOK_FADE_SET_LOD_TYPE && callEvents.back().args[1] == 6);

	// ---- Material calls: two controllers and 14 material vtables, slots swapped; each swapped slot
	// calls its own vtable's original.
	FakeVtable& floatController = NewVtable(".?AVFloatController@@");
	floatController.slots[0x27] = reinterpret_cast<void*>(&FloatUpdate);
	FakeVtable& colorController = NewVtable(".?AVColorController@@");
	colorController.slots[0x27] = reinterpret_cast<void*>(&ColorUpdate);
	targets.floatController = Ref(floatController);
	targets.colorController = Ref(colorController);
	FillMaterials(targets, std::make_integer_sequence<int, 14>{});
	CHECK(detours::VerifyCalls(SWF_CAP_MATERIAL_CALLS, image, targets));
	CHECK(detours::InstallCalls(SWF_CAP_MATERIAL_CALLS, image, targets));
	for (std::uint32_t hook = SWF_HOOK_FLOAT_CONTROLLER_UPDATE; hook <= SWF_HOOK_MATERIAL_RECEIVE_VALUES; ++hook)
		Subscribe(SWF_EVENT_CALL(hook), SWF_PHASE_POST, record);
	callEvents.clear();
	Call(floatController, 0x27, static_cast<void*>(&a), static_cast<void*>(nullptr));
	CHECK(trace.back() == "float" && callEvents.back().hook == SWF_HOOK_FLOAT_CONTROLLER_UPDATE);
	for (int v = 0; v < 14; ++v) {
		auto& vtable = vtables[nextVtable - 14 + v];
		Call(vtable, 0x09, static_cast<void*>(&a));
		CHECK(trace.back() == "clear " + std::to_string(v) && callEvents.back().hook == SWF_HOOK_MATERIAL_CLEAR_TEXTURES && callEvents.back().args[0] == reinterpret_cast<std::uintptr_t>(&a));
		Call(vtable, 0x0A, static_cast<void*>(&a), true, false, true, false, true);
		CHECK(trace.back() == "receive " + std::to_string(v) + "e" && callEvents.back().argCount == 6 && callEvents.back().args[5] == 1);
	}

	// ---- Tree animation calls: the remove's filter vetoes the call and both its events.
	{
		targets.functions[SWF_HOOK_TREE_ANIM_ADD] = Rva(reinterpret_cast<const void*>(&AnimAdd));
		targets.functions[SWF_HOOK_TREE_ANIM_REMOVE] = Rva(reinterpret_cast<const void*>(&AnimRemove));
		targets.functions[SWF_HOOK_TREE_ANIM_CLEAR] = Rva(reinterpret_cast<const void*>(&AnimClear));
		CHECK(detours::VerifyCalls(SWF_CAP_TREE_ANIM_CALLS, image, targets));
		CHECK(detours::InstallCalls(SWF_CAP_TREE_ANIM_CALLS, image, targets));
		for (std::uint32_t hook = SWF_HOOK_TREE_ANIM_ADD; hook <= SWF_HOOK_TREE_ANIM_CLEAR; ++hook)
			Subscribe(SWF_EVENT_CALL(hook), SWF_PHASE_POST, record);
		static void* held = nullptr;
		dispatch::Entry filter;
		filter.filter = [](const SWF_EventHeader* header, void*) -> std::int32_t {
			const auto& event = *reinterpret_cast<const SWF_CallEvent*>(header);
			CHECK(header->id == SWF_FILTER_CALL(SWF_HOOK_TREE_ANIM_REMOVE) && event.argCount == 2);
			return reinterpret_cast<void*>(event.args[1]) == held;
		};
		SWF_Subscription subscription = 0;
		CHECK(dispatch::Filters().Add(SWF_FILTER_CALL(SWF_HOOK_TREE_ANIM_REMOVE), filter, &subscription) == SWF_OK);
		callEvents.clear();
		int manager = 0, kept = 0, other = 0;
		held = &kept;
		const auto remove = reinterpret_cast<std::uint64_t (*)(void*, void*)>(&AnimRemove);
		trace.clear();
		CHECK(remove(&manager, &kept) == 0 && trace.empty() && callEvents.empty());
		CHECK(remove(&manager, &other) == 7 && trace.back() == "anim remove" && callEvents.size() == 1 && callEvents[0].result == 7);
		reinterpret_cast<void (*)(void*, void*, std::uint64_t, std::uint64_t)>(&AnimAdd)(&manager, &kept, 1, 2);
		CHECK(trace.back() == "anim add 1" && callEvents.back().hook == SWF_HOOK_TREE_ANIM_ADD && callEvents.back().args[3] == 2);
		CHECK(reinterpret_cast<std::uint64_t (*)(void*, std::uint64_t, std::uint64_t, std::uint64_t)>(&AnimClear)(&manager, 0, 0, 0) == 9);
		CHECK(callEvents.back().hook == SWF_HOOK_TREE_ANIM_CLEAR && callEvents.back().result == 9);
		CHECK(dispatch::Filters().Remove(subscription) == SWF_OK);
		CHECK(remove(&manager, &kept) == 7);
	}

	// ---- LOD segment calls: hooked at their call sites, in a synthetic region; a site that calls
	// elsewhere leaves the family out.
	{
		auto* region = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
		CHECK(region);
		// The writers: `ret`. Each caller: `sub rsp, 0x28; call writer; add rsp, 0x28; ret`, its call at +4.
		const std::uint32_t writers[] = { 0x300, 0x100, 0x200 };  // not 0: a table's "no address"
		for (const auto writer : writers)
			region[writer] = 0xC3;
		auto call = [&](std::uint32_t site, std::uint32_t to) {
			const std::uint8_t frame[] = { 0x48, 0x83, 0xEC, 0x28 }, unframe[] = { 0x48, 0x83, 0xC4, 0x28, 0xC3 };
			std::memcpy(region + site - 4, frame, sizeof(frame));
			region[site] = 0xE8;
			const auto displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(to) - (site + 5));
			std::memcpy(region + site + 1, &displacement, 4);
			std::memcpy(region + site + 5, unframe, sizeof(unframe));
		};
		call(0x404, writers[0]);
		call(0x424, writers[1]);
		call(0x444, writers[2]);
		call(0x464, writers[2]);
		const hooking::Image regionImage{ reinterpret_cast<std::uintptr_t>(region), 0x1000 };
		HookTargets lod{};
		lod.functions[SWF_HOOK_LOD_SEGMENT_SHOW] = writers[0];
		lod.functions[SWF_HOOK_LOD_SEGMENT_HIDE] = writers[1];
		lod.functions[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = writers[2];
		lod.callSites[SWF_HOOK_LOD_SEGMENT_SHOW] = { { 0x404 }, 1 };
		lod.callSites[SWF_HOOK_LOD_SEGMENT_HIDE] = { { 0x424 }, 1 };
		lod.callSites[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = { { 0x444, 0x464 }, 2 };
		call(0x464, writers[1]);
		CHECK(!detours::VerifyCalls(SWF_CAP_LOD_SEGMENT_CALLS, regionImage, lod));
		call(0x464, writers[2]);
		CHECK(detours::VerifyCalls(SWF_CAP_LOD_SEGMENT_CALLS, regionImage, lod));
		CHECK(detours::InstallCalls(SWF_CAP_LOD_SEGMENT_CALLS, regionImage, lod));
		for (std::uint32_t hook = SWF_HOOK_LOD_SEGMENT_SHOW; hook <= SWF_HOOK_LOD_SEGMENT_SHOW_ALL; ++hook)
			Subscribe(SWF_EVENT_CALL(hook), SWF_PHASE_POST, record);
		callEvents.clear();
		int shape = 0;
		reinterpret_cast<void (*)(void*, std::uint64_t)>(region + 0x400)(&shape, 3);
		reinterpret_cast<void (*)(void*, std::uint64_t)>(region + 0x420)(&shape, 4);
		reinterpret_cast<void (*)(void*)>(region + 0x440)(&shape);
		reinterpret_cast<void (*)(void*)>(region + 0x460)(&shape);
		CHECK(callEvents.size() == 4 && callEvents[0].hook == SWF_HOOK_LOD_SEGMENT_SHOW && callEvents[0].args[1] == 3);
		CHECK(callEvents[1].hook == SWF_HOOK_LOD_SEGMENT_HIDE && callEvents[1].args[1] == 4 && callEvents[1].args[0] == reinterpret_cast<std::uintptr_t>(&shape));
		CHECK(callEvents[2].hook == SWF_HOOK_LOD_SEGMENT_SHOW_ALL && callEvents[3].hook == SWF_HOOK_LOD_SEGMENT_SHOW_ALL);
		// The writers themselves, called directly, are not hooked.
		reinterpret_cast<void (*)(void*)>(region + writers[2])(&shape);
		CHECK(callEvents.size() == 4);
	}

	std::puts("detour families: all checks passed");
	return 0;
}
