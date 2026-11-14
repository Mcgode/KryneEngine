/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include <atomic>
#include <limits>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    template <class T>
    concept IsAllocatorVarIntrusible = requires(T _t)
    { { _t.m_allocator } -> std::convertible_to<AllocatorInstance>; };

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
        {
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

        bool operator==(std::nullptr_t) const noexcept { return m_ptr == nullptr; }
        bool operator==(const IntrusiveUniquePtr& _other) const noexcept { return m_ptr == _other.m_ptr; }

        T* Get() const { return m_ptr; }
        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }

    private:
        T* m_ptr = nullptr;
    };

    /**
     * @brief Shared pointer using a reference count stored in the object itself (`m_refCount`).
     *
     * @details
     * The object is deleted when its last reference is released. A count of 0 alone doesn't mean the object is
     * released: a new object starts at 0 until its first reference. When the last reference goes, the count is moved
     * from 0 to a released marker before deleting, so #TryAcquire can tell the two apart and never revives an object
     * being deleted.
     */
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
        // Takes over the reference of `_other`, without adding one.
        IntrusiveSharedPtr& operator=(IntrusiveSharedPtr&& _other) noexcept
        {
            if (&_other != this)
            {
                Reset();
                m_ptr = _other.m_ptr;
                _other.m_ptr = nullptr;
            }
            return *this;
        }

        /**
         * @brief Takes a reference on `_ptr`, unless its last reference was already released.
         *
         * @details
         * Unlike the constructor, this is safe to call on an object that another thread may be releasing at the same
         * time: either it takes its reference first and the object stays alive, or the object is released first and
         * this returns an empty pointer.
         *
         * @warning The object's memory must stay valid for the duration of the call. The caller must keep the deletion
         * from completing meanwhile, for example by holding a lock that the object's destructor also takes.
         */
        [[nodiscard]] static IntrusiveSharedPtr TryAcquire(T* _ptr)
        {
            IntrusiveSharedPtr result;
            if (_ptr == nullptr)
                return result;

            std::atomic_ref count(_ptr->m_refCount);
            auto current = count.load(std::memory_order::relaxed);
            do
            {
                if (current == ReleasedRefCount())
                    return result;
            }
            while (!count.compare_exchange_weak(
                current,
                current + 1,
                std::memory_order::acquire,
                std::memory_order::relaxed));

            result.m_ptr = _ptr;
            return result;
        }

        void Reset(T* _ptr = nullptr)
        {
            if (m_ptr != nullptr)
            {
                std::atomic_ref refCount(m_ptr->m_refCount);
                const s64 count = refCount.fetch_sub(1, std::memory_order::release) - 1;
                if constexpr (HasReleaseNotifier<T>)
                    m_ptr->OnRelease();

                // At 0, a concurrent `TryAcquire()` may still take a reference: only delete if the count can be moved
                // to the released marker first.
                bool release = count < 0;
                if (count == 0)
                {
                    auto expected = static_cast<decltype(T::m_refCount)>(0);
                    release = refCount.compare_exchange_strong(
                        expected,
                        ReleasedRefCount(),
                        std::memory_order::acq_rel,
                        std::memory_order::relaxed);
                }
                if (release)
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

        /// Count of an object whose last reference was released, so it is being deleted.
        static constexpr auto ReleasedRefCount()
        {
            return std::numeric_limits<decltype(T::m_refCount)>::min();
        }
    };

    template <class T, class... Args> requires IsAllocatorIntrusible<T>
    IntrusiveUniquePtr<T> MakeIntrusiveUniquePtr(AllocatorInstance _allocator, Args&&... _args)
    {
        return IntrusiveUniquePtr<T>(_allocator.New<T>(_allocator, std::forward<Args>(_args)...));
    }

    template <class T, class... Args> requires IsAllocatorIntrusible<T> && IsRefCountIntrusible<T>
    IntrusiveSharedPtr<T> MakeIntrusiveSharedPtr(AllocatorInstance _allocator, Args&&... _args)
    {
        return IntrusiveSharedPtr<T>(_allocator.New<T>(_allocator, std::forward<Args>(_args)...));
    }
}