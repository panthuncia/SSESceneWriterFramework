#include "core/Provider.h"

#include "core/Dispatch.h"
#include "core/Log.h"
#include "core/Writes.h"
#include "families/Families.h"
#include "families/Stores.h"
#include "hooking/Integrity.h"
#include "hooking/Memory.h"
#include "swf/swf_client.h"

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <string>

namespace swf::inline SWF_NAMESPACE::provider
{
	namespace
	{
		std::atomic<uint32_t> state{ SWF_STATE_IDLE };
		std::atomic<SWF_Caps> caps{ 0 };
		uint32_t candidates = 0;
		std::mutex activateMutex;
#ifdef SWF_TESTING
		bool forceFailure = false;
		uint32_t installCount = 0;
#endif

		bool Ready()
		{
			return state.load(std::memory_order_acquire) == SWF_STATE_ACTIVE;
		}

		// SWF_OK if `id` is known and its family installed.
		SWF_Result Supported(uint32_t id, bool filter)
		{
			const SWF_Caps needed = CapFor(id, filter);
			if (needed == ~0ull)
				return id == 0 || id >= dispatch::kMaxIds ? SWF_E_INVALID : SWF_E_UNSUPPORTED;
			return (caps.load() & needed) == needed ? SWF_OK : SWF_E_UNSUPPORTED;
		}

		SWF_Result GetProviderInfo(SWF_ProviderInfo* out)
		{
			if (!out || out->size < sizeof(SWF_ProviderInfo))
				return SWF_E_INVALID;
			if (!Ready())
				return SWF_E_NOT_READY;
			out->abiMajor = SWF_ABI_MAJOR;
			out->abiMinor = SWF_ABI_MINOR;
			out->candidateCount = candidates;
			out->implVersion = kImplVersion;
			out->runtimeMask = kRuntimeMask;
			out->caps = caps.load();
			out->hostName = log::HostName();
			return SWF_OK;
		}

		SWF_Caps GetCaps()
		{
			return Ready() ? caps.load() : 0;
		}

		SWF_Result Subscribe(uint32_t eventId, uint32_t phases, int32_t priority, SWF_EventFn fn, void* user, SWF_Subscription* out)
		{
			if (!Ready())
				return SWF_E_NOT_READY;
			if (!fn || !phases || (phases & ~(SWF_PHASE_PRE | SWF_PHASE_POST)))
				return SWF_E_INVALID;
			if (const SWF_Result supported = Supported(eventId, false); supported != SWF_OK)
				return supported;
			dispatch::Entry entry;
			entry.event = fn;
			entry.user = user;
			entry.priority = priority;
			entry.phases = phases;
			return dispatch::Events().Add(eventId, entry, out);
		}

		SWF_Result Unsubscribe(SWF_Subscription subscription)
		{
			return Ready() ? dispatch::Events().Remove(subscription) : SWF_E_NOT_READY;
		}

		SWF_Result AddFilter(uint32_t filterId, int32_t priority, SWF_FilterFn fn, void* user, SWF_Subscription* out)
		{
			if (!Ready())
				return SWF_E_NOT_READY;
			if (!fn)
				return SWF_E_INVALID;
			if (const SWF_Result supported = Supported(filterId, true); supported != SWF_OK)
				return supported;
			dispatch::Entry entry;
			entry.filter = fn;
			entry.user = user;
			entry.priority = priority;
			return dispatch::Filters().Add(filterId, entry, out);
		}

		SWF_Result RemoveFilter(SWF_Subscription filter)
		{
			return Ready() ? dispatch::Filters().Remove(filter) : SWF_E_NOT_READY;
		}

		SWF_Result NotifyWritten(void* object, SWF_Fields fields)
		{
			if (!Ready())
				return SWF_E_NOT_READY;
			if (!object || !fields)
				return SWF_E_INVALID;
			SWF_WrittenEvent event{};
			event.header.size = sizeof(event);
			event.header.id = SWF_EVENT_WRITTEN;
			event.object = object;
			event.fields = fields;
			dispatch::Fire(event.header);
			return SWF_OK;
		}

		uint32_t SiteCount(uint32_t eventId)
		{
			return Ready() ? stores::SiteCount(eventId) : 0;
		}

		SWF_Result ResolveSite(uintptr_t address, uint32_t* eventId, uint32_t* site, uint32_t* siteOffset)
		{
			if (!Ready())
				return SWF_E_NOT_READY;
			uint32_t id = 0, index = 0, offset = 0;
			if (!hooking::ResolveStub(address, id, index, offset))
				return SWF_E_INVALID;
			if (eventId)
				*eventId = id;
			if (site)
				*site = index;
			if (siteOffset)
				*siteOffset = offset;
			return SWF_OK;
		}

		uint32_t CheckIntegrity(SWF_IntegrityFn fn, void* user)
		{
			return Ready() ? hooking::CheckIntegrity(fn, user) : 0;
		}

		SWF_Result Write(uint32_t kind, void* object, uint64_t value)
		{
			return Ready() ? writes::Write(kind, object, value) : SWF_E_NOT_READY;
		}

		constexpr SWF_Api kApi{
			sizeof(SWF_Api),
			SWF_ABI_MINOR,
			&GetProviderInfo,
			&GetCaps,
			&Subscribe,
			&Unsubscribe,
			&AddFilter,
			&RemoveFilter,
			&NotifyWritten,
			&SiteCount,
			&ResolveSite,
			&CheckIntegrity,
			&Write,
		};

		bool OtherCandidateActive()
		{
			for (const auto& candidate : client::FindCandidates())
				if (candidate.info->GetState() == SWF_STATE_ACTIVE)
					return true;
			return false;
		}

		void PinModule()
		{
			HMODULE module = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(&PinModule), &module);
		}

		std::string CapsString(SWF_Caps installed)
		{
			std::string names;
			for (const Family& family : Families())
				if (installed & family.cap)
					names += (names.empty() ? "" : "|") + std::string(family.name);
			return names.empty() ? "none" : names;
		}
	}

	uint32_t State()
	{
		return state.load(std::memory_order_acquire);
	}

	SWF_Result Activate(uint32_t candidateCount)
	{
		std::lock_guard lock(activateMutex);
		switch (state.load()) {
		case SWF_STATE_ACTIVE:
			return SWF_OK;
		case SWF_STATE_FAILED:
			return SWF_E_FAILED;
		default:
			break;
		}

		if (OtherCandidateActive())
			return SWF_E_SUPERSEDED;

#ifdef SWF_TESTING
		if (forceFailure) {
			state.store(SWF_STATE_FAILED);
			log::Warn("activation failed: forced by test");
			return SWF_E_FAILED;
		}
#endif

		// Verify everything before writing anything, so a failed activation leaves the
		// engine untouched for the next candidate.
		const auto families = Families();
		SWF_Caps verified = 0;
		for (const Family& family : families) {
			if (family.verify())
				verified |= family.cap;
			else if (family.required) {
				state.store(SWF_STATE_FAILED);
				log::Error("activation failed: required family '{}' did not verify", family.name);
				return SWF_E_FAILED;
			} else
				log::Warn("family '{}' did not verify; its events are unavailable", family.name);
		}

		SWF_Caps installed = 0;
		for (const Family& family : families) {
			if (!(verified & family.cap))
				continue;
			hooking::SetFamily(family.name);
			const bool ok = family.install();
			hooking::SetFamily(nullptr);
			if (ok)
				installed |= family.cap;
			else
				log::Error("family '{}' verified but failed to install", family.name);
		}

		caps.store(installed);
		candidates = candidateCount;
		PinModule();
#ifdef SWF_TESTING
		++installCount;
#endif
		state.store(SWF_STATE_ACTIVE, std::memory_order_release);
		log::Info("active as provider (abi {}.{}, {} candidates, caps {})", SWF_ABI_MAJOR, SWF_ABI_MINOR, candidateCount, CapsString(installed));
		return SWF_OK;
	}

	const SWF_Api* GetApi(uint32_t abiMinorWanted)
	{
		return Ready() && abiMinorWanted <= SWF_ABI_MINOR ? &kApi : nullptr;
	}

#ifdef SWF_TESTING
	namespace testing
	{
		void ForceActivateFailure(bool fail)
		{
			forceFailure = fail;
		}

		uint32_t InstallCount()
		{
			return installCount;
		}
	}
#endif
}
