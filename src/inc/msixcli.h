// Copyright (c) Howard Kapustein
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
    PCWSTR command)
{
    wil::unique_cotaskmem_string msixAdmin;
    RETURN_IF_FAILED(FindMsixAdminExecutable(hInstance, msixAdmin));
    RETURN_HR_IF_NULL(E_NOT_SET, msixAdmin);

    wil::unique_cotaskmem_string commandLine;
    RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(commandLine, L"%s %s", msixAdmin.get(), command));
    //TODO execute the command line
    MessageBoxW(nullptr, commandLine.get() ? commandLine.get() : L"<null>", L"TODO: ExecuteMsixAdmin()", MB_OK | MB_ICONERROR);
    return S_OK;
}
}
