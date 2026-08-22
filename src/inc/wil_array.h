// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_fastdeletelastptr
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_fastgetptr
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_getptrindex
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_getptrptr
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_search
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_setptr
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_setptrcount
//TODO https://learn.microsoft.com/windows/win32/api/dpa_dsa/nf-dpa_dsa-dpa_sortedinsertptr

#pragma once

#include <limits>

#include <dpa_dsa.h>

namespace wil
{
template <typename T>
class pointer_array
{
public:
    const int npos{ -1 };

public:
    pointer_array(PFNDAENUMCALLBACK deleter, void* deleterData = nullptr, int growth = 16)
    {
        m_array = ::DPA_Create(growth);
        m_deleter = deleter;
    }

    pointer_array(int growth = 16)
    {
        m_array = ::DPA_Create(growth);
    }

    ~pointer_array()
    {
        reset();
    }

public:
    bool is_valid() const
    {
        return m_array != nullptr;
    }

    operator bool() const
    {
        return m_array != nullptr;
    }

    int capacity() const
    {
        return DPA_GetPtrCount(m_array);
    }

    bool grow(int desiredCapacity)
    {
        return !!::DPA_Grow(m_array, desiredCapacity);
    }

    std::uint64_t size() const
    {
        return ::DPA_GetSize(m_array);
    }

    bool empty() const
    {
        return size() == 0;
    }

    T* operator[](int index)
    {
        return ::DPA_GetPtr(m_array, index);
    }

    const T* operator[](int index) const
    {
        return ::DPA_GetPtr(m_array, index);
    }

    T* at(int index)
    {
        return operator[](index);
    }

    const T* at(int index) const
    {
        return operator[](index);
    }

    bool set(int index, T* item)
    {
        return !!DPA_SetPtr(m_array, index, item);
    }

    T* front()
    {
        return empty() ? nullptr : operator[](0);
    }

    const T* front() const
    {
        return empty() ? nullptr : operator[](0);
    }

    T* back()
    {
        const auto size{ size() };
        return size == 0 ? nullptr : operator[](size - 1);
    }

    const T* back() const
    {
        const auto size{ size() };
        return size == 0 ? nullptr : operator[](size - 1);
    }

public:
    int append(T* item)
    {
        return ::DPA_AppendPtr(m_array, item);
    }

    int insert(int pos, T* item)
    {
        return ::DPA_InsertPtr(m_array, pos, item);
    }

    bool clear()
    {
        return !!::DPA_DeleteAllPtrs(m_array);
    }

    T* detach(int index)
    {
        return ::DPA_DeletePtr(m_array, index);
    }

    bool reset()
    {
        if (m_array)
        {
            if (m_deleter)
            {
                ::DPA_DestroyCallback(m_array, m_deleter, m_deleterData);
            }
            else
            {
                if (!::DPA_Destroy(m_array))
                {
                    return false;
                }
            }
            m_array = nullptr;
        }
        return true;
    }

    bool sort(PFNDPACOMPARE comparator, LPARAM data)
    {
        return !!::DPA_Sort(m_array, comparator, data);
    }

    bool sort(PFNDACOMPARECONST comparator, LPARAM data) const
    {
        return !!::DPA_Sort(m_array, comparator, data);
    }

    void for_each(PFNDPAENUMCALLBACK enumerator, void* data)
    {
        DPA_EnumCallback(m_array, enumerator, data);
    }

    void for_each(PFNDPAENUMCALLBACKCONST enumerator, void* data) const
    {
        DPA_EnumCallback(m_array, enumerator, data);
    }

private:
    HDPA m_array{};
    PFNDAENUMCALLBACK m_deleter{};
    void* m_deleterData{};
};
}
