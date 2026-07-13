// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

// Including this file once per binary will automatically opt WIL error handling macros into calling RoOriginateError when they
// begin logging a new error.  This greatly improves the debuggability of errors that propagate before a failfast.
#include <wil/result_originate.h>

#include "ClassFactory.h"
#include "ModuleReferenceCount.h"
#include "Registry.h"

// {8B6E4D93-1364-4bb9-BA15-5FC64B56BFB4}
static const GUID CLSID_MSIXPropertySheet{ 0x8b6e4d93, 0x1364, 0x4bb9, { 0xba, 0x15, 0x5f, 0xc6, 0x4b, 0x56, 0xbf, 0xb4 } };

HINSTANCE g_hInstance{};

BOOL APIENTRY DllMain(HMODULE hmodule, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hmodule;
        DisableThreadLibraryCalls(hmodule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // Release the cached activation context created on demand by the
        // ISOLATION_AWARE_ENABLED wrappers in <commctrl.h>/<shlwapi.h>/etc.
        // Skip during process termination (reserved != nullptr) since the OS
        // is tearing the address space down anyway.
        if (reserved == nullptr)
        {
            IsolationAwareCleanup();
        }
        break;
    }

    // Give WIL a crack at DLLMain processing
    // See DLLMain() in wil/result_macros.h for why
    wil::DLLMain(hmodule, reason, reserved);

    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    RETURN_HR_IF(CLASS_E_CLASSNOTAVAILABLE, rclsid != CLSID_MSIXPropertySheet);

    ClassFactory *pClassFactory = new (std::nothrow) ClassFactory();
    RETURN_IF_NULL_ALLOC(pClassFactory);

    const auto hr{ LOG_IF_FAILED(pClassFactory->QueryInterface(riid, ppv)) };
    pClassFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    return ModuleReferenceCount::CanUnloadNow();
}

STDAPI DllRegisterServer(void)
{
    wil::unique_process_heap_string szModule;
    RETURN_IF_FAILED(wil::GetModuleFileNameW(g_hInstance, szModule));

    // Register the component
    RETURN_IF_FAILED(RegisterInprocServer(szModule.get(), CLSID_MSIXPropertySheet, L"MSIXPropertySheet.PropertySheetExt Class", L"Apartment"));

    // Register the property sheet handler for .msix files
    RETURN_IF_FAILED(RegisterShellExtPropertyHandler(L".appx", CLSID_MSIXPropertySheet));
    RETURN_IF_FAILED(RegisterShellExtPropertyHandler(L".msix", CLSID_MSIXPropertySheet));

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    // Unregister the component
    RETURN_IF_FAILED(UnregisterInprocServer(CLSID_MSIXPropertySheet));

    // Unregister the property sheet handler
    RETURN_IF_FAILED(UnregisterShellExtPropertyHandler(L".appx", CLSID_MSIXPropertySheet));
    RETURN_IF_FAILED(UnregisterShellExtPropertyHandler(L".msix", CLSID_MSIXPropertySheet));

    return S_OK;
}
