// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "Registry.h"

// WARNING: Explorer runs with different privileges and contexts. Shell extensions must be registered in
// HKEY_LOCAL_MACHINE\Software\Classes to work reliably. If we register in HKCU, Explorer might not see it.

// Register in-process server
HRESULT RegisterInprocServer(
    PCWSTR module,
    const CLSID& clsid,
    PCWSTR friendlyName,
    PCWSTR threadingModel)
{
    wchar_t clsidAsString[39]{}; // GUID string is always 39 chars including null terminator
    RETURN_HR_IF(E_UNEXPECTED, StringFromGUID2(clsid, clsidAsString, ARRAYSIZE(clsidAsString)) == 0);

    wil::unique_cotaskmem_string subkey;
    RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\CLSID\\%s", clsidAsString));

    // Create the CLSID key
    wil::unique_hkey regkeyClsid;
    RETURN_IF_FAILED(wil::reg::create_unique_key_nothrow(HKEY_LOCAL_MACHINE, subkey.get(), regkeyClsid, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(regkeyClsid.get(), nullptr, friendlyName));

    // Create the InprocServer32 subkey
    wil::unique_hkey regkeyInprocServer32;
    RETURN_IF_FAILED(wil::reg::create_unique_key_nothrow(regkeyClsid.get(), L"InprocServer32", regkeyInprocServer32, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(regkeyInprocServer32.get(), nullptr, module));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(regkeyInprocServer32.get(), L"ThreadingModel", threadingModel));
    return S_OK;
}

// Unregister in-process server
HRESULT UnregisterInprocServer(
    const CLSID& clsid)
{
    wchar_t clsidAsString[39]{}; // GUID string is always 39 chars including null terminator
    RETURN_HR_IF(E_UNEXPECTED, StringFromGUID2(clsid, clsidAsString, ARRAYSIZE(clsidAsString)) == 0);

    wil::unique_cotaskmem_string subkey;
    RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\CLSID\\%s", clsidAsString));

    RETURN_IF_FAILED(wil::reg::delete_tree_nothrow(HKEY_LOCAL_MACHINE, subkey.get()));
    return S_OK;
}

// Register property sheet handler for a file type.
//
// Property sheet handlers are looked up by Explorer via the file's ProgID and via
// HKCR\SystemFileAssociations\<.ext>. Registering a handler directly under
// HKCR\<.ext>\shellex\PropertySheetHandlers (the extension key itself) is NOT a
// supported lookup location for property sheet handlers and will not add the page
// to the Properties dialog. SystemFileAssociations is the correct location when
// the extension may have a variable ProgID (or none at all, as is the case for
// .msix where the default ProgID is empty and the file is associated only via
// OpenWithProgids with the AppX installer).
//
// See: https://learn.microsoft.com/windows/win32/shell/handlers#registering-shell-extension-handlers
HRESULT RegisterShellExtPropertyHandler(
    PCWSTR fileType,
    const CLSID& clsid)
{
    wchar_t clsidAsString[39]{}; // GUID string is always 39 chars including null terminator
    RETURN_HR_IF(E_UNEXPECTED, StringFromGUID2(clsid, clsidAsString, ARRAYSIZE(clsidAsString)) == 0);

    wil::unique_cotaskmem_string subkey;
    RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\SystemFileAssociations\\%s\\shellex\\PropertySheetHandlers\\MSIXPropertySheet", fileType));

    wil::unique_hkey regkey;
    RETURN_IF_FAILED(wil::reg::create_unique_key_nothrow(HKEY_LOCAL_MACHINE, subkey.get(), regkey, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(regkey.get(), nullptr, clsidAsString));
    return S_OK;
}

// Unregister property sheet handler.
//
// Cleans up both the current (SystemFileAssociations) location and the legacy
// per-extension location (HKCR\<.ext>\shellex\PropertySheetHandlers\MSIXPropertySheet)
// that earlier builds of this DLL incorrectly registered. This keeps regsvr32 /u
// idempotent and ensures stale entries are removed on upgrade.
HRESULT UnregisterShellExtPropertyHandler(
    PCWSTR fileType,
    [[maybe_unused]] const CLSID& clsid)
{
    wil::unique_process_heap_string subkey;
    RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\SystemFileAssociations\\%s\\shellex\\PropertySheetHandlers\\MSIXPropertySheet", fileType));
    RETURN_IF_FAILED(wil::reg::delete_tree_nothrow(HKEY_LOCAL_MACHINE, subkey.get()));
    RETURN_IF_FAILED(wil::reg::delete_key_nothrow(HKEY_LOCAL_MACHINE, subkey.get()));

    // Also clean up the legacy (incorrect) location that earlier builds registered.
    wil::unique_process_heap_string legacySubkey;
    RETURN_IF_FAILED(wil::str_printf_nothrow(legacySubkey, L"Software\\Classes\\%s\\shellex\\PropertySheetHandlers\\MSIXPropertySheet", fileType));
    RETURN_IF_FAILED(wil::reg::delete_tree_nothrow(HKEY_LOCAL_MACHINE, legacySubkey.get()));
    RETURN_IF_FAILED(wil::reg::delete_key_nothrow(HKEY_LOCAL_MACHINE, legacySubkey.get()));

    return S_OK;
}
