/**
 * @file
 * @author Max Godefroy
 * @date 16/10/2026.
 */

#pragma once

#include <EASTL/vector.h>

#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"
#include "KryneEngine/Core/Threads/SpinLock.hpp"


namespace KryneEngine
{
    /**
     * @brief Allocator managing ranges of an external memory space.
     *
     * @details
     * The managed memory is never accessed by the allocator, only its bookkeeping data is stored. Ranges are
     * expressed as offsets and sizes in whatever unit the user of the allocator chooses (bytes, descriptors, ...).
     *
     * The managed space starts empty and grows on demand, by multiples of the block size provided at construction.
     * When no free range can satisfy an allocation, a new block is appended at the end of the managed space and the
     * allocation is carved out of it. The allocator never shrinks.
     *
     * Free ranges are kept sorted by start offset, and adjacent free ranges are merged on deallocation. Merging never
     * crosses a block boundary, so an allocation always lies entirely within a single block. This allows each block
     * to be backed by a separate external resource. Allocations use a first-fit strategy, with linear cost in the
     * number of free ranges.
     *
     * Allocate() and Deallocate() are thread-safe.
     */
    class ExternalRangeAllocator
    {
    public:
        /**
         * @param _allocator Allocator used for the bookkeeping data.
         * @param _blockSize Size by which the managed space grows when it runs out of free space. This is also the
         * maximum size of a single allocation.
         */
        ExternalRangeAllocator(AllocatorInstance _allocator, size_t _blockSize);
        ~ExternalRangeAllocator() = default;

        KE_DEFINE_COPY_MOVE_SEMANTICS(ExternalRangeAllocator, delete, delete);

        /**
         * @brief Reserves a range in the managed space.
         *
         * @details
         * The managed space is grown by one block if no existing free range is large enough, so this never fails.
         *
         * @param _size Size of the range to reserve. Must not exceed the block size.
         * @return The start offset of the reserved range.
         */
        [[nodiscard]] size_t Allocate(size_t _size);

        /**
         * @brief Releases a range previously returned by Allocate().
         *
         * @param _start Start offset of the range, as returned by Allocate().
         * @param _size Size of the range, as passed to Allocate(). Releasing only a part of an allocation is not
         * supported.
         */
        void Deallocate(size_t _start, size_t _size);

    private:
        struct FreeRange
        {
            size_t m_start;
            size_t m_size;
        };

        SpinLock m_lock;
        size_t m_blockSize;
        size_t m_size = 0;
        eastl::vector<FreeRange> m_freeRanges;
    };
} // namespace KryneEngine
