/**
 * @file
 * @author Max Godefroy
 * @date 08/12/2025.
 */

#pragma once

#include "StableVector.hpp"

namespace KryneEngine
{
    template <class T, size_t BlockSize, class Allocator>
    T& StableVector<T, BlockSize, Allocator>::PushBack(const T& _value)
    {
        T& entry = NextEntry();
        new (&entry) T(_value);
        return entry;
    }

    template <class T, size_t BlockSize, class Allocator>
    T& StableVector<T, BlockSize, Allocator>::PushBack(T&& _value)
    {
        T& entry = NextEntry();
        new (&entry) T(std::move(_value));
        return entry;
    }

    template <class T, size_t BlockSize, class Allocator>
    template <class... Args>
    T& StableVector<T, BlockSize, Allocator>::EmplaceBack(Args... _args)
    {
        T& entry = NextEntry();
        new (&entry) T(_args...);
        return entry;
    }

    template <class T, size_t BlockSize, class Allocator>
    T* StableVector<T, BlockSize, Allocator>::BulkAllocate(size_t _count)
    {
        KE_ASSERT_MSG(_count <= BlockSize, "Cannot allocate more than %u elements in a single block!", BlockSize);

        if (_count == 0)
            return nullptr;

        const size_t leftOver = m_size % BlockSize;

        if (leftOver < _count)
        {
            m_size += leftOver;
        }

        T* begin = &NextEntry();
        m_size += _count - 1;

        return begin;
    }

    template <class T, size_t BlockSize, class Allocator>
    void StableVector<T, BlockSize, Allocator>::Clear()
    {
        if (m_firstBlock == nullptr)
            return;

        Block* block = m_firstBlock;
        while (block != nullptr)
        {
            Block* nextBlock = block->m_next;
            m_allocator.deallocate(block, BlockSize);
            block = nextBlock;
        }
        m_firstBlock = nullptr;
        m_lastBlock = nullptr;
        m_size = 0;
    }

    template <class T, size_t BlockSize, class Allocator>
    StableVector<T, BlockSize, Allocator>::~StableVector()
    {
        Clear();
    }

    template <class T, size_t BlockSize, class Allocator>
    T& StableVector<T, BlockSize, Allocator>::NextEntry()
    {
        const size_t localIndex = m_size % BlockSize;

        if (m_firstBlock == nullptr)
        {
            m_firstBlock = static_cast<Block*>(m_allocator.allocate(sizeof(Block), alignof(Block)));
            m_firstBlock->m_next = nullptr;
            m_lastBlock = m_firstBlock;
        }
        else if (localIndex == 0)
        {
            auto* newBlock = static_cast<Block*>(m_allocator.allocate(sizeof(Block), alignof(Block)));
            newBlock->m_next = nullptr;
            m_lastBlock->m_next = newBlock;
            m_lastBlock = newBlock;
        }

        m_size++;

        return m_lastBlock->m_data[localIndex];
    }
} // namespace KryneEngine