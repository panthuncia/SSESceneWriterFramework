#include "tables/Tables.h"

#include "core/Log.h"
#include "swf/swf.h"
#include "tables/AddressLibrary.h"

namespace swf::inline SWF_NAMESPACE::tables
{
	namespace
	{
		const Runtime* active = nullptr;
		HookTargets targets{};

		std::uint32_t At(const AddressLibrary& library, std::uint64_t id) { return library.Offset(id); }

		hooking::Vtable At(const AddressLibrary& library, const VtableId& vtable) { return { library.Offset(vtable.id), vtable.rtti }; }

		// A site whose function the database does not resolve stays 0 (and its family does not verify), not `at`.
		std::uint32_t At(const AddressLibrary& library, const SiteId& site)
		{
			const std::uint32_t function = library.Offset(site.id);
			return function ? function + site.at : 0;
		}

		template <class T, std::size_t N>
		auto At(const AddressLibrary& library, const std::array<T, N>& ids)
		{
			std::array<decltype(At(library, ids[0])), N> out{};
			for (std::size_t i = 0; i < N; ++i)
				out[i] = At(library, ids[i]);
			return out;
		}

		HookTargets Resolve(const HookIds& ids, const AddressLibrary& library)
		{
			HookTargets t{
				.niNode = At(library, ids.niNode),
				.nodeOverrides = At(library, ids.nodeOverrides),
				.removeAllChildren = At(library, ids.removeAllChildren),
				.niSwitchNode = At(library, ids.niSwitchNode),
				.animationGraphPlace = At(library, ids.animationGraphPlace),
				.reference3DPosition = At(library, ids.reference3DPosition),
				.graphAnimation = At(library, ids.graphAnimation),
				.projectileUpdate3D = At(library, ids.projectileUpdate3D),
				.selectiveUpdateCalls = At(library, ids.selectiveUpdateCalls),
				.selectiveUpdateBytes = ids.selectiveUpdateBytes,
				.functions = At(library, ids.functions),
				.fadeNode = At(library, ids.fadeNode),
				.lightingProperty = At(library, ids.lightingProperty),
				.floatController = At(library, ids.floatController),
				.colorController = At(library, ids.colorController),
				.lightingMaterials = At(library, ids.lightingMaterials),
				.callSites = {},
				.actors = At(library, ids.actors),
				.decalArrayEdits = At(library, ids.decalArrayEdits),
				.instanceShape = At(library, ids.instanceShape),
				.effectFloatController = At(library, ids.effectFloatController),
				.effectColorController = At(library, ids.effectColorController),
				.effectMaterial = At(library, ids.effectMaterial),
				.scene = ids.scene,
			};
			for (std::size_t hook = 0; hook < SWF_HOOK_COUNT; ++hook) {
				t.callSites[hook].count = ids.callSites[hook].count;
				for (std::uint32_t i = 0; i < ids.callSites[hook].count; ++i)
					t.callSites[hook].offsets[i] = At(library, ids.callSites[hook].sites[i]);
			}
			// An id the database does not have leaves a 0, which only its family's verify reports: name them here.
			auto missing = [&](std::uint64_t id, const char* what) {
				if (id && library.Loaded() && !library.Offset(id))
					log::Warn("Address Library has no id {} ({}); the family using it will not verify", id, what);
			};
			for (std::size_t hook = 0; hook < SWF_HOOK_COUNT; ++hook) {
				missing(ids.functions[hook], "a hooked function");
				for (std::uint32_t i = 0; i < ids.callSites[hook].count; ++i)
					missing(ids.callSites[hook].sites[i].id, "a call site's function");
			}
			return t;
		}

		struct Tables
		{
			const char* name;
			const HookIds& (*ids)();
			std::span<const HiddenStoreSite> (*hiddenStores)();
			std::span<const std::uint32_t> (*showSites)();
			std::span<const StoreValueSite> (*switchStores)();
			std::span<const StoreValueSite> (*fadeResetStores)();
		};

		const Tables* TablesFor(std::uint64_t runtime)
		{
			static constexpr Tables kAe = { "AE 1.6.1170", &ae_1_6_1170::Ids, &ae_1_6_1170::HiddenStores, &ae_1_6_1170::ShowSites,
				&ae_1_6_1170::SwitchStores, &ae_1_6_1170::FadeResetStores };
			static constexpr Tables kAe17 = { "AE 1.7.104", &ae_1_7_104::Ids, &ae_1_7_104::HiddenStores, &ae_1_7_104::ShowSites,
				&ae_1_7_104::SwitchStores, &ae_1_7_104::FadeResetStores };
			static constexpr Tables kSe = { "SE 1.5.97", &se_1_5_97::Ids, &se_1_5_97::HiddenStores, &se_1_5_97::ShowSites,
				&se_1_5_97::SwitchStores, &se_1_5_97::FadeResetStores };
			switch (runtime) {
			case SWF_RUNTIME_AE_1_6_1170:
				return &kAe;
			case SWF_RUNTIME_AE_1_7_104:
				return &kAe17;
			case SWF_RUNTIME_SE_1_5_97:
				return &kSe;
			default:
				return nullptr;
			}
		}
	}

	bool Select(std::uint64_t runtime, const std::wstring& addressLibrary)
	{
		const Tables* tables = TablesFor(runtime);
		if (!tables)
			return false;
		// Without the database every id resolves to 0: the detour families do not verify, the store families (offsets) still do.
		AddressLibrary library;
		if (!library.Load(addressLibrary.empty() ? AddressLibraryPath(runtime) : addressLibrary))
			log::Warn("{}: no Address Library database, so no detour family can install", tables->name);
		targets = Resolve(tables->ids(), library);
		static Runtime runtimeTables;
		runtimeTables = { tables->name, &targets, tables->hiddenStores(), tables->showSites(), tables->switchStores(), tables->fadeResetStores() };
		active = &runtimeTables;
		return true;
	}

	const Runtime& Active()
	{
		return *active;
	}
}
