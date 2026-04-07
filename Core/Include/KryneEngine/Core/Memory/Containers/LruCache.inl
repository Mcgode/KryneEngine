/**
 * @file
 * @author Max Godefroy
 * @date 06/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Containers/LruCache.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Memory/Containers/FlatHashMap.inl"

namespace KryneEngine
{
    template <class Key, class Value>
    LruCache<Key, Value>::LruCache(const AllocatorInstance _allocator, const u32 _capacity)
        : m_entries(_allocator.Allocate<LinkedListEntry>(_capacity))
        , m_capacity(_capacity)
        , m_hashMap(_allocator, _capacity * 2) // Twice the capacity for a 50% max load
    {
        KE_ASSERT_MSG(_capacity > 0, "Capacity must be greater than 0");

        m_firstFree = 0;
        memset(m_entries, 0, sizeof(LinkedListEntry) * _capacity);

        // Initialize pool
        for (u32 i = 0; i < _capacity; ++i)
        {
            m_entries[i].m_next = i + 1 < _capacity ? i + 1 : kInvalidIndex;
            m_entries[i].m_previous = i > 0 ? i - 1 : kInvalidIndex;
        }
    }

    template <class Key, class Value>
    LruCache<Key, Value>::~LruCache()
    {
        KE_ASSERT_MSG(m_entries == nullptr, "You must call Destroy() before destruction");
    }

    template <class Key, class Value>
    template <class Functor>
    void LruCache<Key, Value>::Destroy(Functor _valueClearFunctor)
    {
        const auto lock = m_lock.AutoLock();

        u32 index = m_firstUsed;
        while (index != kInvalidIndex)
        {
            LinkedListEntry& entry = m_entries[index];
            Key& key = (m_hashMap.begin() + entry.m_keyIdx)->first;
            _valueClearFunctor(key, *reinterpret_cast<Value*>(&entry.m_value));

            index = entry.m_next;
        }

        m_hashMap.GetAllocator().deallocate(m_entries, m_capacity * sizeof(LinkedListEntry));
        m_entries = nullptr;
    }

    template <class Key, class Value>
    template <class Functor>
    Value* LruCache<Key, Value>::Acquire(Key _key, Functor _valueReclaimFunctor)
    {
        auto lock = m_lock.AutoLock();

        auto it = m_hashMap.Find(_key);
        if (it != m_hashMap.end())
        {
            MoveFront(it->second.m_index);
            LinkedListEntry& entry = m_entries[it->second.m_index];
            ++entry.m_counter;

            lock.Unlock();

            // Rare case: we acquire an entry value that is still initializing due to concurrent access => busy wait.
            // This case should be rare enough that a busy wait is acceptable.
            if (std::atomic_ref(entry.m_keyIdx).load(std::memory_order::relaxed) == kInvalidIndex)
            {
                do
                {
                    std::this_thread::yield();
                }
                while (std::atomic_ref(entry.m_keyIdx).load(std::memory_order::acquire) == kInvalidIndex);
            }

            return reinterpret_cast<Value*>(&entry.m_value);
        }

        Value* ptr = nullptr;
        if (m_firstFree == kInvalidIndex)
        {
            u32 index = m_lastUsed;
            while (index != kInvalidIndex)
            {
                LinkedListEntry& entry = m_entries[index];
                if (entry.m_counter == 0)
                {
                    ptr = reinterpret_cast<Value*>(&entry.m_value);
                    m_hashMap.Erase(m_hashMap.begin() + entry.m_keyIdx);
                    std::atomic_ref(entry.m_keyIdx).store(kInvalidIndex, std::memory_order::release);
                    entry.m_counter = 1;
                    const auto pair = m_hashMap.Insert(_key, HashMapEntry{ index });
                    KE_ASSERT(pair.second);
                    MoveFront(index);

                    lock.Unlock();

                    _valueReclaimFunctor(true, ptr);
                    std::atomic_ref(entry.m_keyIdx).store(pair.first - m_hashMap.begin(), std::memory_order::release);
                    return ptr;
                }
                index = entry.m_previous;
            }
        }
        else
        {
            const u32 index = m_firstFree;

            m_firstFree = m_entries[index].m_next;
            if (m_firstFree != kInvalidIndex)
                m_entries[m_firstFree].m_previous = kInvalidIndex;

            MoveFront(index);

            LinkedListEntry& entry = m_entries[index];

            ptr = reinterpret_cast<Value*>(&entry.m_value);

            const auto pair = m_hashMap.Insert(_key, HashMapEntry{ index });
            KE_ASSERT(pair.second);
            std::atomic_ref(entry.m_keyIdx).store(kInvalidIndex, std::memory_order::release);

            entry.m_counter = 1;

            lock.Unlock();

            _valueReclaimFunctor(false, ptr);
            std::atomic_ref(entry.m_keyIdx).store(pair.first - m_hashMap.begin(), std::memory_order::release);
        }
        return ptr;
    }

    template <class Key, class Value>
    void LruCache<Key, Value>::Release(Value* _valuePtr)
    {
        static_assert(offsetof(LinkedListEntry, m_value) == 0);

        if (_valuePtr == nullptr)
            return;

        const auto lock = m_lock.AutoLock();
        auto* entry = reinterpret_cast<LinkedListEntry*>(_valuePtr);
        KE_ASSERT(entry->m_counter > 0);
        --entry->m_counter;
    }

    template <class Key, class Value>
    void LruCache<Key, Value>::MoveFront(u32 _index)
    {
        if (m_firstUsed == _index)
            return;

        LinkedListEntry& entry = m_entries[_index];

        if (entry.m_previous != kInvalidIndex)
            m_entries[entry.m_previous].m_next = entry.m_next;

        if (entry.m_next != kInvalidIndex)
            m_entries[entry.m_next].m_previous = entry.m_previous;

        if (m_lastUsed == _index)
            m_lastUsed = entry.m_previous;

        entry.m_next = m_firstUsed;
        entry.m_previous = kInvalidIndex;
        if (m_firstUsed != kInvalidIndex)
            m_entries[m_firstUsed].m_previous = _index;
        else
            m_lastUsed = _index;
        m_firstUsed = _index;
    }
} // KryneEngine