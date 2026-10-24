# SceneWriterFramework (SWF)

SWF hooks the engine's scene writers once and gives every SKSE plugin the same view of scene changes: synchronous events, filter points, and a write/notify API.

It ships in two ways:

- **Standalone:** `SceneWriterFramework.dll` in `Data/SKSE/Plugins`.
- **Embedded:** a static library inside another mod. Community Shaders is the first.

Every copy in the process is a candidate. At SKSE `kPostLoad` they elect one provider: the newest copy that supports the running game version. That provider installs the hooks and serves the API to everyone. The algorithm is in [`include/swf/swf_client.h`](include/swf/swf_client.h).

Status: pre-release (0.x). The ABI is not frozen until 1.0.0.

## Game versions

SWF has tables for SE 1.5.97, AE 1.6.1170 and AE 1.7.104, and needs [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
(SWF reads its `Data/SKSE/Plugins/version*-<version>.bin` itself, formats 1, 2 and 5, with no CommonLib). The detour families'
functions and vtables are Address Library ids, one set for SE and one for every AE version; only the call sites inside a function
are positioned per version. Without the database no detour family installs. The store families are per-version offsets from a
program-wide scan of that version's executable, since a store's table must list every store and a scan is the only way to know that
it does.

## Hook families

| Family | Engine sites | Event / filter |
|---|---|---|
| hidden | 464 stores to `NiAVObject::flags` (+0xF4) that can change bit 0 | `SWF_EVENT_HIDDEN`; `SWF_FILTER_SHOW` at the 4 LOD show stores |
| switch-index | 10 stores to `NiSwitchNode::index` (+0x12C) | `SWF_EVENT_SWITCH_INDEX` (on change) |
| fade-reset | 11 inline `BSFadeNode::currentFade` (+0x130) resets | `SWF_EVENT_FADE_RESET` |
| structure | NiNode's 7 child-edit implementations, the classes overriding them, and the remove-all | `SWF_EVENT_CHILD_ATTACHED`, `SWF_EVENT_CHILD_DETACHING` |
| switch-structural | NiSwitchNode's own 7 child edits | `SWF_EVENT_SWITCH_STRUCTURAL` |
| moves | 4 reference movers (projectiles' world updates among them) and the cells' 2 update passes | `SWF_EVENT_MOVED` |
| fade / node / property / geometry / material calls | 36 engine functions and vtable slots (see `SWF_HOOK_*` in `swf.h`) | `SWF_EVENT_CALL(hook)`, before and after, with the arguments |
| actor calls | `Actor::SetAlpha` on 3 vtables, and 3 actor move writers | `SWF_EVENT_CALL(hook)` |
| decal calls | the 5 edits of a `BGSDecalNode`'s decal array | `SWF_EVENT_CALL(SWF_HOOK_DECAL_ARRAY_EDIT)` |
| LOD segment calls | object LOD's segment show, hide and show-all, at all 9 of their call sites | `SWF_EVENT_CALL(hook)` |
| tree LOD calls | the tree LOD refill at its 2 call sites, and the instance shape's AddGroup and RemoveGroup | `SWF_EVENT_CALL(hook)` |
| tree animation calls | the tree manager's animation list: add, remove, clear | `SWF_EVENT_CALL(hook)`; `SWF_FILTER_CALL(SWF_HOOK_TREE_ANIM_REMOVE)` |
| effect calls | the effect shader controllers and the effect material's rewrite | `SWF_EVENT_CALL(hook)` |

The provider checks every site before it writes any of them: store and call-site bytes, and each vtable's RTTI name. A family whose sites another plugin has already changed is left out, and its `SWF_CAP_*` bit stays clear. Each client decides from `GetCaps()` whether it can run.

## Embedding

```cmake
include(extern/SceneWriterFramework/cmake/SWFEmbed.cmake)
target_link_libraries(MyPlugin PRIVATE swf::embedded)
```

```cpp
#include "swf/swf_embed.h"

// SKSE messaging handler
case SKSE::MessagingInterface::kPostLoad:
	swfApi = SWF_Embed_OnPostLoad();  // null: disabled, unsupported runtime, or no provider
	break;
```

## Client-only use (no embedded copy)

```cpp
#include "swf/swf_client.h"

const SWF_Api* api = swf::client::Connect();  // at kPostPostLoad or later
```

## Using the API

Subscribe at SKSE `kPostPostLoad` or later. Check `GetCaps()` first: a family that did not install refuses its subscriptions with
`SWF_E_UNSUPPORTED`.

```cpp
void OnAttached(const SWF_EventHeader* event, uintptr_t* scratch, void* user)
{
	const auto& child = *reinterpret_cast<const SWF_ChildEvent*>(event);
	// child.parent, child.child: on the thread that made the edit
}

SWF_Subscription subscription = 0;
api->Subscribe(SWF_EVENT_CHILD_ATTACHED, SWF_PHASE_POST, 0, &OnAttached, nullptr, &subscription);
```

Events are synchronous: the callback runs on the writing thread, inside the engine's call, before the engine continues. Keep it
short, take no lock the engine might hold, and hand heavy work to your own thread.

**Engine calls.** `SWF_EVENT_CALL(hook)` fires PRE before the engine function and POST after it, with its arguments in
`SWF_CallEvent::args` and, at POST, its result. To compare a value before and after a call, subscribe to both phases: the
`SWF_SCRATCH_WORDS` scratch words passed to the callback are yours, and they carry over from that call's PRE to its POST. Calls
nest, and each has its own scratch.

**Filters.** A filter runs before a write and may veto it by returning nonzero. `SWF_FILTER_SHOW` keeps an object hidden at the
engine's LOD show stores. `SWF_FILTER_CALL(hook)` skips a vetoable call (marked in `swf.h`), with its events, returning 0 to the
caller.

**Your own writes.** SWF sees only the engine's writers. For the fields it can store (the hidden bit, a geometry's shader and
alpha properties, an alpha property's flags: `SWF_WRITE_*`), write through `Write(kind, object, value)` instead of storing them
yourself: SWF makes the store between `SWF_EVENT_SCENE_WRITE`'s PRE and POST and counts the properties' references as the engine
does, so subscribers see your write as they see the engine's. For any other direct write, by storing to the object or through
CommonLib's inline setters, call `NotifyWritten(object, SWF_FIELD_*)`. Every subscriber to `SWF_EVENT_WRITTEN` then hears of it.

**Hides within a frame.** Some engine code hides a node and shows it again within one frame, around the views it should not
appear in. `Main::Draw` hides the first-person skeleton for the main camera's cull and the sun's casters, and shows it again
for the first-person draw. `TESWaterReflections::Update` hides the water shapes and the player's 3D around its reflection faces.
These stores are in the hidden table like any other and each one fires `SWF_EVENT_HIDDEN`, a hide and a show per frame. SWF does
not merge or drop the pair. A client that renders views mid-frame needs the bit those views saw, and only the client knows
which views it draws. A client that reads the live bit at some point in the frame can land between the hide and the show. If it
compares that bit with what it has tracked from the events, it should leave these nodes out.

**Integrity.** `CheckIntegrity(fn, user)` reads every patch back. It reports each one that another plugin has rewritten since SWF
installed it, naming the module the new bytes lead into, and logs it. It repairs nothing. The standalone plugin runs it at
`kDataLoaded`, and an embedding host may run it too.

## Compatibility with other hooks

SWF installs at `kPostLoad`, earlier than most plugins hook. Its stubs live in a block of their own just past the game image,
like a CommonLib trampoline. On a target another plugin also hooks:

- **Detours and other prologue hooks** (Detours, MinHook, CommonLib's `detour_thunk`, hand-rolled ones that copy whole
  instructions) chain in either order. The later hook is the outer one.
- **Vtable swaps** that keep the previous implementation (CommonLib's `write_vfunc`) chain in either order.
- **Call sites** another plugin rewrote first fail SWF's check, and the family is left out. A rewrite after SWF's that keeps the
  current call target (`write_call<5>`) chains.
- **Unsafe cases.** These break, or hide calls from SWF:
    - a hook installed after SWF that copies a fixed number of prologue bytes, or scans for the original ones;
    - a replacement that never calls the original.

  `CheckIntegrity` names the module behind every patch that changed after install.

The interop test covers each of these combinations.

## User settings

Settings live in `Data/SKSE/Plugins/SceneWriterFramework.ini`:

```ini
[Election]
Disable=0
PinHost=CommunityShaders.dll   ; try this host's copy first
PinVersion=0.1.0               ; or a specific version (major.minor.patch[.build])
```

The log is `Documents/My Games/Skyrim Special Edition/SKSE/SceneWriterFramework.log`. Every line names the host DLL and SWF version that wrote it.

## Building and testing

```
cmake --preset vs
cmake --build --preset vs-release
ctest --preset vs-release
```

The tests cover the frozen ABI layout and the election, with each scenario in its own process. The scenarios use four host DLLs that embed different versions, plus a client-only DLL. They check:

- that the result doesn't depend on the order hosts run the election;
- version tie-breaking;
- fallback when a candidate fails to activate, and the case where all fail;
- the INI pin and disable settings;
- an unsupported runtime;
- a candidate that loads late;
- client-only connection;
- dispatch order.

A dispatch test subscribes hundreds of subscribers to one event and runs nested PRE/POST calls whose scratch spans several arena blocks.

A detour test runs every detour family against fake vtables carrying real RTTI layouts, real detoured functions, and a synthetic call-site region. It also covers the hooks at call sites and the vetoable call filter.

An interop test hooks SWF's targets the way other plugins do, both before and after SWF, and checks that every hook runs once, in order. It uses Detours, a hand-rolled prologue hook, vtable swaps and call-site rewrites. It then checks that the integrity check reports exactly the patches rewritten after SWF, and traces each one to its module.

A separate store test runs the stubs themselves. It builds a synthetic image from real table entries, patches it the same way the game would be patched, and executes the patched stores. It checks the stored values, the events, the show veto and site resolution.

Two tools check a game version's tables against its executable (an unpacked copy: Steam's DRM stub hides the code), without
launching the game:

- `swf_table_check <exe> <se|ae|ae17> <version*-*.bin>` runs every family's verify pass, its targets resolved through that
  version's Address Library database.
- `tools/store_scan.py <exe>` (Python, `pip install capstone`) lists every hidden store, and every fade reset's store-then-bit-14
  site, as table entries. It finds every entry of the SE and AE tables; the AE 1.7.104 tables came from it.
