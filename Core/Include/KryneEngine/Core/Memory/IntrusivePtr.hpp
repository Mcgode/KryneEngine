/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include <atomic>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    template <class T>
    concept IsAllocatorVarIntrusible = requires(T)
    { { T::m_allocator } -> std::same_as<AllocatorInstance>; };

    template <class T>
    concept IsAllocatorGetterIntrusible = requires(T _t)
    { { _t.GetAllocator() } -> std::convertible_to<AllocatorInstance>; };

    template <class T>
    concept IsAllocatorIntrusible = IsAllocatorVarIntrusible<T> || IsAllocatorGetterIntrusible<T>;

    template <class T>
    concept IsRefCountIntrusible = requires(T _t)
    {
        _t.m_refCount;
    }
    && std::is_integral_v<decltype(T::m_refCount)>
    && sizeof(T::m_refCount) >= sizeof(u32);

    template <class T>
    concept HasReleaseNotifier = requires(T _t) { _t.OnRelease(); };

    template <class T> requires IsAllocatorIntrusible<T> || IsIncomplete<T>
    class IntrusiveUniquePtr
    {
    public:
        IntrusiveUniquePtr() = default;
        explicit IntrusiveUniquePtr(std::nullptr_t) noexcept {}
        explicit IntrusiveUniquePtr(T* _ptr) : m_ptr(_ptr) {}
        ~IntrusiveUniquePtr() { Reset(); }

        IntrusiveUniquePtr(const IntrusiveUniquePtr& _other) = delete;
        IntrusiveUniquePtr& operator=(const IntrusiveUniquePtr& _other) = delete;

        IntrusiveUniquePtr(IntrusiveUniquePtr&& _other) noexcept: m_ptr(_other.m_ptr) { _other.m_ptr = nullptr; }
        IntrusiveUniquePtr& operator=(IntrusiveUniquePtr&& _other) noexcept { Reset(_other.m_ptr); _other.m_ptr = nullptr; return *this; }

        void Reset(T* _ptr = nullptr)
        { m_ptr = _ptr;
            if (m_ptr != nullptr)
            {
                if constexpr (IsAllocatorVarIntrusible<T>)
                {
                    m_ptr->m_allocator.Delete(m_ptr);
                }
                else if constexpr (IsAllocatorGetterIntrusible<T>)
                {
                    m_ptr->GetAllocator().Delete(m_ptr);
                }
            }
            m_ptr = _ptr;
        }

        IntrusiveUniquePtr& operator=(std::nullptr_t) noexcept { Reset(); return *this; }
        IntrusiveUniquePtr& operator=(T* _ptr) { Reset(_ptr); return *this; }

        T* Get() const { return m_ptr; }
        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }

    private:
        T* m_ptr = nullptr;
    };

    template <class T> requires (IsAllocatorIntrusible<T> && IsRefCountIntrusible<T>) || IsIncomplete<T>
    class IntrusiveSharedPtr
    {
    public:
        explicit IntrusiveSharedPtr(T* _ptr = nullptr) : m_ptr(_ptr)
        {
            if (m_ptr != nullptr)
                std::atomic_ref(m_ptr->m_refCount).fetch_add(1, std::memory_order::acquire);
        }
        ~IntrusiveSharedPtr() { Reset(); }

        IntrusiveSharedPtr(const IntrusiveSharedPtr& _other): IntrusiveSharedPtr(_other.m_ptr) {}
        IntrusiveSharedPtr& operator=(const IntrusiveSharedPtr& _other) { if (&_other != this) Reset(_other.m_ptr); return *this; }

        IntrusiveSharedPtr(IntrusiveSharedPtr&& _other) noexcept: m_ptr(_other.m_ptr) { _other.m_ptr = nullptr; }
        IntrusiveSharedPtr& operator=(IntrusiveSharedPtr&& _other) noexcept { Reset(_other.m_ptr); _other.m_ptr = nullptr; return *this; }

        void Reset(T* _ptr = nullptr)
        {
            if (m_ptr != nullptr)
            {
                const s64 count = std::atomic_ref(m_ptr->m_refCount).fetch_sub(1, std::memory_order::release) - 1;
                if constexpr (HasReleaseNotifier<T>)
                    m_ptr->OnRelease();
                if (count <= 0)
                {
                    if constexpr (IsAllocatorVarIntrusible<T>)
                    {
                        m_ptr->m_allocator.Delete(m_ptr);
                    }
                    else if constexpr (IsAllocatorGetterIntrusible<T>)
                    {
                        m_ptr->GetAllocator().Delete(m_ptr);
                    }
                }
            }
            m_ptr = _ptr;
            if (m_ptr != nullptr)
                std::atomic_ref(m_ptr->m_refCount).fetch_add(1, std::memory_order::acquire);
        }

        T* Get() const { return m_ptr; }
        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }

    private:
        T* m_ptr = nullptr;
    };

    template <class T, class... Args> requires IsAllocatorIntrusible<T>
    IntrusiveUniquePtr<T>&& MakeIntrusiveUniquePtr(AllocatorInstance _allocator, Args... _args)
    {
        return std::move(IntrusiveUniquePtr<T>(_allocator.New<T>(_allocator, _args...)));
    }

    template <class T, class... Args> requires IsAllocatorIntrusible<T> && IsRefCountIntrusible<T>
    IntrusiveSharedPtr<T>&& MakeIntrusiveSharedPtr(AllocatorInstance _allocator, Args... _args)
    {
        return std::move(IntrusiveSharedPtr<T>(_allocator.New<T>(_allocator, _args...)));
    }
}