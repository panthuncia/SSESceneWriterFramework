// SWF_Api::Write: a participating plugin's scene write, stored by SWF between the PRE and POST of SWF_EVENT_SCENE_WRITE.
#include "core/Writes.h"

#include "core/Dispatch.h"
#include "tables/Tables.h"

#include <Windows.h>

#include <intrin.h>

namespace swf::inline SWF_NAMESPACE::writes
{
	namespace
	{
		const SceneLayout& Layout()
		{
			return tables::Active().targets->scene;
		}

		template <class T>
		T& Field(void* a_object, std::uint32_t a_offset)
		{
			return *reinterpret_cast<T*>(static_cast<std::byte*>(a_object) + a_offset);
		}

		void AddRef(std::uint64_t a_object)
		{
			if (a_object)
				_InterlockedIncrement(&Field<volatile long>(reinterpret_cast<void*>(a_object), Layout().refCount));
		}

		// NiRefObject::DecRefCount: DeleteThis (a vtable slot) when the count reaches 0.
		void Release(std::uint64_t a_object)
		{
			if (!a_object)
				return;
			auto* object = reinterpret_cast<void*>(a_object);
			if (_InterlockedDecrement(&Field<volatile long>(object, Layout().refCount)) == 0) {
				using DeleteThis = void (*)(void*);
				const auto* vtable = *static_cast<DeleteThis* const*>(object);
				vtable[Layout().deleteThisSlot](object);
			}
		}
	}

	SWF_Result Write(uint32_t kind, void* object, uint64_t value)
	{
		if (!object)
			return SWF_E_INVALID;
		const SceneLayout& layout = Layout();
		SWF_SceneWriteEvent event{};
		event.header.size = sizeof(event);
		event.header.id = SWF_EVENT_SCENE_WRITE;
		event.kind = kind;
		event.object = object;
		event.after = value;
		switch (kind) {
		case SWF_WRITE_HIDDEN: {
			if (value > 1)
				return SWF_E_INVALID;
			auto& flags = Field<std::uint32_t>(object, layout.flags);
			event.before = flags & 1u;
			if (event.before == value)
				return SWF_OK;
			dispatch::Call call(SWF_EVENT_SCENE_WRITE);
			call.Pre(event.header);
			flags = (flags & ~1u) | static_cast<std::uint32_t>(value);
			call.Post(event.header);
			return SWF_OK;
		}
		case SWF_WRITE_SHADER_PROPERTY:
		case SWF_WRITE_ALPHA_PROPERTY: {
			auto& field = Field<std::uint64_t>(object, kind == SWF_WRITE_SHADER_PROPERTY ? layout.shaderProperty : layout.alphaProperty);
			event.before = field;
			if (event.before == value)
				return SWF_OK;
			AddRef(value);
			{
				dispatch::Call call(SWF_EVENT_SCENE_WRITE);
				call.Pre(event.header);
				field = value;
				call.Post(event.header);
			}
			Release(event.before);
			return SWF_OK;
		}
		case SWF_WRITE_ALPHA_FLAGS: {
			if (value > 0xFFFF)
				return SWF_E_INVALID;
			auto& flags = Field<std::uint16_t>(object, layout.alphaFlags);
			event.before = flags;
			if (event.before == value)
				return SWF_OK;
			dispatch::Call call(SWF_EVENT_SCENE_WRITE);
			call.Pre(event.header);
			flags = static_cast<std::uint16_t>(value);
			call.Post(event.header);
			return SWF_OK;
		}
		default:
			return SWF_E_UNSUPPORTED;
		}
	}
}
