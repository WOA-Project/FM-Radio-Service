#pragma once

#include "pch.h"
#include "Checked Windows API Calls.h"

class RPCServer
{
public:
    static void Listen(
        RPC_IF_HANDLE ServerHandle,
        wchar_t Endpoint[])
    {
        //
        // SID for packaged/UWP applications.
        //
        BYTE AppContainerSID[SECURITY_MAX_SID_SIZE] = {};
        DWORD AppContainerSIDSize =
            static_cast<DWORD>(sizeof(AppContainerSID));

        Windows::CheckedBooleanAPICall(
            CreateWellKnownSid,
            WELL_KNOWN_SID_TYPE::WinBuiltinAnyPackageSid,
            nullptr,
            AppContainerSID,
            &AppContainerSIDSize);

        //
        // SID for everyone.
        //
        BYTE WorldEveryoneSID[SECURITY_MAX_SID_SIZE] = {};
        DWORD WorldEveryoneSIDSize =
            static_cast<DWORD>(sizeof(WorldEveryoneSID));

        Windows::CheckedBooleanAPICall(
            CreateWellKnownSid,
            WELL_KNOWN_SID_TYPE::WinWorldSid,
            nullptr,
            WorldEveryoneSID,
            &WorldEveryoneSIDSize);

        //
        // Grant access.
        //
        std::array<EXPLICIT_ACCESS, 2> Permissions = {};

        Permissions[0].grfAccessMode = SET_ACCESS;
        Permissions[0].grfAccessPermissions = GENERIC_ALL;
        Permissions[0].grfInheritance = NO_INHERITANCE;
        Permissions[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        Permissions[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        Permissions[0].Trustee.ptstrName =
            reinterpret_cast<LPWSTR>(WorldEveryoneSID);

        Permissions[1].grfAccessMode = SET_ACCESS;
        Permissions[1].grfAccessPermissions = GENERIC_ALL;
        Permissions[1].grfInheritance = NO_INHERITANCE;
        Permissions[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        Permissions[1].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
        Permissions[1].Trustee.ptstrName =
            reinterpret_cast<LPWSTR>(AppContainerSID);

        PACL AllowedAccessors = nullptr;

        Windows::CheckedAPICall(
            SetEntriesInAcl,
            static_cast<ULONG>(Permissions.size()),
            Permissions.data(),
            nullptr,
            &AllowedAccessors);

        try
        {
            SECURITY_DESCRIPTOR Descriptor = {};

            Windows::CheckedBooleanAPICall(
                InitializeSecurityDescriptor,
                &Descriptor,
                SECURITY_DESCRIPTOR_REVISION);

            Windows::CheckedBooleanAPICall(
                SetSecurityDescriptorDacl,
                &Descriptor,
                TRUE,
                AllowedAccessors,
                FALSE);

            //
            // Local RPC endpoint.
            //
            wchar_t ProtocolSequence[] = L"ncalrpc";

            Windows::CheckedRPCCall(
                RpcServerUseProtseqEp,
                reinterpret_cast<RPC_WSTR>(ProtocolSequence),
                RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                reinterpret_cast<RPC_WSTR>(Endpoint),
                &Descriptor);

            Windows::CheckedRPCCall(
                RpcServerRegisterIf3,
                ServerHandle,
                nullptr,
                nullptr,
                RPC_IF_AUTOLISTEN | RPC_IF_ALLOW_LOCAL_ONLY,
                RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                0,
                nullptr,
                &Descriptor);

            //
            // With RPC_IF_AUTOLISTEN this frequently returns
            // RPC_S_ALREADY_LISTENING depending on timing.
            //
            RPC_STATUS Status =
                RpcServerListen(
                    1,
                    RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                    FALSE);

            if (Status != RPC_S_OK &&
                Status != RPC_S_ALREADY_LISTENING)
            {
                Windows::CheckedRPCCall(
                    [Status](
                        unsigned long,
                        unsigned long,
                        int)
                    {
                        return Status;
                    },
                    0,
                    0,
                    0);
            }
        }
        catch (...)
        {
            if (AllowedAccessors != nullptr)
            {
                LocalFree(AllowedAccessors);
            }

            throw;
        }

        if (AllowedAccessors != nullptr)
        {
            LocalFree(AllowedAccessors);
        }
    }
};