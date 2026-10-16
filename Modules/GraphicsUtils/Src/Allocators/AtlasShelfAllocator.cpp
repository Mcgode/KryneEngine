/**
 * @file
 * @author Max Godefroy
 * @date 08/01/2026.
 */

#include "KryneEngine/Modules/GraphicsUtils/Allocators/AtlasShelfAllocator.hpp"

namespace KryneEngine::Modules::GraphicsUtils
{
    AtlasShelfAllocator::AtlasShelfAllocator(
        const AllocatorInstance _cpuAllocator,
        const Configuration& _config)
            : m_cpuAllocator(_cpuAllocator)
            , m_atlasSize(_config.m_atlasSize)
            , m_shelfWidth(_config.m_shelfWidth)
            , m_shelfColumns(_config.m_atlasSize.x / _config.m_shelfWidth)
            , m_minHeight(_config.m_minHeight)
            , m_slWidth(_config.m_slWidth)
            , m_freeShelves(_cpuAllocator)
            , m_shelves(_cpuAllocator)
            , m_freeSlots(_cpuAllocator)
            , m_slots(_cpuAllocator)
    {
        KE_ASSERT(m_atlasSize.x % m_shelfWidth == 0);

        m_freeShelves.resize(m_shelfColumns);
        for (u32 i = 0; i < m_shelfColumns; i++)
        {
            m_freeShelves[i] = { i * m_atlasSize.y, m_atlasSize.y };
        }

        KE_ASSERT(m_minHeight >> m_slWidth >= kBlockAlignment);
    }

    u32 AtlasShelfAllocator::Allocate(const uint2 _slotSize)
    {
        constexpr u32 invalidIndex = VectorDeLinkedList<ShelfEntry>::kListLimitId;

        u32 slotHeight = Alignment::AlignUp(_slotSize.y, kBlockAlignment);
        const u32 slotWidth = Alignment::AlignUp(_slotSize.x, kBlockAlignment);

        VERIFY_OR_RETURN(slotWidth <= m_shelfWidth && slotHeight <= m_atlasSize.y, invalidIndex);

        // Round up slot height
        if (slotHeight > m_minHeight)
            slotHeight += (1 << (BitUtils::GetMostSignificantBit(slotHeight) - m_slWidth)) - 1;

        slotHeight = eastl::max(slotHeight, m_minHeight);

        const u32 fl = BitUtils::GetMostSignificantBit(slotHeight);
        const u32 category = slotHeight & (BitUtils::BitMask<u32>(m_slWidth + 1) << (fl - m_slWidth));
        const u32 allocatedHeight = category;

        auto it = m_shelfCategories.lower_bound(category);
        if (it != m_shelfCategories.end())
        {
            if (it->first == category)
            {
                const u32 slot = FindSlot(slotWidth, it->second, true);
                if (slot != kInvalidSlot)
                {
                    return slot;
                }
            }

            // Try to allocate a new shelf only if there was no entry for this category
            const u32 newShelf = it->first == category ? invalidIndex : TryAllocateShelf(allocatedHeight);

            if (newShelf != invalidIndex)
            {
                m_shelfCategories.insert(it, { category, newShelf });
                const u32 slot = FindSlot(slotWidth, newShelf, false);
                KE_ASSERT_MSG(slot != kInvalidSlot, "Shelf is new, why the hell can't you find a free spot ?");
                return slot;
            }

            // Cannot allocate a new shelf, pack it in a bigger shelf if possible
            for (; it != m_shelfCategories.end(); ++it)
            {
                const u32 slot = FindSlot(slotWidth, it->second, false);
                if (slot != kInvalidSlot)
                {
                    return slot;
                }
            }
        }
        else
        {
            // No shelf big enough, allocate a new one
            const u32 shelfIndex = TryAllocateShelf(allocatedHeight);
            if (shelfIndex != invalidIndex)
            {
                m_shelfCategories.emplace_back_unsorted(category, shelfIndex);
                const u32 slot = FindSlot(slotWidth, shelfIndex, false);
                KE_ASSERT_MSG(slot != invalidIndex, "Shelf is new, why the hell can't you find a free spot ?");
                return slot;
            }
        }

        return invalidIndex;
    }

    void AtlasShelfAllocator::Free(const u32 _slot)
    {
        const SlotEntry slot = m_slots[_slot];

        // Free slot index
        m_slots[_slot].m_shelf = m_nextSlotIndex; // We use m_shelf to store the next free index
        m_nextSlotIndex = _slot;

        ShelfEntry& shelf = m_shelves[slot.m_shelf];

        if (shelf.m_firstFree == VectorDeLinkedList<FreeSlotEntry>::kListLimitId)
        {
            const u32 freeNodeIndex = m_freeSlots.AllocateNode();
            m_freeSlots[freeNodeIndex].m_start = slot.m_start;
            m_freeSlots[freeNodeIndex].m_width = slot.m_width;
            shelf.m_firstFree = freeNodeIndex;
            return;
        }

        u32 previousFreeSlotIndex = ~0u;
        u32 freeSlotIndex = shelf.m_firstFree;
        for (
            FreeSlotEntry* freeSlot = &m_freeSlots[freeSlotIndex];
            freeSlotIndex != VectorDeLinkedList<FreeSlotEntry>::kListLimitId;
            previousFreeSlotIndex = freeSlotIndex, freeSlotIndex = freeSlot->m_next, freeSlot = &m_freeSlots[freeSlotIndex])
        {
            if (freeSlot->m_start > slot.m_start)
                break;
        }

        bool backMerge = false;
        if (freeSlotIndex != VectorDeLinkedList<FreeSlotEntry>::kListLimitId)
        {
            FreeSlotEntry& freeSlot = m_freeSlots[freeSlotIndex];
            if (slot.m_start + slot.m_width == freeSlot.m_start)
            {
                freeSlot.m_start = slot.m_start;
                freeSlot.m_width += slot.m_width;
                backMerge = true;
            }
        }

        bool frontMerge = false;
        if (previousFreeSlotIndex != VectorDeLinkedList<FreeSlotEntry>::kListLimitId)
        {
            FreeSlotEntry& previousFreeSlot = m_freeSlots[previousFreeSlotIndex];
            if (previousFreeSlot.m_start + previousFreeSlot.m_width == slot.m_start)
            {
                if (backMerge)
                {
                    previousFreeSlot.m_width += m_freeSlots[freeSlotIndex].m_width;
                    m_freeSlots.FreeNode(freeSlotIndex);
                }
                else
                {
                    previousFreeSlot.m_width += slot.m_width;
                }
                frontMerge = true;
            }
        }

        if (!frontMerge && !backMerge)
        {
            const u32 newFreeSlotIndex = m_freeSlots.AllocateNode();
            m_freeSlots[newFreeSlotIndex].m_start = slot.m_start;
            m_freeSlots[newFreeSlotIndex].m_width = slot.m_width;
            m_freeSlots[newFreeSlotIndex].m_next = freeSlotIndex;
            m_freeSlots[newFreeSlotIndex].m_previous = previousFreeSlotIndex;
            if (previousFreeSlotIndex == VectorDeLinkedList<FreeSlotEntry>::kListLimitId)
                shelf.m_firstFree = newFreeSlotIndex;
            else
                m_freeSlots[previousFreeSlotIndex].m_next = newFreeSlotIndex;
            if (freeSlotIndex != VectorDeLinkedList<FreeSlotEntry>::kListLimitId)
                m_freeSlots[freeSlotIndex].m_previous = newFreeSlotIndex;
        }

        if (m_freeSlots[shelf.m_firstFree].m_width >= m_shelfWidth)
        {
            FreeShelf({ .m_start = shelf.m_start, .m_size = shelf.m_size });
            const u32 next = shelf.m_next;
            m_shelves.FreeNode(slot.m_shelf);

            auto it = m_shelfCategories.find(shelf.m_size);
            VERIFY_OR_RETURN_VOID(it != m_shelfCategories.end());
            if (it->second == slot.m_shelf)
            {
                if (next == VectorDeLinkedList<ShelfEntry>::kListLimitId)
                    m_shelfCategories.erase(it);
                else
                    it->second = next;
            }
        }
    }

    Rect AtlasShelfAllocator::GetSlotRect(const u32 _slot) const
    {
        const SlotEntry slot = m_slots[_slot];
        const ShelfEntry& shelf = m_shelves[slot.m_shelf];

        const u32 shelfX = (shelf.m_start / m_atlasSize.y) * m_shelfWidth,
                  shelfY = (shelf.m_start % m_atlasSize.y);

        return Rect(
            slot.m_start + shelfX,
            shelfY,
            slot.m_start + slot.m_width + shelfX,
            shelfY + shelf.m_size);
    }

    void AtlasShelfAllocator::FreeShelf(const FreeShelfEntry _freedShelf)
    {
        FreeShelfEntry* it = eastl::upper_bound(
            m_freeShelves.begin(),
            m_freeShelves.end(),
            _freedShelf,
            [](const FreeShelfEntry& _a, const FreeShelfEntry& _b) { return _a.m_start < _b.m_start; });

        if (it == m_freeShelves.end())
        {
            m_freeShelves.push_back(_freedShelf);
            return;
        }

        bool mergedBack = false;

        // Merge with the next entry if possible
        const bool sameColumnBack = it->m_start / m_shelfWidth == _freedShelf.m_start / m_shelfWidth;
        if (it->m_start == _freedShelf.m_start + _freedShelf.m_size && sameColumnBack)
        {
            it->m_size += _freedShelf.m_size;
            it->m_start -= _freedShelf.m_size;
            mergedBack = true;
        }

        bool mergedFront = false;
        if (it != m_freeShelves.begin())
        {
            FreeShelfEntry *pIt = it - 1;
            // Merge with the previous entry if possible
            const bool sameColumnFront = pIt->m_start / m_shelfWidth == _freedShelf.m_start / m_shelfWidth;
            if (pIt->m_start + pIt->m_size == _freedShelf.m_start && sameColumnFront)
            {
                // If already merged with next, we're filling a gap.
                if (mergedBack)
                {
                    pIt->m_size += it->m_size;
                    m_freeShelves.erase(it);
                }
                else
                {
                    pIt->m_size += _freedShelf.m_size;
                }
                mergedFront = true;
            }
        }

        if (!mergedFront && !mergedBack)
        {
            m_freeShelves.insert(it, _freedShelf);
        }
    }

    u32 AtlasShelfAllocator::FindSlot(const u32 _width, const u32 _shelfIndex, const bool _allocateShelfIfNeeded)
    {
        u32 shelfIndex = _shelfIndex;
        for (
            ShelfEntry* shelf = &m_shelves[_shelfIndex];
            shelfIndex != VectorDeLinkedList<ShelfEntry>::kListLimitId;
            shelfIndex = shelf->m_next, shelf = &m_shelves[shelfIndex])
        {
            u32 freeSlotIndex = shelf->m_firstFree;
            for (
                FreeSlotEntry* freeSlot = nullptr;
                freeSlotIndex != VectorDeLinkedList<FreeSlotEntry>::kListLimitId;
                freeSlotIndex = freeSlot->m_next)
            {
                freeSlot = &m_freeSlots[freeSlotIndex];

                if (freeSlot->m_width >= _width)
                {
                    const u32 slot = AllocateSlot();
                    m_slots[slot] = { .m_shelf = shelfIndex, .m_start = freeSlot->m_start, .m_width = _width };
                    freeSlot->m_start += _width;
                    freeSlot->m_width -= _width;

                    if (freeSlot->m_width == 0)
                    {
                        const u32 next = freeSlot->m_next;
                        m_freeSlots.FreeNode(freeSlotIndex);
                        if (freeSlotIndex == shelf->m_firstFree)
                            shelf->m_firstFree = next;
                    }
                    return slot;
                }
            }

            if (_allocateShelfIfNeeded && shelf->m_next == VectorDeLinkedList<ShelfEntry>::kListLimitId)
            {
                const u32 newShelf = TryAllocateShelf(shelf->m_size);
                if (newShelf == VectorDeLinkedList<ShelfEntry>::kListLimitId)
                    continue;

                m_shelves.SetNext(shelfIndex, newShelf);
                const u32 slot = AllocateSlot();
                m_slots[slot] = {
                    .m_shelf = newShelf,
                    .m_start = 0,
                    .m_width = _width,
                };

                FreeSlotEntry& freeSlot = m_freeSlots[m_shelves[newShelf].m_firstFree];
                freeSlot.m_width -= _width;
                freeSlot.m_start += _width;
                return slot;
            }
        }

        return kInvalidSlot;
    }

    u32 AtlasShelfAllocator::TryAllocateShelf(const u32 _height)
    {
        for (FreeShelfEntry* it = m_freeShelves.begin(); it != m_freeShelves.end(); ++it)
        {
            if (it->m_size >= _height)
            {
                const u32 start = it->m_start;
                const u32 shelfIndex = m_shelves.AllocateNode();
                it->m_start += _height;
                it->m_size -= _height;
                if (it->m_size == 0)
                {
                    m_freeShelves.erase(it);
                }

                const u32 freeSlotIndex = m_freeSlots.AllocateNode();
                m_shelves[shelfIndex].m_start = start;
                m_shelves[shelfIndex].m_size = _height;
                m_shelves[shelfIndex].m_firstFree = freeSlotIndex;
                m_freeSlots[freeSlotIndex].m_start = 0;
                m_freeSlots[freeSlotIndex].m_width = m_shelfWidth;

                return shelfIndex;
            }
        }
        return VectorDeLinkedList<ShelfEntry>::kListLimitId;
    }

    u32 AtlasShelfAllocator::AllocateSlot()
    {
        if (m_nextSlotIndex == kInvalidSlot)
        {
            m_slots.push_back();
            return m_slots.size() - 1;
        }
        const u32 index = m_nextSlotIndex;
        m_nextSlotIndex = m_slots[index].m_shelf; // We use m_shelf to store the next free index
        return index;
    }
} // namespace KryneEngine::Modules::GraphicsUtils