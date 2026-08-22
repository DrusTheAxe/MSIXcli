// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace msixcli
{
inline HRESULT FindMsixAdminExecutable(
    HINSTANCE hInstance,
    wil::unique_cotaskmem_string& msixAdminExecutable)
{
    msixAdminExecutable.reset();

    // msixadmin.exe is colocated with the caller...
    //   1. ...in the same directory
    //   2. ...in the relative path ..\msixadmin
    wil::unique_cotaskmem_string path;
    RETURN_IF_FAILED(wil::GetModuleFileNameW(hInstance, path));
    PWSTR lastSlash{ wcsrchr(path.get(), L'\\') };
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
    wil::unique_cotaskmem_string candidate;
    RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(candidate, L"%smsixadmin.exe", path.get()));
    if (GetFileAttributesW(candidate.get()) == INVALID_FILE_ATTRIBUTES)
    {
        RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(candidate, L"%s..\\msixadmin\\msixadmin.exe", path.get()));
        if (GetFileAttributesW(candidate.get()) == INVALID_FILE_ATTRIBUTES)
        {
            return S_OK;
        }
    }
    msixAdminExecutable = wistd::move(candidate);
    return S_OK;
}

inline HRESULT ExecuteMsixAdmin(
    HINSTANCE hInstance,
    PCWSTR command,
    HRESULT& exitCode)
{
    exitCode = 0;

    wil::unique_cotaskmem_string msixAdmin;
    RETURN_IF_FAILED(FindMsixAdminExecutable(hInstance, msixAdmin));
    RETURN_HR_IF_NULL(E_NOT_SET, msixAdmin);

    // Launch msixadmin.exe via ShellExecuteEx so it can elevate. msixadmin.exe's Fusion
    // manifest declares requestedExecutionLevel=requireAdministrator (see the project's
    // <UACExecutionLevel>RequireAdministrator</UACExecutionLevel>), but that only declares
    // the requirement -- it does not by itself produce a UAC prompt. CreateProcess never
    // auto-elevates: from a Medium-IL caller it fails with ERROR_ELEVATION_REQUIRED and no
    // prompt is shown. Only ShellExecuteEx routes through the AppInfo service, which reads
    // the manifest, shows the UAC consent dialog, and spawns the elevated process. The "runas"
    // verb makes the intent explicit (the manifest would elevate under the default verb too).
    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = msixAdmin.get();
    sei.lpParameters = command;
    sei.nShow = SEE_MASK_NO_CONSOLE; //TODO SEE_MASK_FLAG_NO_UI
    RETURN_IF_WIN32_BOOL_FALSE(::ShellExecuteExW(&sei));
    RETURN_HR_IF_NULL(E_UNEXPECTED, sei.hProcess);
    wil::unique_process_handle process{ sei.hProcess };
    RETURN_LAST_ERROR_IF(::WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0);
    RETURN_IF_WIN32_BOOL_FALSE(::GetExitCodeProcess(process.get(), reinterpret_cast<DWORD*>(&exitCode)));
    return S_OK;
}

inline HRESULT ExecuteMsixAdminUI(
    HINSTANCE hInstance,
    PCWSTR command,
    HRESULT& exitCode,
    HWND hwndParent,
    PCWSTR verb)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, command);

    const HRESULT hr{ LOG_IF_FAILED_MSG(ExecuteMsixAdmin(hInstance, command, exitCode), "%ls", command) };
    if (FAILED(hr))
    {
        wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };

        PCWSTR caption{ L"MSIX Property Page" };

        wil::unique_cotaskmem_string text;
        PCWSTR formatter{ L"Error 0x%08X %ls\n\n%ls\n\nCOMMAND: %ls" };
        RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(text, formatter, hr, verb, message ? message.get() : L"<null>", command));
        ::MessageBoxW(hwndParent, text ? text.get() : L"<null>", caption, MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }
    return hr;
}
}
