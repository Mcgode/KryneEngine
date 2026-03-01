/**
 * @file
 * @author Max Godefroy
 * @date 02/02/2024.
 */

#pragma once

#include <atomic>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include "KryneEngine/Core/Threads/SpinLock.hpp"

namespace KryneEngine
{
    namespace GenPool
    {
        static constexpr size_t kIndexBits = 20;
        static constexpr size_t minGenerationIntegerByteSize = (32 - kIndexBits + 7) / 8;

        struct Handle
        {
            u32 m_index: kIndexBits;
            u32 m_generation: 32 - kIndexBits;

            bool operator==(const Handle &rhs) const
            {
                return static_cast<u32>(*this) == static_cast<u32>(rhs);
            }

            bool operator<(const Handle &rhs) const
            {
                return static_cast<u32>(*this) < static_cast<u32>(rhs);
            }

            explicit operator u32() const
            {
                static_assert(sizeof(Handle) == sizeof(u32));
                return *reinterpret_cast<const u32*>(this);
            }

            static Handle FromU32(u32 _rawHandle)
            {
                Handle handle {};
                *reinterpret_cast<u32*>(&handle) = _rawHandle;
                return handle;
            }
        };

        static constexpr Handle kInvalidHandle = {
                0u,
                ~0u
        };

        static constexpr Handle kUndefinedHandle = {
                ~0u,
                ~0u
        };

        template <class T>
        concept IsValidIntrusiveGeneration = requires(T _t)
        {
            _t.m_generation;
        }
        && eastl::is_integral_v<decltype(T::m_generation)>
        && sizeof(T::m_generation) >= minGenerationIntegerByteSize;
    }

#define KE_GENPOOL_DECLARE_HANDLE(HandleName) struct HandleName                                 \
    {                                                                                           \
        GenPool::Handle m_handle = GenPool::kInvalidHandle;                                     \
                                                                                                \
        HandleName& operator=(GenPool::Handle _other) { m_handle = _other; return *this; }      \
        bool operator==(GenPool::Handle _other) const { return m_handle == _other; }            \
        bool operator==(HandleName _other) const { return m_handle == _other.m_handle; }        \
        bool operator<(HandleName _other) const { return m_handle < _other.m_handle; }          \
    }

    /**
     * @brief Thread safe generational pool.
     *
     * @details
     * Reads are completely lock-free, writes are locked to limit unnecessary complexity. As this container is meant to
     * be read very frequently during a frame and not to have many writes, this should provide great performance.
     *
     * Compared to a non-thread safe design with a single contiguous array, there is a little overhead due to both the
     * two atomic loads (one for the segment, one for the generation) and the final index computation.
     * In most cases, however, this overhead is mostly negligible: a few ALU cycles for the indexing, plus no cache
     * invalidation on the atomics. Cache invalidations are rare and can only be triggered by adjacent atomic write ops.
     * The added thread safety should more than make up for it.
     */
    template <class HotDataStruct, class ColdDataStruct = void, class Allocator = AllocatorInstance>
    class GenerationalPool
    {
        struct HotDataWithGeneration
        {
            HotDataStruct m_userHotData;
            u32 m_generation;
        };

        using HotData = eastl::conditional_t<GenPool::IsValidIntrusiveGeneration<HotDataStruct>, HotDataStruct, HotDataWithGeneration>;

        static constexpr size_t kInitialSizePot = 5;
        static constexpr u64 kInitialSize = 1 << kInitialSizePot;
        static constexpr u64 kMaxSize = (1 << GenPool::kIndexBits) - (1 << kInitialSizePot);
        static constexpr bool kHasColdData = !eastl::is_same_v<void, ColdDataStruct>;

        using Segment = HotData*;

        // Segment size will double every new segment, and we don't want to grow beyond the max index
        static constexpr size_t kSegmentCount = GenPool::kIndexBits - kInitialSizePot;

        Allocator m_allocator {};

        eastl::array<std::atomic<Segment>, kSegmentCount> m_segments;

        std::atomic<size_t> m_size { 0 };

        eastl::vector<u32> m_availableIndices;
        eastl::vector<u32> m_availableIndicesDeferred;

        alignas(Threads::kCacheLineSize) SpinLock m_lock;

        void _Grow(size_t _index);

        static size_t GetSegmentIndex(size_t _index);
        static size_t GetLocalIndex(size_t _index, size_t _segmentIndex);
        static ColdDataStruct* GetColdData(Segment _segment, size_t _segmentIndex) requires kHasColdData;
        static HotDataStruct* GetHotData(Segment _segment, size_t _localIndex, u32 _generation);

    public:
        explicit GenerationalPool(const Allocator &_allocator);

        ~GenerationalPool();

        HotDataStruct* Get(GenPool::Handle _handle) const;
        eastl::pair<HotDataStruct*, ColdDataStruct*> GetAll(GenPool::Handle _handle) const;

        ColdDataStruct* GetCold(const GenPool::Handle _handle) const requires kHasColdData
        {
            return GetAll(_handle).second;
        }

        GenPool::Handle Allocate();
        bool Free(const GenPool::Handle &_handle,
                  HotDataStruct *_hotCopy = nullptr,
                  ColdDataStruct *_coldCopy = nullptr);

        void FlushDeferredFrees();

        [[nodiscard]] size_t GetSize() const { return m_size.load(std::memory_order::relaxed); }

        [[nodiscard]] const Allocator& GetAllocator() const { return m_allocator; }
    };

} // KryneEngine