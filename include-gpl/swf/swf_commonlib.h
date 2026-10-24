// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Panthuncia. GPL-3.0-or-later with the exceptions in EXCEPTIONS.md.
//
// SceneWriterFramework for CommonLibSSE-NG plugins: swf.hpp's API with the engine objects as
// RE:: types. This header is under the GPL; a plugin under another license uses swf.hpp.
//
//   const swf::clib::Api api = swf::clib::Api::Connect();  // at kPostPostLoad or later
//   api.On<SWF_EVENT_CHILD_ATTACHED>(SWF_PHASE_POST, 0, [](const swf::clib::ChildEvent& event) {
//       RE::NiNode* parent = event.Parent();
//       RE::NiAVObject* child = event.Child();
//   });
//   api.SetHidden(object, true);
#pragma once

#include "swf/swf.hpp"

#include <RE/Skyrim.h>

namespace swf::clib
{
	/// A typed view of one dispatch's event; valid during the callback.
	template <class T>
	class EventView
	{
	public:
		explicit EventView(const T& raw) :
			raw(raw) {}

		/// The event struct, as swf.h declares it.
		const T& Raw() const { return raw; }
		const SWF_EventHeader& Header() const { return raw.header; }
		bool Pre() const { return raw.header.phase == SWF_PHASE_PRE; }
		bool Post() const { return raw.header.phase == SWF_PHASE_POST; }
		uint32_t ThreadId() const { return raw.header.threadId; }
		uint64_t Sequence() const { return raw.header.sequence; }

	protected:
		const T& raw;
	};

	/// SWF_EVENT_WRITTEN: a plugin reported a direct write. The object is an NiAVObject, a shader
	/// property or a material, as the writer reported it.
	class WrittenEvent : public EventView<SWF_WrittenEvent>
	{
	public:
		using EventView::EventView;
		template <class T = RE::NiAVObject>
		T* Object() const { return static_cast<T*>(raw.object); }
		SWF_Fields Fields() const { return raw.fields; }
	};

	/// SWF_EVENT_HIDDEN: an engine store changed the hidden bit.
	class HiddenEvent : public EventView<SWF_HiddenEvent>
	{
	public:
		using EventView::EventView;
		RE::NiAVObject* Object() const { return static_cast<RE::NiAVObject*>(raw.object); }
		bool Hidden() const { return raw.hidden != 0; }
		uint32_t Site() const { return raw.site; }
		uint32_t SiteOffset() const { return raw.siteOffset; }
	};

	/// SWF_EVENT_SWITCH_INDEX: an engine store changed a switch node's index.
	class SwitchIndexEvent : public EventView<SWF_SwitchIndexEvent>
	{
	public:
		using EventView::EventView;
		RE::NiSwitchNode* Node() const { return static_cast<RE::NiSwitchNode*>(raw.node); }
		int32_t Before() const { return raw.before; }
		int32_t After() const { return raw.after; }
		uint32_t Site() const { return raw.site; }
		uint32_t SiteOffset() const { return raw.siteOffset; }
	};

	/// SWF_EVENT_FADE_RESET: an engine store reset a fade node's current fade.
	class FadeResetEvent : public EventView<SWF_FadeResetEvent>
	{
	public:
		using EventView::EventView;
		RE::BSFadeNode* Node() const { return static_cast<RE::BSFadeNode*>(raw.node); }
		float Value() const { return raw.value; }
		uint32_t Site() const { return raw.site; }
		uint32_t SiteOffset() const { return raw.siteOffset; }
	};

	/// SWF_FILTER_SHOW: an engine LOD store about to show an object.
	class ShowFilterEvent : public EventView<SWF_ShowFilterEvent>
	{
	public:
		using EventView::EventView;
		RE::NiAVObject* Object() const { return static_cast<RE::NiAVObject*>(raw.object); }
		uint32_t Site() const { return raw.site; }
		uint32_t SiteOffset() const { return raw.siteOffset; }
	};

	/// SWF_EVENT_CHILD_ATTACHED and SWF_EVENT_CHILD_DETACHING.
	class ChildEvent : public EventView<SWF_ChildEvent>
	{
	public:
		using EventView::EventView;
		RE::NiNode* Parent() const { return static_cast<RE::NiNode*>(raw.parent); }
		/// May be null: an empty slot set or detached.
		RE::NiAVObject* Child() const { return static_cast<RE::NiAVObject*>(raw.child); }
		/// CHILD_ATTACHED from SetAt: the slot already held the child.
		bool Unchanged() const { return raw.unchanged != 0; }
	};

	/// SWF_EVENT_SWITCH_STRUCTURAL: one of a switch node's own child edits.
	class SwitchStructuralEvent : public EventView<SWF_NodeEvent>
	{
	public:
		using EventView::EventView;
		RE::NiSwitchNode* Node() const { return static_cast<RE::NiSwitchNode*>(raw.node); }
	};

	/// SWF_EVENT_MOVED: a reference, or a node without one, that an engine writer moved.
	class MovedEvent : public EventView<SWF_MovedEvent>
	{
	public:
		using EventView::EventView;
		/// The reference moved, or null when the key is a node.
		RE::TESObjectREFR* Reference() const
		{
			return raw.keyKind == SWF_MOVED_KEY_REFERENCE ? static_cast<RE::TESObjectREFR*>(const_cast<void*>(raw.key)) : nullptr;
		}
		/// The node moved, when it has no reference (null otherwise).
		RE::NiAVObject* Node() const
		{
			return raw.keyKind == SWF_MOVED_KEY_NODE ? static_cast<RE::NiAVObject*>(const_cast<void*>(raw.key)) : nullptr;
		}
		const void* Key() const { return raw.key; }
		uint32_t Writer() const { return raw.writer; }
	};

	/// SWF_EVENT_SCENE_WRITE: a plugin's write through SWF, before and after the store.
	class SceneWriteEvent : public EventView<SWF_SceneWriteEvent>
	{
	public:
		using EventView::EventView;
		uint32_t Kind() const { return raw.kind; }
		/// RE::NiAVObject for SWF_WRITE_HIDDEN, RE::BSGeometry for the properties, RE::NiAlphaProperty for its flags.
		template <class T>
		T* Object() const { return static_cast<T*>(raw.object); }
		/// The field's value before and after: a pointer (RE::BSShaderProperty*, RE::NiAlphaProperty*), the bit, or the flags.
		template <class T>
		T Before() const { return As<T>(raw.before); }
		template <class T>
		T After() const { return As<T>(raw.after); }
	};

	/// SWF_EVENT_CALL(hook) and SWF_FILTER_CALL(hook): an engine call and its arguments, typed by the caller
	/// from the hook's signature in swf.h.
	class CallEvent : public EventView<SWF_CallEvent>
	{
	public:
		using EventView::EventView;
		uint32_t Hook() const { return raw.hook; }
		uint32_t ArgCount() const { return raw.argCount; }
		template <class T>
		T Arg(uint32_t index) const { return swf::Arg<T>(raw, index); }
		/// POST only.
		template <class T>
		T Result() const { return swf::Result<T>(raw); }
	};

	/// The typed view of event `Id`.
	template <uint32_t Id>
	struct TypedOf { using type = CallEvent; };
	template <>
	struct TypedOf<SWF_EVENT_WRITTEN> { using type = WrittenEvent; };
	template <>
	struct TypedOf<SWF_EVENT_HIDDEN> { using type = HiddenEvent; };
	template <>
	struct TypedOf<SWF_EVENT_SWITCH_INDEX> { using type = SwitchIndexEvent; };
	template <>
	struct TypedOf<SWF_EVENT_FADE_RESET> { using type = FadeResetEvent; };
	template <>
	struct TypedOf<SWF_EVENT_CHILD_ATTACHED> { using type = ChildEvent; };
	template <>
	struct TypedOf<SWF_EVENT_CHILD_DETACHING> { using type = ChildEvent; };
	template <>
	struct TypedOf<SWF_EVENT_SWITCH_STRUCTURAL> { using type = SwitchStructuralEvent; };
	template <>
	struct TypedOf<SWF_EVENT_MOVED> { using type = MovedEvent; };
	template <>
	struct TypedOf<SWF_EVENT_SCENE_WRITE> { using type = SceneWriteEvent; };
	template <uint32_t Id>
	using Typed = typename TypedOf<Id>::type;

	/// The typed view of filter `Id`.
	template <uint32_t Id>
	struct TypedFilterOf { using type = CallEvent; };
	template <>
	struct TypedFilterOf<SWF_FILTER_SHOW> { using type = ShowFilterEvent; };
	template <uint32_t Id>
	using TypedFilter = typename TypedFilterOf<Id>::type;

	/// swf::Api with typed events and writes. The untyped calls stay available through swf::Api.
	class Api : public swf::Api
	{
	public:
		using swf::Api::Api;

		static Api Connect(uint32_t abiMinor = SWF_ABI_MINOR) { return Api(SWF_Connect(abiMinor)); }

		/// Subscribes `fn` to event `Id`: fn(const Typed<Id>&) or fn(const Typed<Id>&, swf::Scratch).
		template <uint32_t Id, class Fn>
		SWF_Result On(uint32_t phases, int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			return SubscribeAs<Id, Typed<Id>>(phases, priority, std::forward<Fn>(fn), out);
		}

		/// Adds `fn` as filter `Id`: fn(const TypedFilter<Id>&) returns true to veto the write.
		template <uint32_t Id, class Fn>
		SWF_Result Filter(int32_t priority, Fn&& fn, SWF_Subscription* out = nullptr) const
		{
			return AddFilterAs<Id, TypedFilter<Id>>(priority, std::forward<Fn>(fn), out);
		}

		SWF_Result NotifyWritten(RE::NiAVObject* object, SWF_Fields fields) const { return swf::Api::NotifyWritten(object, fields); }
		SWF_Result NotifyWritten(RE::BSShaderProperty* property, SWF_Fields fields) const { return swf::Api::NotifyWritten(property, fields); }
		SWF_Result NotifyWritten(RE::BSShaderMaterial* material, SWF_Fields fields) const { return swf::Api::NotifyWritten(material, fields); }

		SWF_Result SetHidden(RE::NiAVObject* object, bool hidden) const { return swf::Api::SetHidden(object, hidden); }
		SWF_Result SetShaderProperty(RE::BSGeometry* geometry, RE::BSShaderProperty* property) const
		{
			return swf::Api::SetShaderProperty(geometry, property);
		}
		SWF_Result SetAlphaProperty(RE::BSGeometry* geometry, RE::NiAlphaProperty* property) const
		{
			return swf::Api::SetAlphaProperty(geometry, property);
		}
		SWF_Result SetAlphaFlags(RE::NiAlphaProperty* property, uint16_t flags) const { return swf::Api::SetAlphaFlags(property, flags); }
	};
}
