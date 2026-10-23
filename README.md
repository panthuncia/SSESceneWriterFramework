# SceneWriterFramework (SWF)

SWF hooks the engine's scene writers once and gives every SKSE plugin the same view of scene changes: synchronous events, filter points, and a write/notify API.

It ships in two ways:

- **Standalone:** `SceneWriterFramework.dll` in `Data/SKSE/Plugins`.
- **Embedded:** a static library inside another mod. Community Shaders is the first.

Every copy in the process is a candidate. At SKSE `kPostLoad` they elect one provider: the newest copy that supports the running game version. That provider installs the hooks and serves the API to everyone. The algorithm is in [`include/swf/swf_client.h`](include/swf/swf_client.h).

Status: pre-release (0.x). The ABI is not frozen until 1.0.0. Only the AE 1.6.1170 tables exist.

## Hook families

| Family | Engine sites | Event / filter |
|---|---|---|
| hidden | 464 stores to `NiAVObject::flags` (+0xF4) that can change bit 0 | `SWF_EVENT_HIDDEN`; `SWF_FILTER_SHOW` at the 4 LOD show stores |
| switch-index | 10 stores to `NiSwitchNode::index` (+0x12C) | `SWF_EVENT_SWITCH_INDEX` (on change) |
| fade-reset | 11 inline `BSFadeNode::currentFade` (+0x130) resets | `SWF_EVENT_FADE_RESET` |
| structure | NiNode's 7 child-edit implementations, the classes overriding them, and the remove-all | `SWF_EVENT_CHILD_ATTACHED`, `SWF_EVENT_CHILD_DETACHING` |
| switch-structural | NiSwitchNode's own 7 child edits | `SWF_EVENT_SWITCH_STRUCTURAL` |
| moves | 3 reference movers and the cells' 2 update passes | `SWF_EVENT_MOVED` |
| fade / node / property / geometry / material calls | 35 engine functions and vtable slots (see `SWF_HOOK_*` in `swf.h`) | `SWF_EVENT_CALL(hook)`, before and after, with the arguments |

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

A detour test runs every detour family against fake vtables carrying real RTTI layouts, real detoured functions, and a synthetic call-site region.

A separate store test runs the stubs themselves. It builds a synthetic image from real table entries, patches it the same way the game would be patched, and executes the patched stores. It checks the stored values, the events, the show veto and site resolution.
