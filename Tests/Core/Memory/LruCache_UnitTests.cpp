/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2026.
 */

#include "KryneEngine/Core/Memory/Containers/LruCache.inl"
#include <EASTL/vector_map.h>
#include <gtest/gtest.h>
#include <random>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    namespace
    {
        // Initializes the value with its key, and records which calls had to reclaim an entry
        struct InitFunctor
        {
            u32 m_key;
            eastl::vector<bool>* m_calls;

            void operator()(const bool _mustReclaim, u32* _value) const
            {
                m_calls->push_back(_mustReclaim);
                new (_value) u32(m_key);
            }
        };
    }

    TEST(LruCache, AcquireAndHit)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        LruCache<u32, u32> cache { AllocatorInstance(), 4 };
        eastl::vector<bool> calls;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32* first = cache.Acquire(10, InitFunctor { 10, &calls });
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(*first, 10);
        ASSERT_EQ(calls.size(), 1);
        EXPECT_FALSE(calls[0]);

        // A hit returns the same value without initializing it again
        u32* second = cache.Acquire(10, InitFunctor { 10, &calls });
        EXPECT_EQ(second, first);
        EXPECT_EQ(calls.size(), 1);

        EXPECT_EQ(cache.Acquire(first), first);

        cache.Release(first);
        cache.Release(first);
        cache.Release(first);
        cache.Destroy();

        catcher.ExpectNoMessage();
    }

    TEST(LruCache, EvictsLeastRecentlyUsed)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        LruCache<u32, u32> cache { AllocatorInstance(), 2 };
        eastl::vector<bool> calls;

        cache.Release(cache.Acquire(1, InitFunctor { 1, &calls }));
        cache.Release(cache.Acquire(2, InitFunctor { 2, &calls }));
        // Touch 1, so that 2 is now the least recently used
        cache.Release(cache.Acquire(1, InitFunctor { 1, &calls }));
        ASSERT_EQ(calls.size(), 2);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32* third = cache.Acquire(3, InitFunctor { 3, &calls });
        ASSERT_NE(third, nullptr);
        EXPECT_EQ(*third, 3);
        ASSERT_EQ(calls.size(), 3);
        EXPECT_TRUE(calls[2]) << "Cache is full, an entry must be reclaimed";
        cache.Release(third);

        // 1 is still cached, 2 was evicted
        cache.Release(cache.Acquire(1, InitFunctor { 1, &calls }));
        EXPECT_EQ(calls.size(), 3);
        cache.Release(cache.Acquire(2, InitFunctor { 2, &calls }));
        EXPECT_EQ(calls.size(), 4);

        cache.Destroy();

        catcher.ExpectNoMessage();
    }

    TEST(LruCache, AcquiredEntriesAreNotEvicted)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        LruCache<u32, u32> cache { AllocatorInstance(), 2 };
        eastl::vector<bool> calls;

        // 1 is the least recently used, but is still held
        u32* held = cache.Acquire(1, InitFunctor { 1, &calls });
        cache.Release(cache.Acquire(2, InitFunctor { 2, &calls }));

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32* third = cache.Acquire(3, InitFunctor { 3, &calls });
        ASSERT_NE(third, nullptr);
        EXPECT_EQ(*held, 1) << "Held entry was reclaimed";

        // Nothing left to reclaim
        EXPECT_EQ(cache.Acquire(4, InitFunctor { 4, &calls }), nullptr);
        EXPECT_EQ(calls.size(), 3);

        cache.Release(held);
        cache.Release(third);
        cache.Destroy();

        catcher.ExpectNoMessage();
    }

    TEST(LruCache, Churn)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        constexpr u32 capacity = 64;
        LruCache<u32, u32> cache { AllocatorInstance(), capacity };
        eastl::vector<bool> calls;
        std::mt19937 rng(42);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // Many more keys than the capacity, so that most accesses evict an entry
        for (u32 i = 0; i < 100'000; ++i)
        {
            const u32 key = rng() % 1000;
            u32* value = cache.Acquire(key, InitFunctor { key, &calls });
            ASSERT_NE(value, nullptr) << "Access " << i;
            ASSERT_EQ(*value, key) << "Access " << i;
            cache.Release(value);
        }

        // At most one initialization per access, and some hits
        EXPECT_LT(calls.size(), 100'000);
        EXPECT_GT(calls.size(), 90'000);

        cache.Destroy();

        catcher.ExpectNoMessage();
    }

    TEST(LruCache, DestroyClearsAllEntries)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        LruCache<u32, u32> cache { AllocatorInstance(), 8 };
        eastl::vector<bool> calls;
        for (u32 key = 0; key < 5; ++key)
            cache.Release(cache.Acquire(key, InitFunctor { key, &calls }));

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        eastl::vector_map<u32, u32> cleared;
        cache.Destroy([&](u32& _key, u32& _value) { cleared[_key] = _value; });

        ASSERT_EQ(cleared.size(), 5);
        for (u32 key = 0; key < 5; ++key)
            EXPECT_EQ(cleared[key], key);

        catcher.ExpectNoMessage();
    }

    TEST(LruCache, DestroyIsRequired)
    {
        ScopedAssertCatcher catcher;

        {
            LruCache<u32, u32> cache { AllocatorInstance(), 2 };
        }

        catcher.ExpectMessageCount(1);
    }
}
