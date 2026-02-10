/**
* @file
 * @author Max Godefroy
 * @date 09/02/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Containers/FlatHashMap.hpp"

namespace KryneEngine
{
    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::FlatHashMap(
        const AllocatorInstance _allocator,
        const size_t _initialCapacity)
            : m_allocator(_allocator)
    {
        if (_initialCapacity > 0)
            Grow(_initialCapacity);
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::~FlatHashMap()
    {
        if (m_kvpBuffer != nullptr)
            m_allocator.deallocate(m_kvpBuffer, sizeof(*m_kvpBuffer) * m_capacity);
        if (m_controlBuffer != nullptr)
            m_allocator.deallocate(m_controlBuffer, sizeof(*m_controlBuffer) * m_capacity);
    }

    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::FlatHashMap(FlatHashMap&& _other) noexcept
        : m_allocator(_other.m_allocator)
        , m_capacity(_other.m_capacity)
        , m_kvpBuffer(_other.m_kvpBuffer)
        , m_controlBuffer(_other.m_controlBuffer)
        , m_count(_other.m_count)
    {
        _other.m_kvpBuffer = nullptr;
        _other.m_controlBuffer = nullptr;
        _other.m_capacity = 0;
        _other.m_count = 0;
    }

    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>& FlatHashMap<Key, Value>::operator=(FlatHashMap&& _other) noexcept
    {
        if (m_kvpBuffer != nullptr)
            m_allocator.deallocate(m_kvpBuffer, sizeof(*m_kvpBuffer) * m_capacity);
        if (m_controlBuffer != nullptr)
            m_allocator.deallocate(m_controlBuffer, sizeof(*m_controlBuffer) * (m_capacity + kControlBufferPadding));

        m_allocator = _other.m_allocator;
        m_capacity = _other.m_capacity;
        m_count = _other.m_count;
        m_kvpBuffer = _other.m_kvpBuffer;
        m_controlBuffer = _other.m_controlBuffer;

        _other.m_kvpBuffer = nullptr;
        _other.m_controlBuffer = nullptr;
        _other.m_capacity = 0;
        _other.m_count = 0;
        return *this;
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::iterator FlatHashMap<Key, Value>::begin()
    {
        return m_kvpBuffer;
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::iterator FlatHashMap<Key, Value>::end()
    {
        return m_kvpBuffer == nullptr ? nullptr :m_kvpBuffer + m_capacity;
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::const_iterator FlatHashMap<Key, Value>::begin() const
    {
        return m_kvpBuffer;
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::const_iterator FlatHashMap<Key, Value>::end() const
    {
        return m_kvpBuffer == nullptr ? nullptr : m_kvpBuffer + m_capacity;
    }
    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    bool FlatHashMap<Key, Value>::IsValidEntry(const_iterator _it) const
    {
        return (m_controlBuffer[eastl::distance(begin(), _it)] & kAvailableSlotFlag) == 0;
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value>::iterator FlatHashMap<Key, Value>::Find(const Key& _key)
    {
        const size_t hash = eastl::hash<Key>()(_key);
        const u8 expectedControl = hash >> (sizeof(size_t) == 8 ? 57 : 25);

        size_t probeIndex = hash % m_capacity;

        if constexpr (!kUseSimd)
        {
            size_t i = 0;
            u8& control = m_controlBuffer[probeIndex];
            while (i < m_capacity && control != kUnused)
            {
                if (control == expectedControl && m_kvpBuffer[probeIndex].first == _key)
                    return m_kvpBuffer + probeIndex;

                ++i;
                probeIndex = (probeIndex + 1) % m_capacity;
            }
        }
        else
        {
            using Arch = Math::SimdHighestArch;

            size_t i = 0;
            const xsimd::batch<u8, Arch> expectedControlBatch { expectedControl };
            const xsimd::batch<u8, Arch> unusedBatch { kUnused };

            while (i < m_capacity)
            {
                const xsimd::batch<u8, Arch>& controlBatch = xsimd::load_unaligned(m_controlBuffer + probeIndex);

                const u64 unusedMask = (controlBatch == unusedBatch).mask();
                const u64 expectedMask = (controlBatch == expectedControlBatch).mask();

                if (expectedMask == 0)
                {
                    if (unusedMask != 0)
                        return end();
                }
                else
                {
                    const u64 bitmask = unusedMask == 0 ? ~0ull : BitUtils::BitMask<u64>(BitUtils::GetLeastSignificantBit(unusedMask));
                    u64 matchMask = expectedMask & bitmask;

                    u64 offset = 0;
                    while (matchMask != 0)
                    {
                        const u64 bitIndex = BitUtils::GetLeastSignificantBit(matchMask);
                        offset += bitIndex;
                        matchMask <<= bitIndex;

                        if (m_kvpBuffer[probeIndex + offset].first == _key)
                            return m_kvpBuffer + probeIndex + offset;
                    }

                    if (unusedMask != 0)
                        return end();
                }

                probeIndex += kControlAlignment;
                i += kControlAlignment;
                probeIndex %= m_capacity;
            }
        }

        return end();
    }
    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    bool FlatHashMap<Key, Value>::Remove(const Key& _key)
    {
        auto it = Find(_key);
        if (it != end())
        {
            --m_count;
            m_controlBuffer[eastl::distance(begin(), it)] = kTombstone;
            return true;
        }
        return false;
    }
    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value>::Defragment()
    {
        if (m_capacity == 0)
            return;

        FlatHashMap temp(m_allocator, m_capacity);
        for (iterator it = begin(); it != end(); ++it)
        {
            if (!IsValidEntry(it))
                continue;

            // Assume no duplicated keys, so we can use the unstable variants for improved performance
            if constexpr (std::is_move_constructible_v<kvp>)
                temp.EmplaceUnstable(std::move(*it));
            else
                temp.InsertUnstable(*it);
        }
        *this = std::move(temp);
    }

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value>::Grow(size_t _newCapacity)
    {
        if (m_capacity == 0)
        {
            const size_t newCapacity = Alignment::AlignUp(_newCapacity, kControlAlignment);

            m_kvpBuffer = m_allocator.Allocate<eastl::pair<Key, Value>>(newCapacity);

            m_controlBuffer = static_cast<u8*>(m_allocator.allocate(newCapacity + kControlBufferPadding, kControlAlignment));
            memset(m_controlBuffer, kUnused, newCapacity + kControlBufferPadding);

            m_count = 0;
            m_capacity = newCapacity;
        }
        else
        {
            // There is negligible
            FlatHashMap temp(m_allocator, _newCapacity);
            for (auto i = 0; i < m_count; ++i)
            {
                if ((m_controlBuffer[i] & kAvailableSlotFlag) == 0)
                {
                    // We assume that we don't have any duplicated keys in the current map, so we can use unstable
                    // variant for improved performance

                    if constexpr (std::is_move_constructible_v<kvp>)
                    {
                        temp.EmplaceUnstable(std::move(m_kvpBuffer[i]));
                    }
                    else
                    {
                        temp.InsertUnstable(m_kvpBuffer[i]);
                    }
                }
            }
            *this = std::move(temp);
        }
    }

    template <class Key, class Value>
        requires FlatHashMapValidKvp<Key, Value>
    template <bool Fast>
    eastl::pair<typename FlatHashMap<Key, Value>::iterator, bool> FlatHashMap<Key, Value>::FindAndAllocateSlot(const Key& _key)
    {
        if (m_capacity == 0)
        {
            Grow(32);
        }
        else
        {
            const auto loadFactor = static_cast<double>(m_count + 1) / static_cast<double>(m_capacity);
            if (loadFactor > kMaxLoadFactor)
                Grow(m_capacity * 2);
        }

        const u64 hash = eastl::hash<Key>()(_key);
        const u8 control = hash >> (sizeof(size_t) == 8 ? 57 : 25);
        KE_ASSERT((control & kAvailableSlotFlag) == 0);

        size_t probeIndex = hash % m_capacity;

        if constexpr (kUseSimd)
        {
            using Arch = Math::SimdHighestArch;


            const xsimd::batch<u8, Arch> testBatch { kAvailableSlotFlag };

            if constexpr (Fast)
            {
                size_t i = 0;
                while (i < m_capacity)
                {
                    const xsimd::batch<u8, Arch>& controlBatch = xsimd::load_unaligned(m_controlBuffer + probeIndex);

                    const u64 availableMask = ((controlBatch & testBatch) == testBatch).mask();

                    if (availableMask != 0)
                    {
                        const u64 firstIndex = BitUtils::GetLeastSignificantBit(availableMask);
                        if (firstIndex + probeIndex < m_capacity)
                        {
                            m_controlBuffer[probeIndex + firstIndex] = control;
                            ++m_count;
                            return { m_kvpBuffer + probeIndex + firstIndex, true };
                        }
                    }

                    probeIndex += kControlAlignment;
                    i += kControlAlignment;
                    probeIndex %= m_capacity;
                }
            }
            else
            {
                xsimd::batch<u8, Arch> controlTest { control };
                xsimd::batch<u8, Arch> unusedTest { kUnused };

                size_t firstAvailableIndex = m_capacity;

                size_t i = 0;
                while (i < m_capacity)
                {
                    const xsimd::batch<u8, Arch>& controlBatch = xsimd::load_unaligned(m_controlBuffer + probeIndex);

                    if (firstAvailableIndex >= m_capacity)
                    {
                        const u64 availableMask = ((controlBatch & testBatch) == testBatch).mask();
                        if (availableMask != 0)
                        {
                            firstAvailableIndex = probeIndex + BitUtils::GetLeastSignificantBit(availableMask);
                        }
                    }

                    const u64 controlMask = (controlBatch == controlTest).mask();
                    const u64 unusedMask = (controlBatch == unusedTest).mask();

                    if (controlMask == 0)
                    {
                        if (unusedMask != 0)
                        {
                            KE_ASSERT(firstAvailableIndex < m_capacity);
                            m_controlBuffer[firstAvailableIndex] = control;
                            ++m_count;
                            return {
                                m_kvpBuffer + firstAvailableIndex,
                                true
                            };
                        }
                    }
                    else
                    {
                        const u64 bitmask = unusedMask == 0 ? ~0ull : BitUtils::BitMask<u64>(BitUtils::GetLeastSignificantBit(unusedMask));
                        u64 matchMask = controlMask & bitmask;

                        u64 offset = 0;
                        while (matchMask != 0)
                        {
                            const u64 bitIndex = BitUtils::GetLeastSignificantBit(matchMask);
                            offset += bitIndex;
                            matchMask <<= bitIndex;

                            if (m_kvpBuffer[probeIndex + offset].first == _key)
                                return { m_kvpBuffer + probeIndex + offset, false };
                        }

                        if (unusedMask != 0)
                        {
                            KE_ASSERT(firstAvailableIndex < m_capacity);
                            m_controlBuffer[firstAvailableIndex] = control;
                            ++m_count;
                            return {
                                m_kvpBuffer + firstAvailableIndex,
                                true
                            };
                        }
                    }

                    probeIndex += kControlAlignment;
                    i += kControlAlignment;
                    probeIndex %= m_capacity;
                }
            }
        }
        else
        {
            size_t i = 0;
            while (i < m_capacity)
            {
                u8& controlSlot = m_controlBuffer[probeIndex];
                if constexpr (!Fast)
                {
                    if (controlSlot == control && m_kvpBuffer[probeIndex].first == _key)
                        return { m_kvpBuffer + probeIndex, false };
                }

                if (controlSlot & kAvailableSlotFlag)
                {
                    controlSlot = control;
                    ++m_count;
                    return { m_kvpBuffer + probeIndex, true};
                }

                ++i;
                probeIndex = (probeIndex + 1) % m_capacity;
            }
        }
        KE_ERROR("Unreachable code");
        return { end(), false };
    }
} // namespace KryneEngine