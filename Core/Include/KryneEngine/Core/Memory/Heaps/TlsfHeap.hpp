/**
 * @file
 * @author Max Godefroy
 * @date 13/02/2025.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

#if defined(__SANITIZE_ADDRESS__)
#   define HAS_ASAN 1
#elif defined(__has_feature)
#   if __has_feature(address_sanitizer)
#       define HAS_ASAN 1
#   endif
#endif

namespace KryneEngine::TlsfHeap
{
    static constexpr u8 kSlCountPot = 5; // 32 sub-ranges
    static constexpr u8 kSlCount = 1 << kSlCountPot;
    static constexpr u8 kAlignmentPot = 3; // 8 byte alignment for blocks
    static constexpr size_t kAlignment = 1 << kAlignmentPot; // 8 byte alignment for blocks
    static constexpr u8 kFlIndexMaxPot = 32;
    static constexpr u8 kFlShift = kSlCountPot + kAlignmentPot;
    static constexpr u8 kFlIndexCount = kFlIndexMaxPot - kFlShift + 1;
    static constexpr u64 kSmallBlockSize = 1 << kFlShift;

    struct BlockHeader
    {
        BlockHeader* m_previousPhysicalBlock;
        size_t m_size;
        BlockHeader* m_nextFreeBlock;
        BlockHeader* m_previousFreeBlock;

        void SetSize(size_t _size) { m_size = (_size & kSizeBitMask) | (m_size & ~kSizeBitMask); }
        void SetFree() { m_size = m_size & ~kUsedBitMask; }
        void SetPrevFree() { m_size = m_size & ~kPrevUsedBitMask; }
        void SetUsed() { m_size = m_size | kUsedBitMask; }
        void SetPrevUsed() { m_size = m_size | kPrevUsedBitMask; }

        [[nodiscard]] size_t GetSize() const{ return m_size & kSizeBitMask; }
        [[nodiscard]] bool IsFree() const{ return (m_size & kUsedBitMask) == 0; }
        [[nodiscard]] bool IsPrevFree() const{ return (m_size & kPrevUsedBitMask) == 0; }

        [[nodiscard]] bool IsLast() const { return GetSize() == 0; }

    private:
        // Since size alignment is at least 4, the last 2 bits can be used as flags
        static constexpr size_t kSizeBitMask = ~0 << 2;
        static constexpr size_t kUsedBitMask = 0b01;
        static constexpr size_t kPrevUsedBitMask = 0b10;
    };

    static constexpr size_t kMinBlockSize = sizeof(BlockHeader) - sizeof(BlockHeader*);
    static constexpr size_t kMaxBlockSize = 1ull << kFlIndexMaxPot;

    struct alignas(kAlignment) ControlBlock
    {
        BlockHeader m_nullBlock;
#if HAS_ASAN
        size_t m_redZone0;
#endif
        u32 m_flBitmap;
#if HAS_ASAN
        size_t m_redZone1;
#endif
        u32 m_slBitmaps[kFlIndexCount];
#if HAS_ASAN
        size_t m_redZone2;
#endif
        BlockHeader* m_headerMap[kFlIndexCount][kSlCount];

        static_assert(sizeof(m_flBitmap) * 8 >= kFlIndexMaxPot, "m_flBitmap integer type inadequate");
        static_assert(sizeof(*m_slBitmaps) * 8 == (1 << kSlCountPot), "m_slBitmaps integer type inadequate");
    };

    static constexpr u64 kBlockHeaderMemoryAddressLeftOffset = sizeof(size_t);
    static constexpr u64 kBlockHeaderOverhead = sizeof(size_t);
    static constexpr u64 kUserPtrToBlockHeaderOffset = offsetof(BlockHeader, m_size) + sizeof(size_t);
    static constexpr u64 kHeapPoolOverhead = sizeof(BlockHeader) * 2;

    inline BlockHeader* UserPtrToBlockHeader(void* _ptr)
    {
        return reinterpret_cast<BlockHeader*>(reinterpret_cast<uintptr_t>(_ptr) - kUserPtrToBlockHeaderOffset);
    }

    inline void* BlockHeaderToUserPtr(const BlockHeader* _header)
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(_header) + kUserPtrToBlockHeaderOffset);
    }
}