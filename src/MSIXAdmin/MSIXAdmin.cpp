// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include <windows.h>

#if !defined(MSIXADMIN)
#define MSIXADMIN 1
#endif

#if !defined(MSIX_EXE_NAME)
#if MSIXADMIN == 0
#define MSIX_EXE_NAME   L"msix"
#else
#define MSIX_EXE_NAME   L"msixadmin"
#endif
#endif

static bool g_benchmark{};

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

[[noreturn]] void UnknownFileType(PCWSTR filename)
{
    wprintf(L"Error 0x00000001: Unknown file type\n"
            L"    Full command line: '%ls'\n"
            L"Argument: %ls\n",
            GetCommandLine(), filename);
    ::ExitProcess(1);
}

HRESULT FindPackageDependencies(
    PCWSTR packageFamilyName,
    std::uint32_t& packageDependencyIdsCount,
    PWSTR** packageDependencyIds)
{
    packageDependencyIdsCount = 0;
    *packageDependencyIds = nullptr;

    decltype(::FindPackageDependency)* findPackageDependencyFunction{};

    wil::unique_hmodule module;
    RETURN_IF_FAILED(wil::win32::load_library(L"kernelbase.dll", wil::out_param(module)));
    if (module)
    {
        RETURN_IF_FAILED(wil::win32::try_get_function(module.get(), "FindPackageDependency", &findPackageDependencyFunction));
    }
    if (!findPackageDependencyFunction)
    {
        return S_OK;
    }

    FindPackageDependencyCriteria findPackageDependencyCriteria{};
    findPackageDependencyCriteria.PackageFamilyName = packageFamilyName;
    RETURN_IF_FAILED(findPackageDependencyFunction(&findPackageDependencyCriteria, &packageDependencyIdsCount, packageDependencyIds));
    return S_OK;
}

HRESULT GetPackageDependencyResolvedToPackageFullName(
    PCWSTR packageDependencyId,
    PWSTR* packageFullName)
{
    *packageFullName = nullptr;

    decltype(::GetResolvedPackageFullNameForPackageDependency)* getResolvedPackageFullNameForPackageDependencyFunction{};

    wil::unique_hmodule module;
    RETURN_IF_FAILED(wil::win32::load_library(L"kernelbase.dll", wil::out_param(module)));
    if (module)
    {
        RETURN_IF_FAILED(wil::win32::try_get_function(module.get(), "GetResolvedPackageFullNameForPackageDependency", &getResolvedPackageFullNameForPackageDependencyFunction));
    }
    if (!getResolvedPackageFullNameForPackageDependencyFunction)
    {
        return S_OK;
    }

    RETURN_IF_FAILED(getResolvedPackageFullNameForPackageDependencyFunction(packageDependencyId, packageFullName));
    return S_OK;
}

HRESULT ActivateInstance(
    IInspectable** inspectable,
    PCWSTR activatableClassId)
{
    *inspectable = nullptr;
    HSTRING_HEADER classIdHeader{};
    HSTRING classId{};
    RETURN_IF_FAILED(::WindowsCreateStringReference(activatableClassId, static_cast<std::uint32_t>(wcslen(activatableClassId)), &classIdHeader, &classId));
    RETURN_IF_FAILED(::RoActivateInstance(classId, inspectable));
    return S_OK;
}

HRESULT ActivateInstance(
    wil::com_ptr_nothrow<IInspectable>& inspectable,
    PCWSTR activatableClassId)
{
    RETURN_IF_FAILED(ActivateInstance(inspectable.put(), activatableClassId));
    return S_OK;
}

HRESULT FindProvisionedPackageFullNames(
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& packageFullNames,
    bool sort = false)
{
    packageFullNames.reset();

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
    RETURN_IF_FAILED(packageManager9->FindProvisionedPackages(packages.put()));
    std::uint32_t count{};
    RETURN_IF_FAILED(packages->get_Size(&count));

    auto allocation{ wil::make_unique_cotaskmem_nothrow<PWSTR[]>(count) };
    RETURN_IF_NULL_ALLOC(allocation);
    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> stringsFullNames{ allocation.release(), count };

    for (std::uint32_t index = 0; index < count; ++index)
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        RETURN_IF_FAILED(packages->GetAt(index, package.put()));
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageId> packageId;
        RETURN_IF_FAILED(package->get_Id(packageId.put()));
        wil::unique_hstring packageFullName;
        RETURN_IF_FAILED(packageId->get_FullName(wil::out_param(packageFullName)));
        PCWSTR fullName{ WindowsGetStringRawBuffer(packageFullName.get(), nullptr) };
        auto string{ wil::make_cotaskmem_string_nothrow(fullName) };
        RETURN_IF_NULL_ALLOC(string);
        stringsFullNames[index] = string.release();
    }

    if (sort)
    {
        qsort(stringsFullNames.get(), count, sizeof(PWSTR), wil::compare_wstring_nocase);
    }

    packageFullNames = wistd::move(stringsFullNames);
    return S_OK;
}

HRESULT FindProvisionedPackageFamilyNames(
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& packageFamilyNames,
    bool sort = false)
{
    packageFamilyNames.reset();

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9local;
    if (!packageManager9)
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager9local.put()));
        packageManager9 = packageManager9local.get();
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
    RETURN_IF_FAILED(packageManager9->FindProvisionedPackages(packages.put()));
    std::uint32_t count{};
    RETURN_IF_FAILED(packages->get_Size(&count));

    auto allocation{ wil::make_unique_cotaskmem_nothrow<PWSTR[]>(count) };
    RETURN_IF_NULL_ALLOC(allocation);
    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> stringsFamilyNames{ allocation.release(), count };

    for (std::uint32_t index = 0; index < count; ++index)
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        RETURN_IF_FAILED(packages->GetAt(index, package.put()));
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageId> packageId;
        RETURN_IF_FAILED(package->get_Id(packageId.put()));
        wil::unique_hstring packageFamilyName;
        RETURN_IF_FAILED(packageId->get_FamilyName(wil::out_param(packageFamilyName)));
        PCWSTR fullName{ WindowsGetStringRawBuffer(packageFamilyName.get(), nullptr) };
        auto string{ wil::make_cotaskmem_string_nothrow(fullName) };
        RETURN_IF_NULL_ALLOC(string);
        stringsFamilyNames[index] = string.release();
    }

    if (sort)
    {
        qsort(stringsFamilyNames.get(), count, sizeof(PWSTR), wil::compare_wstring_nocase);
    }

    packageFamilyNames = wistd::move(stringsFamilyNames);
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

void PrintPackageError(PCWSTR prefix, HRESULT hr)
{
    wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
    wprintf(L"%ls***ERROR 0x%08X %ls", prefix, hr, message.get());
}

void PrintPackageKeyValueError(PCWSTR key, HRESULT hr)
{
    wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
    wprintf(L"%-30ls : ***ERROR 0x%08X %ls", key, hr, message.get());
}

void PrintPackageKeyValueError(PCWSTR prefix, PCWSTR key, HRESULT hr)
{
    wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
    wprintf(L"%ls%-30ls : ***ERROR 0x%08X %ls", prefix, key, hr, message.get());
}

void PrintPackageValue(PCWSTR key)
{
    wprintf(L"%-30ls :\n", key);
}

void PrintPackageValue(PCWSTR prefix, PCWSTR key)
{
    wprintf(L"%ls%-30ls :\n", prefix, key);
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

void PrintPackageValue(PCWSTR prefix, PCWSTR key, HRESULT hr, PCWSTR value)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
    }
    else
    {
        wprintf(L"%s%-30ls : %ls\n", prefix, key, value);
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

HRESULT TryParseInteger(PCWSTR value, std::uint16_t& integer, PCWSTR& endOfParse)
{
    wchar_t* endOfString{};
    const auto n{ wcstoul(value, &endOfString, 10) };
    RETURN_HR_IF(E_INVALIDARG, (errno != 0));
    RETURN_HR_IF(E_INVALIDARG, n > UINT16_MAX);
    integer = static_cast<std::uint16_t>(n);
    endOfParse = endOfString;
    return S_OK;
}

HRESULT ToVersion(PCWSTR value, std::uint64_t& version)
{
    if (wil::string_starts_with(value, L"0x"))
    {
        wchar_t* endOfString{};
        version = ::wcstoull(value, &endOfString, 16);
        RETURN_HR_IF(E_INVALIDARG, (errno != 0) || (endOfString && *endOfString));
    }
    else
    {
        if ((value[0] == L'*') && (value[0] == L'\0'))
        {
            version = 0xFFFFFFFFFFFFFFFFllu;
        }
        else
        {
            std::uint16_t n{};
            PCWSTR endOfParse{};
            RETURN_IF_FAILED(TryParseInteger(value, n, endOfParse));
            RETURN_HR_IF(E_INVALIDARG, *endOfParse != L'.');
            version = static_cast<std::uint64_t>(n) << 48;
            value = endOfParse + 1;
            if ((value[0] == L'*') && (value[0] == L'\0'))
            {
                version |= 0x0000FFFFFFFFFFFFllu;
            }
            else
            {
                RETURN_IF_FAILED(TryParseInteger(value, n, endOfParse));
                RETURN_HR_IF(E_INVALIDARG, *endOfParse != L'.');
                version |= static_cast<std::uint64_t>(n) << 32;
                value = endOfParse + 1;
                if ((value[0] == L'*') && (value[0] == L'\0'))
                {
                    version |= 0x00000000FFFFFFFFllu;
                }
                else
                {
                    RETURN_IF_FAILED(TryParseInteger(value, n, endOfParse));
                    RETURN_HR_IF(E_INVALIDARG, *endOfParse != L'.');
                    version |= static_cast<std::uint64_t>(n) << 16;
                    value = endOfParse + 1;
                    if ((value[0] == L'*') && (value[0] == L'\0'))
                    {
                        version |= 0x000000000000FFFFllu;
                    }
                    else
                    {
                        RETURN_IF_FAILED(TryParseInteger(value, n, endOfParse));
                        RETURN_HR_IF(E_INVALIDARG, *endOfParse != L'\0');
                        version |= static_cast<std::uint64_t>(n);
                    }
                }
            }
        }
    }
    return S_OK;
}

UINT64 ToVersion(const ABI::Windows::ApplicationModel::PackageVersion value)
{
    return (static_cast<std::uint64_t>(value.Major) << 48) |
           (static_cast<std::uint64_t>(value.Minor) << 32) |
           (static_cast<std::uint64_t>(value.Build) << 16) |
           static_cast<std::uint64_t>(value.Revision);
}

void PrintPackageValue(PCWSTR key, HRESULT hr, const ABI::Windows::ApplicationModel::PackageVersion value)
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
        return S_OK;
    }

    RETURN_IF_FAILED(package.package2()->get_IsResourcePackage(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Resource;
        return S_OK;
    }

    RETURN_IF_FAILED(package.package4()->get_IsOptional(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Optional;
        return S_OK;
    }

    RETURN_IF_FAILED(package.package2()->get_IsBundle(&value));
    if (value)
    {
        packageType = ABI::Windows::Management::Deployment::PackageTypes_Bundle;
        return S_OK;
    }

    packageType = ABI::Windows::Management::Deployment::PackageTypes_Main;
    return S_OK;
}

enum class DependencyType
{
    None        = 0,
    Framework   = 0x0001,
    HostRuntime = 0x0002,
    Optional    = 0x0004,
    Resource    = 0x0008,

    All = Framework | HostRuntime | Optional | Resource,
};
DEFINE_ENUM_FLAG_OPERATORS(DependencyType)

HRESULT ToDependencyTypes(
    PCWSTR string,
    DependencyType& dependencyTypes)
{
    dependencyTypes = DependencyType::None;

    RETURN_HR_IF_NULL(E_INVALIDARG, string);

    for (PCWSTR s = string; *s != L'\0'; ++s)
    {
        if (*s == L'*')
        {
            WI_SetAllFlags(dependencyTypes, DependencyType::Framework |
                                            DependencyType::HostRuntime |
                                            DependencyType::Optional |
                                            DependencyType::Resource);
        }
        else if (*s == L'f')
        {
            WI_SetFlag(dependencyTypes, DependencyType::Framework);
        }
        else if (*s == L'h')
        {
            WI_SetFlag(dependencyTypes, DependencyType::HostRuntime);
        }
        else if (*s == L'o')
        {
            WI_SetFlag(dependencyTypes, DependencyType::Optional);
        }
        else if (*s == L'r')
        {
            WI_SetFlag(dependencyTypes, DependencyType::Resource);
        }
        else
        {
            UnknownArgument(string);
        }
    }
    return S_OK;
}

enum class ReferenceType
{
    None        = 0,
    Framework   = 0x0001,
    HostRuntime = 0x0002,
    Optional    = 0x0004,
    Resource    = 0x0008,
    Provisioned = 0x0010,
    Pinned      = 0x0020,
    Explicit    = 0x0040,
    Uup         = 0x0080,
    Dynamic     = 0x0100,

    All = Framework | HostRuntime | Optional | Resource | Provisioned | Pinned | Explicit | Uup | Dynamic,
};
DEFINE_ENUM_FLAG_OPERATORS(ReferenceType)

HRESULT ToReferenceTypes(
    PCWSTR string,
    ReferenceType& referenceTypes)
{
    referenceTypes = ReferenceType::None;

    RETURN_HR_IF_NULL(E_INVALIDARG, string);

    for (PCWSTR s = string; *s != L'\0'; ++s)
    {
        if (*s == L'*')
        {
            WI_SetAllFlags(referenceTypes, ReferenceType::Framework |
                                           ReferenceType::HostRuntime |
                                           ReferenceType::Optional |
                                           ReferenceType::Resource |
                                           ReferenceType::Provisioned |
                                           ReferenceType::Pinned |
                                           ReferenceType::Explicit |
                                           ReferenceType::Uup |
                                           ReferenceType::Dynamic);
        }
        else if (*s == L'f')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Framework);
        }
        else if (*s == L'h')
        {
            WI_SetFlag(referenceTypes, ReferenceType::HostRuntime);
        }
        else if (*s == L'o')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Optional);
        }
        else if (*s == L'r')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Resource);
        }
        else if (*s == L'p')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Provisioned);
        }
        else if (*s == L'p')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Pinned);
        }
        else if (*s == L'e')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Explicit);
        }
        else if (*s == L'u')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Uup);
        }
        else if (*s == L'd')
        {
            WI_SetFlag(referenceTypes, ReferenceType::Dynamic);
        }
        else
        {
            UnknownArgument(string);
        }
    }
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
    Disabled             = 0x00020010,
    DataOffline          = 0x00020020,
    PackageOffline       = 0x00020040,

    Servicing            = 0x00040000,
    DeploymentInProgress = 0x00040100,

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
                if (value)
                {
                    status |= Status::DependencyIssue;
                }
                if (FAILED_LOG(hr = packageStatus->get_LicenseIssue(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::LicenseIssue;
                }
                if (FAILED_LOG(hr = packageStatus->get_Modified(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::Modified;
                }
                if (FAILED_LOG(hr = packageStatus->get_Tampered(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::Tampered;
                }
            }

            if (FAILED_LOG(hr = packageStatus->get_NotAvailable(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                if (value)
                {
                    status |= Status::NotAvailable;
                }
                if (FAILED_LOG(hr = packageStatus->get_Disabled(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::Disabled;
                }
                if (FAILED_LOG(hr = packageStatus->get_DataOffline(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::DataOffline;
                }
                if (FAILED_LOG(hr = packageStatus->get_PackageOffline(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::PackageOffline;
                }
            }

            if (FAILED_LOG(hr = packageStatus->get_Servicing(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                if (value)
                {
                    status |= Status::Servicing;
                }
                if (FAILED_LOG(hr = packageStatus->get_DeploymentInProgress(&value)))
                {
                    PrintPackageKeyValueError(key, hr);
                }
                if (value)
                {
                    status |= Status::DeploymentInProgress;
                }
            }

            wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageStatus2> packageStatus2;
            if (FAILED_LOG(hr = packageStatus.query_to(packageStatus2.put())))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (FAILED_LOG(hr = packageStatus2->get_IsPartiallyStaged(&value)))
            {
                PrintPackageKeyValueError(key, hr);
            }
            if (value)
            {
                status |= Status::IsPartiallyStaged;
            }

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
                        wprintf(L",");
                    }
                    wprintf(L"LicenseIssue");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::Modified))
                {
                    if (needDelimiter)
                    {
                        wprintf(L",");
                    }
                    wprintf(L"Modified");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::Tampered))
                {
                    if (needDelimiter)
                    {
                        wprintf(L",");
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
                        wprintf(L",");
                    }
                    wprintf(L"DataOffline");
                    needDelimiter = true;
                }
                if (WI_AreAllFlagsSet(status, Status::PackageOffline))
                {
                    if (needDelimiter)
                    {
                        wprintf(L",");
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
            wprintf(L"\n");
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

void PrintPackages(
    PCWSTR key,
    HRESULT hr,
    ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>* packages)
{
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(key, hr);
        return;
    }

    std::uint32_t packagesCount{};
    if (FAILED_LOG(hr = packages->get_Size(&packagesCount)))
    {
        PrintPackageKeyValueError(key, hr);
        return;
    }
    wprintf(L"%-30ls : %u package%ls\n", key, packagesCount, packagesCount == 1 ? L"" : L"s");

    auto allocation{ wil::make_unique_cotaskmem_nothrow<HSTRING[]>(packagesCount) };
    if (!allocation)
    {
        PrintPackageKeyValueError(key, E_OUTOFMEMORY);
        return;
    }
    wil::unique_cotaskmem_array_ptr<wil::unique_hstring> packageFullNames{ allocation.release(), packagesCount };

    for (std::uint32_t index = 0; index < packagesCount; ++index)
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        if (FAILED_LOG(hr = packages->GetAt(index, package.put())))
        {
            PrintPackageKeyValueError(key, hr);
            continue;
        }
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageId> packageId;
        if (FAILED_LOG(hr = package->get_Id(packageId.put())))
        {
            PrintPackageKeyValueError(key, hr);
            continue;
        }
        wil::unique_hstring packageFullNameAsHString;
        if (FAILED_LOG(hr = packageId->get_FullName(&packageFullNames[index])))
        {
            PrintPackageKeyValueError(key, hr);
            continue;
        }
    }

    qsort(packageFullNames.get(), packagesCount, sizeof(HSTRING), wil::compare_hstring_nocase);

    for (std::uint32_t index = 0; index < packagesCount; ++index)
    {
        const auto& packageFullNameAsHString{ packageFullNames[index] };
        PCWSTR packageFullName{ WindowsGetStringRawBuffer(packageFullNameAsHString, nullptr) };
        wprintf(L"        %ls\n", packageFullName);
    }
}

void PrintPackageRelated(
    PCWSTR key,
    ABI::Windows::ApplicationModel::IPackage9* package9,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions)
{
    if (findRelatedPackagesOptions)
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
        const HRESULT hr{ LOG_IF_FAILED(package9->FindRelatedPackages(findRelatedPackagesOptions, packages.put())) };
        PrintPackages(key, hr, packages.get());
    }
}

void PrintPackagePhysicalPath(
    PCWSTR key,
    HRESULT hr,
    PCWSTR packagePath)
{
    if (FAILED(hr) || wil::string_is_null_or_empty(packagePath))
    {
        PrintPackageValue(key);
    }
    else
    {
        wil::unique_hfile file{ ::CreateFileW(packagePath, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
        if (!file)
        {
            PrintPackageKeyValueError(key, LOG_LAST_ERROR());
        }
        else
        {
            DWORD length{ GetFinalPathNameByHandleW(file.get(), nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS) };
            if (length == 0)
            {
                PrintPackageKeyValueError(key, LOG_LAST_ERROR());
            }
            else
            {
                auto physicalPath{ wil::make_unique_cotaskmem_nothrow<WCHAR[]>(length) };
                if (!physicalPath)
                {
                    PrintPackageKeyValueError(key, E_OUTOFMEMORY);
                }
                else
                {
                    length = GetFinalPathNameByHandleW(file.get(), physicalPath.get(), length, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                    if (length == 0)
                    {
                        PrintPackageKeyValueError(key, LOG_LAST_ERROR());
                    }
                    else
                    {
                        PrintPackageValue(key, S_OK, physicalPath.get());
                    }
                }
            }
        }
    }
}

void PrintPackage(
    PackageX& package,
    ABI::Windows::ApplicationModel::IPackageId* packageId,
    const bool localTimeZone,
    PCWSTR user = nullptr,
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9 = nullptr,
    ABI::Windows::Management::Deployment::IPackageManager12* packageManager12 = nullptr,
    DependencyType dependencies = DependencyType::None,
    ReferenceType references = ReferenceType::None,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_Dependencies_Frameworks = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_Dependencies_HostRuntimes = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_Dependencies_Optionals = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_Dependencies_Resources = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_References_Frameworks = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_References_HostRuntimes = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_References_Optionals = nullptr,
    ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions* findRelatedPackagesOptions_References_Resources = nullptr)
{
    wprintf(L"Identity\n");
    wil::unique_hstring packageFullNameHString;
    const HRESULT hrPackageFullName{ LOG_IF_FAILED(packageId->get_FullName(wil::out_param(packageFullNameHString))) };
    PCWSTR packageFullName{ WindowsGetStringRawBuffer(packageFullNameHString.get(), nullptr) };
    PrintPackageValue(L"    PackageFullName", hrPackageFullName, packageFullName);
    wil::unique_hstring packageFamilyNameHString;
    HRESULT hrPackageFamilyName{ LOG_IF_FAILED(packageId->get_FamilyName(wil::out_param(packageFamilyNameHString))) };
    PCWSTR packageFamilyName{ WindowsGetStringRawBuffer(packageFamilyNameHString.get(), nullptr) };
    PrintPackageValue(L"    PackageFamilyName", hrPackageFamilyName, packageFamilyName);
    wil::unique_hstring string;
    HRESULT hr{ LOG_IF_FAILED(packageId->get_Name(wil::out_param(string))) };
    PrintPackageValue(L"    Name", hr, string);
    ABI::Windows::ApplicationModel::PackageVersion version{};
    PrintPackageValue(L"    Version", LOG_IF_FAILED(packageId->get_Version(&version)), version);
    ABI::Windows::System::ProcessorArchitecture architecture{ ABI::Windows::System::ProcessorArchitecture_Unknown };
    PrintPackageValue(L"    Architecture", LOG_IF_FAILED(packageId->get_Architecture(&architecture)), architecture);
    hr = LOG_IF_FAILED(packageId->get_ResourceId(wil::out_param(string)));
    PrintPackageValue(L"    ResourceId", hr, string);
    hr = LOG_IF_FAILED(packageId->get_Publisher(wil::out_param(string)));
    PrintPackageValue(L"    Publisher", hr, string);
    hr = LOG_IF_FAILED(packageId->get_PublisherId(wil::out_param(string)));
    PrintPackageValue(L"    PublisherId", hr, string);

    wprintf(L"Metadata\n");
    ABI::Windows::Foundation::DateTime dateTime{};
    PrintPackageValue(L"    InstalledDate", LOG_IF_FAILED(package.package3()->get_InstalledDate(&dateTime)), dateTime, localTimeZone);
    boolean boolean{};
    PrintPackageValue(L"    IsDevelopmentMode", LOG_IF_FAILED(package.package2()->get_IsDevelopmentMode(&boolean)), boolean);
    ABI::Windows::Management::Deployment::PackageTypes packageType{};
    PrintPackageValue(L"    IsStub", LOG_IF_FAILED(package.package8()->get_IsStub(&boolean)), boolean);
    PrintPackageValue(L"    PackageType", LOG_IF_FAILED(ToPackageType(package, packageType)), packageType);
    hr = LOG_IF_FAILED(package.package9()->get_SourceUriSchemeName(wil::out_param(string)));
    PrintPackageValue(L"    SourceUriSchemeName", hr, string);

    wprintf(L"Display\n");
    hr = LOG_IF_FAILED(package.package2()->get_DisplayName(wil::out_param(string)));
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"    DisplayName") : PrintPackageValue(L"    DisplayName", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_Description(wil::out_param(string)));
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"    Description") : PrintPackageValue(L"    Description", hr, string);
    hr = LOG_IF_FAILED(package.package2()->get_PublisherDisplayName(wil::out_param(string)));
    (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) ? PrintPackageValue(L"    PublisherDisplayName") : PrintPackageValue(L"    PublisherDisplayName", hr, string);

    wprintf(L"Location\n");
    hr = LOG_IF_FAILED(package.package8()->get_EffectivePath(wil::out_param(string)));
    PrintPackageValue(L"    EffectivePath", hr, string);
    wil::unique_hstring installedPathHString;
    HRESULT hrInstalledPath{ LOG_IF_FAILED(package.package8()->get_InstalledPath(wil::out_param(installedPathHString))) };
    PCWSTR installedPath{ WindowsGetStringRawBuffer(installedPathHString.get(), nullptr) };
    PrintPackageValue(L"    InstalledPath", hrInstalledPath, installedPath);
    wil::unique_hstring mutablePathHString;
    HRESULT hrMutablePath{ LOG_IF_FAILED(package.package8()->get_MutablePath(wil::out_param(mutablePathHString))) };
    PCWSTR mutablePath{ WindowsGetStringRawBuffer(installedPathHString.get(), nullptr) };
    PrintPackageValue(L"    MutablePath", hrMutablePath, mutablePath);
    wil::unique_hstring machineExternalPathHString;
    HRESULT hrMachineExternalPath{ LOG_IF_FAILED(package.package8()->get_MachineExternalPath(wil::out_param(machineExternalPathHString))) };
    PCWSTR machineExternalPath{ WindowsGetStringRawBuffer(machineExternalPathHString.get(), nullptr) };
    PrintPackageValue(L"    MachineExternalPath", hrMachineExternalPath, machineExternalPath);
    wil::unique_hstring userExternalPathHString;
    HRESULT hrUserExternalPath{ LOG_IF_FAILED(package.package8()->get_UserExternalPath(wil::out_param(userExternalPathHString))) };
    PCWSTR userExternalPath{ WindowsGetStringRawBuffer(userExternalPathHString.get(), nullptr) };
    PrintPackageValue(L"    UserExternalPath", hrUserExternalPath, userExternalPath);
    wil::unique_hstring effectiveExternalPathHString;
    HRESULT hrEffectiveExternalPath{ LOG_IF_FAILED(package.package8()->get_EffectiveExternalPath(wil::out_param(effectiveExternalPathHString))) };
    PCWSTR effectiveExternalPath{ WindowsGetStringRawBuffer(effectiveExternalPathHString.get(), nullptr) };
    PrintPackageValue(L"    EffectiveExternalPath", hrEffectiveExternalPath, effectiveExternalPath);

    wprintf(L"Physical Location\n");
    PrintPackagePhysicalPath(L"    InstalledPath", hrInstalledPath, installedPath);
    PrintPackagePhysicalPath(L"    MutablePath", hrMutablePath, mutablePath);
    PrintPackagePhysicalPath(L"    MachineExternalPath", hrMachineExternalPath, machineExternalPath);
    PrintPackagePhysicalPath(L"    UserExternalPath", hrUserExternalPath, userExternalPath);

    wprintf(L"Signing\n");
    PackageOrigin packageOrigin{};
    hr = LOG_IF_FAILED(::GetStagedPackageOrigin(packageFullName, &packageOrigin));
    if (FAILED(hr))
    {
        PrintPackageKeyValueError(L"    IsSigned", hr);
        PrintPackageKeyValueError(L"    PackageOrigin", hr);
    }
    else
    {
        const auto isSigned{ (packageOrigin == ::PackageOrigin_Inbox) ||
                             (packageOrigin == ::PackageOrigin_Store) ||
                             (packageOrigin == ::PackageOrigin_DeveloperSigned) ||
                             (packageOrigin == ::PackageOrigin_LineOfBusiness) };
        PrintPackageValue(L"    IsSigned", hr, isSigned);
        PrintPackageValue(L"    PackageOrigin", hr, packageOrigin);
    }
    ABI::Windows::ApplicationModel::PackageSignatureKind signatureKind{};
    PrintPackageValue(L"    PackageSignatureKind", LOG_IF_FAILED(package.package4()->get_SignatureKind(&signatureKind)), signatureKind);

    wprintf(L"State\n");
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackageStatus> status;
    PrintPackageValue(L"    Status", LOG_IF_FAILED(package.package3()->get_Status(status.put())), status);
    HRESULT hrIsProvisioned{ E_NOTIMPL };
    bool isProvisioned{};
    if (packageManager9)
    {
        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> provisionedPackageFamilyNames;
        hr = LOG_IF_FAILED(FindProvisionedPackageFamilyNames(packageManager9, provisionedPackageFamilyNames));
        if (FAILED(hr))
        {
            PrintPackageKeyValueError(L"    IsProvisioned", hr);
            hrIsProvisioned = hr;
        }
        else
        {
            for (const auto& provisionedPackageFamilyName : provisionedPackageFamilyNames)
            {
                if (CompareStringOrdinal(packageFamilyName, -1, provisionedPackageFamilyName, -1, TRUE) == CSTR_EQUAL)
                {
                    isProvisioned = true;
                    break;
                }
            }
            PrintPackageValue(L"    IsProvisioned", S_OK, isProvisioned);
        }
    }
    PrintPackageValue(L"    IsPinned", S_OK, L"? ==> https://github.com/microsoft/WindowsAppSDK/pull/6379");
    if (user)
    {
        PrintPackageValue(L"    User", hr, user);
    }
    if (!packageFullName)
    {
        PrintPackageKeyValueError(L"    RemovalPending", hrPackageFullName);
    }
    else if (!packageManager12)
    {
        PrintPackageValue(L"    RemovalPending", S_OK, L"N/A (requires Windows 11 24H2 (build 26100) or newer)");
    }
    else
    {
        //TODO user=null == current user == S-1-...
        if (user)
        {
            HSTRING_HEADER userHeader{};
            HSTRING userHString{};
            hr = LOG_IF_FAILED_MSG(wil::to_hstring_reference(user, userHeader, userHString), "%ls", user);
            if (SUCCEEDED(hr))
            {
                wil::unique_process_heap_string value;
                hr = LOG_IF_FAILED(packageManager12->IsPackageRemovalPendingForUser(packageFullNameHString.get(), userHString, &boolean));
                if (SUCCEEDED(hr))
                {
                    wil::unique_any_psid userSid;
                    hr = LOG_IF_FAILED(wil::security::to_sid(user, userSid));
                    if (SUCCEEDED(hr))
                    {
                        wil::unique_process_heap_string userName;
                        hr = LOG_IF_FAILED(wil::security::to_account_name(userSid.get(), userName));
                        if (SUCCEEDED(hr))
                        {
                            hr = LOG_IF_FAILED(wil::str_printf_nothrow(value, L"{%ls [%ls]: %ls}", user, userName ? userName.get() : L"<null>", boolean ? L"Yes" : L"No"));
                        }
                    }
                }
                PrintPackageValue(L"    RemovalPending", hr, value.get());
            }
        }
        else
        {
            PrintPackageValue(L"    RemovalPending", LOG_IF_FAILED(packageManager12->IsPackageRemovalPending(packageFullNameHString.get(), &boolean)), boolean);
        }
    }

    if (dependencies != DependencyType::None)
    {
        wprintf(L"Dependencies\n");
        if (!packageFullName)
        {
            PrintPackageKeyValueError(L"    HostRuntimeDependency", hrPackageFullName);
            PrintPackageKeyValueError(L"    Optional", hrPackageFullName);
            PrintPackageKeyValueError(L"    PackageDependency", hrPackageFullName);
            PrintPackageKeyValueError(L"    Resource", hrPackageFullName);
        }
        else
        {
            PrintPackageRelated(L"    HostRuntimesDependency", package.package9(), findRelatedPackagesOptions_Dependencies_HostRuntimes);
            PrintPackageRelated(L"    Optional", package.package9(), findRelatedPackagesOptions_Dependencies_Optionals);
            PrintPackageRelated(L"    PackageDependency", package.package9(), findRelatedPackagesOptions_Dependencies_Frameworks);
            PrintPackageRelated(L"    Resource", package.package9(), findRelatedPackagesOptions_Dependencies_Resources);
        }
    }

    if (references != ReferenceType::None)
    {
        wprintf(L"References\n");
        if (WI_IsFlagSet(references, ReferenceType::Dynamic))
        {
            if (!packageFamilyName)
            {
                PrintPackageKeyValueError(L"    HostRuntimeDependency", hrPackageFamilyName);
            }
            else
            {
                std::uint32_t packageDependencyIdsCount{};
                wil::unique_process_heap_ptr<PWSTR[]> packageDependencyIds;
                hr = FindPackageDependencies(packageFamilyName, packageDependencyIdsCount, wil::out_param(packageDependencyIds));
                if (FAILED(hr))
                {
                    PrintPackageKeyValueError(L"    Dynamic", hr);
                }
                else
                {
                    wprintf(L"    Dynamic                    : %u PackageDependencyId%s\n", packageDependencyIdsCount, packageDependencyIdsCount == 1 ? L"" : L"s");
                    for (std::uint32_t index = 0; index < packageDependencyIdsCount; ++index)
                    {
                        PCWSTR packageDependencyId{ packageDependencyIds[index] };
                        wil::unique_process_heap_ptr<WCHAR[]> resolvedPackageFullName;
                        hr = LOG_IF_FAILED(::GetPackageDependencyResolvedToPackageFullName(packageDependencyId, wil::out_param(resolvedPackageFullName)));
                        PrintPackageValue(L"        ", packageDependencyId, hr, resolvedPackageFullName.get());
                        if (SUCCEEDED(hr))
                        {
                            UINT32 processIdsCount{};
                            wil::unique_process_heap_ptr<DWORD[]> processIds;
                            hr = LOG_IF_FAILED(::GetProcessesUsingPackageDependency(packageDependencyId, nullptr, FALSE, &processIdsCount, wil::out_param(processIds)));
                            if (FAILED(hr))
                            {
                                PrintPackageError(L"            ", hr);
                            }
                            else
                            {
                                for (UINT32 processIndex = 0; processIndex < processIdsCount; ++processIndex)
                                {
                                    const auto processId{ processIds[processIndex] };
                                    wil::unique_handle processHandle{ ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
                                    if (!processHandle)
                                    {
                                        LOG_LAST_ERROR();
                                        processHandle.reset(wistd::move(::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId)));
                                        if (!processHandle)
                                        {
                                            hr = LOG_LAST_ERROR();
                                        }
                                    }
                                    wil::unique_process_heap_string imageName;
                                    if (processHandle)
                                    {
                                        hr = LOG_IF_FAILED(wil::QueryFullProcessImageNameW<wil::unique_process_heap_string>(processHandle.get(), 0, imageName));
                                    }
                                    if (FAILED(hr))
                                    {
                                        wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
                                        wprintf(L"            PID %-10u : ***ERROR 0x%08X %ls", processId, hr, message.get());
                                    }
                                    else
                                    {
                                        wprintf(L"            PID %-14u : %ls\n", processId, imageName.get());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (WI_IsFlagSet(references, ReferenceType::Explicit))
        {
            wprintf(L"    Explicit                   : ? ==> TODO\n");
        }
        if (!packageFullName)
        {
            PrintPackageKeyValueError(L"    HostRuntimesDependency", hrPackageFullName);
            PrintPackageKeyValueError(L"    Optional", hrPackageFullName);
            PrintPackageKeyValueError(L"    PackageDependency", hrPackageFullName);
        }
        else
        {
            PrintPackageRelated(L"    HostRuntimesDependency", package.package9(), findRelatedPackagesOptions_References_HostRuntimes);
            PrintPackageRelated(L"    Optional", package.package9(), findRelatedPackagesOptions_References_Optionals);
            PrintPackageRelated(L"    PackageDependency", package.package9(), findRelatedPackagesOptions_References_Frameworks);
        }
        if (WI_IsFlagSet(references, ReferenceType::Pinned))
        {
            wprintf(L"    Pinned                     : ? ==> https://github.com/microsoft/WindowsAppSDK/pull/6379\n");
        }
        if (WI_IsFlagSet(references, ReferenceType::Provisioned))
        {
            if (FAILED(hrIsProvisioned))
            {
                wil::unique_hlocal_string message{ wil::format_message_nothrow(hrIsProvisioned) };
                wprintf(L"    Provisioned                : ***ERROR 0x%08X %ls", hrIsProvisioned, message.get());
            }
            else
            {
                wprintf(L"    Provisioned                : %ls\n", isProvisioned ? L"Yes" : L"No");
            }
        }
        if (!packageFullName)
        {
            PrintPackageKeyValueError(L"    Resource", hrPackageFullName);
        }
        else
        {
            PrintPackageRelated(L"    Resource", package.package9(), findRelatedPackagesOptions_References_Resources);
        }
        if (WI_IsFlagSet(references, ReferenceType::Uup))
        {
            wprintf(L"    UUP                        : ? ==> TODO\n");
        }
    }
}

HRESULT ToPackageVolume(
    PCWSTR path,
    PCWSTR name,
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
                            if (path)
                            {
                                PCWSTR packageStorePath{ WindowsGetStringRawBuffer(packageStorePathHString.get(), nullptr) };
                                if (packageStorePath)
                                {
                                    // Only 1 online PackageVolume can exist per drive
                                    bool isOnlineAndMatch{};
                                    if ((path[1] == L':') && (path[2] == L'\0') && wil::string_starts_with(packageStorePath, path, true))
                                    {
                                        boolean isOffline{};
                                        if (SUCCEEDED_LOG(volume->get_IsOffline(&isOffline)))
                                        {
                                            isOnlineAndMatch = !isOffline;
                                        }
                                    }
                                    if (isOnlineAndMatch || (CompareStringOrdinal(packageStorePath, -1, path, -1, TRUE) == CSTR_EQUAL))
                                    {
                                        packageVolume = wistd::move(volume);
                                        packageVolumePathHString = wistd::move(packageStorePathHString);
                                        return S_OK;
                                    }
                                }
                            }
                            if (name)
                            {
                                wil::unique_hstring volumeNameHString;
                                if (SUCCEEDED_LOG(volume->get_Name(wil::out_param(volumeNameHString))))
                                {
                                    PCWSTR volumeName{ WindowsGetStringRawBuffer(volumeNameHString.get(), nullptr) };
                                    if (volumeName)
                                    {
                                        if (CompareStringOrdinal(volumeName, -1, name, -1, TRUE) == CSTR_EQUAL)
                                        {
                                            packageVolume = wistd::move(volume);
                                            packageVolumePathHString = wistd::move(volumeNameHString);
                                            return S_OK;
                                        }
                                    }
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
    PCWSTR name,
    ABI::Windows::Management::Deployment::IPackageManager3* packageManager3,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume)
{
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(path, name, packageManager3, packageVolume, packageVolumePathHString));
    return S_OK;
}

HRESULT ToPackageVolume(
    PCWSTR path,
    PCWSTR name,
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume,
    wil::unique_hstring& packageVolumePathHString)
{
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    RETURN_IF_FAILED(packageManager9->QueryInterface(IID_PPV_ARGS(packageManager3.put())));
    RETURN_IF_FAILED(ToPackageVolume(path, name, packageManager3.get(), packageVolume, packageVolumePathHString));
    return S_OK;
}

HRESULT ToPackageVolume(
    PCWSTR path,
    PCWSTR name,
    ABI::Windows::Management::Deployment::IPackageManager9* packageManager9,
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& packageVolume)
{
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(path, name, packageManager9, packageVolume, packageVolumePathHString));
    return S_OK;
}

bool IsPackageFileOrUri(PCWSTR fileOrUri)
{
    return wil::string_ends_with(fileOrUri, L".msix", true) ||
           wil::string_ends_with(fileOrUri, L".msixbundle", true) ||
           wil::string_ends_with(fileOrUri, L".appx", true) ||
           wil::string_ends_with(fileOrUri, L".appxbundle", true);
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
        wprintf(MSIX_EXE_NAME L" v%hu.%hu.%hu.%hu - Copyright (C) Howard Kapustein. All rights reserved.\n", major, minor, build, patch);
    }
    else
    {
        wprintf(MSIX_EXE_NAME L" v%hu.%hu.%hu - Copyright (C) Howard Kapustein. All rights reserved.\n", major, minor, build);
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
                L"  " MSIX_EXE_NAME L" <command> [arguments]\n"
                L"\n"
                L"Commands:\n"
                L"  certificate  Certificate management\n"
                L"  help         Help system\n"
                L"  package      Package management\n"
                L"  provision    Provision management\n"
                L"  shortcut     Shortcut operations\n"
                L"  tool         Install or manage tools that extend the MSIX experience\n"
                L"  version      Display version\n"
                L"  volume       Package volume management\n"
                L"\n"
                L"Run '" MSIX_EXE_NAME L" [command] --help' for more information on a command\n");
    }
    ::ExitProcess(1);
}

constexpr PCWSTR help_Command_Certificate_Add{
    L"Description:\n"
    L"  Add the certificate from the signed package file to the system certificate store\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" certificate add <FILE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark         Display elapsed time\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
};

constexpr PCWSTR help_Command_Certificate_Exists{
    L"Description:\n"
    L"  Check if the certificate for the signed package file exists in the system certificate store\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" certificate exists <FILE*> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark         Display elapsed time\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <FILE*> can be '0x<HEX>' to specify a certificate by its SHA-256 thumbprint\n"
};

constexpr PCWSTR help_Command_Certificate_List{
    L"Description:\n"
    L"  Display the certificate from the signed package file\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" certificate list <FILE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark         Display elapsed time\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
};

constexpr PCWSTR help_Command_Certificate_Remove{
    L"Description:\n"
    L"  Remove the certificate for the signed package file from the system certificate store\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" certificate remove <FILE*> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark         Display elapsed time\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <FILE*> can be '0x<HEX>' to specify a certificate by its SHA-256 thumbprint\n"
};

constexpr PCWSTR help_Command_Certificate{
    L"Description:\n"
    L"  MSIX Certificate Management\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" certificate [command] [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark         Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" help commands tree [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Help_Commands{
    L"Description:\n"
    L"  Help about commands\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" help commands <commands> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" help <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" package <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add <PACKAGE>               Add a package\n"
    L"  list                        Display packages registered for the user\n"
    L"  move <PACKAGE>              Move a package\n"
    L"  register <PACKAGE>          Register a package\n"
    L"  remove <PACKAGE>            Remove a package\n"
    L"  stage <PACKAGE>             Stage a package\n"
    L"  status                      Display or modify package status\n"
    L"  verify <PACKAGEFULLNAME>    Verify a package's integrity\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE> = PackageFamilyName|PackageFullName|file|URI\n"
};

constexpr PCWSTR help_Command_Package_Add{
    L"Description:\n"
    L"  Add a package\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package add <PACKAGE> [options]\n"
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
    L"  --benchmark                   Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" package list [options] [GLOB]\n"
    L"\n"
    L"Options:\n"
    L"  --dependencies[:<DEPTYPE>]     Display dependencies of the package\n"
    L"  --format=<FORMAT>              Display package format (default=full)\n"
    L"  --glob[:<PROPERTY>]=<PATTERN>  Display packages with <PROPERTY> (default=name) matching PATTERN (*,? wildcards)\n"
    L"  --max-version=<VERSION>        Display packages with version <= <VERSION>\n"
    L"  --min-version=<VERSION>        Display packages with version >= <VERSION>\n"
    L"  --no-dependencies              Do not display dependencies of the package\n"
    L"  --no-references                Do not display references on the package\n"
    L"  --package-type=<TYPE>          Display packages of the specified package type (*=all)\n"
    L"  --references[:<REFTYPE>]       Display references on the package\n"
    L"  --user=<SID>                   Display packages for a user (*=all, default=current)\n"
    L"  --timezone=<TIMEZONE>          Display timezone for timestamps (default=local)\n"
    L"  --no-summary                   Do not display summary information\n"
    L"  --                             End option processing\n"
    L"  --benchmark                    Display elapsed time\n"
    L"  -nologo, --no-logo             Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                 Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  [GLOB] = Same as '--glob=GLOB'\n"
    L"  <DEPTYPE> = f (framework), h (hostruntime), o (optional), r (resource)\n"
    L"  <FORMAT> = full|packagefamilyname|packagefullname\n"
    L"  <PROPERTY> = name|packagefamilyname|packagefullname\n"
    L"  <REFTYPE> = d (dynamic), e (explicit), f (framework), h (hostruntime), i (pinned), o (optional), p (provisioned), r (resource), u (uup)\n"
    L"  <TIMEZONE> = local|utc\n"
    L"  <TYPE> = any combination of b (bundle), f (framework), m (main), o (optional), r (resource)\n"
    L"  <VERSION> = 0xmmmmnnnnbbbbrrrr or major.minor.build.revision (aka DotQuadNumber, optional ending wildcard (*) e.g. 1.2.* = 1.2.65535.65535)\n"
};

constexpr PCWSTR help_Command_Package_Move{
    L"Description:\n"
    L"  Move a package\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package move <PACKAGEFULLNAME> <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --force                    Forcibly shutdown processes using the package if in use\n"
    L"  --retain-files-on-failure  Keep files created on a failed deployment\n"
    L"  --benchmark                Display elapsed time\n"
    L"  -nologo, --no-logo         Do not display startup banner or copyright message\n"
    L"  -?, -h, --help             Show command line help\n"
};

constexpr PCWSTR help_Command_Package_Register{
    L"Description:\n"
    L"  Register a package\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package register <PACKAGE> [options]\n"
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
    L"  --benchmark                   Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" package remove <PACKAGE> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --defer                      Defer removal if package is in use\n"
    L"  --preserve-application-data  Keep application data\n"
    L"  --all-users                  Remove the package for all users\n"
    L"  --benchmark                  Display elapsed time\n"
    L"  -nologo, --no-logo           Do not display startup banner or copyright message\n"
    L"  -?, -h, --help               Show command line help\n"
};

constexpr PCWSTR help_Command_Package_Stage{
    L"Description:\n"
    L"  Stage a package\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package stage <PACKAGE> [options]\n"
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
    L"  --benchmark                 Display elapsed time\n"
    L"  -nologo, --no-logo          Do not display startup banner or copyright message\n"
    L"  -?, -h, --help              Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = file|URI\n"
    L"  <PRIORITY> = low|normal|high\n"
    L"  <STUB>     = default|full|stub|preference\n"
};

constexpr PCWSTR help_Command_Package_Status{
    L"Description:\n"
    L"  View or modify package's status\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package status <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark                Display elapsed time\n"
    L"  -nologo, --no-logo         Do not display startup banner or copyright message\n"
    L"  -?, -h, --help             Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  clear <PACKAGE>  Clear package status\n"
    L"  set <PACKAGE>    Set packages status\n"
};

constexpr PCWSTR help_Command_Package_Status_Clear{
    L"Description:\n"
    L"  Clear a package's status\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package status clear <PACKAGE> <STATUS> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark                   Display elapsed time\n"
    L"  -nologo, --no-logo            Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = PackageFullName|file|URI\n"
    L"  <STATUS>   = Comma-delimited list: disabled|licenseissue|modified|tampered\n"
};

constexpr PCWSTR help_Command_Package_Status_Set{
    L"Description:\n"
    L"  Set a package's status\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package status set <PACKAGE> <STATUS> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark                   Display elapsed time\n"
    L"  -nologo, --no-logo            Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGE>  = PackageFullName|file|URI\n"
    L"  <STATUS>   = Comma-delimited list: Disabled|LicenseIssue|Modified|Tampered\n"
};

constexpr PCWSTR help_Command_Package_Verify{
    L"Description:\n"
    L"  Verify a package's integrity\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" package verify <PACKAGEFULLNAME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark                   Display elapsed time\n"
    L"  -nologo, --no-logo            Do not display startup banner or copyright message\n"
    L"  -?, -h, --help                Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <PACKAGEFULLNAME>  = PackageFullName\n"
    L"\n"
    L"Links:\n"
    L"  * https://learn.microsoft.com/uwp/api/windows.applicationmodel.package.verifycontentintegrityasync\n"
};

constexpr PCWSTR help_Command_Provision{
    L"Description:\n"
    L"  View or modify the provisioned list\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" provision <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" provision add <PACKAGEFAMILYNAME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  --defer-registration  Defer automatic registration\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Provision_List{
    L"Description:\n"
    L"  Display the currently provisioned package families\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" provision list [options]\n"
    L"\n"
    L"Options:\n"
    L"  --format=<FORMAT>        Display package format (default=packagefamilyname)\n"
    L"  --glob=<PATTERN>         Display package families matching PATTERN (*,? wildcards)\n"
    L"  --max-version=<VERSION>  Display packages with version <= <VERSION>\n"
    L"  --min-version=<VERSION>  Display packages with version >= <VERSION>\n"
    L"  --timezone=<TIMEZONE>    Display timezone for timestamps (default=local)\n"
    L"  --no-summary             Do not display summary information\n"
    L"  --benchmark              Display elapsed time\n"
    L"  -nologo, --no-logo       Do not display startup banner or copyright message\n"
    L"  -?, -h, --help           Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <TIMEZONE> = local|utc\n"
    L"  <FORMAT> = full|packagefamilyname\n"
    L"  <VERSION> = 0xmmmmnnnnbbbbrrrr or major.minor.build.revision (aka DotQuadNumber, optional ending wildcard (*) e.g. 1.2.* = 1.2.65535.65535)\n"
};

constexpr PCWSTR help_Command_Provision_Remove{
    L"Description:\n"
    L"  Remove a package family from the provisioning list\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" provision remove <PACKAGEFAMILYNAME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Shortcut{
    L"Description:\n"
    L"  Manage Shortcut\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" shortcut <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add  Create a shortcut (.LNK file)\n"
};

constexpr PCWSTR help_Command_Shortcut_Add{
    L"Description:\n"
    L"  Create a shortcut (.LNK file) to run a target command\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" shortcut add <FILE> <TARGET> [options]\n"
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
    L"  --benchmark                  Display elapsed time\n"
    L"  -nologo, --no-logo           Do not display startup banner or copyright message\n"
    L"  -?, -h, --help               Show command line help\n"
    L"\n"
    L"NOTE: URLs only support the --icon option\n"
};

constexpr PCWSTR help_Command_Tool{
    L"Description:\n"
    L"  Install or manage tools that extend the MSIX experience\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" tool <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" tool propertysheet <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
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
    L"  " MSIX_EXE_NAME L" tool propertysheet install [options]\n"
    L"\n"
    L"Options:\n"
    L"  --confirm             The install is approved (required to do the work)\n"
    L"  --path=<FILE>         The path to the MSIX property sheet DLL (default = GetPath(" MSIX_EXE_NAME L".exe) + \\MSIXPropertySheet.dll)\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet_List{
    L"Description:\n"
    L"  Display the installed MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" tool propertysheet list [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Tool_PropertySheet_Uninstall{
    L"Description:\n"
    L"  Uninstall the MSIX property sheet extension\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" tool propertysheet uninstall [options]\n"
    L"\n"
    L"Options:\n"
    L"  --confirm             The uninstall is approved (required to do the work)\n"
    L"  --path=<FILE>         The path to the MSIX property sheet DLL (default = GetPath(" MSIX_EXE_NAME L".exe) + \\MSIXPropertySheet.dll)\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Version{
    L"Description:\n"
    L"  Version information\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" version [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
    L"  -?, -h, --help      Show command line help\n"
};

constexpr PCWSTR help_Command_Volume_Add{
    L"Description:\n"
    L"  Add a package volume\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume add <PATH> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Volume_Default_Get{
    L"Description:\n"
    L"  Display the default package volume\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume default get [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Volume_Default_Set{
    L"Description:\n"
    L"  Set the default package volume\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume default set <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

constexpr PCWSTR help_Command_Volume_Default{
    L"Description:\n"
    L"  View or modify package volume defaults\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume default <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  get             Display the default volume\n"
    L"  set <VOLUME>    Set the default volume\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

constexpr PCWSTR help_Command_Volume_List{
    L"Description:\n"
    L"  Display package volumes\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume list [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
};

constexpr PCWSTR help_Command_Volume_Offline{
    L"Description:\n"
    L"  Set a package volume to offline\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume offline <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

constexpr PCWSTR help_Command_Volume_Online{
    L"Description:\n"
    L"  Set a package volume to online\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume online <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

constexpr PCWSTR help_Command_Volume_Remove{
    L"Description:\n"
    L"  Remove a package volume\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume remove <VOLUME> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

constexpr PCWSTR help_Command_Volume{
    L"Description:\n"
    L"  View or modify package volumes\n"
    L"\n"
    L"Usage:\n"
    L"  " MSIX_EXE_NAME L" volume <command> [options]\n"
    L"\n"
    L"Options:\n"
    L"  --benchmark           Display elapsed time\n"
    L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
    L"  -?, -h, --help        Show command line help\n"
    L"\n"
    L"Commands:\n"
    L"  add <PATH>          Add a package volume\n"
    L"  default             Display or modify the default package volume\n"
    L"  list                Display package volumes\n"
    L"  offline <VOLUME>    Set a package volume offline\n"
    L"  online <VOLUME>     Set a package volume online\n"
    L"  remove <VOLUME>     Remove a package volume\n"
    L"\n"
    L"Arguments:\n"
    L"  <VOLUME> = Volume by drive (E:), path (E:\\What\\Ever) or name (\\\\?\\Volume{7ce02272-043f-11ec-91b1-e8f408dc8470})\n"
};

HRESULT Command_Certificate_Add(PCWSTR filename)
{
    constexpr auto help_string{ help_Command_Certificate_Add };

    if ((CompareStringOrdinal(filename, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

    MSIX::Signing::AddResult result{};
    if (MSIX::IsPackage(filename))
    {
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(MSIX::Signing::AddCertificate(packageReader.get(), result), "%ls", filename);
    }
    else if (MSIX::IsBundle(filename))
    {
        wil::com_ptr_nothrow<IAppxBundleReader> bundleReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, bundleReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(MSIX::Signing::AddCertificate(bundleReader.get(), result), "%ls", filename);
    }

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
    constexpr auto help_string{ help_Command_Certificate_Exists };

    if ((CompareStringOrdinal(filename, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
        if (MSIX::IsPackage(filename))
        {
            wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
            RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
            RETURN_IF_FAILED_MSG(MSIX::Signing::IsCertificateInstalled(packageReader.get(), isInstalled), "%ls", filename);
        }
        else if (MSIX::IsBundle(filename))
        {
            wil::com_ptr_nothrow<IAppxBundleReader> bundleReader;
            RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, bundleReader), "%ls", filename);
            RETURN_IF_FAILED_MSG(MSIX::Signing::IsCertificateInstalled(bundleReader.get(), isInstalled), "%ls", filename);
        }
        else
        {
            UnknownFileType(filename);
        }
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
    constexpr auto help_string{ help_Command_Certificate_List };

    if ((CompareStringOrdinal(filename, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

    const auto isPackage{ MSIX::IsPackage(filename) };
    if (!isPackage && !MSIX::IsBundle(filename))
    {
        UnknownFileType(filename);
    }

    // Classify where the package's trust comes from (read-only; no elevation needed)
    MSIX::SignatureOrigin origin{ MSIX::SignatureOrigin::Unsigned };
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    wil::com_ptr_nothrow<IAppxBundleReader> bundleReader;
    if (isPackage)
    {
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(MSIX::Signing::DetectSignatureOrigin(packageReader.get(), origin), "%ls", filename);
    }
    else
    {
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, bundleReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(MSIX::Signing::DetectSignatureOrigin(bundleReader.get(), origin), "%ls", filename);
    }

    wprintf(L"Package: %ls\n", filename);
    wprintf(L"Origin:  %ls\n", MSIX::ToString(origin));

    if (origin == MSIX::SignatureOrigin::Unsigned)
    {
        wprintf(L"No certificate to list (package is unsigned)\n");
        return S_OK;
    }

    // Extract and display the leaf (signing) certificate carried in the package
    wil::unique_cert_context signingCertificate;
    const HRESULT signerHr{
        isPackage ?
        MSIX::Signing::GetSigningCertificate(packageReader.get(), signingCertificate) :
        MSIX::Signing::GetSigningCertificate(bundleReader.get(), signingCertificate) };
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
    constexpr auto help_string{ help_Command_Certificate_Remove };

    if ((CompareStringOrdinal(filename, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
    else if (MSIX::IsPackage(filename))
    {
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(hr = MSIX::Signing::RemoveCertificate(packageReader.get()), "%ls", filename);
    }
    else if (MSIX::IsBundle(filename))
    {
        wil::com_ptr_nothrow<IAppxBundleReader> bundleReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, bundleReader), "%ls", filename);
        RETURN_IF_FAILED_MSG(hr = MSIX::Signing::RemoveCertificate(bundleReader.get()), "%ls", filename);
    }
    else
    {
        UnknownFileType(filename);
    }
    wprintf(L"Certificate with thumbprint '%ls' %ls\n", filename + 2, (hr == S_OK ? L"is removed" : (hr == S_FALSE ? L"is not found" : L"???")));
    return S_OK;
}

HRESULT Command_Certificate(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Certificate };

    if (argc < 3)
    {
        Help(help_string);
    }

    PCWSTR action{ argv[2] };
    HRESULT (*command)(PCWSTR){};
    PCWSTR commandHelp{};
    if (CompareStringOrdinal(action, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        command = Command_Certificate_Add;
        commandHelp = help_Command_Certificate_Add;
    }
    else if (CompareStringOrdinal(action, -1, L"exists", -1, FALSE) == CSTR_EQUAL)
    {
        command = Command_Certificate_Exists;
        commandHelp = help_Command_Certificate_Exists;
    }
    else if (CompareStringOrdinal(action, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        command = Command_Certificate_List;
        commandHelp = help_Command_Certificate_List;
    }
    else if (CompareStringOrdinal(action, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        command = Command_Certificate_Remove;
        commandHelp = help_Command_Certificate_Remove;
    }
    else
    {
        Help(help_string);
    }

    if (argc < 4)
    {
        Help(commandHelp);
    }
    PCWSTR filename{ argv[3] };
    if ((CompareStringOrdinal(filename, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(filename, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(commandHelp);
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
            Help(commandHelp);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    return command(filename);
}

#if defined(MSIXADMIN) && (MSIXADMIN == 0)
#define MSIXADMIN_ELEVATION         L"*"
#define MSIXADMIN_ELEVATION_MESSAGE L"\nNOTE: * = Requires elevation\n"
#else
#define MSIXADMIN_ELEVATION         L""
#define MSIXADMIN_ELEVATION_MESSAGE L""
#endif

HRESULT Command_Help_Commands_Tree(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Help_Commands_Tree };

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
            Help(help_string);
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
        wprintf(MSIX_EXE_NAME L"\n"
                L"+--certificate\n"
                L"|  +--add" MSIXADMIN_ELEVATION L"\n"
                L"|  +--exists" MSIXADMIN_ELEVATION L"\n"
                L"|  +--list\n"
                L"|  +--remove" MSIXADMIN_ELEVATION L"\n"
                L"+--package\n"
                L"|  +--add\n"
                L"|  +--list\n"
                L"|  +--move\n"
                L"|  +--register\n"
                L"|  +--remove\n"
                L"|  +--stage\n"
                L"|  +--status\n"
                L"|    +--clear\n"
                L"|    +--set\n"
                L"|  +--verify\n"
                L"+--provision\n"
                L"|  +--add" MSIXADMIN_ELEVATION L"\n"
                L"|  +--list" MSIXADMIN_ELEVATION L"\n"
                L"|  +--remove" MSIXADMIN_ELEVATION L"\n"
                L"+--shortcut\n"
                L"|  +--add\n"
                L"+--tool\n"
                L"|  +--propertysheet\n"
                L"|     +--install" MSIXADMIN_ELEVATION L"\n"
                L"|     +--list\n"
                L"|     +--uninstall" MSIXADMIN_ELEVATION L"\n"
                L"+--version\n"
                L"+--volume\n"
                L"|  +--add\n"
                L"|  +--default\n"
                L"|     +--get\n"
                L"|     +--set\n"
                L"|  +--list\n"
                L"|  +--offline\n"
                L"|  +--online\n"
                L"|  +--remove\n"
                MSIXADMIN_ELEVATION_MESSAGE);
    }
    else
    {
        _setmode(_fileno(stdout), _O_U16TEXT);
        wprintf(MSIX_EXE_NAME L"\n"
                L"\u251C\u2500\u2500certificate\n"
                L"\u2502  \u251C\u2500\u2500add" MSIXADMIN_ELEVATION L"\n"
                L"\u2502  \u251C\u2500\u2500exists" MSIXADMIN_ELEVATION L"\n"
                L"\u2502  \u251C\u2500\u2500list\n"
                L"\u2502  \u2514\u2500\u2500remove" MSIXADMIN_ELEVATION L"\n"
                L"\u251C\u2500\u2500package\n"
                L"\u2502  \u251C\u2500\u2500add\n"
                L"\u2502  \u251C\u2500\u2500list\n"
                L"\u2502  \u251C\u2500\u2500move\n"
                L"\u2502  \u251C\u2500\u2500register\n"
                L"\u2502  \u251C\u2500\u2500remove\n"
                L"\u2502  \u251C\u2500\u2500stage\n"
                L"\u2502  \u251C\u2500\u2500status\n"
                L"\u2502  \u2502  \u251C\u2500\u2500clear\n"
                L"\u2502  \u2502  \u2514\u2500\u2500set\n"
                L"\u2502  \u2514\u2500\u2500verify\n"
                L"\u251C\u2500\u2500provision\n"
                L"\u2502  \u251C\u2500\u2500add" MSIXADMIN_ELEVATION L"\n"
                L"\u2502  \u251C\u2500\u2500list" MSIXADMIN_ELEVATION L"\n"
                L"\u2502  \u2514\u2500\u2500remove" MSIXADMIN_ELEVATION L"\n"
                L"\u251C\u2500\u2500shortcut\n"
                L"\u2502  \u2514\u2500\u2500add\n"
                L"\u251C\u2500\u2500tool\n"
                L"\u2502  \u2514\u2500\u2500propertysheet\n"
                L"\u2502     \u251C\u2500\u2500install" MSIXADMIN_ELEVATION L"\n"
                L"\u2502     \u251C\u2500\u2500list\n"
                L"\u2502     \u2514\u2500\u2500uninstall" MSIXADMIN_ELEVATION L"\n"
                L"\u251C\u2500\u2500version\n"
                L"\u2514\u2500\u2500volume\n"
                L"   \u251C\u2500\u2500add\n"
                L"   \u251C\u2500\u2500default\n"
                L"   \u2502  \u251C\u2500\u2500get\n"
                L"   \u2502  \u2514\u2500\u2500set\n"
                L"   \u251C\u2500\u2500list\n"
                L"   \u251C\u2500\u2500offline\n"
                L"   \u251C\u2500\u2500online\n"
                L"   \u2514\u2500\u2500remove\n"
                MSIXADMIN_ELEVATION_MESSAGE);
    }

    return S_OK;
}

HRESULT Command_Help_Commands(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Help_Commands };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[3] };
    if (CompareStringOrdinal(command, -1, L"tree", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Help_Commands_Tree(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Help(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Help };

    if (argc < 3)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"commands", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Help_Commands(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT PackageToPackageFullName(IAppxPackageReader* packageReader, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
{
    wil::com_ptr_nothrow<IAppxManifestReader> manifestReader;
    RETURN_IF_FAILED(packageReader->GetManifest(&manifestReader));
    wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
    RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
    RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(packageFullName)));
    return S_OK;
}

HRESULT PackageToPackageFullName(IAppxBundleReader* bundleReader, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
{
    wil::com_ptr_nothrow<IAppxBundleManifestReader> manifestReader;
    RETURN_IF_FAILED(bundleReader->GetManifest(&manifestReader));
    wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
    RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
    RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(packageFullName)));
    return S_OK;
}

HRESULT PackageToPackageFullName(PCWSTR filename, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
{
    if (MSIX::IsPackage(filename))
    {
        wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
        RETURN_IF_FAILED(PackageToPackageFullName(packageReader.get(), packageFullName));
    }
    else if (MSIX::IsBundle(filename))
    {
        wil::com_ptr_nothrow<IAppxBundleReader> bundleReader;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, bundleReader), "%ls", filename);
        RETURN_IF_FAILED(PackageToPackageFullName(bundleReader.get(), packageFullName));
    }
    else
    {
        RETURN_HR_MSG(E_INVALIDARG, "%ls", filename);
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
    constexpr auto help_string{ help_Command_Package_Add };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[3] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_AddPackageOptions));
        RETURN_IF_FAILED(inspectable.query_to(addPackageOptions.put()));
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
            Help(help_string);
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
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions2> addPackageOptions2;
    if (limitToExisting)
    {
        const HRESULT hr{ addPackageOptions.query_to(addPackageOptions2.put()) };
        if (hr == E_NOTIMPL)
        {
            UnsupportedArgument(L"----limit-to-existing requires Windows 11 22H2 (build 22621) or newer");
        }
        else
        {
            RETURN_IF_FAILED(hr);
        }
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions3> addPackageOptions3;
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
        const HRESULT hr{ addPackageOptions.query_to(addPackageOptions3.put()) };
        if (hr == E_NOTIMPL)
        {
            UnsupportedArgument(L"--priority=<PRIORITY> requires Windows 11 24H2 (build 26100) or newer");
        }
        else
        {
            RETURN_IF_FAILED(hr);
        }
    }

    if (logo)
    {
        ShowLogo();
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(package, packageUri));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager9.put()));
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
        RETURN_IF_FAILED(ToPackageVolume(target, nullptr, packageManager9.get(), targetVolume));
        RETURN_IF_FAILED(addPackageOptions->put_TargetVolume(targetVolume.get()));
    }
    if (limitToExisting)
    {
        RETURN_IF_FAILED(addPackageOptions2->put_LimitToExistingPackages(true));
    }
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
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
    constexpr auto help_string{ help_Command_Package_List };

    enum class PackageDisplayFormat { Full = 0, PackageFullName = 1, PackageFamilyName = 2 };

    DependencyType dependencies{ DependencyType::All };
    PackageDisplayFormat format{};
    PCWSTR glob_name{};
    PCWSTR glob_packageFamilyName{};
    PCWSTR glob_packageFullName{};
    bool logo{ true };
    UINT64 max_version{ UINT64_MAX };
    UINT64 min_version{};
    ABI::Windows::Management::Deployment::PackageTypes packageTypes{};
    ReferenceType references{ ReferenceType::All };
    bool references_parameter{};
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--dependencies", -1, FALSE) == CSTR_EQUAL)
        {
            dependencies = DependencyType::All;
        }
        else if (wil::string_starts_with(arg, L"--dependencies:"))
        {
            if (FAILED_LOG(ToDependencyTypes(arg + (ARRAYSIZE(L"--dependencies:") - 1), dependencies)))
            {
                UnknownArgument(arg);
            }
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
        else if (wil::string_starts_with(arg, L"--glob="))
        {
            glob_name = arg + (ARRAYSIZE(L"--glob=") - 1);
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
        else if (wil::string_starts_with(arg, L"--max-version="))
        {
            if (FAILED(ToVersion(arg + (ARRAYSIZE(L"--max-version=") - 1), max_version)))
            {
                UnknownArgument(arg);
            }
        }
        else if (wil::string_starts_with(arg, L"--min-version="))
        {
            if (FAILED(ToVersion(arg + (ARRAYSIZE(L"--min-version=") - 1), min_version)))
            {
                UnknownArgument(arg);
            }
        }
        else if (CompareStringOrdinal(arg, -1, L"--no-dependencies", -1, FALSE) == CSTR_EQUAL)
        {
            dependencies = DependencyType::None;
        }
        else if (CompareStringOrdinal(arg, -1, L"--no-references", -1, FALSE) == CSTR_EQUAL)
        {
            references = ReferenceType::None;
        }
        else if (wil::string_starts_with(arg, L"--package-type="))
        {
            packageTypes = ToPackageTypes(arg + (ARRAYSIZE(L"--package-type=") - 1));
            if (packageTypes == PackageTypes_Error)
            {
                UnknownArgument(arg);
            }
        }
        else if (CompareStringOrdinal(arg, -1, L"--references", -1, FALSE) == CSTR_EQUAL)
        {
            references = ReferenceType::All;
        }
        else if (wil::string_starts_with(arg, L"--references:"))
        {
            if (FAILED_LOG(ToReferenceTypes(arg + (ARRAYSIZE(L"--references:") - 1), references)))
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
        else if (CompareStringOrdinal(arg, -1, L"--", -1, FALSE) == CSTR_EQUAL)
        {
            ++argn;
            break;
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else if (arg[0] != L'-')
        {
            break;
        }
        else
        {
            UnknownArgument(argv[argn]);
        }
    }
    if (argn < argc)
    {
        glob_name = argv[argn++];
    }
    if (argn < argc)
    {
        UnknownArgument(argv[argn]);
    }

    if ((dependencies != DependencyType::None) && user)
    {
        wprintf(L"Error 0x00000001: Incompatible argument\n"
                L"    Full command line: '%ls'\n"
                L"Argument: --dependencies and --user=<SID> are not compatible\n",
                GetCommandLine());
        ::ExitProcess(1);
    }

    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_References;
    if (WI_IsAnyFlagSet(references, ReferenceType::Framework | ReferenceType::HostRuntime | ReferenceType::Optional | ReferenceType::Resource))
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        const HRESULT hr{ ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions) };
        if (hr == REGDB_E_CLASSNOTREG)
        {
            if (references_parameter)
            {
                UnsupportedArgument(L"--references=<f|h|o|r> requires Windows 11 22H2 (build 22621) or newer");
            }
            else
            {
                WI_ClearAllFlags(references, ReferenceType::Framework | ReferenceType::HostRuntime | ReferenceType::Optional | ReferenceType::Resource);
            }
        }
        else
        {
            RETURN_IF_FAILED(hr);
        }
    }

    if (logo)
    {
        ShowLogo();
    }

    std::uint32_t countDisplayed{};

    wil::com_ptr_nothrow<IInspectable> inspectablePackageManager;
    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterable<ABI::Windows::ApplicationModel::Package*>> iterablePackages;
    {
        RETURN_IF_FAILED(ActivateInstance(inspectablePackageManager, RuntimeClass_Windows_Management_Deployment_PackageManager));

        // Choose the optimal FindPackage*() variant given our inputs/options
        if (CompareStringOrdinal(user, -1, L"*", -1, FALSE) == CSTR_EQUAL)
        {
            // No user context i.e. all packages on the machine
            if (packageTypes == ABI::Windows::Management::Deployment::PackageTypes_None)
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager> packageManager;
                RETURN_IF_FAILED(inspectablePackageManager.query_to(packageManager.put()));
                RETURN_IF_FAILED(packageManager->FindPackages(iterablePackages.put()));
            }
            else
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
                RETURN_IF_FAILED(inspectablePackageManager.query_to(packageManager2.put()));
                RETURN_IF_FAILED(packageManager2->FindPackagesWithPackageTypes(packageTypes, iterablePackages.put()));
            }
        }
        else
        {
            // User context
            HSTRING_HEADER userHeader{};
            HSTRING userHString{};
            RETURN_IF_FAILED(wil::to_hstring_reference(user ? user : L"", userHeader, userHString));
            if (packageTypes == ABI::Windows::Management::Deployment::PackageTypes_None)
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager> packageManager;
                RETURN_IF_FAILED(inspectablePackageManager.query_to(packageManager.put()));
                RETURN_IF_FAILED(packageManager->FindPackagesByUserSecurityId(userHString, iterablePackages.put()));
            }
            else
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
                RETURN_IF_FAILED(inspectablePackageManager.query_to(packageManager2.put()));
                RETURN_IF_FAILED(packageManager2->FindPackagesByUserSecurityIdWithPackageTypes(userHString, packageTypes, iterablePackages.put()));
            }
        }
    }
    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IVector<ABI::Windows::ApplicationModel::Package*>> packages;
    RETURN_IF_FAILED(iterablePackages.query_to(&packages));

    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_Dependencies_Frameworks;
    if (WI_IsFlagSet(dependencies, DependencyType::Framework))
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
        RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_Dependencies_Frameworks.put()));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Frameworks->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependencies));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Frameworks->put_IncludeFrameworks(true));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_Dependencies_HostRuntimes;
    if (WI_IsFlagSet(dependencies, DependencyType::HostRuntime))
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
        RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_Dependencies_HostRuntimes.put()));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_HostRuntimes->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependencies));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_HostRuntimes->put_IncludeHostRuntimes(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_Dependencies_Optionals;
    if (WI_IsFlagSet(dependencies, DependencyType::Optional))
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
        RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_Dependencies_Optionals.put()));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Optionals->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependencies));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Optionals->put_IncludeOptionals(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_Dependencies_Resources;
    if (WI_IsFlagSet(dependencies, DependencyType::Resource))
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
        RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_Dependencies_Resources.put()));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Resources->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependencies));
        RETURN_IF_FAILED(findRelatedPackagesOptions_Dependencies_Resources->put_IncludeResources(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }

    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_References_Frameworks;
    if (WI_IsFlagSet(references, ReferenceType::Framework))
    {
        if (findRelatedPackagesOptions_References)
        {
            findRelatedPackagesOptions_References_Frameworks = wistd::move(findRelatedPackagesOptions_References);
        }
        else
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
            RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_References_Frameworks.put()));
        }
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Frameworks->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Frameworks->put_IncludeFrameworks(true));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_References_HostRuntimes;
    if (WI_IsFlagSet(references, ReferenceType::HostRuntime))
    {
        if (findRelatedPackagesOptions_References)
        {
            findRelatedPackagesOptions_References_HostRuntimes = wistd::move(findRelatedPackagesOptions_References);
        }
        else
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
            RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_References_HostRuntimes.put()));
        }
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_HostRuntimes->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_HostRuntimes->put_IncludeHostRuntimes(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_References_Optionals;
    if (WI_IsFlagSet(references, ReferenceType::Optional))
    {
        if (findRelatedPackagesOptions_References)
        {
            findRelatedPackagesOptions_References_Optionals = wistd::move(findRelatedPackagesOptions_References);
        }
        else
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
            RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_References_Optionals.put()));
        }
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Optionals->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Optionals->put_IncludeOptionals(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }
    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IFindRelatedPackagesOptions> findRelatedPackagesOptions_References_Resources;
    if (WI_IsFlagSet(references, ReferenceType::Resource))
    {
        if (findRelatedPackagesOptions_References)
        {
            findRelatedPackagesOptions_References_Resources = wistd::move(findRelatedPackagesOptions_References);
        }
        else
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_ApplicationModel_FindRelatedPackagesOptions));
            RETURN_IF_FAILED(inspectable.query_to(findRelatedPackagesOptions_References_Resources.put()));
        }
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Resources->put_Relationship(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
        RETURN_IF_FAILED(findRelatedPackagesOptions_References_Resources->put_IncludeResources(ABI::Windows::ApplicationModel::PackageRelationship_Dependents));
    }

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
                if (name)
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

        if ((max_version != UINT64_MAX) || (min_version != 0))
        {
            ABI::Windows::ApplicationModel::PackageVersion packageVersion{};
            RETURN_IF_FAILED(packageId->get_Version(&packageVersion));
            const auto version{ ToVersion(packageVersion) };
            if ((version < min_version) || (version > max_version))
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
            wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
            wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager12> packageManager12;
            HRESULT hr{ LOG_IF_FAILED(inspectablePackageManager.query_to(&packageManager9)) };
            if (SUCCEEDED(hr))
            {
                hr = LOG_IF_FAILED(inspectablePackageManager.query_to(&packageManager12));
            }
            if (hr == E_NOINTERFACE)
            {
                hr = S_OK;
            }
            PrintPackage(packageX, packageId.get(), timeZoneIsLocal, user, packageManager9.get(), packageManager12.get(), dependencies, references,
                         findRelatedPackagesOptions_Dependencies_Frameworks.get(), findRelatedPackagesOptions_Dependencies_HostRuntimes.get(),
                         findRelatedPackagesOptions_Dependencies_Optionals.get(), findRelatedPackagesOptions_Dependencies_Resources.get(),
                         findRelatedPackagesOptions_References_Frameworks.get(), findRelatedPackagesOptions_References_HostRuntimes.get(),
                         findRelatedPackagesOptions_References_Optionals.get(), findRelatedPackagesOptions_References_Resources.get());
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
    constexpr auto help_string{ help_Command_Package_Move };

    if (argc < 5)
    {
        Help(help_string);
    }

    PCWSTR packageFullName{ argv[3] };
    if ((CompareStringOrdinal(packageFullName, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFullName, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFullName, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }
    PCWSTR target{ argv[4] };
    if ((CompareStringOrdinal(target, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(target, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(target, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--force", -1, FALSE) == CSTR_EQUAL)
        {
            force = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--retain-files-on-failure", -1, FALSE) == CSTR_EQUAL)
        {
            retainFilesOnFailure = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
    wil::unique_hstring packageVolumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(target, nullptr, packageManager3.get(), targetVolume, packageVolumePathHString));

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
    constexpr auto help_string{ help_Command_Package_Register };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[3] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_AddPackageOptions));
        RETURN_IF_FAILED(inspectable.query_to(registerPackageOptions.put()));
    }

    int argn{ 4 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_string);
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
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager9.put()));
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
        RETURN_IF_FAILED(ToPackageVolume(appDataTarget, nullptr, packageManager9.get(), appDataVolume));
        RETURN_IF_FAILED(registerPackageOptions->put_AppDataVolume(appDataVolume.get()));
    }

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    if (packageFullName)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
        RETURN_IF_FAILED(packageManager9.query_to(packageManager2.put()));
        HSTRING_HEADER packageFullNameHeader{};
        HSTRING packageFullNameHString{};
        RETURN_IF_FAILED(wil::to_hstring_reference(packageFullName, packageFullNameHeader, packageFullNameHString));
        RETURN_IF_FAILED(packageManager2->RegisterPackageByFullNameAsync(packageFullNameHString, dependenciesNames.get(), deploymentOptions, deploymentOperation.put()));
    }
    else if (packageFamilyName)
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager5> packageManager5;
        RETURN_IF_FAILED(packageManager9.query_to(packageManager5.put()));
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
    constexpr auto help_string{ help_Command_Package_Remove };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[3] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
            Help(help_string);
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
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        RETURN_IF_FAILED(PackageToPackageFullName(package, packageFullNameBuffer));
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

    wil::com_ptr_nothrow<IInspectable> inspectable;
    RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> packageManager2;
    RETURN_IF_FAILED(inspectable.query_to(packageManager2.put()));

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
    constexpr auto help_string{ help_Command_Package_Stage };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[3] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_StagePackageOptions));
        RETURN_IF_FAILED(inspectable.query_to(stagePackageOptions.put()));
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
            Help(help_string);
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
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IStagePackageOptions3> stagePackageOptions3;
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
        const HRESULT hr{ stagePackageOptions.query_to(stagePackageOptions3.put()) };
        if (hr == E_NOTIMPL)
        {
            UnsupportedArgument(L"--priority=<PRIORITY> requires Windows 11 24H2 (build 26100) or newer");
        }
        else
        {
            RETURN_IF_FAILED(hr);
        }
    }

    if (logo)
    {
        ShowLogo();
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(package, packageUri));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager9.put()));
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
        RETURN_IF_FAILED(ToPackageVolume(target, nullptr, packageManager9.get(), targetVolume));
        RETURN_IF_FAILED(stagePackageOptions->put_TargetVolume(targetVolume.get()));
    }
    if (priority != ABI::Windows::Management::Deployment::PackageOperationPriority_Normal)
    {
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

HRESULT ToPackageStatusToModify(PCWSTR string, ABI::Windows::Management::Deployment::PackageStatus& status)
{
    status = ABI::Windows::Management::Deployment::PackageStatus_OK;

    RETURN_HR_IF_NULL(E_INVALIDARG, string);

    for (PCWSTR s = string; *s != L'\0';)
    {
        if (wil::string_starts_with(s, L"disabled"))
        {
            WI_SetFlag(status, ABI::Windows::Management::Deployment::PackageStatus_Disabled);
            s += ARRAYSIZE(L"disabled") - 1;
        }
        else if (wil::string_starts_with(s, L"licenseissue"))
        {
            WI_SetFlag(status, ABI::Windows::Management::Deployment::PackageStatus_LicenseIssue);
            s += ARRAYSIZE(L"licenseissue") - 1;
        }
        else if (wil::string_starts_with(s, L"modified"))
        {
            WI_SetFlag(status, ABI::Windows::Management::Deployment::PackageStatus_Modified);
            s += ARRAYSIZE(L"modified") - 1;
        }
        else if (wil::string_starts_with(s, L"tampered"))
        {
            WI_SetFlag(status, ABI::Windows::Management::Deployment::PackageStatus_Tampered);
            s += ARRAYSIZE(L"tampered") - 1;
        }
        else
        {
            UnknownArgument(string);
        }
        if (*s)
        {
            if (*s++ != L',')
            {
                UnknownArgument(string);
            }
        }
    }
    return S_OK;
}

HRESULT ClearOrSetPackageStatus(PCWSTR packageFullName, ABI::Windows::Management::Deployment::PackageStatus packageStatus, bool setStatus)
{
    HSTRING_HEADER packageFullNameHeader{};
    HSTRING packageFullNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(packageFullName, packageFullNameHeader, packageFullNameHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    if (setStatus)
    {
        RETURN_IF_FAILED(packageManager3->SetPackageStatus(packageFullNameHString, packageStatus));
    }
    else
    {
        RETURN_IF_FAILED(packageManager3->ClearPackageStatus(packageFullNameHString, packageStatus));
    }
    return S_OK;
}

HRESULT ClearPackageStatus(PCWSTR packageFullName, ABI::Windows::Management::Deployment::PackageStatus packageStatus)
{
    RETURN_IF_FAILED(ClearOrSetPackageStatus(packageFullName, packageStatus, false));
    return S_OK;
}

HRESULT SetPackageStatus(PCWSTR packageFullName, ABI::Windows::Management::Deployment::PackageStatus packageStatus)
{
    RETURN_IF_FAILED(ClearOrSetPackageStatus(packageFullName, packageStatus, true));
    return S_OK;
}

HRESULT Command_Package_Status_Clear(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Package_Status_Clear };

    if (argc < 6)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[4] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }
    PCWSTR status{ argv[5] };
    if ((CompareStringOrdinal(status, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(status, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(status, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

    bool logo{ true };

    int argn{ 6 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    auto packageStatus{ ABI::Windows::Management::Deployment::PackageStatus_OK };
    RETURN_IF_FAILED(ToPackageStatusToModify(status, packageStatus));

    if (logo)
    {
        ShowLogo();
    }

    PCWSTR packageFullName{};
    wil::unique_cotaskmem_ptr<WCHAR[]> packageFullNameBuffer;
    if (::VerifyPackageFullName(package) == ERROR_SUCCESS)
    {
        packageFullName = package;
    }
    else if (IsPackageFileOrUri(package))
    {
        RETURN_IF_FAILED(PackageToPackageFullName(package, packageFullNameBuffer));
        packageFullName = packageFullNameBuffer.get();
    }
    else
    {
        UnknownArgument(package);
    }
    RETURN_IF_FAILED(ClearPackageStatus(packageFullName, packageStatus));
    wprintf(L"Package '%ls' cleared status %ls\n", packageFullName, status);
    return S_OK;
}

HRESULT Command_Package_Status_Set(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Package_Status_Set };

    if (argc < 6)
    {
        Help(help_string);
    }

    PCWSTR package{ argv[4] };
    if ((CompareStringOrdinal(package, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(package, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }
    PCWSTR status{ argv[5] };
    if ((CompareStringOrdinal(status, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(status, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(status, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

    bool logo{ true };

    int argn{ 6 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    auto packageStatus{ ABI::Windows::Management::Deployment::PackageStatus_OK };
    RETURN_IF_FAILED(ToPackageStatusToModify(status, packageStatus));

    if (logo)
    {
        ShowLogo();
    }

    HSTRING_HEADER packageFullNameHeader{};
    HSTRING packageFullNameHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(package, packageFullNameHeader, packageFullNameHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    PCWSTR packageFullName{};
    wil::unique_cotaskmem_ptr<WCHAR[]> packageFullNameBuffer;
    if (::VerifyPackageFullName(package) == ERROR_SUCCESS)
    {
        packageFullName = package;
    }
    else if (IsPackageFileOrUri(package))
    {
        RETURN_IF_FAILED(PackageToPackageFullName(package, packageFullNameBuffer));
        packageFullName = packageFullNameBuffer.get();
    }
    else
    {
        UnknownArgument(package);
    }
    RETURN_IF_FAILED(SetPackageStatus(packageFullName, packageStatus));
    wprintf(L"Package '%ls' set status %ls\n", packageFullName, status);
    return S_OK;
}

HRESULT Command_Package_Status(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Package_Status };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[3] };
    if (CompareStringOrdinal(command, -1, L"clear", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Status_Clear(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"set", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Status_Set(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Package_Verify(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Package_Verify };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR packageFullName{ argv[3] };
    if ((CompareStringOrdinal(packageFullName, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFullName, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFullName, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager> packageManager;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage4> package4;
    {
        wil::com_ptr_nothrow<ABI::Windows::ApplicationModel::IPackage> package;
        RETURN_IF_FAILED(packageManager->FindPackageByPackageFullName(packageFullNameHString, package.put()));
        RETURN_IF_FAILED(package.query_to(package4.put()));
    }
    wil::com_ptr_nothrow<ABI::Windows::Foundation::__FIAsyncOperation_1_boolean_t> operation;
    RETURN_IF_FAILED(package4->VerifyContentIntegrityAsync(operation.put()));
    boolean verified{};
    RETURN_IF_FAILED(wil::wait_for_completion_nothrow(operation.get(), &verified));
    wprintf(L"Package '%ls' integrity is%ls modified\n", packageFullName, verified ? L" NOT" : L"");
    return verified ? S_OK : S_FALSE;
}

HRESULT Command_Package(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Package };

    if (argc < 3)
    {
        Help(help_string);
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
    else if (CompareStringOrdinal(command, -1, L"status", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Status(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"verify", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Package_Verify(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Provision_Add(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Provision_Add };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR packageFamilyName{ argv[3] };
    if ((CompareStringOrdinal(packageFamilyName, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFamilyName, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFamilyName, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--defer-registration", -1, FALSE) == CSTR_EQUAL)
        {
            deferRegistration = true;
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageAllUserProvisioningOptions> packageAllUserProvisioningOptions;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageAllUserProvisioningOptions2> packageAllUserProvisioningOptions2;
    if (deferRegistration)
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        HRESULT hr{ ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageAllUserProvisioningOptions) };
        if (SUCCEEDED(hr))
        {
            if (SUCCEEDED(hr = inspectable.query_to(packageAllUserProvisioningOptions.put())))
            {
                hr = inspectable.query_to(packageAllUserProvisioningOptions2.put());
            }
        }
        if (hr == E_NOTIMPL)
        {
            UnsupportedArgument(L"--defer requires Windows 11 24H2 (build 26100) or newer");
        }
        else
        {
            RETURN_IF_FAILED(hr);
        }
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
        RETURN_IF_FAILED(packageAllUserProvisioningOptions2->put_DeferAutomaticRegistration(true));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager10> packageManager10;
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
            RETURN_IF_FAILED(inspectable.query_to(packageManager10.put()));
        }
        RETURN_IF_FAILED(packageManager10->ProvisionPackageForAllUsersWithOptionsAsync(packageFamilyNameHString, packageAllUserProvisioningOptions.get(), deploymentOperation.put()));
    }
    else
    {
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager6> packageManager6;
        {
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
            RETURN_IF_FAILED(inspectable.query_to(packageManager6.put()));
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
    constexpr auto help_string{ help_Command_Provision_List };

    enum class PackageDisplayFormat { PackageFamilyName = 0, Full = 1 };

    bool logo{ true };
    PackageDisplayFormat format{};
    PCWSTR glob{};
    UINT64 max_version{ UINT64_MAX };
    UINT64 min_version{};
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
            Help(help_string);
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
        else if (wil::string_starts_with(arg, L"--max-version="))
        {
            if (FAILED(ToVersion(arg + (ARRAYSIZE(L"--max-version=") - 1), max_version)))
            {
                UnknownArgument(arg);
            }
        }
        else if (wil::string_starts_with(arg, L"--min-version="))
        {
            if (FAILED(ToVersion(arg + (ARRAYSIZE(L"--min-version=") - 1), min_version)))
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
        else if (CompareStringOrdinal(arg, -1, L"--no-summary", -1, FALSE) == CSTR_EQUAL)
        {
            summary = false;
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> packageManager9;
        RETURN_IF_FAILED(inspectable.query_to(packageManager9.put()));
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
        if ((max_version != UINT64_MAX) || (min_version != 0))
        {
            ABI::Windows::ApplicationModel::PackageVersion packageVersion{};
            RETURN_IF_FAILED(packageId->get_Version(&packageVersion));
            const auto version{ ToVersion(packageVersion) };
            if ((version < min_version) || (version > max_version))
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
    constexpr auto help_string{ help_Command_Provision_Remove };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR packageFamilyName{ argv[3] };
    if ((CompareStringOrdinal(packageFamilyName, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFamilyName, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(packageFamilyName, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager8> packageManager8;
        RETURN_IF_FAILED(inspectable.query_to(packageManager8.put()));
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
    constexpr auto help_string{ help_Command_Provision };

    if (argc < 3)
    {
        Help(help_string);
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
        Help(help_string);
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
    constexpr auto help_string{ help_Command_Shortcut_Add };

    if (argc < 5)
    {
        Help(help_string);
    }

    bool logo{ true };
    PCWSTR file{ argv[3] };
    if ((CompareStringOrdinal(file, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(file, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(file, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }
    PCWSTR target{ argv[4] };
    if ((CompareStringOrdinal(target, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(target, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(target, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

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
            Help(help_string);
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
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
            Help(help_string);
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
            wil::com_ptr_nothrow<IPropertySetStorage> propertySetStorage;
            RETURN_IF_FAILED(url.query_to(&propertySetStorage));
            wil::com_ptr_nothrow<IPropertyStorage> propertyStorage;
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

        RETURN_IF_FAILED(url.query_to(&persistFile));
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
            RETURN_IF_FAILED(shellLink.query_to(&propertyStore));
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
            RETURN_IF_FAILED(shellLink.query_to(&dataList));
            DWORD flags{};
            RETURN_IF_FAILED(dataList->GetFlags(&flags));
            flags |= SLDF_RUNAS_USER;
            RETURN_IF_FAILED(dataList->SetFlags(flags));
        }

        RETURN_IF_FAILED(shellLink.query_to(&persistFile));
    }
    RETURN_IF_FAILED(persistFile->Save(file, TRUE));
    wprintf(L"Shortcut '%ls' is created\n", file);

    return S_OK;
}

HRESULT Command_Shortcut(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Shortcut };

    if (argc < 3)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Shortcut_Add(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Tool_PropertySheet_Install(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Tool_PropertySheet_Install };

    if (argc < 4)
    {
        Help(help_string);
    }

    bool confirm{};
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--confirm", -1, FALSE) == CSTR_EQUAL)
        {
            confirm = true;
        }
        else if (wil::string_starts_with(arg, L"--path="))
        {
            path = arg + (ARRAYSIZE(L"--path=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    if (!confirm)
    {
        Help(help_string);
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
    constexpr auto help_string{ help_Command_Tool_PropertySheet_List };

    if (argc < 4)
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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
        PCWSTR fileTypes[]{ L".appx", L".appxbundle", L".msix", L".msixbundle" };
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
    constexpr auto help_string{ help_Command_Tool_PropertySheet_Uninstall };

    if (argc < 4)
    {
        Help(help_string);
    }

    bool confirm{};
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--confirm", -1, FALSE) == CSTR_EQUAL)
        {
            confirm = true;
        }
        else if (wil::string_starts_with(arg, L"--path="))
        {
            path = arg + (ARRAYSIZE(L"--path=") - 1);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    if (!confirm)
    {
        Help(help_string);
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
    constexpr auto help_string{ help_Command_Tool_PropertySheet };

    if (argc < 4)
    {
        Help(help_string);
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
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Tool(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Tool };

    if (argc < 3)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"propertysheet", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Tool_PropertySheet(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT Command_Version(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Version };

    if (argc < 2)
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

void PrintVolumeKeyValueError(PCWSTR key, HRESULT hr)
{
    wil::unique_hlocal_string message{ wil::format_message_nothrow(hr) };
    wprintf(L"%-30ls : ***ERROR 0x%08X %ls", key, hr, message.get());
}

void PrintVolumeValue(PCWSTR key, HRESULT hr, PCWSTR value)
{
    if (FAILED(hr))
    {
        PrintVolumeKeyValueError(key, hr);
    }
    else
    {
        wprintf(L"%-30ls : %ls\n", key, value);
    }
}

void PrintVolumeValue(PCWSTR key, HRESULT hr, const wil::unique_hstring& value)
{
    if (FAILED(hr))
    {
        PrintVolumeKeyValueError(key, hr);
    }
    else
    {
        PrintVolumeValue(key, hr, WindowsGetStringRawBuffer(value.get(), nullptr));
    }
}

void PrintVolumeValue(PCWSTR key, HRESULT hr, const boolean& value)
{
    if (FAILED(hr))
    {
        PrintVolumeKeyValueError(key, hr);
    }
    else
    {
        wprintf(L"%-30ls : %ls\n", key, value ? L"Yes" : L"No");
    }
}

constexpr PCWSTR ToDriveTypeString(UINT driveType)
{
    constexpr PCWSTR driveTypes[]{ L"Unknown", L"No volume mounted at path", L"Removable", L"Fixed", L"Remote", L"CD-ROM", L"RAM disk" };
    return driveType < ARRAYSIZE(driveTypes) ? driveTypes[driveType] : L"???";
}

void PrintVolume(wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume>& volume)
{
    if (!volume)
    {
        return;
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume2> volume2;
    const HRESULT hrVolume2{ LOG_IF_FAILED(volume.query_to(volume2.put())) };

    wil::unique_hstring string;
    HRESULT hr{ LOG_IF_FAILED(volume->get_PackageStorePath(wil::out_param(string))) };
    PrintVolumeValue(L"Path", hr, string);
    boolean isOffline{};
    HRESULT hrIsOffline{ LOG_IF_FAILED(volume->get_IsOffline(&isOffline)) };
    if (FAILED(hrIsOffline))
    {
        PrintVolumeKeyValueError(L"    AvailableSpace", hrIsOffline);
    }
    else if (isOffline)
    {
        wprintf(L"    %-26ls : --Offline--\n", L"AvailableSpace");
    }
    else if (FAILED(hrVolume2))
    {
        PrintVolumeKeyValueError(L"    AvailableSpace", hrVolume2);
    }
    else
    {
        std::uint64_t availableSpace{};
        wil::com_ptr_nothrow<ABI::Windows::Foundation::__FIAsyncOperation_1_UINT64_t> operation;
        hr = LOG_IF_FAILED(volume2->GetAvailableSpaceAsync(operation.put()));
        if (SUCCEEDED(hr))
        {
            hr = LOG_IF_FAILED(wil::wait_for_completion_nothrow(operation.get(), &availableSpace));
        }
        if (FAILED(hr))
        {
            PrintVolumeKeyValueError(L"    AvailableSpace", hr);
        }
        else
        {
            PCWSTR units{};
            const std::uint64_t kb{ 1024 };
            const std::uint64_t mb{ kb * 1024 };
            const std::uint64_t gb{ mb * 1024 };
            const std::uint64_t tb{ gb * 1024 };
            if (availableSpace > tb)
            {
                availableSpace /= tb;
                units = L"TB";
            }
            else if (availableSpace > gb)
            {
                availableSpace /= gb;
                units = L"GB";
            }
            else if (availableSpace > mb)
            {
                availableSpace /= mb;
                units = L"MB";
            }
            else if (availableSpace > kb)
            {
                availableSpace /= kb;
                units = L"KB";
            }
            else
            {
                units = L"bytes";
            }
            wprintf(L"    %-26ls : %llu %ls\n", L"AvailableSpace", availableSpace, units);
        }
    }
    if (FAILED(hrIsOffline))
    {
        PrintVolumeKeyValueError(L"    DriveType", hrIsOffline);
        PrintVolumeKeyValueError(L"    FileSystem", hrIsOffline);
    }
    else if (isOffline)
    {
        wprintf(L"    %-26ls : --Offline--\n", L"DriveType");
        wprintf(L"    %-26ls : --Offline--\n", L"FileSystem");
    }
    else
    {
        PCWSTR path{ WindowsGetStringRawBuffer(string.get(), nullptr) };
        WCHAR volumeName[]{ path[0], L':', L'\\', L'\0' };
        wprintf(L"    %-26ls : %ls\n", L"DriveType", ToDriveTypeString(::GetDriveTypeW(volumeName)));
        WCHAR fileSystemName[MAX_PATH + 1]{};
        DWORD fileSystemNameSize{ ARRAYSIZE(fileSystemName) };
        hr = LOG_IF_WIN32_BOOL_FALSE_MSG(::GetVolumeInformation(volumeName, nullptr, 0, nullptr, nullptr, nullptr, fileSystemName, fileSystemNameSize), "%ls", volumeName);
        PrintVolumeValue(L"    FileSystem", hr, fileSystemName);
    }
    hr = LOG_IF_FAILED(volume->get_MountPoint(wil::out_param(string)));
    PrintVolumeValue(L"    MountPoint", hr, string);
    hr = LOG_IF_FAILED(volume->get_Name(wil::out_param(string)));
    PrintVolumeValue(L"    Name", hr, string);
    PrintVolumeValue(L"    State", hrIsOffline, isOffline ? L"Offline" : L"Online");
    boolean boolean{};
    hr = LOG_IF_FAILED(volume2->get_IsFullTrustPackageSupported(&boolean));
    PrintVolumeValue(L"    SupportsFullTrustPackages", hr, boolean);
    hr = LOG_IF_FAILED(volume->get_SupportsHardLinks(&boolean));
    PrintVolumeValue(L"    SupportsHardLinks", hr, boolean);
    hr = LOG_IF_FAILED(volume->get_IsSystemVolume(&boolean));
    PrintVolumeValue(L"    SystemVolume", hr, boolean);
}

HRESULT Command_Volume_Add(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Add };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR path{ argv[3] };
    if ((CompareStringOrdinal(path, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(path, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(path, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    HSTRING_HEADER pathHeader{};
    HSTRING pathHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(path, pathHeader, pathHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<__FIAsyncOperation_1_Windows__CManagement__CDeployment__CPackageVolume> operation;
    RETURN_IF_FAILED(packageManager3->AddPackageVolumeAsync(pathHString, operation.put()));
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
    RETURN_IF_FAILED(wil::wait_for_completion_nothrow(operation.get(), wil::out_param(volume)));

    PCWSTR packageStorePathAlreadyExistsOnDriveButPathDiffers{};
    wil::unique_hstring packageStorePathHString;
    HRESULT hr{ LOG_IF_FAILED(volume->get_PackageStorePath(wil::out_param(packageStorePathHString))) };
    if (SUCCEEDED(hr))
    {
        packageStorePathAlreadyExistsOnDriveButPathDiffers = WindowsGetStringRawBuffer(packageStorePathHString.get(), nullptr);
        if (CompareStringOrdinal(packageStorePathAlreadyExistsOnDriveButPathDiffers, -1, path, -1, TRUE) == CSTR_EQUAL)
        {
            packageStorePathAlreadyExistsOnDriveButPathDiffers = nullptr;
        }
    }
    if (packageStorePathAlreadyExistsOnDriveButPathDiffers)
    {
        wprintf(L"PackageVolume already exists at '%ls'\n", packageStorePathAlreadyExistsOnDriveButPathDiffers);
    }
    else
    {
        wprintf(L"PackageVolume '%ls' is added\n", path);
    }
    PrintVolume(volume);
    return packageStorePathAlreadyExistsOnDriveButPathDiffers ? S_OK : S_FALSE;
}

HRESULT Command_Volume_List(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_List };

    enum class PackageDisplayFormat { Full = 0, PackageFullName = 1, PackageFamilyName = 2 };

    bool logo{ true };
    bool summary{ true };

    int argn{ 3 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--no-summary", -1, FALSE) == CSTR_EQUAL)
        {
            summary = false;
        }
        else if (CompareStringOrdinal(arg, -1, L"--", -1, FALSE) == CSTR_EQUAL)
        {
            ++argn;
            break;
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
        }
        else if ((CompareStringOrdinal(arg, -1, L"-nologo", -1, FALSE) == CSTR_EQUAL) ||
                 (CompareStringOrdinal(arg, -1, L"--no-logo", -1, FALSE) == CSTR_EQUAL))
        {
            logo = false;
        }
        else if (arg[0] != L'-')
        {
            break;
        }
        else
        {
            UnknownArgument(argv[argn]);
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterable<ABI::Windows::Management::Deployment::PackageVolume*>> volumes;
    RETURN_IF_FAILED(packageManager3->FindPackageVolumes(&volumes));
    if (volumes)
    {
        wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterator<ABI::Windows::Management::Deployment::PackageVolume*>> volumesIterator;
        RETURN_IF_FAILED(volumes->First(&volumesIterator));
        if (volumesIterator)
        {
            boolean hasCurrent{};
            RETURN_IF_FAILED(volumesIterator->get_HasCurrent(&hasCurrent));
            while (hasCurrent)
            {
                wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
                RETURN_IF_FAILED(volumesIterator->get_Current(&volume));
                wprintf(L"#%u\n", countDisplayed);
                PrintVolume(volume);
                if (FAILED_LOG(volumesIterator->MoveNext(&hasCurrent)))
                {
                    break;
                }
            }
        }
    }

    if (summary)
    {
        wprintf(L"%u volumes%ls\n", countDisplayed, countDisplayed == 1 ? L"" : L"s");
    }

    return S_OK;
}

HRESULT Command_Volume_Default_Get(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Default_Get };

    if (argc < 4)
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> packageVolume;
    RETURN_IF_FAILED(packageManager3->GetDefaultPackageVolume(wil::out_param(packageVolume)));
    PrintVolume(packageVolume);
    return S_OK;
}
HRESULT Command_Volume_Default_Set(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Default_Set };

    if (argc < 5)
    {
        Help(help_string);
    }

    PCWSTR volumeNameOrPath{ argv[4] };
    if ((CompareStringOrdinal(volumeNameOrPath, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
    }

    bool logo{ true };

    int argn{ 5 };
    for (; argn < argc; ++argn)
    {
        PCWSTR arg{ argv[argn] };
        if ((CompareStringOrdinal(arg, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
            (CompareStringOrdinal(arg, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
        {
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    HSTRING_HEADER volumeNameOrPathHeader{};
    HSTRING volumeNameOrPathHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(volumeNameOrPath, volumeNameOrPathHeader, volumeNameOrPathHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
    wil::unique_hstring volumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(volumeNameOrPath, volumeNameOrPath, packageManager3.get(), volume, volumePathHString));
    PrintVolume(volume);
    RETURN_IF_FAILED(packageManager3->SetDefaultPackageVolume(volume.get()));
    PCWSTR packageStorePath{ WindowsGetStringRawBuffer(volumePathHString.get(), nullptr) };
    wprintf(L"PackageVolume '%ls' is the default\n", packageStorePath);
    return S_OK;
}

HRESULT Command_Volume_Default(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Default };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[3] };
    if (CompareStringOrdinal(command, -1, L"get", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Default_Get(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"set", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Default_Set(argc, argv));
    }
    else
    {
        Help(help_string);
    }
    return S_OK;
}

HRESULT SetPackageVolumeOfflineOrOnline(PCWSTR volumeNameOrPath, bool online)
{
    HSTRING_HEADER volumeNameOrPathHeader{};
    HSTRING volumeNameOrPathHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(volumeNameOrPath, volumeNameOrPathHeader, volumeNameOrPathHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
    wil::unique_hstring volumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(volumeNameOrPath, volumeNameOrPath, packageManager3.get(), volume, volumePathHString));
    PrintVolume(volume);
    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    if (online)
    {
        RETURN_IF_FAILED(packageManager3->SetPackageVolumeOnlineAsync(volume.get(), deploymentOperation.put()));
    }
    else
    {
        RETURN_IF_FAILED(packageManager3->SetPackageVolumeOfflineAsync(volume.get(), deploymentOperation.put()));
    }
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    PCWSTR packageStorePath{ WindowsGetStringRawBuffer(volumePathHString.get(), nullptr) };
    wprintf(L"PackageVolume '%ls' is %ls\n", packageStorePath, online ? L"online" : L"offline");
    return S_OK;
}

HRESULT SetPackageVolumeOffline(PCWSTR volumeNameOrPath)
{
    RETURN_IF_FAILED(SetPackageVolumeOfflineOrOnline(volumeNameOrPath, false));
    return S_OK;
}

HRESULT SetPackageVolumeOnline(PCWSTR volumeNameOrPath)
{
    RETURN_IF_FAILED(SetPackageVolumeOfflineOrOnline(volumeNameOrPath, true));
    return S_OK;
}

HRESULT Command_Volume_Offline(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Offline };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR volumeNameOrPath{ argv[3] };
    if ((CompareStringOrdinal(volumeNameOrPath, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    RETURN_IF_FAILED(SetPackageVolumeOffline(volumeNameOrPath));
    return S_OK;
}

HRESULT Command_Volume_Online(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Online };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR volumeNameOrPath{ argv[3] };
    if ((CompareStringOrdinal(volumeNameOrPath, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    RETURN_IF_FAILED(SetPackageVolumeOnline(volumeNameOrPath));
    return S_OK;
}

HRESULT Command_Volume_Remove(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume_Remove };

    if (argc < 4)
    {
        Help(help_string);
    }

    PCWSTR volumeNameOrPath{ argv[3] };
    if ((CompareStringOrdinal(volumeNameOrPath, -1, L"-?", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"-h", -1, FALSE) == CSTR_EQUAL) ||
        (CompareStringOrdinal(volumeNameOrPath, -1, L"--help", -1, FALSE) == CSTR_EQUAL))
    {
        Help(help_string);
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
            Help(help_string);
        }
        else if (CompareStringOrdinal(arg, -1, L"--benchmark", -1, FALSE) == CSTR_EQUAL)
        {
            g_benchmark = true;
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

    HSTRING_HEADER volumeNameOrPathHeader{};
    HSTRING volumeNameOrPathHString{};
    RETURN_IF_FAILED(wil::to_hstring_reference(volumeNameOrPath, volumeNameOrPathHeader, volumeNameOrPathHString));

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> packageManager3;
    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        RETURN_IF_FAILED(ActivateInstance(inspectable, RuntimeClass_Windows_Management_Deployment_PackageManager));
        RETURN_IF_FAILED(inspectable.query_to(packageManager3.put()));
    }

    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
    wil::unique_hstring volumePathHString;
    RETURN_IF_FAILED(ToPackageVolume(volumeNameOrPath, volumeNameOrPath, packageManager3.get(), volume, volumePathHString));
    PrintVolume(volume);
    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    RETURN_IF_FAILED(packageManager3->RemovePackageVolumeAsync(volume.get(), deploymentOperation.put()));
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString{};
    HRESULT extendedError{};
    GUID activityId{};
    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    PCWSTR packageStorePath{ WindowsGetStringRawBuffer(volumePathHString.get(), nullptr) };
    wprintf(L"PackageVolume '%ls' is removed\n", packageStorePath);
    return S_OK;
}

HRESULT Command_Volume(int argc, wchar_t* argv[])
{
    constexpr auto help_string{ help_Command_Volume };

    if (argc < 3)
    {
        Help(help_string);
    }

    PCWSTR command{ argv[2] };
    if (CompareStringOrdinal(command, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Add(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"default", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Default(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_List(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"offline", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Offline(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"online", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Online(argc, argv));
    }
    else if (CompareStringOrdinal(command, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Volume_Remove(argc, argv));
    }
    else
    {
        Help(help_string);
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

int Main(int argc, wchar_t* argv[])
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
    else if (CompareStringOrdinal(arg, -1, L"volume", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Volume(argc, argv));
    }
    else
    {
        Help();
    }
}

int wmain(int argc, wchar_t* argv[])
{
    LARGE_INTEGER benchmarkFrequency{};
    const HRESULT hrBenchmarkFrequency{ LOG_IF_WIN32_BOOL_FALSE(::QueryPerformanceFrequency(&benchmarkFrequency)) };
    LARGE_INTEGER benchmarkStart{};
    const HRESULT hrBenchmarkStart{ LOG_IF_WIN32_BOOL_FALSE(::QueryPerformanceCounter(&benchmarkStart)) };

    const auto exitCode{ Main(argc, argv) };

    LARGE_INTEGER benchmarkStop{};
    const HRESULT hrBenchmarkStop{ LOG_IF_WIN32_BOOL_FALSE(::QueryPerformanceCounter(&benchmarkStop)) };

    if (g_benchmark)
    {
        if (FAILED_LOG(hrBenchmarkFrequency))
        {
            wprintf(L"BENCHMARK: ***ERROR 0x%08X\n", hrBenchmarkFrequency);
        }
        else if (FAILED_LOG(hrBenchmarkStart))
        {
            wprintf(L"BENCHMARK: ***ERROR 0x%08X\n", hrBenchmarkStart);
        }
        else if (FAILED_LOG(hrBenchmarkStop))
        {
            wprintf(L"BENCHMARK: ***ERROR 0x%08X\n", hrBenchmarkStop);
        }
        else
        {
            const auto elapsedTicks{ benchmarkStop.QuadPart - benchmarkStart.QuadPart };
            const auto remainingTicks{ elapsedTicks % benchmarkFrequency.QuadPart };
            const auto microseconds{ remainingTicks * 1'000'000 / benchmarkFrequency.QuadPart };
            const auto seconds{ elapsedTicks / benchmarkFrequency.QuadPart };
            const auto minutes{ seconds / 60 };
            const auto hours{ minutes / 24 };
            wprintf(L"\nElapsed: %lld:%lld:%lld.%06lld seconds\n", hours, minutes % 60, seconds % 60, microseconds);
        }
    }

    return exitCode;
}
