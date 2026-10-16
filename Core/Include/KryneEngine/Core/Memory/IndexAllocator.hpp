/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#pragma once

#include <EASTL/vector.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    template <class T = u32, T Invalid = 0xFFFFFFFFu>
    class IndexAllocatorT
    {
    public:
        IndexAllocatorT() = default;

        void Initialize(AllocatorInstance _allocator, T _max)
        {
            m_max = _max;
            m_freeIndices.set_allocator(_allocator);
        }

        T Allocate()
        {
            if (m_freeIndices.empty())
            {
                T value = m_totalAllocated++;
                return value < m_max ? value : Invalid;
            }
            T value = m_freeIndices.back();
            m_freeIndices.pop_back();
            return value;
        }
        void Free(T _index)
        {
            m_freeIndices.push_back(_index);
        }

        static constexpr T InvalidIndex() { return Invalid; }

    private:
        eastl::vector<T> m_freeIndices;
        T m_totalAllocated = 0;
        T m_max = 0;
    };

    using IndexAllocator = IndexAllocatorT<>;
} // namespace KryneEngine
