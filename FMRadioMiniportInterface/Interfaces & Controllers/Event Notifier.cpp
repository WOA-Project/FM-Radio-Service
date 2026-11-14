#include "pch.h"
#include "Event Notifier.h"
#include "Notifier Handles.h"
#include "Checked Windows API Calls.h"

EventNotifier::EventBit EventNotifier::AsyncContextToBit(HTUNER_ASYNCCTXT EventHandle)
{
    using namespace NotifierHandles;

    const auto EventType =
        static_cast<AsyncContextHandle>(
            reinterpret_cast<uintptr_t>(EventHandle));

    switch (EventType)
    {
    case AsyncContextHandle::FrequencyChange:
        return EventBit::Frequency;

    case AsyncContextHandle::PlayStateChange:
        return EventBit::PlayState;

    case AsyncContextHandle::AntennaStatusChange:
        return EventBit::AntennaStatus;

    case AsyncContextHandle::RDSAvailablility:
        return EventBit::RDS;
    }

    throw std::invalid_argument("Unknown async context handle.");
}

EventNotifier::EventNotifier(
    IMiniportFmRxDevice *&Device,
    RadioTopology &Controller,
    QualcommMiniportProxy &Proxy) : MiniportReceiveDevice(Device),
                                    TopologyController(Controller),
                                    LibraryProxy(Proxy)
{
}

void EventNotifier::AcquireEvent(
    Client ClientId,
    Notification *RadioEvent)
{
    if (RadioEvent == nullptr)
    {
        throw std::invalid_argument("RadioEvent was null.");
    }

    EventBit EventType = EventBit::Shutdown;

    {
        std::unique_lock<decltype(EventsMutex)> Lock(EventsMutex);

        NewEvent.wait(
            Lock,
            [this, ClientId]
            {
                const auto Event = Events.find(ClientId);

                if (Event == Events.cend())
                {
                    throw std::invalid_argument(
                        "The specified client identifier was not found.");
                }

                return Event->second.any();
            });

        const auto EventIterator = Events.find(ClientId);

        if (EventIterator == Events.cend())
        {
            throw std::invalid_argument(
                "The specified client identifier was not found.");
        }

        auto &Event = EventIterator->second;

        // Process one event at a time and clear it.
        for (
            size_t Bit = 0;
            Bit < static_cast<size_t>(EventBit::Count);
            ++Bit)
        {
            if (!Event[Bit])
            {
                continue;
            }

            EventType = static_cast<EventBit>(Bit);
            Event[Bit] = false;
            break;
        }
    }

    switch (EventType)
    {
    case EventBit::Shutdown:
    {
        // Client doesn't come back after this because the
        // RPC server should already be stopped.
        return;
    }

    case EventBit::Frequency:
    {
        RadioEvent->Type = Event::FrequencyChanged;
        RadioEvent->tagged_union.KHz =
            LibraryProxy.GetFrequency();
        return;
    }

    case EventBit::PlayState:
    {
        BOOL State = FALSE;

        TopologyController.GetFmState(&State);

        RadioEvent->Type = Event::PlayStateChanged;
        RadioEvent->tagged_union.PlayState =
            static_cast<boolean>(State);

        return;
    }

    case EventBit::AntennaStatus:
    {
        RadioEvent->Type = Event::AntennaStateChanged;
        RadioEvent->tagged_union.Present =
            TopologyController.IsAntennaPresent();

        return;
    }

    case EventBit::RDS:
    {
        // Not implemented yet.
        return;
    }
    }

    throw std::logic_error("Unexpected EventBit value.");
}

void EventNotifier::OnRadioEvent(HTUNER_ASYNCCTXT EventHandle)
{
    if (EventHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    const auto Bit = AsyncContextToBit(EventHandle);

    const auto BitIndex =
        static_cast<size_t>(Bit);

    if (BitIndex >= static_cast<size_t>(EventBit::Count))
    {
        throw std::logic_error(
            "Invalid EventBit value.");
    }

    {
        std::lock_guard<decltype(EventsMutex)> Lock(EventsMutex);

        for (auto &Event : Events)
        {
            Event.second[BitIndex] = true;
        }
    }

    NewEvent.notify_all();
}

void EventNotifier::OnClientAdded(Client ClientId)
{
    std::lock_guard<decltype(EventsMutex)> Lock(EventsMutex);

    Events.emplace(
        ClientId,
        decltype(Events)::value_type::second_type());
}

void EventNotifier::Shutdown()
{
    {
        std::lock_guard<decltype(EventsMutex)> Lock(EventsMutex);

        for (auto &Event : Events)
        {
            Event.second[static_cast<size_t>(EventBit::Shutdown)] =
                true;
        }
    }

    NewEvent.notify_all();
}