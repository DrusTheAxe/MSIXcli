// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <wil_extension.h>

namespace MSIX
{
inline bool IsPackage(PCWSTR filename)
{
    return wil::string_ends_with(filename, L".msix") || wil::string_ends_with(filename, L".appx");
}

inline bool IsBundle(PCWSTR filename)
{
    return wil::string_ends_with(filename, L".msixbundle") || wil::string_ends_with(filename, L".appxbundle");
}

namespace Packaging
{
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

namespace Package::Reader
{
inline HRESULT OpenFile(
    PCWSTR filename,
    wil::com_ptr_nothrow<IAppxPackageReader>& packageReader)
{
    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(SHCreateStreamOnFileEx(filename, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &stream));

    wil::com_ptr_nothrow<IAppxFactory> factory;
    RETURN_IF_FAILED(::CoCreateInstance(__uuidof(AppxFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    RETURN_IF_FAILED(factory->CreatePackageReader(stream.get(), packageReader.put()));
    return S_OK;
}

inline HRESULT OpenFile(
    PCWSTR filename,
    wil::com_ptr_nothrow<IAppxBundleReader>& bundleReader)
{
    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(SHCreateStreamOnFileEx(filename, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &stream));

    wil::com_ptr_nothrow<IAppxBundleFactory> factory;
    RETURN_IF_FAILED(::CoCreateInstance(__uuidof(AppxBundleFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    RETURN_IF_FAILED(factory->CreateBundleReader(stream.get(), bundleReader.put()));
    return S_OK;
}

inline HRESULT OpenStream(
    ABI::Windows::Storage::Streams::IRandomAccessStreamReference* randomAccessStreamReference,
    wil::com_ptr_nothrow<IStream>& stream)
{
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Storage::Streams::IRandomAccessStreamWithContentType*>> operation;
    RETURN_IF_FAILED(randomAccessStreamReference->OpenReadAsync(operation.put()));

    wil::com_ptr_nothrow<ABI::Windows::Storage::Streams::IRandomAccessStreamWithContentType> randomAccess;
    RETURN_IF_FAILED(wil::wait_for_completion_nothrow(operation.get(), randomAccess.put()));

    RETURN_IF_FAILED(::CreateStreamOverRandomAccessStream(randomAccess.get(), IID_PPV_ARGS(stream.put())));
    return S_OK;
}

inline HRESULT OpenStream(
    ABI::Windows::Foundation::IUriRuntimeClass* uri,
    wil::com_ptr_nothrow<IStream>& stream)
{
    HSTRING_HEADER streamClassBuffer{};
    HSTRING streamClass{};
    RETURN_IF_FAILED(WindowsCreateStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference,
        ARRAYSIZE(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference) - 1, &streamClassBuffer, &streamClass));

    wil::com_ptr_nothrow<ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics> factory;
    RETURN_IF_FAILED(::RoGetActivationFactory(streamClass, IID_PPV_ARGS(factory.put())));

    wil::com_ptr_nothrow<ABI::Windows::Storage::Streams::IRandomAccessStreamReference> randomAccessStreamReference;
    RETURN_IF_FAILED(factory->CreateFromUri(uri, randomAccessStreamReference.put()));

    RETURN_IF_FAILED(OpenStream(randomAccessStreamReference.get(), stream));
    return S_OK;
}

inline HRESULT OpenStream(
    HSTRING uriAsHString,
    wil::com_ptr_nothrow<IStream>& stream)
{
    HSTRING_HEADER uriClassBuffer{};
    HSTRING uriClass{};
    RETURN_IF_FAILED(::WindowsCreateStringReference(RuntimeClass_Windows_Foundation_Uri,
        ARRAYSIZE(RuntimeClass_Windows_Foundation_Uri) - 1, &uriClassBuffer, &uriClass));

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClassFactory> uriFactory;
    RETURN_IF_FAILED(::RoGetActivationFactory(uriClass, IID_PPV_ARGS(uriFactory.put())));

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> uri;
    RETURN_IF_FAILED(uriFactory->CreateUri(uriAsHString, uri.put()));

    RETURN_IF_FAILED(OpenStream(uri.get(), stream));
    return S_OK;
}

inline HRESULT OpenStream(
    PCWSTR uriAsString,
    wil::com_ptr_nothrow<IStream>& stream)
{
    HSTRING_HEADER uriBuffer{};
    HSTRING uriAsHString{};
    RETURN_IF_FAILED(::WindowsCreateStringReference(uriAsString, static_cast<UINT32>(wcslen(uriAsString)), &uriBuffer, &uriAsHString));

    RETURN_IF_FAILED(OpenStream(uriAsHString, stream));
    return S_OK;
}

inline HRESULT OpenUri(
    PCWSTR uri,
    wil::com_ptr_nothrow<IAppxPackageReader>& packageReader)
{
    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(OpenStream(uri, stream));

    wil::com_ptr_nothrow<IAppxFactory> factory;
    RETURN_IF_FAILED(::CoCreateInstance(__uuidof(AppxFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    RETURN_IF_FAILED(factory->CreatePackageReader(stream.get(), packageReader.put()));
    return S_OK;
}

inline HRESULT OpenUri(
    PCWSTR uri,
    wil::com_ptr_nothrow<IAppxBundleReader>& bundleReader)
{
    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(OpenStream(uri, stream));

    wil::com_ptr_nothrow<IAppxBundleFactory> factory;
    RETURN_IF_FAILED(::CoCreateInstance(__uuidof(AppxBundleFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    RETURN_IF_FAILED(factory->CreateBundleReader(stream.get(), bundleReader.put()));
    return S_OK;
}

inline bool IsUrl(PCWSTR string)
{
    return wil::string_starts_with(string, L"https:") || wil::string_starts_with(string, L"http:");
}

inline HRESULT Open(
    PCWSTR uriOrFilename,
    wil::com_ptr_nothrow<IAppxPackageReader>& packageReader)
{
    if (IsUrl(uriOrFilename))
    {
        RETURN_IF_FAILED(OpenUri(uriOrFilename, packageReader));
    }
    else
    {
        RETURN_IF_FAILED(OpenFile(uriOrFilename, packageReader));
    }
    return S_OK;
}

inline HRESULT Open(
    PCWSTR uriOrFilename,
    wil::com_ptr_nothrow<IAppxBundleReader>& bundleReader)
{
    if (IsUrl(uriOrFilename))
    {
        RETURN_IF_FAILED(OpenUri(uriOrFilename, bundleReader));
    }
    else
    {
        RETURN_IF_FAILED(OpenFile(uriOrFilename, bundleReader));
    }
    return S_OK;
}

inline HRESULT PackageToPackageFullName(IAppxPackageReader* packageReader, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
{
    wil::com_ptr_nothrow<IAppxManifestReader> manifestReader;
    RETURN_IF_FAILED(packageReader->GetManifest(&manifestReader));
    wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
    RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
    RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(packageFullName)));
    return S_OK;
}

inline HRESULT PackageToPackageFullName(IAppxBundleReader* bundleReader, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
{
    wil::com_ptr_nothrow<IAppxBundleManifestReader> manifestReader;
    RETURN_IF_FAILED(bundleReader->GetManifest(&manifestReader));
    wil::com_ptr_nothrow<IAppxManifestPackageId> manifestPackageId;
    RETURN_IF_FAILED(manifestReader->GetPackageId(&manifestPackageId));
    RETURN_IF_FAILED(manifestPackageId->GetPackageFullName(wil::out_param(packageFullName)));
    return S_OK;
}

inline HRESULT PackageToPackageFullName(PCWSTR filename, wil::unique_cotaskmem_ptr<WCHAR[]>& packageFullName)
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
}
}
}
