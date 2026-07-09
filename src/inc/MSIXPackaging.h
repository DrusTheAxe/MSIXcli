// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace MSIX::Packaging::Package::Reader
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
}
