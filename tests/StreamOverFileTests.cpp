// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include <StreamOverFile.h>

#include <array>
#include <cstdio>
#include <cstring>

#define CHECK(condition) RETURN_HR_IF_MSG(E_FAIL, !(condition), "%hs", #condition)

class ShortWriteStream final : public IStream
{
public:
    HRESULT result{ S_OK };

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, object);
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ISequentialStream) || iid == __uuidof(IStream))
        {
            *object = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++m_references; }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto references{ --m_references };
        if (references == 0)
        {
            delete this;
        }
        return references;
    }
    HRESULT STDMETHODCALLTYPE Write(const void* /*buffer*/, ULONG size, ULONG* bytesWritten) noexcept override
    {
        if (bytesWritten)
        {
            *bytesWritten = size == 0 ? 0 : 1;
        }
        return result;
    }
    HRESULT STDMETHODCALLTYPE Read(void* /*buffer*/, ULONG /*size*/, ULONG* /*bytesRead*/) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER /*move*/, DWORD /*origin*/, ULARGE_INTEGER* /*position*/) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER /*size*/) noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyTo(
        IStream* /*stream*/, ULARGE_INTEGER /*size*/,
        ULARGE_INTEGER* /*bytesRead*/, ULARGE_INTEGER* /*bytesWritten*/) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Commit(DWORD /*flags*/) noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Revert() noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRegion(
        ULARGE_INTEGER /*offset*/, ULARGE_INTEGER /*size*/, DWORD /*type*/) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE UnlockRegion(
        ULARGE_INTEGER /*offset*/, ULARGE_INTEGER /*size*/, DWORD /*type*/) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* /*stat*/, DWORD /*flags*/) noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Clone(IStream** /*stream*/) noexcept override { return E_NOTIMPL; }

private:
    std::atomic<ULONG> m_references{ 1 };
};

HRESULT TestStreamOverFile() noexcept
{
    WCHAR directory[MAX_PATH]{};
    const DWORD length{ GetTempPathW(ARRAYSIZE(directory), directory) };
    RETURN_LAST_ERROR_IF(length == 0);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), length >= ARRAYSIZE(directory));
    WCHAR filename[MAX_PATH]{};
    RETURN_LAST_ERROR_IF(GetTempFileNameW(directory, L"sof", 0, filename) == 0);
    auto cleanup = wil::scope_exit([&]() noexcept { LOG_IF_WIN32_BOOL_FALSE(DeleteFileW(filename)); });

    std::array<BYTE, 65537> data{};
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = static_cast<BYTE>(i % 251);
    }
    constexpr ULONG dataSize{ static_cast<ULONG>(data.size()) };
    wil::com_ptr_nothrow<IStream> stream;
    RETURN_IF_FAILED(StreamOverFile::CreateWriter(stream.put(), filename));
    {
        wil::unique_hfile shared{ CreateFileW(
            filename, GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, 0, nullptr) };
        RETURN_LAST_ERROR_IF(!shared);
    }

    wil::com_ptr_nothrow<ISequentialStream> sequential;
    RETURN_IF_FAILED(stream->QueryInterface(IID_PPV_ARGS(sequential.put())));
    wil::com_ptr_nothrow<IUnknown> identity;
    RETURN_IF_FAILED(sequential->QueryInterface(IID_PPV_ARGS(identity.put())));
    CHECK(identity.get() == static_cast<IUnknown*>(stream.get()));
    void* unsupported{ stream.get() };
    CHECK(stream->QueryInterface(__uuidof(IStorage), &unsupported) == E_NOINTERFACE);
    CHECK(unsupported == nullptr);
    CHECK(stream->QueryInterface(__uuidof(IStream), nullptr) == E_POINTER);

    ULARGE_INTEGER size{};
    size.QuadPart = dataSize;
    RETURN_IF_FAILED(stream->SetSize(size));
    ULARGE_INTEGER position{};
    LARGE_INTEGER move{};
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_CUR, &position));
    CHECK(position.QuadPart == 0);

    wil::com_ptr_nothrow<IStream> source;
    RETURN_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, source.put()));
    ULONG written{};
    RETURN_IF_FAILED(source->Write(data.data(), dataSize, &written));
    CHECK(written == dataSize);
    RETURN_IF_FAILED(source->Seek(move, STREAM_SEEK_SET, nullptr));
    ULARGE_INTEGER readCount{};
    ULARGE_INTEGER writeCount{};
    RETURN_IF_FAILED(source->CopyTo(stream.get(), size, &readCount, &writeCount));
    CHECK(readCount.QuadPart == dataSize && writeCount.QuadPart == dataSize);
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_CUR, &position));
    CHECK(position.QuadPart == dataSize);

    size.QuadPart += 100;
    RETURN_IF_FAILED(stream->SetSize(size));
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_CUR, &position));
    CHECK(position.QuadPart == dataSize);
    size.QuadPart = dataSize;
    RETURN_IF_FAILED(stream->SetSize(size));

    move.QuadPart = 0x100000001LL;
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, &position));
    CHECK(position.QuadPart == 0x100000001ULL);
    move.QuadPart = -1;
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_END, &position));
    CHECK(position.QuadPart == dataSize - 1);
    RETURN_IF_FAILED(sequential->Write(data.data() + dataSize - 1, 1, nullptr));

    STATSTG stat{};
    RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
    CHECK(stat.cbSize.QuadPart == dataSize);
    CHECK(stat.type == STGTY_STREAM && stat.grfMode == (STGM_WRITE | STGM_SHARE_DENY_WRITE));
    CHECK(stat.pwcsName == nullptr);
    RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_DEFAULT));
    wil::unique_cotaskmem_string name{ stat.pwcsName };
    CHECK(name && name.get()[0] != L'\0');

    ULONG count{ 123 };
    CHECK(sequential->Read(data.data(), 1, &count) == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) && count == 0);
    CHECK(sequential->Write(nullptr, 1, &count) == STG_E_INVALIDPOINTER && count == 0);
    RETURN_IF_FAILED(sequential->Write(nullptr, 0, &count));
    CHECK(count == 0);
    move.QuadPart = 0;
    CHECK(stream->Seek(move, 999, &position) == STG_E_INVALIDFUNCTION);
    size.QuadPart = 0x8000000000000000ULL;
    CHECK(stream->SetSize(size) == STG_E_INVALIDFUNCTION);
    wil::com_ptr_nothrow<IStream> clone;
    CHECK(stream->Clone(clone.put()) == E_NOTIMPL && !clone);
    readCount.QuadPart = writeCount.QuadPart = 123;
    CHECK(stream->CopyTo(source.get(), size, &readCount, &writeCount) == STG_E_ACCESSDENIED);
    CHECK(readCount.QuadPart == 0 && writeCount.QuadPart == 0);

    ULARGE_INTEGER offset{};
    size.QuadPart = 1;
    RETURN_IF_FAILED(stream->LockRegion(offset, size, LOCK_EXCLUSIVE));
    RETURN_IF_FAILED(stream->UnlockRegion(offset, size, LOCK_EXCLUSIVE));
    CHECK(stream->LockRegion(offset, size, LOCK_ONLYONCE) == STG_E_INVALIDFUNCTION);
    RETURN_IF_FAILED(stream->Commit(STGC_DEFAULT));

    identity.reset();
    sequential.reset();
    stream.reset();
    wil::unique_hfile input{ CreateFileW(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr) };
    RETURN_LAST_ERROR_IF(!input);
    std::array<BYTE, dataSize> actual{};
    DWORD read{};
    RETURN_IF_WIN32_BOOL_FALSE(ReadFile(input.get(), actual.data(), dataSize, &read, nullptr));
    CHECK(read == dataSize && std::memcmp(actual.data(), data.data(), dataSize) == 0);
    input.reset();

    RETURN_IF_FAILED(StreamOverFile::CreateReader(stream.put(), filename));
    RETURN_IF_FAILED(stream->Read(actual.data(), dataSize, &count));
    CHECK(count == dataSize && std::memcmp(actual.data(), data.data(), dataSize) == 0);
    CHECK(stream->Read(actual.data(), 1, &count) == S_FALSE && count == 0);
    RETURN_IF_FAILED(stream->Read(nullptr, 0, &count));
    CHECK(count == 0);
    CHECK(stream->Read(nullptr, 1, &count) == STG_E_INVALIDPOINTER && count == 0);
    move.QuadPart = -1;
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_END, nullptr));
    BYTE tail[2]{};
    CHECK(stream->Read(tail, ARRAYSIZE(tail), &count) == S_FALSE && count == 1);
    CHECK(tail[0] == data.back());
    move.QuadPart = 0;
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    RETURN_IF_FAILED(stream->Read(tail, 1, nullptr));
    CHECK(tail[0] == data.front());
    RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
    CHECK(stat.grfMode == (STGM_READ | STGM_SHARE_DENY_WRITE));
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    RETURN_IF_FAILED(source->Seek(move, STREAM_SEEK_SET, nullptr));
    RETURN_IF_FAILED(source->SetSize(ULARGE_INTEGER{}));
    size.QuadPart = dataSize;
    RETURN_IF_FAILED(stream->CopyTo(source.get(), size, &readCount, &writeCount));
    CHECK(readCount.QuadPart == dataSize && writeCount.QuadPart == dataSize);
    RETURN_IF_FAILED(source->Seek(move, STREAM_SEEK_SET, nullptr));
    RETURN_IF_FAILED(source->Read(actual.data(), dataSize, &count));
    CHECK(count == dataSize && std::memcmp(actual.data(), data.data(), dataSize) == 0);

    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    size.QuadPart = ~0ULL;
    RETURN_IF_FAILED(stream->CopyTo(source.get(), size, &readCount, &writeCount));
    CHECK(readCount.QuadPart == dataSize && writeCount.QuadPart == dataSize);
    RETURN_IF_FAILED(stream->CopyTo(source.get(), size, &readCount, &writeCount));
    CHECK(readCount.QuadPart == 0 && writeCount.QuadPart == 0);
    size.QuadPart = 0;
    RETURN_IF_FAILED(stream->CopyTo(source.get(), size, &readCount, &writeCount));
    CHECK(readCount.QuadPart == 0 && writeCount.QuadPart == 0);
    CHECK(stream->CopyTo(nullptr, size, &readCount, &writeCount) == STG_E_INVALIDPOINTER);

    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    size.QuadPart = 2;
    RETURN_IF_FAILED(stream->CopyTo(source.get(), size, nullptr, nullptr));

    auto shortWriter{ wil::make_unique_nothrow<ShortWriteStream>() };
    RETURN_IF_NULL_ALLOC(shortWriter.get());
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    CHECK(stream->CopyTo(shortWriter.get(), size, &readCount, &writeCount) == STG_E_MEDIUMFULL);
    CHECK(readCount.QuadPart == 2 && writeCount.QuadPart == 1);
    shortWriter->result = STG_E_WRITEFAULT;
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    CHECK(stream->CopyTo(shortWriter.get(), size, &readCount, &writeCount) == STG_E_WRITEFAULT);
    CHECK(readCount.QuadPart == 2 && writeCount.QuadPart == 1);
    stream.reset();

    RETURN_IF_FAILED(StreamOverFile::Create(stream.put(), filename, OPEN_EXISTING));
    RETURN_IF_FAILED(stream->Write(data.data(), 1, nullptr));
    RETURN_IF_FAILED(stream->Seek(move, STREAM_SEEK_SET, nullptr));
    RETURN_IF_FAILED(stream->Read(tail, 1, nullptr));
    CHECK(tail[0] == data.front());
    RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
    CHECK(stat.grfMode == (STGM_READWRITE | STGM_SHARE_DENY_WRITE));
    stream.reset();

    CHECK(StreamOverFile::Create(stream.put(), filename, CREATE_NEW) == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
    CHECK(!stream);
    RETURN_IF_FAILED(StreamOverFile::Create(stream.put(), filename));
    RETURN_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
    CHECK(stat.cbSize.QuadPart == 0);
    CHECK(stat.grfMode == (STGM_READWRITE | STGM_SHARE_DENY_WRITE));
    RETURN_IF_FAILED(stream->SetSize(ULARGE_INTEGER{}));
    RETURN_IF_FAILED(stream->Commit(STGC_DEFAULT));
    stream.reset();
    return S_OK;
}

int wmain()
{
    const HRESULT hr{ TestStreamOverFile() };
    if (FAILED(hr))
    {
        std::fwprintf(stderr, L"StreamOverFile tests failed: 0x%08lX\n", static_cast<ULONG>(hr));
        return 1;
    }
    std::wprintf(L"StreamOverFile tests passed.\n");
    return 0;
}
