# Changelog

## 0.1.0 (unreleased)

First release. Supports SE 1.5.97, AE 1.6.1170 and AE 1.7.104.

### Licensing and headers

- SWF is licensed under GPL-3.0-or-later with the Modding, Linking and Interface exceptions (EXCEPTIONS.md).
- MIT interface headers in `include/swf`, CMake target `swf::interface` (no dependencies):
  - `swf.h`, the C ABI;
  - `swf_loader.h`, a header-only C loader (`SWF_Connect`, `SWF_FindProvider`, `SWF_FormatVersion`);
  - `swf.hpp`, a header-only C++ layer with typed event structs, kept callables, write helpers and call argument casts.
- GPL headers in `include-gpl/swf`:
  - `swf_commonlib.h` (`swf::commonlib`), with events and writes typed as CommonLibSSE-NG's `RE::` classes;
  - `swf_embed.h` (`swf::embedded`).
- `swf_client.h` is gone. Clients use `swf_loader.h` or `swf.hpp`, and the election moved into the core.

### Provider and election

- Each copy of SWF, standalone or embedded, exports `SWF_Candidate_v1`.
- At `kPostLoad` the copies elect one provider: the newest copy that supports the running game version. The INI can pin a host or a version, or disable SWF.

### Hook families

| Kind | Families |
|---|---|
| Stores | hidden, switch-index, fade-reset |
| Detours | structure, switch-structural, moves |
| Call families | fade, node, property, geometry, material, actor, decal, LOD segment, tree LOD, tree animation, effect |

- The detour families' targets are Address Library ids, read from the game's own database (formats 1, 2 and 5): one set for SE, one for AE, with per-version positions for the call sites inside a function. The store tables are per-version scans.
- Every site is verified before anything is written.
- A family whose sites changed is left out, and its capability bit stays clear.

### API

- Events, PRE and POST, with scratch words for each call.
- Filters: `SWF_FILTER_SHOW`, and `SWF_FILTER_CALL` for vetoable calls.
- `NotifyWritten` for a plugin's own writes.
- `SWF_HOOK_FLORA_HARVEST_ALPHAS`: a flora's harvested look, where the engine changes alpha thresholds inline.
- `Write` for a plugin's scene writes: SWF makes the store between `SWF_EVENT_SCENE_WRITE`'s PRE and POST (the hidden bit, a geometry's shader and alpha properties, an alpha property's flags), so subscribers see them as they see the engine's writers.
- `SiteCount`, `ResolveSite` and `CheckIntegrity`.

### ABI notes

The ABI stays pre-release until 1.0.0, so struct layouts and ids may still change. From 1.0.0:

- the C ABI only grows (`tests/abi_layout.cpp` freezes the layouts);
- new functions go at the end of `SWF_Api`, and `SWF_ABI_MINOR` rises with them;
- event, filter, hook and capability ids are only ever added, never changed.

Within 0.x:

- `SWF_ABI_MINOR` 1 added `CheckIntegrity`.
- `SWF_ABI_MINOR` 2 added `Write` and `SWF_EVENT_SCENE_WRITE`.
- `SWF_ABI_MINOR` 3 added `SWF_MovedEvent::keyKind` (`SWF_MOVED_KEY_*`: whether the key is a reference or a node).
- A client asks for the minor it was built with, so it connects only to a provider at least that new.
