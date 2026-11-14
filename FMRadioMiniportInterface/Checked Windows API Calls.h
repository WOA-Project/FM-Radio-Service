#pragma once

#include "pch.h"

namespace Windows
{
    template<typename FunctionType, typename... ArgumentTypes>
    void CheckedBooleanAPICall(
        FunctionType Function,
        ArgumentTypes... Arguments)
    {
        if (Function(Arguments...))
        {
            return;
        }

        throw std::system_error(
            ::GetLastError(),
            std::system_category());
    }

    template<typename FunctionType, typename... ArgumentTypes>
    void CheckedAPICall(
        FunctionType Function,
        ArgumentTypes... Arguments)
    {
        const auto Result =
            Function(Arguments...);

        if (SUCCEEDED(Result))
        {
            return;
        }

        throw std::system_error(
            Result,
            std::system_category());
    }

    template<
        class ClassType,
        typename FunctionType,
        typename... ArgumentTypes>
    void CheckedMemberAPICall(
        ClassType* Instance,
        FunctionType Function,
        ArgumentTypes... Arguments)
    {
        if (Instance == nullptr)
        {
            throw std::invalid_argument(
                "Instance must not be null.");
        }

        const auto Result =
            (Instance->*Function)(Arguments...);

        if (SUCCEEDED(Result))
        {
            return;
        }

        throw std::system_error(
            Result,
            std::system_category());
    }

    template<typename FunctionType, typename... ArgumentTypes>
    void CheckedRPCCall(
        FunctionType Function,
        ArgumentTypes... Arguments)
    {
        const auto Result =
            Function(Arguments...);

        if (Result == RPC_S_OK)
        {
            return;
        }

        throw std::system_error(
            HRESULT_FROM_WIN32(Result),
            std::system_category());
    }

    template<typename FunctionType>
    FunctionType* GetFunctionAddress(
        HMODULE StartAddress,
        LPCSTR ExportedName)
    {
        if (StartAddress == nullptr)
        {
            throw std::invalid_argument(
                "StartAddress must not be null.");
        }

        if (ExportedName == nullptr)
        {
            throw std::invalid_argument(
                "ExportedName must not be null.");
        }

        const auto FunctionAddress =
            reinterpret_cast<FunctionType*>(
                ::GetProcAddress(
                    StartAddress,
                    ExportedName));

        if (FunctionAddress == nullptr)
        {
            throw std::system_error(
                ::GetLastError(),
                std::system_category());
        }

        return FunctionAddress;
    }
}