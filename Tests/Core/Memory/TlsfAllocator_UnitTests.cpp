/**
 * @file
 * @author Max Godefroy
 * @date 13/02/2025.
 */

#include <EASTL/unique_ptr.h>
#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/BitUtils.hpp>
#include <KryneEngine/Core/Common/Utils/Alignment.hpp>
#include <KryneEngine/Core/Memory/Allocators/TlsfAllocator.hpp>
#include <KryneEngine/Core/Memory/Heaps/TlsfHeap.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    inline TlsfHeap::ControlBlock* GetControlBlock(TlsfAllocator* _allocator)
    {
        constexpr size_t offset = Alignment::AlignUp(sizeof(TlsfAllocator), TlsfHeap::kAlignment);
        return reinterpret_cast<TlsfHeap::ControlBlock*>(reinterpret_cast<uintptr_t>(_allocator) + offset);
    }

    inline TlsfHeap::BlockHeader* NextBlock(TlsfHeap::BlockHeader* _block)
    {
        return reinterpret_cast<TlsfHeap::BlockHeader*>(
            reinterpret_cast<uintptr_t>(_block) + _block->GetSize() + TlsfHeap::kBlockHeaderOverhead);
    }

    TEST(TlsfAllocator, Creation)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        const TlsfHeap::ControlBlock* control = GetControlBlock(allocator);

        // After creation, we expect the null block to have no entry in `m_nextFreeBlock`, but we expect the initial
        // pool free block to be found in `m_previousFreeBlock`
        EXPECT_EQ(control->m_nullBlock.m_nextFreeBlock, nullptr);
        EXPECT_NE(control->m_nullBlock.m_previousFreeBlock, nullptr);

        // The fl bitmap should have exactly 1 bit set, for the free block
        EXPECT_NE(control->m_flBitmap, 0);
        EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_flBitmap), BitUtils::GetLeastSignificantBit(control->m_flBitmap));
        const u8 flIndex = BitUtils::GetMostSignificantBit(control->m_flBitmap);

        // The sl bitmaps should be zero, expect the one at fl index, which should have 1 bit set, at the sl index
        for (auto i = 0; i < eastl::size(control->m_slBitmaps); i++)
        {
            if (i != flIndex)
                EXPECT_EQ(control->m_slBitmaps[i], 0);
            else
            {
                EXPECT_NE(control->m_slBitmaps[i], 0);
                EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_slBitmaps[i]), BitUtils::GetLeastSignificantBit(control->m_slBitmaps[i]));
            }
        }
        const u8 slIndex = BitUtils::GetMostSignificantBit(control->m_slBitmaps[flIndex]);

        // Check that all the header map entries are the null block, except at the free block index
        for (auto fl = 0; fl < TlsfHeap::kFlIndexCount; fl++)
        {
            for (auto sl = 0; sl < TlsfHeap::kSlCount; sl++)
            {
                if (fl != flIndex || sl != slIndex)
                    EXPECT_EQ(control->m_headerMap[fl][sl], &control->m_nullBlock) << fl << ":" << sl;
                else
                    EXPECT_NE(control->m_headerMap[fl][sl], &control->m_nullBlock);
            }
        }

        EXPECT_EQ(control->m_headerMap[flIndex][slIndex], control->m_nullBlock.m_previousFreeBlock);

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, SingleAllocate)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const TlsfHeap::ControlBlock* control = GetControlBlock(allocator);
        const TlsfHeap::BlockHeader* firstBlock = control->m_nullBlock.m_previousFreeBlock;

        void* p0 = allocator->Allocate(1024, 0);

        EXPECT_NE(p0, nullptr);
        EXPECT_EQ(firstBlock, TlsfHeap::UserPtrToBlockHeader(p0));

        // Similar to creation, after a single allocation we should only have 1 block
        EXPECT_NE(control->m_flBitmap, 0);
        EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_flBitmap), BitUtils::GetLeastSignificantBit(control->m_flBitmap));
        const u8 flIndex = BitUtils::GetMostSignificantBit(control->m_flBitmap);

        for (auto i = 0; i < eastl::size(control->m_slBitmaps); i++)
        {
            if (i != flIndex)
                EXPECT_EQ(control->m_slBitmaps[i], 0);
            else
            {
                EXPECT_NE(control->m_slBitmaps[i], 0);
                EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_slBitmaps[i]), BitUtils::GetLeastSignificantBit(control->m_slBitmaps[i]));
            }
        }
        const u8 slIndex = BitUtils::GetMostSignificantBit(control->m_slBitmaps[flIndex]);

        for (auto fl = 0; fl < TlsfHeap::kFlIndexCount; fl++)
        {
            for (auto sl = 0; sl < TlsfHeap::kSlCount; sl++)
            {
                if (fl != flIndex || sl != slIndex)
                    EXPECT_EQ(control->m_headerMap[fl][sl], &control->m_nullBlock);
                else
                    EXPECT_NE(control->m_headerMap[fl][sl], &control->m_nullBlock);
            }
        }

        EXPECT_NE(control->m_headerMap[flIndex][slIndex], firstBlock);

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, InvalidAllocations)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // Size 0 outputs nullptr
        EXPECT_EQ(allocator->Allocate(0, 0), nullptr);

        // If bigger that biggest allocatable size, outputs nullptr
        EXPECT_EQ(allocator->Allocate(1ull << 60, 0), nullptr);

        // Even if valid size, if not enough space, cannot allocate
        allocator->SetAutoGrowth(false);
        EXPECT_EQ(allocator->Allocate(heapSize, 0), nullptr);

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, SingleFree)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const TlsfHeap::ControlBlock* control = GetControlBlock(allocator);
        const TlsfHeap::BlockHeader* firstBlock = control->m_nullBlock.m_previousFreeBlock;

        const size_t firstBlockSize = firstBlock->GetSize();

        void* p = allocator->Allocate(1024, 0);
        EXPECT_NE(p, nullptr);

        allocator->Free(p, 1024);

        // Similar to single allocation, after a single alloc & free we should only have 1 block
        EXPECT_NE(control->m_flBitmap, 0);
        EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_flBitmap), BitUtils::GetLeastSignificantBit(control->m_flBitmap));
        const u8 flIndex = BitUtils::GetMostSignificantBit(control->m_flBitmap);

        for (auto i = 0; i < eastl::size(control->m_slBitmaps); i++)
        {
            if (i != flIndex)
                EXPECT_EQ(control->m_slBitmaps[i], 0);
            else
            {
                EXPECT_NE(control->m_slBitmaps[i], 0);
                EXPECT_EQ(BitUtils::GetMostSignificantBit(control->m_slBitmaps[i]), BitUtils::GetLeastSignificantBit(control->m_slBitmaps[i]));
            }
        }
        const u8 slIndex = BitUtils::GetMostSignificantBit(control->m_slBitmaps[flIndex]);

        for (auto fl = 0; fl < TlsfHeap::kFlIndexCount; fl++)
        {
            for (auto sl = 0; sl < TlsfHeap::kSlCount; sl++)
            {
                if (fl != flIndex || sl != slIndex)
                    EXPECT_EQ(control->m_headerMap[fl][sl], &control->m_nullBlock);
                else
                    EXPECT_NE(control->m_headerMap[fl][sl], &control->m_nullBlock);
            }
        }

        EXPECT_EQ(control->m_headerMap[flIndex][slIndex], firstBlock);
        EXPECT_EQ(firstBlock->GetSize(), firstBlockSize);

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, AdvancedBlockMerge)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const TlsfHeap::ControlBlock* control = GetControlBlock(allocator);
        TlsfHeap::BlockHeader* firstBlock = control->m_nullBlock.m_previousFreeBlock;

        const size_t initialSize = firstBlock->GetSize();

        constexpr size_t p0Size = 128;
        void* p0 = allocator->Allocate(p0Size, 0);
        EXPECT_NE(p0, nullptr);

        TlsfHeap::BlockHeader* previous = firstBlock;
        TlsfHeap::BlockHeader* block = NextBlock(firstBlock);

        EXPECT_EQ(TlsfHeap::UserPtrToBlockHeader(p0), firstBlock);
        EXPECT_EQ(previous->GetSize(), p0Size);
        EXPECT_EQ(p0Size + block->GetSize() + TlsfHeap::kBlockHeaderOverhead, initialSize);
        size_t offset = p0Size + TlsfHeap::kBlockHeaderOverhead;

        constexpr size_t p1Size = 256;
        void* p1 = allocator->Allocate(p1Size, 0);
        EXPECT_NE(p1, nullptr);

        previous = block;
        block = NextBlock(block);
        EXPECT_EQ(TlsfHeap::UserPtrToBlockHeader(p1), previous);
        EXPECT_EQ(previous->GetSize(), p1Size);
        EXPECT_EQ(p1Size + block->GetSize() + TlsfHeap::kBlockHeaderOverhead, initialSize - offset);
        offset += p1Size + TlsfHeap::kBlockHeaderOverhead;

        constexpr size_t p2Size = 512;
        void* p2 = allocator->Allocate(p2Size, 0);
        EXPECT_NE(p2, nullptr);

        previous = block;
        block = NextBlock(block);
        EXPECT_EQ(TlsfHeap::UserPtrToBlockHeader(p2), previous);
        EXPECT_EQ(previous->GetSize(), p2Size);
        EXPECT_EQ(p2Size + block->GetSize() + TlsfHeap::kBlockHeaderOverhead, initialSize - offset);

        allocator->Free(p0, p0Size);
        size_t size = p0Size;
        EXPECT_EQ(firstBlock->GetSize(), size);
        EXPECT_EQ(NextBlock(firstBlock), TlsfHeap::UserPtrToBlockHeader(p1));

        allocator->Free(p1, p1Size);
        size += p1Size + TlsfHeap::kBlockHeaderOverhead;
        EXPECT_EQ(firstBlock->GetSize(), size);
        EXPECT_EQ(NextBlock(firstBlock), TlsfHeap::UserPtrToBlockHeader(p2));

        allocator->Free(p2, p2Size);
        EXPECT_EQ(firstBlock->GetSize(), initialSize); // All freed, should have all merged

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, AlignedAlloc)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 16 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        constexpr size_t blockSize = 1024;
        for (u8 i = 0; i <= 10; i++)
        {
            const size_t alignment = 1 << i;
            void* p = allocator->Allocate(blockSize, alignment);
            EXPECT_TRUE(Alignment::IsAligned(reinterpret_cast<uintptr_t>(p), alignment));
            allocator->Free(p, blockSize);
        }

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }

    TEST(TlsfAllocator, MultiHeapDestruction)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t containingHeapSize = 1024 * 1024;
        TlsfAllocator* containingAllocator = TlsfAllocator::Create(AllocatorInstance(), containingHeapSize);
        containingAllocator->SetAutoGrowth(false);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const TlsfHeap::BlockHeader* firstBlock = GetControlBlock(containingAllocator)->m_nullBlock.m_previousFreeBlock;
        const size_t allFreeSize = firstBlock->GetSize();

        constexpr size_t containedHeapSize = 8 * 1024;
        TlsfAllocator* containedAllocator = TlsfAllocator::Create(AllocatorInstance(containingAllocator), containedHeapSize);
        containedAllocator->AddHeap();
        containedAllocator->AddHeap();

        // Call destructor, should remove all added heaps and deallocate the initial heap
        TlsfAllocator::Destroy(containedAllocator);

        EXPECT_EQ(firstBlock->GetSize(), allFreeSize);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(containingAllocator);
    }

    TEST(TlsfAllocator, AutoGrowth)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr size_t heapSize = 8 * 1024;
        TlsfAllocator* allocator = TlsfAllocator::Create(AllocatorInstance(), heapSize);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // Block size that is big enough to warrant a new heap
        constexpr size_t blockSize = 6 * 1024;

        EXPECT_TRUE(allocator->IsAutoGrowth());
        void* p0 = allocator->Allocate(blockSize, 0);
        EXPECT_NE(p0, nullptr);

        allocator->SetAutoGrowth(false);
        void* p1 = allocator->Allocate(blockSize, 0);
        EXPECT_EQ(p1, nullptr);

        catcher.ExpectNoMessage();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        TlsfAllocator::Destroy(allocator);
    }
}