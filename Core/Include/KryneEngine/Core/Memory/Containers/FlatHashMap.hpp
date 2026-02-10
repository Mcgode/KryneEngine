/**
 * @file
 * @author Max Godefroy
 * @date 09/02/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"
#include "KryneEngine/Core/Math/XSimdUtils.hpp"

namespace KryneEngine
{
    /**
     * @brief A performance-focused hash map implementation.
     *
     * @details
     * The hash map entries are laid in a single contiguous array and are accessed through open addressing (using
     * linear probing).
     *
     * This implementation also uses a second array as a form of Swiss Table: we have control bytes with 1 bit to
     * indicate if the slot is occupied, and the 7 other bits are used to store the 7 most significant bits of the key
     * hash value. These control bytes are used to quickly approximate if a slot corresponds to the key we're looking
     * for.
     * The operation can also be SIMD-friendly (depending on SIMD arch), allowing us to increase lookup performance.
     *
     * The map supports both adding and removing entries, and allows reuse of deleted slots. It also automatically
     * grows and rehashes when the load factor exceeds a certain threshold, though the operation isn't cheap.
     * It should be noted that removing elements replaces the slot with a tombstone, which, while they can be recycled,
     * means that, over time, there can be a fragmentation buildup, which can negatively impact performance. When the
     * map grows, it will automatically defragment itself
     */
    template <class Key, class Value>
    concept FlatHashMapValidKvp = requires (Key _k, Value _v)
    {
        eastl::pair<Key, Value>{_k, _v};
    }
        && (std::is_copy_constructible_v<eastl::pair<Key, Value>> || std::is_move_constructible_v<eastl::pair<Key, Value>>);

    template <class Key, class Value>
    requires FlatHashMapValidKvp<Key, Value>
    class FlatHashMap
    {
    public:
        using iterator = eastl::pair<Key, Value>*;
        using const_iterator = const eastl::pair<Key, Value>*;
        using kvp = eastl::pair<Key, Value>;

        explicit FlatHashMap(AllocatorInstance _allocator, size_t _initialCapacity = 0);

        ~FlatHashMap();

        FlatHashMap(const FlatHashMap& other) = delete;
        FlatHashMap(FlatHashMap&& _other) noexcept;
        FlatHashMap& operator=(const FlatHashMap& other) = delete;
        FlatHashMap& operator=(FlatHashMap&& _other) noexcept;

        [[nodiscard]] iterator begin();
        [[nodiscard]] iterator end();

        [[nodiscard]] const_iterator begin() const;
        [[nodiscard]] const_iterator end() const;

        [[nodiscard]] bool IsValidEntry(const_iterator _it) const;

        [[nodiscard]] iterator Find(const Key& _key);

        eastl::pair<iterator, bool> Emplace(kvp&& _kvp) requires std::is_move_constructible_v<kvp>
        {
            auto result = FindAndAllocateSlot<false>(_kvp.first);
            KE_ASSERT(result.first != end());
            if (result.second)
            {
                new (result.first) kvp(_kvp);
            }
            return result;
        }

        eastl::pair<iterator, bool> Insert(const kvp& _kvp)
            requires std::is_copy_constructible_v<kvp>
        {
            auto result = FindAndAllocateSlot<false>(_kvp.first);
            KE_ASSERT(result.first != end());
            if (result.second)
            {
                new (result.first) kvp(_kvp);
            }
            return result;
        }

        /**
         * @brief Faster but riskier version of `Emplace`.
         *
         * @details
         * This function assumes that the key is not already present in the map, making the slot search algorithm
         * faster. If the key already exists, you will end up with duplicated entries.
         */
        iterator EmplaceUnstable(kvp&& _kvp) requires std::is_move_constructible_v<kvp>
        {
            auto result = FindAndAllocateSlot<true>(_kvp.first);
            KE_ASSERT(result.first != end() && result.second);
            new (result.first) kvp(_kvp);
            return result.first;
        }

        /**
         * @brief Faster but riskier version of `Insert`.
         *
         * @details
         * This function assumes that the key is not already present in the map, making the slot search algorithm
         * faster. If the key already exists, you will end up with duplicated entries.
         */
        iterator InsertUnstable(const kvp& _kvp) requires std::is_copy_constructible_v<kvp>
        {
            auto result = FindAndAllocateSlot<true>(_kvp.first);
            KE_ASSERT(result.first != end() && result.second);
            new (result.first) kvp(_kvp);
            return result.first;
        }

        bool Remove(const Key& _key);

        /**
         * @brief Defragments the map by removing all tombstones.
         */
        void Defragment();

    private:
        static constexpr bool kUseSimd =
            !std::is_same_v<Math::SimdHighestArch, xsimd::unavailable>

            // No optimized way to coalesce SIMD words into masks in NEON, so the performance isn't better, no need for
            // extra complexity
            && !std::is_same_v<Math::SimdHighestArch, xsimd::neon64>
            && !std::is_same_v<Math::SimdHighestArch, xsimd::neon>;

        static constexpr size_t kControlAlignment = kUseSimd ? 1 : Math::SimdHighestArch::alignment();
        static constexpr size_t kControlBufferPadding = kControlAlignment == 1 ? 0 : kControlAlignment;
        static constexpr u8 kUnused  = 0b1000'0000u;
        static constexpr u8 kTombstone = 0b1000'0001u;
        static constexpr u8 kAvailableSlotFlag = 1 << 7;
        static constexpr double kMaxLoadFactor = 0.7;

        static_assert((kAvailableSlotFlag & kUnused) != 0 && (kAvailableSlotFlag & kTombstone) != 0);

        AllocatorInstance m_allocator;
        size_t m_capacity = 0;
        size_t m_count = 0;
        kvp* m_kvpBuffer = nullptr;
        u8* m_controlBuffer = nullptr;

        void Grow(size_t _newCapacity);

        template <bool Fast>
        eastl::pair<iterator, bool> FindAndAllocateSlot(const Key& _key);
    };
}