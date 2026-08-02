// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <windows.foundation.collections.h>

#include <wil/com.h>
#include <wil/result.h>

#include <atomic>
#include <cstdint>
#include <new>
#include <type_traits>

namespace wil::winrt
{
namespace details
{
template <typename T>
struct vector_traits
{
    using logical_type = T*;
    using vector_interface = ABI::Windows::Foundation::Collections::IVector<logical_type>;
    using view_interface = ABI::Windows::Foundation::Collections::IVectorView<logical_type>;
    using iterable_interface = ABI::Windows::Foundation::Collections::IIterable<logical_type>;
    using iterator_interface = ABI::Windows::Foundation::Collections::IIterator<logical_type>;
    using complex_type = typename vector_interface::T_complex;
    using abi_type = typename ABI::Windows::Foundation::Internal::GetAbiType<complex_type>::type;
    using interface_type = std::remove_pointer_t<abi_type>;
};

template <typename T>
class vector_implementation;

template <typename T>
class vector_iterator final : public vector_traits<T>::iterator_interface
{
private:
    using traits = vector_traits<T>;
    using abi_type = typename traits::abi_type;
    using iterator_interface = typename traits::iterator_interface;

public:
    explicit vector_iterator(vector_implementation<T>* owner) noexcept :
        m_owner(owner)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, object);
        *object = nullptr;

        if ((iid == __uuidof(IUnknown)) ||
            (iid == __uuidof(IInspectable)) ||
            (iid == __uuidof(iterator_interface)))
        {
            *object = static_cast<iterator_interface*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++m_references;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto references{ --m_references };
        if (references == 0)
        {
            delete this;
        }
        return references;
    }

    HRESULT STDMETHODCALLTYPE GetIids(ULONG* iidCount, IID** iids) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, iidCount);
        RETURN_HR_IF_NULL(E_POINTER, iids);
        *iidCount = 0;
        *iids = nullptr;

        auto result{ static_cast<IID*>(CoTaskMemAlloc(sizeof(IID))) };
        RETURN_IF_NULL_ALLOC(result);
        result[0] = __uuidof(iterator_interface);
        *iidCount = 1;
        *iids = result;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* className) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, className);
        *className = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* trustLevel) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, trustLevel);
        *trustLevel = BaseTrust;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Current(abi_type* current) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, current);
        *current = nullptr;
        RETURN_HR_IF(E_BOUNDS, m_index >= m_owner->size());
        return m_owner->GetAt(m_index, current);
    }

    HRESULT STDMETHODCALLTYPE get_HasCurrent(boolean* hasCurrent) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, hasCurrent);
        *hasCurrent = m_index < m_owner->size();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MoveNext(boolean* hasCurrent) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, hasCurrent);
        if (m_index < m_owner->size())
        {
            ++m_index;
        }
        *hasCurrent = m_index < m_owner->size();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetMany(
        unsigned capacity,
        abi_type* value,
        unsigned* actual) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, actual);
        *actual = 0;
        RETURN_HR_IF(E_POINTER, (capacity != 0) && !value);
        ZeroMemory(value, sizeof(*value) * capacity);

        while ((*actual < capacity) && (m_index < m_owner->size()))
        {
            const auto hr{ m_owner->GetAt(m_index, &value[*actual]) };
            if (FAILED(hr))
            {
                cleanup(value, *actual);
                *actual = 0;
                return hr;
            }
            ++m_index;
            ++*actual;
        }
        return S_OK;
    }

private:
    static void cleanup(abi_type* value, unsigned count) noexcept
    {
        for (unsigned index = 0; index < count; ++index)
        {
            if (value[index])
            {
                value[index]->Release();
            }
            value[index] = nullptr;
        }
    }

    std::atomic<ULONG> m_references{ 1 };
    wil::com_ptr_nothrow<vector_implementation<T>> m_owner;
    unsigned m_index{};
};

template <typename T>
class vector_implementation final :
    public vector_traits<T>::vector_interface,
    public vector_traits<T>::view_interface,
    public vector_traits<T>::iterable_interface
{
private:
    using traits = vector_traits<T>;
    using abi_type = typename traits::abi_type;
    using vector_interface = typename traits::vector_interface;
    using view_interface = typename traits::view_interface;
    using iterable_interface = typename traits::iterable_interface;
    using iterator_interface = typename traits::iterator_interface;
    using interface_type = typename traits::interface_type;

public:
    constexpr static std::uint32_t default_capacity{ 16 };

    ~vector_implementation() noexcept
    {
        delete[] m_data;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, object);
        *object = nullptr;

        if ((iid == __uuidof(IUnknown)) ||
            (iid == __uuidof(IInspectable)) ||
            (iid == __uuidof(vector_interface)))
        {
            *object = static_cast<vector_interface*>(this);
        }
        else if (iid == __uuidof(view_interface))
        {
            *object = static_cast<view_interface*>(this);
        }
        else if (iid == __uuidof(iterable_interface))
        {
            *object = static_cast<iterable_interface*>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++m_references;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto references{ --m_references };
        if (references == 0)
        {
            delete this;
        }
        return references;
    }

    HRESULT STDMETHODCALLTYPE GetIids(ULONG* iidCount, IID** iids) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, iidCount);
        RETURN_HR_IF_NULL(E_POINTER, iids);
        *iidCount = 0;
        *iids = nullptr;

        constexpr ULONG count{ 3 };
        auto result{ static_cast<IID*>(CoTaskMemAlloc(sizeof(IID) * count)) };
        RETURN_IF_NULL_ALLOC(result);
        result[0] = __uuidof(vector_interface);
        result[1] = __uuidof(view_interface);
        result[2] = __uuidof(iterable_interface);
        *iidCount = count;
        *iids = result;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* className) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, className);
        *className = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* trustLevel) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, trustLevel);
        *trustLevel = BaseTrust;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetAt(unsigned index, abi_type* item) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, item);
        *item = nullptr;
        RETURN_HR_IF(E_BOUNDS, index >= m_size);
        return m_data[index].copy_to(item);
    }

    HRESULT STDMETHODCALLTYPE get_Size(unsigned* size) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, size);
        *size = m_size;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetView(view_interface** view) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, view);
        *view = nullptr;
        return this->QueryInterface(IID_PPV_ARGS(view));
    }

    HRESULT STDMETHODCALLTYPE IndexOf(
        abi_type value,
        unsigned* index,
        boolean* found) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, index);
        RETURN_HR_IF_NULL(E_POINTER, found);

        *index = 0;
        *found = false;

        wil::com_ptr_nothrow<IUnknown> valueIdentity;
        if (value)
        {
            RETURN_IF_FAILED(value->QueryInterface(IID_PPV_ARGS(valueIdentity.put())));
        }

        for (unsigned current = 0; current < m_size; ++current)
        {
            bool matches{ !m_data[current] && !value };
            if (m_data[current] && value)
            {
                wil::com_ptr_nothrow<IUnknown> currentIdentity;
                RETURN_IF_FAILED(m_data[current]->QueryInterface(IID_PPV_ARGS(currentIdentity.put())));
                matches = currentIdentity.get() == valueIdentity.get();
            }

            if (matches)
            {
                *index = current;
                *found = true;
                break;
            }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetAt(unsigned index, abi_type item) noexcept override
    {
        RETURN_HR_IF(E_BOUNDS, index >= m_size);
        m_data[index] = item;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE InsertAt(unsigned index, abi_type item) noexcept override
    {
        RETURN_HR_IF(E_BOUNDS, index > m_size);
        RETURN_HR_IF(E_OUTOFMEMORY, m_size == UINT32_MAX);
        RETURN_IF_FAILED(reserve(m_size + 1));

        for (auto current = m_size; current > index; --current)
        {
            m_data[current] = wistd::move(m_data[current - 1]);
        }
        m_data[index] = item;
        ++m_size;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RemoveAt(unsigned index) noexcept override
    {
        RETURN_HR_IF(E_BOUNDS, index >= m_size);

        for (auto current = index; current + 1 < m_size; ++current)
        {
            m_data[current] = wistd::move(m_data[current + 1]);
        }
        m_data[--m_size].reset();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Append(abi_type item) noexcept override
    {
        RETURN_HR_IF(E_OUTOFMEMORY, m_size == UINT32_MAX);
        RETURN_IF_FAILED(reserve(m_size + 1));
        m_data[m_size++] = item;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RemoveAtEnd() noexcept override
    {
        RETURN_HR_IF(E_BOUNDS, m_size == 0);
        m_data[--m_size].reset();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clear() noexcept override
    {
        while (m_size != 0)
        {
            m_data[--m_size].reset();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetMany(
        unsigned startIndex,
        unsigned capacity,
        abi_type* value,
        unsigned* actual) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, actual);
        *actual = 0;
        RETURN_HR_IF(E_POINTER, (capacity != 0) && !value);
        RETURN_HR_IF(E_BOUNDS, startIndex > m_size);
        ZeroMemory(value, sizeof(*value) * capacity);

        while ((*actual < capacity) && (startIndex + *actual < m_size))
        {
            const auto hr{ m_data[startIndex + *actual].copy_to(&value[*actual]) };
            if (FAILED(hr))
            {
                cleanup(value, *actual);
                *actual = 0;
                return hr;
            }
            ++*actual;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ReplaceAll(unsigned count, abi_type* value) noexcept override
    {
        RETURN_HR_IF(E_POINTER, (count != 0) && !value);
        RETURN_IF_FAILED(reserve(count));

        unsigned index = 0;
        for (; index < count; ++index)
        {
            m_data[index] = value[index];
        }
        for (; index < m_size; ++index)
        {
            m_data[index].reset();
        }
        m_size = count;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE First(iterator_interface** first) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, first);
        *first = nullptr;

        auto iterator{ new (std::nothrow) vector_iterator<T>(this) };
        RETURN_IF_NULL_ALLOC(iterator);
        *first = iterator;
        return S_OK;
    }

    abi_type at(std::uint32_t index) const noexcept
    {
        return m_data[index].get();
    }

    std::uint32_t size() const noexcept
    {
        return m_size;
    }

    std::uint32_t capacity() const noexcept
    {
        return m_capacity;
    }

private:
    static void cleanup(abi_type* value, unsigned count) noexcept
    {
        for (unsigned index = 0; index < count; ++index)
        {
            if (value[index])
            {
                value[index]->Release();
            }
            value[index] = nullptr;
        }
    }

    HRESULT reserve(std::uint32_t requestedCapacity) noexcept
    {
        if (requestedCapacity <= m_capacity)
        {
            return S_OK;
        }

        const auto doubledCapacity{ m_capacity > (UINT32_MAX / 2) ? UINT32_MAX : m_capacity * 2 };
        const auto newCapacity{
            requestedCapacity <= default_capacity ? default_capacity :
            requestedCapacity < doubledCapacity ? doubledCapacity :
            requestedCapacity };

        auto newData{ new (std::nothrow) wil::com_ptr_nothrow<interface_type>[newCapacity] };
        RETURN_IF_NULL_ALLOC(newData);

        for (std::uint32_t index = 0; index < m_size; ++index)
        {
            newData[index] = wistd::move(m_data[index]);
        }
        delete[] m_data;
        m_data = newData;
        m_capacity = newCapacity;
        return S_OK;
    }

    std::atomic<ULONG> m_references{ 1 };
    wil::com_ptr_nothrow<interface_type>* m_data{};
    std::uint32_t m_capacity{};
    std::uint32_t m_size{};
};
}

template <typename T>
class vector
{
private:
    using traits = details::vector_traits<T>;

public:
    using abi_type = typename traits::vector_interface;
    using item_type = typename traits::abi_type;

    vector() noexcept
    {
        m_implementation.attach(new (std::nothrow) details::vector_implementation<T>());
    }

    item_type operator[](size_t index) const noexcept
    {
        return m_implementation->at(static_cast<std::uint32_t>(index));
    }

    operator abi_type*() const noexcept
    {
        return get();
    }

    abi_type* get() const noexcept
    {
        return m_implementation.get();
    }

    std::uint32_t size() const noexcept
    {
        return m_implementation ? m_implementation->size() : 0;
    }

    std::uint32_t capacity() const noexcept
    {
        return m_implementation ? m_implementation->capacity() : 0;
    }

    bool empty() const noexcept
    {
        return size() == 0;
    }

    void clear() noexcept
    {
        if (m_implementation)
        {
            m_implementation->Clear();
        }
    }

    HRESULT push_back(item_type item) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, item);
        RETURN_IF_NULL_ALLOC(m_implementation);
        return m_implementation->Append(item);
    }

    HRESULT insert(size_t index, item_type item) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, item);
        RETURN_HR_IF(E_BOUNDS, index > UINT32_MAX);
        RETURN_IF_NULL_ALLOC(m_implementation);
        return m_implementation->InsertAt(static_cast<std::uint32_t>(index), item);
    }

private:
    wil::com_ptr_nothrow<details::vector_implementation<T>> m_implementation;
};
}
