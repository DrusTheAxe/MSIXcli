// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/token_helpers.h>

#include <shlwapi.h>

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


inline bool string_is_null_or_empty(PCWSTR string)
{
    return !string || (*string == L'\0');
}

inline HRESULT glob(PCWSTR string, PCWSTR pattern, bool& match)
{
    match = false;

    // PathMatchSpecEx() isn't technically GLOB, but given strings like PackageFamilyName
    // content restrictions its wildcard matching fits the data correctly.
    //
    // We'll use our poor-man's GLOB, as the alternatives (std::regex, VBScript.RegExp, etc)
    // require C++ Exceptions or heavyweight dependencies.
    //
    // @see https://learn.microsoft.com/windows/apps/desktop/modernize/package-identity-overview#package-identity-fields-limits
    // @see https://learn.microsoft.com/windows/win32/api/shlwapi/nf-shlwapi-pathmatchspecexw
    const HRESULT hr{ ::PathMatchSpecEx(string, pattern, 0) };
    RETURN_IF_FAILED_MSG(hr, "glob('%ls', '%ls')", string, pattern);
    match = (hr == S_OK);
    return S_OK;
}

inline HRESULT to_hstring_reference(PCWSTR string, HSTRING_HEADER& hstringHeader, HSTRING& hstring)
{
    RETURN_IF_FAILED(::WindowsCreateStringReference(string ? string : L"<null>", static_cast<std::uint32_t>(wcslen(string)), &hstringHeader, &hstring));
    return S_OK;
}

namespace details
{
    inline bool is_digit(const wchar_t c)
    {
        return (c >= L'0') && (c <= L'9');
    }

    inline bool is_alpha_upper(const wchar_t c)
    {
        return (c >= L'A') && (c <= L'Z');
    }

    inline bool is_alpha_lower(const wchar_t c)
    {
        return (c >= L'a') && (c <= L'z');
    }

    inline bool is_alpha(const wchar_t c)
    {
        return is_alpha_upper(c) || is_alpha_lower(c);
    }

    inline int hexdigit_to_byte(const wchar_t c)
    {
        if (isdigit(c))
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

inline bool is_url(PCWSTR url)
{
    // https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Syntax
    // A non-empty scheme component followed by a colon (:), consisting of a sequence of characters
    // beginning with a letter and followed by any combination of letters, digits, plus (+), period (.),
    // or hyphen (-). Although schemes are case-insensitive, the canonical form is lowercase and
    // documents that specify schemes must do so with lowercase letters.

    if (!details::is_alpha(*url))
    {
        return false;
    }

    for (++url; *url; ++url)
    {
        const auto c{ *url };
        if (c == L':')
        {
            return true;
        }
        else if (details::is_digit(c) || details::is_alpha(c) || (c == L'+') || (c == L'.') || (c == L'-'))
        {
            continue;
        }
        else
        {
            break;
        }
    }

    return false;
}

#if (defined(_INC_SHIDFACT) && !defined(__WIL_SHIDFACT_H__)) || defined(WIL_DOXYGEN)
#define __WIL_SHIDFACT_H__
inline HRESULT set_property_store_value(IPropertyStore* propertyStore, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    RETURN_IF_FAILED(propertyStore->SetValue(key, value));
    RETURN_IF_FAILED(propertyStore->Commit());
    return S_OK;
}

inline HRESULT set_property_store_value(wil::com_ptr_nothrow<IPropertyStore>& propertyStore, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    return set_property_store_value(propertyStore.get(), key, value);
}

inline HRESULT set_property_store_value(wil::com_ptr_nothrow<IPropertyStore>& propertyStore, REFPROPERTYKEY key, PCWSTR value)
{
    wil::unique_prop_variant pv;
    RETURN_IF_FAILED(InitPropVariantFromString(value, &pv));
    return set_property_store_value(propertyStore.get(), key, pv);
}

inline HRESULT set_property_store_value(wil::com_ptr_nothrow<IPropertyStore>& propertyStore, REFPROPERTYKEY key, std::int32_t value)
{
    wil::unique_prop_variant pv;
    RETURN_IF_FAILED(InitPropVariantFromInt32(value, &pv));
    return set_property_store_value(propertyStore.get(), key, pv);
}
#endif // (defined(_INC_SHIDFACT) && !defined(__WIL_SHIDFACT_H__)) || defined(WIL_DOXYGEN)

inline HRESULT exe_path(wil::unique_process_heap_string& path)
{
    RETURN_IF_FAILED(wil::GetModuleFileNameW(nullptr, path));
    PCWSTR lastPathSegment{ wil::find_last_path_segment(path.get()) };
    const auto offset{ lastPathSegment - path.get() };
    path.get()[offset] = L'\0';
    return S_OK;
}

inline HRESULT get_module_version(HMODULE module, std::uint16_t& major, std::uint16_t& minor, std::uint16_t& build, std::uint16_t& revision)
{
    wil::unique_process_heap_string path;
    RETURN_IF_FAILED(wil::GetModuleFileNameW(module, path));

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

inline HRESULT get_exe_version(std::uint16_t& major, std::uint16_t& minor, std::uint16_t& build, std::uint16_t& revision)
{
    RETURN_IF_FAILED(get_module_version(nullptr, major, minor, build, revision));
    return S_OK;
}
inline HRESULT file_exists(PCWSTR filename, bool& isDirectory, bool& isFile)
{
    const auto attributes{ ::GetFileAttributesW(filename) };
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        isDirectory = false;
        isFile = false;
        RETURN_LAST_ERROR();
    }
    else if (WI_IsFlagSet(attributes, FILE_ATTRIBUTE_DIRECTORY))
    {
        isDirectory = true;
        isFile = false;
    }
    else
    {
        isDirectory = false;
        isFile = true;
    }
    return S_OK;
}


inline HRESULT file_exists(PCWSTR filename, bool& exists)
{
    const auto attributes{ ::GetFileAttributesW(filename) };
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        exists = false;
        RETURN_LAST_ERROR();
    }
    else
    {
        exists = true;
        return S_OK;
    }
}

inline HRESULT is_directory(PCWSTR filename, bool& exists)
{
    const auto attributes{ ::GetFileAttributesW(filename) };
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        exists = false;
        RETURN_LAST_ERROR();
    }
    else
    {
        exists = WI_IsFlagSet(attributes, FILE_ATTRIBUTE_DIRECTORY);
        return S_OK;
    }
}

inline HRESULT is_regular_file(PCWSTR filename, bool& exists)
{
    bool isDirectory{};
    RETURN_IF_FAILED(is_directory(filename, isDirectory));
    exists = !isDirectory;
    return S_OK;
}

inline HRESULT to_uri(
    PCWSTR string,
    ABI::Windows::Foundation::IUriRuntimeClassFactory* uriFactory,
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass>& uri)
{
    uri.reset();

    if (!uriFactory)
    {
        HSTRING_HEADER header{};
        HSTRING classId{};
        RETURN_IF_FAILED(WindowsCreateStringReference(RuntimeClass_Windows_Foundation_Uri,
            ARRAYSIZE(RuntimeClass_Windows_Foundation_Uri) - 1, &header, &classId));
        RETURN_IF_FAILED(RoGetActivationFactory(classId, IID_PPV_ARGS(&uriFactory)));
    }

    WCHAR stackUrl[MAX_PATH];
    PWSTR url{ stackUrl };
    DWORD cchUrl{ ARRAYSIZE(stackUrl) };
    HRESULT urlHr{ ::UrlCreateFromPathW(string, stackUrl, &cchUrl, 0) };

    wistd::unique_ptr<WCHAR[]> heapUrl;
    if (urlHr == E_POINTER)
    {
        heapUrl = wil::make_unique_nothrow<WCHAR[]>(cchUrl);
        RETURN_IF_NULL_ALLOC(heapUrl);
        url = heapUrl.get();
        urlHr = ::UrlCreateFromPathW(string, url, &cchUrl, 0);
    }
    RETURN_IF_FAILED(urlHr);

    HSTRING_HEADER header{};
    HSTRING uriRef{};
    RETURN_IF_FAILED(::WindowsCreateStringReference(url, cchUrl, &header, &uriRef));
    RETURN_IF_FAILED(uriFactory->CreateUri(uriRef, uri.put()));
    return S_OK;
}

inline HRESULT to_uri(
    PCWSTR string,
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass>& uri)
{
    RETURN_IF_FAILED(to_uri(string, nullptr, uri));
    return S_OK;
}

namespace ui
{
    inline void set_busy(HWND hwnd, bool busy)
    {
        ::SetPropW(hwnd, L"msixcli.busy", busy ? reinterpret_cast<HANDLE>(1) : nullptr);
    }

    inline bool is_busy(HWND hwnd)
    {
        return ::GetPropW(hwnd, L"msixcli.busy") != nullptr;
    }

    inline void refresh_cursor(HWND hwnd)
    {
        POINT pt{};
        GetCursorPos(&pt);

        HWND under{ WindowFromPoint(pt) };
        if (under == hwnd)
        {
            SendMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }
    }

    [[nodiscard]] inline auto scoped_wait_cursor(HWND hwnd)
    {
        set_busy(hwnd, true);
        refresh_cursor(hwnd);
        return wil::scope_exit([hwnd]() noexcept {
            set_busy(hwnd, false);
            refresh_cursor(hwnd);
        });
    }

    [[nodiscard]] inline auto scoped_wait_cursor()
    {
        HCURSOR previousCursor{ SetCursor(LoadCursorW(nullptr, IDC_WAIT)) };
        return wil::scope_exit([previousCursor]() noexcept { SetCursor(previousCursor); });
    }
}

namespace reg
{
    /**
     * @brief Opens a new HKEY to the specified path - see RegOpenKeyExW
     * @param key An open or well-known registry key
     * @param subKey The name of the registry subkey to be opened.
     *        If `nullptr`, then `key` is used without modification.
     * @param[out] hkey A reference to a wil::unique_hkey to receive the opened HKEY
     * @param access The requested access desired for the opened key
     * @return HRESULT error code indicating success or failure (does not throw C++ exceptions)
     */
    inline HRESULT try_open_unique_key_nothrow(
        HKEY key, _In_opt_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
    {
        const auto hr{ open_unique_key_nothrow(key, subKey, hkey, access) };
        if (is_registry_not_found(hr))
        {
            hkey.reset();
            return S_OK;
        }
        return hr;
    }
}

namespace security
{
inline HRESULT get_integrity_level(DWORD& integrityLevel, HANDLE token = nullptr)
{
    wil::unique_tokeninfo_ptr<TOKEN_MANDATORY_LABEL> tokenMandatoryLabel;
    RETURN_IF_FAILED(wil::get_token_information_nothrow<TOKEN_MANDATORY_LABEL>(
            tokenMandatoryLabel, !token ? GetCurrentThreadEffectiveToken() : token));
    auto sid{ (*tokenMandatoryLabel).Label.Sid };
    auto subAuthorityCount{ *::GetSidSubAuthorityCount(sid) - 1 };
    integrityLevel = *::GetSidSubAuthority(sid, static_cast<DWORD>(static_cast<UCHAR>(subAuthorityCount)));
    return S_OK;
}

inline bool is_integrity_level_elevated(const DWORD integrityLevel)
{
    return integrityLevel >= SECURITY_MANDATORY_HIGH_RID;
}

inline HRESULT is_elevated(bool& isElevated, HANDLE token = nullptr)
{
    isElevated = false;

    DWORD integrityLevel{};
    RETURN_IF_FAILED(get_integrity_level(integrityLevel, token));
    isElevated = is_integrity_level_elevated(integrityLevel);
    return S_OK;
}

inline PCWSTR integrity_level_to_string(const DWORD integrityLevel)
{
    switch (integrityLevel)
    {
        case SECURITY_MANDATORY_UNTRUSTED_RID:          return L"Untrusted";    // 0x00000000 Untrusted.
        case SECURITY_MANDATORY_LOW_RID:                return L"Low";          // 0x00001000 Low integrity.
        case SECURITY_MANDATORY_MEDIUM_RID:             return L"Medium";       // 0x00002000 Medium integrity.
        case SECURITY_MANDATORY_MEDIUM_PLUS_RID:        return L"MediumPlus";   // SECURITY_MANDATORY_MEDIUM_RID + 0x100 Medium high integrity.
        case SECURITY_MANDATORY_HIGH_RID:               return L"High";         // 0X00003000 High integrity.
        case SECURITY_MANDATORY_SYSTEM_RID:             return L"System";       // 0x00004000 System integrity.
        case SECURITY_MANDATORY_PROTECTED_PROCESS_RID:  return L"ProtectedProcess";
        default:                                        return L"???";
    }
}
}

namespace filetime
{
    inline HRESULT to_systemtime(const FILETIME fileTime, const bool toLocalTime, SYSTEMTIME& systemTime)
    {
        systemTime = SYSTEMTIME{};

        SYSTEMTIME st{};
        RETURN_IF_WIN32_BOOL_FALSE(::FileTimeToSystemTime(&fileTime, &st));
        if (toLocalTime)
        {
            RETURN_IF_WIN32_BOOL_FALSE(::TzSpecificLocalTimeToSystemTime(nullptr, &st, &systemTime));
        }
        else
        {
            systemTime = st;
        }
        return S_OK;
    }

    inline HRESULT format_filetime(FILETIME fileTime, bool toLocalTime, wil::unique_cotaskmem_string& formatted)
    {
        formatted.reset();

        SYSTEMTIME systemTime{};
        const auto hr{ LOG_IF_FAILED(wil::filetime::to_systemtime(fileTime, toLocalTime, systemTime)) };
        if (FAILED(hr))
        {
            formatted = wil::make_cotaskmem_string_nothrow(L"????/??/?? ??:??:??");
            RETURN_HR(hr);
        }
        WCHAR date[128]{};
        const auto rcDate{ ::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &systemTime, L"yyyy'/'MM'/'dd", date, ARRAYSIZE(date), nullptr) };
        if ((wil::filetime::to_int64(fileTime) % wil::filetime_duration::one_day) == 0)
        {
            formatted = wil::make_cotaskmem_string_nothrow(rcDate ? date : L"????/??/??");
            RETURN_IF_NULL_ALLOC(formatted);
            return S_OK;
        }
        WCHAR time[128]{};
        const auto rcTime{ ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &systemTime, L"HH':'mm':'ss", time, ARRAYSIZE(time)) };
        RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(formatted, L"%ls %ls", rcDate ? date : L"????/??/??", rcTime ? time : L"??:??:??"));
        return S_OK;
    }
}
}
