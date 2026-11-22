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

namespace Packaging::Package::Reader
{
inline HRESULT Open(
    PCWSTR filename,
    wil::com_ptr_nothrow<IAppxPackageReader>& packageReader)
{
    wil::com_ptr_nothrow<IAppxFactory> factory;
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(AppxFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(SHCreateStreamOnFileEx(filename, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &stream));
    RETURN_IF_FAILED(factory->CreatePackageReader(stream.get(), packageReader.put()));
    return S_OK;
}

inline HRESULT Open(
    PCWSTR filename,
    wil::com_ptr_nothrow<IAppxBundleReader>& bundleReader)
{
    wil::com_ptr_nothrow<IAppxBundleFactory> factory;
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(AppxBundleFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(SHCreateStreamOnFileEx(filename, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &stream));
    RETURN_IF_FAILED(factory->CreateBundleReader(stream.get(), bundleReader.put()));
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
