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

inline bool string_starts_with(PCWSTR haystack, PCWSTR needle, bool noCase = false)
{
    const auto haystackLength{ wcslen(haystack) };
    const auto needleLength{ wcslen(needle) };
    if (needleLength > haystackLength)
    {
        return false;
    }
    return CompareStringOrdinal(haystack, static_cast<int>(needleLength), needle, static_cast<int>(needleLength), noCase ? TRUE : FALSE) == CSTR_EQUAL;
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

namespace details
{
    inline int hexdigit_to_byte(const wchar_t c)
    {
        if ((c >= L'0') && (c <= L'9'))
        {
            return c - L'0';
        }
        else if ((c >= L'A') && (c <= L'F'))
        {
            return c - L'A' + 10;
        }
        else if ((c >= L'a') && (c <= L'f'))
        {
            return c - L'a' + 10;
        }
        else
        {
            return -1;
        }
    }
}

inline HRESULT parse_hexstring(PCWSTR string, size_t bytesSize, BYTE* bytes)
{
    const size_t stringLength{ wcslen(string) };
    RETURN_HR_IF(E_INVALIDARG, stringLength != bytesSize * 2);

    for (size_t index=0; index < stringLength; ++index)
    {
        const wchar_t c1{ string[index] };
        const auto b1{ details::hexdigit_to_byte(c1) };
        RETURN_HR_IF(E_INVALIDARG, b1 < 0);

        const wchar_t c2{ string[++index] };
        const auto b2{ details::hexdigit_to_byte(c2) };
        RETURN_HR_IF(E_INVALIDARG, b2 < 0);

        const BYTE value{ static_cast<BYTE>((b1 << 4) | b2) };
        *bytes++ = value;
    }
    return S_OK;
}
}
