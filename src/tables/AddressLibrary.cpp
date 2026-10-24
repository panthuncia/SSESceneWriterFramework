#include "tables/AddressLibrary.h"

#include "core/Log.h"
#include "swf/swf_client.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace swf::inline SWF_NAMESPACE::tables
{
	namespace
	{
		class Reader
		{
		public:
			explicit Reader(const std::vector<char>& a_bytes) : bytes(a_bytes) {}

			template <class T>
			bool Read(T& value)
			{
				if (bytes.size() - position < sizeof(T))
					return false;
				std::memcpy(&value, bytes.data() + position, sizeof(T));
				position += sizeof(T);
				return true;
			}

			bool Skip(std::size_t count)
			{
				if (bytes.size() - position < count)
					return false;
				position += count;
				return true;
			}

			std::size_t Position() const { return position; }

		private:
			const std::vector<char>& bytes;
			std::size_t position = 0;
		};

		// Formats 1 and 2: each entry a type byte, then the id and the offset as deltas from the previous entry's (or in full).
		bool Unpack(Reader& in, std::uint32_t pointerSize, std::uint32_t count, std::vector<std::uint32_t>& offsets)
		{
			std::uint64_t id = 0, offset = 0, previousId = 0, previousOffset = 0;
			for (std::uint32_t i = 0; i < count; ++i) {
				std::uint8_t type = 0;
				if (!in.Read(type))
					return false;
				auto delta = [&](std::uint8_t code, std::uint64_t base, std::uint64_t& value) {
					std::uint8_t b = 0;
					std::uint16_t w = 0;
					std::uint32_t d = 0;
					switch (code) {
					case 0: return in.Read(value);
					case 1: value = base + 1; return true;
					case 2: return in.Read(b) && ((value = base + b), true);
					case 3: return in.Read(b) && ((value = base - b), true);
					case 4: return in.Read(w) && ((value = base + w), true);
					case 5: return in.Read(w) && ((value = base - w), true);
					case 6: return in.Read(w) && ((value = w), true);
					default: return in.Read(d) && ((value = d), true);
					}
				};
				const std::uint8_t high = type >> 4;
				const std::uint64_t base = (high & 8) ? previousOffset / pointerSize : previousOffset;
				if (!delta(type & 0xF, previousId, id) || !delta(high & 7, base, offset))
					return false;
				if (high & 8)
					offset *= pointerSize;
				if (id >= offsets.size())
					offsets.resize(static_cast<std::size_t>(id) + 1);
				offsets[static_cast<std::size_t>(id)] = static_cast<std::uint32_t>(offset);
				previousId = id;
				previousOffset = offset;
			}
			return true;
		}
	}

	bool AddressLibrary::Load(const std::wstring& path)
	{
		offsets.clear();
		std::ifstream file(path, std::ios::binary);
		const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
		Reader in(bytes);
		std::int32_t format = 0;
		std::uint32_t version[4]{}, pointerSize = 0, count = 0;
		bool ok = in.Read(format) && in.Read(version);
		if (ok && (format == 1 || format == 2)) {
			std::uint32_t nameLength = 0;
			ok = in.Read(nameLength) && in.Skip(nameLength) && in.Read(pointerSize) && in.Read(count) && pointerSize &&
			     Unpack(in, pointerSize, count, offsets);
		} else if (ok && format == 5) {
			// A fixed 64-byte name, the pointer size, a data-format word, then a dense u32 offset per id.
			std::uint32_t dataFormat = 0;
			ok = in.Skip(64) && in.Read(pointerSize) && in.Read(dataFormat) && in.Read(count) &&
			     bytes.size() - in.Position() >= std::size_t(count) * sizeof(std::uint32_t);
			if (ok) {
				offsets.resize(count);
				std::memcpy(offsets.data(), bytes.data() + in.Position(), std::size_t(count) * sizeof(std::uint32_t));
			}
		} else
			ok = false;
		if (!ok || offsets.empty()) {
			offsets.clear();
			std::string narrow;
			for (const wchar_t c : path)
				narrow += c < 0x80 ? static_cast<char>(c) : '?';
			log::Warn("Address Library database {} is missing, malformed or of an unknown format ({})", narrow, format);
			return false;
		}
		return true;
	}

	std::uint32_t AddressLibrary::Offset(std::uint64_t id) const
	{
		return id < offsets.size() ? offsets[static_cast<std::size_t>(id)] : 0;
	}

	std::wstring AddressLibraryPath(std::uint64_t runtime)
	{
		const wchar_t* file = runtime == SWF_RUNTIME_SE_1_5_97     ? L"version-1-5-97-0.bin" :
		                      runtime == SWF_RUNTIME_AE_1_6_1170   ? L"versionlib-1-6-1170-0.bin" :
		                      runtime == SWF_RUNTIME_AE_1_7_104    ? L"versionlib-1-7-104-0.bin" :
		                                                             nullptr;
		return file ? client::detail::ExeDirectory() + L"Data\\SKSE\\Plugins\\" + file : std::wstring();
	}
}
