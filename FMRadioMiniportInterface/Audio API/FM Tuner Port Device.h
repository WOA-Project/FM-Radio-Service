#pragma once

#include "pch.h"
#include "FM Receive Port Device.h"
#include "Interfaces & Controllers/Event Notifier.h"

class FakePortTuner : public IPortTunerDevice
{
public:
    FakePortTuner(
        FakePortFmRx &ReceiveDevice,
        EventNotifier &Notifier) : ReferenceCounter(1),
                                   ReceiveDevice(ReceiveDevice),
                                   Notifier(Notifier)
    {
    }

    HRESULT STDMETHODCALLTYPE SendTunerEvent(
        [[maybe_unused]] REFGUID rguidEventId,
        [[maybe_unused]] AV_VARIANT *pavvParam) final override
    {
        return S_OK;
    }

    [[nodiscard]]
    HRESULT STDMETHODCALLTYPE AsyncComplete(
        HRESULT hrStatus,
        HTUNER_ASYNCCTXT hAsyncCtxt) final override
    {
        if (FAILED(hrStatus))
        {
            // Event was ignored / cancelled.
            return S_OK;
        }

        try
        {
            Notifier.OnRadioEvent(hAsyncCtxt);
            return S_OK;
        }
        catch (...)
        {
            return E_FAIL;
        }
    }

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
                static_cast<IUnknown *>(this);
        }
        else if (
            riid == IID_IPortFmRxDevice ||
            riid == __uuidof(IPortFmRxDevice))
        {
            *ppvObject =
                static_cast<IPortFmRxDevice *>(
                    &ReceiveDevice);
        }
        else if (
            riid == __uuidof(IPortTunerDevice))
        {
            *ppvObject =
                static_cast<IPortTunerDevice *>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }

        //
        // Very important.
        //
        // COM QueryInterface must AddRef the object
        // it returns.
        //
        if (riid == IID_IPortFmRxDevice ||
            riid == __uuidof(IPortFmRxDevice))
        {
            ReceiveDevice.AddRef();
        }
        else
        {
            AddRef();
        }

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
        // Lifetime is externally owned by
        // CommandListener.
        //
        // Do not delete this.
        //
        return static_cast<ULONG>(RefCount);
    }

private:
    std::atomic_ulong ReferenceCounter;

    FakePortFmRx &ReceiveDevice;
    EventNotifier &Notifier;
};