// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

// Helper functions for COM registration
HRESULT RegisterInprocServer(
    PCWSTR module,
    const CLSID& clsid,
    PCWSTR friendlyName,
    PCWSTR threadingModel);

HRESULT UnregisterInprocServer(
    const CLSID& clsid);

HRESULT RegisterShellExtPropertyHandler(
    PCWSTR fileType,
    const CLSID& clsid);

HRESULT UnregisterShellExtPropertyHandler(
    PCWSTR fileType,
    const CLSID& clsid);

namespace wil::reg
{
    inline HRESULT delete_tree_nothrow(HKEY key, PCWSTR sub_key)
    {
        const auto hr{ HRESULT_FROM_WIN32(::RegDeleteTreeW(key, sub_key)) };
        RETURN_HR_IF_MSG(hr, !::wil::reg::is_registry_not_found(hr), "hkey:%p subkey:%ls", key, sub_key);
        return S_OK;
    }

    inline HRESULT delete_key_nothrow(HKEY key, PCWSTR sub_key)
    {
        const auto hr{ HRESULT_FROM_WIN32(::RegDeleteKeyW(key, sub_key)) };
        RETURN_HR_IF_MSG(hr, !::wil::reg::is_registry_not_found(hr), "hkey:%p subkey:%ls", key, sub_key);
        return S_OK;
    }
}
