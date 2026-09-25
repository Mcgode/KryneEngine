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
        const moodycamel::ProducerToken producerToken(m_idQueue);

        for (u16 i = 0; i < static_cast<u16>(kPoolSize); i++)
        {
            m_idQueue.enqueue(producerToken, i);
        }
    }

    SyncCounterId SyncCounterPool::AcquireCounter(const u32 _initialValue)
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
            // here too, this reset could run concurrently with that cleanup.
            const auto lock = entry.m_mutex.AutoLock();
            KE_ASSERT_MSG(entry.m_waitingJobs.empty(), "Reusing a sync counter slot with pending waiters");
            entry.m_counter.store(initValue, std::memory_order_release);
            const u32 generation = entry.m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

            return { id, generation };
        }
        return kInvalidSyncCounterId;
    }

    bool SyncCounterPool::AddWaitingJob(const SyncCounterId _id, FiberJob *_newJob)
    {
        VERIFY_OR_RETURN(_id != kInvalidSyncCounterId, true);

        auto& entry = m_entries[_id.GetIndex()];
        const auto lock = entry.m_mutex.AutoLock();

        // A stale id -- one whose slot has already been freed and reacquired for a newer, unrelated
        // batch by the time this call runs -- has nothing left to validly wait on. Reject it instead
        // of touching that unrelated batch's state.
        VERIFY_OR_RETURN(entry.m_generation.load(std::memory_order_relaxed) == _id.GetGeneration(), true);

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

            entry.m_waitingJobs.push_back(_newJob);

            return false;
        }
    }

    u32 SyncCounterPool::DecrementCounterValue(const SyncCounterId _id)
    {
        VERIFY_OR_RETURN(_id != kInvalidSyncCounterId, 0);

        auto& entry = m_entries[_id.GetIndex()];

        // Reject a decrement whose generation no longer matches this slot's current one: its batch
        // has already been superseded (freed and reacquired for a newer, unrelated one), so applying
        // it here would silently corrupt that unrelated batch's count instead of whatever bug caused
        // this decrement to arrive late in the first place. Deliberately a lock-free check (not
        // mutex-guarded against AcquireCounter()) to keep this, the hot path (one call per completed
        // job), cheap; the class of bug this guards against is rare by construction -- a healthy
        // batch's last decrement is exactly what unblocks its own FreeCounter() call, so there is
        // nothing left to decrement late for that same generation.
        VERIFY_OR_RETURN(entry.m_generation.load(std::memory_order_acquire) == _id.GetGeneration(), 0);

        const s32 value = --entry.m_counter;
        if (KE_VERIFY(value >= 0))
        {
            if (value == 0)
            {
                const auto lock = entry.m_mutex.AutoLock();

                // Re-check the generation now that the lock is actually held: the generation check
                // above and the atomic decrement itself are both lock-free, so an unbounded amount
                // of time can pass between computing value == 0 and actually getting this lock. In
                // that window, our own waiter (if it had one) can only have left via
                // AddWaitingJob()'s fast path (it reads entry.m_counter == 0 without needing this
                // lock) -- and if it did, AcquireCounter() may already have reused this slot for an
                // unrelated, newer generation, whose own waiter (if any) is now the one sitting in
                // entry.m_waitingJobs. Without this re-check, a late decrement like this one would
                // wake that newer generation's waiter before its own counter had actually reached 0
                // -- confirmed live: it unblocked a batch whose FreeCounter() then ran while its
                // counter was still untouched, which the id-generation check above only catches once
                // *that* batch's real jobs eventually try to decrement a long-since-recycled id.
                if (entry.m_generation.load(std::memory_order_acquire) == _id.GetGeneration())
                {
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
            }

            return value;
        }

        return 0;
    }

    void SyncCounterPool::FreeCounter(SyncCounterId &_id)
    {
        VERIFY_OR_RETURN_VOID(_id != kInvalidSyncCounterId);

        // The caller only gets here after observing the counter reach 0, which happens while
        // DecrementCounterValue() still holds entry.m_mutex (it calls QueueJob() on the waiting
        // job(s) from inside that critical section, so they can resume and reach here before that
        // section has finished). Taking the lock here -- even though there's nothing left to read
        // or write in the entry -- forces this to wait for that section to fully finish before the
        // id is allowed back into m_idQueue, matching the lock AcquireCounter() takes before
        // resetting the slot for a new owner.
        {
            auto& entry = m_entries[_id.GetIndex()];
            const auto lock = entry.m_mutex.AutoLock();
            VERIFY_OR_RETURN_VOID(entry.m_generation.load(std::memory_order_acquire) == _id.GetGeneration());
        }

        m_idQueue.enqueue(_id.GetIndex());
        _id = kInvalidSyncCounterId;
    }

    SyncCounterPool::AutoSyncCounter::~AutoSyncCounter()
    {
        m_pool->FreeCounter(m_id);
    }

    SyncCounterPool::AutoSyncCounter::AutoSyncCounter(AutoSyncCounter &&_other) noexcept
        : m_id(_other.m_id)
        , m_pool(_other.m_pool)
    {
        _other.m_id = kInvalidSyncCounterId;
        _other.m_pool = nullptr;
    }

    SyncCounterPool::AutoSyncCounter::AutoSyncCounter(const SyncCounterId _id, SyncCounterPool *_pool)
        : m_id(_id)
        , m_pool(_pool)
    {}

    SyncCounterPool::AutoSyncCounter SyncCounterPool::AcquireAutoCounter(const u32 _count)
    {
        return AutoSyncCounter(AcquireCounter(_count), this);
    }
} // KryneEngine
