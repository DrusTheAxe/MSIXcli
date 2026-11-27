// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace msixcli
{
// Synchronous file stream.
//
// Reference counting is thread-safe.
// @warning File operations and the shared seek position require serialization by the caller.
class StreamOverFile final : public IStream
{
public:
    static HRESULT Create(
        _COM_Outptr_ IStream** stream,
        _In_ PCWSTR filename,
        DWORD creationDisposition = CREATE_ALWAYS,
        DWORD shareMode = FILE_SHARE_DELETE | FILE_SHARE_READ,
        DWORD desiredAccess = GENERIC_READ | GENERIC_WRITE) noexcept
    {
        RETURN_HR_IF_NULL(E_POINTER, stream);
        *stream = nullptr;
        RETURN_HR_IF_NULL(E_INVALIDARG, filename);
        RETURN_HR_IF(E_INVALIDARG, filename[0] == L'\0');
        RETURN_HR_IF(E_INVALIDARG, (desiredAccess != GENERIC_READ) &&
                                   (desiredAccess != GENERIC_WRITE) &&
                                   (desiredAccess != (GENERIC_READ | GENERIC_WRITE)));

        auto streamOverFile{ wil::make_unique_nothrow<StreamOverFile>() };
        RETURN_IF_NULL_ALLOC(streamOverFile.get());
        streamOverFile->m_file.reset(::CreateFileW(filename, desiredAccess, shareMode, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr));
        RETURN_LAST_ERROR_IF(!streamOverFile->m_file);

        streamOverFile->m_mode = (desiredAccess == GENERIC_READ ?
                                                   STGM_READ :
                                                   (desiredAccess == GENERIC_WRITE ? STGM_WRITE : STGM_READWRITE));
        const DWORD readWriteSharing{ shareMode & (FILE_SHARE_READ | FILE_SHARE_WRITE) };
        streamOverFile->m_mode |= (readWriteSharing == 0 ?
                                   STGM_SHARE_EXCLUSIVE :
                                   (readWriteSharing == FILE_SHARE_READ ?
                                        STGM_SHARE_DENY_WRITE :
                                        (readWriteSharing == FILE_SHARE_WRITE ?
                                            STGM_SHARE_DENY_READ :
                                            STGM_SHARE_DENY_NONE)));

        *stream = streamOverFile.release();
        return S_OK;
    }

    static HRESULT CreateWriter(
        _COM_Outptr_ IStream** stream,
        _In_ PCWSTR filename,
        DWORD creationDisposition = CREATE_ALWAYS,
        DWORD shareMode = FILE_SHARE_DELETE | FILE_SHARE_READ) noexcept
    {
        RETURN_IF_FAILED(Create(stream, filename, creationDisposition, shareMode, GENERIC_WRITE));
        return S_OK;
    }

    static HRESULT CreateReader(
        _COM_Outptr_ IStream** stream,
        _In_ PCWSTR filename,
        DWORD creationDisposition = OPEN_EXISTING,
        DWORD shareMode = FILE_SHARE_DELETE | FILE_SHARE_READ) noexcept
    {
        RETURN_IF_FAILED(Create(stream, filename, creationDisposition, shareMode, GENERIC_READ));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, _COM_Outptr_ void** object) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, object);
        *object = nullptr;
        if ((iid == __uuidof(IUnknown)) ||
            (iid == __uuidof(ISequentialStream)) ||
            (iid == __uuidof(IStream)))
        {
            *object = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return m_references.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto references{ m_references.fetch_sub(1, std::memory_order_acq_rel) - 1 };
        if (references == 0)
        {
            delete this;
        }
        return references;
    }

    HRESULT STDMETHODCALLTYPE Read(
        _Out_writes_bytes_to_opt_(size, *bytesRead) void* buffer,
        ULONG size,
        _Out_opt_ ULONG* bytesRead) noexcept override
    {
        if (bytesRead)
        {
            *bytesRead = 0;
        }

        if (size == 0)
        {
            return S_OK;
        }

        RETURN_HR_IF_NULL(STG_E_INVALIDPOINTER, buffer);

        DWORD read{};
        const BOOL succeeded{ ::ReadFile(m_file.get(), buffer, size, &read, nullptr) };
        if (bytesRead)
        {
            *bytesRead = read;
        }
        RETURN_IF_WIN32_BOOL_FALSE(succeeded);
        return read == size ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Write(
        _In_reads_bytes_opt_(size) const void* buffer,
        ULONG size,
        _Out_opt_ ULONG* bytesWritten) noexcept override
    {
        if (bytesWritten)
        {
            *bytesWritten = 0;
        }

        if (size == 0)
        {
            return S_OK;
        }

        RETURN_HR_IF_NULL(STG_E_INVALIDPOINTER, buffer);

        DWORD written{};
        const BOOL succeeded{ ::WriteFile(m_file.get(), buffer, size, &written, nullptr) };
        if (bytesWritten)
        {
            *bytesWritten = written;
        }
        RETURN_IF_WIN32_BOOL_FALSE(succeeded);
        RETURN_HR_IF(STG_E_MEDIUMFULL, written != size);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER move,
        DWORD origin,
        _Out_opt_ ULARGE_INTEGER* newPosition) noexcept override
    {
        if (newPosition)
        {
            newPosition->QuadPart = 0;
        }
        RETURN_HR_IF(STG_E_INVALIDFUNCTION, (origin != STREAM_SEEK_SET) && (origin != STREAM_SEEK_CUR) && (origin != STREAM_SEEK_END));

        LARGE_INTEGER position{};
        RETURN_IF_WIN32_BOOL_FALSE(::SetFilePointerEx(m_file.get(), move, &position, origin));
        if (newPosition)
        {
            newPosition->QuadPart = static_cast<ULONGLONG>(position.QuadPart);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER size) noexcept override
    {
        RETURN_HR_IF(STG_E_INVALIDFUNCTION, (size.HighPart & 0x80000000u) != 0);
        FILE_END_OF_FILE_INFO info{};
        info.EndOfFile.QuadPart = static_cast<LONGLONG>(size.QuadPart);
        // Unlike SetFilePointerEx + SetEndOfFile, this does not move the seek position.
        RETURN_IF_WIN32_BOOL_FALSE(::SetFileInformationByHandle(
            m_file.get(), FileEndOfFileInfo, &info, sizeof(info)));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CopyTo(
        _In_ IStream* pstm,
        ULARGE_INTEGER cb,
        _Out_opt_ ULARGE_INTEGER* bytesRead,
        _Out_opt_ ULARGE_INTEGER* bytesWritten) noexcept override
    {
        if (bytesRead)
        {
            bytesRead->QuadPart = 0;
        }
        if (bytesWritten)
        {
            bytesWritten->QuadPart = 0;
        }
        RETURN_HR_IF_NULL(STG_E_INVALIDPOINTER, pstm);
        RETURN_HR_IF(STG_E_ACCESSDENIED, WI_IsFlagSet(m_mode, STGM_WRITE));
        if (cb.QuadPart == 0)
        {
            return S_OK;
        }

        constexpr ULONG bufferSize{ 64 * 1024 };
        if (!m_buffer)
        {
            m_buffer = wil::make_unique_nothrow<BYTE[]>(bufferSize);
            RETURN_IF_NULL_ALLOC(m_buffer.get());
        }

        ULONGLONG totalRead{};
        ULONGLONG totalWritten{};
        auto reportCounts = wil::scope_exit([&]() noexcept
        {
            if (bytesRead)
            {
                bytesRead->QuadPart = totalRead;
            }
            if (bytesWritten)
            {
                bytesWritten->QuadPart = totalWritten;
            }
        });

        while (totalRead < cb.QuadPart)
        {
            const ULONGLONG remaining{ cb.QuadPart - totalRead };
            const ULONG requested{ remaining < bufferSize ?
                static_cast<ULONG>(remaining) : bufferSize };
            ULONG read{};
            const HRESULT readResult{ Read(m_buffer.get(), requested, &read) };
            totalRead += read;
            RETURN_IF_FAILED(readResult);
            if (read == 0)
            {
                break;
            }

            ULONG written{};
            const HRESULT writeResult{ pstm->Write(m_buffer.get(), read, &written) };
            totalWritten += written;
            RETURN_IF_FAILED(writeResult);
            RETURN_HR_IF(STG_E_MEDIUMFULL, written != read);
            if (read < requested)
            {
                break;
            }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Commit(DWORD flags) noexcept override
    {
        RETURN_HR_IF(STG_E_INVALIDFLAG, (flags != STGC_DEFAULT) && (flags != STGC_OVERWRITE));

        RETURN_IF_WIN32_BOOL_FALSE(::FlushFileBuffers(m_file.get()));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Revert() noexcept override
    {
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type) noexcept override
    {
        RETURN_HR_IF(STG_E_INVALIDFUNCTION, (type != LOCK_WRITE) && (type != LOCK_EXCLUSIVE));

        OVERLAPPED overlapped{};
        overlapped.Offset = offset.LowPart;
        overlapped.OffsetHigh = offset.HighPart;
        const DWORD flags{ LOCKFILE_FAIL_IMMEDIATELY | (type == LOCK_EXCLUSIVE ? LOCKFILE_EXCLUSIVE_LOCK : 0u) };
        RETURN_IF_WIN32_BOOL_FALSE(::LockFileEx(m_file.get(), flags, 0, size.LowPart, size.HighPart, &overlapped));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type) noexcept override
    {
        RETURN_HR_IF(STG_E_INVALIDFUNCTION, (type != LOCK_WRITE) && (type != LOCK_EXCLUSIVE));

        OVERLAPPED overlapped{};
        overlapped.Offset = offset.LowPart;
        overlapped.OffsetHigh = offset.HighPart;
        RETURN_IF_WIN32_BOOL_FALSE(::UnlockFileEx(m_file.get(), 0, size.LowPart, size.HighPart, &overlapped));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Stat(_Out_ STATSTG* stat, DWORD flags) noexcept override
    {
        RETURN_HR_IF_NULL(STG_E_INVALIDPOINTER, stat);
        *stat = {};
        RETURN_HR_IF(STG_E_INVALIDFLAG, (flags != STATFLAG_DEFAULT) && (flags != STATFLAG_NONAME) && (flags != STATFLAG_NOOPEN));

        BY_HANDLE_FILE_INFORMATION info{};
        RETURN_IF_WIN32_BOOL_FALSE(::GetFileInformationByHandle(m_file.get(), &info));
        wil::unique_cotaskmem_string name;
        if (flags != STATFLAG_NONAME)
        {
            RETURN_IF_FAILED(wil::GetFinalPathNameByHandleW(m_file.get(), name, wil::VolumePrefix::Dos, wil::PathOptions::Opened));
        }
        stat->type = STGTY_STREAM;
        stat->cbSize.HighPart = info.nFileSizeHigh;
        stat->cbSize.LowPart = info.nFileSizeLow;
        stat->mtime = info.ftLastWriteTime;
        stat->ctime = info.ftCreationTime;
        stat->atime = info.ftLastAccessTime;
        stat->grfMode = m_mode;
        stat->grfLocksSupported = LOCK_WRITE | LOCK_EXCLUSIVE;
        stat->pwcsName = name.release();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(_COM_Outptr_ IStream** stream) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, stream);
        *stream = nullptr;

        // DuplicateHandle would share the seek position, violating IStream::Clone.
        return E_NOTIMPL;
    }

private:
    friend wistd::unique_ptr<StreamOverFile> wil::make_unique_nothrow<StreamOverFile>();
    friend struct wistd::default_delete<StreamOverFile>;

    StreamOverFile() noexcept = default;
    ~StreamOverFile() = default;

    std::atomic<ULONG> m_references{ 1 };
    wil::unique_hfile m_file;
    wistd::unique_ptr<BYTE[]> m_buffer;
    DWORD m_mode{ STGM_WRITE };
};
}
