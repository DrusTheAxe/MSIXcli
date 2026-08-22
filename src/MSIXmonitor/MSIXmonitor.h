// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information

#pragma once

class MSIXmonitor
{
public:
    MSIXmonitor() = default;
    ~MSIXmonitor() = default;

    MSIXmonitor(const MSIXmonitor&) = delete;
    MSIXmonitor& operator=(const MSIXmonitor&) = delete;

public:
    static void ShowError(const HRESULT hr, HWND window);

    static void ShowErrorIfFailed(const HRESULT hr, HWND window)
    {
        if (FAILED(hr))
        {
            ShowError(hr, window);
        }
    }

    void ShowErrorIfFailed(const HRESULT hr) const
    {
        if (FAILED(hr))
        {
            ShowError(hr, m_window);
        }
    }

    HRESULT RunMessageLoop(int& exitCode);

    void Close();

    HRESULT InitializeApplication(HINSTANCE instance);

private:
    HRESULT RegisterApplicationWindowClass();

private:
    static constexpr UINT WM_MSIXMONITOR_TRAY{ WM_APP + 1 };
    static constexpr UINT TRAY_ICON_ID{ 1 };
    static constexpr UINT MAX_ACTIVITIES{ 256 };
    static constexpr PCWSTR WINDOW_CLASS_NAME{ L"MSIXmonitor.HiddenWindow" };

private:
    struct Activity
    {
        WCHAR dateTime[32];
        WCHAR package[128];
        WCHAR action[64];
        WCHAR message[256];
    };

    HRESULT AddActivity(PCWSTR package, PCWSTR action, PCWSTR message);
    HRESULT InsertActivityColumn(HWND list, int index, PCWSTR title, int width);
    HRESULT SetActivityItemText(HWND list, int item, int subItem, PWSTR text);
    HRESULT InitializeActivityList(HWND dialog);

    static INT_PTR CALLBACK ActivityDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM);

private:
    static INT_PTR CALLBACK AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM);

private:
    bool IsActivityMonitorEnabled() const;
    HRESULT EnableActivityMonitor(bool isEnabled);
    HRESULT ToggleActivityMonitor();
    HRESULT UpdateActivityMonitorMenu(HMENU menu) const;

    HRESULT ShowTrayMenu(HWND window);
    void RemoveTrayIcon(HWND window);
    HRESULT AddTrayIcon(HWND window);

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HRESULT ShowDialog(UINT resourceId, DLGPROC dialogProc)
    {
        RETURN_LAST_ERROR_IF(DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(resourceId), m_window, dialogProc, 0) == -1);
        return S_OK;
    }

private:
    HINSTANCE m_hInstance{};
    HWND m_window{};
    UINT m_taskbarCreatedMessage{};
    bool m_trayIconAdded{};
    bool m_activityMonitorEnabled{};
    wil::unique_hicon m_trayIcon;
    wil::unique_hfont m_aboutNameFont;
    wil::unique_hfont m_aboutDescriptionFont;

    Activity m_activities[MAX_ACTIVITIES]{};
    UINT m_activityStart{};
    UINT m_activityCount{};
};
