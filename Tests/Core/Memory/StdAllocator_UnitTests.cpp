/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/StdAllocator.hpp>
#include <KryneEngine/Core/Platform/StdAlloc.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    namespace
    {
        class CountingAllocator final: public IAllocator
        {
        public:
            CountingAllocator(): IAllocator("CountingAllocator") {}

            void* Allocate(const size_t _size, const size_t _alignment) override
            {
                m_allocationCount++;
                m_liveAllocations++;
                m_lastAlignment = _alignment;
                return StdAlloc::MemAlign(_size, _alignment);
            }

            void Free(void* _ptr, size_t) override
            {
                m_liveAllocations--;
                StdAlloc::Free(_ptr);
            }

            size_t m_allocationCount = 0;
            size_t m_liveAllocations = 0;
            size_t m_lastAlignment = 0;
        };
    }

    TEST(StdAllocator, ForwardsToAllocator)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        CountingAllocator allocator;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            std::vector<u32, StdAllocator<u32>> vector { StdAllocator<u32>(&allocator) };
            for (u32 i = 0; i < 100; i++)
            {
                vector.push_back(i);
            }

            EXPECT_GT(allocator.m_allocationCount, 0);
            EXPECT_GT(allocator.m_liveAllocations, 0);
            EXPECT_EQ(allocator.m_lastAlignment, alignof(u32));
        }

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        EXPECT_EQ(allocator.m_liveAllocations, 0);
        catcher.ExpectNoMessage();
    }

    TEST(StdAllocator, Rebind)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        CountingAllocator allocator;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            // std::map rebinds the allocator to its internal node type
            using Pair = std::pair<const u32, u64>;
            std::map<u32, u64, std::less<>, StdAllocator<Pair>> map { StdAllocator<Pair>(&allocator) };
            for (u32 i = 0; i < 10; i++)
            {
                map[i] = i * 2;
            }

            EXPECT_GE(allocator.m_allocationCount, 10);
        }

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        EXPECT_EQ(allocator.m_liveAllocations, 0);
        catcher.ExpectNoMessage();
    }

    TEST(StdAllocator, Equality)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        CountingAllocator allocatorA;
        CountingAllocator allocatorB;

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        EXPECT_EQ(StdAllocator<u32>(&allocatorA), StdAllocator<u64>(&allocatorA));
        EXPECT_NE(StdAllocator<u32>(&allocatorA), StdAllocator<u32>(&allocatorB));
        EXPECT_NE(StdAllocator<u32>(&allocatorA), StdAllocator<u32>());
        EXPECT_EQ(StdAllocator<u32>(), StdAllocator<u64>());
    }

    TEST(StdAllocator, DefaultHeapFallback)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        std::vector<u64, StdAllocator<u64>> vector;
        vector.resize(64, 42);
        EXPECT_EQ(vector.back(), 42);

        catcher.ExpectNoMessage();
    }
}
