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

[[noreturn]] void UnsupportedArgument(PCWSTR arg)
{
    wprintf(L"Error 0x00000001: Unsupported argument\n"
            L"    Full command line: '%ls'\n"
            L"Argument: %ls\n",
            GetCommandLine(), arg);
    ::ExitProcess(1);
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

void PrintPackageValue(PCWSTR key)
{
    wprintf(L"%-30ls :\n", key);
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
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"DisplayName") : PrintPackageValue(L"DisplayName", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_PublisherDisplayName(wil::out_param(string)));
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"PublisherDisplayName") : PrintPackageValue(L"PublisherDisplayName", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_Description(wil::out_param(string)));
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"Description") : PrintPackageValue(L"Description", hr, string);
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

HRESULT ToPackageVolume(
    PCWSTR path,
    ABI::Windows::Management::Deployment::IPackageManager3* packageManager3,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume,
    wil::unique_hstring& packageVolumePathHString)
{
    packageVolume.reset();
    packageVolumePathHString.reset();

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterable<ABI::Windows::Management::Deployment::PackageVolume*>> volumes;
    if (SUCCEEDED_LOG(packageManager3->FindPackageVolumes(&volumes)) && volumes)
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterator<ABI::Windows::Management::Deployment::PackageVolume*>> volumesIterator;
        if (SUCCEEDED_LOG(volumes->First(&volumesIterator)) && volumesIterator)
        {
            boolean hasCurrent{};
            if (SUCCEEDED_LOG(volumesIterator->get_HasCurrent(&hasCurrent)))
            {
                while (hasCurrent)
                {
                    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
                    if (SUCCEEDED_LOG(volumesIterator->get_Current(&volume)) && volume)
                    {
                        wil::unique_hstring packageStorePathHString;
                        if (SUCCEEDED_LOG(volume->get_PackageStorePath(wil::out_param(packageStorePathHString))))
                        {
                            PCWSTR packageStorePath{ WindowsGetStringRawBuffer(packageStorePathHString.get(), nullptr) };
                            if (packageStorePath)
                            {
                                if (wil::string_starts_with(packageStorePath, path, true))
                                {
                                    packageVolume = wistd::move(volume);
                                    packageVolumePathHString = wistd::move(packageStorePathHString);
                                    return S_OK;
                                }
                            }
                        }
                    }
                    if (FAILED_LOG(volumesIterator->MoveNext(&hasCurrent)))
                    {
                        break;
                    }
                }
            }
        }
    }
    RETURN_HR_MSG(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "%ls", path);
}

HRESULT ToPackageVolume(
    PCWSTR path,
    ABI::Windows::Management::Deployment::IPackageManager3* packageManager3,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume)
{
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(path, packageManager3, packageVolume, packageVolumePathHString));
    return S_OK;
}

HRESULT ToPackageVolume(
    PCWSTR path,
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume,
    wil::unique_hstring& packageVolumePathHString)
{
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    RETURN_IF_FAILED(packageManager9->QueryInterface(IID_PPV_ARGS(packageManager3.put())));
    RETURN_IF_FAILED(ToPackageVolume(path, packageManager3.get(), packageVolume, packageVolumePathHString));
    return S_OK;
}

HRESULT ToPackageVolume(
    PCWSTR path,
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume)
{
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(path, packageManager9, packageVolume, packageVolumePathHString));
    return S_OK;
}

HRESULT IsMsUupProtocol(ABI::Windows::Foundation::IUriRuntimeClass* uri, bool& isMsUup)
{
    isMsUup = false;

    wil::unique_hstring schemeName;
    RETURN_IF_FAILED(uri->get_SchemeName(wil::out_param(schemeName)));
    PCWSTR scheme{ WindowsGetStringRawBuffer(schemeName.get(), nullptr) };
    if (CompareStringOrdinal(scheme, -1, L"ms-uup:", -1, TRUE) != CSTR_EQUAL)
    {
        isMsUup = true;
    }
    return S_OK;
}

HRESULT ShowLogo()
{
    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t build{};
    std::uint16_t patch{};
    RETURN_IF_FAILED(wil::get_exe_version(major, minor, build, patch));
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
                L"  help         Help system\n"
                L"  package      Package management\n"
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
    L"Arguments:\n"
    L"  <FILE*> can be '0x<HEX>' to specify a certificate by its SHA-256 thumbprint\n"
};

constexpr PCWSTR help_Command_Help_Commands_Tree{
    L"Description:\n"
    L"  Display the command tree\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin help commands tree [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Help_Commands{
    L"Description:\n"
    L"  Help about commands\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin help commands <commands> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  tree  Display the command tree\n"
};

constexpr PCWSTR help_Command_Help{
    L"Description:\n"
    L"  Help system\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin help <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  commands  Display help about commands\n"
};

constexpr PCWSTR help_Command_Package{
    L"Description:\n"
    L"  View or modify packages\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add <PACKAGE>       Add a package\n"
    L"  list                Display packages registered for the user\n"
    L"  move <PACKAGE>      Move a package\n"
    L"  register <PACKAGE>  Register a package\n"
    L"  remove <PACKAGE>    Remove a package\n"
    L"  stage <PACKAGE>     Stage a package\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE> = PackageFamilyName|PackageFullName|file|URI\n"
};

constexpr PCWSTR help_Command_Package_Add{
    L"Description:\n"
    L"  Add a package\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package add <PACKAGE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --allow-unsigned              Allow an unsigned package\n"
    L"  --defer                       Defer registration if package is in use\n"
    L"  --dependency=<URI>            Dependency package\n"
    L"  --developer-mode              Install the package in development mode\n"
    L"  --external-location=<PATH>    Add the package with the external location\n"
    L"  --force                       Forcibly shutdown processes using the package if in use\n"
    L"  --limit-to-existing           Do not download missing referenced packages\n"
    L"  --priority=<PRIORITY>         Execute the deployment operation with the specified priority\n"
    L"  --retain-files-on-failure     Keep files created on a failed deployment\n"
    L"  --stage-in-place              Stage the package in place\n"
    L"  --stub=<STUB>                 Add a stub package\n"
    L"  --target=<VOLUME>             Add the package to the target Package Volume (e.g. C:)\n"
    L"  -nologo, --no-logo            Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = PackageFamilyName|PackageFullName|file|URI\n"
    L"  <PRIORITY> = low|normal|high\n"
    L"  <STUB>     = default|full|stub|preference\n"
};

constexpr PCWSTR help_Command_Package_List{
    L"Description:\n"
    L"  Display the currently installed packages\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package list [options]\n"
    L"\n"
    L"Options:\n"
    L"  --format=<FORMAT>            Display package format (default=full)\n"
    L"  --glob:<PROPERTY>=<PATTERN>  Display packages with <PROPERTY> matching PATTERN (*,? wildcards)\n"
    L"  --package-type=<TYPE>        Display packages of the specified package type (*=all)\n"
    L"  --user=<SID>                 Display packages for a user (*=all, default=current)\n"
    L"  --timezone=<TIMEZONE>        Display timezone for timestamps (default=local)\n"
    L"  --no-summary                 Do not display summary information\n"
    L"  -nologo, --no-logo           Do not display startup banner or copyright message\n"
    L"  -?, -h, --help               Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <FORMAT> = full|packagefamilyname|packagefullname\n"
    L"  <PROPERTY> = name|packagefamilyname|packagefullname\n"
    L"  <TIMEZONE> = local|utc\n"
    L"  <TYPE> = any combination of b (bundle), f (framework), m (main), o (optional), r (resource)\n"
};

constexpr PCWSTR help_Command_Package_Move{
    L"Description:\n"
    L"  Move a package\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package move <PACKAGEFULLNAME> <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --force                    Forcibly shutdown processes using the package if in use\n"
    L"  --retain-files-on-failure  Keep files created on a failed deployment\n"
    L"  -nologo, --no-logo         Do not display startup banner or copyright message\n"
    L"  -?, -h, --help             Show command line help\n"
};

constexpr PCWSTR help_Command_Package_Register{
    L"Description:\n"
    L"  Register a package\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package register <PACKAGE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --allow-unsigned              Allow an unsigned package\n"
    L"  --defer                       Defer registration if package is in use\n"
    L"  --dependency=<URI>            Dependency package\n"
    L"  --developer-mode              Install the package in development mode\n"
    L"  --external-location=<PATH>    Register the package with the external location\n"
    L"  --force                       Forcibly shutdown processes using the package if in use\n"
    L"  --stage-in-place              Stage the package in place\n"
    L"  --target=<VOLUME>             Register the package to the target Package Volume (e.g. C:)\n"
    L"  -nologo, --no-logo            Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = PackageFamilyName|PackageFullName|file|path|URI\n"
    L"  <STUB>     = default|full|stub|preference\n"
};

constexpr PCWSTR help_Command_Package_Remove{
    L"Description:\n"
    L"  Remove a package\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package remove <PACKAGE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --defer                      Defer removal if package is in use\n"
    L"  --preserve-application-data  Keep application data\n"
    L"  --all-users                  Remove the package for all users\n"
    L"  -nologo, --no-logo           Do not display startup banner or copyright message\n"
    L"  -?, -h, --help               Show command line help\n"
};

constexpr PCWSTR help_Command_Package_Stage{
    L"Description:\n"
    L"  Stage a package\n"
    L"\n"
    L"Usage:\n"
    L"  msixadmin package stage <PACKAGE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --allow-unsigned            Allow an unsigned package\n"
    L"  --dependency=<URI>          Dependency package\n"
    L"  --developer-mode            Install the package in development mode\n"
    L"  --external-location=<PATH>  Add the package with the external location\n"
    L"  --priority=<PRIORITY>       Execute the deployment operation with the specified priority\n"
    L"  --retain-files-on-failure   Keep files created on a failed deployment\n"
    L"  --stage-in-place            Stage the package in place\n"
    L"  --stub=<STUB>               Add a stub package\n"
    L"  --target=<VOLUME>           Add the package to the target Package Volume (e.g. C:)\n"
    L"  -nologo, --no-logo          Do not display startup banner or copyright message\n"
    L"  -?, -h, --help              Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = file|URI\n"
    L"  <PRIORITY> = low|normal|high\n"
    L"  <STUB>     = default|full|stub|preference\n"
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
    L"\n"
    L"Arguments:\n"
    L"  <TIMEZONE> = local|utc\n"
    L"  <FORMAT> = full|packagefamilyname\n"
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
    L"NOTE: URLs only support the --icon option\n"
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

HRESULT Command_Help_Commands_Tree(int argc, wchar_t* argv[])
{
    bool logo{};
    bool ascii{};

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Help_Commands_Tree);
        }
        else if (CompareStringOrdinal(arg, -1, L"--ascii", -1, FALSE) == CSTR_EQUAL)
        {
            ascii = true;
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

    if (ascii)
    {
        wprintf(L"msixadmin\n"
                L"+--certificate\n"
                L"|  +--add\n"
                L"|  +--exists\n"
                L"|  +--list\n"
                L"|  +--remove\n"
                L"+--package\n"
                L"|  +--add\n"
                L"|  +--list\n"
                L"|  +--move\n"
                L"|  +--register\n"
                L"|  +--remove\n"
                L"|  +--stage\n"
                L"+--provision\n"
                L"|  +--add\n"
                L"|  +--list\n"
                L"|  +--remove\n"
                L"+--shortcut\n"
                L"|  +--add\n"
                L"+--tool\n"
                L"|  +--propertysheet\n"
                L"|     +--install\n"
                L"|     +--list\n"
                L"|     +--uninstall\n"
                L"+--version\n");
    }
    else
    {
        _setmode(_fileno(stdout), _O_U16TEXT);
        wprintf(L"msixadmin\n"
                L"\u251C\u2500\u2500certificate\n"
                L"\u2502  \u251C\u2500\u2500add\n"
                L"\u2502  \u251C\u2500\u2500exists\n"
                L"\u2502  \u251C\u2500\u2500list\n"
                L"\u2502  \u2514\u2500\u2500remove\n"
                L"\u251C\u2500\u2500package\n"
                L"\u2502  \u251C\u2500\u2500add\n"
                L"\u2502  \u251C\u2500\u2500list\n"
                L"\u2502  \u251C\u2500\u2500move\n"
                L"\u2502  \u251C\u2500\u2500register\n"
                L"\u2502  \u251C\u2500\u2500remove\n"
                L"\u2502  \u2514\u2500\u2500stage\n"
                L"\u251C\u2500\u2500provision\n"
                L"\u2502  \u251C\u2500\u2500add\n"
                L"\u2502  \u251C\u2500\u2500list\n"
                L"\u2502  \u2514\u2500\u2500remove\n"
                L"\u251C\u2500\u2500shortcut\n"
                L"\u2502  \u2514\u2500\u2500add\n"
                L"\u251C\u2500\u2500tool\n"
                L"\u2502  \u2514\u2500\u2500propertysheet\n"
                L"\u2502     \u251C\u2500\u2500install\n"
                L"\u2502     \u251C\u2500\u2500list\n"
                L"\u2502     \u2514\u2500\u2500uninstall\n"
                L"\u2514\u2500\u2500version\n");
    }

    return S_OK;
}

HRESULT Command_Help_Commands(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Help_Commands);
    }

    PCWSTR command{ argv[3] };
    if (CompareStringOrdinal(command, -1, L"tree", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Help_Commands_Tree(argc, argv));
    }
    else
    {
        Help(help_Command_Help_Commands);
    }
    return S_OK;
}

HRESULT Command_Help(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Help(help_Command_Help);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"commands", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Help_Commands(argc, argv));
    }
    else
    {
        Help(help_Command_Help);
    }
    return S_OK;
}

constexpr auto PackageTypes_Error{ ABI::Windows::Management::Deployment::PackageTypes_Xap };
ABI::Windows::Management::Deployment::PackageTypes ToPackageTypes(PCWSTR string)
{
    auto packageTypes{ ABI::Windows::Management::Deployment::PackageTypes_None };
    for (auto c = *string; c != L'\0'; c = *++string)
    {
        if (c == L'*')
        {
            return ABI::Windows::Management::Deployment::PackageTypes_All;
        }
        else if (c == L'm')
        {
            packageTypes |= ABI::Windows::Management::Deployment::PackageTypes_Main;
        }
        else if (c == L'f')
        {
            packageTypes |= ABI::Windows::Management::Deployment::PackageTypes_Framework;
        }
        else if (c == L'r')
        {
            packageTypes |= ABI::Windows::Management::Deployment::PackageTypes_Resource;
        }
        else if (c == L'b')
        {
            packageTypes |= ABI::Windows::Management::Deployment::PackageTypes_Bundle;
        }
        else if (c == L'o')
        {
            packageTypes |= ABI::Windows::Management::Deployment::PackageTypes_Optional;
        }
        else
        {
            // Invalid
            return PackageTypes_Error;
        }
    }
    return packageTypes;
}

HRESULT Command_Package_Add(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Package_Add);
    }

    PCWSTR package{ argv[3] };

    bool logo{ true };
    bool allowUnsigned{};
    bool defer{};
    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::Foundation::Uri*>> dependencies;
    bool developerMode{};
    PCWSTR externalLocation{};
    bool force{};
    bool limitToExisting{};
    bool retainFilesOnFailure{};
    bool stageInPlace{};
    auto priority{ ABI::Windows::Management::Deployment::PackageOperationPriority_Normal };
    auto stub{ ABI::Windows::Management::Deployment::StubPackageOption_Default };
    PCWSTR target{};

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions> addPackageOptions;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_AddPackageOptions,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_AddPackageOptions) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(addPackageOptions.put())));
    }
    RETURN_IF_FAILED(addPackageOptions->get_DependencyPackageUris(dependencies.put()));

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_Add);
        }
        else if (CompareStringOrdinal(arg, -1, L"--allow-unsigned", -1, FALSE) == CSTR_EQUAL)
        {
            allowUnsigned = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--defer", -1, FALSE) == CSTR_EQUAL)
        {
            defer = true;
        }
        else if (wil::string_starts_with(arg, L"--dependency="))
        {
            PCWSTR dependency{ arg + (ARRAYSIZE(L"--dependency=") - 1) };
            wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> uri;
            RETURN_IF_FAILED(wil::to_uri(dependency, uri));
            RETURN_IF_FAILED(dependencies->Append(uri.get()));
        }
        else if (CompareStringOrdinal(arg, -1, L"--developer-mode", -1, FALSE) == CSTR_EQUAL)
        {
            developerMode = true;
        }
        else if (wil::string_starts_with(arg, L"--external-location="))
        {
            externalLocation = arg + (ARRAYSIZE(L"--externalLocation=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--force", -1, FALSE) == CSTR_EQUAL)
        {
            force = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--limit-to-existing", -1, FALSE) == CSTR_EQUAL)
        {
            limitToExisting = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=low", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_Low;
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=normal", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_Normal;
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=high", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_High;
        }
        else if (CompareStringOrdinal(arg, -1, L"--retain-files-on-failure", -1, FALSE) == CSTR_EQUAL)
        {
            retainFilesOnFailure = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stage-in-place", -1, FALSE) == CSTR_EQUAL)
        {
            stageInPlace = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=default", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_Default;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=full", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_InstallFull;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=stub", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_InstallStub;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=preference", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_UsePreference;
        }
        else if (wil::string_starts_with(arg, L"--target="))
        {
            target = arg + (ARRAYSIZE(L"--target=") - 1);
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

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(package, packageUri));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager9.put())));
    }

    if (allowUnsigned)
    {
        RETURN_IF_FAILED(addPackageOptions->put_AllowUnsigned(true));
    }
    if (developerMode)
    {
        RETURN_IF_FAILED(addPackageOptions->put_DeveloperMode(true));
    }
    if (externalLocation)
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> externalLocationUri;
        RETURN_IF_FAILED(wil::to_uri(package, externalLocationUri));
        RETURN_IF_FAILED(addPackageOptions->put_ExternalLocationUri(externalLocationUri.get()));
    }
    if (stageInPlace)
    {
        RETURN_IF_FAILED(addPackageOptions->put_StageInPlace(true));
    }
    if (stub != ABI::Windows::Management::Deployment::StubPackageOption_Default)
    {
        RETURN_IF_FAILED(addPackageOptions->put_StubPackageOption(stub));
    }
    if (target)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
        RETURN_IF_FAILED(ToPackageVolume(target, packageManager9.get(), targetVolume));
        RETURN_IF_FAILED(addPackageOptions->put_TargetVolume(targetVolume.get()));
    }
    if (limitToExisting)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions2> addPackageOptions2;
        RETURN_IF_FAILED(addPackageOptions->QueryInterface(IID_PPV_ARGS(addPackageOptions2.put())));
        RETURN_IF_FAILED(addPackageOptions2->put_LimitToExistingPackages(true));
    }
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions3> addPackageOptions3;
        RETURN_IF_FAILED(addPackageOptions->QueryInterface(IID_PPV_ARGS(addPackageOptions3.put())));
        RETURN_IF_FAILED(addPackageOptions3->put_PackageOperationPriority(priority));
    }

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    RETURN_IF_FAILED(packageManager9->AddPackageByUriAsync(packageUri.get(), addPackageOptions.get(), deploymentOperation.put()));
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"'%ls' is added\n", package);

    return S_OK;
}

HRESULT Command_Package_List(int argc, wchar_t* argv[])
{
    enum class PackageDisplayFormat { Full = 0, PackageFullName = 1, PackageFamilyName = 2 };

    PackageDisplayFormat format{};
    PCWSTR glob_name{};
    PCWSTR glob_packageFamilyName{};
    PCWSTR glob_packageFullName{};
    bool logo{ true };
    ABI::Windows::Management::Deployment::PackageTypes packageTypes{};
    bool summary{ true };
    bool timeZoneIsLocal{ true };
    PCWSTR user{};

    int argn{ 3 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_List);
        }
        else if (CompareStringOrdinal(arg, -1, L"--format=full", -1, FALSE) == CSTR_EQUAL)
        {
            format = PackageDisplayFormat::Full;
        }
        else if (CompareStringOrdinal(arg, -1, L"--format=packagefamilyname", -1, FALSE) == CSTR_EQUAL)
        {
            format = PackageDisplayFormat::PackageFamilyName;
        }
        else if (CompareStringOrdinal(arg, -1, L"--format=packagefullname", -1, FALSE) == CSTR_EQUAL)
        {
            format = PackageDisplayFormat::PackageFullName;
        }
        else if (wil::string_starts_with(arg, L"--glob:name="))
        {
            glob_name = arg + (ARRAYSIZE(L"--glob:name=") - 1);
        }
        else if (wil::string_starts_with(arg, L"--glob:packagefamilyname="))
        {
            glob_packageFamilyName = arg + (ARRAYSIZE(L"--glob:packagefamilyname=") - 1);
        }
        else if (wil::string_starts_with(arg, L"--glob:packagefullname="))
        {
            glob_packageFullName = arg + (ARRAYSIZE(L"--glob:packagefullname=") - 1);
        }
        else if (wil::string_starts_with(arg, L"--package-type="))
        {
            packageTypes = ToPackageTypes(arg + (ARRAYSIZE(L"--package-type=") - 1));
            if (packageTypes == PackageTypes_Error)
            {
                UnknownArgument(arg);
            }
        }
        else if (CompareStringOrdinal(arg, -1, L"--timezone=local", -1, FALSE) == CSTR_EQUAL)
        {
            timeZoneIsLocal = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--timezone=utc", -1, FALSE) == CSTR_EQUAL)
        {
            timeZoneIsLocal = false;
        }
        else if (wil::string_starts_with(arg, L"--user="))
        {
            user = arg + (ARRAYSIZE(L"--user=") - 1);
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

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterable<ABI::Windows::ApplicationModel::Package*>> iterablePackages;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));

        // Choose the optimal FindPackage*() variant given our inputs/options
        if (!user || (CompareStringOrdinal(user, -1, L"*", -1, FALSE) == CSTR_EQUAL))
        {
            // No user context
            if (packageTypes == ABI::Windows::Management::Deployment::PackageTypes_None)
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager> packageManager;
                RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager.put())));
                RETURN_IF_FAILED(packageManager->FindPackages(iterablePackages.put()));
            }
            else
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
                RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager2.put())));
                RETURN_IF_FAILED(packageManager2->FindPackagesWithPackageTypes(packageTypes, iterablePackages.put()));
            }
        }
        else
        {
            // User context
            HSTRING_HEADER userHeader{};
            HSTRING userHString{};
            RETURN_IF_FAILED(wil::to_hstring_reference(user, userHeader, userHString));
            if (packageTypes == ABI::Windows::Management::Deployment::PackageTypes_None)
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager> packageManager;
                RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager.put())));
                RETURN_IF_FAILED(packageManager->FindPackagesByUserSecurityId(userHString, iterablePackages.put()));
            }
            else
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
                RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager2.put())));
                RETURN_IF_FAILED(packageManager2->FindPackagesByUserSecurityIdWithPackageTypes(userHString, packageTypes, iterablePackages.put()));
            }
        }
    }
    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
    RETURN_IF_FAILED(iterablePackages.query_to(&packages));

    std::uint32_t packagesCount{};
    RETURN_IF_FAILED(packages->get_Size(&packagesCount));
    for (std::uint32_t index = 0; index < packagesCount; ++index)
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        RETURN_IF_FAILED(packages->GetAt(index, package.put()));

        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageId> packageId;
        RETURN_IF_FAILED(package->get_Id(packageId.put()));

        wil::unique_hstring packageFullNameAsHString;
        PCWSTR packageFullName{};
        if (!wil::string_is_null_or_empty(glob_packageFullName) ||
            (format == PackageDisplayFormat::PackageFullName))
        {
            RETURN_IF_FAILED(packageId->get_FullName(wil::out_param(packageFullNameAsHString)));
            packageFullName = WindowsGetStringRawBuffer(packageFullNameAsHString.get(), nullptr);
        }

        wil::unique_hstring packageFamilyNameAsHString;
        PCWSTR packageFamilyName{};
        if (!wil::string_is_null_or_empty(glob_packageFamilyName) ||
            (format == PackageDisplayFormat::PackageFamilyName))
        {
            RETURN_IF_FAILED(packageId->get_FamilyName(wil::out_param(packageFamilyNameAsHString)));
            packageFamilyName = WindowsGetStringRawBuffer(packageFamilyNameAsHString.get(), nullptr);
        }

        if (!wil::string_is_null_or_empty(glob_name) ||
            !wil::string_is_null_or_empty(glob_packageFullName) ||
            !wil::string_is_null_or_empty(glob_packageFamilyName))
        {
            if (!wil::string_is_null_or_empty(glob_packageFullName))
            {
                bool match{};
                RETURN_IF_FAILED(wil::glob(packageFullName, glob_packageFullName, match));
                if (!match)
                {
                    continue;
                }
            }

            if (!wil::string_is_null_or_empty(glob_packageFamilyName))
            {
                bool match{};
                RETURN_IF_FAILED(wil::glob(packageFamilyName, glob_packageFamilyName, match));
                if (!match)
                {
                    continue;
                }
            }

            if (!wil::string_is_null_or_empty(glob_packageFamilyName))
            {
                bool match{};
                RETURN_IF_FAILED(wil::glob(packageFamilyName, glob_packageFamilyName, match));
                if (!match)
                {
                    continue;
                }
            }

            if (!wil::string_is_null_or_empty(glob_name))
            {
                wil::unique_hstring nameAsHString;
                RETURN_IF_FAILED(packageId->get_Name(wil::out_param(nameAsHString)));
                PCWSTR name{ WindowsGetStringRawBuffer(nameAsHString.get(), nullptr) };
                if (glob_name)
                {
                    bool match{};
                    RETURN_IF_FAILED(wil::glob(name, glob_name, match));
                    if (!match)
                    {
                        continue;
                    }
                }
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
        else if (format == PackageDisplayFormat::PackageFullName)
        {
            wprintf(L"%ls\n", packageFullName);
        }
        else if (format == PackageDisplayFormat::PackageFamilyName)
        {
            wprintf(L"%ls\n", packageFamilyName);
        }
    }

    if (summary)
    {
        wprintf(L"%u package%ls\n", countDisplayed, countDisplayed == 1 ? L"" : L"s");
    }

    return S_OK;
}

HRESULT Command_Package_Move(int argc, wchar_t* argv[])
{
    if (argc < 5)
    {
        Help(help_Command_Package_Stage);
    }

    PCWSTR packageFullName{ argv[3] };
    PCWSTR target{ argv[4] };

    bool logo{ true };
    bool force{};
    bool retainFilesOnFailure{};

    int argn{ 5 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_Stage);
        }
        else if (CompareStringOrdinal(arg, -1, L"--force", -1, FALSE) == CSTR_EQUAL)
        {
            force = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--retain-files-on-failure", -1, FALSE) == CSTR_EQUAL)
        {
            retainFilesOnFailure = true;
        }
        else if (wil::string_starts_with(arg, L"--target="))
        {
            target = arg + (ARRAYSIZE(L"--target=") - 1);
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

    HSTRING_HEADER packageFullNameHeader{};
    HSTRING packageFullNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(packageFullName, packageFullNameHeader, packageFullNameHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager3.put())));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(target, packageManager3.get(), targetVolume, packageVolumePathHString));

    auto deploymentOptions{ ABI::Windows::Management::Deployment::DeploymentOptions_None };
    WI_SetFlagIf(deploymentOptions, ABI::Windows::Management::Deployment::DeploymentOptions_ForceTargetApplicationShutdown, force);
    WI_SetFlagIf(deploymentOptions, ABI::Windows::Management::Deployment::DeploymentOptions_RetainFilesOnFailure, retainFilesOnFailure);

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    RETURN_IF_FAILED(packageManager3->MovePackageToVolumeAsync(packageFullNameHString, deploymentOptions, targetVolume.get(), deploymentOperation.put()));
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    PCWSTR packageVolumePath{ WindowsGetStringRawBuffer(packageVolumePathHString.get(), nullptr) };
    wprintf(L"'%ls' is moved to %ls\n", packageFullName, packageVolumePath);

    return S_OK;
}

HRESULT Command_Package_Register(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Package_Add);
    }

    PCWSTR package{ argv[3] };

    bool logo{ true };
    bool allowUnsigned{};
    bool defer{};
    wil::winrt::vector_hstring dependenciesNames;
    bool developerMode{};
    PCWSTR externalLocation{};
    bool force{};
    bool stageInPlace{};
    PCWSTR appDataTarget{};

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IRegisterPackageOptions> registerPackageOptions;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_AddPackageOptions,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_AddPackageOptions) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(registerPackageOptions.put())));
    }

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_Add);
        }
        else if (CompareStringOrdinal(arg, -1, L"--allow-unsigned", -1, FALSE) == CSTR_EQUAL)
        {
            allowUnsigned = true;
        }
        else if (wil::string_starts_with(arg, L"--appdata-target="))
        {
            appDataTarget = arg + (ARRAYSIZE(L"--appdata-target=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--defer", -1, FALSE) == CSTR_EQUAL)
        {
            defer = true;
        }
        else if (wil::string_starts_with(arg, L"--dependency="))
        {
            PCWSTR dependency{ arg + (ARRAYSIZE(L"--dependency=") - 1) };
            HSTRING_HEADER dependencyHeader{};
            HSTRING dependencyHString{};
            RETURN_IF_FAILED(wil::to_hstring_reference(dependency, dependencyHeader, dependencyHString));
            RETURN_IF_FAILED(dependenciesNames.push_back(dependencyHString));
        }
        else if (CompareStringOrdinal(arg, -1, L"--developer-mode", -1, FALSE) == CSTR_EQUAL)
        {
            developerMode = true;
        }
        else if (wil::string_starts_with(arg, L"--external-location="))
        {
            externalLocation = arg + (ARRAYSIZE(L"--externalLocation=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--force", -1, FALSE) == CSTR_EQUAL)
        {
            force = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stage-in-place", -1, FALSE) == CSTR_EQUAL)
        {
            stageInPlace = true;
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

    PCWSTR packageFamilyName{};
    PCWSTR packageFullName{};
    PCWSTR packagePath{};
    wil::unique_process_heap_string packagePathBuffer;
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    if (::VerifyPackageFullName(package) == ERROR_SUCCESS)
    {
        packageFullName = package;
    }
    else if (::VerifyPackageFamilyName(package) == ERROR_SUCCESS)
    {
        packageFamilyName = package;
    }
    else
    {
        bool isDirectory{};
        bool isFile{};
        if (SUCCEEDED(wil::file_exists(package, isDirectory, isFile)))
        {
            if (isFile)
            {
                packagePath = package;
            }
            else
            {
                RETURN_IF_FAILED(wil::str_printf_nothrow(packagePathBuffer, L"%ls\\AppxManifest.xml", package));
                bool exists{};
                if (SUCCEEDED(wil::is_regular_file(packagePathBuffer.get(), exists)) && exists)
                {
                    packagePath = packagePathBuffer.get();
                }
                else if (SUCCEEDED(wil::to_uri(package, packageUri)))
                {
                    bool isMsUup{};
                    RETURN_IF_FAILED(IsMsUupProtocol(packageUri.get(), isMsUup));
                    if (!isMsUup)
                    {
                        packageUri.reset();
                    }
                }
            }
        }
    }
    if (!packageFullName && !packageFamilyName && !packageUri)
    {
        UnknownArgument(package);
    }
    wprintf(L"Registering '%ls'...\n", packagePath ? packagePath : package);

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager9.put())));
    }

    auto deploymentOptions{ ABI::Windows::Management::Deployment::DeploymentOptions_None };
    if (allowUnsigned)
    {
        if (packageFullName)
        {
            UnsupportedArgument(L"--allow-unsigned not supported if PACKAGE=PackageFullName");
        }
        RETURN_IF_FAILED(registerPackageOptions->put_AllowUnsigned(true));
    }
    if (defer)
    {
        if (packageFullName)
        {
            UnsupportedArgument(L"--allow-unsigned not supported if PACKAGE=PackageFullName");
        }
        RETURN_IF_FAILED(registerPackageOptions->put_DeferRegistrationWhenPackagesAreInUse(true));
    }
    if (force)
    {
        if (packageFullName)
        {
            WI_SetFlag(deploymentOptions, ABI::Windows::Management::Deployment::DeploymentOptions_ForceTargetApplicationShutdown);
        }
        else
        {
            RETURN_IF_FAILED(registerPackageOptions->put_ForceTargetAppShutdown(true));
        }
    }
    if (developerMode)
    {
        if (packageFullName)
        {
            WI_SetFlag(deploymentOptions, ABI::Windows::Management::Deployment::DeploymentOptions_DevelopmentMode);
        }
        else
        {
            RETURN_IF_FAILED(registerPackageOptions->put_DeveloperMode(true));
        }
    }
    if (externalLocation)
    {
        if (packageFullName)
        {
            UnsupportedArgument(L"--allow-unsigned not supported if PACKAGE=PackageFullName");
        }
        wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> externalLocationUri;
        RETURN_IF_FAILED(wil::to_uri(package, externalLocationUri));
        RETURN_IF_FAILED(registerPackageOptions->put_ExternalLocationUri(externalLocationUri.get()));
    }
    if (appDataTarget)
    {
        if (packageFullName)
        {
            UnsupportedArgument(L"--allow-unsigned not supported if PACKAGE=PackageFullName");
        }
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> appDataVolume;
        RETURN_IF_FAILED(ToPackageVolume(appDataTarget, packageManager9.get(), appDataVolume));
        RETURN_IF_FAILED(registerPackageOptions->put_AppDataVolume(appDataVolume.get()));
    }

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    if (packageFullName)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
        RETURN_IF_FAILED(packageManager9->QueryInterface(IID_PPV_ARGS(packageManager2.put())));
        HSTRING_HEADER packageFullNameHeader{};
        HSTRING packageFullNameHString{};
        RETURN_IF_FAILED(wil::to_hstring_reference(packageFullName, packageFullNameHeader, packageFullNameHString));
        RETURN_IF_FAILED(packageManager2->RegisterPackageByFullNameAsync(packageFullNameHString, dependenciesNames.get(), deploymentOptions, deploymentOperation.put()));
    }
    else if (packageFamilyName)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager5> packageManager5;
        RETURN_IF_FAILED(packageManager9->QueryInterface(IID_PPV_ARGS(packageManager5.put())));
        HSTRING_HEADER packageFamilyNameHeader{};
        HSTRING packageFamilyNameHString{};
        RETURN_IF_FAILED(wil::to_hstring_reference(packageFamilyName, packageFamilyNameHeader, packageFamilyNameHString));
        RETURN_IF_FAILED(packageManager5->RegisterPackageByFamilyNameAndOptionalPackagesAsync(packageFamilyNameHString, dependenciesNames.get(), deploymentOptions, nullptr, nullptr, deploymentOperation.put()));
    }
    else
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::Foundation::Uri*>> dependenciesUris;
        RETURN_IF_FAILED(registerPackageOptions->get_DependencyPackageUris(dependenciesUris.put()));
        for (std::uint32_t index = 0; index < dependenciesNames.size(); ++index)
        {
            auto dependency{ dependenciesNames[index] };
            wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> uri;
            RETURN_IF_FAILED(wil::to_uri(::WindowsGetStringRawBuffer(dependency, nullptr), uri));
            RETURN_IF_FAILED(dependenciesUris->Append(uri.get()));
        }
        RETURN_IF_FAILED(packageManager9->RegisterPackageByUriAsync(packageUri.get(), registerPackageOptions.get(), deploymentOperation.put()));
    }
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"%ls is registered\n", packagePath ? packagePath : package);

    return S_OK;
}

HRESULT Command_Package_Remove(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Package_Remove);
    }

    PCWSTR package{ argv[3] };

    bool allUsers{};
    bool defer{};
    bool logo{ true };
    bool preserveApplicationData{};

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_List);
        }
        else if (CompareStringOrdinal(arg, -1, L"--all-users", -1, FALSE) == CSTR_EQUAL)
        {
            allUsers = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--defer", -1, FALSE) == CSTR_EQUAL)
        {
            defer = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--preserve-application-data", -1, FALSE) == CSTR_EQUAL)
        {
            preserveApplicationData = true;
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

    PCWSTR packageFullName{};
    PCWSTR packageFamilyName{};
    wil::unique_cotaskmem_ptr<WCHAR[]> packageFullNameBuffer;
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    bool exists{};
    if (::VerifyPackageFullName(package) == ERROR_SUCCESS)
    {
        packageFullName = package;
    }
    else if (::VerifyPackageFamilyName(package) == ERROR_SUCCESS)
    {
        packageFamilyName = package;
    }
    else if (SUCCEEDED(wil::is_regular_file(package, exists)) && exists)
    {
        wil::com_ptr_nothrow<IAppxFactory> factory;
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(package, packageReader), "%ls", package);
        wil::com_ptr_nothrow<IAppxManifestReader> manifestReader;
        RETURN_IF_FAILED(packageReader->GetManifest(&manifestReader));
        wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
        RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
        RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(packageFullNameBuffer)));
        packageFullName = packageFullNameBuffer.get();
    }
    else if (SUCCEEDED(wil::to_uri(package, packageUri)))
    {
        bool isMsUup{};
        RETURN_IF_FAILED(IsMsUupProtocol(packageUri.get(), isMsUup));
        if (!isMsUup)
        {
            packageUri.reset();
        }
    }
    if (!packageFullName && !packageFamilyName && !packageUri)
    {
        UnknownArgument(package);
    }
    wprintf(L"Removing '%ls'...\n", packageFullName ? packageFullName : package);

    HSTRING_HEADER packageFullNameHeader{};
    HSTRING packageFullNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(packageFullName, packageFullNameHeader, packageFullNameHString));

    HSTRING_HEADER classIdHeader{};
    HSTRING classId{};
    RETURN_IF_FAILED(WindowsCreateStringReference(
        RuntimeClass_Windows_Management_Deployment_PackageManager,
        ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
        &classIdHeader, &classId));
    wil::com_ptr_nothrow<IInspectable> inspectable;
    RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
    RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager2.put())));

    ABI::Windows::Management::Deployment::RemovalOptions removalOptions{};
    WI_SetFlagIf(removalOptions, ABI::Windows::Management::Deployment::RemovalOptions_RemoveForAllUsers, allUsers);
    WI_SetFlagIf(removalOptions, ABI::Windows::Management::Deployment::RemovalOptions_DeferRemovalWhenPackagesAreInUse, defer);
    WI_SetFlagIf(removalOptions, ABI::Windows::Management::Deployment::RemovalOptions_PreserveApplicationData, preserveApplicationData);

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    RETURN_IF_FAILED(packageManager2->RemovePackageWithOptionsAsync(packageFullNameHString, removalOptions, deploymentOperation.put()));
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"'%ls' is removed\n", packageFullName);

    return S_OK;
}

HRESULT Command_Package_Stage(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Help(help_Command_Package_Stage);
    }

    PCWSTR package{ argv[3] };

    bool logo{ true };
    bool allowUnsigned{};
    bool developerMode{};
    PCWSTR externalLocation{};
    bool retainFilesOnFailure{};
    bool stageInPlace{};
    auto priority{ ABI::Windows::Management::Deployment::PackageOperationPriority_Normal };
    auto stub{ ABI::Windows::Management::Deployment::StubPackageOption_Default };
    PCWSTR target{};

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IStagePackageOptions> stagePackageOptions;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_StagePackageOptions,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_StagePackageOptions) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(stagePackageOptions.put())));
    }
    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::Foundation::Uri*>> dependencies;
    RETURN_IF_FAILED(stagePackageOptions->get_DependencyPackageUris(dependencies.put()));

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_Command_Package_Stage);
        }
        else if (CompareStringOrdinal(arg, -1, L"--allow-unsigned", -1, FALSE) == CSTR_EQUAL)
        {
            allowUnsigned = true;
        }
        else if (wil::string_starts_with(arg, L"--dependency="))
        {
            PCWSTR dependency{ arg + (ARRAYSIZE(L"--dependency=") - 1) };
            wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> uri;
            RETURN_IF_FAILED(wil::to_uri(dependency, uri));
            RETURN_IF_FAILED(dependencies->Append(uri.get()));
        }
        else if (CompareStringOrdinal(arg, -1, L"--developer-mode", -1, FALSE) == CSTR_EQUAL)
        {
            developerMode = true;
        }
        else if (wil::string_starts_with(arg, L"--external-location="))
        {
            externalLocation = arg + (ARRAYSIZE(L"--externalLocation=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=low", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_Low;
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=normal", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_Normal;
        }
        else if (CompareStringOrdinal(arg, -1, L"--priority=high", -1, FALSE) == CSTR_EQUAL)
        {
            priority = ABI::Windows::Management::Deployment::PackageOperationPriority_High;
        }
        else if (CompareStringOrdinal(arg, -1, L"--retain-files-on-failure", -1, FALSE) == CSTR_EQUAL)
        {
            retainFilesOnFailure = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stage-in-place", -1, FALSE) == CSTR_EQUAL)
        {
            stageInPlace = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=default", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_Default;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=full", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_InstallFull;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=stub", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_InstallStub;
        }
        else if (CompareStringOrdinal(arg, -1, L"--stub=preference", -1, FALSE) == CSTR_EQUAL)
        {
            stub = ABI::Windows::Management::Deployment::StubPackageOption_UsePreference;
        }
        else if (wil::string_starts_with(arg, L"--target="))
        {
            target = arg + (ARRAYSIZE(L"--target=") - 1);
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

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(package, packageUri));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
    {
        HSTRING_HEADER classIdHeader{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(
            RuntimeClass_Windows_Management_Deployment_PackageManager,
            ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
            &classIdHeader, &classId));
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
        RETURN_IF_FAILED(inspectable->QueryInterface(IID_PPV_ARGS(packageManager9.put())));
    }

    if (allowUnsigned)
    {
        RETURN_IF_FAILED(stagePackageOptions->put_AllowUnsigned(true));
    }
    if (developerMode)
    {
        RETURN_IF_FAILED(stagePackageOptions->put_DeveloperMode(true));
    }
    if (externalLocation)
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> externalLocationUri;
        RETURN_IF_FAILED(wil::to_uri(package, externalLocationUri));
        RETURN_IF_FAILED(stagePackageOptions->put_ExternalLocationUri(externalLocationUri.get()));
    }
    if (stageInPlace)
    {
        RETURN_IF_FAILED(stagePackageOptions->put_StageInPlace(true));
    }
    if (stub != ABI::Windows::Management::Deployment::StubPackageOption_Default)
    {
        RETURN_IF_FAILED(stagePackageOptions->put_StubPackageOption(stub));
    }
    if (target)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
        RETURN_IF_FAILED(ToPackageVolume(target, packageManager9.get(), targetVolume));
        RETURN_IF_FAILED(stagePackageOptions->put_TargetVolume(targetVolume.get()));
    }
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IStagePackageOptions3> stagePackageOptions3;
        RETURN_IF_FAILED(stagePackageOptions->QueryInterface(IID_PPV_ARGS(stagePackageOptions3.put())));
        RETURN_IF_FAILED(stagePackageOptions3->put_PackageOperationPriority(priority));
    }

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    RETURN_IF_FAILED(packageManager9->StagePackageByUriAsync(packageUri.get(), stagePackageOptions.get(), deploymentOperation.put()));
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    wprintf(L"'%ls' is staged\n", package);

    return S_OK;
}

HRESULT Command_Package(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        Help(help_Command_Package);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Add(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_List(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"move", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Move(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Remove(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"stage", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Stage(argc, argv));
    }
    else
    {
        Help(help_Command_Package);
    }
    return S_OK;
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
            RETURN_IF_FAILED(wil::str_printf_nothrow(parseName, L"shell:AppsFolder\\%ls", target));

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
        RETURN_IF_FAILED(wil::exe_path(exePath));
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
        RETURN_IF_FAILED(wil::exe_path(exePath));
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
    RETURN_IF_FAILED(wil::get_exe_version(major, minor, build, patch));
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
    else if (CompareStringOrdinal(arg, -1, L"help", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Help(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"package", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Package(argc, argv));
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
