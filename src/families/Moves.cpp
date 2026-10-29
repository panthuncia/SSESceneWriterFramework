// Moves: the engine writers that move references and nodes, as SWF_EVENT_MOVED after each.
#include "families/DetourFamilies.h"

#include "core/Dispatch.h"
#include "core/Log.h"
#include "hooking/Hooks.h"
#include "hooking/Integrity.h"

#include <cstring>
#include <iterator>
#include <tuple>
#include <vector>

namespace swf::inline SWF_NAMESPACE::detours
{
	namespace
	{
		constexpr std::size_t kUserData = 0xF8;                     // NiAVObject::userData (SE/AE): its TESObjectREFR*
		constexpr std::size_t kUpdateSelectedDownwardPass = 0x2D;  // NiAVObject's slot

		void Moved(const void* key, std::uint32_t writer, std::uint32_t keyKind = SWF_MOVED_KEY_REFERENCE)
		{
			if (!*dispatch::Events().CountWord(SWF_EVENT_MOVED))
				return;
			SWF_MovedEvent event{};
			event.header.size = sizeof(event);
			event.header.id = SWF_EVENT_MOVED;
			event.key = key;
			event.writer = writer;
			event.keyKind = keyKind;
			dispatch::Fire(event.header);
		}

		bool (*animationGraphPlace)(void*) = nullptr;
		bool AnimationGraphPlace(void* reference)
		{
			const bool result = animationGraphPlace(reference);
			Moved(reference, SWF_MOVE_ANIMATION_GRAPH_PLACE);
			return result;
		}

		void (*reference3DPosition)(void*, bool) = nullptr;
		void Reference3DPosition(void* reference, bool warp)
		{
			reference3DPosition(reference, warp);
			Moved(reference, SWF_MOVE_REFERENCE_3D_POSITION);
		}

		void (*graphAnimation)(void*) = nullptr;
		void GraphAnimation(void* reference)
		{
			graphAnimation(reference);
			Moved(reference, SWF_MOVE_GRAPH_ANIMATION);
		}

		// Every projectile's world update, each frame (Projectile::Update3D: its 3D's NiAVObject::Update, after UpdateImpl placed it).
		void (*projectileUpdate3D)(void*) = nullptr;
		void ProjectileUpdate3D(void* projectile)
		{
			projectileUpdate3D(projectile);
			Moved(projectile, SWF_MOVE_PROJECTILE_3D);
		}

		using Update = void (*)(void*, void*, std::uint32_t);

		// The patched `call [rax + 0x168]` at site N: what the site called (the node's UpdateSelectedDownwardPass, or another plugin's
		// hook there), then its reference (or the node itself) as the key.
		template <std::size_t N>
		struct SelectiveUpdate
		{
			static inline Update original = nullptr;  // null: the node's slot, as the engine's call

			static void Thunk(void* node, void* data, std::uint32_t arg)
			{
				if (original)
					original(node, data, arg);
				else
					(*reinterpret_cast<Update* const*>(node))[kUpdateSelectedDownwardPass](node, data, arg);
				const void* reference = *reinterpret_cast<void* const*>(static_cast<std::byte*>(node) + kUserData);
				if (reference)
					Moved(reference, SWF_MOVE_SELECTIVE_UPDATE);
				else
					Moved(node, SWF_MOVE_SELECTIVE_UPDATE, SWF_MOVED_KEY_NODE);
			}
		};
		void* const kSelectiveThunks[] = { reinterpret_cast<void*>(&SelectiveUpdate<0>::Thunk), reinterpret_cast<void*>(&SelectiveUpdate<1>::Thunk) };
		Update* const kSelectiveOriginals[] = { &SelectiveUpdate<0>::original, &SelectiveUpdate<1>::original };

		/**
		 * What a selective-update site calls now: the engine's `call [rax + 0x168]` (its bytes; `original` stays null), or another
		 * plugin's rewrite of it, chained: `call [rip + rel32]` (write_call<6>'s form: the address it holds) or `call rel32` then a NOP,
		 * either to code outside the image. False for anything else.
		 */
		bool SiteOriginal(const hooking::Image& image, const HookTargets& targets, std::uint32_t site, Update& original)
		{
			original = nullptr;
			if (hooking::BytesMatch(image, site, targets.selectiveUpdateBytes.data(), targets.selectiveUpdateBytes.size()))
				return true;
			std::uintptr_t target = 0;
			if (site && static_cast<std::size_t>(site) + 6 <= image.size) {
				const auto* code = reinterpret_cast<const std::uint8_t*>(image.base + site);
				if (code[0] == 0xFF && code[1] == 0x15) {
					std::int32_t displacement = 0;
					std::memcpy(&displacement, code + 2, sizeof(displacement));
					const std::uintptr_t slot = image.base + site + 6 + displacement;
					if (hooking::Readable(slot, sizeof(std::uintptr_t)))
						target = *reinterpret_cast<const std::uintptr_t*>(slot);
				} else if (code[5] == 0x90) {
					target = hooking::CallTarget(image, site);
				}
			}
			if (!target || hooking::InImage(image, target) || !hooking::Executable(target)) {
				log::Warn("moves: the cells' update pass at +{:#x} is not the expected call", site);
				return false;
			}
			log::Info("moves: the cells' update pass at +{:#x} was rewritten by {}; SWF's hook calls on to it", site, hooking::LeadsInto(target));
			original = reinterpret_cast<Update>(target);
			return true;
		}

		std::vector<hooking::Detour> MoveDetours(const hooking::Image& image, const HookTargets& targets)
		{
			animationGraphPlace = reinterpret_cast<decltype(animationGraphPlace)>(hooking::At(image, targets.animationGraphPlace));
			reference3DPosition = reinterpret_cast<decltype(reference3DPosition)>(hooking::At(image, targets.reference3DPosition));
			graphAnimation = reinterpret_cast<decltype(graphAnimation)>(hooking::At(image, targets.graphAnimation));
			projectileUpdate3D = reinterpret_cast<decltype(projectileUpdate3D)>(hooking::At(image, targets.projectileUpdate3D));
			return {
				{ reinterpret_cast<void**>(&animationGraphPlace), reinterpret_cast<void*>(&AnimationGraphPlace) },
				{ reinterpret_cast<void**>(&reference3DPosition), reinterpret_cast<void*>(&Reference3DPosition) },
				{ reinterpret_cast<void**>(&graphAnimation), reinterpret_cast<void*>(&GraphAnimation) },
				{ reinterpret_cast<void**>(&projectileUpdate3D), reinterpret_cast<void*>(&ProjectileUpdate3D) },
			};
		}
	}

	bool VerifyMoves(const hooking::Image& image, const HookTargets& targets)
	{
		for (const std::uint32_t site : targets.selectiveUpdateCalls) {
			Update original = nullptr;
			if (!SiteOriginal(image, targets, site, original))
				return false;
		}
		for (const std::uint32_t offset : { targets.animationGraphPlace, targets.reference3DPosition, targets.graphAnimation, targets.projectileUpdate3D })
			if (!hooking::InImage(image, hooking::At(image, offset)))
				return false;
		return true;
	}

	bool InstallMoves(const hooking::Image& image, const HookTargets& targets)
	{
		// The update passes' sites take write_call<6>'s form (`call [rip + rel32]`, the instruction's six bytes), so a hook written
		// there after SWF's keeps SWF's as the call it takes. Their stubs are placed before the writers are detoured and written
		// after: a failure of either installs nothing.
		static_assert(std::size(kSelectiveThunks) == std::tuple_size_v<decltype(targets.selectiveUpdateCalls)>);
		std::vector<hooking::SitePatch> sites;
		for (std::size_t i = 0; i < targets.selectiveUpdateCalls.size(); ++i) {
			if (!SiteOriginal(image, targets, targets.selectiveUpdateCalls[i], *kSelectiveOriginals[i]))
				return false;
			sites.push_back({ targets.selectiveUpdateCalls[i], static_cast<std::uint8_t>(targets.selectiveUpdateBytes.size()), kSelectiveThunks[i], true });
		}
		hooking::PendingCalls pending = hooking::PrepareCallSites(image, sites);
		if (!pending) {
			log::Warn("moves: no room for the cells' update passes' stubs");
			return false;
		}
		if (!hooking::AttachAll(MoveDetours(image, targets), "moves"))
			return false;
		pending.Commit();
		log::Info("moves: 4 writers detoured, {} update passes patched", targets.selectiveUpdateCalls.size());
		return true;
	}
}
