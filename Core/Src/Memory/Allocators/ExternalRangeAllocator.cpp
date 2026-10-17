/**
 * @file
 * @author Max Godefroy
 * @date 16/10/2026.
 */

#include "KryneEngine/Core/Memory/Allocators/ExternalRangeAllocator.hpp"

#include <EASTL/algorithm.h>

#include "KryneEngine/Core/Common/Assert.hpp"

namespace KryneEngine
{
    ExternalRangeAllocator::ExternalRangeAllocator(const AllocatorInstance _allocator, const size_t _blockSize)
        : m_blockSize(_blockSize)
        , m_freeRanges(_allocator)
    {}

    size_t ExternalRangeAllocator::Allocate(const size_t _size)
    {
        KE_ASSERT(_size > 0);
        KE_ASSERT(_size <= m_blockSize);

        const auto lock = m_lock.AutoLock();

        for (auto it = m_freeRanges.begin(); it != m_freeRanges.end(); ++it)
        {
            if (it->m_size > _size)
            {
                const size_t index = it->m_start;
                it->m_size -= _size;
                it->m_start += _size;
                return index;
            }
            if (it->m_size == _size)
            {
                const size_t index = it->m_start;
                m_freeRanges.erase(it);
                return index;
            }
        }

        const size_t index = m_size;
        m_size += m_blockSize;

        if (_size < m_blockSize)
        {
            m_freeRanges.emplace_back(index + _size, m_blockSize - _size);
        }

        return index;
    }

    void ExternalRangeAllocator::Deallocate(const size_t _start, const size_t _size)
    {
        KE_ASSERT(_size > 0);
        KE_ASSERT(_start / m_blockSize == (_start + _size - 1) / m_blockSize);

        const auto lock = m_lock.AutoLock();

        KE_ASSERT(_start + _size <= m_size);

        auto* it = eastl::lower_bound(
            m_freeRanges.begin(),
            m_freeRanges.end(),
            _start,
            [](const FreeRange& _range, const size_t _value) { return _range.m_start < _value; });

        const size_t index = eastl::distance(m_freeRanges.begin(), it);

        bool previousMerged = false;
        if (index > 0)
        {
            FreeRange& previous = m_freeRanges[index - 1];
            KE_ASSERT(previous.m_start + previous.m_size <= _start);
            if (previous.m_start + previous.m_size == _start && _start % m_blockSize != 0)
            {
                previous.m_size += _size;
                previousMerged = true;
            }
        }

        bool nextMerged = false;
        if (index < m_freeRanges.size())
        {
            FreeRange& next = m_freeRanges[index];
            KE_ASSERT(next.m_start >= _start + _size);
            if (next.m_start == _start + _size && next.m_start % m_blockSize != 0)
            {
                if (previousMerged)
                {
                    m_freeRanges[index - 1].m_size += next.m_size;
                    m_freeRanges.erase(m_freeRanges.begin() + index);
                }
                else
                {
                    next.m_start = _start;
                    next.m_size += _size;
                }
                nextMerged = true;
            }
        }

        if (!previousMerged && !nextMerged)
        {
            m_freeRanges.insert(it, FreeRange{ .m_start = _start, .m_size = _size });
        }
    }
} // namespace KryneEngine
