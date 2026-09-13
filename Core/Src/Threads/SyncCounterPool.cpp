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
            auto& entry = m_entries[id];

            // Reuse of a pool slot must be serialized against DecrementCounterValue()'s own
            // critical section below (which runs when the *previous* owner's counter hits 0):
            // that section calls QueueJob() on the waiting job(s) while still holding this lock,
            // so the job can resume and call FreeCounter() (returning this id to m_idQueue) before
            // the section has finished iterating/clearing m_waitingJobs. Without taking the lock
            // here too, this reset could run concurrently with that cleanup and/or with a stale
            // decrement still in flight for the previous generation, corrupting m_counter (e.g.
            // driving it negative once this new generation's own decrements also apply).
            const auto lock = entry.m_mutex.AutoLock();
            KE_ASSERT_MSG(entry.m_waitingJobs.empty(), "Reusing a sync counter slot with pending waiters");
            entry.m_counter.store(initValue, std::memory_order_release);

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

        auto& entry = m_entries[static_cast<s32>(_id)];

        // The caller only gets here after observing the counter reach 0, which happens while
        // DecrementCounterValue() still holds entry.m_mutex (it calls QueueJob() on the waiting
        // job(s) from inside that critical section, so they can resume and reach here before that
        // section has finished). Taking the lock here -- even though there's nothing left to read
        // or write in the entry -- forces this to wait for that section to fully finish before the
        // id is allowed back into m_idQueue, matching the lock AcquireCounter() now takes before
        // resetting the slot for a new owner.
        {
            const auto lock = entry.m_mutex.AutoLock();
        }

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