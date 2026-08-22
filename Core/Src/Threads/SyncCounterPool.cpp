/**
 * @file
 * @author Max Godefroy
 * @date 03/07/2022.
 */

#include "KryneEngine/Core/Threads/SyncCounterPool.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Threads/FibersManager.hpp"

namespace KryneEngine
{
    SyncCounterPool::SyncCounterPool()
    {
        moodycamel::ProducerToken producerToken(m_idQueue);

        for (u16 i = 0; i < (u16)kPoolSize; i++)
        {
            m_idQueue.enqueue(producerToken, i);
        }
    }

    SyncCounterId SyncCounterPool::AcquireCounter(u32 _initialValue)
    {
        const s32 initValue = static_cast<s32>(_initialValue);
        VERIFY_OR_RETURN(initValue > 0, kInvalidSyncCounterId);

        u16 id;
        if (m_idQueue.try_dequeue(id))
        {
            m_entries[id].m_counter = initValue;
            return { id };
        }
        return kInvalidSyncCounterId;
    }

    bool SyncCounterPool::AddWaitingJob(SyncCounterId _id, FiberJob *_newJob)
    {
        VERIFY_OR_RETURN(static_cast<s32>(_id) >= 0 && static_cast<s32>(_id) < kPoolSize, true);

        auto& entry = m_entries[static_cast<s32>(_id)];
        if (entry.m_counter == 0)
        {
            return true;
        }
        const auto lock = entry.m_mutex.AutoLock();

        if (entry.m_counter == 0)
        {
            // By the time we locked, the counter was decremented to 0.
            // We can thus continue the job, no need to suspend and queue it
            return true;
        }
        else
        {
            // Manually pause here, to avoid auto re-queueing when yielding.
            // The status update is performed here, to avoid a data race.
            _newJob->m_status.store(FiberJob::Status::Paused, std::memory_order_release);

            m_entries[static_cast<s32>(_id)].m_waitingJobs.push_back(_newJob);

            return false;
        }
    }

    u32 SyncCounterPool::DecrementCounterValue(SyncCounterId _id)
    {
        VERIFY_OR_RETURN(static_cast<s32>(_id) >= 0 && static_cast<s32>(_id) < kPoolSize, 0);

        auto& entry = m_entries[static_cast<s32>(_id)];

        const s32 value = --entry.m_counter;
        if (KE_VERIFY(value >= 0))
        {
            if (value == 0)
            {
                // Use lock as
                const auto lock = entry.m_mutex.AutoLock();

                auto* fibersManager = FibersManager::GetInstance();
                for (auto* job: entry.m_waitingJobs)
                {
                    const s32 dependencyJobsRunning = job->m_dependencyJobsRunning.fetch_sub(1, std::memory_order_acq_rel);
                    if (dependencyJobsRunning == 1)
                    {
                        fibersManager->QueueJob(job);
                    }
                }
                entry.m_waitingJobs.clear();
            }

            return value;
        }

        return 0;
    }

    void SyncCounterPool::FreeCounter(SyncCounterId &_id)
    {
        VERIFY_OR_RETURN_VOID(static_cast<s32>(_id) >= 0 && static_cast<s32>(_id) < kPoolSize);

        m_idQueue.enqueue(static_cast<s32>(_id));
        _id = kInvalidSyncCounterId;
    }

    SyncCounterPool::AutoSyncCounter::~AutoSyncCounter()
    {
        m_pool->FreeCounter(m_id);
    }

    SyncCounterPool::AutoSyncCounter::AutoSyncCounter(SyncCounterPool::AutoSyncCounter &&_other)
        : m_id(_other.m_id)
        , m_pool(_other.m_pool)
    {
        _other.m_id = kInvalidSyncCounterId;
        _other.m_pool = nullptr;
    }

    SyncCounterPool::AutoSyncCounter::AutoSyncCounter(SyncCounterId _id, SyncCounterPool *_pool)
        : m_id(_id)
        , m_pool(_pool)
    {
    }

    SyncCounterPool::AutoSyncCounter &&SyncCounterPool::AcquireAutoCounter(u32 _count)
    {
        const auto syncCounter = AcquireCounter(_count);
        return eastl::move(AutoSyncCounter(syncCounter, this));
    }
} // KryneEngine