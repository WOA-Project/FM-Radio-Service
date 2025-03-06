#include "pch.h"
#include "RPC Server.h"
#include "Command Listener.h"
#include "Notifier Handles.h"
#include "Checked Windows API Calls.h"
#include "Miniport Service Interface_s.c"

CommandListener * Server;

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
	return operator new(len);
}

void __RPC_USER midl_user_free(void __RPC_FAR* ptr)
{
	delete ptr;
}

void DisableRadio()
{
	Server->DisableRadio();
}

void SeekForwards()
{
	Server->SeekForwards();
}

void SeekBackwards()
{
	Server->SeekBackwards();
}

void SetAudioEndpoint(AudioEndpoint Endpoint)
{
	Server->SetAudioEndpoint(Endpoint);
}

void SetFrequency(FrequencyType Frequency)
{
	Server->SetFrequency(Frequency);
}

unsigned GetSignalQuality()
{
	return Server->GetSignalQuality();
}

void EnableRadio()
{
	Server->EnableRadio();
}

void AcquireEvent(Client ClientId, Notification * Event)
{
	Server->AcquireEvent(ClientId, Event);
}

void AcquireInitialState()
{
	Server->AcquireInitialState();
}

Client AcquireClientId()
{
	return Server->AcquireClientId();
}

LSTATUS GetFMMiniportLibraryPath(LPWSTR Buffer, DWORD BufferSize)
{
	HKEY Key;
	LSTATUS status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Tuner\\Audio\\FmRadio", 0, KEY_READ, &Key);
	if (status != ERROR_SUCCESS)
	{
		throw std::system_error(status, std::system_category());
	}

	if (Key == nullptr)
	{
		throw std::system_error(::GetLastError(), std::system_category());
	}

	status = RegQueryValueEx(Key, L"MiniportDll", nullptr, nullptr, reinterpret_cast<LPBYTE>(Buffer), &BufferSize);
	if (status != ERROR_SUCCESS)
	{
		throw std::system_error(status, std::system_category());
	}

	RegCloseKey(Key);

	return ERROR_SUCCESS;
}

CommandListener::CommandListener(RadioTopology & Controller) :
	LibraryProxy(MiniportTunerDevice, MiniportReceiveDevice),
	TopologyController(Controller),
	Notifier(MiniportReceiveDevice, Controller, LibraryProxy),
	PortTunerDevice(PortReceiveDevice, Notifier),
	NextClientId(0)
{
	Server = this;

	wchar_t MiniportDllPath[MAX_PATH];
	DWORD BufferSize = sizeof(MiniportDllPath);

	GetFMMiniportLibraryPath(MiniportDllPath, BufferSize);

	const auto FMSLMiniportAddress = LoadLibrary(MiniportDllPath);
	if (FMSLMiniportAddress == nullptr)
	{
		throw std::system_error(::GetLastError(), std::system_category());
	}

	const auto CreateAdapterAddress = Windows::GetFunctionAddress<CreateAdapterDriver_t>(FMSLMiniportAddress, "CreateAdapterDriver");

	IAdapterDriver * Driver;
	Windows::CheckedAPICall(CreateAdapterAddress, &Driver);
	Windows::CheckedMemberAPICall(Driver, &IAdapterDriver::CreateMiniportTunerDevice, 0, 0, &PortTunerDevice, &MiniportTunerDevice);

	// TODO: check for errors
	MiniportTunerDevice->QueryInterface(IID_IMiniportFmRxDevice, reinterpret_cast<void **>(&MiniportReceiveDevice));

	std::this_thread::sleep_for(std::chrono::seconds(5));

	Windows::CheckedMemberAPICall(MiniportTunerDevice, &IMiniportTunerDevice::SetPowerState, TUNER_POWERSTATE::TUNER_POWERSTATE_ON, INVALID_HANDLE_VALUE);

	// Service occasionally crashes on operating system boot inside the miniport DLL
	// Seems to be related to these two calls and a race condition internally?
	std::this_thread::sleep_for(std::chrono::seconds(5));

	FM_REGIONPARAMS p;
	p.Emphasis = FM_EMPHASIS::FM_EMPHASIS_75_USEC;
	p.FrequencyMin = 87500;
	p.FrequencyMax = 108000;
	p.FrequencySpacing = 50;

	using namespace NotifierHandles;
	Windows::CheckedMemberAPICall(MiniportReceiveDevice, &IMiniportFmRxDevice::SetRegionParams, &p, INVALID_HANDLE_VALUE /*AsyncContextToHANDLE(AsyncContextHandle::PlayStateChange)*/);

	Controller.OnAntennaStatusChange = [this] {
		Notifier.OnRadioEvent(AsyncContextToHANDLE(AsyncContextHandle::AntennaStatusChange));
	};
}

CommandListener::~CommandListener()
{
	MiniportTunerDevice->Release();
}

void CommandListener::Listen()
{
	wchar_t Endpoint[] = L"FM Radio Miniport Service Interface";
	RPCServer::Listen(MiniportServiceInterface_v1_0_s_ifspec, Endpoint);
}

void CommandListener::Shutdown()
{
	Windows::CheckedRPCCall(RpcMgmtStopServerListening, nullptr);
	Notifier.Shutdown();

	// RpcServerUnregisterIf(nullptr, nullptr, false); TODO: this seems to fail with RPC_S_UNKNOWN_MGR_TYPE
}

void CommandListener::AcquireEvent(Client ClientId, Notification * Event)
{
	Notifier.AcquireEvent(ClientId, Event);
}

void CommandListener::AcquireInitialState()
{
	using namespace NotifierHandles;
	Notifier.OnRadioEvent(AsyncContextToHANDLE(AsyncContextHandle::FrequencyChange));
	Notifier.OnRadioEvent(AsyncContextToHANDLE(AsyncContextHandle::PlayStateChange));
}

Client CommandListener::AcquireClientId()
{
	const auto ClientId = NextClientId++;
	Notifier.OnClientAdded(ClientId);
	Notifier.OnRadioEvent(NotifierHandles::AsyncContextToHANDLE(NotifierHandles::AsyncContextHandle::AntennaStatusChange));
	return ClientId;
}

void CommandListener::EnableRadio()
{
	TopologyController.SetFmState(true);

	using namespace NotifierHandles;
	Notifier.OnRadioEvent(AsyncContextToHANDLE(AsyncContextHandle::PlayStateChange));
}

void CommandListener::DisableRadio()
{
	TopologyController.SetFmState(false);

	using namespace NotifierHandles;
	Notifier.OnRadioEvent(AsyncContextToHANDLE(AsyncContextHandle::PlayStateChange));

	// "C:\Program Files (x86)\Windows Kits\10\bin\x86\Inf2Cat.exe" /driver:"." /os:10_RS4_ARM64

	/*using namespace NotifierHandles;
	Windows::CheckedMemberAPICall(MiniportTunerDevice, &IMiniportTunerDevice::SetPowerState, TUNER_POWERSTATE::TUNER_POWERSTATE_ON, AsyncContextToHANDLE(AsyncContextHandle::PlayStateChange));*/
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
		{
			TopologyController.SetFmEndpointId(0);
			return;
		}
		case AudioEndpoint::Headset:
		{
			TopologyController.SetFmEndpointId(1);
			return;
		}
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