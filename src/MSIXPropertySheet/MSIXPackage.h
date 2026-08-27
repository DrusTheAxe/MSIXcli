// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <minappmodel.h>
#include <appmodel.h>

#include <MSIXPackaging.h>
#include <MSIXSigning.h>

namespace MSIX
{
class Package
{
public:
    Package() = default;
    ~Package() = default;

    void Close()
    {
        m_packageReader.reset();
        m_bundleReader.reset();
    }

    HRESULT OpenPackage(
        PCWSTR filename,
        ABI::Windows::Management::Deployment::IPackageManager12* packageManager12)
    {
        wil::com_ptr_nothrow<IAppxFactory> factory;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, m_packageReader), "%ls", filename);

        wil::com_ptr_nothrow<IAppxManifestReader> manifestReader;
        RETURN_IF_FAILED(m_packageReader->GetManifest(&manifestReader));
        wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
        RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
        RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(m_packageFullName)));
        RETURN_IF_FAILED(manifestPackageId->GetPackageFamilyName(wil::out_param(m_packageFamilyName)));

        RETURN_IF_FAILED(DetectSignatureOrigin(m_packageReader.get()));

        wil::com_ptr_nothrow<IAppxBlockMapReader> blockMapReader;
        RETURN_IF_FAILED(m_packageReader->GetBlockMap(&blockMapReader));
        RETURN_IF_FAILED(LoadSizes(m_packageReader.get(), blockMapReader.get()));

        RETURN_IF_FAILED(DetectPackage(packageManager12));
        return S_OK;
    }

    HRESULT OpenBundle(
        PCWSTR filename,
        ABI::Windows::Management::Deployment::IPackageManager12* packageManager12)
    {
        wil::com_ptr_nothrow<IAppxFactory> factory;
        RETURN_IF_FAILED_MSG(MSIX::Packaging::Package::Reader::Open(filename, m_bundleReader), "%ls", filename);

        wil::com_ptr_nothrow<IAppxBundleManifestReader> bundleManifestReader;
        RETURN_IF_FAILED(m_bundleReader->GetManifest(&bundleManifestReader));
        wil::com_ptr_nothrow<IAppxManifestPackageId> bundleManifestPackageId;
        RETURN_IF_FAILED(bundleManifestReader->GetPackageId(&bundleManifestPackageId));
        RETURN_IF_FAILED(bundleManifestPackageId->GetPackageFullName(wil::out_param(m_packageFullName)));
        RETURN_IF_FAILED(bundleManifestPackageId->GetPackageFamilyName(wil::out_param(m_packageFamilyName)));

        RETURN_IF_FAILED(DetectSignatureOrigin(m_bundleReader.get()));

        wil::com_ptr_nothrow<IAppxBlockMapReader> blockMapReader;
        RETURN_IF_FAILED(m_bundleReader->GetBlockMap(&blockMapReader));
        RETURN_IF_FAILED(LoadSizes(m_bundleReader.get(), blockMapReader.get()));

        RETURN_IF_FAILED(DetectPackage(packageManager12));
        return S_OK;
    }

    HRESULT Open(
        PCWSTR filename,
        ABI::Windows::Management::Deployment::IPackageManager12* packageManager12)
    {
        if (MSIX::IsPackage(filename))
        {
            RETURN_IF_FAILED(OpenPackage(filename, packageManager12));
        }
        else if (MSIX::IsBundle(filename))
        {
            RETURN_IF_FAILED(OpenBundle(filename, packageManager12));
        }
        else
        {
            RETURN_HR_MSG(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED), "%s", filename);
        }
        return S_OK;
    }

    HRESULT DetectSignatureOrigin(IAppxPackageReader* packageReader)
    {
        RETURN_IF_FAILED(MSIX::Signing::DetectSignatureOrigin(packageReader, m_signatureOrigin,
            m_certificateThumbprintSize, m_certificateThumbprint, m_certificateSubject, m_certificateIssuer,
            m_certificateValidFrom, m_certificateValidTo, m_certificateValid));
        return S_OK;
    }

    HRESULT DetectSignatureOrigin(IAppxBundleReader* bundleReader)
    {
        RETURN_IF_FAILED(MSIX::Signing::DetectSignatureOrigin(bundleReader, m_signatureOrigin,
            m_certificateThumbprintSize, m_certificateThumbprint, m_certificateSubject, m_certificateIssuer,
            m_certificateValidFrom, m_certificateValidTo, m_certificateValid));
        return S_OK;
    }

    HRESULT LoadSizes(IAppxPackageReader* packageReader, IAppxBlockMapReader* blockMapReader)
    {
        RETURN_IF_FAILED(LoadFootprintSizes(packageReader, blockMapReader));
        RETURN_IF_FAILED(LoadPayloadSizes(blockMapReader));
        return S_OK;
    }

    HRESULT LoadSizes(IAppxBundleReader* bundleReader, IAppxBlockMapReader* blockMapReader)
    {
        RETURN_IF_FAILED(LoadFootprintSizes(bundleReader, blockMapReader));
        RETURN_IF_FAILED(LoadPayloadSizes(blockMapReader));
        return S_OK;
    }

    HRESULT LoadFootprintSizes(IAppxPackageReader* packageReader, IAppxBlockMapReader* blockMapReader)
    {
        for (const auto type : s_packageFootprintFileTypes)
        {
            // AppxBlockMap.xml and AppxSignature.p7x sizes aren't available via the block map
            if ((type == APPX_FOOTPRINT_FILE_TYPE_BLOCKMAP) || (type == APPX_FOOTPRINT_FILE_TYPE_SIGNATURE))
            {
                wil::com_ptr_nothrow<IAppxFile> footprintFile;
                RETURN_IF_FAILED(packageReader->GetFootprintFile(type, &footprintFile));
                wil::com_ptr_nothrow<IStream> stream;
                RETURN_IF_FAILED(footprintFile->GetStream(&stream));
                STATSTG stat{};
                RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
                std::uint64_t size{ stat.cbSize.QuadPart };
                m_footprintFileSize[FootprintFileSizeIndex(type, false)] = size;
                // Packaging API doesn't exposed the compressed sizes
                //TODO Read the .msix as a ZIP file and examine the CentralDirectory entry
            }
            else
            {
                PCWSTR filename{ s_packageFileNames[type] };

                wil::com_ptr_nothrow<IAppxBlockMapFile> blockMapFile;
                const HRESULT hr{ blockMapReader->GetFile(filename, &blockMapFile) };
                if (SUCCEEDED(hr))
                {
                    std::uint64_t size{};
                    RETURN_IF_FAILED(blockMapFile->GetUncompressedSize(&size));
                    m_footprintFileSize[FootprintFileSizeIndex(type, false)] = size;

                    std::uint64_t compressedSize{};
                    wil::com_ptr_nothrow<IAppxBlockMapBlocksEnumerator> blocksEnumerator;
                    RETURN_IF_FAILED(blockMapFile->GetBlocks(&blocksEnumerator));
                    BOOL hasCurrent{};
                    RETURN_IF_FAILED(blocksEnumerator->GetHasCurrent(&hasCurrent));
                    while (hasCurrent)
                    {
                        wil::com_ptr_nothrow<IAppxBlockMapBlock> block;
                        RETURN_IF_FAILED(blocksEnumerator->GetCurrent(&block));
                        UINT32 blockCompressedSize{};
                        RETURN_IF_FAILED(block->GetCompressedSize(&blockCompressedSize));
                        compressedSize += blockCompressedSize;
                        BOOL hasNext{};
                        RETURN_IF_FAILED(blocksEnumerator->MoveNext(&hasNext));
                        hasCurrent = hasNext;
                    }
                    m_footprintFileSize[FootprintFileSizeIndex(type, true)] = compressedSize;
                }
                else
                {
                    RETURN_HR_IF_MSG(hr, hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "%ls", filename);
                }
            }
        }
        return S_OK;
    }

    HRESULT LoadFootprintSizes(IAppxBundleReader* bundleReader, IAppxBlockMapReader* blockMapReader)
    {
        for (const auto type : s_bundleFootprintFileTypes)
        {
            // AppxBlockMap.xml and AppxSignature.p7x sizes aren't available via the block map
            if ((type == APPX_BUNDLE_FOOTPRINT_FILE_TYPE_BLOCKMAP) || (type == APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE))
            {
                wil::com_ptr_nothrow<IAppxFile> footprintFile;
                RETURN_IF_FAILED(bundleReader->GetFootprintFile(type, &footprintFile));
                wil::com_ptr_nothrow<IStream> stream;
                RETURN_IF_FAILED(footprintFile->GetStream(&stream));
                STATSTG stat{};
                RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
                std::uint64_t size{ stat.cbSize.QuadPart };
                m_footprintFileSize[FootprintFileSizeIndex(type, false)] = size;
                // Packaging API doesn't exposed the compressed sizes
                //TODO Read the .msix as a ZIP file and examine the CentralDirectory entry
            }
            else
            {
                PCWSTR filename{ s_bundleFileNames[type] };

                wil::com_ptr_nothrow<IAppxBlockMapFile> blockMapFile;
                const HRESULT hr{ blockMapReader->GetFile(filename, &blockMapFile) };
                if (SUCCEEDED(hr))
                {
                    std::uint64_t size{};
                    RETURN_IF_FAILED(blockMapFile->GetUncompressedSize(&size));
                    m_footprintFileSize[FootprintFileSizeIndex(type, false)] = size;

                    std::uint64_t compressedSize{};
                    wil::com_ptr_nothrow<IAppxBlockMapBlocksEnumerator> blocksEnumerator;
                    RETURN_IF_FAILED(blockMapFile->GetBlocks(&blocksEnumerator));
                    BOOL hasCurrent{};
                    RETURN_IF_FAILED(blocksEnumerator->GetHasCurrent(&hasCurrent));
                    while (hasCurrent)
                    {
                        wil::com_ptr_nothrow<IAppxBlockMapBlock> block;
                        RETURN_IF_FAILED(blocksEnumerator->GetCurrent(&block));
                        UINT32 blockCompressedSize{};
                        RETURN_IF_FAILED(block->GetCompressedSize(&blockCompressedSize));
                        compressedSize += blockCompressedSize;
                        BOOL hasNext{};
                        RETURN_IF_FAILED(blocksEnumerator->MoveNext(&hasNext));
                        hasCurrent = hasNext;
                    }
                    m_footprintFileSize[FootprintFileSizeIndex(type, true)] = compressedSize;
                }
                else
                {
                    RETURN_HR_IF_MSG(hr, hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "%ls", filename);
                }
            }
        }
        return S_OK;
    }

    HRESULT LoadPayloadSizes(IAppxBlockMapReader* blockMapReader)
    {
        wil::com_ptr_nothrow<IAppxBlockMapFilesEnumerator> filesEnumerator;
        const HRESULT hr{ blockMapReader->GetFiles(&filesEnumerator) };
        if (SUCCEEDED(hr))
        {
            BOOL hasCurrent{};
            RETURN_IF_FAILED(filesEnumerator->GetHasCurrent(&hasCurrent));
            while (hasCurrent)
            {
                wil::com_ptr_nothrow<IAppxBlockMapFile> blockMapFile;
                RETURN_IF_FAILED(filesEnumerator->GetCurrent(&blockMapFile));
                std::uint64_t size{};
                RETURN_IF_FAILED(blockMapFile->GetUncompressedSize(&size));
                m_packagePayloadFilesSizeUncompressed += size;
                std::uint64_t compressedSize{};
                wil::com_ptr_nothrow<IAppxBlockMapBlocksEnumerator> blocksEnumerator;
                RETURN_IF_FAILED(blockMapFile->GetBlocks(&blocksEnumerator));
                BOOL hasCurrentBlock{};
                RETURN_IF_FAILED(blocksEnumerator->GetHasCurrent(&hasCurrentBlock));
                while (hasCurrentBlock)
                {
                    wil::com_ptr_nothrow<IAppxBlockMapBlock> block;
                    RETURN_IF_FAILED(blocksEnumerator->GetCurrent(&block));
                    UINT32 blockCompressedSize{};
                    RETURN_IF_FAILED(block->GetCompressedSize(&blockCompressedSize));
                    compressedSize += blockCompressedSize;
                    BOOL hasNextBlock{};
                    RETURN_IF_FAILED(blocksEnumerator->MoveNext(&hasNextBlock));
                    hasCurrentBlock = hasNextBlock;
                }
                m_packagePayloadFilesSizeCompressed += compressedSize;
                ++m_packagePayloadFilesCount;
                BOOL hasNext{};
                RETURN_IF_FAILED(filesEnumerator->MoveNext(&hasNext));
                hasCurrent = hasNext;
            }
        }
        return S_OK;
    }

    HRESULT AddCertificate(MSIX::Signing::AddResult& result)
    {
        RETURN_HR_IF(E_NOT_VALID_STATE, !m_packageReader);

        RETURN_IF_FAILED(MSIX::Signing::AddCertificate(m_packageReader.get(), result));
        return S_OK;
    }

    HRESULT DetectPackage(
        ABI::Windows::Management::Deployment::IPackageManager12* packageManager12)
    {
        LONG rc{ ::GetStagedPackageOrigin(m_packageFullName.get(), &m_packageOrigin) };
        if (rc != ERROR_SUCCESS)
        {
            const HRESULT hr{ HRESULT_FROM_WIN32(rc) };
            std::ignore = LOG_HR_IF_MSG(hr, hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "%ls", m_packageFullName.get());
            m_packageOrigin = ::PackageOrigin_Unknown;
        }

        m_isRegistered = false;
        wistd::unique_ptr<PWSTR[]> packageFullNames;
        wistd::unique_ptr<WCHAR[]> buffer;
        std::uint32_t count{};
        std::uint32_t bufferLength{};
        const auto filter{ PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT | PACKAGE_FILTER_OPTIONAL | PACKAGE_FILTER_RESOURCE | PACKAGE_FILTER_BUNDLE | PACKAGE_INFORMATION_BASIC };
        rc = ::FindPackagesByPackageFamily(m_packageFamilyName.get(), filter, &count, nullptr, &bufferLength, nullptr, nullptr);
        if (rc == ERROR_SUCCESS)
        {
            // No packages in the family registered to the user
        }
        else
        {
            RETURN_HR_IF(HRESULT_FROM_WIN32(rc), rc != ERROR_INSUFFICIENT_BUFFER);
            packageFullNames = wil::make_unique_nothrow<PWSTR[]>(count);
            RETURN_IF_NULL_ALLOC(packageFullNames);
            buffer = wil::make_unique_nothrow<WCHAR[]>(bufferLength);
            RETURN_IF_NULL_ALLOC(buffer);
            RETURN_IF_WIN32_ERROR(::FindPackagesByPackageFamily(m_packageFamilyName.get(), filter, &count, packageFullNames.get(), &bufferLength, buffer.get(), nullptr));
        }
        for (std::uint32_t index=0; index < count; ++index)
        {
            PCWSTR packageFullName{ packageFullNames[index] };
            if (CompareStringOrdinal(m_packageFullName.get(), -1, packageFullName, -1, TRUE) == CSTR_EQUAL)
            {
                m_isRegistered = true;
            }
            else
            {
                BYTE packageIdBuffer[
                    sizeof(PACKAGE_ID) +
                    sizeof(WCHAR) * (PACKAGE_NAME_MAX_LENGTH + 1) +
                    sizeof(WCHAR) * (PACKAGE_VERSION_MAX_LENGTH + 1) +
                    sizeof(WCHAR) * (PACKAGE_ARCHITECTURE_MAX_LENGTH + 1) +
                    sizeof(WCHAR) * (PACKAGE_RESOURCEID_MAX_LENGTH + 1) +
                    sizeof(WCHAR) * (PACKAGE_PUBLISHERID_MAX_LENGTH + 1)]{};
                std::uint32_t packageIdBufferLength{ ARRAYSIZE(packageIdBuffer) };
                RETURN_IF_WIN32_ERROR_MSG(::PackageIdFromFullName(packageFullName, PACKAGE_INFORMATION_BASIC, &packageIdBufferLength, packageIdBuffer), "%ls", packageFullName);
                const auto& packageId{ *reinterpret_cast<PACKAGE_ID*>(packageIdBuffer) };
                if (m_newerVersionFound.Version < packageId.version.Version)
                {
                    m_newerVersionFound.Version = packageId.version.Version;
                }
            }
        }

        if (packageManager12)
        {
            HSTRING_HEADER packageFullNameHeader{};
            HSTRING packageFullNameAsHstring{};
            const std::uint32_t packageFullNameLength{ static_cast<std::uint32_t>(wcslen(m_packageFullName.get())) };
            RETURN_IF_FAILED(WindowsCreateStringReference(m_packageFullName.get(), packageFullNameLength, &packageFullNameHeader, &packageFullNameAsHstring));
            boolean isRemovalPending{};
            RETURN_IF_FAILED(packageManager12->IsPackageRemovalPending(packageFullNameAsHstring, &isRemovalPending));
            m_isRemovalPending = !!isRemovalPending;
        }

        return S_OK;
    }

    bool IsPackage() const
    {
        return !!m_packageReader;
    }

    bool IsBundle() const
    {
        return !!m_bundleReader;
    }

    IAppxPackageReader* PackageReader()
    {
        return m_packageReader.get();
    }

    IAppxBundleReader* BundleReader()
    {
        return m_bundleReader.get();
    }

    PCWSTR PackageFullName() const
    {
        return m_packageFullName.get();
    }

    PCWSTR PackageFamilyName() const
    {
        return m_packageFamilyName.get();
    }

    PACKAGE_VERSION NewerVersionFound() const
    {
        return m_newerVersionFound;
    }

    bool IsNewerVersionAvailable() const
    {
        return m_newerVersionFound.Version > 0;
    }

    bool IsSigned() const
    {
        return (m_signatureOrigin == MSIX::SignatureOrigin::Windows) ||
               (m_signatureOrigin == MSIX::SignatureOrigin::Store) ||
               (m_signatureOrigin == MSIX::SignatureOrigin::LineOfBusiness) ||
               (m_signatureOrigin == MSIX::SignatureOrigin::Unknown);
    }

    PCWSTR CertificateSubject() const
    {
        return m_certificateSubject.get();
    }

    PCWSTR CertificateIssuer() const
    {
        return m_certificateIssuer.get();
    }

    HRESULT GetCertificateThumbprint(size_t& thumbprintSize, wil::unique_cotaskmem_ptr<BYTE[]>& thumbprint)
    {
        thumbprintSize = 0;
        thumbprint.reset();

        if ((m_certificateThumbprintSize > 0) && m_certificateThumbprint)
        {
            auto buffer{ wil::make_unique_cotaskmem_nothrow<BYTE[]>(m_certificateThumbprintSize) };
            RETURN_IF_NULL_ALLOC(buffer);
            memcpy(buffer.get(), m_certificateThumbprint.get(), m_certificateThumbprintSize);
            thumbprint = wistd::move(buffer);
            thumbprintSize = m_certificateThumbprintSize;
        }
        return S_OK;
    }

    FILETIME CertificateValidFrom() const
    {
        return m_certificateValidFrom;
    }

    FILETIME CertificateValidTo() const
    {
        return m_certificateValidTo;
    }

    MSIX::Signing::CertificateValid CertificateValid() const
    {
        return m_certificateValid;
    }

    MSIX::SignatureOrigin SignatureOrigin() const
    {
        return m_signatureOrigin;
    }

    ::PackageOrigin PackageOrigin() const
    {
        return m_packageOrigin;
    }

    PCWSTR PackageOriginString() const
    {
        return ToString(m_packageOrigin);
    }

    static constexpr PCWSTR ToString(::PackageOrigin packageOrigin)
    {
        switch (packageOrigin)
        {
            case PackageOrigin_Unknown:             return L"???";
            case PackageOrigin_Unsigned:            return L"Unsigned (not signed, or not validly)";
            case PackageOrigin_Inbox:               return L"Inbox (aka Windows)";
            case PackageOrigin_Store:               return L"Store";
            case PackageOrigin_DeveloperUnsigned:   return L"Developer Unsigned";
            case PackageOrigin_DeveloperSigned:     return L"Developer Signed";
            case PackageOrigin_LineOfBusiness:      return L"Line of Business";
            default:                                return L"???";
        }
    }

    bool IsStaged() const
    {
        return m_packageOrigin != ::PackageOrigin_Unknown;
    }

    bool IsRegistered() const
    {
        return m_isRegistered;
    }

    bool IsRemovable() const
    {
        return (m_packageOrigin != ::PackageOrigin_Inbox) && (IsRegistered() || IsStaged());
    }

    bool IsRemovalPending() const
    {
        return m_isRemovalPending;
    }

    static constexpr PCWSTR PackageFootprintFileName(APPX_FOOTPRINT_FILE_TYPE type)
    {
        static_assert(APPX_FOOTPRINT_FILE_TYPE_MANIFEST == 0, "APPX_FOOTPRINT_FILE_TYPE_MANIFEST != 0");
        static_assert(APPX_FOOTPRINT_FILE_TYPE_CONTENTGROUPMAP == ARRAYSIZE(s_packageFileNames) - 1, "APPX_FOOTPRINT_FILE_TYPE_CONTENTGROUPMAP != size of s_packageFileNames");
        if ((type < APPX_FOOTPRINT_FILE_TYPE_MANIFEST) || (type > APPX_FOOTPRINT_FILE_TYPE_CONTENTGROUPMAP))
        {
            return L"???";
        }
        return s_packageFileNames[type];
    };


    std::uint64_t PackageFootprintFileSize(APPX_FOOTPRINT_FILE_TYPE type, bool compressed) const
    {
        static_assert(APPX_FOOTPRINT_FILE_TYPE_MANIFEST == 0, "APPX_FOOTPRINT_FILE_TYPE_MANIFEST != 0");
        if ((type < APPX_FOOTPRINT_FILE_TYPE_MANIFEST) || (type > APPX_FOOTPRINT_FILE_TYPE_CONTENTGROUPMAP))
        {
            return 0;
        }
        return m_footprintFileSize[FootprintFileSizeIndex(type, compressed)];
    }

    static constexpr PCWSTR BundleFootprintFileName(APPX_BUNDLE_FOOTPRINT_FILE_TYPE type)
    {
        static_assert(APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST == 0, "APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST != 0");
        static_assert(APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE == ARRAYSIZE(s_bundleFileNames) - 1, "APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE != size of s_bundleFileNames");
        if ((type < APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST) || (type > APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE))
        {
            return L"???";
        }
        return s_bundleFileNames[type];
    };


    std::uint64_t BundleFootprintFileSize(APPX_BUNDLE_FOOTPRINT_FILE_TYPE type, bool compressed) const
    {
        static_assert(APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST == 0, "APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST != 0");
        if ((type < APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST) || (type > APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE))
        {
            return 0;
        }
        return m_footprintFileSize[FootprintFileSizeIndex(type, compressed)];
    }

    std::uint64_t FootprintTotalSizeCompressed(bool compressed) const
    {
        if (IsPackage())
        {
            return PackageFootprintTotalSizeCompressed(compressed);
        }
        else if (IsBundle())
        {
            return BundleFootprintTotalSizeCompressed(compressed);
        }
        else
        {
            return 0;
        }
    }

    std::uint64_t PackageFootprintTotalSizeCompressed(bool compressed) const
    {
        std::uint64_t size{};
        for (const auto type : s_packageFootprintFileTypes)
        {
            size += m_footprintFileSize[FootprintFileSizeIndex(type, compressed)];
        }
        return size;
    }

    std::uint64_t BundleFootprintTotalSizeCompressed(bool compressed) const
    {
        std::uint64_t size{};
        for (const auto type : s_bundleFootprintFileTypes)
        {
            size += m_footprintFileSize[FootprintFileSizeIndex(type, compressed)];
        }
        return size;
    }

    std::uint64_t PayloadTotalCount() const
    {
        return m_packagePayloadFilesCount;
    }

    std::uint64_t PayloadTotalSizeCompressed() const
    {
        return m_packagePayloadFilesSizeCompressed;
    }

    std::uint64_t PayloadTotalSizeUncompressed() const
    {
        return m_packagePayloadFilesSizeUncompressed;
    }

private:
    int FootprintFileSizeIndex(APPX_FOOTPRINT_FILE_TYPE type, bool compressed) const
    {
        return type * 2 + (compressed ? 0 : 1);
    }

    int FootprintFileSizeIndex(APPX_BUNDLE_FOOTPRINT_FILE_TYPE type, bool compressed) const
    {
        return type * 2 + (compressed ? 0 : 1);
    }

private:
    inline static constexpr APPX_FOOTPRINT_FILE_TYPE s_packageFootprintFileTypes[]{
        APPX_FOOTPRINT_FILE_TYPE_MANIFEST,
        APPX_FOOTPRINT_FILE_TYPE_BLOCKMAP,
        APPX_FOOTPRINT_FILE_TYPE_SIGNATURE,
        APPX_FOOTPRINT_FILE_TYPE_CODEINTEGRITY,
        APPX_FOOTPRINT_FILE_TYPE_CONTENTGROUPMAP
    };
    inline static constexpr PCWSTR s_packageFileNames[]{
        L"AppxManifest.xml",
        L"AppxBlockMap.xml",
        L"AppxSignature.p7x",
        L"CodeIntegrity.cat",
        L"ContentGroupMap.xml"
    };

private:
    inline static constexpr APPX_BUNDLE_FOOTPRINT_FILE_TYPE s_bundleFootprintFileTypes[]{
        APPX_BUNDLE_FOOTPRINT_FILE_TYPE_MANIFEST,
        APPX_BUNDLE_FOOTPRINT_FILE_TYPE_BLOCKMAP,
        APPX_BUNDLE_FOOTPRINT_FILE_TYPE_SIGNATURE
    };
    inline static constexpr PCWSTR s_bundleFileNames[]{
        L"AppxMetadata/AppxManifest.xml",
        L"AppxBlockMap.xml",
        L"AppxSignature.p7x"
    };

private:
    wil::com_ptr_nothrow<IAppxPackageReader> m_packageReader;
    wil::com_ptr_nothrow<IAppxBundleReader> m_bundleReader;
    wil::unique_cotaskmem_ptr<WCHAR[]> m_packageFullName;
    wil::unique_cotaskmem_ptr<WCHAR[]> m_packageFamilyName;
    ::PackageOrigin m_packageOrigin{ ::PackageOrigin_Unknown };
    bool m_isRegistered{};
    PACKAGE_VERSION m_newerVersionFound{};
    bool m_isRemovalPending{};
    MSIX::SignatureOrigin m_signatureOrigin{ MSIX::SignatureOrigin::Unsigned };
    wil::unique_cotaskmem_ptr<WCHAR[]> m_certificateSubject;
    wil::unique_cotaskmem_ptr<WCHAR[]> m_certificateIssuer;
    size_t m_certificateThumbprintSize{};
    wil::unique_cotaskmem_ptr<BYTE[]> m_certificateThumbprint;
    FILETIME m_certificateValidFrom{};
    FILETIME m_certificateValidTo{};
    MSIX::Signing::CertificateValid m_certificateValid{ MSIX::Signing::CertificateValid::Unknown };
    std::uint64_t m_footprintFileSize[10]{};
    std::uint64_t m_packagePayloadFilesCount{};
    std::uint64_t m_packagePayloadFilesSizeCompressed{};
    std::uint64_t m_packagePayloadFilesSizeUncompressed{};
};
}
