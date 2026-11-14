#pragma once

#include "pch.h"

// From sysvad.h
#define VALUE_NORMALIZE_P(v, step) \
    ((((v) + (step) / 2) / (step)) * (step))

#define VALUE_NORMALIZE(v, step) \
    ((v) > 0 ? VALUE_NORMALIZE_P((v), (step)) : -(VALUE_NORMALIZE_P(-(v), (step))))

#define VALUE_NORMALIZE_IN_RANGE_EX(v, min, max, step) \
    ((v) > (max) ? (max) : \
    (v) < (min) ? (min) : \
    VALUE_NORMALIZE((v), (step))

#define MAX_PROVIDERIDS 4

class MMEvent : public IMMNotificationClient
{
public:
    using StatusEvent = std::function<void()>;

    MMEvent(
        const LPWSTR &SpeakersId,
        const StatusEvent &AntennaStatusHandler) : ReferenceCounter(1),
                                                   CommunicationSpeakerId(SpeakersId),
                                                   OnAntennaStatusChange(AntennaStatusHandler)
    {
    }

private:
    [[nodiscard]]
    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        _COM_Outptr_ void **ppvObject) final override
    {
        if (ppvObject == nullptr)
        {
            return E_POINTER;
        }

        *ppvObject = nullptr;

        if (riid == __uuidof(IUnknown))
        {
            *ppvObject =
                static_cast<IUnknown *>(
                    static_cast<IMMNotificationClient *>(this));
        }
        else if (riid == __uuidof(IMMNotificationClient))
        {
            *ppvObject =
                static_cast<IMMNotificationClient *>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) final override
    {
        return ReferenceCounter.fetch_add(
                   1,
                   std::memory_order_relaxed) +
               1;
    }

    ULONG STDMETHODCALLTYPE Release(void) final override
    {
        const auto RefCount =
            ReferenceCounter.fetch_sub(
                1,
                std::memory_order_acq_rel) -
            1;

        //
        // Lifetime is owned by RadioTopology.
        // Do not delete this.
        //
        return static_cast<ULONG>(RefCount);
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
        EDataFlow,
        ERole,
        LPCWSTR) final override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(
        LPCWSTR) final override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(
        LPCWSTR) final override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
        LPCWSTR DeviceId,
        DWORD) final override
    {
        if (
            (CommunicationSpeakerId != nullptr) &&
            (DeviceId != nullptr) &&
            (wcscmp(DeviceId, CommunicationSpeakerId) == 0) &&
            OnAntennaStatusChange)
        {
            OnAntennaStatusChange();
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
        LPCWSTR,
        PROPERTYKEY) final override
    {
        return S_OK;
    }

    std::atomic_ulong ReferenceCounter;

    const LPWSTR &CommunicationSpeakerId;
    const StatusEvent &OnAntennaStatusChange;
};

class RadioTopology
{
public:
    RadioTopology();
    ~RadioTopology();

    HRESULT SetFmVolume(_In_ LONG volume);
    HRESULT GetFmVolume(_Out_ LONG *plVolume);

    HRESULT SetFmEndpointId(_In_ LONG index);
    HRESULT GetFmEndpointId(_Out_ PKSTOPOLOGY_ENDPOINTID pEndpointId);
    HRESULT GetFmAntennaEndpointId(_Out_ PKSTOPOLOGY_ENDPOINTID pEndpointId);

    HRESULT SetFmState(_In_ BOOL state);
    HRESULT GetFmState(_Out_ BOOL *pbState);

    bool IsAntennaPresent();

    MMEvent::StatusEvent OnAntennaStatusChange;

private:
    bool m_ComInitialized = false;

    LPWSTR m_CommunicationSpeakers = nullptr;

    MMEvent Event;

    CComPtr<IPart> CommunicationSpeakersPart;

    CComPtr<IKsControl> m_spWaveKsControl;
    CComPtr<IKsControl> m_spTopologyKsControl;
};