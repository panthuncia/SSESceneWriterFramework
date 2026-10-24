// AE 1.7.104 detour targets: AE's Address Library ids (Ae_1_6_1170_Hooks.cpp), which versionlib-1-7-104-0 resolves for every
// function and vtable SWF hooks, with the call sites 1.7.104 moved. Found by a scan of every `call rel32` in 1.7.104's .text
// to each site-hooked function: the same callers as on 1.6.1170, except
//  - the active and large-reference grids' segment updates (1.6.1170's ids 31846 and 31847) were renumbered 523588 and 523589;
//  - 523589 calls the segment hide twice (as SE's FUN_1404b5220 does: the compiler duplicated the hide branch).
// The tree LOD refill's callers and the cells' selective update calls kept their positions.
#include "tables/HookTargets.h"

namespace swf::inline SWF_NAMESPACE::tables::ae_1_7_104
{
	const HookIds& Ids()
	{
		static const HookIds ids = [] {
			HookIds i = ae_1_6_1170::Ids();
			i.callSites[SWF_HOOK_LOD_SEGMENT_SHOW] = { { { { 523589, 0x28c }, { 523588, 0x1d0 } } }, 2 };
			i.callSites[SWF_HOOK_LOD_SEGMENT_HIDE] = { { { { 523589, 0x242 }, { 523589, 0x280 }, { 523588, 0x1c3 } } }, 3 };
			i.callSites[SWF_HOOK_LOD_SEGMENT_SHOW_ALL] = {
				{ { { 31587, 0x190 }, { 31737, 0x141 }, { 523589, 0x110 }, { 523588, 0x222 }, { 77492, 0x413 } } }, 5
			};
			return i;
		}();
		return ids;
	}
}
