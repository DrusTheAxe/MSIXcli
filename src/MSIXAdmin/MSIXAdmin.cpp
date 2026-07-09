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
            L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
            L"  -?, -h, --help      Show command line help\n"
            L"\n"
            L"Commands:\n"
            L"  add <FILE>     Add the certificate from the signed package file\n"
            L"  exists <FILE>  Check if the certificate from the signed package file exists\n"
            L"  list <FILE>    List the certificate from the signed package file\n"
            L"  remove <FILE>  Remove the certificate per the signed package file\n");
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
            L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
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
            L"  --defer-registration  Defer automatic registration\n"
            L"  -nologo, --no-logo    Do not display startup banner or copyright message\n"
            L"  -?, -h, --help        Show command line help\n");
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
            L"  -nologo, --no-logo  Do not display startup banner or copyright message\n"
            L"  -?, -h, --help      Show command line help\n");
    ::ExitProcess(1);
}

HRESULT Command_Certificate_Add(PCWSTR filename)
{
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
    RETURN_IF_FAILED_MSG(MSIX::Signing::AddCertificate(packageReader.get()), "%ls", filename);

    wprintf(L"Certificate from '%ls' added to the system\n", filename);
    return S_OK;
}

HRESULT Command_Certificate_Exists(PCWSTR filename)
{
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);
    bool isInstalled{};
    RETURN_IF_FAILED_MSG(MSIX::Signing::IsCertificateInstalled(packageReader.get(), isInstalled), "%ls", filename);

    wprintf(L"Certificate from '%ls' is%ls installed\n", filename, isInstalled ? L"" : L" not");
    return S_OK;
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
    wil::com_ptr_nothrow<IAppxPackageReader> packageReader;
    RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, packageReader), "%ls", filename);

    //TODO certificate remove
    (void)filename;
    return S_OK;
}

HRESULT Command_Certificate(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        Command_Certificate_Help();
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

    if (CompareStringOrdinal(action, -1, L"add", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Certificate_Add(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"exists", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Certificate_Exists(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"list", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Certificate_List(filename));
    }
    else if (CompareStringOrdinal(action, -1, L"remove", -1, FALSE) == CSTR_EQUAL)
    {
        RETURN_IF_FAILED(Command_Certificate_Remove(filename));
    }
    else
    {
        FAIL_FAST_HR(E_UNEXPECTED);
    }
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

    //TODO deprovision
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

    bool deferRegistration{ true };
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
    else if (CompareStringOrdinal(arg, -1, L"deprovision", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Deprovision(argc, argv));
    }
    else if (CompareStringOrdinal(arg, -1, L"provision", -1, FALSE) == CSTR_EQUAL)
    {
        return MessageOnError(Command_Provision(argc, argv));
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
