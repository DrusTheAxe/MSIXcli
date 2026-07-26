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

HRESULT GetExePath(wil::unique_process_heap_string& path)
{
    RETURN_IF_FAILED(wil::GetModuleFileNameW(nullptr, path));
    PCWSTR lastPathSegment{ wil::find_last_path_segment(path.get()) };
    const auto offset{ lastPathSegment - path.get() };
    path.get()[offset] = L'\0';
    return S_OK;
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

class PackageX
{
public:
    PackageX() = default;

public:
    static HRESULT Make(ABI::Windows::ApplicationModel::IPackage* package, PackageX& packageX)
    {
        packageX.m_package = package;
        RETURN_IF_FAILED(package->QueryInterface(IID_PPV_ARGS(packageX.m_package2.put())));
        RETURN_IF_FAILED(package->QueryInterface(IID_PPV_ARGS(packageX.m_package3.put())));
        RETURN_IF_FAILED(package->QueryInterface(IID_PPV_ARGS(packageX.m_package4.put())));
        RETURN_IF_FAILED(package->QueryInterface(IID_PPV_ARGS(packageX.m_package8.put())));
        RETURN_IF_FAILED(package->QueryInterface(IID_PPV_ARGS(packageX.m_package9.put())));
        return S_OK;
    }

    ABI::Windows::ApplicationModel::IPackage* operator->()
    {
        return m_package.get();
    }

    ABI::Windows::ApplicationModel::IPackage2* package2()
    {
        return m_package2.get();
    }

    ABI::Windows::ApplicationModel::IPackage3* package3()
    {
        return m_package3.get();
    }

    ABI::Windows::ApplicationModel::IPackage4* package4()
    {
        return m_package4.get();
    }

    ABI::Windows::ApplicationModel::IPackage8* package8()
    {
        return m_package8.get();
    }

    ABI::Windows::ApplicationModel::IPackage9* package9()
    {
        return m_package9.get();
    }

private:
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> m_package;
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage2> m_package2;
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage3> m_package3;
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage4> m_package4;
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage8> m_package8;
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage9> m_package9;
};

void PrintPackageKeyValueError(PCWSTR key, HRESULT hr)
{
    wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
    wprintf(L"%-30ls : ***ERROR 0x%08X %ls", key, hr, message.get());
}

void PrintPackageValue(PCWSTR key, HRESULT hr, PCWSTR value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        wprintf(L"%-30ls : %ls\n", key, value);
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const wil::unique_hstring& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        PrintPackageValue(key, hr, WindowsGetStringRawBuffer(value.get(), nullptr));
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::ApplicationModel::PackageVersion& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        const std::uint64_t version{ (static_cast<std::uint64_t>(value.Major) << 48) |
                                     (static_cast<std::uint64_t>(value.Minor) << 32) |
                                     (static_cast<std::uint64_t>(value.Build) << 16) |
                                     static_cast<std::uint64_t>(value.Revision) };
        wprintf(L"%-30ls : %hu.%hu.%hu.%hu  (0x%llX)\n", key, value.Major, value.Minor, value.Build, value.Revision, version);
    }
}

constexpr PCWSTR ToString(const ABI::Windows::System::ProcessorArchitecture architecture)
{
    switch (architecture)
    {
        case ABI::Windows::System::ProcessorArchitecture_X86:        return L"X86";
        case ABI::Windows::System::ProcessorArchitecture_Arm:        return L"ARM";
        case ABI::Windows::System::ProcessorArchitecture_X64:        return L"X64";
        case ABI::Windows::System::ProcessorArchitecture_Neutral:    return L"Neutral";
        case ABI::Windows::System::ProcessorArchitecture_Arm64:      return L"ARM64";
        case ABI::Windows::System::ProcessorArchitecture_X86OnArm64: return L"x86 on ARM64 (CHPE)";
        case ABI::Windows::System::ProcessorArchitecture_Unknown:    return L"Unknown";
        default: return nullptr;
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::System::ProcessorArchitecture& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        PCWSTR architecture{ ToString(value) };
        if (architecture)
        {
            wprintf(L"%-30ls : %ls\n", key, architecture);
        }
        else
        {
            wprintf(L"%-30ls : ??? (%d)\n", key, value);
        }
    }
}

constexpr PCWSTR ToString(const ABI::Windows::Management::Deployment::PackageTypes packageType)
{
    switch (packageType)
    {
        case ABI::Windows::Management::Deployment::PackageTypes_Framework: return L"Framework";
        case ABI::Windows::Management::Deployment::PackageTypes_Resource:  return L"Resource";
        case ABI::Windows::Management::Deployment::PackageTypes_Optional:  return L"Optional";
        case ABI::Windows::Management::Deployment::PackageTypes_Bundle:    return L"Bundle";
        case ABI::Windows::Management::Deployment::PackageTypes_Main:      return L"Main";
        default: FAIL_FAST_HR_MSG(E_UNEXPECTED, "Unknown PackageType: %u", static_cast<std::uint32_t>(packageType));
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::Management::Deployment::PackageTypes& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        PCWSTR packageType{ ToString(value) };
        wprintf(L"%-30ls : %ls\n", key, packageType);
    }
}

HRESULT ToPackageType(
    PackageX& package,
    ABI::Windows::Management::Deployment::PackageTypes& packageType)
{
    boolean value{};
    RETURN_IF_FAILED(package->get_IsFramework(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Framework;
    }

    RETURN_IF_FAILED(package.package2()->get_IsResourcePackage(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Resource;
    }

    RETURN_IF_FAILED(package.package4()->get_IsOptional(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Optional;
    }

    RETURN_IF_FAILED(package.package2()->get_IsBundle(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Bundle;
    }

    packageType = ABI::Windows::Management::Deployment::PackageTypes_Main;
    return S_OK;
}

enum class Status
{
    Unknown = -1,

    Ok = 0,

    NeedsRemediation     = 0x00010000,
    DependencyIssue      = 0x00010001,
    LicenseIssue         = 0x00010002,
    Modified             = 0x00010004,
    Tampered             = 0x00010008,

    NotAvailable         = 0x00020000,
    Disabled             = 0x00020001,
    DataOffline          = 0x00020002,
    PackageOffline       = 0x00020004,

    Servicing            = 0x00040000,
    DeploymentInProgress = 0x00040001,

    IsPartiallyStaged    = 0x00100000,
};
DEFINE_ENUM_FLAG_OPERATORS(Status)

void PrintPackageValue(PCWSTR key, HRESULT hr, wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageStatus>& packageStatus)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        boolean value{};
        if (FAILED_LOG(hr = packageStatus->VerifyIsOK(&value)))
        {
            PrintPackageKeyValueError(key, hr);
        }
        else if (value)
        {
            wprintf(L"%-30ls : OK\n", key);
        }
        else
        {
            Status status{ Status::Ok };
            if (FAILED_LOG(hr = packageStatus->get_NeedsRemediation(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                status |= Status::NeedsRemediation;
                if (FAILED_LOG(hr = packageStatus->get_DependencyIssue(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::DependencyIssue;
                if (FAILED_LOG(hr = packageStatus->get_LicenseIssue(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::LicenseIssue;
                if (FAILED_LOG(hr = packageStatus->get_Modified(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::Modified;
                if (FAILED_LOG(hr = packageStatus->get_Tampered(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::Tampered;
            }

            if (FAILED_LOG(hr = packageStatus->get_NotAvailable(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                status |= Status::NotAvailable;
                if (FAILED_LOG(hr = packageStatus->get_Disabled(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::Disabled;
                if (FAILED_LOG(hr = packageStatus->get_DataOffline(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::DataOffline;
                if (FAILED_LOG(hr = packageStatus->get_PackageOffline(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::PackageOffline;
            }

            if (FAILED_LOG(hr = packageStatus->get_Servicing(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                status |= Status::Servicing;
                if (FAILED_LOG(hr = packageStatus->get_DeploymentInProgress(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                status |= Status::DeploymentInProgress;
            }

            wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageStatus2> packageStatus2;
            if (FAILED_LOG(hr = packageStatus->QueryInterface(IID_PPV_ARGS(packageStatus2.put()))))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (FAILED_LOG(hr = packageStatus2->get_IsPartiallyStaged(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            status |= Status::IsPartiallyStaged;

            wprintf(L"%-30ls :", key);

            if (WI_IsFlagSet(status, Status::NeedsRemediation))
            {
                wprintf(L" NeedsRemediation[");
                bool needDelimiter{};
                if (WI_AreAllFlagsSet(status, Status::DependencyIssue))
                {
                    wprintf(L"DependencyIssue");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::LicenseIssue))
                {
                    if (needDelimiter)
                    {
                        wprintf(L"','");
                    }
                    wprintf(L"LicenseIssue");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::Modified))
                {
                    if (needDelimiter)
                    {
                        wprintf(L"','");
                    }
                    wprintf(L"Modified");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::Tampered))
                {
                    if (needDelimiter)
                    {
                        wprintf(L"','");
                    }
                    wprintf(L"Tampered");
                }
                wprintf(L"]");
            }

            if (WI_IsFlagSet(status, Status::NotAvailable))
            {
                wprintf(L" NotAvailable[");
                bool needDelimiter{};
                if (WI_AreAllFlagsSet(status, Status::Disabled))
                {
                    wprintf(L"Disabled");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::DataOffline))
                {
                    if (needDelimiter)
                    {
                        wprintf(L"','");
                    }
                    wprintf(L"DataOffline");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::PackageOffline))
                {
                    if (needDelimiter)
                    {
                        wprintf(L"','");
                    }
                    wprintf(L"PackageOffline");
                }
                wprintf(L"]");
            }

            if (WI_IsFlagSet(status, Status::Servicing))
            {
                wprintf(L" Servicing[");
                if (WI_AreAllFlagsSet(status, Status::DeploymentInProgress))
                {
                    wprintf(L"DeploymentInProgress");
                }
                wprintf(L"]");
            }

            if (WI_IsFlagSet(status, Status::IsPartiallyStaged))
            {
                wprintf(L" IsPartiallyStaged");
            }
        }
    }
}

constexpr PCWSTR ToString(const ::PackageOrigin packageOrigin)
{
    switch (packageOrigin)
    {
        case ::PackageOrigin_Unknown:           return L"Unknown";
        case ::PackageOrigin_Unsigned:          return L"Unsigned";
        case ::PackageOrigin_Inbox:             return L"Inbox";
        case ::PackageOrigin_Store:             return L"Store";
        case ::PackageOrigin_DeveloperUnsigned: return L"DeveloperUnsigned";
        case ::PackageOrigin_DeveloperSigned:   return L"DeveloperSigned";
        case ::PackageOrigin_LineOfBusiness:    return L"LineOfBusiness";
        default: return nullptr;
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ::PackageOrigin value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        PCWSTR packageOrigin{ ToString(value) };
        if (!packageOrigin)
        {
            wprintf(L"%-30ls : ??? %d\n", key, value);
        }
        else
        {
            wprintf(L"%-30ls : %ls\n", key, packageOrigin);
        }
    }
}

constexpr PCWSTR ToString(const ABI::Windows::ApplicationModel::PackageSignatureKind signatureKind)
{
    switch (signatureKind)
    {
        case ABI::Windows::ApplicationModel::PackageSignatureKind_None:       return L"Unknown";
        case ABI::Windows::ApplicationModel::PackageSignatureKind_Developer:  return L"Unsigned";
        case ABI::Windows::ApplicationModel::PackageSignatureKind_Enterprise: return L"Inbox";
        case ABI::Windows::ApplicationModel::PackageSignatureKind_Store:      return L"Store";
        case ABI::Windows::ApplicationModel::PackageSignatureKind_System:     return L"DeveloperUnsigned";
        default: return nullptr;
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::ApplicationModel::PackageSignatureKind& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        PCWSTR signatureKind{ ToString(value) };
        if (!signatureKind)
        {
            wprintf(L"%-30ls : ??? %d\n", key, value);
        }
        else
        {
            wprintf(L"%-30ls : %ls\n", key, signatureKind);
        }
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::Foundation::DateTime& value, const bool localTimeZone)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        wil::unique_cotaskmem_string dateTime;
        hr = wil::filetime::format_filetime(wil::filetime::from_int64(value.UniversalTime), localTimeZone, dateTime);
        wprintf(L"%-30ls : %ls\n", key, dateTime.get());
    }
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const boolean& value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        wprintf(L"%-30ls : %ls\n", key, value ? L"Yes" : L"No");
    }
}

void PrintPackage(
    PackageX& package,
    ABI::Windows::ApplicationModel::IPackageId* packageId,
    const bool localTimeZone)
{
    wil::unique_hstring string;
    HRESULT hr{ LOG_IF_FAILED(packageId->get_FullName(wil::out_param(string))) };
    PCWSTR packageFullName{ WindowsGetStringRawBuffer(string.get(), nullptr) };
    PrintPackageValue(L"PackageFullName", hr, packageFullName);
    hr = LOG_IF_FAILED(packageId->get_FamilyName(wil::out_param(string)));
    PrintPackageValue(L"PackageFamilyName", hr, string);
    hr = LOG_IF_FAILED(packageId->get_Name(wil::out_param(string)));
    PrintPackageValue(L"Name", hr, string);
    ABI::Windows::ApplicationModel::PackageVersion version{};
    PrintPackageValue(L"Version", LOG_IF_FAILED(packageId->get_Version(&version)), version);
    ABI::Windows::System::ProcessorArchitecture architecture{ ABI::Windows::System::ProcessorArchitecture_Unknown };
    PrintPackageValue(L"Architecture", LOG_IF_FAILED(packageId->get_Architecture(&architecture)), architecture);
    hr = LOG_IF_FAILED(packageId->get_ResourceId(wil::out_param(string)));
    PrintPackageValue(L"ResourceId", hr, string);
    hr = LOG_IF_FAILED(packageId->get_Publisher(wil::out_param(string)));
    PrintPackageValue(L"Publisher", hr, string);
    hr = LOG_IF_FAILED(packageId->get_PublisherId(wil::out_param(string)));
    PrintPackageValue(L"PublisherId", hr, string);

    ABI::Windows::Management::Deployment::PackageTypes packageType{};
    PrintPackageValue(L"PackageType", LOG_IF_FAILED(ToPackageType(package, packageType)), packageType);
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageStatus> status;
    PrintPackageValue(L"Status", LOG_IF_FAILED(package.package3()->get_Status(status.put())), status);

    hr = LOG_IF_FAILED(package.package2()->get_DisplayName(wil::out_param(string)));
    PrintPackageValue(L"DisplayName", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_PublisherDisplayName(wil::out_param(string)));
    PrintPackageValue(L"PublisherDisplayName", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_Description(wil::out_param(string)));
    PrintPackageValue(L"Description", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_EffectivePath(wil::out_param(string)));
    PrintPackageValue(L"EffectivePath", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_EffectiveExternalPath(wil::out_param(string)));
    PrintPackageValue(L"EffectiveExternalPath", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_InstalledPath(wil::out_param(string)));
    PrintPackageValue(L"InstalledPath", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_MutablePath(wil::out_param(string)));
    PrintPackageValue(L"MutablePath", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_MachineExternalPath(wil::out_param(string)));
    PrintPackageValue(L"MachineExternalPath", hr, string);
    hr = LOG_IF_FAILED(package.package8()->get_UserExternalPath(wil::out_param(string)));
    PrintPackageValue(L"UserExternalPath", hr, string);
    ABI::Windows::Foundation::DateTime dateTime{};
    PrintPackageValue(L"InstalledDate", LOG_IF_FAILED(package.package3()->get_InstalledDate(&dateTime)), dateTime, localTimeZone);
    //TODO PrintPackageValue(L"Dependencies
    boolean boolean{};
    PrintPackageValue(L"IsDevelopmentMode", LOG_IF_FAILED(package.package2()->get_IsDevelopmentMode(&boolean)), boolean);
    PackageOrigin packageOrigin{};
    hr = LOG_IF_FAILED(::GetStagedPackageOrigin(packageFullName, &packageOrigin));
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(L"IsSigned", hr);
        PrintPackageKeyValueError(L"PackageOrigin", hr);
    }
    else
    {
        const auto isSigned{ (packageOrigin == ::PackageOrigin_Inbox) ||
                             (packageOrigin == ::PackageOrigin_Store) ||
                             (packageOrigin == ::PackageOrigin_DeveloperSigned) ||
                             (packageOrigin == ::PackageOrigin_LineOfBusiness) };
        PrintPackageValue(L"IsSigned", hr, isSigned);
        PrintPackageValue(L"PackageOrigin", hr, packageOrigin);
    }
    ABI::Windows::ApplicationModel::PackageSignatureKind signatureKind{};
    PrintPackageValue(L"PackageSignatureKind", LOG_IF_FAILED(package.package4()->get_SignatureKind(&signatureKind)), signatureKind);
    PrintPackageValue(L"IsStub", LOG_IF_FAILED(package.package8()->get_IsStub(&boolean)), boolean);
    hr = LOG_IF_FAILED(package.package9()->get_SourceUriSchemeName(wil::out_param(string)));
    PrintPackageValue(L"SourceUriSchemeName", hr, string);
}

HRESULT ShowLogo()
{
    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t build{};
    std::uint16_t patch{};
    RETURN_IF_FAILED(GetExeVersion(major, minor, build, patch));
    if (patch != 0)
    {
        wprintf(L"msixadmin v%hu.%hu.%hu.%hu - Copyright (C) Howard Kapustein\n", major, minor, build, patch);
    }
    else
    {
        wprintf(L"msixadmin v%hu.%hu.%hu - Copyright (C) Howard Kapustein\n", major, minor, build);
    }
    return S_OK;
}

[[noreturn]] void Help(PCWSTR help = nullptr)
{
    ShowLogo();
    if (help)
    {
        wprintf(help);
    }
    else
    {
        wprintf(L"Usage:\n"
                L"  msixadmin <command> [arguments]\n"
                L"\n"
                L"Commands:\n"
                L"  certificate  Certificate management\n"
                L"  provision    Provision management\n"
                L"  shortcut     Shortcut operations\n"
                L"  tool         Install or manage tools that extend the MSIX experience\n"
                L"  version      Display version\n"
                L"\n"
                L"Run 'MSIXAdmin [command] --help' for more information on a command\n");
    }
    ::ExitProcess(1);
}

constexpr PCWSTR help_Command_Certificate{
    L"Description:\n"
    L"  MSIX Certificate Management\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin certificate [command] [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add <FILE>      Add the certificate from the signed package file\n"
    L"  exists <FILE*>  Check if the certificate from the signed package file exists\n"
    L"  list <FILE>     List the certificate from the signed package file\n"
    L"  remove <FILE*>  Remove the certificate per the signed package file\n"
    L"\n"
    L"NOTE: <FILE*> can be '0x<HEX>' to specify a certificate by its SHA-256 thumbprint\n"
};

constexpr PCWSTR help_Command_Provision{
    L"Description:\n"
    L"  View or modify the provisioned list\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin provision <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add <PACKAGEFAMILYNAME>     Add a package family to the provisioning list\n"
    L"  list                        Display the currently provisioned package families\n"
    L"  remove <PACKAGEFAMILYNAME>  Remove a package family from the provisioning list\n"
};

constexpr PCWSTR help_Command_Provision_Add{
    L"Description:\n"
    L"  Add a package family to the provisioning list\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin provision add <PACKAGEFAMILYNAME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --defer-registration  Defer automatic registration\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Provision_List{
    L"Description:\n"
    L"  Display the currently provisioned package families\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin provision list [options]\n"
    L"\n"
    L"Options:\n"
    L"  --format=<FORMAT>      Display package format (default=packagefamilyname)\n"
    L"  --glob=<PATTERN>       Display package families matching PATTERN (*,? wildcards)\n"
    L"  --timezone=<TIMEZONE>  Display timezone for timestamps (default=local)\n"
    L"  --no-summary           Do not display summary information\n"
    L"  -nologo, --no-logo     Do not display startup banner or copyright message\n"
    L"  -?, -h, --help         Show command line help\n"
    L""
    L"FORMAT Values:\n"
    L"  full                   Display the full package information\n"
    L"  packagefamilyname      Display the package family name\n"
    L"TIMEZONE\n"
    L"  local                  Display timestamps as local time zone\n"
    L"  utc                    Display timestamps as UTC\n"
};

constexpr PCWSTR help_Command_Provision_Remove{
    L"Description:\n"
    L"  Remove a package family from the provisioning list\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin provision remove <PACKAGEFAMILYNAME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Shortcut_Add{
    L"Description:\n"
    L"  Create a shortcut (.LNK file) to run a target command\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin shortcut add <FILE> <TARGET> [options]\n"
    L"\n"
    L"Arguments:\n"
    L"  <FILE>    The shortcut file (*.url for an Internet shortcut)\n"
    L"  <TARGET>  The target of the shortcut (file, appUserModelID or URL)\n"
    L"\n"
    L"Options:\n"
    L"  --arguments=<ARGUMENTS>      Set the arguments\n"
    L"  --description=<DESCRIPTION>  Set the description\n"
    L"  --icon=<FILE>[,INDEX]        Set the icon (INDEX default = 0)\n"
    L"  --run-as-administrator       Run as administrator\n"
    L"  --show=<normal|min|max>      Set the initial window show state\n"
    L"  --target=<app|file|url>      TARGET is an application (ApplicationUserModelID), file or URL\n"
    L"  --working-directory=<PATH>   Set the working directory\n"
    L"  -nologo, --no-logo           Do not display startup banner or copyright message\n"
    L"  -?, -h, --help               Show command line help\n"
    L"\n"
    L"NOTE: URLs only support the --icon\n"
};

constexpr PCWSTR help_Command_Shortcut{
    L"Description:\n"
    L"  Manage Shortcut\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin shortcut <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add  Create a shortcut (.LNK file)\n"
};

constexpr PCWSTR help_Command_Tool{
    L"Description:\n"
    L"  Install or manage tools that extend the MSIX experience\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin tool <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  propertysheet  Manage the MSIX property sheet extension\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet{
    L"Description:\n"
    L"  Manage the MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin tool propertysheet <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  install    Install the MSIX property sheet extension\n"
    L"  list       Display the currently installed MSIX property sheet extension\n"
    L"  uninstall  Uninstall the MSIX property sheet extension\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet_Install{
    L"Description:\n"
    L"  Install the MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin tool propertysheet install [options]\n"
    L"\n"
    L"Options:\n"
    L"  --path=<FILE>         The path to the MSIX property sheet DLL (default = GetPath(msixadmin.exe) + \\MSIXPropertySheet.dll)\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet_List{
    L"Description:\n"
    L"  Display the installed MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin tool propertysheet list [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet_Uninstall{
    L"Description:\n"
    L"  Uninstall the MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin tool propertysheet uninstall [options]\n"
    L"\n"
    L"Options:\n"
    L"  --path=<FILE>         The path to the MSIX property sheet DLL (default = GetPath(msixadmin.exe) + \\MSIXPropertySheet.dll)\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Version{
    L"Description:\n"
    L"  Version information\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin version [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
};

HRESULT Command_Certificate_Add(PCWSTR filename)
{
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
    MSIX::Signing::AddResult result{};
    RETURN_IF_FAILED_MSG(MSIX::Signing::AddCertificate(packageReader.get(), result), "%ls", filename);

    if (result == MSIX::Signing::AddResult::Installed)
    {
        wprintf(L"Certificate from '%ls' added to the system\n", filename);
        return S_OK;
    }
    else if (result == MSIX::Signing::AddResult::AlreadyTrusted)
    {
        wprintf(L"Certificate from '%ls' is already trusted\n", filename);
        return S_FALSE;
    }
    else
    {
        wprintf(L"'%ls' is not signed\n", filename);
        return SCARD_E_NO_SUCH_CERTIFICATE;
    }
}

HRESULT Command_Certificate_Exists(PCWSTR filename)
{
    bool isInstalled{};
    if (wil::string_starts_with(filename, L"0x"))
    {
        const size_t thumbprintLength{ wcslen(filename + 2) };
        RETURN_HR_IF(E_INVALIDARG, (thumbprintLength % 2) != 0);
        const size_t thumbprintSize{ thumbprintLength / 2 };
        wistd::unique_ptr<BYTE[]> thumbprint{ new (std::nothrow) BYTE[thumbprintSize] };
        RETURN_IF_NULL_ALLOC(thumbprint);
        RETURN_IF_FAILED(wil::parse_hexstring(filename + 2, thumbprintSize, thumbprint.get()));
        RETURN_IF_FAILED_MSG(MSIX::Signing::IsCertificateInstalled(thumbprintSize, thumbprint.get(), isInstalled), "%ls", filename);
        wprintf(L"Certificate with thumbprint '%ls' is%ls installed\n", filename + 2, isInstalled ? L"" : L" not");
    }
    else
    {
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(MSIX::Signing::IsCertificateInstalled(packageReader.get(), isInstalled), "%ls", filename);
        wprintf(L"Certificate from '%ls' is%ls installed\n", filename, isInstalled ? L"" : L" not");
    }

    return isInstalled ? S_OK : S_FALSE;
}

// Writes a byte blob as an uppercase hex string. Set bigEndian to reverse the
// bytes for little-endian fields such as a certificate serial number.
void PrintCertificateHexBytes(const BYTE* data, DWORD size, bool bigEndian)
{
    for (DWORD i = 0; i < size; ++i)
    {
        wprintf(L"%02X", data[bigEndian ? (size - 1 - i) : i]);
    }
}

// Writes a certificate name (Subject or Issuer) as an X.500 distinguished name
HRESULT PrintCertificateName(PCWSTR label, PCERT_NAME_BLOB nameBlob)
{
    const DWORD chars{ CertNameToStrW(X509_ASN_ENCODING, nameBlob, CERT_X500_NAME_STR, nullptr, 0) };
    if (chars <= 1)
    {
        wprintf(L"%ls(none)\n", label);
        return S_OK;
    }
    auto buffer{ wil::make_unique_cotaskmem_nothrow<WCHAR[]>(chars) };
    RETURN_IF_NULL_ALLOC(buffer);
    CertNameToStrW(X509_ASN_ENCODING, nameBlob, CERT_X500_NAME_STR, buffer.get(), chars);
    wprintf(L"%ls%ls\n", label, buffer.get());
    return S_OK;
}

// Writes a certificate validity timestamp (stored in UTC) in ISO-like form
void PrintCertificateFileTime(PCWSTR label, const FILETIME& fileTime)
{
    SYSTEMTIME utc{};
    if (FileTimeToSystemTime(&fileTime, &utc))
    {
        wprintf(L"%ls%04hu-%02hu-%02hu %02hu:%02hu:%02hu UTC\n", label,
                utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
    }
    else
    {
        wprintf(L"%ls(unknown)\n", label);
    }
}

// Writes whether the certificate is valid for code signing and lists its
// Enhanced Key Usages (EKUs). A certificate carrying no EKU restriction is
// valid for all uses.
HRESULT PrintCertificateEnhancedKeyUsage(PCCERT_CONTEXT certificate)
{
    DWORD usageSize{};
    wil::unique_cotaskmem_ptr<BYTE[]> usageBuffer;
    PCERT_ENHKEY_USAGE usage{};
    if (CertGetEnhancedKeyUsage(certificate, 0, nullptr, &usageSize) && (usageSize > 0))
    {
        usageBuffer = wil::make_unique_cotaskmem_nothrow<BYTE[]>(usageSize);
        RETURN_IF_NULL_ALLOC(usageBuffer);
        if (CertGetEnhancedKeyUsage(certificate, 0, reinterpret_cast<PCERT_ENHKEY_USAGE>(usageBuffer.get()), &usageSize))
        {
            usage = reinterpret_cast<PCERT_ENHKEY_USAGE>(usageBuffer.get());
        }
    }

    bool codeSigning{ false };
    if (usage)
    {
        for (DWORD i = 0; i < usage->cUsageIdentifier; ++i)
        {
            const PCSTR oid{ usage->rgpszUsageIdentifier[i] };
            if (oid && (strcmp(oid, szOID_PKIX_KP_CODE_SIGNING) == 0))
            {
                codeSigning = true;
                break;
            }
        }
    }
    wprintf(L"  Code signing: %ls\n", codeSigning ? L"Yes" : L"No");

    if (!usage || (usage->cUsageIdentifier == 0))
    {
        wprintf(L"  EKU(s):       (none; valid for all uses)\n");
        return S_OK;
    }

    wprintf(L"  EKU(s):\n");
    for (DWORD i = 0; i < usage->cUsageIdentifier; ++i)
    {
        const PCSTR oid{ usage->rgpszUsageIdentifier[i] };
        if (!oid)
        {
            continue;
        }
        const PCCRYPT_OID_INFO oidInfo{ CryptFindOIDInfo(CRYPT_OID_INFO_OID_KEY,
            const_cast<PSTR>(oid), CRYPT_ENHKEY_USAGE_OID_GROUP_ID) };
        if (oidInfo && oidInfo->pwszName)
        {
            wprintf(L"    %ls (%hs)\n", oidInfo->pwszName, oid);
        }
        else
        {
            wprintf(L"    %hs\n", oid);
        }
    }
    return S_OK;
}

// Writes the interesting fields of a certificate to the console
HRESULT PrintCertificate(PCCERT_CONTEXT certificate)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, certificate);
    const PCERT_INFO info{ certificate->pCertInfo };

    RETURN_IF_FAILED(PrintCertificateName(L"  Subject:      ", &info->Subject));
    RETURN_IF_FAILED(PrintCertificateName(L"  Issuer:       ", &info->Issuer));

    wprintf(L"  Serial:       ");
    PrintCertificateHexBytes(info->SerialNumber.pbData, info->SerialNumber.cbData, true);
    wprintf(L"\n");

    BYTE thumbprint[20]{};
    DWORD thumbprintSize{ sizeof(thumbprint) };
    if (CertGetCertificateContextProperty(certificate, CERT_SHA1_HASH_PROP_ID, thumbprint, &thumbprintSize))
    {
        wprintf(L"  Thumbprint:   ");
        PrintCertificateHexBytes(thumbprint, thumbprintSize, false);
        wprintf(L"\n");
    }

    PrintCertificateFileTime(L"  Valid from:   ", info->NotBefore);
    PrintCertificateFileTime(L"  Valid to:     ", info->NotAfter);

    // CertVerifyTimeValidity(nullptr, ...) compares NotBefore/NotAfter against the
    // current time, returning 0 only when now is within the validity window
    const bool isValid{ CertVerifyTimeValidity(nullptr, info) == 0 };
    wprintf(L"  Is valid:     %ls\n", isValid ? L"Yes" : L"No (Expired)");

    PCSTR algorithmOid{ info->SignatureAlgorithm.pszObjId };
    if (algorithmOid)
    {
        const PCCRYPT_OID_INFO algorithmInfo{ CryptFindOIDInfo(CRYPT_OID_INFO_OID_KEY,
            const_cast<PSTR>(algorithmOid), CRYPT_SIGN_ALG_OID_GROUP_ID) };
        if (algorithmInfo && algorithmInfo->pwszName)
        {
            wprintf(L"  Algorithm:    %ls (%hs)\n", algorithmInfo->pwszName, algorithmOid);
        }
        else
        {
            wprintf(L"  Algorithm:    %hs\n", algorithmOid);
        }
    }

    DWORD keyProvSize{};
    const bool hasPrivateKey{ CertGetCertificateContextProperty(certificate, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &keyProvSize) != FALSE };
    wprintf(L"  Private key:  %ls\n", hasPrivateKey ? L"Yes" : L"No");

    RETURN_IF_FAILED(PrintCertificateEnhancedKeyUsage(certificate));
    return S_OK;
}

HRESULT Command_Certificate_List(PCWSTR filename)
{
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);

    // Classify where the package's trust comes from (read-only; no elevation needed)
    MSIX::SignatureOrigin origin{ MSIX::SignatureOrigin::Unsigned };
    RETURN_IF_FAILED_MSG(MSIX::Signing::DetectSignatureOrigin(packageReader.get(), origin), "%ls", filename);

    wprintf(L"Package: %ls\n", filename);
    wprintf(L"Origin:  %ls\n", MSIX::ToString(origin));

    if (origin == MSIX::SignatureOrigin::Unsigned)
    {
        wprintf(L"No certificate to list (package is unsigned)\n");
        return S_OK;
    }

    // Extract and display the leaf (signing) certificate carried in the package
    wil::unique_cert_context signingCertificate;
    const HRESULT signerHr{ MSIX::Signing::GetSigningCertificate(packageReader.get(), signingCertificate) };
    RETURN_IF_FAILED_MSG(signerHr, "%ls", filename);
    if ((signerHr == S_FALSE) || !signingCertificate)
    {
        wprintf(L"No certificate to list\n");
        return S_OK;
    }

    RETURN_IF_FAILED(PrintCertificate(signingCertificate.get()));
    return S_OK;
}

HRESULT Command_Certificate_Remove(PCWSTR filename)
{
    HRESULT hr{};
    if (wil::string_starts_with(filename, L"0x"))
    {
        const size_t thumbprintLength{ wcslen(filename + 2) };
        RETURN_HR_IF(E_INVALIDARG, (thumbprintLength % 2) != 0);
        const size_t thumbprintSize{ thumbprintLength / 2 };
        wistd::unique_ptr<BYTE[]> thumbprint{ new (std::nothrow) BYTE[thumbprintSize] };
        RETURN_IF_NULL_ALLOC(thumbprint);
        RETURN_IF_FAILED(wil::parse_hexstring(filename + 2, thumbprintSize, thumbprint.get()));
        RETURN_IF_FAILED_MSG(hr = MSIX::Signing::RemoveCertificate(thumbprintSize, thumbprint.get()), "%ls", filename);
    }
    else
    {
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(hr = MSIX::Signing::RemoveCertificate(packageReader.get()), "%ls", filename);
    }
    wprintf(L"Certificate with thumbprint '%ls' %ls\n", filename + 2, (hr == S_OK ? L"is removed" : (hr == S_FALSE ? L"is not found" : L"???")));
    return S_OK;
}

HRESULT Command_Certificate(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Certificate);
    }

    PCWSTR action{ argv[2] };
    PCWSTR filename{ argv[3] };
    if ((CompareStringOrdinal(action, -1, L"add", -1, FALSE) != CSTR_EQUAL) &&
        (CompareStringOrdinal(action, -1, L"exists", -1, FALSE) != CSTR_EQUAL) &&
        (CompareStringOrdinal(action, -1, L"list", -1, FALSE) != CSTR_EQUAL) &&
        (CompareStringOrdinal(action, -1, L"remove", -1, FALSE) != CSTR_EQUAL))
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
            Help(help_Command_Certificate);
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

    HRESULT exitCode{};
    if (CompareStringOrdinal(action, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(exitCode = Command_Certificate_Add(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"exists", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(exitCode = Command_Certificate_Exists(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(exitCode = Command_Certificate_List(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(exitCode = Command_Certificate_Remove(filename));
    }
    else
    {
        FAIL_FAST_HR(E_UNEXPECTED);
    }
    return exitCode;
}

HRESULT Command_Provision_Add(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Provision_Add);
    }

    PCWSTR packageFamilyName{ argv[3] };

    bool deferRegistration{ true };
    bool logo{ true };

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_Add);
        }
        else if (CompareStringOrdinal(arg, -1, L"--defer-registration", -1, FALSE) == CSTR_EQUAL)
        {
            deferRegistration = true;
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

    HSTRING_HEADER packageFamilyNameHeader{};
    HSTRING packageFamilyNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(packageFamilyName, packageFamilyNameHeader, packageFamilyNameHString));

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    if (deferRegistration)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageAllUserProvisioningOptions> packageAllUserProvisioningOptions;
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageAllUserProvisioningOptions2> packageAllUserProvisioningOptions2;
        {
            HSTRING_HEADER classIdHeader{};
            HSTRING classId{};
            RETURN_IF_FAILED(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_PackageAllUserProvisioningOptions,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageAllUserProvisioningOptions) - 1,
                &classIdHeader, &classId));
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
            RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageAllUserProvisioningOptions.put())));
            RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageAllUserProvisioningOptions2.put())));
        }
        RETURN_IF_FAILED(packageAllUserProvisioningOptions2->put_DeferAutomaticRegistration(true));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager10> packageManager10;
        {
            HSTRING_HEADER classIdHeader{};
            HSTRING classId{};
            RETURN_IF_FAILED(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_PackageManager,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
                &classIdHeader, &classId));
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
            RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager10.put())));
        }
        RETURN_IF_FAILED(packageManager10->ProvisionPackageForAllUsersWithOptionsAsync(packageFamilyNameHString, packageAllUserProvisioningOptions.get(), deploymentOperation.put()));
    }
    else
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager6> packageManager6;
        {
            HSTRING_HEADER classIdHeader{};
            HSTRING classId{};
            RETURN_IF_FAILED(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_PackageManager,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
                &classIdHeader, &classId));
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
            RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager6.put())));
        }
        RETURN_IF_FAILED(packageManager6->ProvisionPackageForAllUsersAsync(packageFamilyNameHString, deploymentOperation.put()));
    }
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"Package family '%ls' is provisioned\n", packageFamilyName);

    return S_OK;
}

HRESULT Command_Provision_List(int argc, wchar_t* argv[])
{
    enum class PackageDisplayFormat { PackageFamilyName = 0, Full = 1 };

    bool logo{ true };
    PackageDisplayFormat format{};
    PCWSTR glob{};
    bool summary{ true };
    bool timeZoneIsLocal{ true };

    int argn{ 3 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_List);
        }
        else if (CompareStringOrdinal(arg, -1, L"--format=full", -1, FALSE) == CSTR_EQUAL)
        {
            format = PackageDisplayFormat::Full;
        }
        else if (CompareStringOrdinal(arg, -1, L"--format=packagefamilyname", -1, FALSE) == CSTR_EQUAL)
        {
            format = PackageDisplayFormat::PackageFamilyName;
        }
        else if (wil::string_starts_with(arg, L"--glob="))
        {
            glob = arg + (ARRAYSIZE(L"--glob=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--timezone=local", -1, FALSE) == CSTR_EQUAL)
        {
            timeZoneIsLocal = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--timezone=utc", -1, FALSE) == CSTR_EQUAL)
        {
            timeZoneIsLocal = false;
        }
        else if (CompareStringOrdinal(arg, -1, L"--no-summary", -1, FALSE) == CSTR_EQUAL)
        {
            summary = false;
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

    std::uint32_t countDisplayed{};

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager9.put())));
        RETURN_IF_FAILED(packageManager9->FindProvisionedPackages(packages.put()));
    }
    std::uint32_t packagesCount{};
    RETURN_IF_FAILED(packages->get_Size(&packagesCount));
    for (std::uint32_t index = 0; index < packagesCount; ++index)
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        RETURN_IF_FAILED(packages->GetAt(index, package.put()));
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageId> packageId;
        RETURN_IF_FAILED(package->get_Id(packageId.put()));
        wil::unique_hstring packageFamilyName;
        RETURN_IF_FAILED(packageId->get_FamilyName(wil::out_param(packageFamilyName)));
        PCWSTR familyName{ WindowsGetStringRawBuffer(packageFamilyName.get(), nullptr) };
        if (glob)
        {
            bool match{};
            RETURN_IF_FAILED(wil::glob(familyName, glob, match));
            if (!match)
            {
                continue;
            }
        }

        ++countDisplayed;

        if (format == PackageDisplayFormat::Full)
        {
            wprintf(L"#%u\n", countDisplayed);
            PackageX packageX;
            RETURN_IF_FAILED(PackageX::Make(package.get(), packageX));
            PrintPackage(packageX, packageId.get(), timeZoneIsLocal);
            wprintf(L"\n");
        }
        else
        {
            wprintf(L"%ls\n", familyName);
        }
    }

    if (summary)
    {
        wprintf(L"%u package%ls\n", countDisplayed, countDisplayed == 1 ? L"" : L"s");
    }

    return S_OK;
}


HRESULT Command_Provision_Remove(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Provision_Remove);
    }

    PCWSTR packageFamilyName{ argv[3] };

    bool logo{ true };

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_Remove);
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

    HSTRING_HEADER packageFamilyNameHeader{};
    HSTRING packageFamilyNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(packageFamilyName, packageFamilyNameHeader, packageFamilyNameHString));

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager8> packageManager8;
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager8.put())));
        RETURN_IF_FAILED(packageManager8->DeprovisionPackageForAllUsersAsync(packageFamilyNameHString, deploymentOperation.put()));
    }
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"Package family '%ls' is deprovisioned\n", packageFamilyName);

    return S_OK;
}

HRESULT Command_Provision(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Help(help_Command_Provision);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Provision_Add(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Provision_List(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Provision_Remove(argc, argv));
    }
    else
    {
        Help(help_Command_Provision);
    }
    return S_OK;
}

enum class ShortcutTargetType
{
    Unknown = 0,
    File,
    ApplicationUserModelId,
    URL,
};


HRESULT Command_Shortcut_Add(int argc, wchar_t* argv[])
{
    if (argc < 5)
    {
        Help(help_Command_Shortcut_Add);
    }

    bool logo{ true };
    PCWSTR file{ argv[3] };
    PCWSTR target{ argv[4] };

    PCWSTR arguments{};
    PCWSTR description{};
    wil::unique_cotaskmem_string iconPathBuffer;
    PCWSTR iconPath{};
    int iconIndex{};
    bool runAsAdministrator{};
    int showCommand{};
    auto targetType{ ShortcutTargetType::Unknown };
    PCWSTR workingDirectory{};

    int argn{ 5 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_Add);
        }
        else if (wil::string_starts_with(arg, L"--arguments="))
        {
            arguments = arg + (ARRAYSIZE(L"--arguments=") - 1);
        }
        else if (wil::string_starts_with(arg, L"--description="))
        {
            description = arg + (ARRAYSIZE(L"--description=") - 1);
        }
        else if (wil::string_starts_with(arg, L"--icon="))
        {
            iconPath = arg + (ARRAYSIZE(L"--icon=") - 1);
            PCWSTR delimiter{ wcsrchr(iconPath, L',') };
            if (delimiter)
            {
                for (PCWSTR p = delimiter + 1; *delimiter; ++delimiter)
                {
                    const auto c{ *p };
                    if ((c < L'0') || (L'9' < c))
                    {
                        UnknownArgument(arg);
                    }
                    iconIndex = iconIndex * 10 + static_cast<int>(c - L'0');
                }
                iconPathBuffer = wil::make_cotaskmem_string_nothrow(iconPath, delimiter - iconPath);
                RETURN_IF_NULL_ALLOC(iconPathBuffer);
                iconPath = iconPathBuffer.get();
            }
        }
        else if (CompareStringOrdinal(arg, -1, L"--run-as-administrator", -1, FALSE) == CSTR_EQUAL)
        {
            runAsAdministrator = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--show=min", -1, FALSE) == CSTR_EQUAL)
        {
            showCommand = SW_SHOWMINNOACTIVE;
        }
        else if (CompareStringOrdinal(arg, -1, L"--show=max", -1, FALSE) == CSTR_EQUAL)
        {
            showCommand = SW_SHOWMAXIMIZED;
        }
        else if (CompareStringOrdinal(arg, -1, L"--show=normal", -1, FALSE) == CSTR_EQUAL)
        {
            showCommand = SW_SHOWNORMAL;
        }
        else if (CompareStringOrdinal(arg, -1, L"--target=file", -1, FALSE) == CSTR_EQUAL)
        {
            targetType = ShortcutTargetType::File;
        }
        else if (CompareStringOrdinal(arg, -1, L"--target=app", -1, FALSE) == CSTR_EQUAL)
        {
            targetType = ShortcutTargetType::ApplicationUserModelId;
        }
        else if (CompareStringOrdinal(arg, -1, L"--target=url", -1, FALSE) == CSTR_EQUAL)
        {
            targetType = ShortcutTargetType::URL;
        }
        else if (wil::string_starts_with(arg, L"--working-directory="))
        {
            workingDirectory = arg + (ARRAYSIZE(L"--working-directory=") - 1);
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

    if (targetType == ShortcutTargetType::Unknown)
    {
        if (wil::string_ends_with(file, L".url", true))
        {
            targetType = ShortcutTargetType::URL;
        }
        else if (::VerifyApplicationUserModelId(target) == ERROR_SUCCESS)
        {
            targetType = ShortcutTargetType::ApplicationUserModelId;
        }
        else
        {
            targetType = ShortcutTargetType::File;
        }
    }
    else if (targetType == ShortcutTargetType::ApplicationUserModelId)
    {
        if (::VerifyApplicationUserModelId(target) != ERROR_SUCCESS)
        {
            UnknownArgument(target);
        }
    }

    if (targetType == ShortcutTargetType::URL)
    {
        if (arguments || description || runAsAdministrator || showCommand || workingDirectory)
        {
            Help(help_Command_Shortcut_Add);
        }
    }

    if (logo)
    {
        ShowLogo();
    }

    wil::com_ptr_nothrow<IPersistFile> persistFile;
    if (targetType == ShortcutTargetType::URL)
    {
        wil::com_ptr_nothrow<IUniformResourceLocatorW> url;
        RETURN_IF_FAILED(CoCreateInstance(CLSID_InternetShortcut, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&url)));

        if (iconPath)
        {
            wil::com_ptr<IPropertySetStorage> propertySetStorage;
            RETURN_IF_FAILED(url->QueryInterface(IID_PPV_ARGS(&propertySetStorage)));
            wil::com_ptr<IPropertyStorage> propertyStorage;
            HRESULT hr = propertySetStorage->Open(FMTID_Intshcut, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &propertyStorage);
            if (hr == STG_E_FILENOTFOUND)
            {
                RETURN_IF_FAILED(propertySetStorage->Create(FMTID_Intshcut, nullptr, PROPSETFLAG_DEFAULT,
                                                        STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &propertyStorage));
            }
            else
            {
                RETURN_IF_FAILED(hr);
            }

            PROPSPEC propertySpec[2]{};
            PROPVARIANT propertyVariant[2]{};
            ULONG propertyCount{};
            if (iconPath)
            {
                propertySpec[propertyCount].ulKind = PRSPEC_PROPID;
                propertySpec[propertyCount].propid = PID_IS_ICONFILE;
                propertyVariant[propertyCount].vt = VT_LPWSTR;
                propertyVariant[propertyCount].pwszVal = const_cast<PWSTR>(iconPath);
                ++propertyCount;
            }
            if (iconIndex)
            {
                propertySpec[propertyCount].ulKind = PRSPEC_PROPID;
                propertySpec[propertyCount].propid = PID_IS_ICONINDEX;
                propertyVariant[propertyCount].vt = VT_I4;
                propertyVariant[propertyCount].lVal = iconIndex;
                ++propertyCount;
            }
            RETURN_IF_FAILED(propertyStorage->WriteMultiple(propertyCount, propertySpec, propertyVariant, 0));
            RETURN_IF_FAILED(propertyStorage->Commit(STGC_DEFAULT));
        }

        RETURN_IF_FAILED(url->QueryInterface(IID_PPV_ARGS(&persistFile)));
    }
    else
    {
        wil::com_ptr_nothrow<IShellLinkW> shellLink;
        RETURN_IF_FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)));

        if (targetType == ShortcutTargetType::ApplicationUserModelId)
        {
            wil::unique_cotaskmem_string parseName;
            RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(parseName, L"shell:AppsFolder\\%ls", target));

            wil::unique_cotaskmem_ptr<ITEMIDLIST_ABSOLUTE> pidl;
            RETURN_IF_FAILED(SHParseDisplayName(parseName.get(), nullptr, wil::out_param(pidl), 0, nullptr));
            RETURN_IF_FAILED(shellLink->SetIDList(pidl.get()));

            wil::com_ptr_nothrow<IPropertyStore> propertyStore;
            RETURN_IF_FAILED(shellLink->QueryInterface(IID_PPV_ARGS(&propertyStore)));
            RETURN_IF_FAILED(wil::set_property_store_value(propertyStore, PKEY_AppUserModel_ID, target));
            RETURN_IF_FAILED(propertyStore->Commit());
        }
        else
        {
            RETURN_IF_FAILED(shellLink->SetPath(target));
        }

        if (arguments)
        {
            RETURN_IF_FAILED(shellLink->SetArguments(arguments));
        }
        if (description)
        {
            RETURN_IF_FAILED(shellLink->SetDescription(description));
        }
        if (iconPath)
        {
            RETURN_IF_FAILED(shellLink->SetIconLocation(iconPath, iconIndex));
        }
        if (showCommand)
        {
            RETURN_IF_FAILED(shellLink->SetShowCmd(showCommand));
        }
        if (workingDirectory)
        {
            RETURN_IF_FAILED(shellLink->SetWorkingDirectory(workingDirectory));
        }

        if (runAsAdministrator)
        {
            wil::com_ptr_nothrow<IShellLinkDataList> dataList;
            RETURN_IF_FAILED(shellLink->QueryInterface(IID_PPV_ARGS(&dataList)));
            DWORD flags{};
            RETURN_IF_FAILED(dataList->GetFlags(&flags));
            flags |= SLDF_RUNAS_USER;
            RETURN_IF_FAILED(dataList->SetFlags(flags));
        }

        RETURN_IF_FAILED(shellLink->QueryInterface(IID_PPV_ARGS(&persistFile)));
    }
    RETURN_IF_FAILED(persistFile->Save(file, TRUE));
    wprintf(L"Shortcut '%ls' is created\n", file);

    return S_OK;
}

HRESULT Command_Shortcut(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Help(help_Command_Shortcut);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Shortcut_Add(argc, argv));
    }
    else
    {
        Help(help_Command_Shortcut);
    }
    return S_OK;
}

HRESULT Command_Tool_PropertySheet_Install(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Tool_PropertySheet_Install);
    }

    bool logo{ true };
    PCWSTR path{};

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_Add);
        }
        else if (wil::string_starts_with(arg, L"--path="))
        {
            path = arg + (ARRAYSIZE(L"--path=") - 1);
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

    wil::unique_process_heap_string filename;
    PCWSTR dllFilename{};
    if (path)
    {
        dllFilename = path;
    }
    else
    {
        wil::unique_process_heap_string exePath;
        RETURN_IF_FAILED(GetExePath(exePath));
        RETURN_IF_FAILED(wil::str_printf_nothrow(filename, L"%ls\\MSIXPropertySheet.dll", exePath.get()));
        dllFilename = filename.get();
    }
    wil::unique_hmodule dll{ ::LoadLibraryExW(dllFilename, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) };
    RETURN_LAST_ERROR_IF_NULL_MSG(dll, "%ls", dllFilename);
    auto* function{ ::GetProcAddress(dll.get(), "DllRegisterServer") };
    auto dllRegisterServer{ reinterpret_cast<HRESULT(__stdcall*)(void)>(function) };
    RETURN_IF_FAILED(dllRegisterServer());
    wprintf(L"MSIX property sheet DLL '%ls' is installed\n", dllFilename);

    return S_OK;
}

HRESULT Command_Tool_PropertySheet_List(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Tool_PropertySheet_List);
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
            Help(help_Command_Provision_Add);
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

    // {8B6E4D93-1364-4bb9-BA15-5FC64B56BFB4}
    constexpr GUID CLSID_MSIXPropertySheet{ 0x8b6e4d93, 0x1364, 0x4bb9, { 0xba, 0x15, 0x5f, 0xc6, 0x4b, 0x56, 0xbf, 0xb4 } };

    wchar_t clsidAsString[39]{}; // GUID string is always 39 chars including null terminator
    RETURN_HR_IF(E_UNEXPECTED, StringFromGUID2(CLSID_MSIXPropertySheet, clsidAsString, ARRAYSIZE(clsidAsString)) == 0);

    {
        wil::unique_cotaskmem_string subkey;
        RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\CLSID\\%s", clsidAsString));

        wil::unique_hkey regkeyClsid;
        RETURN_IF_FAILED(wil::reg::try_open_unique_key_nothrow(HKEY_LOCAL_MACHINE, subkey.get(), regkeyClsid, wil::reg::key_access::read));
        if (!regkeyClsid)
        {
            wprintf(L"MSIX property sheet extension is not installed\n");
            return S_FALSE;
        }
        wil::unique_hkey regkeyInprocServer32;
        RETURN_IF_FAILED(wil::reg::try_open_unique_key_nothrow(regkeyClsid.get(), L"InprocServer32", regkeyInprocServer32, wil::reg::key_access::read));
        wil::unique_cotaskmem_string filename;
        if (regkeyInprocServer32)
        {
            RETURN_IF_FAILED(wil::reg::get_value_string_nothrow(regkeyInprocServer32.get(), nullptr, filename));
        }
        wprintf(L"MSIX property sheet extension '%ls' is installed\n", filename ? filename.get() : L"<null>");
    }
    {
        wprintf(L"Supported File Types:\n");
        PCWSTR fileTypes[]{ L".appx", L".msix" };
        for (PCWSTR fileType : fileTypes)
        {
            wil::unique_cotaskmem_string subkey;
            RETURN_IF_FAILED(wil::str_printf_nothrow(subkey, L"Software\\Classes\\SystemFileAssociations\\%s\\shellex\\PropertySheetHandlers\\MSIXPropertySheet", fileType));
            wil::unique_hkey regkey;
            RETURN_IF_FAILED_MSG(wil::reg::try_open_unique_key_nothrow(HKEY_LOCAL_MACHINE, subkey.get(), regkey, wil::reg::key_access::read), "%ls", subkey.get());
            bool isEnabled{};
            if (regkey)
            {
                wil::unique_cotaskmem_string installedClsidAsString;
                RETURN_IF_FAILED(wil::reg::get_value_string_nothrow(regkey.get(), nullptr, installedClsidAsString));
                isEnabled = (CompareStringOrdinal(installedClsidAsString.get(), -1, clsidAsString, -1, TRUE) == CSTR_EQUAL);
            }
            wprintf(L"    %ls: %ls\n", fileType, isEnabled ? L"Yes" : L"No");
        }
    }

    return S_OK;
}

HRESULT Command_Tool_PropertySheet_Uninstall(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Tool_PropertySheet_Uninstall);
    }

    bool logo{ true };
    PCWSTR path{};

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Provision_Add);
        }
        else if (wil::string_starts_with(arg, L"--path="))
        {
            path = arg + (ARRAYSIZE(L"--path=") - 1);
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

    wil::unique_process_heap_string filename;
    PCWSTR dllFilename{};
    if (path)
    {
        dllFilename = path;
    }
    else
    {
        wil::unique_process_heap_string exePath;
        RETURN_IF_FAILED(GetExePath(exePath));
        RETURN_IF_FAILED(wil::str_printf_nothrow(filename, L"%ls\\MSIXPropertySheet.dll", exePath.get()));
        dllFilename = filename.get();
    }
    wil::unique_hmodule dll{ ::LoadLibraryExW(dllFilename, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) };
    RETURN_LAST_ERROR_IF_NULL_MSG(dll, "%ls", dllFilename);
    auto* function{ ::GetProcAddress(dll.get(), "DllUnregisterServer") };
    auto dllUnregisterServer{ reinterpret_cast<HRESULT(__stdcall*)(void)>(function) };
    RETURN_IF_FAILED(dllUnregisterServer());
    wprintf(L"MSIX property sheet DLL '%ls' is uninstalled\n", dllFilename);

    return S_OK;
}

HRESULT Command_Tool_PropertySheet(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Tool_PropertySheet);
    }

    PCWSTR command{ argv[3] };
    if (CompareStringOrdinal(command, -1, L"install", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Tool_PropertySheet_Install(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Tool_PropertySheet_List(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"uninstall", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Tool_PropertySheet_Uninstall(argc, argv));
    }
    else
    {
        Help(help_Command_Tool_PropertySheet);
    }
    return S_OK;
}

HRESULT Command_Tool(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Help(help_Command_Tool);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"propertysheet", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Tool_PropertySheet(argc, argv));
    }
    else
    {
        Help(help_Command_Tool);
    }
    return S_OK;
}

HRESULT Command_Version(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        Help(help_Command_Version);
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
            Help(help_Command_Version);
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
    if (patch == 0)
    {
        wprintf(L"%hu.%hu.%hu\n", major, minor, build);
    }
    else
    {
        wprintf(L"%hu.%hu.%hu.%hu\n", major, minor, build, patch);
    }
    return S_OK;
}

HRESULT MessageOnError(HRESULT hr)
{
    if (FAILED(hr))
    {
        wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
        wprintf(L"Error 0x%08X: %ls", hr, message.get());
    }
    return hr;
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
        return MessageOnError(Command_Certificate(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"provision", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Provision(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"shortcut", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Shortcut(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"tool", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Tool(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"version", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Version(argc, argv));
    }
    else
    {
        Help();
    }
}
