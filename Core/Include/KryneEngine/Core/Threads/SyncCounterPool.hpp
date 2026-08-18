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

        inline bool operator ==(const SyncCounterId& _other) const { return m_id == _other.m_id; }
        inline bool operator !=(const SyncCounterId& _other) const { return !(*this == _other); }

    private:
        s32 m_id = -1;

        SyncCounterId(s32 _value): m_id(_value) {}

        operator s32() const { return m_id; }
    };
    static const SyncCounterId kInvalidSyncCounterId = SyncCounterId();

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
            AutoSyncCounter(AutoSyncCounter&& _other);

            AutoSyncCounter& operator=(const AutoSyncCounter& _other) = delete;
            AutoSyncCounter& operator=(AutoSyncCounter&& _other) = delete;

            [[nodiscard]] const SyncCounterId &GetId() const { return m_id; }

        private:
            AutoSyncCounter(SyncCounterId _id, SyncCounterPool* _pool);

            SyncCounterId m_id;
            SyncCounterPool* m_pool;
        };

        AutoSyncCounter&& AcquireAutoCounter(u32 _count);

    private:
        struct Entry
        {
            std::atomic<s32> m_counter;
            eastl::fixed_vector<FiberJob*, 4> m_waitingJobs;
            LightweightMutex m_mutex;
        };

        static constexpr u16 kPoolSize = 1024;
        eastl::array<Entry, kPoolSize> m_entries;

        moodycamel::ConcurrentQueue<u16> m_idQueue;
    };
} // KryneEngine