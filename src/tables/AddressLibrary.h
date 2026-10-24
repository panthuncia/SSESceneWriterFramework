#pragma once

#include "core/Version.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swf::inline SWF_NAMESPACE::tables
{
	/// An Address Library database: each id's offset from the image base. SWF reads the file itself, so it needs no CommonLib.
	class AddressLibrary
	{
	public:
		/// Reads formats 1 (SE), 2 (AE before 1.7.99) and 5 (AE 1.7.99 and later). False, with a warning logged, if the file is
		/// missing or malformed.
		bool Load(const std::wstring& path);

		/// The offset `id` names, or 0 if the database has none.
		std::uint32_t Offset(std::uint64_t id) const;

		bool Loaded() const { return !offsets.empty(); }

	private:
		std::vector<std::uint32_t> offsets;  // indexed by id
	};

	/// Where the running game keeps the database for a SWF_RUNTIME_* bit: Data/SKSE/Plugins/version-1-5-97-0.bin or
	/// versionlib-<version>-0.bin next to the game's executable. Empty for a runtime SWF has no tables for.
	std::wstring AddressLibraryPath(std::uint64_t runtime);
}
