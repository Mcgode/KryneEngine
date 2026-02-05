/**
 * @file
 * @author Max Godefroy
 * @date 05/02/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    /**
     * @brief A basic contiguous pool, where the freelist is directly integrated into the array
     */
    template<class T, class IndexType = u32, class Allocator = AllocatorInstance>
    requires (sizeof(T) >= sizeof(IndexType))
    class Pool
    {
    public:
        explicit Pool(Allocator _allocator, IndexType _baseCapacity = 8)
            : m_allocator(_allocator)
            , m_pool(m_allocator.template Allocate<T>(_baseCapacity))
            , m_firstFree(kInvalidIndex)
            , m_size(0)
            , m_capacity(_baseCapacity)
        {}

        ~Pool()
        {
            if (m_pool != nullptr)
            {
                m_allocator.deallocate(m_pool);
            }
        }

        T& operator[](IndexType _index) { return m_pool[_index]; }
        const T& operator[](IndexType _index) const { return m_pool[_index]; }

        IndexType Allocate()
        {
            if (m_firstFree == kInvalidIndex)
            {
                if (m_size == m_capacity)
                    Grow();

                return m_size++;
            }
            const IndexType result = m_firstFree;
            m_firstFree = *reinterpret_cast<IndexType*>(m_pool + result);
            return result;
        }

        void Free(IndexType _index)
        {
            KE_ASSERT(_index < m_size);
            *reinterpret_cast<IndexType*>(m_pool + _index) = m_firstFree;
            m_firstFree = _index;
        }

    private:
        static constexpr IndexType kInvalidIndex = ~0ull;

        Allocator m_allocator;
        T* m_pool;
        IndexType m_firstFree;
        IndexType m_size;
        IndexType m_capacity;

        void Grow()
        {
            if (m_capacity == 0)
            {
                m_capacity = 1;
                m_pool = m_allocator.template Allocate<T>(m_capacity);
            }
            else
            {
                T* oldPool = m_pool;
                m_pool = m_allocator.template Allocate<T>(m_capacity * 2);
                if constexpr (std::is_trivially_copyable_v<T>)
                    std::memcpy(m_pool, oldPool, sizeof(T) * m_capacity);
                else
                {
                    for (IndexType i = 0; i < m_capacity; ++i)
                    {
                        new (&m_pool[i]) T(oldPool[i]);
                    }
                }
                m_allocator.deallocate(oldPool);
                m_capacity *= 2;
            }
        }
    };
}