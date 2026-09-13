/**
 * @file
 * @author Max Godefroy
 * @date 03/07/2022.
 */

#pragma once

#include <atomic>
#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <moodycamel/concurrentqueue.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

namespace KryneEngine
{
    struct SyncCounterId
    {
        friend class SyncCounterPool;

    public:
        SyncCounterId() = default;

        inline bool operator ==(const SyncCounterId& _other) const { return Raw() == _other.Raw(); }
        inline bool operator !=(const SyncCounterId& _other) const { return !(*this == _other); }

        // Bit-cast conversion: an opaque 32-bit token for code that just needs to stash this
        // somewhere and hand it back later (e.g. Box3D packs one into a void* userdata pointer via
        // reinterpret_cast, see Modules/Box3D/Src/Context.cpp). Never use this to index into the
        // pool -- that's exactly the mistake that let a stale id silently alias a reused slot; see
        // GetIndex()/GetGeneration(), which is what SyncCounterPool itself uses.
        explicit operator s32() const { return Raw(); }

    private:
        // kIndexBits must cover SyncCounterPool::kPoolSize (static_assert'd in SyncCounterPool.cpp).
        // Packing {index, generation} into one 32-bit value -- rather than a bare slot index, as
        // before -- mirrors GenPool::Handle (Core/Include/KryneEngine/Core/Memory/GenerationalPool.hpp):
        // a stale id (one whose slot has since been freed and reacquired for a new, unrelated batch)
        // now carries an old generation that no longer matches the slot's current one, so
        // SyncCounterPool can detect and reject it instead of silently corrupting that new batch's
        // counter.
        static constexpr u32 kIndexBits = 15;
        static constexpr u32 kGenerationBits = 32 - kIndexBits;

        u32 m_index : kIndexBits = (1u << kIndexBits) - 1;
        u32 m_generation : kGenerationBits = (1u << kGenerationBits) - 1;

        SyncCounterId(const u16 _index, const u32 _generation)
            : m_index(_index)
            , m_generation(_generation)
        {}

        [[nodiscard]] s32 Raw() const { return *reinterpret_cast<const s32*>(this); }
        [[nodiscard]] u16 GetIndex() const { return static_cast<u16>(m_index); }
        [[nodiscard]] u32 GetGeneration() const { return m_generation; }
    };
    static_assert(sizeof(SyncCounterId) == sizeof(s32));
    static constexpr SyncCounterId kInvalidSyncCounterId {};

    class FiberJob;

    class SyncCounterPool
    {
    public:
        SyncCounterPool();

        SyncCounterId AcquireCounter(u32 _initialValue);

        bool AddWaitingJob(SyncCounterId _id, FiberJob* _newJob);

        u32 DecrementCounterValue(SyncCounterId _id);

        void FreeCounter(SyncCounterId &_id);

        class AutoSyncCounter
        {
            friend SyncCounterPool;

        public:
            ~AutoSyncCounter();

            AutoSyncCounter(const AutoSyncCounter& _other) = delete;
            AutoSyncCounter(AutoSyncCounter&& _other) noexcept;

            AutoSyncCounter& operator=(const AutoSyncCounter& _other) = delete;
            AutoSyncCounter& operator=(AutoSyncCounter&& _other) = delete;

            [[nodiscard]] const SyncCounterId& GetId() const { return m_id; }

        private:
            AutoSyncCounter(SyncCounterId _id, SyncCounterPool* _pool);

            SyncCounterId m_id;
            SyncCounterPool* m_pool;
        };

        AutoSyncCounter AcquireAutoCounter(u32 _count);

    private:
        struct Entry
        {
            std::atomic<s32> m_counter;
            eastl::fixed_vector<FiberJob*, 4> m_waitingJobs;
            LightweightMutex m_mutex;
            // Bumped each time this slot is handed out by AcquireCounter(). Paired with the
            // generation stamped into the SyncCounterId it returns: any later call made with a
            // SyncCounterId whose generation doesn't match the slot's current one is operating on a
            // stale handle (the slot has since been freed and reused for an unrelated batch) and is
            // rejected rather than corrupting that unrelated batch's state.
            std::atomic<u32> m_generation { 0 };
        };

        static constexpr u16 kPoolSize = 1024;
        static_assert(kPoolSize <= (1u << SyncCounterId::kIndexBits));
        eastl::array<Entry, kPoolSize> m_entries;

        moodycamel::ConcurrentQueue<u16> m_idQueue;
    };
} // KryneEngine