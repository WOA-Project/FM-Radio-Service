#include "pch.h"
#include "Notifier Handles.h"
#include "Qualcomm Miniport Proxy.h"
#include "Checked Windows API Calls.h"

QualcommMiniportProxy::QualcommMiniportProxy(
    IMiniportTunerDevice *&MiniportTunerDevice,
    IMiniportFmRxDevice *&MiniportReceiveDevice) : MiniportTunerDevice(MiniportTunerDevice),
                                                   MiniportReceiveDevice(MiniportReceiveDevice)
{
}

void QualcommMiniportProxy::SeekForwards()
{
    std::lock_guard<decltype(LibraryLock)> Lock(LibraryLock);

    if (MiniportReceiveDevice == nullptr)
    {
        throw std::runtime_error(
            "MiniportReceiveDevice is null.");
    }

    Windows::CheckedMemberAPICall(
        MiniportReceiveDevice,
        &IMiniportFmRxDevice::Seek,
        FM_SEEKDIR::FM_SEEKDIR_FORWARD,
        NotifierHandles::AsyncContextToHANDLE(
            NotifierHandles::AsyncContextHandle::FrequencyChange));
}

void QualcommMiniportProxy::SeekBackwards()
{
    std::lock_guard<decltype(LibraryLock)> Lock(LibraryLock);

    if (MiniportReceiveDevice == nullptr)
    {
        throw std::runtime_error(
            "MiniportReceiveDevice is null.");
    }

    Windows::CheckedMemberAPICall(
        MiniportReceiveDevice,
        &IMiniportFmRxDevice::Seek,
        FM_SEEKDIR::FM_SEEKDIR_BACKWARD,
        NotifierHandles::AsyncContextToHANDLE(
            NotifierHandles::AsyncContextHandle::FrequencyChange));
}

void QualcommMiniportProxy::SetFrequency(
    FrequencyType Frequency)
{
    std::lock_guard<decltype(LibraryLock)> Lock(LibraryLock);

    if (MiniportReceiveDevice == nullptr)
    {
        throw std::runtime_error(
            "MiniportReceiveDevice is null.");
    }

    Windows::CheckedMemberAPICall(
        MiniportReceiveDevice,
        &IMiniportFmRxDevice::Tune,
        static_cast<FM_FREQUENCY>(Frequency),
        NotifierHandles::AsyncContextToHANDLE(
            NotifierHandles::AsyncContextHandle::FrequencyChange));
}

FrequencyType QualcommMiniportProxy::GetFrequency()
{
    std::lock_guard<decltype(LibraryLock)> Lock(LibraryLock);

    if (MiniportReceiveDevice == nullptr)
    {
        throw std::runtime_error(
            "MiniportReceiveDevice is null.");
    }

    FM_FREQUENCY Frequency = 0;

    Windows::CheckedMemberAPICall(
        MiniportReceiveDevice,
        &IMiniportFmRxDevice::GetTunedFrequency,
        &Frequency);

    return static_cast<FrequencyType>(Frequency);
}

unsigned QualcommMiniportProxy::GetSignalQuality()
{
    std::lock_guard<decltype(LibraryLock)> Lock(LibraryLock);

    if (MiniportTunerDevice == nullptr)
    {
        throw std::runtime_error(
            "MiniportTunerDevice is null.");
    }

    DWORD ReturnSize = 0;
    TUNER_PROPDATA_RSSI RSSI = {};

    Windows::CheckedMemberAPICall(
        MiniportTunerDevice,
        &IMiniportTunerDevice::GetProperty,
        TUNER_PROPID_RSSI,
        &RSSI,
        sizeof(RSSI),
        &ReturnSize);

    static const auto Minimum = 320L;
    static const auto Maximum = 400L;

    const auto Quality =
        (100 * (static_cast<long>(RSSI.dwRssi) - Minimum)) /
        (Maximum - Minimum);

    return static_cast<unsigned>(
        std::clamp<long>(
            Quality,
            0,
            100));
}