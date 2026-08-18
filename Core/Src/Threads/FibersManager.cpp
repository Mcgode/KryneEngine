/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#include "KryneEngine/Core/Threads/FibersManager.hpp"

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
            fiberThread.Stop(m_waitVariable);
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
        return eastl::move(m_syncCounterPool.AcquireAutoCounter(_count));
    }

    void FibersManager::QueueJob(FiberJob* _job)
    {
        VERIFY_OR_RETURN_VOID(_job != nullptr);

        KE_ASSERT(_job->CanRun());

        const u8 priorityId = static_cast<u8>(_job->GetPriorityType());
        if (FiberThread::IsFiberThread())
        {
            const moodycamel::ProducerToken& producerToken = m_jobProducerTokens.Load()[priorityId];
            m_jobQueues[priorityId].enqueue(producerToken, _job);
        }
        else
        {
            m_jobQueues[priorityId].enqueue(_job);
        }
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
            for (const auto& syncCounter : _syncCounters)
            {
                if (!m_syncCounterPool.AddWaitingJob(syncCounter, currentJob))
                {
                    YieldJob();
                }
                else
                {
                    currentJob->m_dependencyJobsRunning.fetch_sub(1, std::memory_order_acq_rel);
                }
            }
        }
        else
        {
            KE_ZoneScopedFunction("FibersManager::WaitForCounters");

            TracyLockable(std::mutex, waitMutex);
            std::condition_variable_any waitVariable;

            InitAndBatchJobsNoCounter(
                {.m_function =
                     [&waitVariable, &_syncCounters](u16)
                 {
                     GetInstance()->WaitForCounters(_syncCounters);
                     waitVariable.notify_one();
                 },
                 .m_priority = FiberJob::Priority::Medium});

            std::unique_lock lock(waitMutex);
            waitVariable.wait(lock);
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

            if (m_jobQueues[queueIndex].try_dequeue(consumerTokens[i], job_))
            {
                if (!job_->HasContextAssigned())
                {
                    KE_ASSERT(job_->GetStatus() == FiberJob::Status::PendingStart);

                    u16 id;
                    if (m_contextAllocator->Allocate(job_->m_bigStack, id))
                    {
                        job_->SetContext(id, m_contextAllocator->GetContext(id));
                    }
                }
                else if (!job_->CanRun())
                {
                    // If job is already finished or still running, ignore it and keep trying to retrieve the next job.
                    // This might happen because the job was run by skipping this step, which is legal.
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

    void FibersManager::OnContextSwitched()
    {
        const auto fiberIndex = FiberThread::GetCurrentFiberThreadIndex();

        Status& status = m_statuses.Load(fiberIndex);
        FiberJob* oldJob = status.m_currentJob;
        FiberJob* newJob = status.m_nextJob;

        if (oldJob != nullptr && oldJob->GetStatus() == FiberJob::Status::Finished)
        {
            if (oldJob->m_associatedCounterId != kInvalidSyncCounterId)
            {
                // Decrement counter
                m_syncCounterPool.DecrementCounterValue(oldJob->m_associatedCounterId);
            }

            m_contextAllocator->Free(oldJob->m_contextId);

            oldJob->ResetContext();
            m_fiberThreads.GetAllocator().Delete(oldJob);
        }

        status.m_currentJob = newJob;
        status.m_nextJob = nullptr;
    }

    void FibersManager::ThreadWaitForJob()
    {
        std::unique_lock lock(m_waitMutex);
        m_waitVariable.wait(lock); // Allow spurious wakeup.
    }

    void FibersManager::UpdateRoundRobinTotal()
    {
        m_priorityRoundRobinTotal = 0;
        for (u32 i = 0; i < m_priorityRoundRobinIterations.size(); ++i)
            m_priorityRoundRobinTotal += m_priorityRoundRobinIterations[i];
    }

    thread_local FibersManager* FibersManager::s_manager = nullptr;
}