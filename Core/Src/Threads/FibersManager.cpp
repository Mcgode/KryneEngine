/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#include "KryneEngine/Core/Threads/FibersManager.hpp"

#include <chrono>
#include <condition_variable>

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Core/Threads/FiberJob.hpp"
#include "KryneEngine/Core/Threads/FiberThread.hpp"
#include "KryneEngine/Core/Threads/FiberTls.inl"
#include "Threads/Internal/FiberContext.hpp"

namespace KryneEngine
{
    FibersManager::FibersManager(const s32 _requestedThreadCount, AllocatorInstance _allocator)
        : m_jobProducerTokens(_allocator)
        , m_jobConsumerTokens(_allocator)
        , m_fiberThreads(_allocator)
        , m_statuses(_allocator)
        , m_baseContexts(_allocator)
    {
        KE_ZoneScopedFunction("FibersManager::FibersManager()");

        UpdateRoundRobinTotal();

        m_contextAllocator = _allocator.New<FiberContextAllocator>(_allocator);

        u16 fiberThreadCount;
        if (_requestedThreadCount <= 0)
        {
            // Always at least 1 thread, the current thread
            fiberThreadCount = eastl::max<u16>(std::thread::hardware_concurrency(), 1);

            if (_requestedThreadCount < 0)
            {
                // Make sure we can't go below 1
                fiberThreadCount -= eastl::min<u16>(abs(_requestedThreadCount), _requestedThreadCount - 1);
            }
        }
        else
        {
            fiberThreadCount = _requestedThreadCount;
        }

        KE_ASSERT_MSG(fiberThreadCount > 0, "You need at least one fiber thread");

        // Resize array first!
        // This size is used to init the FiberTls objects,
        m_fiberThreads.Resize(fiberThreadCount);

        // Init FiberTls objects before initializing the threads, to avoid racing conditions.
        {
            m_jobProducerTokens.InitFunc(
                this,
                [this](JobProducerTokenArray& _array)
                {
                    for (u64 i = 0; i < _array.size(); i++)
                    {
                        // Do in-place memory init, else it will try to interpret uninitialized memory as a valid
                        // object.
                        ::new (&_array[i]) moodycamel::ProducerToken(m_jobQueues[i]);
                    }
                });

            m_jobConsumerTokens.InitFunc(
                this,
                [this](JobConsumerTokenArray& _array)
                {
                    for (u64 i = 0; i < _array.size(); i++)
                    {
                        // Do in-place memory init, else it will try to interpret uninitialized memory as a valid
                        // object.
                        ::new (&_array[i]) moodycamel::ConsumerToken(m_jobQueues[i]);
                    }
                });

            m_statuses.InitDefault(this);
            m_baseContexts.InitDefault(this);

            for (u32 i = 0; i < fiberThreadCount; i++)
            {
                m_baseContexts.Load(i).m_name.sprintf("Base fiber %d", i);
            }
        }

        for (u16 i = 0; i < fiberThreadCount; i++)
        {
            m_fiberThreads.Init(i, this, i);
        }
    }

    FibersManager::~FibersManager()
    {
        for (auto& fiberThread : m_fiberThreads)
        {
            fiberThread.Stop(*this);
        }
        // Make sure to end and join all the fiber threads before anything else.
        m_fiberThreads.Clear();
        m_fiberThreads.GetAllocator().Delete(m_contextAllocator);
    }

    FibersManager* FibersManager::GetInstance() { return s_manager; }

    void FibersManager::SetInstance(FibersManager* _instance) { s_manager = _instance; }

    FiberJob* FibersManager::GetCurrentJob() { return m_statuses.Load().m_currentJob; }

    SyncCounterId FibersManager::InitAndBatchJobs(const FiberJob::Desc& _desc)
    {
        if (_desc.m_jobCount == 0)
        {
            return kInvalidSyncCounterId;
        }

        const auto syncCounter = m_syncCounterPool.AcquireCounter(_desc.m_jobCount);

        VERIFY_OR_RETURN(syncCounter != kInvalidSyncCounterId, kInvalidSyncCounterId);

        for (u16 i = 0; i < _desc.m_jobCount; i++)
        {
            auto* job = m_fiberThreads.GetAllocator().New<FiberJob>();
            job->m_function = eastl::move(_desc.m_function);
            job->m_jobIndex = i;
            job->m_priority = _desc.m_priority;
            job->m_bigStack = _desc.m_useBigStack;
            job->m_associatedCounterId = syncCounter;
            QueueJob(job);
        }

        return syncCounter;
    }

    void FibersManager::InitAndBatchJobsNoCounter(const FiberJob::Desc& _desc)
    {
        if (_desc.m_jobCount == 0)
        {
            return;
        }

        for (u16 i = 0; i < _desc.m_jobCount; i++)
        {
            auto* job = m_fiberThreads.GetAllocator().New<FiberJob>();
            job->m_function = eastl::move(_desc.m_function);
            job->m_jobIndex = i;
            job->m_priority = _desc.m_priority;
            job->m_bigStack = _desc.m_useBigStack;
            job->m_associatedCounterId = kInvalidSyncCounterId;
            QueueJob(job);
        }
    }

    SyncCounterPool::AutoSyncCounter FibersManager::AcquireAutoSyncCounter(const u32 _count)
    {
        return m_syncCounterPool.AcquireAutoCounter(_count);
    }

    void FibersManager::QueueJob(FiberJob* _job)
    {
        VERIFY_OR_RETURN_VOID(_job != nullptr);

        KE_ASSERT(_job->CanRun());

        const u8 priorityId = static_cast<u8>(_job->GetPriorityType());
        if (FiberThread::IsFiberThread())
        {
            const moodycamel::ProducerToken& producerToken = m_jobProducerTokens.Load()[priorityId];
            KE_ASSERT(_job != nullptr);
            m_jobQueues[priorityId].enqueue(producerToken, _job);
        }
        else
        {
            KE_ASSERT(_job != nullptr);
            m_jobQueues[priorityId].enqueue(_job);
        }

        // Deliberately lock-free: this is meant to be cheap. A thread that's genuinely asleep in
        // ThreadWaitForJob() wakes on this notify regardless; ThreadWaitForJob()'s own bounded wait
        // is what covers the rare case where this races ahead of a thread that hasn't registered as
        // a waiter yet (see its comment for why that isn't worth closing here).
        m_waitVariable.notify_one();
    }

    void FibersManager::WaitForCounters(const eastl::span<const SyncCounterId> _syncCounters)
    {
        if (_syncCounters.empty())
        {
            return;
        }

        if (FiberThread::IsFiberThread())
        {
            auto* currentJob = GetCurrentJob();
            currentJob->m_dependencyJobsRunning.fetch_add(
                static_cast<s32>(_syncCounters.size()), std::memory_order_acq_rel);

            // Register every counter before yielding on any of them. Yielding as soon as the first
            // AddWaitingJob() call returns false (as this used to) leaves this function -- the
            // context switch doesn't return here until something calls QueueJob() on this job, which
            // can only happen once all of m_dependencyJobsRunning's registered dependencies resolve --
            // so counters after the first unsatisfied one are never registered at all. This job then
            // has nothing waiting on it for those counters, and the one counter it IS registered for
            // can only ever bring m_dependencyJobsRunning down from its full initial count, never down
            // to the 1 -> 0 transition DecrementCounterValue() requeues on: a permanent hang whenever
            // waiting on 2+ counters where the first one isn't already resolved.
            bool needsToWait = false;
            for (const auto& syncCounter : _syncCounters)
            {
                if (!m_syncCounterPool.AddWaitingJob(syncCounter, currentJob))
                {
                    needsToWait = true;
                }
                else
                {
                    currentJob->m_dependencyJobsRunning.fetch_sub(1, std::memory_order_acq_rel);
                }
            }

            if (needsToWait)
            {
                YieldJob();
            }
        }
        else
        {
            KE_ZoneScopedFunction("FibersManager::WaitForCounters");

            // Deliberately a plain std::mutex, not TracyLockable: tracy::Lockable<T>::unlock() calls
            // the wrapped mutex's unlock() first and only *then* touches `this` for its own profiling
            // bookkeeping. Since this mutex is a stack-local about to be destroyed the instant the
            // waiting thread below observes the unlock and returns, that trailing touch would be a
            // use-after-scope. A plain std::mutex has no such trailing access after unlock() returns.
            std::mutex waitMutex;
            std::condition_variable_any waitVariable;
            std::atomic<bool> done = false;

            InitAndBatchJobsNoCounter({
                .m_function = [&waitMutex, &waitVariable, &done, _syncCounters](u16)
                {
                    GetInstance()->WaitForCounters(_syncCounters);

                    // Set the flag and notify while still holding the lock. The waiting thread can only
                    // return from wait() (and so only tear down this function's stack frame, which is
                    // what waitMutex/waitVariable/done live on) once it has re-acquired waitMutex, which
                    // can't happen until this lock_guard's destructor releases it below -- and nothing
                    // here touches any of them afterwards.
                    const std::lock_guard lock(waitMutex);
                    done.store(true, std::memory_order_release);
                    waitVariable.notify_one();
                },
                .m_priority = FiberJob::Priority::Medium
            });

            // Wait on a predicate (rather than a bare wait()) so that a lost wakeup (notify_one() racing
            // ahead of wait()) or a spurious wakeup can never cause this to return before the job above has
            // actually completed and signalled.
            std::unique_lock lock(waitMutex);
            waitVariable.wait(lock, [&done] { return done.load(std::memory_order_acquire); });
        }
    }

    void FibersManager::ResetCounter(SyncCounterId _syncCounter) { m_syncCounterPool.FreeCounter(_syncCounter); }

    void FibersManager::YieldJob(FiberJob* _nextJob)
    {
        const auto fiberIndex = FiberThread::GetCurrentFiberThreadIndex();
        auto* currentJob = m_statuses.Load(fiberIndex).m_currentJob;

        if (currentJob != nullptr && currentJob->GetStatus() == FiberJob::Status::Running)
        {
            currentJob->m_status.store(FiberJob::Status::Paused, std::memory_order_release);
            QueueJob(currentJob);
        }

        IF_NOT_VERIFY(_nextJob == nullptr || _nextJob->CanRun()) { _nextJob = nullptr; }

        m_fiberThreads[fiberIndex].SwitchToNextJob(this, currentJob, _nextJob);
    }

    bool FibersManager::RetrieveNextJob(FiberJob*& job_, const u16 _fiberIndex)
    {
        JobConsumerTokenArray& consumerTokens = m_jobConsumerTokens.Load(_fiberIndex);
        u32& roundRobinProgress = m_statuses.Load(_fiberIndex).m_priorityRoundRobinProgress;

        // Set up queue indices to respect round robin priority.
        // Resuming jobs queue is always first.
        u32 queueIndices[kJobQueuesCount] = { 0 };
        {
            u32 cumulated = 0;
            for (u32 i = 0; i < kPrioritiesCount; i++)
            {
                if (cumulated + m_priorityRoundRobinIterations[i] > roundRobinProgress)
                {
                    queueIndices[1] = i + 1;
                    for (u32 j = 1; j < kPrioritiesCount; j++)
                        queueIndices[j + 1] = ((i + j) % kPrioritiesCount) + 1;
                    break;
                }
                cumulated += m_priorityRoundRobinIterations[i];
            }
        }
        for (s64 i = 0; i < static_cast<s64>(kJobQueuesCount); i++)
        {
            const u32 queueIndex = queueIndices[i];

            if (m_jobQueues[queueIndex].try_dequeue(consumerTokens[queueIndex], job_))
            {
                KE_ASSERT(job_ != nullptr);

                // Is this job still somebody's current job right now? A job that's about to yield
                // (waiting on a counter, or just finishing) is marked Paused/its dependency resolved
                // -- and so becomes legally dequeue-able here -- *before* the thread running it has
                // actually context-switched away: AddWaitingJob() sets its status, and a different
                // thread's DecrementCounterValue() can call QueueJob() on it, while the owning thread
                // is still mid-way through YieldJob()/SwitchToNextJob(), still physically on that
                // job's context. Handing it back as a "next job" here -- to THIS fiber (self-collision,
                // the case this originally only checked for) or, just as dangerously, to any OTHER
                // fiber concurrently doing the exact same lookup -- makes FiberContext::SwapContext()
                // try to lock a context mutex that's still held by whoever is currently running it.
                // The self case deadlocks outright (a thread re-locking its own held mutex); the
                // cross-thread case is worse: if fiber A is simultaneously handed fiber B's current
                // job while fiber B is handed fiber A's, each blocks waiting for a mutex the other
                // is holding and will only release once it finishes entering the context it's
                // blocked on -- a circular wait, and a real deadlock between two otherwise-healthy
                // fibers, not just a fiber colliding with itself.
                // So: check this job against every fiber's current job, not just this one's own, and
                // if it matches any of them, put it back in its queue and keep scanning the other
                // queues for something else to run instead (don't roll back `i`, to avoid spinning
                // forever if it's the only job in this queue). If nothing else is found, this fiber
                // will switch out to its base context, and the job will be retrieved (and safely
                // resumed) on a later pass, once it is no longer anyone's current job.
                const bool isStillSomeonesCurrentJob = job_->m_ownerThread.load(std::memory_order::acquire) != nullptr;

                if (isStillSomeonesCurrentJob)
                {
                    QueueJob(job_);
                    job_ = nullptr;
                    continue;
                }

                if (!job_->HasContextAssigned())
                {
                    KE_ASSERT(job_->GetStatus() == FiberJob::Status::PendingStart);

                    u16 id;
                    if (m_contextAllocator->Allocate(job_->m_bigStack, id))
                    {
                        job_->SetContext(id, m_contextAllocator->GetContext(id));
                    }
                    else
                    {
                        // Out of fiber stacks for this size class right now. Put the job back rather
                        // than handing it back to the caller with no context assigned -- SwitchToNextJob()
                        // would then try to SwapContext() into a null pointer. Don't roll back `i` to
                        // retry this same queue (as the CanRun() == false case below does): the pool being
                        // exhausted isn't specific to this one job, so retrying here would likely just
                        // pull another same-size-class job out of the same queue and fail again, spinning
                        // instead of giving the scheduler a chance to actually free one up elsewhere.
                        QueueJob(job_);
                        job_ = nullptr;
                        continue;
                    }
                }
                else if (!job_->CanRun())
                {
                    // A job only ever reaches the shared queue while PendingStart or Paused (QueueJob()
                    // asserts CanRun() on entry), and the ownership check above already routes away any
                    // job that's still actively Running under some other thread's ownership. So a job
                    // that both has a context assigned and fails CanRun() here can only be one that
                    // finished after being dequeued from this queue and before this check ran -- which
                    // isn't possible either, since RunFiber()'s own post-completion YieldJob() call
                    // never re-queues a Finished job in the first place (see its "GetStatus() == Running"
                    // guard). In other words, nothing in the scheduler should be able to produce this
                    // case; treat it as a scheduler invariant violation rather than silently discarding
                    // job_ (which used to leak the job, its context id, and -- critically -- its
                    // associated sync counter, hanging anything still waiting on it). Still route it
                    // through the normal finalize path as a safety net: if this invariant is ever broken
                    // by a future change, at least the job gets torn down correctly instead of leaking.
                    KE_ASSERT_MSG(
                        false,
                        "RetrieveNextJob() dequeued a job that is neither runnable nor still owned -- "
                        "scheduler invariant violated");
                    FinalizeLeavingJob(job_);
                    job_ = nullptr;
                    i--; // Roll back index to try retrieving again from this queue.
                    continue;
                }

                // Update round robin progress, if relevant.
                if (i == 1)
                {
                    roundRobinProgress++;
                }
                else if (i > 0)
                {
                    u32 cumulated = 0;
                    const u32 priority = queueIndex - 1;
                    for (u32 j = 0; j < kPrioritiesCount; j++)
                    {
                        if (j < priority)
                            cumulated += m_priorityRoundRobinIterations[j];
                    }
                    roundRobinProgress = cumulated + 1;
                }
                roundRobinProgress %= m_priorityRoundRobinTotal;

                return true;
            }
        }

        roundRobinProgress = 0;
        return false;
    }

    void FibersManager::FinalizeLeavingJob(FiberJob* _job)
    {
        // This must run BEFORE the context we are switching away from has its mutex unlocked
        // (i.e. before FiberContext::SwapContext()/RunFiber() make it resumable again), while this
        // thread still has exclusive access to _job. Running it afterwards (as OnContextSwitched()
        // used to, right after the swap) raced against another thread immediately resuming this
        // same (still-valid, not-yet-finished-at-the-time-of-the-race) job, finishing it, and
        // triggering its own, legitimate finalization concurrently with this one -- a genuine
        // double free/use-after-free of the job, not just of its bookkeeping.
        if (_job != nullptr && _job->GetStatus() == FiberJob::Status::Finished)
        {
            KE_ASSERT_MSG(!_job->m_finalized.exchange(true, std::memory_order_acq_rel), "Job finalized twice");

            if (_job->m_associatedCounterId != kInvalidSyncCounterId)
            {
                // Decrement counter
                m_syncCounterPool.DecrementCounterValue(_job->m_associatedCounterId);
            }

            m_contextAllocator->Free(_job->m_contextId);

            _job->ResetContext();
            m_fiberThreads.GetAllocator().Delete(_job);
        }
    }

    void FibersManager::OnContextSwitched()
    {
        const auto fiberIndex = FiberThread::GetCurrentFiberThreadIndex();

        Status& status = m_statuses.Load(fiberIndex);

        // status.m_currentJob is safe to dereference here: FiberThread::SwitchToNextJob() already
        // nulls it out itself, *before* calling FinalizeLeavingJob(), whenever that call is about to
        // delete the job it's handed (see the comment there) -- so by the time this runs, it's
        // either a job that's still alive, or already nullptr. It can't instead be re-checked here,
        // after the fact: this same function is also FiberContext::RunFiber()'s entry-point
        // registration for a brand new context, which has no access to whatever FiberThread::
        // SwitchToNextJob() call initiated the jump that landed here, and so no other way to learn
        // whether that job survived.
        if (status.m_currentJob != nullptr)
            status.m_currentJob->m_ownerThread.store(nullptr, std::memory_order_release);

        status.m_currentJob = status.m_nextJob;

        if (status.m_currentJob != nullptr)
            status.m_currentJob->m_ownerThread.store(&m_fiberThreads[fiberIndex], std::memory_order_release);

        status.m_nextJob = nullptr;
    }

    void FibersManager::ThreadWaitForJob()
    {
        std::unique_lock lock(m_waitMutex);

        // No predicate: a plain wait_for() already wakes immediately on *any* notify_one()/
        // notify_all() -- from QueueJob() queuing a job or FiberThread::Stop() requesting shutdown,
        // both deliberately lock-free -- and the caller (_TryRetrieveNextJob) re-checks RetrieveNextJob()
        // and m_shouldStop right after this returns regardless of why it returned, so there is nothing
        // useful for a predicate to re-test here. (An earlier version of this used _shouldStop as the
        // predicate, which was wrong: wait_for(lock, timeout, pred) re-checks only pred() after being
        // woken, so a job-queued notify would wake this thread and then put it straight back to sleep
        // for whatever was left of the bound, since _shouldStop was still false -- turning ordinary
        // job pickup latency into the full bound.)
        // The bound only matters for one narrow race: a notify landing in the gap between this thread
        // deciding to wait and actually registering as a waiter, with nothing registered to receive
        // it. That's what turns an already-rare missed wakeup into a bounded delay instead of a
        // permanent hang.
        constexpr auto kMaxWait = std::chrono::milliseconds(500);
        m_waitVariable.wait_for(lock, kMaxWait);
    }

    void FibersManager::UpdateRoundRobinTotal()
    {
        m_priorityRoundRobinTotal = 0;
        for (u32 i = 0; i < m_priorityRoundRobinIterations.size(); ++i)
            m_priorityRoundRobinTotal += m_priorityRoundRobinIterations[i];
    }

    thread_local FibersManager* FibersManager::s_manager = nullptr;
}