// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information

#pragma once

class MSIXPropertyPage
{
public:
    explicit MSIXPropertyPage(wil::unique_process_heap_ptr<WCHAR[]> filePath);
    ~MSIXPropertyPage();

    HPROPSHEETPAGE CreatePropertyPage();

private:
    static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK PropPageCallbackProc(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp);
    void ReleaseTargetVolumes(HWND hwndDlg);

    void InitializeToolTips(HWND hwndDlg, HWND& hwndTip);

    void OnInitDialog(HWND hwndDlg);
    void OnDestroy(HWND hwndDlg);

    void UpdatePackageStatus(HWND hwndDlg);

    // Background/text color of a live package-status badge.
    // "None" hides the badge.
    enum class StatusColor { None, Ok, Warning, Error };

    // Owner-draw handler for the live status labels. Paints a bounded,
    // state-colored "pill" so the status reads as distinct live system
    // information. The area outside the pill blends with the themed tab background.
    void DrawStatusBadge(const DRAWITEMSTRUCT& dis);

    void RefreshPackageStatus(
        HWND hwndDlg,
        ABI::Windows::Management::Deployment::IPackageManager12* packageManager12);

    void UpdateAddCertificateButton(HWND hwndDlg);

    void OnCommand(HWND hwndDlg, WPARAM wParam, LPARAM lParam);

    enum class InstallAction { Add, Stage };

    // Handles a change of the Install action combobox (Add / Stage): updates the
    // selected action and the controls that depend on it.
    void OnInstallDropDown(HWND hwndDlg);

    // Performs the "add certificate" action; invoked from the modal certificate dialog
    void OnAddCertificate(HWND hwndDlg);

    // Handles the IDC_CERTIFICATE button click: shows IDD_MSIX_CERTIFICATE modally
    void OnShowCertificateDialog(HWND hwndDlg);

    // Dialog procedure for the modal IDD_MSIX_CERTIFICATE dialog
    static INT_PTR CALLBACK CertificateDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Handles the IDC_INSTALL button click: runs the currently selected action
    void OnInstall(HWND hwndDlg);

    // Run the requested deployment action and report any failure to the user
    void ExecuteInstall(HWND hwndDlg, InstallAction action);

    HRESULT OnInstall(
        HWND hwndDlg,
        InstallAction action,
        PCWSTR& errorText,
        wil::unique_hstring& errorTextHString,
        HRESULT& extendedError,
        GUID& activityId);

    // Handles the IDC_UNINSTALL button click
    void OnUninstall(HWND hwndDlg);

    // Run the requested deployment action and report any failure to the user
    void ExecuteUninstall(HWND hwndDlg);

    HRESULT OnUninstall(
        HWND hwndDlg,
        PCWSTR& errorText,
        wil::unique_hstring& errorTextHString,
        HRESULT& extendedError,
        GUID& activityId);

    HRESULT LoadFileInfo();

    HRESULT AddToList(HWND hList, PCWSTR key, PCWSTR value);

    void EnableAndShowControl(HWND hControl, bool enable);
    void EnableControl(HWND hControl, bool enable);
    void ShowControl(HWND hControl, bool show);

    void FormatSize(std::uint64_t bytes, PWSTR buffer, size_t cch) noexcept;
    void SetDlgItemText_Format(HWND hwndDlg, int nIDDlgItem, std::uint64_t value, PCWSTR suffix = nullptr);
    void SetDlgItemText_FormatSize(HWND hwndDlg, int nIDDlgItem, std::uint64_t size);
    void SetDlgItemText_FormatSizeAndRatio(HWND hwndDlg, int nIDDlgItem, std::uint64_t size, std::uint64_t ratio);
    void SetDlgItemText_FormatSizeAndRatioAndCount(HWND hwndDlg, int nIDDlgItem, std::uint64_t size, std::uint64_t ratio, std::uint64_t count, PCWSTR suffix);

    HRESULT ToUri(PCWSTR string, wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClass>& uri);

    HRESULT GetText(HWND hwndDlg, int nIDDlgItem, wistd::unique_ptr<WCHAR[]>& text);

    ABI::Windows::Management::Deployment::PackageOperationPriority GetPriority(HWND hwndDlg);

    PCWSTR FilePath() const
    {
        return m_filePath ? m_filePath.get() : L"";
    }

    PCWSTR PackageFullName() const
    {
        return m_package.PackageFullName() ? m_package.PackageFullName() : L"";
    }

    PCWSTR PackageFamilyName() const
    {
        return m_package.PackageFamilyName() ? m_package.PackageFamilyName() : L"";
    }

    MSIX::SignatureOrigin SignatureOrigin() const
    {
        return m_package.SignatureOrigin();
    }

    PCWSTR CertificateSubject() const
    {
        return m_package.CertificateSubject();
    }

private:
    wil::unique_process_heap_ptr<WCHAR[]> m_filePath{};
    MSIX::Package m_package;
    wil::com_ptr_nothrow<ABI::Windows::Foundation::IUriRuntimeClassFactory> m_uriFactory;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager2> m_packageManager2;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager3> m_packageManager3;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager9> m_packageManager9;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IPackageManager12> m_packageManager12;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions> m_addPackageOptions;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions2> m_addPackageOptions2;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IAddPackageOptions3> m_addPackageOptions3;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IStagePackageOptions> m_stagePackageOptions;
    wil::com_ptr_nothrow<ABI::Windows::Management::Deployment::IStagePackageOptions3> m_stagePackageOptions3;
    InstallAction m_installAction{ InstallAction::Add };

    // Tooltip control created in OnInitDialog; destroyed in OnDestroy. It is a top-level
    // (WS_POPUP) window, so it is not auto-destroyed with the dialog's child controls.
    HWND m_hwndTip{};

    // Tooltip control for the modal certificate dialog (IDD_MSIX_CERTIFICATE); created in
    // CertificateDialogProc's WM_INITDIALOG and destroyed in its WM_DESTROY. Kept separate
    // from m_hwndTip so the page's and the modal dialog's tooltips don't clobber each other.
    HWND m_certificateHwndTip{};

    // Scratch buffer for tooltip text supplied via TTN_GETDISPINFO. The notify
    // struct's own szText is only 80 chars, which truncates longer tooltip
    // strings, so we load the full resource string here and point lpszText at it.
    WCHAR m_tooltipText[512]{};

    // Current status-badge colors, refreshed by UpdatePackageStatus and applied
    // by the owner-draw handler (DrawStatusBadge).
    StatusColor m_registeredStatusColor{ StatusColor::None };
    StatusColor m_removalPendingStatusColor{ StatusColor::None };
};
