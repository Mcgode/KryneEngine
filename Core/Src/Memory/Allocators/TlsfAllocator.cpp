/**
 * @file
 * @author Max Godefroy
 * @date 10/02/2025.
 */

#include "KryneEngine/Core/Memory/Allocators/TlsfAllocator.hpp"

#include <cstring>
#include <stdexcept>

#include "KryneEngine/Core/Common/BitUtils.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Memory/Heaps/TlsfHeap.hpp"

#if !defined(TLSF_HEAP_ASSERT)
#   define TLSF_HEAP_ASSERT 1
#endif

#if TLSF_HEAP_ASSERT
#   define TLSF_ASSERT(cond) KE_ASSERT_FATAL(cond)
#   define TLSF_ASSERT_MSG(cond, ...) KE_ASSERT_FATAL_MSG(cond, __VA_ARGS__)
#else
#   define TLSF_ASSERT(cond)
#   define TLSF_ASSERT_MSG(cond, ...)
#endif

namespace KryneEngine
{
    TlsfAllocator::~TlsfAllocator()
    {
        HeapLink nextHeap = m_nextHeap;
        while (nextHeap.m_next != nullptr)
        {
            HeapLink* current = nextHeap.m_next;
            nextHeap = *nextHeap.m_next;
            m_parentAllocator.deallocate(current, m_heapSize);
        }
    }

    void* TlsfAllocator::Allocate(size_t _size, size_t _alignment)
    {
        TlsfHeap::BlockHeader* block = nullptr;

        if (_size == 0)
            return nullptr;

        size_t adjusted = Alignment::AlignUp(_size, TlsfHeap::kAlignment);
        adjusted = eastl::max<size_t>(adjusted, TlsfHeap::kMinBlockSize);
        size_t alignedSize = adjusted;

        const size_t usableHeapSize = m_heapSize - TlsfHeap::kHeapPoolOverhead - sizeof(void*);
        if (adjusted > usableHeapSize)
            return nullptr;

        constexpr size_t gapMinimum = sizeof(TlsfHeap::BlockHeader);
        if (_alignment > TlsfHeap::kAlignment)
        {
            /*
             * We must allocate an additional minimum block size bytes so that if
             * our free block will leave an alignment gap which is smaller, we can
             * trim a leading free block and release it back to the pool. We must
             * do this because the previous physical block is in use, therefore
             * the prev_phys_block field is not valid, and we can't simply adjust
             * the size of that block.
             */
            alignedSize = Alignment::AlignUp(_size + gapMinimum + _alignment, _alignment);
        }

        auto [fl, sl] = MappingSearch(alignedSize);
        block = SearchHeader(_size, fl, sl);

        if (block == nullptr)
        {
            // Try to auto-grow the allocator by adding a heap
            if (!m_autoGrowth || !AddHeap())
                return nullptr;
            eastl::pair<u8, u8> pair = MappingSearch(alignedSize);
            fl = pair.first; sl = pair.second;
            block = SearchHeader(_size, fl, sl);

            if (block == nullptr)
                return nullptr;
        }

        TLSF_ASSERT(block->GetSize() >= _size);
        RemoveBlock(block, fl, sl);

        if (_alignment > TlsfHeap::kAlignment)
        {
            const auto ptr = reinterpret_cast<uintptr_t>(TlsfHeap::BlockHeaderToUserPtr(block));
            uintptr_t aligned = Alignment::AlignUp(ptr, _alignment);
            size_t gap = aligned - ptr;

            if (gap && gap < gapMinimum)
            {
                const size_t offset = Alignment::AlignUp(gapMinimum - gap, _alignment);
                aligned += offset;
                gap = aligned - ptr;
            }

            if (gap)
            {
                TLSF_ASSERT(gap >= gapMinimum);

                TlsfHeap::BlockHeader* remaining = block;
                if (CanSplit(block, gap))
                {
                    remaining = SplitBlock(block, gap - TlsfHeap::kBlockHeaderOverhead);
                    remaining->SetPrevFree();

                    LinkNext(block);
                    InsertBlock(block);
                }
                block = remaining;
            }
        }

        return PrepareBlockUsed(block, alignedSize);
    }

    void TlsfAllocator::Free(void* _ptr, size_t /* _size */)
    {
        if (_ptr == nullptr)
            return;

        TlsfHeap::BlockHeader* block = TlsfHeap::UserPtrToBlockHeader(_ptr);
        TLSF_ASSERT_MSG(!block->IsFree(), "Block must not be free");
        MarkAsFree(block);
        block = MergePreviousBlock(block);
        block = MergeNextBlock(block);
        InsertBlock(block);
    }

    TlsfAllocator* TlsfAllocator::Create(AllocatorInstance _parentAllocator, size_t _heapSize, const char* _name)
    {
        const u32 allocatorSize = Alignment::AlignUp(sizeof(TlsfAllocator), TlsfHeap::kAlignment);
        KE_ASSERT_MSG(_heapSize > allocatorSize, "Heap size must be greater than the size of the control structures");

        auto* heapStart = reinterpret_cast<std::byte*>(_parentAllocator.allocate(_heapSize, TlsfHeap::kAlignment));
        IF_NOT_VERIFY_MSG(heapStart != nullptr, "Failed to allocate memory for the allocator")
        {
            return nullptr;
        }

        auto* allocator = reinterpret_cast<TlsfAllocator*>(heapStart);
        new (allocator) TlsfAllocator(_parentAllocator, _heapSize, allocatorSize, _name);
        heapStart += allocator->m_allocatorSize;
        _heapSize -= allocator->m_allocatorSize;

        TlsfHeap::ControlBlock* control = allocator->GetControlBlock();
        memset(control, 0, offsetof(TlsfHeap::ControlBlock, m_headerMap));
        for (auto& headerList : control->m_headerMap)
        {
            for (auto& header : headerList)
            {
                header = &control->m_nullBlock;
            }
        }

        allocator->SetupHeapPool(heapStart + sizeof(TlsfHeap::ControlBlock), _heapSize - sizeof(TlsfHeap::ControlBlock));
        return allocator;
    }

    void TlsfAllocator::Destroy(TlsfAllocator* _allocator)
    {
        VERIFY_OR_RETURN_VOID(_allocator != nullptr);

        _allocator->~TlsfAllocator();
        _allocator->m_parentAllocator.deallocate(_allocator, _allocator->m_heapSize);
    }

    bool TlsfAllocator::AddHeap()
    {
        HeapLink* lastHeap = &m_nextHeap;
        while (lastHeap->m_next != nullptr)
        {
            lastHeap = lastHeap->m_next;
        }

        auto* newHeapStart = reinterpret_cast<std::byte*>(m_parentAllocator.allocate(m_heapSize, TlsfHeap::kAlignment));
        if (newHeapStart == nullptr)
            return false;

        auto* newHeapLink = reinterpret_cast<HeapLink*>(newHeapStart);
        lastHeap->m_next = newHeapLink;
        newHeapLink->m_next = nullptr;
        newHeapStart += sizeof(HeapLink);
        SetupHeapPool(newHeapStart, m_heapSize - sizeof(void*));
        return true;
    }

    TlsfAllocator::TlsfAllocator(
        const AllocatorInstance _parentAllocator,
        const size_t _heapSize,
        const u32 _allocatorSize,
        const char* _name)
            : IAllocator(_name)
            , m_parentAllocator(_parentAllocator)
            , m_heapSize(_heapSize)
            , m_allocatorSize(_allocatorSize)
    {}

    void TlsfAllocator::SetupHeapPool(std::byte* _heapStart, size_t _heapSize)
    {
        using namespace TlsfHeap;

        const size_t heapPoolBytes = Alignment::AlignDownPot(_heapSize - kHeapPoolOverhead, kAlignmentPot);

        TLSF_ASSERT_MSG(
            Alignment::IsAligned(reinterpret_cast<uintptr_t>(_heapStart), kAlignment),
            "Heap start must be aligned to %lld bytes", kAlignment);

        TLSF_ASSERT_MSG(
            heapPoolBytes >= kMinBlockSize && heapPoolBytes <= kMaxBlockSize,
            "Heap pool size must be contained between 0x%x and 0x%x00 bytes",
            kHeapPoolOverhead + kMinBlockSize,
            (kHeapPoolOverhead + kMaxBlockSize) / 256
            );

        auto* block = reinterpret_cast<BlockHeader*>(_heapStart - kBlockHeaderMemoryAddressLeftOffset);
        block->SetSize(heapPoolBytes);
        block->SetFree();
        block->SetPrevUsed();
        InsertBlock(block);

        BlockHeader* next = LinkNext(block);
        next->SetSize(0);
        next->SetUsed();
        next->SetPrevFree();
    }

    void TlsfAllocator::InsertBlock(TlsfHeap::BlockHeader* _block)
    {
        TLSF_ASSERT_MSG(_block->IsFree(), "Block must be free");
        const auto [fl, sl] = MappingInsert(_block->GetSize());

        TlsfHeap::ControlBlock* control = GetControlBlock();
        TlsfHeap::BlockHeader* current = control->m_headerMap[fl][sl];
        TLSF_ASSERT_MSG(current != nullptr, "Freelist cannot have a null entry");
        TLSF_ASSERT_MSG(_block != nullptr, "Cannot insert a null entry in the freelist");
        _block->m_nextFreeBlock = current;
        _block->m_previousFreeBlock = &control->m_nullBlock;
        current->m_previousFreeBlock = _block;

        TLSF_ASSERT_MSG(
            Alignment::IsAligned(reinterpret_cast<uintptr_t>(TlsfHeap::BlockHeaderToUserPtr(_block)), TlsfHeap::kAlignment),
            "Block not aligned properly");

        control->m_headerMap[fl][sl] = _block;
        control->m_flBitmap |= (1 << fl);
        control->m_slBitmaps[fl] |= (1 << sl);
    }

    void TlsfAllocator::RemoveBlock(TlsfHeap::BlockHeader* _block, u8 _fl, u8 _sl)
    {
        TlsfHeap::BlockHeader* previous = _block->m_previousFreeBlock;
        TlsfHeap::BlockHeader* next = _block->m_nextFreeBlock;
        TLSF_ASSERT(previous != nullptr);
        TLSF_ASSERT(next != nullptr);

        next->m_previousFreeBlock = previous;
        previous->m_nextFreeBlock = next;

        TlsfHeap::ControlBlock* control = GetControlBlock();
        if (control->m_headerMap[_fl][_sl] == _block)
        {
            control->m_headerMap[_fl][_sl] = next;
            if (next == &control->m_nullBlock)
            {
                control->m_slBitmaps[_fl] &= ~(1 << _sl);
                if (control->m_slBitmaps[_fl] == 0)
                {
                    control->m_flBitmap &= ~(1 << _fl);
                }
            }
        }
    }

    TlsfHeap::BlockHeader* TlsfAllocator::LinkNext(TlsfHeap::BlockHeader* _block)
    {
        TlsfHeap::BlockHeader* next = NextBlock(_block);
        next->m_previousPhysicalBlock = _block;
        return next;
    }

    TlsfHeap::BlockHeader* TlsfAllocator::NextBlock(const TlsfHeap::BlockHeader* _block)
    {
        auto* next = reinterpret_cast<TlsfHeap::BlockHeader*>(
            reinterpret_cast<uintptr_t>(TlsfHeap::BlockHeaderToUserPtr(_block)) +
            _block->GetSize() - TlsfHeap::kBlockHeaderOverhead);
        TLSF_ASSERT(!_block->IsLast());
        return next;
    }

    bool TlsfAllocator::CanSplit(const TlsfHeap::BlockHeader* _block, size_t _size)
    {
        return _block->GetSize() > _size + sizeof(TlsfHeap::BlockHeader);
    }

    TlsfHeap::BlockHeader* TlsfAllocator::SplitBlock(TlsfHeap::BlockHeader* _block, size_t _size)
    {
        auto* remaining = reinterpret_cast<TlsfHeap::BlockHeader*>(
            reinterpret_cast<uintptr_t>(_block) + _size + TlsfHeap::kBlockHeaderOverhead);
        const size_t remainingSize = _block->GetSize() - (_size + TlsfHeap::kBlockHeaderOverhead);

        TLSF_ASSERT_MSG(
            Alignment::IsAligned(reinterpret_cast<uintptr_t>(TlsfHeap::BlockHeaderToUserPtr(remaining)), TlsfHeap::kAlignment),
            "Remaining block not aligned properly");

        TLSF_ASSERT(_block->GetSize() == _size + remainingSize + TlsfHeap::kBlockHeaderOverhead);
        remaining->SetSize(remainingSize);
        TLSF_ASSERT_MSG(remaining->GetSize() >= TlsfHeap::kMinBlockSize, "Remaining block must be at least %lld bytes", TlsfHeap::kSmallBlockSize);

        _block->SetSize(_size);
        MarkAsFree(remaining);

        return remaining;
    }

    eastl::pair<u8, u8> TlsfAllocator::MappingInsert(u64 _insertSize)
    {
        if (_insertSize < TlsfHeap::kSmallBlockSize)
        {
            // Store small blocks in first list.
            return { 0, _insertSize / (TlsfHeap::kSmallBlockSize / TlsfHeap::kSlCount) };
        }
        const u8 fl = BitUtils::GetMostSignificantBit( _insertSize);
        const u8 sl = (_insertSize >> (fl - TlsfHeap::kSlCountPot)) - (1 << TlsfHeap::kSlCountPot);
        return { fl - TlsfHeap::kFlShift + 1, sl };
    }

    eastl::pair<u8, u8> TlsfAllocator::MappingSearch(u64 _desiredSize)
    {
        // Since _desired_size is aligned to base alignment, no need to round up, as the SL slots cover all the possible
        // sizes
        if (_desiredSize >= TlsfHeap::kSmallBlockSize)
        {
            // Round up instead of rounding down
            _desiredSize += (1 << (BitUtils::GetMostSignificantBit(_desiredSize) - TlsfHeap::kSlCountPot)) - 1;
        }
        return MappingInsert(_desiredSize);
    }

    TlsfHeap::BlockHeader* TlsfAllocator::SearchHeader(u64 _desiredSize, u8& _fl, u8& _sl)
    {
        if (_fl >= TlsfHeap::kFlIndexCount)
            return nullptr;

        TlsfHeap::ControlBlock* control = GetControlBlock();
        u64 bitmap = control->m_slBitmaps[_fl] & (~0 << _sl);

        u8 selectedFl = 255;
        u8 selectedSl = _sl;
        if (bitmap != 0)
        {
            selectedFl = _fl;
            selectedSl = BitUtils::GetLeastSignificantBit(bitmap);
        }
        else
        {
            bitmap = control->m_flBitmap & (~0 << (_fl + 1));
            if (bitmap != 0)
            {
                selectedFl = BitUtils::GetLeastSignificantBit(bitmap);
                selectedSl = BitUtils::GetLeastSignificantBit(control->m_slBitmaps[selectedFl]);
            }
        }

        _fl = selectedFl;
        _sl = selectedSl;

        if (selectedFl != 255)
            return control->m_headerMap[_fl][_sl];
        return nullptr;
    }

    void* TlsfAllocator::PrepareBlockUsed(TlsfHeap::BlockHeader* _block, size_t _size)
    {
        if (_block == nullptr)
            return nullptr;

        TLSF_ASSERT_MSG(_size > 0, "Size must be non-zero");
        TrimFree(_block, _size);
        MarkAsUsed(_block);
        return TlsfHeap::BlockHeaderToUserPtr(_block);
    }

    void TlsfAllocator::TrimFree(TlsfHeap::BlockHeader* _block, size_t _size)
    {
        TLSF_ASSERT_MSG(_block->IsFree(), "Block must be free");

        if (CanSplit(_block, _size))
        {
            TlsfHeap::BlockHeader* remaining = SplitBlock(_block, _size);
            LinkNext(_block);
            remaining->SetPrevFree();
            InsertBlock(remaining);
        }
    }

    void TlsfAllocator::MarkAsFree(TlsfHeap::BlockHeader* _block)
    {
        TlsfHeap::BlockHeader* next = NextBlock(_block);
        next->SetPrevFree();
        next->m_previousPhysicalBlock = _block;
        _block->SetFree();
    }

    void TlsfAllocator::MarkAsUsed(TlsfHeap::BlockHeader* _block)
    {
        TlsfHeap::BlockHeader* next = NextBlock(_block);
        next->SetPrevUsed();
        _block->SetUsed();
    }

    TlsfHeap::BlockHeader* TlsfAllocator::MergePreviousBlock(TlsfHeap::BlockHeader* _block)
    {
        if (_block->IsPrevFree())
        {
            TlsfHeap::BlockHeader* previous = _block->m_previousPhysicalBlock;
            TLSF_ASSERT_MSG(previous != nullptr, "Previous physical block must not be null");
            TLSF_ASSERT_MSG(previous->IsFree(), "Previous physical block must be free");

            const auto [fl, sl] = MappingInsert(previous->GetSize());
            RemoveBlock(previous, fl, sl);

            _block = MergeBlocks(previous, _block);
        }

        return _block;
    }

    TlsfHeap::BlockHeader* TlsfAllocator::MergeNextBlock(TlsfHeap::BlockHeader* _block)
    {
        TlsfHeap::BlockHeader* next = NextBlock(_block);
        TLSF_ASSERT_MSG(next != nullptr, "Next physical block must not be null");

        if (next->IsFree())
        {
            TLSF_ASSERT_MSG(!_block->IsLast(), "Physical block must not be last");

            const auto [fl, sl] = MappingInsert(next->GetSize());
            RemoveBlock(next, fl, sl);

            _block = MergeBlocks(_block, next);
        }

        return _block;
    }

    TlsfHeap::BlockHeader* TlsfAllocator::MergeBlocks(TlsfHeap::BlockHeader* _left, TlsfHeap::BlockHeader* _right)
    {
        TLSF_ASSERT_MSG(!_left->IsLast(), "Left block must not be last");
        // Note: Leaves flags untouched
        _left->m_size += _right->GetSize() + TlsfHeap::kBlockHeaderOverhead;
        LinkNext(_left);
        return _left;
    }
} // namespace KryneEngine