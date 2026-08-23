// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "MSIXPropertyPage.h"
#include "resource.h"

extern HINSTANCE g_hInstance;

namespace
{
    // EDIT controls that should look like static value text on the page
    // background. Style 0x50010080 (ES_AUTOHSCROLL, NOT WS_BORDER) plus
    // EM_SETREADONLY applied at runtime, matching Explorer's General-tab
    // Location field. We also paint their background with the dialog face
    // brush so they don't show up as a white editable rectangle.
    constexpr int c_readOnlyEdits[]
    {
        IDC_PACKAGE_FULL_NAME,
        IDC_PACKAGE_FAMILY_NAME,
        IDC_PACKAGEORIGIN,
        IDC_SIGNATURE_ORIGIN,
        IDC_FILE_COMPRESSED_SIZE,
        IDC_FILE_UNCOMPRESSED_SIZE,
        IDC_FOOTPRINT_COMPRESSED_SIZE,
        IDC_FOOTPRINT_UNCOMPRESSED_SIZE,
        IDC_PAYLOAD_COMPRESSED_SIZE,
        IDC_PAYLOAD_UNCOMPRESSED_SIZE
    };

    // EDIT controls that should look like static value text on the page
    // background. Style 0x50010080 (ES_AUTOHSCROLL, NOT WS_BORDER) plus
    // EM_SETREADONLY applied at runtime, matching Explorer's General-tab
    // Location field. We also paint their background with the dialog face
    // brush so they don't show up as a white editable rectangle.
    constexpr int c_certificateReadOnlyEdits[]
    {
        IDC_PACKAGE_FULL_NAME,
        IDC_PACKAGE_FAMILY_NAME,
        IDC_SIGNATURE_ORIGIN,
        IDC_CERTIFICATE_SUBJECT,
        IDC_CERTIFICATE_ISSUER,
        IDC_CERTIFICATE_THUMBPRINT,
        IDC_CERTIFICATE_VALID,
        IDC_CERTIFICATE_STATUS
    };

    bool IsBorderlessReadOnlyEdit(int ctlId, const int* controlIds, size_t controlIdsCount) noexcept
    {
        for (size_t index = 0; index < controlIdsCount; ++index)
        {
            const int id{ controlIds[index] };
            if (id == ctlId)
            {
                return true;
            }
        }
        return false;
    }

    bool IsBorderlessReadOnlyEdit(int ctlId) noexcept
    {
        return IsBorderlessReadOnlyEdit(ctlId, c_readOnlyEdits, ARRAYSIZE(c_readOnlyEdits));
    }

    bool IsCertificateBorderlessReadOnlyEdit(int ctlId) noexcept
    {
        return IsBorderlessReadOnlyEdit(ctlId, c_certificateReadOnlyEdits, ARRAYSIZE(c_certificateReadOnlyEdits));
    }

    // Status-label colors. These follow the widely recognized spreadsheet
    // conditional-formatting palette: a green "good" style for the healthy/ok
    // state and a yellow "neutral" style for warnings, each paired with a
    // darker matching foreground for legible text.
    constexpr COLORREF c_statusOkBackground{ RGB(198, 239, 206) };
    constexpr COLORREF c_statusOkText{ RGB(0, 97, 0) };
    constexpr COLORREF c_statusWarningBackground{ RGB(255, 235, 156) };
    constexpr COLORREF c_statusWarningText{ RGB(156, 101, 0) };
    constexpr COLORREF c_statusErrorBackground{ RGB(255, 199, 206) };
    constexpr COLORREF c_statusErrorText{ RGB(156, 0, 6) };
}

MSIXPropertyPage::MSIXPropertyPage(wil::unique_process_heap_ptr<WCHAR[]> filePath) :
    m_filePath(std::move(filePath))
{
    HSTRING_HEADER classIdHeader{};
    HSTRING classId{};

    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        if (SUCCEEDED_LOG(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_PackageManager,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_PackageManager) - 1,
                &classIdHeader, &classId)) &&
            SUCCEEDED_LOG(RoActivateInstance(classId, inspectable.put())))
        {
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_packageManager2.put()));
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_packageManager3.put()));
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_packageManager9.put()));
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_packageManager12.put()));
        }
    }

    {
        wil::com_ptr_nothrow<IInspectable> inspectable;
        if (SUCCEEDED_LOG(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_AddPackageOptions,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_AddPackageOptions) - 1,
                &classIdHeader, &classId)) &&
            SUCCEEDED_LOG(RoActivateInstance(classId, inspectable.put())))
        {
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_addPackageOptions.put()));
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_addPackageOptions2.put()));
            std::ignore = LOG_IF_FAILED(inspectable.query_to(m_addPackageOptions3.put()));
        }
    }
}

MSIXPropertyPage::~MSIXPropertyPage()
{
}

HPROPSHEETPAGE MSIXPropertyPage::CreatePropertyPage()
{
    // Register the common control classes used by the dialog template (listview, etc.).
    // Idempotent and safe to call multiple times; pairs with the v6 common-controls
    // manifest embedded at resource ID 2 (ISOLATIONAWARE_MANIFEST_RESOURCE_ID).
    const INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_LINK_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    PROPSHEETPAGE psp{ 0 };
    psp.dwSize = sizeof(psp);
    psp.dwFlags = PSP_USEREFPARENT | PSP_USETITLE | PSP_USECALLBACK;
    psp.hInstance = g_hInstance;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_MSIX_PROPPAGE);
    psp.pfnDlgProc = DialogProc;
    psp.pszTitle = L"MSIX";
    psp.lParam = reinterpret_cast<LPARAM>(this);
    psp.pfnCallback = PropPageCallbackProc;
    psp.pcRefParent = nullptr;

    return CreatePropertySheetPage(&psp);
}

INT_PTR CALLBACK MSIXPropertyPage::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MSIXPropertyPage *pThis{};

    if (uMsg == WM_INITDIALOG)
    {
        LPPROPSHEETPAGE psp{ reinterpret_cast<LPPROPSHEETPAGE>(lParam) };
        pThis = reinterpret_cast<MSIXPropertyPage*>(psp->lParam);
        SetWindowLongPtr(hwndDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
        pThis->OnInitDialog(hwndDlg);
        return TRUE;
    }

    // WM_CTLCOLOR* doesn't depend on instance state and can arrive before
    // WM_INITDIALOG returns, so handle it outside the pThis guard.
    if (uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLOREDIT)
    {
        HWND hwndCtl{ reinterpret_cast<HWND>(lParam) };
        if (IsBorderlessReadOnlyEdit(GetDlgCtrlID(hwndCtl)))
        {
            // Draw these read-only edits exactly like the static value labels
            // beside them. Because they're read-only they send WM_CTLCOLORSTATIC,
            // so deferring to the default dialog handling makes them pick up the
            // same themed property-page (tab) background the labels use instead
            // of a darker, hard-coded COLOR_BTNFACE rectangle.
            return FALSE;
        }
    }

    pThis = reinterpret_cast<MSIXPropertyPage*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
    if (pThis)
    {
        switch (uMsg)
        {
            case WM_COMMAND:
            {
                pThis->OnCommand(hwndDlg, wParam, lParam);
                return TRUE;
            }
            case WM_DRAWITEM:
            {
                pThis->DrawStatusBadge(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
                return TRUE;
            }
            case WM_SETCURSOR:
            {
                // (HWND)wParam is the window under the cursor; LOWORD(lParam) is the hit-test
                if (wil::ui::is_busy(hwndDlg) && reinterpret_cast<HWND>(wParam) == hwndDlg)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                    SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break; // not busy -> default arrow
            }
            case WM_DESTROY:
            {
                pThis->OnDestroy(hwndDlg);
                return TRUE;
            }
            case WM_NOTIFY:
            {
                LPNMHDR pnmh{ reinterpret_cast<LPNMHDR>(lParam) };
                switch (pnmh->code)
                {
                    case PSN_APPLY:
                    {
                        SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                        return TRUE;
                    }
                    case NM_CLICK:
                        [[fallthrough]];
                    case NM_RETURN:
                    {
                        if (pnmh->idFrom == IDC_HOME)
                        {
                            ::ShellExecuteW(hwndDlg, L"open", L"https://github.com/drustheaxe/msixcli", nullptr, nullptr, SW_SHOWNORMAL);
                            return TRUE;
                        }
                        break;
                    }
                    case TTN_GETDISPINFO:
                    {
                        NMTTDISPINFO* pdi{ reinterpret_cast<NMTTDISPINFO*>(lParam) };
                        // With TTF_IDISHWND the tool's idFrom is the control's HWND;
                        // map it back to the control ID and load the matching tooltip
                        // string from the resource string table.
                        const UINT controlId{ static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(pdi->hdr.idFrom))) };
                        pdi->szText[0] = L'\0';
                        if (controlId == IDC_PACKAGE_FULL_NAME)
                        {
                            pdi->lpszText = const_cast<PWSTR>(pThis->m_package.PackageFullName());
                        }
                        else if (controlId == IDC_PACKAGE_FAMILY_NAME)
                        {
                            pdi->lpszText = const_cast<PWSTR>(pThis->m_package.PackageFamilyName());
                        }
                        else
                        {
                            LoadStringW(g_hInstance, controlId, pThis->m_tooltipText, ARRAYSIZE(pThis->m_tooltipText));
                            pdi->lpszText = pThis->m_tooltipText;
                        }
                    }
                    break;
                }
                break;
            }
        }
    }

    return FALSE;
}

UINT CALLBACK MSIXPropertyPage::PropPageCallbackProc(HWND /*hwnd*/, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
    if (uMsg == PSPCB_RELEASE)
    {
        MSIXPropertyPage *pThis{ reinterpret_cast<MSIXPropertyPage*>(ppsp->lParam) };
        delete pThis;
    }
    return 1;
}

void MSIXPropertyPage::ReleaseTargetVolumes(HWND hwndDlg)
{
    auto hwndVolume{ GetDlgItem(hwndDlg, IDC_VOLUME) };

    const auto count{ SendMessage(hwndVolume, CB_GETCOUNT, 0, 0) };
    for (int index = 0; index < count; ++index)
    {
        auto volume{ reinterpret_cast<ABI::Windows::Management::Deployment::IPackageVolume*>(SendMessage(hwndVolume, CB_GETITEMDATA, static_cast<WPARAM>(index), 0)) };
        if (volume)
        {
            volume->Release();
        }
    }
}

void MSIXPropertyPage::OnDestroy(HWND hwndDlg)
{
    // Release the PackageVolume objects associated with items in the IDC_VOLUME combobox
    ReleaseTargetVolumes(hwndDlg);

    // The tooltip control is a top-level WS_POPUP window, so it isn't destroyed
    // automatically with the dialog's child controls; tear it down explicitly.
    if (m_hwndTip)
    {
        DestroyWindow(m_hwndTip);
        m_hwndTip = nullptr;
    }
}

void MSIXPropertyPage::InitializeToolTips(HWND hwndDlg, HWND& hwndTip)
{
    // Initialize tooltip support: create a tooltip control owned by the dialog and
    // register every child control as a tool. The tooltip text is supplied on
    // demand via TTN_GETDISPINFO (see DialogProc), keyed by each control's ID.
    HWND hwndTipCtrl{ CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwndDlg, nullptr, g_hInstance, nullptr) };
    hwndTip = hwndTipCtrl;
    if (hwndTipCtrl)
    {
        SendMessage(hwndTipCtrl, TTM_SETMAXTIPWIDTH, 0, 400);
        EnumChildWindows(hwndDlg, [](HWND hwndChild, LPARAM lParam) -> BOOL
        {
            HWND hwndTip{ reinterpret_cast<HWND>(lParam) };
            TOOLINFO ti{};
            ti.cbSize = sizeof(ti);
            // TTF_SUBCLASS lets the tooltip relay mouse messages itself; TTF_IDISHWND
            // makes uId the control HWND so TTN_GETDISPINFO can recover the control ID.
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = GetParent(hwndChild);
            ti.uId = reinterpret_cast<UINT_PTR>(hwndChild);
            ti.lpszText = LPSTR_TEXTCALLBACK;
            SendMessage(hwndTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
            return TRUE;
        }, reinterpret_cast<LPARAM>(hwndTip));
    }
}

void MSIXPropertyPage::OnInitDialog(HWND hwndDlg)
{
    InitializeToolTips(hwndDlg, m_hwndTip);

    // Make the borderless-edit value fields read-only at runtime, matching the
    // behaviour Explorer's General tab applies to its Location/Size/etc. fields.
    // (Their design-time style is the same 0x50010080 we use here; ES_READONLY
    // is applied via EM_SETREADONLY rather than the dialog template, so the
    // fields look like static text but can still be selected and copied.)
    for (const auto id : c_readOnlyEdits)
    {
        SendDlgItemMessage(hwndDlg, id, EM_SETREADONLY, TRUE, 0);
    }

    // Disable UI for functionality not supported on the current system
    if (!m_addPackageOptions)
    {
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_ALLOWUNSIGNED), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_DEFERWHILEINUSE), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_DEVELOPERMODE), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_FORCE), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_LIMITTOEXISTINGPACKAGES), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_RETAINFILESONFAILURE), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_EXTERNAL), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_EXTERNAL_PATH), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_VOLUME_LABEL), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_VOLUME), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_INSTALLACTION), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_INSTALL), false);
    }
    if (!m_addPackageOptions2)
    {
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_LIMITTOEXISTINGPACKAGES), false);
    }
    if (!m_addPackageOptions3)
    {
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_PRIORITY_LABEL), false);
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_PRIORITY), false);
    }

    // Set the file path
    SetDlgItemText(hwndDlg, IDC_FILE_PATH, m_filePath ? m_filePath.get() : L"");

    // Populate the Priority combobox and select "Default" by default
    {
        HWND hPriority{ GetDlgItem(hwndDlg, IDC_PRIORITY) };
        SendMessage(hPriority, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Low"));
        const LRESULT defaultIndex{ SendMessage(hPriority, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Default")) };
        SendMessage(hPriority, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"High"));
        SendMessage(hPriority, CB_SETCURSEL, static_cast<WPARAM>(defaultIndex), 0);
    }

    // Populate the Install action combobox (Add / Stage) and select "Add". The
    // order matters: index 0 is Add, index 1 is Stage (see OnInstallDropDown).
    {
        HWND hAction{ GetDlgItem(hwndDlg, IDC_INSTALLACTION) };
        SendMessage(hAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Add"));
        SendMessage(hAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stage"));
        SendMessage(hAction, CB_SETCURSEL, 0, 0);
    }

    // Load file information
    std::ignore = LOG_IF_FAILED(LoadFileInfo());
    SetDlgItemText(hwndDlg, IDC_PACKAGE_FULL_NAME, m_package.PackageFullName());
    SetDlgItemText(hwndDlg, IDC_PACKAGE_FAMILY_NAME, m_package.PackageFamilyName());
    SetDlgItemText(hwndDlg, IDC_SIGNATURE_ORIGIN, MSIX::ToString(m_package.SignatureOrigin()));

    UpdatePackageStatus(hwndDlg);

    std::uint64_t sizeFootprintCompressed{ m_package.FootprintTotalSizeCompressed(true) };
    std::uint64_t sizeFootprintUncompressed{ m_package.FootprintTotalSizeCompressed(false) };
    std::uint64_t footprintCompressionRatio{ (sizeFootprintCompressed * 100 + (sizeFootprintUncompressed - 1)) / sizeFootprintUncompressed };
    std::uint64_t sizePayloadCompressed{ m_package.PayloadTotalSizeCompressed() };
    std::uint64_t sizePayloadUncompressed{ m_package.PayloadTotalSizeUncompressed() };
    std::uint64_t payloadCompressionRatio{ (sizePayloadCompressed * 100 + (sizePayloadUncompressed - 1)) / sizePayloadUncompressed };
    //TODO std::uint64_t sizeCompressed{ sizeFootprintCompressed + sizePayloadCompressed };
    std::uint64_t sizeUncompressed{ sizeFootprintUncompressed + sizePayloadUncompressed };
    SetDlgItemText_FormatSize(hwndDlg, IDC_FILE_UNCOMPRESSED_SIZE, sizeUncompressed);
    SetDlgItemText_FormatSizeAndRatio(hwndDlg, IDC_FOOTPRINT_COMPRESSED_SIZE, sizeFootprintCompressed, footprintCompressionRatio);
    SetDlgItemText_FormatSizeAndRatioAndCount(hwndDlg, IDC_PAYLOAD_COMPRESSED_SIZE, sizePayloadCompressed, payloadCompressionRatio,
                                              m_package.PayloadTotalCount(), m_package.PayloadTotalCount() == 1 ? L" file" : L" files");

    if (m_packageManager3)
    {
        HWND hVolume{ GetDlgItem(hwndDlg, IDC_VOLUME) };

        LRESULT defaultVolumeIndex{};
        wil::unique_hstring defaultPackageStorePathAsHString;
        PCWSTR defaultPackageStorePath{};
        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> defaultVolume;
        if (SUCCEEDED_LOG(m_packageManager3->GetDefaultPackageVolume(&defaultVolume)))
        {
            wil::unique_hstring path;
            if (SUCCEEDED_LOG(defaultVolume->get_PackageStorePath(wil::out_param(path))))
            {
                defaultPackageStorePath = WindowsGetStringRawBuffer(path.get(), nullptr);
            }
        }

        wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterable<ABI::Windows::Management::Deployment::PackageVolume*>> volumes;
        if (SUCCEEDED_LOG(m_packageManager3->FindPackageVolumes(&volumes)) && volumes)
        {
            wil::com_ptr_nothrow<ABI::Windows::Foundation::Collections::IIterator<ABI::Windows::Management::Deployment::PackageVolume*>> volumesIterator;
            if (SUCCEEDED_LOG(volumes->First(&volumesIterator)) && volumesIterator)
            {
                boolean hasCurrent{};
                if (SUCCEEDED_LOG(volumesIterator->get_HasCurrent(&hasCurrent)))
                {
                    while (hasCurrent)
                    {
                        wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> volume;
                        if (SUCCEEDED_LOG(volumesIterator->get_Current(&volume)) && volume)
                        {
                            wil::unique_hstring path;
                            if (SUCCEEDED_LOG(volume->get_PackageStorePath(wil::out_param(path))))
                            {
                                PCWSTR packageStorePath{ WindowsGetStringRawBuffer(path.get(), nullptr) };
                                if (packageStorePath)
                                {
                                    bool isDefault{};
                                    WCHAR display[MAX_PATH + 32]{};
                                    if (defaultPackageStorePath && (CompareStringOrdinal(packageStorePath, -1, defaultPackageStorePath, -1, TRUE) == CSTR_EQUAL))
                                    {
                                        std::ignore = LOG_IF_FAILED(StringCchPrintf(display, ARRAYSIZE(display), L"%s [DEFAULT]", packageStorePath));
                                        isDefault = true;
                                    }
                                    else
                                    {
                                        boolean isOffline{};
                                        if (SUCCEEDED_LOG(volume->get_IsOffline(&isOffline)) && isOffline)
                                        {
                                            std::ignore = LOG_IF_FAILED(StringCchPrintf(display, ARRAYSIZE(display), L"%s [OFFLINE]", packageStorePath));
                                        }
                                        else
                                        {
                                            std::ignore = LOG_IF_FAILED(StringCchPrintf(display, ARRAYSIZE(display), L"%s", packageStorePath));
                                        }
                                    }
                                    const LRESULT index{ SendMessage(hVolume, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display)) };
                                    SendMessage(hVolume, CB_SETITEMDATA, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(volume.detach()));
                                    if (isDefault)
                                    {
                                        defaultVolumeIndex = index;
                                    }
                                }
                            }
                        }
                        if (FAILED_LOG(volumesIterator->MoveNext(&hasCurrent)))
                        {
                            break;
                        }
                    }
                }
            }
        }
        if (defaultVolumeIndex)
        {
            SendMessage(hVolume, CB_SETCURSEL, static_cast<WPARAM>(defaultVolumeIndex), 0);
        }
        else
        {
            SendMessage(hVolume, CB_SETCURSEL, 0, 0);
        }
    }

    UpdateAddCertificateButton(hwndDlg);

    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t build{};
    std::uint16_t revision{};
    if (SUCCEEDED_LOG(wil::get_module_version(g_hInstance, major, minor, build, revision)))
    {
        HRESULT hr{};
        wil::unique_cotaskmem_string link;
        if (revision != 0)
        {
            PCWSTR format{ L"<a href=\"https://github.com/drustheaxe/MSIXcli\">MSIXcli v%hu.%hu.%hu.%hu home</a>" };
            hr = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(link, format, major, minor, build, revision));
        }
        else
        {
            PCWSTR format{ L"<a href=\"https://github.com/drustheaxe/MSIXcli\">MSIXcli v%hu.%hu.%hu home</a>" };
            hr = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(link, format, major, minor, build));
        }
        if (SUCCEEDED(hr))
        {
            SetDlgItemText(hwndDlg, IDC_HOME, link.get());
        }
    }
}

void MSIXPropertyPage::UpdatePackageStatus(HWND hwndDlg)
{
    if (m_package.IsStaged() || m_package.IsRegistered())
    {
        SetDlgItemText(hwndDlg, IDC_PACKAGEORIGIN, m_package.PackageOriginString());
    }
    else
    {
        SetDlgItemText(hwndDlg, IDC_PACKAGEORIGIN, L"--N/A--");
    }

    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_IS_STAGED), m_package.IsStaged());

    if (m_package.IsRegistered())
    {
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_IS_REGISTERED), true);
        // Registered on the newest version is "ok"; a pending newer version is a
        // warning that the installed package is out of date.
        m_registeredStatusColor = m_package.IsNewerVersionAvailable() ? StatusColor::Warning : StatusColor::Ok;
    }
    else
    {
        EnableAndShowControl(GetDlgItem(hwndDlg, IDC_IS_REGISTERED), false);
        m_registeredStatusColor = StatusColor::None;
    }

    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_IS_REMOVE_PENDING), m_package.IsRemovalPending());
    m_removalPendingStatusColor = m_package.IsRemovalPending() ? StatusColor::Warning : StatusColor::None;

    // The Remove button is only shown/enabled when the package can be removed
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_UNINSTALL), m_package.IsRemovable());

    // Repaint the owner-drawn status badges to reflect their current colors
    if (HWND hwndStaged{ GetDlgItem(hwndDlg, IDC_IS_STAGED) })
    {
        InvalidateRect(hwndStaged, nullptr, TRUE);
    }
    if (HWND hwndRegistered{ GetDlgItem(hwndDlg, IDC_IS_REGISTERED) })
    {
        InvalidateRect(hwndRegistered, nullptr, TRUE);
    }
    if (HWND hwndRemovalPending{ GetDlgItem(hwndDlg, IDC_IS_REMOVE_PENDING) })
    {
        InvalidateRect(hwndRemovalPending, nullptr, TRUE);
    }
}

void MSIXPropertyPage::DrawStatusBadge(const DRAWITEMSTRUCT& dis)
{
    StatusColor color{ StatusColor::None };
    switch (dis.CtlID)
    {
        case IDC_IS_STAGED:                 color = StatusColor::Ok; break;
        case IDC_IS_REGISTERED:             color = m_registeredStatusColor; break;
        case IDC_IS_REMOVE_PENDING:         color = m_removalPendingStatusColor; break;
        case IDC_CERTIFICATE_VALID_STATUS:  color = m_certificateStatusColor; break;
    }

    HDC hdc{ dis.hDC };
    const RECT rc{ dis.rcItem };

    // Blend the control's background with the themed tab texture behind it so the
    // area outside the badge matches the rest of the page.
    if (FAILED(DrawThemeParentBackground(dis.hwndItem, hdc, &rc)))
    {
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
    }

    if (color == StatusColor::None)
    {
        return;
    }

    COLORREF backColor{ c_statusOkBackground };
    COLORREF textColor{ c_statusOkText };
    switch (color)
    {
        case StatusColor::Warning: backColor = c_statusWarningBackground; textColor = c_statusWarningText; break;
        case StatusColor::Error:   backColor = c_statusErrorBackground;   textColor = c_statusErrorText;   break;
    }

    WCHAR text[64]{};
    const int length{ GetWindowTextW(dis.hwndItem, text, ARRAYSIZE(text)) };

    HFONT font{ reinterpret_cast<HFONT>(SendMessageW(dis.hwndItem, WM_GETFONT, 0, 0)) };
    HFONT oldFont{ font ? reinterpret_cast<HFONT>(SelectObject(hdc, font)) : nullptr };

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, text, length, &textSize);

    // Badge sized to the text plus horizontal padding, left-aligned within the
    // control and filling its height for a consistent bounded pill.
    const int padding{ textSize.cy / 2 };
    const RECT badge{ rc.left, rc.top, rc.left + textSize.cx + padding * 2, rc.bottom };

    // Rounded "pill": state-colored fill with a slightly darker (text-color) border
    const int diameter{ badge.bottom - badge.top };
    wil::unique_hpen pen{ CreatePen(PS_SOLID, 1, textColor) };
    wil::unique_hbrush fill{ CreateSolidBrush(backColor) };
    HGDIOBJ oldPen{ SelectObject(hdc, pen.get()) };
    HGDIOBJ oldBrush{ SelectObject(hdc, fill.get()) };
    RoundRect(hdc, badge.left, badge.top, badge.right, badge.bottom, diameter, diameter);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);

    // State-colored text centered within the badge
    const int previousBkMode{ SetBkMode(hdc, TRANSPARENT) };
    const COLORREF previousTextColor{ SetTextColor(hdc, textColor) };
    RECT textRect{ badge };
    DrawTextW(hdc, text, length, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SetTextColor(hdc, previousTextColor);
    SetBkMode(hdc, previousBkMode);

    if (oldFont)
    {
        SelectObject(hdc, oldFont);
    }
}

void MSIXPropertyPage::RefreshPackageStatus(
    HWND hwndDlg,
    ABI::Windows::Management::Deployment::IPackageManager12* packageManager12)
{
    std::ignore = LOG_IF_FAILED(m_package.DetectPackage(packageManager12));
    UpdatePackageStatus(hwndDlg);
}

void MSIXPropertyPage::UpdateAddCertificateButton(HWND hwndDlg)
{
    // Only show the Add Certificate button if the package is signed but not known to the system and actionable
    const bool enable{
        (m_package.SignatureOrigin() == MSIX::SignatureOrigin::Windows) ||
        (m_package.SignatureOrigin() == MSIX::SignatureOrigin::Store) ||
        (m_package.SignatureOrigin() == MSIX::SignatureOrigin::LineOfBusiness) ||
        (m_package.SignatureOrigin() == MSIX::SignatureOrigin::Unknown)
    };
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_CERTIFICATE), enable);
}

void MSIXPropertyPage::OnCommand(HWND hwndDlg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (LOWORD(wParam))
    {
        case IDC_CERTIFICATE:
            OnShowCertificateDialog(hwndDlg);
            break;

        case IDC_INSTALLACTION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                OnInstallDropDown(hwndDlg);
            }
            break;

        case IDC_INSTALL:
            OnInstall(hwndDlg);
            break;

        case IDC_UNINSTALL:
            OnUninstall(hwndDlg);
            break;
    }
}

void MSIXPropertyPage::OnAddOrRemoveCertificate(HWND hwndDlg, const bool add)
{
    auto wait = wil::ui::scoped_wait_cursor(hwndDlg);

    wil::unique_cotaskmem_string command;
    std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(command, L"certificate %ls \"%ls\"", add ? L"add" : L"remove", m_filePath.get()));
    HRESULT exitCode{};
    PCWSTR verb{ add ? L"adding certificate" : L"removing certificate" };
    const HRESULT hr{ LOG_IF_FAILED(::msixcli::ExecuteMsixAdminUI(g_hInstance, command.get(), exitCode, hwndDlg, verb)) };
    if (SUCCEEDED(hr))
    {
        SetDlgItemText(hwndDlg, IDC_CERTIFICATE_STATUS, add ? L"Installed" : L"Not installed");
    }

    // Re-evaluate the package's Signature Origin
    if (m_package.IsPackage())
    {
        std::ignore = LOG_IF_FAILED(m_package.DetectSignatureOrigin(m_package.PackageReader()));
    }
    else if (m_package.IsBundle())
    {
        std::ignore = LOG_IF_FAILED(m_package.DetectSignatureOrigin(m_package.BundleReader()));
    }
    SetDlgItemText(hwndDlg, IDC_SIGNATURE_ORIGIN, MSIX::ToString(m_package.SignatureOrigin()));
}

void MSIXPropertyPage::OnCheckCertificateStatus(HWND hwndDlg)
{
    auto wait = wil::ui::scoped_wait_cursor(hwndDlg);

    SetDlgItemText(hwndDlg, IDC_CERTIFICATE_STATUS, L"?" );

    wil::unique_cotaskmem_string command;
    std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(command, L"certificate exists \"%ls\"", m_filePath.get()));
    HRESULT exitCode{};
    PCWSTR verb{ L"checking certificate status" };
    const HRESULT hr{ LOG_IF_FAILED(::msixcli::ExecuteMsixAdminUI(g_hInstance, command.get(), exitCode, hwndDlg, verb)) };
    if (SUCCEEDED(hr))
    {
        const bool isInstalled{ exitCode == S_OK };
        SetDlgItemText(hwndDlg, IDC_CERTIFICATE_STATUS, isInstalled ? L"Installed" : L"Not installed");
    }
}

void MSIXPropertyPage::OnShowCertificateDialog(HWND hwndDlg)
{
    // Show the certificate dialog modally. It returns when the user dismisses it
    // (OK, Cancel/Escape, or the close button).
    std::ignore = DialogBoxParamW(g_hInstance, MAKEINTRESOURCE(IDD_MSIX_CERTIFICATE),
                                  hwndDlg, CertificateDialogProc, reinterpret_cast<LPARAM>(this));

    // Adding a certificate may have changed the package's Signature Origin, so
    // re-evaluate whether the page's Certificate button should still be shown.
    UpdateAddCertificateButton(hwndDlg);
}

void MSIXPropertyPage::CenterDialog(HWND hwndDlg, HWND hwndParent)
{
    // Center hwndDlg over hwndParent, clamped to the parent monitor's work area so
    // the dialog is never pushed partly off-screen for an odd parent placement.
    RECT dialogRect{};
    if (!hwndParent || !GetWindowRect(hwndDlg, &dialogRect))
    {
        return;
    }
    const int dialogWidth{ dialogRect.right - dialogRect.left };
    const int dialogHeight{ dialogRect.bottom - dialogRect.top };

    RECT parentRect{};
    if (!GetWindowRect(hwndParent, &parentRect))
    {
        return;
    }

    int x{ parentRect.left + ((parentRect.right - parentRect.left) - dialogWidth) / 2 };
    int y{ parentRect.top + ((parentRect.bottom - parentRect.top) - dialogHeight) / 2 };

    MONITORINFO monitorInfo{ sizeof(monitorInfo) };
    if (GetMonitorInfo(MonitorFromWindow(hwndParent, MONITOR_DEFAULTTONEAREST), &monitorInfo))
    {
        const RECT& work{ monitorInfo.rcWork };
        x = max(work.left, min(x, work.right - dialogWidth));
        y = max(work.top, min(y, work.bottom - dialogHeight));
    }

    SetWindowPos(hwndDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK MSIXPropertyPage::CertificateDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MSIXPropertyPage* pThis{};
    if (uMsg == WM_INITDIALOG)
    {
        pThis = reinterpret_cast<MSIXPropertyPage*>(lParam);
        SetWindowLongPtr(hwndDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));

        CenterDialog(hwndDlg, GetParent(hwndDlg));

        if (pThis)
        {
            pThis->InitializeToolTips(hwndDlg, pThis->m_certificateHwndTip);

            // Make the borderless-edit value fields read-only at runtime, matching the
            // behaviour Explorer's General tab applies to its Location/Size/etc. fields.
            // (Their design-time style is the same 0x50010080 we use here; ES_READONLY
            // is applied via EM_SETREADONLY rather than the dialog template, so the
            // fields look like static text but can still be selected and copied.)
            for (const auto id : c_certificateReadOnlyEdits)
            {
                SendDlgItemMessage(hwndDlg, id, EM_SETREADONLY, TRUE, 0);
            }

            SetDlgItemText(hwndDlg, IDC_FILE_PATH, pThis->FilePath());
            SetDlgItemText(hwndDlg, IDC_PACKAGE_FULL_NAME, pThis->PackageFullName());
            SetDlgItemText(hwndDlg, IDC_PACKAGE_FAMILY_NAME, pThis->PackageFamilyName());

            const auto signatureOrigin{ pThis->SignatureOrigin() };
            SetDlgItemText(hwndDlg, IDC_SIGNATURE_ORIGIN, MSIX::ToString(signatureOrigin));

            SetDlgItemText(hwndDlg, IDC_CERTIFICATE_SUBJECT, pThis->CertificateSubject());
            SetDlgItemText(hwndDlg, IDC_CERTIFICATE_ISSUER, pThis->CertificateIssuer());
            {
                bool enable{};
                switch (pThis->CertificateValidStatus())
                {
                    case MSIX::Signing::CertificateValid::Unknown:     break;
                    case MSIX::Signing::CertificateValid::NotYetValid: enable = true; pThis->m_certificateStatusColor = StatusColor::Warning; break;
                    case MSIX::Signing::CertificateValid::Valid:       break;
                    case MSIX::Signing::CertificateValid::Expired:     enable = true; pThis->m_certificateStatusColor = StatusColor::Error; break;
                }
                EnableAndShowControl(GetDlgItem(hwndDlg, IDC_CERTIFICATE_VALID_STATUS), enable);
            }
            {
                wil::unique_cotaskmem_string validFrom;
                std::ignore = LOG_IF_FAILED(wil::filetime::format_filetime(pThis->CertificateValidFrom(), true, validFrom));
                wil::unique_cotaskmem_string validTo;
                std::ignore = LOG_IF_FAILED(wil::filetime::format_filetime(pThis->CertificateValidTo(), true, validTo));
                wil::unique_cotaskmem_string valid;
                std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(valid, L"%ls  to  %ls", validFrom.get(), validTo.get()));
                SetDlgItemText(hwndDlg, IDC_CERTIFICATE_VALID, valid ? valid.get() : L"???  to  ???");
            }
            SetDlgItemText(hwndDlg, IDC_CERTIFICATE_STATUS, L"?");

            size_t thumbprintSize{};
            wil::unique_cotaskmem_ptr<BYTE[]> thumbprint;
            if (SUCCEEDED_LOG(pThis->m_package.GetCertificateThumbprint(thumbprintSize, thumbprint)))
            {
                if ((thumbprintSize > 0) && thumbprint)
                {
                    wil::unique_cotaskmem_string thumbprintAsString{ wil::make_cotaskmem_string_nothrow(nullptr, thumbprintSize * 2) };
                    if (thumbprintAsString)
                    {
                        for (size_t index = 0; index < thumbprintSize; ++index)
                        {
                            StringCchPrintfW(thumbprintAsString.get() + (index * 2), 3, L"%02X", thumbprint[index]);
                        }
                        SetDlgItemText(hwndDlg, IDC_CERTIFICATE_THUMBPRINT, thumbprintAsString.get());
                    }
                }
            }

            const bool installable{
                (signatureOrigin == MSIX::SignatureOrigin::LineOfBusiness) ||
                (signatureOrigin == MSIX::SignatureOrigin::Unknown)
            };
            if (installable)
            {
                bool isElevated{};
                if (SUCCEEDED_LOG( wil::security::is_elevated(isElevated)))
                {
                    const BOOL elevationRequiredState{ isElevated ? FALSE : TRUE };
                    Button_SetElevationRequiredState(GetDlgItem(hwndDlg, IDC_CERTIFICATE_CHECK_STATUS), elevationRequiredState);
                    Button_SetElevationRequiredState(GetDlgItem(hwndDlg, IDC_CERTIFICATE_ADD), elevationRequiredState);
                    Button_SetElevationRequiredState(GetDlgItem(hwndDlg, IDC_CERTIFICATE_REMOVE), elevationRequiredState);
                }
            }
            EnableAndShowControl(GetDlgItem(hwndDlg, IDC_CERTIFICATE_CHECK_STATUS), installable);
            EnableAndShowControl(GetDlgItem(hwndDlg, IDC_CERTIFICATE_ADD), installable);
            EnableAndShowControl(GetDlgItem(hwndDlg, IDC_CERTIFICATE_REMOVE), installable);
        }

        // Repaint the owner-drawn status badges to reflect their current colors
        if (HWND hwndStaged{ GetDlgItem(hwndDlg, IDC_CERTIFICATE_VALID_STATUS) })
        {
            InvalidateRect(hwndStaged, nullptr, TRUE);
        }

        return TRUE;
    }

    // WM_CTLCOLOR* doesn't depend on instance state and can arrive before
    // WM_INITDIALOG returns, so handle it outside the pThis guard.
    if (uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLOREDIT)
    {
        HWND hwndCtl{ reinterpret_cast<HWND>(lParam) };
        if (IsCertificateBorderlessReadOnlyEdit(GetDlgCtrlID(hwndCtl)))
        {
            // Draw these read-only edits exactly like the static value labels
            // beside them. Because they're read-only they send WM_CTLCOLORSTATIC,
            // so deferring to the default dialog handling makes them pick up the
            // same themed property-page (tab) background the labels use instead
            // of a darker, hard-coded COLOR_BTNFACE rectangle.
            return FALSE;
        }
    }

    pThis = reinterpret_cast<MSIXPropertyPage*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
    if (pThis)
    {
        switch (uMsg)
        {
            case WM_COMMAND:
            {
                switch (LOWORD(wParam))
                {
                    case IDC_CERTIFICATE_ADD:
                        pThis->OnAddOrRemoveCertificate(hwndDlg, true);
                        return TRUE;

                    case IDC_CERTIFICATE_REMOVE:
                        pThis->OnAddOrRemoveCertificate(hwndDlg, false);
                        return TRUE;

                    case IDC_CERTIFICATE_CHECK_STATUS:
                        pThis->OnCheckCertificateStatus(hwndDlg);
                        return TRUE;

                    case IDC_OK:
                        EndDialog(hwndDlg, IDC_OK);
                        return TRUE;

                    case IDCANCEL:
                        EndDialog(hwndDlg, IDCANCEL);
                        return TRUE;
                }
                break;
            }
            case WM_DRAWITEM:
            {
                pThis->DrawStatusBadge(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
                return TRUE;
            }
            case WM_SETCURSOR:
            {
                // (HWND)wParam is the window under the cursor; LOWORD(lParam) is the hit-test
                if (wil::ui::is_busy(hwndDlg) && reinterpret_cast<HWND>(wParam) == hwndDlg)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                    SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break; // not busy -> default arrow
            }
            case WM_NOTIFY:
            {
                LPNMHDR pnmh{ reinterpret_cast<LPNMHDR>(lParam) };
                if (pnmh && (pnmh->code == TTN_GETDISPINFO))
                {
                    NMTTDISPINFO* pdi{ reinterpret_cast<NMTTDISPINFO*>(lParam) };
                    // With TTF_IDISHWND the tool's idFrom is the control's HWND;
                    // map it back to the control ID and load the matching tooltip
                    // string from the resource string table.
                    const UINT controlId{ static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(pdi->hdr.idFrom))) };
                    pdi->szText[0] = L'\0';
                    if (controlId == IDC_PACKAGE_FULL_NAME)
                    {
                        pdi->lpszText = const_cast<PWSTR>(pThis->PackageFullName());
                    }
                    else if (controlId == IDC_PACKAGE_FAMILY_NAME)
                    {
                        pdi->lpszText = const_cast<PWSTR>(pThis->PackageFamilyName());
                    }
                    else
                    {
                        LoadStringW(g_hInstance, controlId, pThis->m_tooltipText, ARRAYSIZE(pThis->m_tooltipText));
                        pdi->lpszText = pThis->m_tooltipText;
                    }
                }
                break;
            }
            case WM_DESTROY:
            {
                // The tooltip control is a top-level WS_POPUP window, so it isn't
                // destroyed automatically with the dialog's child controls; tear it
                // down explicitly (mirrors the property page's OnDestroy).
                if (pThis->m_certificateHwndTip)
                {
                    DestroyWindow(pThis->m_certificateHwndTip);
                    pThis->m_certificateHwndTip = nullptr;
                }
                break;
            }
        }
    }

    return FALSE;
}

void MSIXPropertyPage::OnInstallDropDown(HWND hwndDlg)
{
    // Read the newly selected action from the combobox
    const LRESULT selection{ SendDlgItemMessage(hwndDlg, IDC_INSTALLACTION, CB_GETCURSEL, 0, 0) };
    const InstallAction action{ (selection == 1) ? InstallAction::Stage : InstallAction::Add };

    // Remember the choice as the new default and enable/disable the options that
    // only apply when adding (rather than staging) the package.
    m_installAction = action;
    const bool enable{ action == InstallAction::Add };
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_DEFERWHILEINUSE), enable);
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_FORCE), enable);
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_LIMITTOEXISTINGPACKAGES), enable);
    EnableAndShowControl(GetDlgItem(hwndDlg, IDC_RETAINFILESONFAILURE), enable);
}

void MSIXPropertyPage::OnInstall(HWND hwndDlg)
{
    auto wait = wil::ui::scoped_wait_cursor(hwndDlg);

    ExecuteInstall(hwndDlg, m_installAction);
    RefreshPackageStatus(hwndDlg, m_packageManager12.get());
}

void MSIXPropertyPage::ExecuteInstall(HWND hwndDlg, InstallAction action)
{
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString;
    HRESULT extendedError{};
    GUID activityId{};
    const auto hr{ LOG_IF_FAILED(OnInstall(hwndDlg, action, errorText, errorTextHString, extendedError, activityId)) };
    if (FAILED(hr))
    {
        PCWSTR verb{ (action == InstallAction::Stage) ? L"staging" : L"installing" };
        wil::unique_cotaskmem_string caption;
        std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(caption,
            L"MSIX Property Page: Error %ls package", verb));
        wil::unique_cotaskmem_string text;
        PCWSTR formatter{ L"Error 0x%08X %ls %ls\n\n%ls\n\nFor more details run powershell command:\n\nGet-AppxPackageLog -ActivityID %ls" };
        WCHAR activityIdAsString[40]{};
        std::ignore = StringFromGUID2(activityId, activityIdAsString, ARRAYSIZE(activityIdAsString));
        std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(text, formatter,
            hr, verb, m_filePath.get(), errorText ? errorText : L"<null>", activityIdAsString));
        MessageBoxW(hwndDlg, text ? text.get() : L"<null>", caption ? caption.get() : L"MSIX Property Page", MB_OK | MB_ICONERROR);
    }
}

HRESULT MSIXPropertyPage::OnInstall(
    HWND hwndDlg,
    InstallAction action,
    PCWSTR& errorText,
    wil::unique_hstring& errorTextHString,
    HRESULT& extendedError,
    GUID& activityId)
{
    errorText = nullptr;
    errorTextHString.reset();
    extendedError = S_OK;

    if (!m_filePath || !m_packageManager9 || !m_packageManager3)
    {
        return S_OK;
    }

    const auto isChecked = [hwndDlg](int id) -> boolean
    {
        return (IsDlgButtonChecked(hwndDlg, id) == BST_CHECKED) ? TRUE : FALSE;
    };

    // Resolve the inputs shared by both Add and Stage
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
    auto hwndVolume{ GetDlgItem(hwndDlg, IDC_VOLUME) };
    const auto targetVolumeIndex{ SendMessage(hwndVolume, CB_GETCURSEL, 0, 0) };
    WCHAR targetVolumePath[MAX_PATH]{};
    SendMessage(hwndVolume, CB_GETLBTEXT, static_cast<WPARAM>(targetVolumeIndex), reinterpret_cast<LPARAM>(targetVolumePath));
    if (targetVolumePath && !wil::string_ends_with(targetVolumePath, L"[DEFAULT]"))
    {
        targetVolume = reinterpret_cast<ABI::Windows::Management::Deployment::IPackageVolume*>(
            SendMessage(hwndVolume, CB_GETITEMDATA, static_cast<WPARAM>(targetVolumeIndex), 0));
    }

    // If the External Location option is checked, read the path from the editbox
    // and convert it to a URI.
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> externalLocationUri;
    if (isChecked(IDC_EXTERNAL))
    {
        const int cch{ GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_EXTERNAL_PATH)) };
        auto externalPath{ wil::make_unique_nothrow<WCHAR[]>(static_cast<size_t>(cch) + 1) };
        RETURN_IF_NULL_ALLOC(externalPath);
        GetDlgItemTextW(hwndDlg, IDC_EXTERNAL_PATH, externalPath.get(), cch + 1);
        RETURN_IF_FAILED(wil::to_uri(externalPath.get(), m_uriFactory.get(), externalLocationUri));
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(m_filePath.get(), m_uriFactory.get(), packageUri));

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;
    if (action == InstallAction::Stage)
    {
        // Activate the stage options object on first use. IStagePackageOptions
        // supports a subset of the install options.
        if (!m_stagePackageOptions)
        {
            HSTRING_HEADER classIdHeader{};
            HSTRING classId{};
            RETURN_IF_FAILED(WindowsCreateStringReference(
                RuntimeClass_Windows_Management_Deployment_StagePackageOptions,
                ARRAYSIZE(RuntimeClass_Windows_Management_Deployment_StagePackageOptions) - 1,
                &classIdHeader, &classId));
            wil::com_ptr_nothrow<IInspectable> inspectable;
            RETURN_IF_FAILED(RoActivateInstance(classId, inspectable.put()));
            RETURN_IF_FAILED(inspectable.query_to(m_stagePackageOptions.put()));
            RETURN_IF_FAILED(inspectable.query_to(m_stagePackageOptions3.put()));
        }

        RETURN_IF_FAILED(m_stagePackageOptions->put_DeveloperMode(isChecked(IDC_DEVELOPERMODE)));
        RETURN_IF_FAILED(m_stagePackageOptions->put_AllowUnsigned(isChecked(IDC_ALLOWUNSIGNED)));
        if (externalLocationUri)
        {
            RETURN_IF_FAILED(m_stagePackageOptions->put_ExternalLocationUri(externalLocationUri.get()));
        }
        if (targetVolume)
        {
            RETURN_IF_FAILED(m_stagePackageOptions->put_TargetVolume(targetVolume.get()));
        }
        if (m_stagePackageOptions3)
        {
            RETURN_IF_FAILED(m_stagePackageOptions3->put_PackageOperationPriority(GetPriority(hwndDlg)));
        }
        RETURN_IF_FAILED(m_packageManager9->StagePackageByUriAsync(packageUri.get(), m_stagePackageOptions.get(), deploymentOperation.put()));
    }
    else
    {
        //TODO JIT activate instance
        if (!m_addPackageOptions)
        {
            return S_OK;
        }

        // Map the option checkboxes onto the AddPackageOptions object
        RETURN_IF_FAILED(m_addPackageOptions->put_ForceAppShutdown(isChecked(IDC_FORCE)));
        RETURN_IF_FAILED(m_addPackageOptions->put_DeferRegistrationWhenPackagesAreInUse(isChecked(IDC_DEFERWHILEINUSE)));
        RETURN_IF_FAILED(m_addPackageOptions->put_DeveloperMode(isChecked(IDC_DEVELOPERMODE)));
        RETURN_IF_FAILED(m_addPackageOptions->put_AllowUnsigned(isChecked(IDC_ALLOWUNSIGNED)));
        RETURN_IF_FAILED(m_addPackageOptions->put_RetainFilesOnFailure(isChecked(IDC_RETAINFILESONFAILURE)));
        if (externalLocationUri)
        {
            RETURN_IF_FAILED(m_addPackageOptions->put_ExternalLocationUri(externalLocationUri.get()));
        }
        if (targetVolume)
        {
            RETURN_IF_FAILED(m_addPackageOptions->put_TargetVolume(targetVolume.get()));
        }
        if (m_addPackageOptions2)
        {
            RETURN_IF_FAILED(m_addPackageOptions2->put_LimitToExistingPackages(isChecked(IDC_LIMITTOEXISTINGPACKAGES)));
        }
        if (m_addPackageOptions3)
        {
            RETURN_IF_FAILED(m_addPackageOptions3->put_PackageOperationPriority(GetPriority(hwndDlg)));
        }
        RETURN_IF_FAILED(m_packageManager9->AddPackageByUriAsync(packageUri.get(), m_addPackageOptions.get(), deploymentOperation.put()));
    }

    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    return S_OK;
}

void MSIXPropertyPage::OnUninstall(HWND hwndDlg)
{
    auto wait = wil::ui::scoped_wait_cursor(hwndDlg);

    ExecuteUninstall(hwndDlg);
    RefreshPackageStatus(hwndDlg, m_packageManager12.get());
}

void MSIXPropertyPage::ExecuteUninstall(HWND hwndDlg)
{
    PCWSTR errorText{};
    wil::unique_hstring errorTextHString;
    HRESULT extendedError{};
    GUID activityId{};
    const auto hr{ LOG_IF_FAILED(OnUninstall(hwndDlg, errorText, errorTextHString, extendedError, activityId)) };
    if (FAILED(hr))
    {
        PCWSTR caption{ L"MSIX Property Page: Error removing package" };
        wil::unique_cotaskmem_string text;
        PCWSTR formatter{ L"Error 0x%08X removing %ls\n\n%ls\n\nFor more details run powershell command:\n\nGet-AppxPackageLog -ActivityID %ls" };
        WCHAR activityIdAsString[40]{};
        std::ignore = StringFromGUID2(activityId, activityIdAsString, ARRAYSIZE(activityIdAsString));
        std::ignore = LOG_IF_FAILED(wil::str_printf_nothrow<wil::unique_cotaskmem_string>(text, formatter,
            hr, m_filePath.get(), errorText ? errorText : L"<null>", activityIdAsString));
        MessageBoxW(hwndDlg, text ? text.get() : L"<null>", caption ? caption : L"MSIX Property Page", MB_OK | MB_ICONERROR);
    }
}

HRESULT MSIXPropertyPage::OnUninstall(
    HWND hwndDlg,
    PCWSTR& errorText,
    wil::unique_hstring& errorTextHString,
    HRESULT& extendedError,
    GUID& activityId)
{
    errorText = nullptr;
    errorTextHString.reset();
    extendedError = S_OK;

    if (!m_filePath || !m_packageManager9 || !m_packageManager3)
    {
        return S_OK;
    }

    const auto isChecked = [hwndDlg](int id) -> boolean
    {
        return (IsDlgButtonChecked(hwndDlg, id) == BST_CHECKED) ? TRUE : FALSE;
    };

    // Resolve the inputs shared by both Add and Stage
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageVolume> targetVolume;
    auto hwndVolume{ GetDlgItem(hwndDlg, IDC_VOLUME) };
    const auto targetVolumeIndex{ SendMessage(hwndVolume, CB_GETCURSEL, 0, 0) };
    WCHAR targetVolumePath[MAX_PATH]{};
    SendMessage(hwndVolume, CB_GETLBTEXT, static_cast<WPARAM>(targetVolumeIndex), reinterpret_cast<LPARAM>(targetVolumePath));
    if (targetVolumePath && !wil::string_ends_with(targetVolumePath, L"[DEFAULT]"))
    {
        targetVolume = reinterpret_cast<ABI::Windows::Management::Deployment::IPackageVolume*>(
            SendMessage(hwndVolume, CB_GETITEMDATA, static_cast<WPARAM>(targetVolumeIndex), 0));
    }

    // If the External Location option is checked, read the path from the editbox
    // and convert it to a URI.
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> externalLocationUri;
    if (isChecked(IDC_EXTERNAL))
    {
        const int cch{ GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_EXTERNAL_PATH)) };
        auto externalPath{ wil::make_unique_nothrow<WCHAR[]>(static_cast<size_t>(cch) + 1) };
        RETURN_IF_NULL_ALLOC(externalPath);
        GetDlgItemTextW(hwndDlg, IDC_EXTERNAL_PATH, externalPath.get(), cch + 1);
        RETURN_IF_FAILED(wil::to_uri(externalPath.get(), m_uriFactory.get(), externalLocationUri));
    }

    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass> packageUri;
    RETURN_IF_FAILED(wil::to_uri(m_filePath.get(), m_uriFactory.get(), packageUri));

    wil::com_ptr_nothrow<__FIAsyncOperationWithProgress_2_Windows__CManagement__CDeployment__CDeploymentResult_Windows__CManagement__CDeployment__CDeploymentProgress> deploymentOperation;

    HSTRING_HEADER packageFullNameHeader{};
    HSTRING packageFullName{};
    RETURN_IF_FAILED(wil::to_hstring_reference(m_package.PackageFullName(), packageFullNameHeader, packageFullName));
    auto removeOptions{ ABI::Windows::Management::Deployment::RemovalOptions_None };
#if defined(TODO_REMOVE_OPTIONS)
    if (isChecked(IDC_PRESERVEDATA))
    {
        removeOptions |= ABI::Windows::Management::Deployment::RemovalOptions_PreserveApplicationData;
    }
    if (isChecked(IDC_REMOVEALLUSERS))
    {
        removeOptions |= ABI::Windows::Management::Deployment::RemovalOptions_RemoveForAllUsers;
    }
    if (isChecked(IDC_DEFERREMOVE))
    {
        removeOptions |= ABI::Windows::Management::Deployment::RemovalOptions_DeferRemovalWhenPackagesAreInUse;
    }
#endif
    RETURN_IF_FAILED(m_packageManager2->RemovePackageWithOptionsAsync(packageFullName, removeOptions, deploymentOperation.put()));

    RETURN_IF_FAILED(MSIX::Deployment::GetResults(deploymentOperation.get(), errorText, errorTextHString, extendedError, activityId));
    return S_OK;
}

HRESULT MSIXPropertyPage::LoadFileInfo()
{
    if (!m_filePath)
    {
        return S_OK;
    }

    RETURN_IF_FAILED(m_package.Open(m_filePath.get(), m_packageManager12.get()));
    return S_OK;
}

HRESULT MSIXPropertyPage::AddToList(HWND hList, PCWSTR key, PCWSTR value)
{
    LVITEM item{};
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    item.iSubItem = 0;
    item.pszText = const_cast<PWSTR>(key ? key : L"<null>");
    int row{ ListView_InsertItem(hList, &item) };
    ListView_SetItemText(hList, row, 1, const_cast<PWSTR>(value ? value : L"<null>"));
    return S_OK;
}

void MSIXPropertyPage::EnableAndShowControl(HWND hControl, bool enable)
{
    EnableControl(hControl, enable);
    ShowControl(hControl, enable);
}

void MSIXPropertyPage::EnableControl(HWND hControl, bool enable)
{
    EnableWindow(hControl, enable ? TRUE : FALSE);
}

void MSIXPropertyPage::ShowControl(HWND hControl, bool show)
{
    ShowWindow(hControl, show ? SW_SHOW : SW_HIDE);
}

void MSIXPropertyPage::FormatSize(std::uint64_t bytes, PWSTR buffer, size_t cch) noexcept
{
    // StrFormatByteSizeW takes LONGLONG; clamp gracefully for huge values
    const LONGLONG signedBytes{ (
        bytes > static_cast<std::uint64_t>(LLONG_MAX))
            ? LLONG_MAX
            : static_cast<LONGLONG>(bytes)
    };

    if (StrFormatByteSizeW(signedBytes, buffer, static_cast<UINT>(cch)) == nullptr)
    {
        StringCchPrintfW(buffer, cch, L"%llu bytes", bytes);
        return;
    }

    if (bytes >= 1024)
    {
        wchar_t tail[64]{};
        StringCchPrintfW(tail, ARRAYSIZE(tail), L"  (%llu bytes)", bytes);
        StringCchCatW(buffer, cch, tail);
    }
}

void MSIXPropertyPage::SetDlgItemText_Format(HWND hwndDlg, int nIDDlgItem, std::uint64_t value, PCWSTR suffix)
{
    WCHAR string[256]{};
    StringCchPrintfW(string, ARRAYSIZE(string), L"%llu%ls", value, suffix ? suffix : L"");
    SetDlgItemText(hwndDlg, nIDDlgItem, string);
}

void MSIXPropertyPage::SetDlgItemText_FormatSize(HWND hwndDlg, int nIDDlgItem, std::uint64_t size)
{
    WCHAR string[64]{};
    FormatSize(size, string, ARRAYSIZE(string));
    SetDlgItemText(hwndDlg, nIDDlgItem, string);
}

void MSIXPropertyPage::SetDlgItemText_FormatSizeAndRatio(HWND hwndDlg, int nIDDlgItem, std::uint64_t size, std::uint64_t ratio)
{
    WCHAR sizeString[64]{};
    FormatSize(size, sizeString, ARRAYSIZE(sizeString));

    WCHAR string[100]{};
    StringCchPrintfW(string, ARRAYSIZE(string), L"%ls  [%llu%%]", sizeString, ratio);
    SetDlgItemText(hwndDlg, nIDDlgItem, string);
}

void MSIXPropertyPage::SetDlgItemText_FormatSizeAndRatioAndCount(HWND hwndDlg, int nIDDlgItem, std::uint64_t size, std::uint64_t ratio, std::uint64_t count, PCWSTR suffix)
{
    WCHAR sizeString[64]{};
    FormatSize(size, sizeString, ARRAYSIZE(sizeString));

    WCHAR string[256]{};
    StringCchPrintfW(string, ARRAYSIZE(string), L"%ls  [%llu%%] \u2012 %llu %ls", sizeString, ratio, count, suffix);
    SetDlgItemText(hwndDlg, nIDDlgItem, string);
}

HRESULT MSIXPropertyPage::GetText(HWND hwndDlg, int nIDDlgItem, wistd::unique_ptr<WCHAR[]>& text)
{
    const int cch{ GetWindowTextLengthW(GetDlgItem(hwndDlg, nIDDlgItem)) };
    text = wistd::move(wil::make_unique_nothrow<WCHAR[]>(static_cast<size_t>(cch) + 1));
    RETURN_IF_NULL_ALLOC(text);
    GetDlgItemTextW(hwndDlg, IDC_EXTERNAL_PATH, text.get(), cch + 1);
    return S_OK;
}

ABI::Windows::Management::Deployment::PackageOperationPriority MSIXPropertyPage::GetPriority(HWND hwndDlg)
{
    // The combobox items are ordered Low, Default, High, matching the enum values 0/1/2
    const LRESULT sel{ SendDlgItemMessage(hwndDlg, IDC_PRIORITY, CB_GETCURSEL, 0, 0) };
    return (sel == CB_ERR) ?
        ABI::Windows::Management::Deployment::PackageOperationPriority_Normal :
        static_cast<ABI::Windows::Management::Deployment::PackageOperationPriority>(sel);
}
