#pragma once

#include "pch.h"

class FakePortFmRx : public IPortFmRxDevice
{
public:
    FakePortFmRx() : ReferenceCounter(1)
    {
    }

    [[nodiscard]]
    HRESULT STDMETHODCALLTYPE ProcessRdsGroup(
        const RDS_GROUP *pRdsGroup) final override
    {
        if (pRdsGroup == nullptr)
        {
            return E_POINTER;
        }

        std::wcout << L"#2: Processing RDS group!" << std::endl;

        wprintf(
            L"FM: Miniport: block 1 is %x, block2 is %x, block3 is %x, block4 is %x\n",
            pRdsGroup->wBlockA,
            pRdsGroup->wBlockB,
            pRdsGroup->wBlockC,
            pRdsGroup->wBlockD);

        return S_OK;
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
            *ppvObject = static_cast<IUnknown *>(this);
        }
        else if (riid == __uuidof(IPortFmRxDevice))
        {
            *ppvObject = static_cast<IPortFmRxDevice *>(this);
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
        // Lifetime is externally owned.
        //
        // Do NOT delete this.
        //
        return static_cast<ULONG>(RefCount);
    }

private:
    std::atomic_ulong ReferenceCounter;
};