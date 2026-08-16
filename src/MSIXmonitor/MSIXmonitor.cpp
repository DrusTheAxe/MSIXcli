// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "MSIXmonitor.h"
#include "resource.h"

MSIXmonitor g_monitor;

void MSIXmonitor::ShowError(const HRESULT hr, HWND window)
{
    wil::unique_process_heap_string caption;
    static_cast<void>(LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(caption, L"MSIX Monitor: Error 0x%08X", hr)));

    wil::unique_process_heap_string text;
    PCWSTR formatter{ L"Error 0x%08X\n\n%ls" };
    const auto message{ wil::format_message_nothrow(hr) };
    static_cast<void>(LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(text, formatter, hr, message ? message.get() : L"<null>")));
    MessageBoxW(window, text ? text.get() : L"<null>", caption ? caption.get() : L"MSIX Monitor", MB_OK | MB_ICONERROR);
}

HRESULT MSIXmonitor::RunMessageLoop(int& exitCode)
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

void MSIXmonitor::Close()
{
    if (m_window && IsWindow(m_window))
    {
        DestroyWindow(m_window);
    }
}

HRESULT MSIXmonitor::InitializeApplication(HINSTANCE instance)
{
    m_hInstance = instance;

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    RETURN_IF_WIN32_BOOL_FALSE(InitCommonControlsEx(&controls));

    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    RETURN_LAST_ERROR_IF(m_taskbarCreatedMessage == 0);

    HICON icon{ static_cast<HICON>(LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_MSIXMONITOR), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR)) };
    RETURN_LAST_ERROR_IF_NULL(icon);
    m_trayIcon.reset(icon);

    RETURN_IF_FAILED(RegisterApplicationWindowClass());

    m_window = CreateWindowExW(0, WINDOW_CLASS_NAME, L"MSIXmonitor", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, m_hInstance, nullptr);
    RETURN_LAST_ERROR_IF_NULL(m_window);

    RETURN_IF_FAILED(AddActivity(L"", L"Started", L"MSIXmonitor started."));
    RETURN_IF_FAILED(AddTrayIcon(m_window));
    return S_OK;
}

HRESULT MSIXmonitor::RegisterApplicationWindowClass()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = m_hInstance;
    windowClass.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_MSIXMONITOR));
    windowClass.hIconSm = m_trayIcon.get();
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WINDOW_CLASS_NAME;

    RETURN_LAST_ERROR_IF(RegisterClassExW(&windowClass) == 0);
    return S_OK;
}

HRESULT MSIXmonitor::AddActivity(PCWSTR package, PCWSTR action, PCWSTR message)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, package);
    RETURN_HR_IF_NULL(E_INVALIDARG, action);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);

    UINT entryIndex{};
    if (m_activityCount < ARRAYSIZE(m_activities))
    {
        entryIndex = (m_activityStart + m_activityCount) % ARRAYSIZE(m_activities);
        ++m_activityCount;
    }
    else
    {
        entryIndex = m_activityStart;
        m_activityStart = (m_activityStart + 1) % ARRAYSIZE(m_activities);
    }

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    Activity& activity{ m_activities[entryIndex] };
    RETURN_IF_FAILED(StringCchPrintfW(
        activity.dateTime,
        ARRAYSIZE(activity.dateTime),
        L"%04u-%02u-%02u %02u:%02u:%02u",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute,
        localTime.wSecond));
    RETURN_IF_FAILED(StringCchCopyW(activity.package, ARRAYSIZE(activity.package), package));
    RETURN_IF_FAILED(StringCchCopyW(activity.action, ARRAYSIZE(activity.action), action));
    RETURN_IF_FAILED(StringCchCopyW(activity.message, ARRAYSIZE(activity.message), message));

    return S_OK;
}

HRESULT MSIXmonitor::InsertActivityColumn(HWND list, int index, PCWSTR title, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<PWSTR>(title);
    column.cx = width;
    column.iSubItem = index;

    RETURN_HR_IF(E_UNEXPECTED, ListView_InsertColumn(list, index, &column) == -1);
    return S_OK;
}

HRESULT MSIXmonitor::SetActivityItemText(HWND list, int item, int subItem, PWSTR text)
{
    LVITEMW listItem{};
    listItem.iSubItem = subItem;
    listItem.pszText = text;
    RETURN_HR_IF(E_UNEXPECTED, !SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&listItem)));
    return S_OK;
}

HRESULT MSIXmonitor::InitializeActivityList(HWND dialog)
{
    HWND list{ GetDlgItem(dialog, IDC_ACTIVITY_LIST) };
    RETURN_HR_IF_NULL(E_UNEXPECTED, list);

    ListView_SetExtendedListViewStyle(list, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

    RETURN_IF_FAILED(InsertActivityColumn(list, 0, L"DateTime", 145));
    RETURN_IF_FAILED(InsertActivityColumn(list, 1, L"Package", 190));
    RETURN_IF_FAILED(InsertActivityColumn(list, 2, L"Action", 100));
    RETURN_IF_FAILED(InsertActivityColumn(list, 3, L"Message", 360));

    for (UINT index = 0; index < m_activityCount; ++index)
    {
        Activity& activity{ m_activities[(m_activityStart + index) % ARRAYSIZE(m_activities)] };
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(index);
        item.pszText = activity.dateTime;

        const int itemIndex{ ListView_InsertItem(list, &item) };
        RETURN_HR_IF(E_UNEXPECTED, itemIndex == -1);
        RETURN_IF_FAILED(SetActivityItemText(list, itemIndex, 1, activity.package));
        RETURN_IF_FAILED(SetActivityItemText(list, itemIndex, 2, activity.action));
        RETURN_IF_FAILED(SetActivityItemText(list, itemIndex, 3, activity.message));
    }

    return S_OK;
}

INT_PTR CALLBACK MSIXmonitor::ActivityDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            const auto hr{ LOG_IF_FAILED(g_monitor.InitializeActivityList(dialog)) };
            if (FAILED(hr))
            {
                ShowError(hr, dialog);
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

INT_PTR CALLBACK MSIXmonitor::AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            HWND name{ GetDlgItem(dialog, IDC_ABOUT_NAME) };
            RETURN_HR_IF_NULL(E_UNEXPECTED, name);
            HWND description{ GetDlgItem(dialog, IDC_ABOUT_DESCRIPTION) };
            RETURN_HR_IF_NULL(E_UNEXPECTED, description);

            HFONT dialogFont{ reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0)) };
            RETURN_HR_IF_NULL(E_UNEXPECTED, dialogFont);

            LOGFONTW logFont{};
            RETURN_HR_IF(E_UNEXPECTED, GetObjectW(dialogFont, sizeof(logFont), &logFont) != sizeof(logFont));
            logFont.lfHeight = MulDiv(logFont.lfHeight, 16, 9);
            logFont.lfWeight = FW_BOLD;

            g_monitor.m_aboutNameFont.reset(CreateFontIndirectW(&logFont));
            RETURN_LAST_ERROR_IF_NULL(g_monitor.m_aboutNameFont);
            SendMessageW(name, WM_SETFONT, reinterpret_cast<WPARAM>(g_monitor.m_aboutNameFont.get()), TRUE);

            logFont.lfHeight = MulDiv(logFont.lfHeight, 12, 14);
            logFont.lfWeight = FW_SEMIBOLD;
            g_monitor.m_aboutDescriptionFont.reset(CreateFontIndirectW(&logFont));
            RETURN_LAST_ERROR_IF_NULL(g_monitor.m_aboutDescriptionFont);
            SendMessageW(description, WM_SETFONT, reinterpret_cast<WPARAM>(g_monitor.m_aboutDescriptionFont.get()), TRUE);

            std::uint16_t major{};
            std::uint16_t minor{};
            std::uint16_t build{};
            std::uint16_t revision{};
            RETURN_IF_FAILED(wil::get_module_version(g_monitor.m_hInstance, major, minor, build, revision));
            wil::unique_process_heap_string text;
            if (revision != 0)
            {
                PCWSTR format{ L"MSIXmonitor v%hu.%hu.%hu.%hu" };
                RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(text, format, major, minor, build, revision));
            }
            else
            {
                PCWSTR format{ L"MSIXmonitor v%hu.%hu.%hu" };
                RETURN_IF_FAILED(wil::str_printf_nothrow<wil::unique_process_heap_string>(text, format, major, minor, build));
            }
            SetDlgItemText(dialog, IDC_ABOUT_NAME, text.get());
            return TRUE;
        }
        case WM_COMMAND:
        {
            if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
            {
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;
            }
            break;
        }
        case WM_NOTIFY:
        {
            const NMHDR* notification{ reinterpret_cast<const NMHDR*>(lParam) };
            if ((notification->idFrom == IDC_HOME) &&
                ((notification->code == NM_CLICK) || (notification->code == NM_RETURN)))
            {
                ::ShellExecuteW(dialog, L"open", L"https://github.com/drustheaxe/msixcli", nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
        {
            g_monitor.m_aboutNameFont.reset();
            g_monitor.m_aboutDescriptionFont.reset();
            break;
        }
        case WM_CLOSE:
        {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

HRESULT MSIXmonitor::ShowTrayMenu(HWND window)
{
    wil::unique_hmenu menu{ LoadMenuW(m_hInstance, MAKEINTRESOURCEW(IDR_TRAY_MENU)) };
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
            RETURN_IF_FAILED(AddActivity(L"", L"About", L"About dialog opened."));
            RETURN_IF_FAILED(ShowDialog(IDD_ABOUT, AboutDialogProc));
            break;

        case IDM_TRAY_ACTIVITY:
            RETURN_IF_FAILED(AddActivity(L"", L"Log", L"Log dialog opened."));
            RETURN_IF_FAILED(ShowDialog(IDD_ACTIVITY, ActivityDialogProc));
            break;

        case IDM_TRAY_EXIT:
            RETURN_IF_WIN32_BOOL_FALSE(DestroyWindow(window));
            return S_OK;
    }

    RETURN_IF_WIN32_BOOL_FALSE(PostMessageW(window, WM_NULL, 0, 0));
    return S_OK;
}

void MSIXmonitor::RemoveTrayIcon(HWND window)
{
    if (m_trayIconAdded)
    {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window;
        data.uID = TRAY_ICON_ID;
        Shell_NotifyIconW(NIM_DELETE, &data);
        m_trayIconAdded = false;
    }
}

HRESULT MSIXmonitor::AddTrayIcon(HWND window)
{
    RemoveTrayIcon(window);

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = TRAY_ICON_ID;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_MSIXMONITOR_TRAY;
    data.hIcon = m_trayIcon.get();
    RETURN_IF_FAILED(StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), L"MSIXmonitor"));
    RETURN_HR_IF(E_UNEXPECTED, !Shell_NotifyIconW(NIM_ADD, &data));
    m_trayIconAdded = true;
    data.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &data))
    {
        RemoveTrayIcon(window);
        RETURN_HR(PEERDIST_ERROR_VERSION_UNSUPPORTED);
    }
    return S_OK;
}

LRESULT CALLBACK MSIXmonitor::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((g_monitor.m_taskbarCreatedMessage != 0) && (message == g_monitor.m_taskbarCreatedMessage))
    {
        ShowErrorIfFailed(LOG_IF_FAILED(g_monitor.AddTrayIcon(window)), window);
        return 0;
    }

    switch (message)
    {
        case WM_MSIXMONITOR_TRAY:
        {
            if (LOWORD(lParam) == WM_CONTEXTMENU)
            {
                ShowErrorIfFailed(LOG_IF_FAILED(g_monitor.ShowTrayMenu(window)), window);
            }
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(window);
            return 0;
        }
        case WM_DESTROY:
        {
            g_monitor.RemoveTrayIcon(window);
            g_monitor.m_window = nullptr;
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow)
{
    wil::unique_mutex_nothrow instanceMutex{ ::CreateMutexW(nullptr, FALSE, L"MSIXmonitor.is.singleton") };
    RETURN_LAST_ERROR_IF_NULL(instanceMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return S_OK;
    }

    auto com_init{ wil::CoInitializeEx_failfast() };

    int exitCode{};

    HRESULT hr{ LOG_IF_FAILED(g_monitor.InitializeApplication(hInstance)) };
    if (SUCCEEDED(hr))
    {
        hr = LOG_IF_FAILED(g_monitor.RunMessageLoop(exitCode));
    }

    g_monitor.ShowErrorIfFailed(hr);

    g_monitor.Close();

    RETURN_IF_FAILED(hr);
    return exitCode;
}
