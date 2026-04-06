/**
 * @file
 * @author Max Godefroy
 * @date 06/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"
#include "KryneEngine/Core/Memory/Containers/FlatHashMap.hpp"
#include "KryneEngine/Core/Threads/SpinLock.hpp"

namespace KryneEngine
{
    /**
     * @brief A Least-Recently-Used Cache container with fixed size.
     *
     * @note
     * Make sure to call Destroy() before destroying the cache to properly release all resources.
     */
    template <class Key, class Value>
    class LruCache
    {
    public:
        LruCache(AllocatorInstance _allocator, u32 _capacity);

        ~LruCache();

        template <class Functor>
        void Destroy(Functor _valueClearFunctor = [](Key&, Value& _v) { _v.~Value(); });

        /**
         * @details
         * The functor must use placement new (aka `new (ptr) Value(args...)`) when initializing a value.
         */
        template <class Functor>
        [[nodiscard]] Value* Acquire(Key _key, Functor _valueReclaimFunctor);

        void Release(Value* _valuePtr);

    private:
        static constexpr u32 kInvalidIndex = 0xFFFFFFFF;

        struct LinkedListEntry
        {
            eastl::aligned_storage<sizeof(Value), alignof(Value)> m_value;
            u32 m_previous;
            u32 m_next;
            u32 m_keyIdx; // Since we won't go beyond the max load factor of the hash map, this index is always valid.
            u32 m_counter;
        };

        struct HashMapEntry
        {
            u32 m_index;
        };

        LinkedListEntry* m_entries;
        u32 m_capacity;
        u32 m_lastUsed = kInvalidIndex;
        u32 m_firstUsed = kInvalidIndex;
        u32 m_firstFree = 0;

        FixedFlatHashMap<Key, HashMapEntry> m_hashMap;

        alignas(Threads::kCacheLineSize) SpinLock m_lock;

        void MoveFront(u32 _index);
    };
} // KryneEngine
