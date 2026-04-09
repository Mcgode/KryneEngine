/**
 * @file
 * @author Max Godefroy
 * @date 19/09/2024.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Memory/GenerationalPool.inl>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    TEST(GenerationalPool, Access)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        GenerationalPool<u32> hotPool {{}};
        GenerationalPool<u32, u32> hotAndColdPool {{}};

        u32 expectedCaughtCount = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        EXPECT_EQ(hotPool.GetSize(), 0);
        EXPECT_EQ(hotAndColdPool.GetSize(), 0);

        EXPECT_EQ(hotPool.GetSize(), hotAndColdPool.GetSize());

        EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedCaughtCount);

        // The gen pools are lazily initialized, so they should be empty, even at the first index.
        const GenPool::Handle invalidHandle { 0, 0 };

        EXPECT_EQ(hotPool.Get(invalidHandle), nullptr);
        expectedCaughtCount++;
        EXPECT_EQ(hotAndColdPool.Get(invalidHandle), nullptr);
        expectedCaughtCount++;

        // Should have received one assert per invalid Get
        EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedCaughtCount);
    }

    TEST(GenerationalPool, Allocate)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        u32 expectedAssertCount = 0;

        GenerationalPool<u32, u32> pool {{}};

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const GenPool::Handle firstHandle = pool.Allocate();
        EXPECT_NE(firstHandle, GenPool::kInvalidHandle);
        EXPECT_EQ(firstHandle.m_index, 0);
        EXPECT_EQ(firstHandle.m_generation, 0);

        for (u32 i = 1; i < pool.GetSize(); i++)
        {
            const GenPool::Handle handle = pool.Allocate();
            EXPECT_EQ(handle.m_index, i);
            EXPECT_EQ(handle.m_generation, 0);
        }

        EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedAssertCount);

        // Fill to max size

        for (u32 i = pool.GetSize(); i < GenerationalPool<u8>::kMaxSize; i++)
        {
            const GenPool::Handle handle = pool.Allocate();
            EXPECT_EQ(handle.m_index, i);
            EXPECT_EQ(handle.m_generation, 0);
        }

        EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedAssertCount);

        // Any additional allocation triggers an assert and returns an invalid handle
        {
            const GenPool::Handle handle = pool.Allocate();
            expectedAssertCount++;
            EXPECT_EQ(handle, GenPool::kInvalidHandle);
            EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedAssertCount);
        }
    }
}
