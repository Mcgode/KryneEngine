/**
* @file
 * @author Max Godefroy
 * @date 09/02/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Containers/FlatHashMap.hpp"

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Math/Hashing.hpp"
#include "KryneEngine/Core/Math/Simd/SimdArithmeticOperations.hpp"
#include "KryneEngine/Core/Math/Simd/SimdCompareOperations.hpp"
#include "KryneEngine/Core/Math/Simd/SimdMemoryOperations.hpp"

namespace KryneEngine
{
    namespace FlatHashMapInternals
    {
        static constexpr bool kUseSimdScan =
#if defined(__ARM_NEON) || defined(__SSE2__)
            true;
#else
            false;
#endif

        static constexpr size_t kControlAlignment =
#if defined(__ARM_NEON)
            sizeof(uint8x16_t);
#elif defined(__SSE2__)
            sizeof(__m128i);
#else
            1;
#endif

        static constexpr size_t kControlBufferPadding = kControlAlignment == 1 ? 0 : kControlAlignment;
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::FlatHashMap(
        const AllocatorInstance _allocator,
        const size_t _initialCapacity)
            : m_allocator(_allocator)
    {
        if (_initialCapacity > 0)
            Grow(_initialCapacity);
        KE_ASSERT(!Fixed || _initialCapacity > 0);
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::~FlatHashMap()
    {
        if (m_kvpBuffer != nullptr)
            m_allocator.deallocate(m_kvpBuffer, sizeof(*m_kvpBuffer) * m_capacity);
        if (m_controlBuffer != nullptr)
            m_allocator.deallocate(m_controlBuffer, sizeof(*m_controlBuffer) * m_capacity);
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::FlatHashMap(FlatHashMap&& _other) noexcept
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

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>& FlatHashMap<Key, Value, Fixed>::operator=(FlatHashMap&& _other) noexcept
    {
        if (m_kvpBuffer != nullptr)
            m_allocator.deallocate(m_kvpBuffer, sizeof(*m_kvpBuffer) * m_capacity);
        if (m_controlBuffer != nullptr)
            m_allocator.deallocate(m_controlBuffer, sizeof(*m_controlBuffer) * (m_capacity + FlatHashMapInternals::kControlBufferPadding));

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

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::iterator FlatHashMap<Key, Value, Fixed>::begin()
    {
        return m_kvpBuffer;
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::iterator FlatHashMap<Key, Value, Fixed>::end()
    {
        return m_kvpBuffer == nullptr ? nullptr :m_kvpBuffer + m_capacity;
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::const_iterator FlatHashMap<Key, Value, Fixed>::begin() const
    {
        return m_kvpBuffer;
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::const_iterator FlatHashMap<Key, Value, Fixed>::end() const
    {
        return m_kvpBuffer == nullptr ? nullptr : m_kvpBuffer + m_capacity;
    }
    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    bool FlatHashMap<Key, Value, Fixed>::IsValidEntry(const_iterator _it) const
    {
        return (m_controlBuffer[eastl::distance(begin(), _it)] & kAvailableSlotFlag) == 0;
    }

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::iterator FlatHashMap<Key, Value, Fixed>::Find(const Key& _key)
    {
        const size_t hash = Hashing::HashKey<Key>(_key);
        const u8 expectedControl = hash >> (sizeof(size_t) * 8 - 7);

        size_t probeIndex = hash % m_capacity;

        if constexpr (!FlatHashMapInternals::kUseSimdScan)
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
            static_assert(!std::is_same_v<Simd::u8x16, eastl::array<u8, 16>>, "Unsupported arch");

            size_t i = 0;

            const Simd::u8x16 expectedControlBatch = Simd::From(expectedControl);
            const Simd::u8x16 unusedBatch = Simd::From(kUnused);

            while (i < m_capacity)
            {
                const Simd::u8x16 controlBatch = Simd::LoadUnaligned(m_controlBuffer + probeIndex);

                const u64 expectedMask = Simd::CompareEqMask(controlBatch, expectedControlBatch);
                const u64 unusedMask = Simd::CompareEqMask(controlBatch, unusedBatch);

                if (expectedMask != 0)
                {
                    const u64 bitmask = unusedMask == 0
                        ? ~0ull
                        : BitUtils::BitMask<u64>(BitUtils::GetLeastSignificantBit(unusedMask));
                    u64 matchMask = expectedMask & bitmask;

                    u64 offset = 0;
                    while (matchMask != 0)
                    {
                        const u64 bitIndex = BitUtils::GetLeastSignificantBit(matchMask) >> Simd::kCompareEqMaskElementWidthPot;
                        offset += bitIndex;
                        matchMask >>= (bitIndex + 1) << Simd::kCompareEqMaskElementWidthPot;

                        if (m_kvpBuffer[probeIndex + offset].first == _key)
                            return m_kvpBuffer + probeIndex + offset;
                    }
                }

                if (unusedMask != 0)
                    return end();

                probeIndex += FlatHashMapInternals::kControlAlignment;
                i += FlatHashMapInternals::kControlAlignment;
                probeIndex %= m_capacity;
            }
        }

        return end();
    }

    template <class Key, class Value, bool Fixed> requires FlatHashMapValidKvp<Key, Value>
    bool FlatHashMap<Key, Value, Fixed>::Erase(iterator _it)
    {
        if (_it != end())
        {
            --m_count;
            m_controlBuffer[eastl::distance(begin(), _it)] = kTombstone;
            return true;
        }
        return false;
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value, Fixed>::Defragment() requires (!Fixed)
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

    template <class Key, class Value, bool Fixed>
    requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value, Fixed>::Grow(size_t _newCapacity)
    {
        if (m_capacity == 0)
        {
            const size_t newCapacity = Alignment::AlignUp(_newCapacity, FlatHashMapInternals::kControlAlignment);

            m_kvpBuffer = m_allocator.Allocate<eastl::pair<Key, Value>>(newCapacity);

            m_controlBuffer = static_cast<u8*>(m_allocator.allocate(newCapacity + FlatHashMapInternals::kControlBufferPadding, FlatHashMapInternals::kControlAlignment));
            memset(m_controlBuffer, kUnused, newCapacity + FlatHashMapInternals::kControlBufferPadding);

            m_count = 0;
            m_capacity = newCapacity;
        }
        else if constexpr (!Fixed)
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

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    template <bool Fast>
    eastl::pair<typename FlatHashMap<Key, Value, Fixed>::iterator, bool> FlatHashMap<Key, Value, Fixed>::FindAndAllocateSlot(const Key& _key)
    {
        if (m_capacity == 0)
        {
            Grow(32);
        }
        else if constexpr (!Fixed)
        {
            const auto loadFactor = static_cast<double>(m_count + 1) / static_cast<double>(m_capacity);
            if (loadFactor > kMaxLoadFactor)
                Grow(m_capacity * 2);
        }

        const u64 hash = Hashing::HashKey<Key>(_key);
        const u8 control = hash >> (sizeof(size_t) == 8 ? 57 : 25);
        KE_ASSERT((control & kAvailableSlotFlag) == 0);

        size_t probeIndex = hash % m_capacity;

        if constexpr (FlatHashMapInternals::kUseSimdScan)
        {
#if !defined(__ARM_NEON) && !defined(__SSE2__)
#   error Unsupported arch
#endif

            size_t i = 0;
            const Simd::u8x16 testBatch = Simd::From(kAvailableSlotFlag);
            constexpr size_t lsbShift = Simd::kCompareEqMaskElementWidthPot;

            if constexpr (Fast)
            {
                while (i < m_capacity)
                {
                    const Simd::u8x16 controlBatch = Simd::LoadUnaligned(m_controlBuffer + probeIndex);

                    const u64 availableMask = Simd::CompareEqMask(Simd::BitwiseAnd(controlBatch, testBatch), testBatch);

                    if (availableMask != 0)
                    {
                        const u64 firstIndex = BitUtils::GetLeastSignificantBit(availableMask) >> lsbShift;
                        if (firstIndex + probeIndex < m_capacity)
                        {
                            m_controlBuffer[probeIndex + firstIndex] = control;
                            ++m_count;
                            return { m_kvpBuffer + probeIndex + firstIndex, true };
                        }
                    }

                    i += FlatHashMapInternals::kControlAlignment;
                    probeIndex += FlatHashMapInternals::kControlAlignment;
                    probeIndex %= m_capacity;
                }
            }
            else
            {
                const Simd::u8x16 controlTest = Simd::From(control);
                const Simd::u8x16 unusedBatch = Simd::From(kUnused);

                size_t firstAvailableIndex = m_capacity;

                while (i < m_capacity)
                {
                    const Simd::u8x16 controlBatch = Simd::LoadUnaligned(m_controlBuffer + probeIndex);

                    if (firstAvailableIndex >= m_capacity)
                    {
                        const u64 availableMask = Simd::CompareEqMask(
                            Simd::BitwiseAnd(controlBatch, testBatch),
                            testBatch);

                        if (availableMask != 0)
                        {
                            firstAvailableIndex = probeIndex + (BitUtils::GetLeastSignificantBit(availableMask) >> lsbShift);
                        }
                    }

                    const u64 controlMask = Simd::CompareEqMask(controlBatch, controlTest);
                    const u64 unusedMask = Simd::CompareEqMask(controlBatch, unusedBatch);

                    if (controlMask != 0)
                    {
                        const u64 bitmask = unusedMask == 0
                            ? ~0ull
                            : BitUtils::BitMask<u64>(BitUtils::GetLeastSignificantBit(unusedMask));
                        u64 matchMask = controlMask & bitmask;

                        u64 offset = 0;
                        while (matchMask != 0)
                        {
                            const u64 bitIndex = BitUtils::GetLeastSignificantBit(matchMask) >> lsbShift;
                            offset += bitIndex;
                            matchMask >>= (bitIndex + 1) << lsbShift;

                            if (m_kvpBuffer[probeIndex + offset].first == _key)
                                return { m_kvpBuffer + probeIndex + offset, false };
                        }
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

                    probeIndex += FlatHashMapInternals::kControlAlignment;
                    i += FlatHashMapInternals::kControlAlignment;
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
        if constexpr (!Fixed)
            KE_ERROR("Unreachable code");
        return { end(), false };
    }
} // namespace KryneEngine