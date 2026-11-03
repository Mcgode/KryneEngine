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

        // NEON compare masks use a 4-bit nibble per element. Keeping a single bit per element allows
        // `mask & (mask - 1)` to clear a whole element at once.
        static constexpr u64 kMatchMaskElementBits =
#if defined(__ARM_NEON)
            0x1111'1111'1111'1111ull;
#else
            0xffff'ffff'ffff'ffffull;
#endif
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
        // Make sure all entries are properly destroyed
        DeleteAll();
        if (m_kvpBuffer != nullptr)
            m_allocator.deallocate(m_kvpBuffer, sizeof(*m_kvpBuffer) * m_capacity);
        if (m_controlBuffer != nullptr)
            m_allocator.deallocate(m_controlBuffer, sizeof(*m_controlBuffer) * (m_capacity + FlatHashMapInternals::kControlBufferPadding));
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    FlatHashMap<Key, Value, Fixed>::FlatHashMap(FlatHashMap&& _other) noexcept
        : m_allocator(_other.m_allocator)
        , m_capacity(_other.m_capacity)
        , m_count(_other.m_count)
        , m_kvpBuffer(_other.m_kvpBuffer)
        , m_controlBuffer(_other.m_controlBuffer)
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
        // Avoid self-assignment
        if (&_other == this)
            return *this;

        // Make sure all entries are properly destroyed
        DeleteAll();
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
    FlatHashMap<Key, Value, Fixed>::const_iterator FlatHashMap<Key, Value, Fixed>::Find(const Key& _key) const
    {
        if (m_capacity == 0)
            return end();

        const size_t hash = Hashing::HashKey<Key>(_key);
        const u8 expectedControl = hash >> (sizeof(size_t) * 8 - 7);

        size_t probeIndex = hash % m_capacity;

        if constexpr (!FlatHashMapInternals::kUseSimdScan)
        {
            size_t i = 0;
            u8* control = m_controlBuffer + probeIndex;
            while (i < m_capacity && *control != kUnused)
            {
                if (*control == expectedControl && m_kvpBuffer[probeIndex].first == _key)
                    return m_kvpBuffer + probeIndex;

                ++i;
                probeIndex = (probeIndex + 1) % m_capacity;
                control = m_controlBuffer + probeIndex;
            }
        }
        else
        {
            static_assert(
                !(FlatHashMapInternals::kUseSimdScan && std::is_same_v<Simd::u8x16, eastl::array<u8, 16>>),
                "Unsupported arch");

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
                    // All matches of the window are checked, including the ones after an unused slot, as `Erase`
                    // relies on it to reset slots to unused instead of tombstones.
                    u64 matchMask = expectedMask & FlatHashMapInternals::kMatchMaskElementBits;

                    while (matchMask != 0)
                    {
                        const u64 bitIndex = BitUtils::GetLeastSignificantBit(matchMask) >> Simd::kCompareEqMaskElementWidthPot;
                        const size_t index = WrapIndex(probeIndex + bitIndex);

                        if (m_kvpBuffer[index].first == _key)
                            return m_kvpBuffer + index;

                        matchMask &= matchMask - 1;
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
    bool FlatHashMap<Key, Value, Fixed>::Erase(const_iterator _it)
    {
        if (_it == end() || !IsValidEntry(_it))
            return false;

        const size_t index = _it - m_kvpBuffer;
        m_kvpBuffer[index].~kvp();

        // A tombstone is only needed if a probe sequence may have gone past this slot to reach another entry.
        // Otherwise, the slot can be reset to unused, which keeps lookup chains short, and prevents fixed-size maps
        // from filling up with tombstones over time.

        // If the next slot is unused, no probe went past this one:
        // - With linear probing, probes stop at the first unused slot, so they would have stopped at the next slot.
        // - With SIMD scans, probes only skip windows without any unused slot, and windows are contiguous. A probe
        //   that skipped a window holding this slot either covered the next slot as well, or resumed right from
        //   it, and that next slot can't have become unused since, as it was on the path to a live entry.
        // The tombstones right before this slot are then in the same situation, so they can be reset as well.
        if (m_controlBuffer[WrapIndex(index + 1)] == kUnused)
        {
            size_t i = index;
            do
            {
                SetControl(i, kUnused);
                --m_count;
                i = (i == 0 ? m_capacity : i) - 1;
            }
            while (m_controlBuffer[i] == kTombstone);
            return true;
        }
        else if constexpr (FlatHashMapInternals::kUseSimdScan)
        {
            // Otherwise, same approach as Abseil's Swiss tables: if the run of non-unused slots going through this
            // slot is shorter than a window, then no window holding this slot was ever free of unused slots, so no
            // probe ever skipped it. As lookups check every match of the window they stop at, an unused slot before
            // the entry within that window doesn't break its lookup.

            constexpr size_t windowSize = FlatHashMapInternals::kControlAlignment;
            constexpr size_t lsbShift = Simd::kCompareEqMaskElementWidthPot;
            const Simd::u8x16 unusedBatch = Simd::From(kUnused);

            const size_t beforeIndex = index >= windowSize ? index - windowSize : index + m_capacity - windowSize;
            const u64 unusedBefore = Simd::CompareEqMask(
                Simd::LoadUnaligned(m_controlBuffer + beforeIndex),
                unusedBatch) & FlatHashMapInternals::kMatchMaskElementBits;
            const u64 unusedAfter = Simd::CompareEqMask(
                Simd::LoadUnaligned(m_controlBuffer + index),
                unusedBatch) & FlatHashMapInternals::kMatchMaskElementBits;

            if (unusedBefore != 0 && unusedAfter != 0)
            {
                const size_t usedAfter = BitUtils::GetLeastSignificantBit(unusedAfter) >> lsbShift;
                const size_t usedBefore = windowSize - 1 - (BitUtils::GetMostSignificantBit(unusedBefore) >> lsbShift);
                if (usedBefore + usedAfter < windowSize)
                {
                    SetControl(index, kUnused);
                    --m_count;
                    return true;
                }
            }
        }

        SetControl(index, kTombstone);
        return true;
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value, Fixed>::Clear()
    {
        if (m_capacity == 0)
            return;

        DeleteAll();
        // Also reset the mirrored padding
        memset(m_controlBuffer, kUnused, m_capacity + FlatHashMapInternals::kControlBufferPadding);
        m_count = 0;
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
            {
                temp.EmplaceUnstable(std::move(*it));
            }
            else
            {
                temp.InsertUnstable(*it);
            }
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
            // Padding mirrors the first control bytes, so it starts unused as well
            memset(m_controlBuffer, kUnused, newCapacity + FlatHashMapInternals::kControlBufferPadding);

            m_count = 0;
            m_capacity = newCapacity;
        }
        else if constexpr (!Fixed)
        {
            // There is negligible
            FlatHashMap temp(m_allocator, _newCapacity);
            for (auto i = 0; i < m_capacity; ++i)
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
            static_assert(
                !(FlatHashMapInternals::kUseSimdScan && std::is_same_v<Simd::u8x16, eastl::array<u8, 16>>),
                "Unsupported arch");

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
                        const size_t index = WrapIndex(
                            probeIndex + (BitUtils::GetLeastSignificantBit(availableMask) >> lsbShift));

                        // Only increment count if not replacing a tombstone
                        if (m_controlBuffer[index] == kUnused)
                            ++m_count;
                        SetControl(index, control);
                        return { m_kvpBuffer + index, true };
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
                            firstAvailableIndex = WrapIndex(
                                probeIndex + (BitUtils::GetLeastSignificantBit(availableMask) >> lsbShift));
                        }
                    }

                    const u64 controlMask = Simd::CompareEqMask(controlBatch, controlTest);
                    const u64 unusedMask = Simd::CompareEqMask(controlBatch, unusedBatch);

                    if (controlMask != 0)
                    {
                        // Same as in `Find`, check all matches of the window
                        u64 matchMask = controlMask & FlatHashMapInternals::kMatchMaskElementBits;

                        while (matchMask != 0)
                        {
                            const size_t index = WrapIndex(
                                probeIndex + (BitUtils::GetLeastSignificantBit(matchMask) >> lsbShift));

                            if (m_kvpBuffer[index].first == _key)
                                return { m_kvpBuffer + index, false };

                            matchMask &= matchMask - 1;
                        }
                    }

                    if (unusedMask != 0)
                    {
                        KE_ASSERT(firstAvailableIndex < m_capacity);

                        // Only increment count if not replacing a tombstone
                        if (m_controlBuffer[firstAvailableIndex] == kUnused)
                            ++m_count;

                        SetControl(firstAvailableIndex, control);
                        return {
                            m_kvpBuffer + firstAvailableIndex,
                            true
                        };
                    }

                    probeIndex += FlatHashMapInternals::kControlAlignment;
                    i += FlatHashMapInternals::kControlAlignment;
                    probeIndex %= m_capacity;
                }

                // The whole map was scanned without finding the key nor any unused slot, so the key isn't present
                // and the first tombstone can be reused.
                if (firstAvailableIndex < m_capacity)
                {
                    SetControl(firstAvailableIndex, control);
                    return { m_kvpBuffer + firstAvailableIndex, true };
                }
            }
        }
        else
        {
            size_t i = 0;
            size_t firstTombstoneIndex = m_capacity;
            while (i < m_capacity)
            {
                u8& controlSlot = m_controlBuffer[probeIndex];
                if constexpr (!Fast)
                {
                    if (controlSlot == control && m_kvpBuffer[probeIndex].first == _key)
                        return { m_kvpBuffer + probeIndex, false };
                }

                if (controlSlot == kTombstone)
                {
                    if (firstTombstoneIndex == m_capacity)
                        firstTombstoneIndex = probeIndex;
                }
                else if (controlSlot == kUnused)
                {
                    if (firstTombstoneIndex < m_capacity)
                    {
                        probeIndex = firstTombstoneIndex;
                    }
                    else
                    {
                        m_count++;
                    }
                    SetControl(probeIndex, control);
                    return { m_kvpBuffer + probeIndex, true};
                }

                ++i;
                probeIndex = (probeIndex + 1) % m_capacity;
            }

            // The whole map was scanned without finding the key nor any unused slot, so the key isn't present
            // and the first tombstone can be reused.
            if (firstTombstoneIndex < m_capacity)
            {
                SetControl(firstTombstoneIndex, control);
                return { m_kvpBuffer + firstTombstoneIndex, true };
            }
        }
        if constexpr (!Fixed)
            KE_ERROR("Unreachable code");
        return { end(), false };
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value, Fixed>::SetControl(const size_t _index, const u8 _control)
    {
        m_controlBuffer[_index] = _control;

        // Capacity is a multiple of the padding size, so mirrored indices are always within the control buffer
        if (_index < FlatHashMapInternals::kControlBufferPadding)
            m_controlBuffer[m_capacity + _index] = _control;
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    size_t FlatHashMap<Key, Value, Fixed>::WrapIndex(const size_t _index) const
    {
        // Indices from probe windows are always below `2 * m_capacity`, so no need for a modulo
        return _index >= m_capacity ? _index - m_capacity : _index;
    }

    template <class Key, class Value, bool Fixed>
        requires FlatHashMapValidKvp<Key, Value>
    void FlatHashMap<Key, Value, Fixed>::DeleteAll()
    {
        if constexpr (!std::is_trivially_destructible_v<kvp>)
        {
            for (auto it = begin(); it != end(); ++it)
            {
                if (IsValidEntry(it))
                {
                    it->~kvp();
                }
            }
        }
    }
} // namespace KryneEngine