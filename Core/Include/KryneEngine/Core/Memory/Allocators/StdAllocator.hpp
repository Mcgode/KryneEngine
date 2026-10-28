/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#pragma once

#include <cstddef>
#include <type_traits>

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    /**
     * @brief Standard library compatible allocator, forwarding to an `AllocatorInstance`.
     *
     * @details
     * Allows std-allocator-aware containers and libraries (e.g. EnTT) to allocate through an `IAllocator`, keeping
     * their memory tracked like any other engine allocation. A default constructed instance (null `IAllocator`) falls
     * back to the default heap, same as `AllocatorInstance`.
     *
     * The allocator is stateful: two instances are only equal when they wrap the same `IAllocator`, and it propagates
     * with its container on copy, move and swap so memory is always freed by the allocator that provided it.
     */
    template <class T>
    class StdAllocator
    {
    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;
        using is_always_equal = std::false_type;

        StdAllocator() noexcept = default;
        StdAllocator(const AllocatorInstance _allocator) noexcept: m_allocator(_allocator) {}

        template <class U>
        StdAllocator(const StdAllocator<U>& _other) noexcept: m_allocator(_other.GetAllocatorInstance()) {}

        [[nodiscard]] T* allocate(const size_t _count) const
        {
            return m_allocator.Allocate<T>(_count);
        }

        void deallocate(T* _ptr, size_t /* _count */) const
        {
            m_allocator.deallocate(_ptr);
        }

        [[nodiscard]] const AllocatorInstance& GetAllocatorInstance() const noexcept { return m_allocator; }

        template <class U>
        bool operator==(const StdAllocator<U>& _other) const noexcept
        {
            return m_allocator == _other.GetAllocatorInstance();
        }

    private:
        AllocatorInstance m_allocator {};
    };
} // namespace KryneEngine
