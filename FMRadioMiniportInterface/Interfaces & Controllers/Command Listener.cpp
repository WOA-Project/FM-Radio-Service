#include "pch.h"
#include "RPC Server.h"
#include "Command Listener.h"
#include "Notifier Handles.h"
#include "Checked Windows API Calls.h"
#include "Miniport Service Interface_s.c"

CommandListener *Server = nullptr;

void __RPC_FAR *__RPC_USER midl_user_allocate(size_t len)
{
    return operator new(len);
}

void __RPC_USER midl_user_free(void __RPC_FAR *ptr)
{
    operator delete(ptr);
}

void DisableRadio()
{
    if (Server != nullptr)
    {
        Server->DisableRadio();
    }
}

void SeekForwards()
{
    if (Server != nullptr)
    {
        Server->SeekForwards();
    }
}

void SeekBackwards()
{
    if (Server != nullptr)
    {
        Server->SeekBackwards();
    }
}

void SetAudioEndpoint(AudioEndpoint Endpoint)
{
    if (Server != nullptr)
    {
        Server->SetAudioEndpoint(Endpoint);
    }
}

void SetFrequency(FrequencyType Frequency)
{
    if (Server != nullptr)
    {
        Server->SetFrequency(Frequency);
    }
}

unsigned GetSignalQuality()
{
    if (Server == nullptr)
    {
        return 0;
    }

    return Server->GetSignalQuality();
}

void EnableRadio()
{
    if (Server != nullptr)
    {
        Server->EnableRadio();
    }
}

void AcquireEvent(Client ClientId, Notification *Event)
{
    if (Server != nullptr)
    {
        Server->AcquireEvent(ClientId, Event);
    }
}

void AcquireInitialState()
{
    if (Server != nullptr)
    {
        Server->AcquireInitialState();
    }
}

Client AcquireClientId()
{
    if (Server == nullptr)
    {
        return 0;
    }

    return Server->AcquireClientId();
}

LSTATUS GetFMMiniportLibraryPath(LPWSTR Buffer, DWORD BufferSize)
{
    HKEY Key = nullptr;

    LSTATUS status =
        RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            L"System\\CurrentControlSet\\Control\\Tuner\\Audio\\FmRadio",
            0,
            KEY_READ,
            &Key);

    if (status != ERROR_SUCCESS)
    {
        throw std::system_error(status, std::system_category());
    }

    try
    {
        status = RegQueryValueEx(
            Key,
            L"MiniportDll",
            nullptr,
            nullptr,
            reinterpret_cast<LPBYTE>(Buffer),
            &BufferSize);

        if (status != ERROR_SUCCESS)
        {
            throw std::system_error(status, std::system_category());
        }

        RegCloseKey(Key);
        return ERROR_SUCCESS;
    }
    catch (...)
    {
        RegCloseKey(Key);
        throw;
    }
}

CommandListener::CommandListener(RadioTopology &Controller) : LibraryProxy(MiniportTunerDevice, MiniportReceiveDevice),
                                                              TopologyController(Controller),
                                                              Notifier(MiniportReceiveDevice, Controller, LibraryProxy),
                                                              PortTunerDevice(PortReceiveDevice, Notifier),
                                                              NextClientId(0)
{
    Server = this;

    MiniportReceiveDevice = nullptr;
    MiniportTunerDevice = nullptr;

    wchar_t MiniportDllPath[MAX_PATH] = {};
    DWORD BufferSize = sizeof(MiniportDllPath);

    GetFMMiniportLibraryPath(MiniportDllPath, BufferSize);

    const auto FMSLMiniportAddress = LoadLibrary(MiniportDllPath);

    if (FMSLMiniportAddress == nullptr)
    {
        throw std::system_error(
            ::GetLastError(),
            std::system_category());
    }

    const auto CreateAdapterAddress =
        Windows::GetFunctionAddress<CreateAdapterDriver_t>(
            FMSLMiniportAddress,
            "CreateAdapterDriver");

    IAdapterDriver *Driver = nullptr;

    Windows::CheckedAPICall(
        CreateAdapterAddress,
        &Driver);

    Windows::CheckedMemberAPICall(
        Driver,
        &IAdapterDriver::CreateMiniportTunerDevice,
        0,
        0,
        &PortTunerDevice,
        &MiniportTunerDevice);

    HRESULT hr =
        MiniportTunerDevice->QueryInterface(
            IID_IMiniportFmRxDevice,
            reinterpret_cast<void **>(&MiniportReceiveDevice));

    if (FAILED(hr) || MiniportReceiveDevice == nullptr)
    {
        throw std::system_error(
            hr,
            std::system_category());
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    Windows::CheckedMemberAPICall(
        MiniportTunerDevice,
        &IMiniportTunerDevice::SetPowerState,
        TUNER_POWERSTATE::TUNER_POWERSTATE_ON,
        INVALID_HANDLE_VALUE);

    // Service occasionally crashes on operating system boot
    // inside the miniport DLL.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    FM_REGIONPARAMS p = {};
    p.Emphasis = FM_EMPHASIS::FM_EMPHASIS_75_USEC;
    p.FrequencyMin = 87500;
    p.FrequencyMax = 108000;
    p.FrequencySpacing = 50;

    using namespace NotifierHandles;

    Windows::CheckedMemberAPICall(
        MiniportReceiveDevice,
        &IMiniportFmRxDevice::SetRegionParams,
        &p,
        INVALID_HANDLE_VALUE);

    Controller.OnAntennaStatusChange =
        [this]
    {
        Notifier.OnRadioEvent(
            AsyncContextToHANDLE(
                AsyncContextHandle::AntennaStatusChange));
    };
}

CommandListener::~CommandListener()
{
    Server = nullptr;

    if (MiniportReceiveDevice != nullptr)
    {
        MiniportReceiveDevice->Release();
        MiniportReceiveDevice = nullptr;
    }

    if (MiniportTunerDevice != nullptr)
    {
        MiniportTunerDevice->Release();
        MiniportTunerDevice = nullptr;
    }
}

void CommandListener::Listen()
{
    wchar_t Endpoint[] = L"FM Radio Miniport Service Interface";

    RPCServer::Listen(
        MiniportServiceInterface_v1_0_s_ifspec,
        Endpoint);
}

void CommandListener::Shutdown()
{
    Windows::CheckedRPCCall(
        RpcMgmtStopServerListening,
        nullptr);

    Notifier.Shutdown();

    // RpcServerUnregisterIf(nullptr, nullptr, false);
}

void CommandListener::AcquireEvent(
    Client ClientId,
    Notification *Event)
{
    Notifier.AcquireEvent(ClientId, Event);
}

void CommandListener::AcquireInitialState()
{
    using namespace NotifierHandles;

    Notifier.OnRadioEvent(
        AsyncContextToHANDLE(
            AsyncContextHandle::FrequencyChange));

    Notifier.OnRadioEvent(
        AsyncContextToHANDLE(
            AsyncContextHandle::PlayStateChange));
}

Client CommandListener::AcquireClientId()
{
    const auto ClientId = NextClientId++;

    Notifier.OnClientAdded(ClientId);

    Notifier.OnRadioEvent(
        NotifierHandles::AsyncContextToHANDLE(
            NotifierHandles::AsyncContextHandle::AntennaStatusChange));

    return ClientId;
}

void CommandListener::EnableRadio()
{
    TopologyController.SetFmState(true);

    using namespace NotifierHandles;

    Notifier.OnRadioEvent(
        AsyncContextToHANDLE(
            AsyncContextHandle::PlayStateChange));
}

void CommandListener::DisableRadio()
{
    TopologyController.SetFmState(false);

    using namespace NotifierHandles;

    Notifier.OnRadioEvent(
        AsyncContextToHANDLE(
            AsyncContextHandle::PlayStateChange));
}

void CommandListener::SeekForwards()
{
    LibraryProxy.SeekForwards();
}

void CommandListener::SeekBackwards()
{
    LibraryProxy.SeekBackwards();
}

void CommandListener::SetAudioEndpoint(AudioEndpoint Endpoint)
{
    switch (Endpoint)
    {
    case AudioEndpoint::Speakers:
        TopologyController.SetFmEndpointId(0);
        return;

    case AudioEndpoint::Headset:
        TopologyController.SetFmEndpointId(1);
        return;

    default:
        throw std::invalid_argument(
            "Unknown AudioEndpoint value.");
    }
}

void CommandListener::SetFrequency(FrequencyType Frequency)
{
    LibraryProxy.SetFrequency(Frequency);
}

unsigned CommandListener::GetSignalQuality()
{
    return LibraryProxy.GetSignalQuality();
}