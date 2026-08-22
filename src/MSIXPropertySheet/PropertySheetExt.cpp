// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "PropertySheetExt.h"
#include "MSIXPropertyPage.h"

#include "ModuleReferenceCount.h"

PropertySheetExt::PropertySheetExt() : m_refCount(1)
{
    ModuleReferenceCount::Increment();
}

PropertySheetExt::~PropertySheetExt()
{
    ModuleReferenceCount::Decrement();
}

// IUnknown methods
IFACEMETHODIMP PropertySheetExt::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[]
    {
        QITABENT(PropertySheetExt, IShellExtInit),
        QITABENT(PropertySheetExt, IShellPropSheetExt),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) PropertySheetExt::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) PropertySheetExt::Release()
{
    const auto cRef{ InterlockedDecrement(&m_refCount) };
    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

// IShellExtInit method
IFACEMETHODIMP PropertySheetExt::Initialize(LPCITEMIDLIST /*pidlFolder*/ , IDataObject* pdtobj, HKEY /*hkeyProgID*/)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pdtobj);

    FORMATETC fe{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    wil::unique_stg_medium storageMedium{};

    // Get the file path from the data object
    RETURN_IF_FAILED(pdtobj->GetData(&fe, &storageMedium));

    wil::unique_hglobal_locked storageMedium_hGlobal{ storageMedium.hGlobal };
    RETURN_LAST_ERROR_IF_NULL(storageMedium.hGlobal);
    auto hDrop{ static_cast<HDROP>(storageMedium.hGlobal) };

    const auto nFiles{ DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0) };
    if (nFiles == 1)  // We only handle single file selection
    {
        wchar_t szFileName[MAX_PATH]{};
        if (DragQueryFile(hDrop, 0, szFileName, ARRAYSIZE(szFileName)))
        {
            const size_t cch{ ::wcslen(szFileName) + 1 };
            m_filePath.reset(static_cast<WCHAR*>(::HeapAlloc(::GetProcessHeap(), 0, cch * sizeof(WCHAR))));
            RETURN_IF_NULL_ALLOC(m_filePath);
            ::wcscpy_s(m_filePath.get(), cch, szFileName);
        }
    }

    return S_OK;
}

// IShellPropSheetExt methods
IFACEMETHODIMP PropertySheetExt::AddPages(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam)
{
    // Allocate a private copy of the file path that the new page will own
    wil::unique_process_heap_ptr<WCHAR[]> pagePath{};
    if (m_filePath)
    {
        const size_t cch{ ::wcslen(m_filePath.get()) + 1 };
        pagePath.reset(static_cast<WCHAR*>(::HeapAlloc(::GetProcessHeap(), 0, cch * sizeof(WCHAR))));
        RETURN_IF_NULL_ALLOC(pagePath);
        ::wcscpy_s(pagePath.get(), cch, m_filePath.get());
    }

    // Create the property page
    auto *pPage = new (std::nothrow) MSIXPropertyPage(std::move(pagePath));
    RETURN_IF_NULL_ALLOC(pPage);

    HPROPSHEETPAGE hPage{ pPage->CreatePropertyPage() };
    RETURN_IF_NULL_ALLOC(hPage);

    // Add the page to the property sheet
    if (pfnAddPage(hPage, lParam))
    {
        AddRef();
    }
    else
    {
        DestroyPropertySheetPage(hPage);
    }
    // Note: We don't delete pPage here because it's owned by the property sheet

    return S_OK;
}

IFACEMETHODIMP PropertySheetExt::ReplacePage(UINT /*uPageID*/, LPFNADDPROPSHEETPAGE /*pfnReplacePage*/, LPARAM /*lParam*/)
{
    // We don't replace any pages
    return E_NOTIMPL;
}
