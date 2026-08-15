// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "resource.h"

namespace
{
constexpr UINT WM_MSIXMONITOR_TRAY{ WM_APP + 1 };
constexpr UINT TRAY_ICON_ID{ 1 };
constexpr UINT MAX_LOG_ENTRIES{ 256 };

constexpr PCWSTR WINDOW_CLASS_NAME{ L"MSIXmonitor.HiddenWindow" };

struct LogEntry
{
    WCHAR dateTime[32];
    WCHAR package[128];
    WCHAR action[64];
    WCHAR message[256];
};

HINSTANCE g_instance{};
HWND g_window{};
UINT g_taskbarCreatedMessage{};
bool g_trayIconAdded{};
wil::unique_hicon g_trayIcon;
LogEntry g_logEntries[MAX_LOG_ENTRIES]{};
UINT g_logEntryStart{};
UINT g_logEntryCount{};

HRESULT AddTrayIcon(HWND window);

HRESULT GetFailureFromLastError()
{
    const DWORD error{ GetLastError() };
    return HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error);
}

void ShowError(HWND owner, HRESULT hr)
{
    wil::unique_process_heap_string caption;
    static_cast<void>(LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(caption, L"MSIX Monitor: Error 0x%08X", hr)));

    wil::unique_process_heap_string text;
    PCWSTR formatter{ L"Error 0x%08X\n\n%ls" };
    const auto message{ wil::format_message_nothrow(hr) };
    static_cast<void>(LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(text, formatter, hr, message ? message.get() : L"<null>")));
    MessageBoxW(owner, text ? text.get() : L"<null>", caption ? caption.get() : L"MSIX Monitor", MB_OK | MB_ICONERROR);
}

HRESULT AddLogEntry(PCWSTR package, PCWSTR action, PCWSTR message)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, package);
    RETURN_HR_IF_NULL(E_INVALIDARG, action);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);

    UINT entryIndex{};
    if (g_logEntryCount < ARRAYSIZE(g_logEntries))
    {
        entryIndex = (g_logEntryStart + g_logEntryCount) % ARRAYSIZE(g_logEntries);
        ++g_logEntryCount;
    }
    else
    {
        entryIndex = g_logEntryStart;
        g_logEntryStart = (g_logEntryStart + 1) % ARRAYSIZE(g_logEntries);
    }

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    LogEntry& entry{ g_logEntries[entryIndex] };
    RETURN_IF_FAILED(StringCchPrintfW(
        entry.dateTime,
        ARRAYSIZE(entry.dateTime),
        L"%04u-%02u-%02u %02u:%02u:%02u",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute,
        localTime.wSecond));
    RETURN_IF_FAILED(StringCchCopyW(entry.package, ARRAYSIZE(entry.package), package));
    RETURN_IF_FAILED(StringCchCopyW(entry.action, ARRAYSIZE(entry.action), action));
    RETURN_IF_FAILED(StringCchCopyW(entry.message, ARRAYSIZE(entry.message), message));

    return S_OK;
}

HRESULT InsertLogColumn(HWND list, int index, PCWSTR title, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<PWSTR>(title);
    column.cx = width;
    column.iSubItem = index;

    RETURN_HR_IF(E_FAIL, ListView_InsertColumn(list, index, &column) == -1);
    return S_OK;
}

HRESULT SetLogItemText(HWND list, int item, int subItem, PWSTR text)
{
    LVITEMW listItem{};
    listItem.iSubItem = subItem;
    listItem.pszText = text;
    RETURN_HR_IF(E_FAIL, !SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&listItem)));
    return S_OK;
}

HRESULT InitializeLogList(HWND dialog)
{
    HWND list{ GetDlgItem(dialog, IDC_LOG_LIST) };
    RETURN_HR_IF_NULL(E_UNEXPECTED, list);

    ListView_SetExtendedListViewStyle(
        list,
        LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

    RETURN_IF_FAILED(InsertLogColumn(list, 0, L"DateTime", 145));
    RETURN_IF_FAILED(InsertLogColumn(list, 1, L"Package", 190));
    RETURN_IF_FAILED(InsertLogColumn(list, 2, L"Action", 100));
    RETURN_IF_FAILED(InsertLogColumn(list, 3, L"Message", 360));

    for (UINT index = 0; index < g_logEntryCount; ++index)
    {
        LogEntry& entry{ g_logEntries[(g_logEntryStart + index) % ARRAYSIZE(g_logEntries)] };
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(index);
        item.pszText = entry.dateTime;

        const int itemIndex{ ListView_InsertItem(list, &item) };
        RETURN_HR_IF(E_FAIL, itemIndex == -1);
        RETURN_IF_FAILED(SetLogItemText(list, itemIndex, 1, entry.package));
        RETURN_IF_FAILED(SetLogItemText(list, itemIndex, 2, entry.action));
        RETURN_IF_FAILED(SetLogItemText(list, itemIndex, 3, entry.message));
    }

    return S_OK;
}

INT_PTR CALLBACK AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM)
{
    switch (message)
    {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
            {
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK LogDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            const HRESULT result{ InitializeLogList(dialog) };
            if (FAILED(result))
            {
                ShowError(dialog, result);
                EndDialog(dialog, IDCANCEL);
            }
            return TRUE;
        }

        case WM_COMMAND:
            if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
            {
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

HRESULT ShowDialog(UINT resourceId, DLGPROC dialogProc)
{
    RETURN_LAST_ERROR_IF(DialogBoxParamW(g_instance, MAKEINTRESOURCEW(resourceId), g_window, dialogProc, 0) == -1);
    return S_OK;
}

HRESULT ShowTrayMenu(HWND window)
{
    wil::unique_hmenu menu{ LoadMenuW(g_instance, MAKEINTRESOURCEW(IDR_TRAY_MENU)) };
    RETURN_LAST_ERROR_IF_NULL(menu);

    HMENU popup{ GetSubMenu(menu.get(), 0) };
    RETURN_LAST_ERROR_IF_NULL(popup);

    POINT position{};
    RETURN_IF_WIN32_BOOL_FALSE(GetCursorPos(&position));
    static_cast<void>(SetForegroundWindow(window));

    SetLastError(ERROR_SUCCESS);
    const UINT command{ static_cast<UINT>(TrackPopupMenuEx(
        popup,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        position.x,
        position.y,
        window,
        nullptr)) };
    if (command == 0)
    {
        const DWORD error{ GetLastError() };
        RETURN_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_SUCCESS);
        return S_OK;
    }

    switch (command)
    {
        case IDM_TRAY_ABOUT:
            RETURN_IF_FAILED(AddLogEntry(L"", L"About", L"About dialog opened."));
            RETURN_IF_FAILED(ShowDialog(IDD_ABOUT, AboutDialogProc));
            break;

        case IDM_TRAY_LOG:
            RETURN_IF_FAILED(AddLogEntry(L"", L"Log", L"Log dialog opened."));
            RETURN_IF_FAILED(ShowDialog(IDD_LOG, LogDialogProc));
            break;

        case IDM_TRAY_EXIT:
            RETURN_IF_WIN32_BOOL_FALSE(DestroyWindow(window));
            return S_OK;
    }

    RETURN_IF_WIN32_BOOL_FALSE(PostMessageW(window, WM_NULL, 0, 0));
    return S_OK;
}

void RemoveTrayIcon(HWND window)
{
    if (g_trayIconAdded)
    {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window;
        data.uID = TRAY_ICON_ID;
        Shell_NotifyIconW(NIM_DELETE, &data);
        g_trayIconAdded = false;
    }
}

HRESULT AddTrayIcon(HWND window)
{
    RemoveTrayIcon(window);

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = TRAY_ICON_ID;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_MSIXMONITOR_TRAY;
    data.hIcon = g_trayIcon.get();
    RETURN_IF_FAILED(StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), L"MSIXmonitor"));
    RETURN_HR_IF(E_FAIL, !Shell_NotifyIconW(NIM_ADD, &data));

    g_trayIconAdded = true;
    data.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &data))
    {
        RemoveTrayIcon(window);
        return E_FAIL;
    }

    return S_OK;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((g_taskbarCreatedMessage != 0) && (message == g_taskbarCreatedMessage))
    {
        const HRESULT result{ AddTrayIcon(window) };
        if (FAILED(result))
        {
            ShowError(window, result);
        }
        return 0;
    }

    switch (message)
    {
        case WM_MSIXMONITOR_TRAY:
            if ((LOWORD(lParam) == WM_CONTEXTMENU) || (LOWORD(lParam) == WM_RBUTTONUP))
            {
                const HRESULT result{ ShowTrayMenu(window) };
                if (FAILED(result))
                {
                    ShowError(window, result);
                }
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon(window);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

HRESULT RegisterApplicationWindowClass()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = g_instance;
    windowClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_MSIXMONITOR));
    windowClass.hIconSm = g_trayIcon.get();
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WINDOW_CLASS_NAME;

    RETURN_LAST_ERROR_IF(RegisterClassExW(&windowClass) == 0);
    return S_OK;
}

HRESULT InitializeApplication()
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    RETURN_IF_WIN32_BOOL_FALSE(InitCommonControlsEx(&controls));

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    RETURN_LAST_ERROR_IF(g_taskbarCreatedMessage == 0);

    HICON icon{ static_cast<HICON>(LoadImageW(g_instance, MAKEINTRESOURCEW(IDI_MSIXMONITOR), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR)) };
    RETURN_LAST_ERROR_IF_NULL(icon);
    g_trayIcon.reset(icon);

    RETURN_IF_FAILED(RegisterApplicationWindowClass());

    g_window = CreateWindowExW(0, WINDOW_CLASS_NAME, L"MSIXmonitor", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, g_instance, nullptr);
    RETURN_LAST_ERROR_IF_NULL(g_window);

    RETURN_IF_FAILED(AddLogEntry(L"", L"Started", L"MSIXmonitor started."));
    RETURN_IF_FAILED(AddTrayIcon(g_window));
    return S_OK;
}

HRESULT RunMessageLoop(int& exitCode)
{
    MSG message{};
    while (true)
    {
        const BOOL result{ GetMessageW(&message, nullptr, 0, 0) };
        RETURN_LAST_ERROR_IF(result == -1);
        if (result == 0)
        {
            exitCode = static_cast<int>(message.wParam);
            return S_OK;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
}

int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow)
{
    g_instance = hInstance;

    HRESULT result{ InitializeApplication() };
    int exitCode{};
    if (SUCCEEDED(result))
    {
        result = RunMessageLoop(exitCode);
    }

    if (FAILED(result))
    {
        ShowError(g_window, result);
        exitCode = static_cast<int>(result);
    }

    if (g_window && IsWindow(g_window))
    {
        DestroyWindow(g_window);
    }

    return exitCode;
}
