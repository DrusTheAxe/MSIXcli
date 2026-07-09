// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <bcrypt.h>
#include <appmodel.h>
#include <strsafe.h>

namespace MSIX
{
// Origin of an MSIX/AppX package's digital signature, determined by inspecting
// the certificate chain stored in the package's AppxSignature.p7x footprint and
// classifying the trust anchor (root) it chains to.
//
// This mirrors the classification performed by the Microsoft MSIX SDK
// (microsoft/msix-packaging, src/inc/internal/AppxSignature.hpp) using the
// "root public key hash" approach: a SHA-256 hash computed over the root
// certificate's public key and compared against the known Microsoft application
// root key hash(es).
enum class SignatureOrigin
{
    Windows = 0,        // Chains to the Windows RCA
    Store = 1,          // Chains to the Microsoft Store RCA
    LineOfBusiness = 2, // Chains to either Verisign, or Authenticode
    Unknown = 3,        // Signed, but unknown origin (e.g. self signed)
    Unsigned = 4,       // No signature (or no valid one)
};

constexpr PCWSTR ToString(SignatureOrigin signatureOrigin)
{
    switch (signatureOrigin)
    {
        case SignatureOrigin::Windows:          return L"Windows";
        case SignatureOrigin::Store:            return L"Store";
        case SignatureOrigin::LineOfBusiness:   return L"Line of Business";
        case SignatureOrigin::Unknown:          return L"Signed but unknown (e.g. self signed)";
        case SignatureOrigin::Unsigned:         return L"Unsigned (No signature or no valid one)";
        default:                                return L"???";
    }
}

class Signing
{
public:
    // Add the package's certificate to the system's certificate store, equivalent to the Powershell script:
    //
    //      $cer = Join-Path $root 'something.cer'
    //      $cert_path = "cert:\LocalMachine\TrustedPeople"
    //      $x509certificates = Import-Certificate -FilePath $cer -CertStoreLocation $cert_path
    //
    // or the manual steps in File Explorer:
    //
    //      1. Right-click on the .msix/.msixbundle/.appx/... package you downloaded via App Center and select **properties**.
    //      2. Under the **Digital Signatures** tab you should see the test certificate. Click to select the certificate and click on **Details** button.
    //      3. Select the button **View Certificate**.
    //      4. Select the button **Install Certificate**.
    //      5. From the **Store Location** radio buttons select **Local Machine**. Click the **Next** button.
    //      6. Click **Yes** on the admin prompt for changes to your device.
    //      7. On the **Certificate Import Wizard** chose the radio button **Place all certificates in the following store** then select the **Browse** button.
    //      8. If the cert is...
    //         1. ...**a leaf cert issued by an already-trusted CA** (e.g. public one) -> You don�t need to do anything. Exit out of adding the cert
    //         2. ...**an untrusted leaf cert** (e.g. self-signed) -> Select the **Trusted People** certificate store (common case)
    //         3. ...**a new CA** -> Select the **Trusted Root Certification Authorities** certificate store (extremely rare)
    //      9. Click the **OK** button.
    //      10. Click the **Next** button on the **Certificate Import Wizard** window.
    //      11. Click **Finish** button to complete the certificate install.
    //      12. Close all windows.
    static HRESULT AddCertificate(IAppxPackageReader* packageReader)
    {
        // Writing to the LocalMachine store requires administrator rights
        // Fail if we're not elevated
        RETURN_HR_IF(E_ACCESSDENIED, !IsRunningAsAdmin());
        RETURN_HR_IF_NULL(E_POINTER, packageReader);

        // Skip packages signed by Windows or the Store
        // Those roots are already trusted, so there is nothing to add
        SignatureOrigin origin{ SignatureOrigin::Unknown };
        RETURN_IF_FAILED(DetectSignatureOrigin(packageReader, origin));
        if ((origin == SignatureOrigin::Windows) || (origin == SignatureOrigin::Store))
        {
            return S_FALSE; // Already trusted -> nothing to install
        }

        // Extract the leaf (signing) certificate from the package signature
        wil::unique_cert_context signingCertificate;
        const HRESULT signerHr{ GetPackageSignerCertificate(packageReader, signingCertificate) };
        RETURN_IF_FAILED(signerHr);
        if (signerHr == S_FALSE)
        {
            return S_FALSE; // No usable certificate -> nothing to install
        }

        // Install the leaf into LocalMachine\TrustedPeople
        RETURN_IF_FAILED(InstallLeafToTrustedPeople(signingCertificate.get()));
        return S_OK;
    }

    // Removes the package's leaf certificate from LocalMachine\TrustedPeople if present.
    // Do nothing for Windows/Store-signed packages.
    // @return S_OK if a certificate was removed, S_FALSE if there was nothing to do
    static HRESULT RemoveCertificate(IAppxPackageReader* packageReader)
    {
        RETURN_HR_IF(E_ACCESSDENIED, !IsRunningAsAdmin());
        RETURN_HR_IF_NULL(E_POINTER, packageReader);

        // Read the AppxSignature.p7x footprint straight from the package container
        wil::unique_cotaskmem_ptr<BYTE[]> signatureBlob;
        DWORD signatureSize{};
        const HRESULT readHr{ ReadSignatureBlob(packageReader, signatureBlob, signatureSize) };
        RETURN_IF_FAILED(readHr);
        if (readHr == S_FALSE)
        {
            return S_FALSE; // No signature -> nothing to remove
        }

        return RemoveCertificateCore(signatureBlob.get(), signatureSize);
    }

    // Removes the certificate of an installed package (identified by its
    // PackageFullName) from LocalMachine\TrustedPeople if present. The signer is
    // read from the installed package's AppxSignature.p7x on disk. Does nothing
    // for Windows/Store-signed packages.
    // @return S_OK if a certificate was removed, S_FALSE if there was nothing to do,
    //         or a failing HRESULT (e.g. the package is not installed)
    static HRESULT RemoveCertificate(PCWSTR packageFullName)
    {
        RETURN_HR_IF(E_ACCESSDENIED, !IsRunningAsAdmin());
        RETURN_HR_IF_NULL(E_POINTER, packageFullName);

        // Resolve the installed package's on-disk location
        UINT32 pathChars{};
        const LONG sizeResult{ GetPackagePathByFullName(packageFullName, &pathChars, nullptr) };
        if (sizeResult == ERROR_NOT_FOUND)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND); // Not an installed package
        }
        RETURN_HR_IF(HRESULT_FROM_WIN32(static_cast<DWORD>(sizeResult)), sizeResult != ERROR_INSUFFICIENT_BUFFER);

        // Build "<installPath>\AppxSignature.p7x". pathChars already accounts for the
        // path's null terminator, which offsets the leaf's leading backslash.
        static constexpr PCWSTR c_signatureLeaf{ L"\\AppxSignature.p7x" };
        const size_t signatureChars{ pathChars + wcslen(c_signatureLeaf) };
        auto signaturePath{ wil::make_unique_cotaskmem_nothrow<WCHAR[]>(signatureChars) };
        RETURN_IF_NULL_ALLOC(signaturePath);
        RETURN_IF_WIN32_ERROR(GetPackagePathByFullName(packageFullName, &pathChars, signaturePath.get()));
        RETURN_IF_FAILED(StringCchCatW(signaturePath.get(), signatureChars, c_signatureLeaf));

        // %ProgramFiles%\WindowsApps is ACL-restricted to TrustedInstaller; read the
        // signature via backup semantics so an elevated caller can still open it
        wil::unique_cotaskmem_ptr<BYTE[]> signatureBlob;
        DWORD signatureSize{};
        const HRESULT readHr{ ReadFileWithBackupPrivilege(signaturePath.get(), signatureBlob, signatureSize) };
        RETURN_IF_FAILED(readHr);
        if (readHr == S_FALSE)
        {
            return S_FALSE; // No usable signature file -> nothing to remove
        }

        return RemoveCertificateCore(signatureBlob.get(), signatureSize);
    }

    // Removes the leaf certificate carried in the given AppxSignature.p7x blob from
    // LocalMachine\TrustedPeople, unless the package is Windows/Store-signed.
    // @return S_OK if a certificate was removed, S_FALSE if there was nothing to do
    static HRESULT RemoveCertificateCore(const BYTE* signatureBlob, DWORD signatureSize)
    {
        // Windows/Store roots are managed by the system; never touch them
        SignatureOrigin origin{ SignatureOrigin::Unknown };
        RETURN_IF_FAILED(DetectSignatureOriginFromBlob(signatureBlob, signatureSize, origin));
        if ((origin == SignatureOrigin::Windows) || (origin == SignatureOrigin::Store))
        {
            return S_FALSE; // Nothing to remove
        }

        wil::unique_cert_context signingCertificate;
        const HRESULT signerHr{ GetSignerCertificateFromBlob(signatureBlob, signatureSize, signingCertificate) };
        RETURN_IF_FAILED(signerHr);
        if (signerHr == S_FALSE)
        {
            return S_FALSE; // No certificate -> nothing to remove
        }

        wil::unique_hcertstore store{ OpenTrustedPeopleStore() };
        RETURN_LAST_ERROR_IF_NULL(store);

        // CertFindCertificateInStore returns a context owned by the store; deleting
        // it consumes the reference, so it must not be double-freed
        PCCERT_CONTEXT found{ CertFindCertificateInStore(store.get(), c_encoding, 0,
            CERT_FIND_EXISTING, signingCertificate.get(), nullptr) };
        if (!found)
        {
            return S_FALSE; // Not installed -> nothing to remove
        }
        RETURN_IF_WIN32_BOOL_FALSE(CertDeleteCertificateFromStore(found));
        return S_OK;
    }

    // Reports whether the certificate is trusted, based on the certificate's thumbprint.
    // @return true for Windows/Store-signed certificates' thumbprints,
    //         otherwise true only if the leaf is present in LocalMachine\TrustedPeople.
    static HRESULT IsCertificateInstalled(size_t thumbprintSize, BYTE* thumbprint, bool& isInstalled)
    {
        //TODO
    }

    // Reports whether the package's certificate is trusted.
    // @return true for Windows/Store-signed packages,
    //         otherwise true only if the leaf is present in LocalMachine\TrustedPeople.
    static HRESULT IsCertificateInstalled(IAppxPackageReader* packageReader, bool& isInstalled)
    {
        isInstalled = false;
        RETURN_HR_IF_NULL(E_POINTER, packageReader);

        SignatureOrigin origin{ SignatureOrigin::Unknown };
        RETURN_IF_FAILED(DetectSignatureOrigin(packageReader, origin));
        if ((origin == SignatureOrigin::Windows) || (origin == SignatureOrigin::Store))
        {
            isInstalled = true; // Already trusted by the system
            return S_OK;
        }

        wil::unique_cert_context signingCertificate;
        const HRESULT signerHr{ GetPackageSignerCertificate(packageReader, signingCertificate) };
        RETURN_IF_FAILED(signerHr);
        if (signerHr == S_FALSE)
        {
            return S_OK; // No certificate -> not installed
        }

        wil::unique_hcertstore store{ OpenTrustedPeopleStore() };
        RETURN_LAST_ERROR_IF_NULL(store);
        wil::unique_cert_context found{ CertFindCertificateInStore(store.get(), c_encoding, 0,
            CERT_FIND_EXISTING, signingCertificate.get(), nullptr) };
        isInstalled = static_cast<bool>(found);
        return S_OK;
    }

    // Extracts the leaf (signing) certificate from a package's signature so
    // callers can inspect or display it. Read-only; requires no elevation.
    // @return S_OK with the certificate, S_FALSE if the package has no usable
    //         signature, or a failing HRESULT on a hard error
    static HRESULT GetSigningCertificate(IAppxPackageReader* packageReader, wil::unique_cert_context& signingCertificate)
    {
        signingCertificate.reset();
        RETURN_HR_IF_NULL(E_POINTER, packageReader);
        return GetPackageSignerCertificate(packageReader, signingCertificate);
    }

public:
    // Determines the signature origin of an opened package
    static HRESULT DetectSignatureOrigin(IAppxPackageReader* packageReader, SignatureOrigin& signatureOrigin)
    {
        signatureOrigin = SignatureOrigin::Unsigned;
        RETURN_HR_IF_NULL(E_POINTER, packageReader);

        // Read the raw AppxSignature.p7x footprint
        wil::unique_cotaskmem_ptr<BYTE[]> signatureBlob;
        DWORD signatureSize{};
        const HRESULT readHr{ ReadSignatureBlob(packageReader, signatureBlob, signatureSize) };
        RETURN_IF_FAILED(readHr);
        if (readHr == S_FALSE)
        {
            return S_OK; // No (readable) signature -> Unsigned
        }

        return DetectSignatureOriginFromBlob(signatureBlob.get(), signatureSize, signatureOrigin);
    }

private:
    // Classifies the signature origin from a raw AppxSignature.p7x blob (the same
    // format as the package footprint or the installed AppxSignature.p7x file)
    static HRESULT DetectSignatureOriginFromBlob(const BYTE* signatureBlob, DWORD signatureSize, SignatureOrigin& signatureOrigin)
    {
        signatureOrigin = SignatureOrigin::Unsigned;

        // Parse the PKCS#7 signed message and obtain its embedded certificates
        wil::unique_hcertstore certificateStore;
        wil::unique_hcryptmsg signedMessage;
        const HRESULT openHr{ OpenSignedMessage(signatureBlob, signatureSize, certificateStore, signedMessage) };
        RETURN_IF_FAILED(openHr);
        if (openHr == S_FALSE)
        {
            return S_OK; // Not a valid signed blob -> Unsigned
        }

        // Locate the end-entity (signing) certificate
        wil::unique_cert_context signingCertificate;
        const HRESULT signerHr{ GetSignerCertificate(signedMessage.get(), certificateStore.get(), signingCertificate) };
        RETURN_IF_FAILED(signerHr);
        if (signerHr == S_FALSE)
        {
            // The blob parsed as signed but no usable signer certificate was found
            signatureOrigin = SignatureOrigin::Unknown;
            return S_OK;
        }

        // From here the package is signed. Default to Unknown until classified
        signatureOrigin = SignatureOrigin::Unknown;

        // Build the certificate chain using only the certificates carried in the
        // package (no network retrieval) so the trust anchor can be inspected
        wil::unique_cert_chain_context certificateChain;
        const HRESULT chainHr{ BuildCertificateChain(signingCertificate.get(), certificateStore.get(), certificateChain) };
        RETURN_IF_FAILED(chainHr);
        if (chainHr == S_FALSE)
        {
            return S_OK; // Unknown
        }

        signatureOrigin = ClassifyChain(certificateChain.get(), signingCertificate.get());
        return S_OK;
    }

public:

private:
    // SHA-256 digest length, in bytes
    static constexpr DWORD c_hashBytes{ 32 };

    // Header magic that prefixes the PKCS#7 data inside an AppxSignature.p7x
    // file: the ASCII characters 'P','K','C','X' stored as a little-endian DWORD
    // (microsoft/msix-packaging: P7X_FILE_ID = 0x58434b50).
    static constexpr DWORD c_p7xFileId{ 0x58434b50 };

    static constexpr DWORD c_encoding{ X509_ASN_ENCODING | PKCS_7_ASN_ENCODING };

    // Object identifier placed in the signing certificate's Enhanced Key Usage
    // when a package is signed for the Microsoft Store
    static constexpr char c_windowsStoreEku[]{ "1.3.6.1.4.1.311.76.3.1" };

    // The system store that holds trusted (e.g. self-signed) leaf certificates;
    // matches the script's cert:\LocalMachine\TrustedPeople.
    static constexpr PCWSTR c_trustedPeopleStore{ L"TrustedPeople" };

    // True if the current process token is a member of the local Administrators group (i.e. elevated)
    static bool IsRunningAsAdmin()
    {
        BYTE administratorsSid[SECURITY_MAX_SID_SIZE]{};
        DWORD sidSize{ sizeof(administratorsSid) };
        if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administratorsSid, &sidSize))
        {
            return false;
        }
        BOOL isMember{};
        if (!CheckTokenMembership(nullptr, administratorsSid, &isMember))
        {
            return false;
        }
        return isMember != FALSE;
    }

    // Extracts the leaf (signing) certificate from a package's signature
    // @return S_OK with the certificate, S_FALSE if the package has no usable signature, or a failing HRESULT on a hard error
    static HRESULT GetPackageSignerCertificate(IAppxPackageReader* packageReader, wil::unique_cert_context& signingCertificate)
    {
        signingCertificate.reset();

        wil::unique_cotaskmem_ptr<BYTE[]> signatureBlob;
        DWORD signatureSize{};
        const HRESULT readHr{ ReadSignatureBlob(packageReader, signatureBlob, signatureSize) };
        RETURN_IF_FAILED(readHr);
        if (readHr == S_FALSE)
        {
            return S_FALSE; // No (readable) signature
        }

        return GetSignerCertificateFromBlob(signatureBlob.get(), signatureSize, signingCertificate);
    }

    // Extracts the leaf (signing) certificate from a raw AppxSignature.p7x blob
    // @return S_OK with the certificate, S_FALSE if the blob has no usable signature, or a failing HRESULT on a hard error
    static HRESULT GetSignerCertificateFromBlob(const BYTE* signatureBlob, DWORD signatureSize, wil::unique_cert_context& signingCertificate)
    {
        signingCertificate.reset();

        wil::unique_hcertstore certificateStore;
        wil::unique_hcryptmsg signedMessage;
        const HRESULT openHr{ OpenSignedMessage(signatureBlob, signatureSize, certificateStore, signedMessage) };
        RETURN_IF_FAILED(openHr);
        if (openHr == S_FALSE)
        {
            return S_FALSE; // Not a valid signed blob
        }

        return GetSignerCertificate(signedMessage.get(), certificateStore.get(), signingCertificate);
    }

    // Enables SE_BACKUP_NAME on the current process token so an elevated caller can
    // read files under ACL-restricted locations (e.g. %ProgramFiles%\WindowsApps,
    // owned by TrustedInstaller). Best-effort: failure is left to the subsequent open.
    static void EnableBackupPrivilege()
    {
        wil::unique_handle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            return;
        }
        LUID luid{};
        if (!LookupPrivilegeValueW(nullptr, SE_BACKUP_NAME, &luid))
        {
            return;
        }
        TOKEN_PRIVILEGES privileges{};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token.get(), FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
    }

    // Reads a file into a heap buffer, using backup semantics so an elevated caller
    // can read past a restrictive DACL (e.g. the installed AppxSignature.p7x).
    //
    // @return S_OK if the file was read, S_FALSE if it is too small/large to be a
    //         signature, or a failing HRESULT on a hard error
    static HRESULT ReadFileWithBackupPrivilege(PCWSTR path, wil::unique_cotaskmem_ptr<BYTE[]>& blob, DWORD& blobSize)
    {
        blob.reset();
        blobSize = 0;

        EnableBackupPrivilege();

        wil::unique_hfile file{ CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
        RETURN_LAST_ERROR_IF(!file);

        LARGE_INTEGER size{};
        RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &size));
        if ((size.QuadPart <= sizeof(DWORD)) || (size.QuadPart > 0xFFFFFFFFull))
        {
            return S_FALSE; // Too small to hold a signature, or implausibly large
        }
        const DWORD total{ static_cast<DWORD>(size.QuadPart) };

        auto buffer{ wil::make_unique_cotaskmem_nothrow<BYTE[]>(total) };
        RETURN_IF_NULL_ALLOC(buffer);

        DWORD offset{};
        while (offset < total)
        {
            DWORD chunkRead{};
            RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get() + offset, total - offset, &chunkRead, nullptr));
            if (chunkRead == 0)
            {
                break; // Short read
            }
            offset += chunkRead;
        }
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_HANDLE_EOF), offset != total);

        blob = wistd::move(buffer);
        blobSize = total;
        return S_OK;
    }

    static wil::unique_hcertstore OpenTrustedPeopleStore()
    {
        return wil::unique_hcertstore{ CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, c_trustedPeopleStore) };
    }

    // Installs a leaf certificate into LocalMachine\TrustedPeople, replacing any existing copy
    // @note Requires the caller is elevated
    static HRESULT InstallLeafToTrustedPeople(PCCERT_CONTEXT certificate)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, certificate);
        wil::unique_hcertstore store{ OpenTrustedPeopleStore() };
        RETURN_LAST_ERROR_IF_NULL(store);
        RETURN_IF_WIN32_BOOL_FALSE(CertAddCertificateContextToStore(store.get(), certificate, CERT_STORE_ADD_REPLACE_EXISTING, nullptr));
        return S_OK;
    }

    // SHA-256 hashes of the public key BIT STRING of known Microsoft application
    // roots. Taken verbatim from the MSIX SDK
    // (microsoft/msix-packaging, src/msix/PAL/Signature/Win32/SignatureValidator.cpp,
    // 'MicrosoftApplicationRootList').
    //
    // Note: only the public key bytes (CERT_PUBLIC_KEY_INFO::PublicKey) are
    // hashed - NOT the full SubjectPublicKeyInfo DER. This list covers the
    // explicit hash fast-path; the CERT_CHAIN_POLICY_MICROSOFT_ROOT policy check
    // is also consulted to remain robust across all Microsoft application roots.
    inline static constexpr BYTE c_microsoftApplicationRootHashes[][c_hashBytes]{
        // CN=Microsoft Root Certificate Authority 2011 (NotBefore 2011-03-22, NotAfter 2036-03-22)
        {
            0x4A, 0xBB, 0x05, 0x94, 0xD3, 0x03, 0xEF, 0x70, 0x77, 0x13,
            0x88, 0x34, 0xAB, 0x31, 0x5E, 0x94, 0x1E, 0x96, 0x30, 0x93,
            0xE0, 0x5B, 0x4B, 0x14, 0xAF, 0x5D, 0xCB, 0x52, 0x77, 0x12,
            0xC0, 0x0A
        },
    };

    // Reads the AppxSignature.p7x footprint into a heap buffer
    //
    // @return S_OK if a signature blob was read, S_FALSE if there is no readable signature (treat as Unsigned),
    //         or a failing HRESULT on a hard error
    static HRESULT ReadSignatureBlob(IAppxPackageReader* packageReader, wil::unique_cotaskmem_ptr<BYTE[]>& blob, DWORD& blobSize)
    {
        blob.reset();
        blobSize = 0;

        wil::com_ptr_nothrow<IAppxFile> signatureFile;
        const HRESULT footprintHr{ LOG_IF_FAILED(packageReader->GetFootprintFile(APPX_FOOTPRINT_FILE_TYPE_SIGNATURE, &signatureFile)) };
        if (FAILED(footprintHr) || !signatureFile)
        {
            return S_FALSE; // No signature footprint -> Unsigned
        }

        wil::com_ptr_nothrow<IStream> stream;
        RETURN_IF_FAILED(signatureFile->GetStream(&stream));

        STATSTG stat{};
        RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
        const ULONGLONG size64{ stat.cbSize.QuadPart };
        if ((size64 <= sizeof(DWORD)) || (size64 > 0xFFFFFFFFull))
        {
            return S_FALSE; // Too small to hold a signature, or implausibly large
        }
        const DWORD size{ static_cast<DWORD>(size64) };

        auto buffer{ wil::make_unique_cotaskmem_nothrow<BYTE[]>(size) };
        RETURN_IF_NULL_ALLOC(buffer);

        const LARGE_INTEGER start{};
        RETURN_IF_FAILED(stream->Seek(start, STREAM_SEEK_SET, nullptr));

        ULONG totalRead{};
        while (totalRead < size)
        {
            ULONG chunkRead{};
            RETURN_IF_FAILED(stream->Read(buffer.get() + totalRead, size - totalRead, &chunkRead));
            if (chunkRead == 0)
            {
                break; // Short read
            }
            totalRead += chunkRead;
        }
        if (totalRead != size)
        {
            return S_FALSE; // Could not read the whole signature
        }

        blob = wistd::move(buffer);
        blobSize = size;
        return S_OK;
    }

    // Strips the P7X magic header and decodes the remaining DER as a PKCS#7
    // signed message, yielding the embedded certificate store and message
    //
    // @return S_OK on success, S_FALSE if the blob is not a valid signed message,
    //         or a failing HRESULT on a hard error
    static HRESULT OpenSignedMessage(const BYTE* blob, DWORD blobSize, wil::unique_hcertstore& certificateStore, wil::unique_hcryptmsg& signedMessage)
    {
        certificateStore.reset();
        signedMessage.reset();

        if (blobSize <= sizeof(DWORD))
        {
            return S_FALSE;
        }
        DWORD magic{};
        memcpy(&magic, blob, sizeof(magic));
        if (magic != c_p7xFileId)
        {
            return S_FALSE;
        }

        CRYPT_DATA_BLOB derBlob{};
        derBlob.pbData = const_cast<BYTE*>(blob + sizeof(DWORD));
        derBlob.cbData = blobSize - sizeof(DWORD);

        DWORD contentType{};
        HCERTSTORE rawStore{};
        HCRYPTMSG rawMessage{};
        if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &derBlob,
                CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED, CERT_QUERY_FORMAT_FLAG_BINARY, 0,
                nullptr, &contentType, nullptr, &rawStore, &rawMessage, nullptr))
        {
            return S_FALSE; // Not a valid PKCS#7 signed blob
        }
        certificateStore.reset(rawStore);
        signedMessage.reset(rawMessage);
        return S_OK;
    }

    // Finds the end-entity (signing) certificate referenced by the message inside the embedded certificate store
    //
    // @return S_OK on success, S_FALSE if no signer certificate is available, or a failing HRESULT on a hard error
    static HRESULT GetSignerCertificate(HCRYPTMSG signedMessage, HCERTSTORE certificateStore, wil::unique_cert_context& signingCertificate)
    {
        signingCertificate.reset();

        DWORD infoSize{};
        if (!CryptMsgGetParam(signedMessage, CMSG_SIGNER_CERT_INFO_PARAM, 0, nullptr, &infoSize) || (infoSize == 0))
        {
            return S_FALSE;
        }

        auto infoBuffer{ wil::make_unique_cotaskmem_nothrow<BYTE[]>(infoSize) };
        RETURN_IF_NULL_ALLOC(infoBuffer);
        if (!CryptMsgGetParam(signedMessage, CMSG_SIGNER_CERT_INFO_PARAM, 0, infoBuffer.get(), &infoSize))
        {
            return S_FALSE;
        }

        const auto certInfo{ reinterpret_cast<PCERT_INFO>(infoBuffer.get()) };
        PCCERT_CONTEXT rawCertificate{ CertGetSubjectCertificateFromStore(certificateStore, c_encoding, certInfo) };
        if (!rawCertificate)
        {
            return S_FALSE;
        }
        signingCertificate.reset(rawCertificate);
        return S_OK;
    }

    // Builds a certificate chain for the signing certificate, restricted to the
    // certificates carried in the package (no network retrieval)
    //
    // @return S_OK on success, S_FALSE if a chain could not be built, or a failing HRESULT on a hard error
    static HRESULT BuildCertificateChain(PCCERT_CONTEXT signingCertificate, HCERTSTORE additionalStore, wil::unique_cert_chain_context& certificateChain)
    {
        certificateChain.reset();

        CERT_CHAIN_PARA chainParameters{};
        chainParameters.cbSize = sizeof(chainParameters);

        PCCERT_CHAIN_CONTEXT rawChain{};
        if (!CertGetCertificateChain(nullptr, signingCertificate, nullptr, additionalStore, &chainParameters,
                CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL, nullptr, &rawChain))
        {
            return S_FALSE;
        }
        certificateChain.reset(rawChain);
        return S_OK;
    }

    // Returns the trust anchor (root) certificate of the primary simple chain, or nullptr if the chain is empty
    static PCCERT_CONTEXT GetRootCertificate(PCCERT_CHAIN_CONTEXT certificateChain)
    {
        if (!certificateChain || (certificateChain->cChain == 0))
        {
            return nullptr;
        }
        const PCERT_SIMPLE_CHAIN simpleChain{ certificateChain->rgpChain[0] };
        if (!simpleChain || (simpleChain->cElement == 0))
        {
            return nullptr;
        }
        return simpleChain->rgpElement[simpleChain->cElement - 1]->pCertContext;
    }

    // Computes the SHA-256 hash of a certificate's public key BIT STRING (only
    // the key bytes, not the full SubjectPublicKeyInfo), matching the MSIX SDK
    static HRESULT HashPublicKey(PCCERT_CONTEXT certificate, BYTE (&hash)[c_hashBytes])
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, certificate);

        const CRYPT_BIT_BLOB& publicKey{ certificate->pCertInfo->SubjectPublicKeyInfo.PublicKey };
        DWORD hashSize{ c_hashBytes };
        RETURN_IF_WIN32_BOOL_FALSE(CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr,
            publicKey.pbData, publicKey.cbData, hash, &hashSize));
        RETURN_HR_IF(E_UNEXPECTED, hashSize != c_hashBytes);
        return S_OK;
    }

    // True if 'hash' matches one of the known Microsoft application root key hashes
    static bool IsKnownMicrosoftRootHash(const BYTE (&hash)[c_hashBytes])
    {
        for (const auto& knownHash : c_microsoftApplicationRootHashes)
        {
            if (memcmp(hash, knownHash, c_hashBytes) == 0)
            {
                return true;
            }
        }
        return false;
    }

    // True if the chain's root is a Microsoft application root, determined first
    // by the explicit root public key hash and then by the Windows Microsoft
    // root chain policy (which covers every Microsoft application root the OS recognizes).
    static bool ChainsToMicrosoftRoot(PCCERT_CHAIN_CONTEXT certificateChain)
    {
        PCCERT_CONTEXT root{ GetRootCertificate(certificateChain) };
        BYTE rootHash[c_hashBytes]{};
        if (root && SUCCEEDED(HashPublicKey(root, rootHash)) && IsKnownMicrosoftRootHash(rootHash))
        {
            return true;
        }

        // The Microsoft application root flag is supplied via the policy
        // parameter's dwFlags; pvExtraPolicyPara must be NULL for this policy
        return ChainSatisfiesPolicy(certificateChain, CERT_CHAIN_POLICY_MICROSOFT_ROOT, MICROSOFT_ROOT_CERT_CHAIN_POLICY_CHECK_APPLICATION_ROOT_FLAG);
    }

    // True if the chain satisfies the given chain policy with no error
    static bool ChainSatisfiesPolicy(PCCERT_CHAIN_CONTEXT certificateChain, PCSTR policyOid, DWORD policyFlags)
    {
        CERT_CHAIN_POLICY_PARA policyParameters{};
        policyParameters.cbSize = sizeof(policyParameters);
        policyParameters.dwFlags = policyFlags;

        CERT_CHAIN_POLICY_STATUS policyStatus{};
        policyStatus.cbSize = sizeof(policyStatus);

        if (!CertVerifyCertificateChainPolicy(policyOid, certificateChain, &policyParameters, &policyStatus))
        {
            return false;
        }
        return policyStatus.dwError == ERROR_SUCCESS;
    }

    // True if the signing certificate carries the Windows Store EKU OID
    static bool HasWindowsStoreEku(PCCERT_CONTEXT certificate)
    {
        DWORD usageSize{};
        if (!CertGetEnhancedKeyUsage(certificate, 0, nullptr, &usageSize) || (usageSize == 0))
        {
            return false;
        }

        auto usageBuffer{ wil::make_unique_cotaskmem_nothrow<BYTE[]>(usageSize) };
        if (!usageBuffer)
        {
            return false;
        }
        const auto usage{ reinterpret_cast<PCERT_ENHKEY_USAGE>(usageBuffer.get()) };
        if (!CertGetEnhancedKeyUsage(certificate, 0, usage, &usageSize))
        {
            return false;
        }

        for (DWORD i = 0; i < usage->cUsageIdentifier; ++i)
        {
            const PSTR oid{ usage->rgpszUsageIdentifier[i] };
            if (oid && (strcmp(oid, c_windowsStoreEku) == 0))
            {
                return true;
            }
        }
        return false;
    }

    // Classifies a signed package from its certificate chain and signing
    // certificate. The order mirrors the MSIX SDK: Store, then Windows,
    // then line-of-business, otherwise Unknown.
    static SignatureOrigin ClassifyChain(PCCERT_CHAIN_CONTEXT certificateChain, PCCERT_CONTEXT signingCertificate)
    {
        if (ChainsToMicrosoftRoot(certificateChain))
        {
            return HasWindowsStoreEku(signingCertificate) ? SignatureOrigin::Store : SignatureOrigin::Windows;
        }
        if (ChainSatisfiesPolicy(certificateChain, CERT_CHAIN_POLICY_AUTHENTICODE, 0))
        {
            return SignatureOrigin::LineOfBusiness;
        }
        return SignatureOrigin::Unknown;
    }
};
}
