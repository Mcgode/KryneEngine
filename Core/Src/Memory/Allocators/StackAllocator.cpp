/**
 * @file
 * @author Max Godefroy
 * @date 18/03/2026.
 */

#include "KryneEngine/Core/Memory/Allocators/StackAllocator.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"

namespace KryneEngine
{
    StackAllocator::StackAllocator(
        const AllocatorInstance _parentAllocator,
        const size_t _initialSize,
        const size_t _maxExtraHeapCount,
        const char* _name)
            : IAllocator(_name)
            , m_parentAllocator(_parentAllocator)
            , m_baseHeapSize(_initialSize)
            , m_heaps(_parentAllocator, 1 + _maxExtraHeapCount, nullptr)
    {
        KE_ASSERT_MSG(Alignment::IsPowerOfTwo(_initialSize), "Initial size must be a power of two");
        m_heaps[0] = static_cast<std::byte*>(_parentAllocator.allocate(_initialSize, alignof(size_t)));
    }

    void* StackAllocator::Allocate(size_t _size, size_t _alignment)
    {
        const size_t initialIndex = m_stackIndex;
        size_t heapIndex = 0;
        size_t heapOffset = m_stackIndex;
        if (m_stackIndex >= m_baseHeapSize)
        {
            heapIndex = BitUtils::GetMostSignificantBit(m_stackIndex / m_baseHeapSize);
            heapOffset = m_stackIndex - m_baseHeapSize * ((1 << (heapIndex - 1)) - 1);
        }
        size_t heapSize = m_baseHeapSize << heapIndex;

        if (m_heaps[heapIndex] == nullptr)
            m_heaps[heapIndex] = static_cast<std::byte*>(m_parentAllocator.allocate(heapSize, alignof(size_t)));

        auto heapStart = reinterpret_cast<size_t>(m_heaps[heapIndex]);
        size_t position = heapStart + heapOffset;
        size_t alignedPosition = Alignment::AlignUp(position, _alignment);
        m_stackIndex += alignedPosition - position;

        while (alignedPosition + _size > heapSize + heapStart)
        {
            if (heapIndex + 1 == m_heaps.Size())
            {
                m_stackIndex = initialIndex;
                return nullptr;
            }

            heapIndex++;

            if (m_heaps[heapIndex] == nullptr)
                m_heaps[heapIndex] = static_cast<std::byte*>(m_parentAllocator.allocate(heapSize, alignof(size_t)));

            m_stackIndex = m_baseHeapSize * ((1 << (heapIndex)) - 1);
            heapStart = reinterpret_cast<size_t>(m_heaps[heapIndex]);
            heapOffset = 0;
            position = heapStart + heapOffset;
            alignedPosition = Alignment::AlignUp(position, _alignment);
            m_stackIndex += alignedPosition - position;
        }

        m_stackIndex += _size;
        return reinterpret_cast<void*>(alignedPosition);
    }

    void StackAllocator::Pop(const size_t _toIndex)
    {
        m_stackIndex = _toIndex;
    }

    void StackAllocator::Reset()
    {
        m_stackIndex = 0;
        for (size_t i = 1; i < m_heaps.Size(); i++)
        {
            m_parentAllocator.deallocate(m_heaps[i]);
            m_heaps[i] = nullptr;
        }
    }
}
