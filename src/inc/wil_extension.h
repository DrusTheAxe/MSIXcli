// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

namespace wil
{
/// @return null if an error occurred.
inline wil::unique_hlocal_string format_message_nothrow(HRESULT hr) noexcept
{
    wil::unique_hlocal_string message;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(hr), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<PWSTR>(&message), 0, nullptr);
    return message;
}


inline bool string_ends_with(PCWSTR haystack, PCWSTR needle, bool noCase = false)
{
    const auto haystackLength{ wcslen(haystack) };
    const auto needleLength{ wcslen(needle) };
    if (needleLength > haystackLength)
    {
        return false;
    }
    const auto offset{ haystackLength - needleLength };
    return CompareStringOrdinal(haystack + offset, static_cast<int>(needleLength), needle, static_cast<int>(needleLength), noCase ? TRUE : FALSE) == CSTR_EQUAL;
}

inline HRESULT to_hstring_reference(PCWSTR string, HSTRING_HEADER& hstringHeader, HSTRING& hstring)
{
    RETURN_IF_FAILED(::WindowsCreateStringReference(string ? string : L"<null>", static_cast<std::uint32_t>(wcslen(string)), &hstringHeader, &hstring));
    return S_OK;
}
}
