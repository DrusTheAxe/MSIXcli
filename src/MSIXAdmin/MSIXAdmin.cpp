// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <windows.h>

[[noreturn]] void UnknownArgument(PCWSTR arg)
{
    wprintf(L"Error 0x00000001: Unknown argument\n"
            L"    Full command line: '%ls'\n"
            L"Argument: %ls\n",
            GetCommandLine(), arg);
    ::ExitProcess(1);
}

HRESULT GetExeVersion(std::uint16_t& major, std::uint16_t& minor, std::uint16_t& build, std::uint16_t& revision)
{
    wil::unique_process_heap_string path;
    RETURN_IF_FAILED(wil::GetModuleFileNameW(nullptr, path));

    DWORD handle{};
    const DWORD fviSize{ ::GetFileVersionInfoSizeW(path.get(), &handle) };
    RETURN_LAST_ERROR_IF(fviSize == 0);

    wistd::unique_ptr<BYTE[]> buffer{ new (std::nothrow) BYTE[fviSize] };
    RETURN_IF_NULL_ALLOC(buffer);
    RETURN_IF_WIN32_BOOL_FALSE(::GetFileVersionInfoW(path.get(), 0, fviSize, buffer.get()));

    VS_FIXEDFILEINFO* ffi{};
    UINT ffiLength{};
    RETURN_IF_WIN32_BOOL_FALSE(::VerQueryValueW(buffer.get(), L"\\", reinterpret_cast<void**>(&ffi), &ffiLength));
    RETURN_HR_IF_NULL(E_UNEXPECTED, ffi);

    major = HIWORD(ffi->dwFileVersionMS);
    minor = LOWORD(ffi->dwFileVersionMS);
    build = HIWORD(ffi->dwFileVersionLS);
    revision = LOWORD(ffi->dwFileVersionLS);
    return S_OK;
}

HRESULT ShowLogo()
{
    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t build{};
    std::uint16_t patch{};
    RETURN_IF_FAILED(GetExeVersion(major, minor, build, patch));
    wprintf(L"msixadmin v%hu.%hu.%hu.%hu - Copyright (C) Howard Kapustein\n", major, minor, build, patch);
    return S_OK;
}

[[noreturn]] void Help()
{
    ShowLogo();
    wprintf(L"Usage:\n"
            L"  msixadmin <command> [arguments]\n"
            L"\n"
            L"Commands:\n"
            L"  certificate  Add a certificate to the system\n"
            L"  deprovision  Remove a package from the provisioning list\n"
            L"  provision    Add a package to the provisioning list\n"
            L"  version      Display version\n"
            L"\n"
            L"Run 'MSIXAdmin [command] --help' for more information on a command\n");
    ::ExitProcess(1);
}

[[noreturn]] void Command_Certificate_Help()
{
    ShowLogo();
    wprintf(L"Description:\n"
            L"  MSIX Certificate Management\n"
            L"\n"
            L"Usage:\n"
            L"  msixadmin certificate [command] [options]\n"
            L"\n"
            L"Options:\n"
            L"  -nologo, --no-logo  Do not display the startup banner or the copyright message\n"
            L"  -?, -h, --help      Show command line help\n"
            L"\n"
            L"Commands:\n"
            L"  add <FILE>  Add the certificate from the signed package file\n");
    ::ExitProcess(1);
}

[[noreturn]] void Command_Deprovision_Help()
{
    ShowLogo();
    wprintf(L"Description:\n"
            L"  MSIX Deprovisioner\n"
            L"\n"
            L"Usage:\n"
            L"  msixadmin deprovision <PACKAGEFAMILYNAME> [options]\n"
            L"\n"
            L"Arguments:\n"
            L"  <PACKAGEFAMILYNAME> The package family to deprovision\n"
            L"\n"
            L"Options:\n"
            L"  -nologo, --no-logo  Do not display the startup banner or the copyright message\n"
            L"  -?, -h, --help      Show command line help\n");
    ::ExitProcess(1);
}

[[noreturn]] void Command_Provision_Help()
{
    ShowLogo();
    wprintf(L"Description:\n"
            L"  MSIX Provisioner\n"
            L"\n"
            L"Usage:\n"
            L"  msixadmin provision <PACKAGEFAMILYNAME> [options]\n"
            L"\n"
            L"Arguments:\n"
            L"  <PACKAGEFAMILYNAME> The package family to provision\n"
            L"\n"
            L"Options:\n"
            L"  -nologo, --no-logo  Do not display the startup banner or the copyright message\n"
            L"  -?, -h, --help      Show command line help\n");
    ::ExitProcess(1);
}

[[noreturn]] void Command_Version_Help()
{
    ShowLogo();
    wprintf(L"Description:\n"
            L"  Version information\n"
            L"\n"
            L"Usage:\n"
            L"  msixadmin version [options]\n"
            L"\n"
            L"Options:\n"
            L"  -nologo, --no-logo  Do not display the startup banner or the copyright message\n"
            L"  -?, -h, --help      Show command line help\n");
    ::ExitProcess(1);
}

HRESULT Command_Certificate(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Command_Certificate_Help();
    }

    PCWSTR action{ argv[2] };
    PCWSTR filename{ argv[3] };
    if (CompareStringOrdinal(action, -1, L"add", -1, FALSE) != CSTR_EQUAL)
    {
        UnknownArgument(action);
    }

    bool logo{ true };

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Command_Certificate_Help();
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else
        {
            UnknownArgument(arg);
        }
    }
    if (argn < argc)
    {
        UnknownArgument(argv[argn]);
    }

    if (logo)
    {
        ShowLogo();
    }

    //TODO
    (void)action;
    (void)filename;
    return S_OK;
}

HRESULT Command_Deprovision(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Command_Deprovision_Help();
    }

    PCWSTR packageFamilyName{ argv[2] };

    bool logo{ true };

    int argn{ 3 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Command_Deprovision_Help();
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else
        {
            UnknownArgument(arg);
        }
    }
    if (argn < argc)
    {
        UnknownArgument(argv[argn]);
    }

    if (logo)
    {
        ShowLogo();
    }

    //TODO
    (void)packageFamilyName;
    return S_OK;
}

HRESULT Command_Provision(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Command_Provision_Help();
    }

    PCWSTR packageFamilyName{ argv[2] };

    bool logo{ true };

    int argn{ 3 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Command_Deprovision_Help();
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else
        {
            UnknownArgument(arg);
        }
    }
    if (argn < argc)
    {
        UnknownArgument(argv[argn]);
    }

    if (logo)
    {
        ShowLogo();
    }

    //TODO
    (void)packageFamilyName;
    return S_OK;
}

HRESULT Command_Version(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        Command_Version_Help();
    }

    bool logo{ true };

    int argn{ 2 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Command_Deprovision_Help();
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else
        {
            UnknownArgument(arg);
        }
    }
    if (argn < argc)
    {
        UnknownArgument(argv[argn]);
    }

    if (logo)
    {
        ShowLogo();
    }

    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t build{};
    std::uint16_t patch{};
    RETURN_IF_FAILED(GetExeVersion(major, minor, build, patch));
    wprintf(L"%hu.%hu.%hu.%hu\n", major, minor, build, patch);
    return S_OK;
}

int wmain(int argc, wchar_t* argv[])
{
    auto com_init{ wil::CoInitializeEx_failfast() };

    if (argc < 2)
    {
        Help();
    }

    // Parse the command line
    int argn{ 1 };
    PCWSTR arg{ argv[argn] };
    if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help();
    }

    if (CompareStringOrdinal(arg, -1, L"certificate", -1, FALSE) == CSTR_EQUAL)
    {
        return Command_Deprovision(argc, argv);
    }
    else if (CompareStringOrdinal(arg, -1, L"deprovision", -1, FALSE) == CSTR_EQUAL)
    {
        return Command_Deprovision(argc, argv);
    }
    else if (CompareStringOrdinal(arg, -1, L"provision", -1, FALSE) == CSTR_EQUAL)
    {
        return Command_Provision(argc, argv);
    }
    else if (CompareStringOrdinal(arg, -1, L"version", -1, FALSE) == CSTR_EQUAL)
    {
        return Command_Version(argc, argv);
    }
    else
    {
        Help();
    }
}
