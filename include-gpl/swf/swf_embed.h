// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Panthuncia. GPL-3.0-or-later with the exceptions in EXCEPTIONS.md.
//
// SceneWriterFramework host integration, for mods that embed SWF (link swf::embedded). Embedding
// compiles SWF's core into the host, so the host is under the GPL; plugins under another license
// connect to a provider through the interface headers instead (swf/swf_loader.h, swf/swf.hpp).
//
// A host calls SWF_Embed_OnPostLoad() from its SKSE kPostLoad message handler. Linking
// this header's translation unit pulls in the SWF_Candidate_v1 export, which makes the
// host DLL a candidate in the election. One DLL embeds at most one copy of SWF.
#pragma once

#include "swf/swf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SWF_LOG_DEBUG 0
#define SWF_LOG_INFO 1
#define SWF_LOG_WARN 2
#define SWF_LOG_ERROR 3

/// Receives this copy's log lines. Without a sink, SWF appends to
/// Documents/My Games/Skyrim Special Edition/SKSE/SceneWriterFramework.log.
typedef void (*SWF_LogSink)(int32_t level, const char* message);

/// Routes this copy's log output to `sink` (null restores the default file).
void SWF_Embed_SetLogSink(SWF_LogSink sink);

/// Runs the election, activating the winning candidate (which may live in another DLL).
/// Call once from the SKSE kPostLoad handler. Returns the provider's API, or null if
/// SWF is disabled, unsupported on this runtime, or no candidate could activate.
const SWF_Api* SWF_Embed_OnPostLoad(void);

#ifdef __cplusplus
}
#endif

#pragma comment(linker, "/INCLUDE:SWF_Candidate_v1")
