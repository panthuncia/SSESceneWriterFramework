/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Panthuncia. The MIT License: see LICENSE in this directory.
 *
 * SceneWriterFramework loader: finds the provider and returns its API. Header-only, C or C++.
 *
 * SWF's copies elect their provider at SKSE kPostLoad, so connect at kPostPostLoad or later:
 *
 *   const SWF_Api* api = SWF_Connect(SWF_ABI_MINOR);
 *   if (!api)
 *       ... // no provider: SceneWriterFramework is not installed, is disabled, or does not support this game version
 *
 * A client never activates a provider: it finds the one candidate the election activated, among every
 * loaded module exporting SWF_Candidate_v1 (the SceneWriterFramework plugin, or a mod embedding SWF).
 */
#pragma once

#include "swf/swf.h"

#include <Windows.h>
#include <Psapi.h>
#include <stddef.h>
#include <stdio.h>

#pragma comment(lib, "psapi.lib")

#ifdef __cplusplus
extern "C" {
#endif

/* The active candidate's info, or null if no candidate of this ABI major is active. */
static inline const SWF_CandidateInfo* SWF_FindProvider(void)
{
	HMODULE stackModules[512];
	HMODULE* modules = stackModules;
	DWORD capacity = (DWORD)sizeof(stackModules);
	DWORD needed = 0;
	const SWF_CandidateInfo* provider = NULL;
	DWORD i;

	for (;;) {
		if (!EnumProcessModules(GetCurrentProcess(), modules, capacity, &needed)) {
			needed = 0;
			break;
		}
		if (needed <= capacity)
			break;
		if (modules != stackModules)
			HeapFree(GetProcessHeap(), 0, modules);
		modules = (HMODULE*)HeapAlloc(GetProcessHeap(), 0, needed);
		if (!modules)
			return NULL;
		capacity = needed;
	}

	for (i = 0; i < needed / sizeof(HMODULE) && !provider; ++i) {
		const SWF_CandidateFn candidate = (SWF_CandidateFn)GetProcAddress(modules[i], SWF_CANDIDATE_EXPORT);
		const SWF_CandidateInfo* info = candidate ? candidate() : NULL;
		if (info && info->size >= sizeof(SWF_CandidateInfo) && info->abiMajor == SWF_ABI_MAJOR && info->GetState && info->GetApi &&
			info->GetState() == SWF_STATE_ACTIVE)
			provider = info;
	}

	if (modules != stackModules)
		HeapFree(GetProcessHeap(), 0, modules);
	return provider;
}

/* The provider's API, or null if there is none or it is older than `abiMinorWanted` (pass SWF_ABI_MINOR, the
 * minor this header declares, unless the client checks api->abiMinor before each newer function). */
static inline const SWF_Api* SWF_Connect(uint32_t abiMinorWanted)
{
	const SWF_CandidateInfo* provider = SWF_FindProvider();
	return provider ? provider->GetApi(abiMinorWanted) : NULL;
}

/* Formats a packed implVersion as "major.minor.patch.build" into `out` (truncated to `size`). Returns `out`. */
static inline char* SWF_FormatVersion(uint64_t version, char* out, size_t size)
{
	if (out && size)
		snprintf(out, size, "%u.%u.%u.%u", (unsigned)((version >> 48) & 0xFFFF), (unsigned)((version >> 32) & 0xFFFF),
			(unsigned)((version >> 16) & 0xFFFF), (unsigned)(version & 0xFFFF));
	return out;
}

#ifdef __cplusplus
}
#endif
