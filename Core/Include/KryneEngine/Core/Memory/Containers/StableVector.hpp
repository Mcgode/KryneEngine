/**
 * @file
 * @author Max Godefroy
 * @date 08/12/2025.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine
{
    /**
     * @brief A vector container that maintains stable memory addresses for its elements
     */
    template <class T, size_t BlockSize = 64, class Allocator = AllocatorInstance>
    class StableVector
    {
    public:
        StableVector() = default;
        explicit StableVector(const Allocator& _allocator): m_allocator(_allocator) {}

        T& PushBack(const T& _value);
        T& PushBack(T&& _value);

        template<class... Args>
        T& EmplaceBack(Args... _args);

        /**
         * @brief Allocates a contiguous block of memory for multiple elements without constructing them.
         *
         * @details
         * If the current block doesn't have enough space for the requested count, it is padded so that a new block is
         * allocated.
         */
        T* BulkAllocate(size_t _count);

        void Clear();
        ~StableVector();

    private:
        struct Block
        {
            T m_data[BlockSize];
            Block* m_next = nullptr;
        };

        Allocator m_allocator {};
        Block* m_firstBlock = nullptr;
        Block* m_lastBlock = nullptr;
        size_t m_size = 0;

        T& NextEntry();
    };
} // namespace KryneEngine
