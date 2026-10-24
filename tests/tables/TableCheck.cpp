// swf_table_check <SkyrimSE.exe> <se|ae|ae17> <Address Library .bin>: every family's verify pass, run against a clean copy of a
// game executable (an unpacked one: Steam's DRM stub hides the code), its hook targets resolved through that version's
// database. The image is mapped at its preferred base, as the game runs it,
// so vtable slots and call targets hold the addresses the provider would see. Not a ctest: it needs the game's exe.
#include "core/Log.h"
#include "families/DetourFamilies.h"
#include "families/Stores.h"
#include "swf/swf_client.h"
#include "tables/Tables.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	using namespace swf;

	void Print(std::int32_t level, const char* message)
	{
		std::printf("    %s%s\n", level >= 2 ? "warn: " : "", message);
	}

	// The executable's headers and sections at its preferred base (0x140000000 for Skyrim; this tool links elsewhere).
	bool Map(const char* path, hooking::Image& image)
	{
		std::ifstream file(path, std::ios::binary);
		const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
		if (bytes.size() < sizeof(IMAGE_DOS_HEADER))
			return false;
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data() + dos->e_lfanew);
		const auto base = reinterpret_cast<std::uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(nt->OptionalHeader.ImageBase),
			nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (!base)
			return false;
		std::memcpy(base, bytes.data(), nt->OptionalHeader.SizeOfHeaders);
		const auto* section = IMAGE_FIRST_SECTION(nt);
		for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
			const std::size_t size = (std::min)(section->SizeOfRawData, section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData);
			if (section->PointerToRawData + size <= bytes.size())
				std::memcpy(base + section->VirtualAddress, bytes.data() + section->PointerToRawData, size);
		}
		image = { reinterpret_cast<std::uintptr_t>(base), nt->OptionalHeader.SizeOfImage };
		return true;
	}
}

int main(int argc, char** argv)
{
	if (argc != 4) {
		std::printf("usage: swf_table_check <SkyrimSE.exe.unpacked.exe> <se|ae|ae17> <version(lib)-*.bin>\n");
		return 2;
	}
	log::SetSink(&Print);
	const std::string runtime = argv[2];
	const std::uint64_t bit = runtime == "se" ? SWF_RUNTIME_SE_1_5_97 : runtime == "ae" ? SWF_RUNTIME_AE_1_6_1170 : runtime == "ae17" ? SWF_RUNTIME_AE_1_7_104 : 0;
	if (!tables::Select(bit, std::wstring(argv[3], argv[3] + std::strlen(argv[3])))) {
		std::printf("unknown runtime '%s'\n", argv[2]);
		return 2;
	}
	hooking::Image image;
	if (!Map(argv[1], image)) {
		std::printf("could not map %s at its preferred base\n", argv[1]);
		return 2;
	}

	const tables::Runtime& table = tables::Active();
	const HookTargets& targets = *table.targets;
	struct Check
	{
		const char* name;
		bool (*verify)(const hooking::Image&, const tables::Runtime&);
	};
	static constexpr Check kChecks[] = {
		{ "hidden", [](const hooking::Image& i, const tables::Runtime& t) { return stores::VerifyHidden(i, t.hiddenStores); } },
		{ "switch-index", [](const hooking::Image& i, const tables::Runtime& t) { return stores::VerifyValues(i, t.switchStores); } },
		{ "fade-reset", [](const hooking::Image& i, const tables::Runtime& t) { return stores::VerifyValues(i, t.fadeResetStores); } },
		{ "structure", [](const hooking::Image& i, const tables::Runtime& t) { return detours::VerifyStructure(i, *t.targets); } },
		{ "switch-structural", [](const hooking::Image& i, const tables::Runtime& t) { return detours::VerifySwitchStructural(i, *t.targets); } },
		{ "moves", [](const hooking::Image& i, const tables::Runtime& t) { return detours::VerifyMoves(i, *t.targets); } },
	};
	static constexpr std::pair<SWF_Caps, const char*> kCalls[] = { { SWF_CAP_FADE_CALLS, "fade-calls" }, { SWF_CAP_NODE_CALLS, "node-calls" },
		{ SWF_CAP_PROPERTY_CALLS, "property-calls" }, { SWF_CAP_GEOMETRY_CALLS, "geometry-calls" }, { SWF_CAP_MATERIAL_CALLS, "material-calls" },
		{ SWF_CAP_ACTOR_CALLS, "actor-calls" }, { SWF_CAP_DECAL_CALLS, "decal-calls" }, { SWF_CAP_LOD_SEGMENT_CALLS, "lod-segment-calls" },
		{ SWF_CAP_TREE_LOD_CALLS, "tree-lod-calls" }, { SWF_CAP_TREE_ANIM_CALLS, "tree-anim-calls" }, { SWF_CAP_EFFECT_CALLS, "effect-calls" } };

	const std::wstring widePath(argv[1], argv[1] + std::strlen(argv[1]));
	std::printf("%s tables against %s (its version reads as runtime %#llx)\n", table.name, argv[1],
		static_cast<unsigned long long>(client::RuntimeOf(widePath.c_str())));
	int failed = 0;
	auto report = [&](const char* name, bool ok) {
		std::printf("  %-18s %s\n", name, ok ? "verifies" : "DOES NOT VERIFY");
		failed += ok ? 0 : 1;
	};
	for (const Check& check : kChecks)
		report(check.name, check.verify(image, table));
	for (const auto& [family, name] : kCalls)
		report(name, detours::VerifyCalls(family, image, targets));
	std::printf("%d of %zu families do not verify\n", failed, std::size(kChecks) + std::size(kCalls));
	return failed ? 1 : 0;
}
