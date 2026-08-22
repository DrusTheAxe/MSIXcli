// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "ClassFactory.h"
#include "ModuleReferenceCount.h"
#include "PropertySheetExt.h"

ClassFactory::ClassFactory() : m_refCount(1)
{
    ModuleReferenceCount::Increment();
}

ClassFactory::~ClassFactory()
{
    ModuleReferenceCount::Decrement();
}

// IUnknown methods
IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[]
    {
        QITABENT(ClassFactory, IClassFactory),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release()
{
    const auto cRef{ InterlockedDecrement(&m_refCount) };
    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

// IClassFactory methods
IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    *ppv = nullptr;

    // Aggregation is not supported
    RETURN_HR_IF(CLASS_E_NOAGGREGATION, pUnkOuter != nullptr);

    auto *propertySheet{ new (std::nothrow) PropertySheetExt() };
    RETURN_IF_NULL_ALLOC(propertySheet);

    const auto hr{ LOG_IF_FAILED(propertySheet->QueryInterface(riid, ppv)) };
    propertySheet->Release();
    RETURN_HR(hr);
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
    {
        ModuleReferenceCount::Increment();
    }
    else
    {
        ModuleReferenceCount::Decrement();
    }
    return S_OK;
}
