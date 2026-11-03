/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2026.
 */

#include "KryneEngine/Core/Memory/Containers/FlatHashMap.inl"
#include <EASTL/string.h>
#include <EASTL/vector_map.h>
#include <gtest/gtest.h>
#include <random>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    namespace
    {
        template <class Map>
        size_t CountEntries(const Map& _map)
        {
            size_t count = 0;
            for (auto it = _map.begin(); it != _map.end(); ++it)
            {
                if (_map.IsValidEntry(it))
                    ++count;
            }
            return count;
        }

        template <class Map>
        size_t Capacity(const Map& _map)
        {
            return _map.end() - _map.begin();
        }

        // Brute-forces `_count` keys (starting from `_firstKey`) whose home slot is `_home` in a map of `_capacity`
        eastl::vector<u32> FindKeysWithHome(const size_t _capacity, const size_t _home, const size_t _count, u32 _firstKey = 0)
        {
            eastl::vector<u32> keys;
            for (u32 key = _firstKey; keys.size() < _count; ++key)
            {
                if (Hashing::HashKey<u32>(key) % _capacity == _home)
                    keys.push_back(key);
            }
            return keys;
        }

        struct LifetimeCounters
        {
            static inline s64 s_alive = 0;
            static inline u32 s_copies = 0;
            static inline u32 s_moves = 0;

            static void Reset()
            {
                s_alive = 0;
                s_copies = 0;
                s_moves = 0;
            }
        };

        struct Tracked
        {
            u32 m_value = 0;

            explicit Tracked(const u32 _value = 0): m_value(_value) { ++LifetimeCounters::s_alive; }
            Tracked(const Tracked& _other): m_value(_other.m_value)
            {
                ++LifetimeCounters::s_alive;
                ++LifetimeCounters::s_copies;
            }
            Tracked(Tracked&& _other) noexcept: m_value(_other.m_value)
            {
                ++LifetimeCounters::s_alive;
                ++LifetimeCounters::s_moves;
            }
            Tracked& operator=(const Tracked&) = default;
            Tracked& operator=(Tracked&&) = default;
            ~Tracked() { --LifetimeCounters::s_alive; }
        };
    }

    TEST(FlatHashMap, DefaultConstructed)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        const auto& constMap = map;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        EXPECT_EQ(map.begin(), nullptr);
        EXPECT_EQ(map.end(), nullptr);
        EXPECT_EQ(constMap.begin(), constMap.end());
        EXPECT_EQ(map.Find(42), map.end());
        EXPECT_FALSE(map.Remove(42));
        EXPECT_EQ(CountEntries(map), 0);

        map.Clear();
        map.Defragment();
        EXPECT_EQ(map.begin(), nullptr);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, InitialCapacity)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (const size_t requested : { 1, 16, 17, 100, 256 })
        {
            FlatHashMap<u32, u32> map { AllocatorInstance(), requested };

            EXPECT_NE(map.begin(), nullptr);
            EXPECT_GE(Capacity(map), requested);
            EXPECT_EQ(CountEntries(map), 0);
            EXPECT_EQ(map.Find(0), map.end());
        }

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, InsertAndFind)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (u32 i = 0; i < 10; ++i)
        {
            const auto [it, inserted] = map.Insert({ i, i * 10 });
            EXPECT_TRUE(inserted);
            ASSERT_NE(it, map.end());
            EXPECT_EQ(it->first, i);
            EXPECT_EQ(it->second, i * 10);
        }

        EXPECT_EQ(CountEntries(map), 10);

        for (u32 i = 0; i < 10; ++i)
        {
            const auto it = map.Find(i);
            ASSERT_NE(it, map.end()) << "Key " << i << " not found";
            EXPECT_TRUE(map.IsValidEntry(it));
            EXPECT_EQ(it->first, i);
            EXPECT_EQ(it->second, i * 10);
        }

        EXPECT_EQ(map.Find(10), map.end());
        EXPECT_EQ(map.Find(~0u), map.end());

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, InsertExistingKey)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        const auto [firstIt, firstInserted] = map.Insert({ 7, 1 });
        ASSERT_TRUE(firstInserted);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const auto [insertIt, insertInserted] = map.Insert({ 7, 2 });
        EXPECT_FALSE(insertInserted);
        EXPECT_EQ(insertIt, firstIt);
        EXPECT_EQ(insertIt->second, 1);

        const auto [emplaceIt, emplaceInserted] = map.Emplace({ 7, 3 });
        EXPECT_FALSE(emplaceInserted);
        EXPECT_EQ(emplaceIt, firstIt);
        EXPECT_EQ(emplaceIt->second, 1);

        EXPECT_EQ(CountEntries(map), 1);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, ModifyThroughIterator)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        map.Insert({ 5, 50 }).first->second = 500;
        EXPECT_EQ(map.Find(5)->second, 500);

        // Insert of an existing key can be used as a find-or-insert
        map.Insert({ 5, 0 }).first->second += 1;
        EXPECT_EQ(map.Find(5)->second, 501);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, EmplaceMovesPair)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        LifetimeCounters::Reset();

        FlatHashMap<u32, Tracked> map { AllocatorInstance(), 64 };
        eastl::pair<u32, Tracked> kvp { 1, Tracked(10) };
        LifetimeCounters::s_copies = 0;
        LifetimeCounters::s_moves = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const auto [it, inserted] = map.Emplace(std::move(kvp));
        EXPECT_TRUE(inserted);
        EXPECT_EQ(it->second.m_value, 10);
        EXPECT_EQ(LifetimeCounters::s_copies, 0) << "Emplace should move the pair, not copy it";
        EXPECT_EQ(LifetimeCounters::s_moves, 1);

        const auto unstableIt = map.EmplaceUnstable({ 2, Tracked(20) });
        EXPECT_EQ(unstableIt->second.m_value, 20);
        EXPECT_EQ(LifetimeCounters::s_copies, 0) << "EmplaceUnstable should move the pair, not copy it";

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, EraseAndRemove)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        for (u32 i = 0; i < 16; ++i)
            map.Insert({ i, i });

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const auto it = map.Find(3);
        ASSERT_NE(it, map.end());
        EXPECT_TRUE(map.Erase(it));
        EXPECT_FALSE(map.IsValidEntry(it));
        EXPECT_EQ(map.Find(3), map.end());
        EXPECT_FALSE(map.Erase(it)) << "Erasing an already erased entry should fail";
        EXPECT_FALSE(map.Erase(map.end()));

        EXPECT_TRUE(map.Remove(4));
        EXPECT_FALSE(map.Remove(4));
        EXPECT_FALSE(map.Remove(100));

        EXPECT_EQ(CountEntries(map), 14);

        for (u32 i = 0; i < 16; ++i)
        {
            if (i == 3 || i == 4)
                continue;
            const auto found = map.Find(i);
            ASSERT_NE(found, map.end()) << "Key " << i << " not found";
            EXPECT_EQ(found->second, i);
        }

        // Re-inserting a removed key works
        const auto [reinsertIt, inserted] = map.Insert({ 3, 33 });
        EXPECT_TRUE(inserted);
        EXPECT_EQ(map.Find(3), reinsertIt);
        EXPECT_EQ(reinsertIt->second, 33);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, TombstonesKeepProbeChains)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // Large-ish fixed capacity and a high fill ratio to get long probe chains, without any regrowth.
        FlatHashMap<u32, u32> map { AllocatorInstance(), 1024 };
        constexpr u32 count = 700;
        ASSERT_LT(count + 1, Capacity(map) * 0.7);

        for (u32 i = 0; i < count; ++i)
            map.Insert({ i, i + 1 });
        const auto* buffer = map.begin();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (u32 i = 0; i < count; i += 2)
            EXPECT_TRUE(map.Remove(i));

        EXPECT_EQ(map.begin(), buffer) << "Removal should not reallocate";
        EXPECT_EQ(CountEntries(map), count / 2);

        for (u32 i = 0; i < count; ++i)
        {
            const auto it = map.Find(i);
            if (i % 2 == 0)
            {
                EXPECT_EQ(it, map.end()) << "Key " << i << " should have been removed";
            }
            else
            {
                ASSERT_NE(it, map.end()) << "Key " << i << " not found";
                EXPECT_EQ(it->second, i + 1);
            }
        }

        // Re-inserting must reuse slots and not create duplicates of the remaining keys
        for (u32 i = 0; i < count; ++i)
        {
            const auto [it, inserted] = map.Insert({ i, i + 1 });
            EXPECT_EQ(inserted, i % 2 == 0) << "Key " << i;
            EXPECT_EQ(it->first, i);
        }
        EXPECT_EQ(CountEntries(map), count);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, SameControlTagInProbeWindow)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance(), 32 };
        const size_t capacity = Capacity(map);

        // Brute-force a few keys sharing both the same home slot and the same 7-bit control tag, so that a single
        // probe window holds multiple tag matches.
        constexpr size_t keysNeeded = 4;
        eastl::vector_map<u32, eastl::vector<u32>> buckets;
        const eastl::vector<u32>* collidingKeys = nullptr;
        for (u32 key = 0; collidingKeys == nullptr; ++key)
        {
            const size_t hash = Hashing::HashKey<u32>(key);
            const u32 bucket = static_cast<u32>((hash >> (sizeof(size_t) * 8 - 7)) * capacity + hash % capacity);
            auto& keys = buckets[bucket];
            keys.push_back(key);
            if (keys.size() == keysNeeded)
                collidingKeys = &keys;
        }

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (const u32 key : *collidingKeys)
        {
            const auto [it, inserted] = map.Insert({ key, key * 2 });
            EXPECT_TRUE(inserted) << "Key " << key;
        }
        EXPECT_EQ(CountEntries(map), keysNeeded);

        for (const u32 key : *collidingKeys)
        {
            const auto it = map.Find(key);
            ASSERT_NE(it, map.end()) << "Key " << key << " not found";
            EXPECT_EQ(it->second, key * 2);

            const auto [insertIt, inserted] = map.Insert({ key, 0 });
            EXPECT_FALSE(inserted) << "Key " << key << " was duplicated";
        }
        EXPECT_EQ(CountEntries(map), keysNeeded);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, Growth)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        constexpr u32 count = 10'000;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (u32 i = 0; i < count; ++i)
        {
            const auto [it, inserted] = map.Insert({ i, ~i });
            EXPECT_TRUE(inserted) << "Key " << i;
        }

        EXPECT_GE(Capacity(map), count / 0.7);
        EXPECT_EQ(CountEntries(map), count);

        for (u32 i = 0; i < count; ++i)
        {
            const auto it = map.Find(i);
            ASSERT_NE(it, map.end()) << "Key " << i << " not found";
            EXPECT_EQ(it->second, ~i);
        }

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, RandomKeys)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u64, u64> map { AllocatorInstance() };
        eastl::vector<u64> keys;

        std::mt19937_64 rng(0xC0FFEE);
        constexpr u32 count = 20'000;
        keys.reserve(count);
        for (u32 i = 0; i < count; ++i)
            keys.push_back(rng());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32 insertedCount = 0;
        for (const u64 key : keys)
        {
            if (map.Insert({ key, key ^ 0xFFFF }).second)
                ++insertedCount;
        }
        EXPECT_EQ(insertedCount, count);
        EXPECT_EQ(CountEntries(map), count);

        u32 missing = 0;
        for (const u64 key : keys)
        {
            const auto it = map.Find(key);
            if (it == map.end())
                ++missing;
            else
                EXPECT_EQ(it->second, key ^ 0xFFFF);
        }
        EXPECT_EQ(missing, 0);

        for (u32 i = 0; i < count; i += 3)
            EXPECT_TRUE(map.Remove(keys[i])) << "Index " << i;

        missing = 0;
        for (u32 i = 0; i < count; ++i)
        {
            const bool found = map.Find(keys[i]) != map.end();
            if (i % 3 == 0)
                EXPECT_FALSE(found) << "Index " << i;
            else if (!found)
                ++missing;
        }
        EXPECT_EQ(missing, 0);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, Iteration)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        constexpr u32 count = 100;
        for (u32 i = 0; i < count; ++i)
            map.Insert({ i, i });
        map.Remove(10);
        map.Remove(20);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        eastl::vector<bool> seen(count, false);
        u32 visited = 0;
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            if (!map.IsValidEntry(it))
                continue;
            ASSERT_LT(it->first, count);
            EXPECT_FALSE(seen[it->first]) << "Key " << it->first << " visited twice";
            seen[it->first] = true;
            ++visited;
        }

        EXPECT_EQ(visited, count - 2);
        for (u32 i = 0; i < count; ++i)
            EXPECT_EQ(seen[i], i != 10 && i != 20) << "Key " << i;

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, Clear)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        for (u32 i = 0; i < 50; ++i)
            map.Insert({ i, i });
        const auto* buffer = map.begin();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        map.Clear();

        EXPECT_EQ(map.begin(), buffer) << "Clear should keep the allocation";
        EXPECT_EQ(CountEntries(map), 0);
        for (u32 i = 0; i < 50; ++i)
            EXPECT_EQ(map.Find(i), map.end()) << "Key " << i;

        for (u32 i = 0; i < 50; ++i)
            EXPECT_TRUE(map.Insert({ i, i + 1 }).second) << "Key " << i;
        for (u32 i = 0; i < 50; ++i)
            EXPECT_EQ(map.Find(i)->second, i + 1);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, ClearAfterRemovals)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance(), 32 };
        const size_t capacity = Capacity(map);
        const u32 maxCount = static_cast<u32>(capacity * 0.7);

        // Fill the map up to the load factor, then remove everything, leaving only tombstones
        for (u32 i = 0; i < maxCount; ++i)
            map.Insert({ i, i });
        for (u32 i = 0; i < maxCount; ++i)
            map.Remove(i);
        ASSERT_EQ(Capacity(map), capacity);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        map.Clear();

        // After clearing, the map should behave as a fresh one: it must still be able to grow when needed
        const u32 newCount = static_cast<u32>(capacity) * 2;
        for (u32 i = 0; i < newCount; ++i)
        {
            const auto [it, inserted] = map.Insert({ 1000 + i, i });
            ASSERT_TRUE(inserted) << "Insertion " << i << " failed";
        }
        for (u32 i = 0; i < newCount; ++i)
            EXPECT_NE(map.Find(1000 + i), map.end()) << "Key " << 1000 + i;

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, Defragment)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };
        constexpr u32 count = 500;
        for (u32 i = 0; i < count; ++i)
            map.Insert({ i, i * 3 });
        for (u32 i = 0; i < count; i += 2)
            map.Remove(i);
        const size_t capacity = Capacity(map);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        map.Defragment();

        EXPECT_EQ(Capacity(map), capacity);
        EXPECT_EQ(CountEntries(map), count / 2);
        for (u32 i = 0; i < count; ++i)
        {
            const auto it = map.Find(i);
            if (i % 2 == 0)
            {
                EXPECT_EQ(it, map.end()) << "Key " << i;
            }
            else
            {
                ASSERT_NE(it, map.end()) << "Key " << i << " not found";
                EXPECT_EQ(it->second, i * 3);
            }
        }

        // Map is still usable afterward
        for (u32 i = 0; i < count; i += 2)
            EXPECT_TRUE(map.Insert({ i, i * 3 }).second) << "Key " << i;
        EXPECT_EQ(CountEntries(map), count);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, MoveSemantics)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> source { AllocatorInstance() };
        for (u32 i = 0; i < 20; ++i)
            source.Insert({ i, i });
        const auto* buffer = source.begin();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        FlatHashMap<u32, u32> moveConstructed(std::move(source));
        EXPECT_EQ(moveConstructed.begin(), buffer);
        EXPECT_EQ(source.begin(), nullptr);
        EXPECT_EQ(source.end(), nullptr);
        EXPECT_EQ(source.Find(1), source.end());
        EXPECT_EQ(CountEntries(moveConstructed), 20);

        FlatHashMap<u32, u32> moveAssigned { AllocatorInstance() };
        moveAssigned.Insert({ 100, 100 });
        moveAssigned = std::move(moveConstructed);
        EXPECT_EQ(moveAssigned.begin(), buffer);
        EXPECT_EQ(moveConstructed.begin(), nullptr);
        EXPECT_EQ(moveAssigned.Find(100), moveAssigned.end());
        for (u32 i = 0; i < 20; ++i)
            EXPECT_NE(moveAssigned.Find(i), moveAssigned.end()) << "Key " << i;

        // Self move-assignment leaves the map untouched
        auto& alias = moveAssigned;
        moveAssigned = std::move(alias);
        EXPECT_EQ(moveAssigned.begin(), buffer);
        EXPECT_EQ(CountEntries(moveAssigned), 20);
        for (u32 i = 0; i < 20; ++i)
            EXPECT_NE(moveAssigned.Find(i), moveAssigned.end()) << "Key " << i;

        // Moved-from maps are still usable
        EXPECT_TRUE(source.Insert({ 1, 2 }).second);
        EXPECT_EQ(source.Find(1)->second, 2);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, StringKeys)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<eastl::string, u32> map { AllocatorInstance() };
        constexpr u32 count = 200;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (u32 i = 0; i < count; ++i)
        {
            eastl::string key;
            key.sprintf("key_%u", i);
            EXPECT_TRUE(map.Insert({ key, i }).second) << key.c_str();
        }

        for (u32 i = 0; i < count; ++i)
        {
            eastl::string key;
            key.sprintf("key_%u", i);
            const auto it = map.Find(key);
            ASSERT_NE(it, map.end()) << key.c_str();
            EXPECT_EQ(it->first, key);
            EXPECT_EQ(it->second, i);
        }

        EXPECT_EQ(map.Find("missing"), map.end());
        EXPECT_FALSE(map.Insert({ "key_0", 0 }).second);
        EXPECT_TRUE(map.Remove("key_0"));
        EXPECT_EQ(map.Find("key_0"), map.end());

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, UnstableInsert)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (u32 i = 0; i < 100; ++i)
        {
            const auto it = (i % 2 == 0) ? map.InsertUnstable({ i, i }) : map.EmplaceUnstable({ i, i });
            ASSERT_NE(it, map.end());
            EXPECT_EQ(it->first, i);
        }
        for (u32 i = 0; i < 100; ++i)
            EXPECT_NE(map.Find(i), map.end()) << "Key " << i;
        EXPECT_EQ(CountEntries(map), 100);

        // Documented behavior: inserting an existing key with the unstable variants duplicates the entry
        map.InsertUnstable({ 0, 1 });
        EXPECT_EQ(CountEntries(map), 101);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, FixedMap)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FixedFlatHashMap<u32, u32> map { AllocatorInstance(), 32 };
        const size_t capacity = Capacity(map);
        const auto* buffer = map.begin();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // A fixed map ignores the load factor and can be filled to full capacity, without reallocating.
        for (u32 i = 0; i < capacity; ++i)
        {
            const auto [it, inserted] = map.Insert({ i, i });
            ASSERT_TRUE(inserted) << "Insertion " << i << " failed with capacity " << capacity;
            EXPECT_EQ(map.begin(), buffer);
        }
        catcher.ExpectNoMessage();

        for (u32 i = 0; i < capacity; ++i)
            EXPECT_NE(map.Find(i), map.end()) << "Key " << i;

        // Inserting an existing key still works when full
        EXPECT_FALSE(map.Insert({ 0, 0 }).second);
        catcher.ExpectNoMessage();

        // Inserting a new key when full fails
        const auto [it, inserted] = map.Insert({ 1000, 0 });
        EXPECT_FALSE(inserted);
        EXPECT_EQ(it, map.end());
        EXPECT_FALSE(catcher.GetCaughtMessages().empty());
        EXPECT_EQ(map.begin(), buffer);
    }

    TEST(FlatHashMap, ProbeWrapAround)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FlatHashMap<u32, u32> map { AllocatorInstance(), 32 };
        const size_t capacity = Capacity(map);

        // All three keys share the last slot as home, so the 2nd and 3rd have to wrap around to slots 0 and 1.
        const eastl::vector<u32> keys = FindKeysWithHome(capacity, capacity - 1, 3);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        for (const u32 key : keys)
            EXPECT_TRUE(map.Insert({ key, key }).second) << "Key " << key;

        EXPECT_EQ(map.Find(keys[0]) - map.begin(), capacity - 1);
        EXPECT_EQ(map.Find(keys[1]) - map.begin(), 0);
        EXPECT_EQ(map.Find(keys[2]) - map.begin(), 1);

        // Removing the wrapped entry must be visible to probes that start before the wrap
        EXPECT_TRUE(map.Remove(keys[1]));
        EXPECT_EQ(map.Find(keys[1]), map.end());
        EXPECT_NE(map.Find(keys[2]), map.end()) << "Probe chain broken by tombstone after wrap-around";

        // Re-inserting reuses the tombstone after the wrap
        const auto [it, inserted] = map.Insert({ keys[1], 0 });
        EXPECT_TRUE(inserted);
        EXPECT_EQ(it - map.begin(), 0);
        EXPECT_FALSE(map.Insert({ keys[2], 0 }).second) << "Wrapped key was duplicated";

        // Clearing also resets the wrapped slots
        map.Clear();
        for (const u32 key : keys)
            EXPECT_EQ(map.Find(key), map.end()) << "Key " << key;

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, FixedMapFillsAcrossWrapAround)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        FixedFlatHashMap<u32, u32> map { AllocatorInstance(), 32 };
        const size_t capacity = Capacity(map);

        // Fill every slot except slot 0, each key landing on its own home slot.
        u32 nextKey = 0;
        for (size_t home = 1; home < capacity; ++home)
        {
            const u32 key = FindKeysWithHome(capacity, home, 1, nextKey)[0];
            nextKey = key + 1;
            const auto [it, inserted] = map.Insert({ key, key });
            ASSERT_TRUE(inserted);
            ASSERT_EQ(it - map.begin(), home);
        }

        // A key with a home in the middle of the map can only land in slot 0 by wrapping around.
        const u32 lastKey = FindKeysWithHome(capacity, 5, 1, nextKey)[0];

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const auto [it, inserted] = map.Insert({ lastKey, 42 });
        ASSERT_TRUE(inserted) << "Free slot 0 not reached by the probe sequence";
        EXPECT_EQ(it - map.begin(), 0);
        EXPECT_EQ(CountEntries(map), capacity);

        const auto found = map.Find(lastKey);
        ASSERT_NE(found, map.end());
        EXPECT_EQ(found->second, 42);

        catcher.ExpectNoMessage();
    }

    TEST(FlatHashMap, FixedMapRequiresCapacity)
    {
        ScopedAssertCatcher catcher;

        FixedFlatHashMap<u32, u32> map { AllocatorInstance() };

        catcher.ExpectMessageCount(1);
    }

    TEST(FlatHashMap, ValueLifetime)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        LifetimeCounters::Reset();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            FlatHashMap<u32, Tracked> map { AllocatorInstance() };

            for (u32 i = 0; i < 10; ++i)
                map.Insert({ i, Tracked(i) });
            EXPECT_EQ(LifetimeCounters::s_alive, 10);

            map.Remove(0);
            EXPECT_EQ(LifetimeCounters::s_alive, 9) << "Erase should destroy the value";

            map.Clear();
            EXPECT_EQ(LifetimeCounters::s_alive, 0) << "Clear should destroy all values";

            // Go through a few growths
            for (u32 i = 0; i < 200; ++i)
                map.Insert({ i, Tracked(i) });
            EXPECT_EQ(LifetimeCounters::s_alive, 200) << "Growth should destroy the moved-from values";

            for (u32 i = 0; i < 200; i += 2)
                map.Remove(i);
            map.Defragment();
            EXPECT_EQ(LifetimeCounters::s_alive, 100) << "Defragment should destroy the moved-from values";

            FlatHashMap<u32, Tracked> other { AllocatorInstance() };
            other.Insert({ 1, Tracked(1) });
            other = std::move(map);
            EXPECT_EQ(LifetimeCounters::s_alive, 100) << "Move assignment should destroy the overwritten values";
        }

        EXPECT_EQ(LifetimeCounters::s_alive, 0) << "Destructor should destroy all values";

        catcher.ExpectNoMessage();
    }
}
