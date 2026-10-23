// Moves: the engine writers that move references and nodes, as SWF_EVENT_MOVED after each.
#include "families/DetourFamilies.h"

#include "core/Dispatch.h"
#include "core/Log.h"
#include "hooking/Hooks.h"

namespace swf::inline SWF_NAMESPACE::detours
{
	namespace
	{
		constexpr std::size_t kUserData = 0xF8;                     // NiAVObject::userData (SE/AE): its TESObjectREFR*
		constexpr std::size_t kUpdateSelectedDownwardPass = 0x2D;  // NiAVObject's slot

		void Moved(const void* key, std::uint32_t writer)
		{
			if (!*dispatch::Events().CountWord(SWF_EVENT_MOVED))
				return;
			SWF_MovedEvent event{};
			event.header.size = sizeof(event);
			event.header.id = SWF_EVENT_MOVED;
			event.key = key;
			event.writer = writer;
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

		// The patched `call [rax + 0x168]`: the node's UpdateSelectedDownwardPass, then its reference
		// (or the node itself) as the key.
		void SelectiveUpdate(void* node, void* data, std::uint32_t arg)
		{
			using Update = void (*)(void*, void*, std::uint32_t);
			(*reinterpret_cast<Update* const*>(node))[kUpdateSelectedDownwardPass](node, data, arg);
			const void* reference = *reinterpret_cast<void* const*>(static_cast<std::byte*>(node) + kUserData);
			Moved(reference ? reference : node, SWF_MOVE_SELECTIVE_UPDATE);
		}

		std::vector<hooking::Detour> MoveDetours(const hooking::Image& image, const HookTargets& targets)
		{
			animationGraphPlace = reinterpret_cast<decltype(animationGraphPlace)>(image.base + targets.animationGraphPlace);
			reference3DPosition = reinterpret_cast<decltype(reference3DPosition)>(image.base + targets.reference3DPosition);
			graphAnimation = reinterpret_cast<decltype(graphAnimation)>(image.base + targets.graphAnimation);
			return {
				{ reinterpret_cast<void**>(&animationGraphPlace), reinterpret_cast<void*>(&AnimationGraphPlace) },
				{ reinterpret_cast<void**>(&reference3DPosition), reinterpret_cast<void*>(&Reference3DPosition) },
				{ reinterpret_cast<void**>(&graphAnimation), reinterpret_cast<void*>(&GraphAnimation) },
			};
		}
	}

	bool VerifyMoves(const hooking::Image& image, const HookTargets& targets)
	{
		for (const std::uint32_t site : targets.selectiveUpdateCalls) {
			if (!hooking::BytesMatch(image, site, targets.selectiveUpdateBytes.data(), targets.selectiveUpdateBytes.size())) {
				log::Warn("moves: the cells' update pass at +{:#x} is not the expected call", site);
				return false;
			}
		}
		for (const std::uint32_t offset : { targets.animationGraphPlace, targets.reference3DPosition, targets.graphAnimation })
			if (!hooking::InImage(image, image.base + offset))
				return false;
		return true;
	}

	bool InstallMoves(const hooking::Image& image, const HookTargets& targets)
	{
		if (!hooking::AttachAll(MoveDetours(image, targets), "moves"))
			return false;
		if (!hooking::PatchCallSites(image, targets.selectiveUpdateCalls, static_cast<std::uint8_t>(targets.selectiveUpdateBytes.size()),
				reinterpret_cast<void*>(&SelectiveUpdate))) {
			log::Error("moves: the writers are detoured but the cells' update passes could not be patched");
			return false;
		}
		log::Info("moves: 3 writers detoured, {} update passes patched", targets.selectiveUpdateCalls.size());
		return true;
	}
}
