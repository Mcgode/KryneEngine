/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#pragma once

#include <EASTL/array.h>
#include <EASTL/span.h>
#include <KryneEngine/Core/Threads/FiberJob.hpp>
#include <KryneEngine/Core/Threads/FiberThread.hpp>
#include <KryneEngine/Core/Threads/FiberTls.hpp>
#include <KryneEngine/Core/Threads/SyncCounterPool.hpp>

namespace KryneEngine
{
    struct FiberContextAllocator;
    class FiberThread;
    class IoQueryManager;

    /**
     * @brief A fibers manager with job scheduling capabilities.
     *
     * @details
     *
     * To avoid low-priority job starvation under high load, a per-thread round-robin system is in place. Each fiber
     * thread tracks a progress counter (`m_priorityRoundRobinProgress`) that determines which priority queue to try
     * first when dequeuing the next job:
     *  - A thread may dequeue up to X High-priority jobs before it must attempt a Normal-priority job.
     *  - After that, it may dequeue up to Y Normal-priority jobs before it must attempt a Low-priority job.
     *  - After Z Low-priority jobs, the cycle resets to High.
     *
     * Default values: High = 4, Normal = 2, Low = 1 (all configurable).
     *
     * If the preferred priority queue is empty, the system falls back through the remaining queues in round-robin order
     * (wrapping around) until a job is found. If no queue has any jobs, the progress resets back to high priority.
     *
     * Suspended jobs that are ready to resume are always dequeued first, bypassing the round-robin entirely, since they
     * already have a fiber context allocated and their dependency has just been resolved.
     */
    class FibersManager
    {
        friend FiberThread;
        friend IoQueryManager;
        friend FiberContext;

    public:
        explicit FibersManager(s32 _requestedThreadCount, AllocatorInstance _allocator);

        ~FibersManager();

        [[nodiscard]] static FibersManager* GetInstance();
        static void SetInstance(FibersManager* _instance);

        [[nodiscard]] static u16 GetFibersCount()
        {
            const auto* manager = GetInstance();
            if (KE_VERIFY(manager != nullptr))
            {
                return manager->GetFiberThreadCount();
            }
        	return 0;
        }

        [[nodiscard]] u16 GetFiberThreadCount() const { return m_fiberThreads.Size(); }

        [[nodiscard]] FiberJob* GetCurrentJob();

        [[nodiscard]] SyncCounterId InitAndBatchJobs(const FiberJob::Desc& _desc);
        void InitAndBatchJobsNoCounter(const FiberJob::Desc& _desc);

        [[nodiscard]] SyncCounterPool::AutoSyncCounter AcquireAutoSyncCounter(u32 _count = 1);

        void QueueJob(FiberJob* _job);

        void WaitForCounters(eastl::span<const SyncCounterId> _syncCounters);
        void WaitForCounter(SyncCounterId _syncCounter) { WaitForCounters({ &_syncCounter, 1 }); }

        void ResetCounter(SyncCounterId _syncCounter);

        void WaitForCounterAndReset(const SyncCounterId _syncCounter)
        {
            WaitForCounter(_syncCounter);
            ResetCounter(_syncCounter);
        }

        void YieldJob(FiberJob* _nextJob = nullptr);

        [[nodiscard]] IoQueryManager* GetIoQueryManager() const { return m_ioManager; }

        [[nodiscard]] u32 GetPriorityRoundRobinIterations(FiberJob::Priority _priority) const
        {
            return m_priorityRoundRobinIterations[static_cast<size_t>(_priority)];
        }

        void SetPriorityRoundRobinIterations(FiberJob::Priority _priority, const u32 _iterations)
        {
            KE_ASSERT(_iterations > 0);
            m_priorityRoundRobinIterations[static_cast<size_t>(_priority)] = _iterations;
            UpdateRoundRobinTotal();
        }

    protected:

        bool RetrieveNextJob(FiberJob*& job_, u16 _fiberIndex);

        void OnContextSwitched();

        void ThreadWaitForJob();

        void UpdateRoundRobinTotal();

    private:
        using JobQueue = moodycamel::ConcurrentQueue<FiberJob*>;
        static constexpr u8 kJobQueuesCount = FiberJob::PriorityType::kJobPriorityTypes;
        eastl::array<JobQueue, kJobQueuesCount> m_jobQueues;

        using JobProducerTokenArray = eastl::array<moodycamel::ProducerToken, kJobQueuesCount>;
        FiberTls<JobProducerTokenArray> m_jobProducerTokens;

        using JobConsumerTokenArray = eastl::array<moodycamel::ConsumerToken, kJobQueuesCount>;
        FiberTls<JobConsumerTokenArray> m_jobConsumerTokens;

        DynamicArray<FiberThread> m_fiberThreads;

        struct Status
        {
            FiberJob* m_currentJob = nullptr;
            FiberJob* m_nextJob = nullptr;
            u32 m_priorityRoundRobinProgress = 0;
        };

        FiberTls<Status> m_statuses;
        FiberTls<FiberContext> m_baseContexts;

        FiberContextAllocator* m_contextAllocator;

        SyncCounterPool m_syncCounterPool {};

        std::mutex m_waitMutex;
        std::condition_variable m_waitVariable;

        static thread_local FibersManager* s_manager;
        IoQueryManager* m_ioManager = nullptr;

        static constexpr size_t kPrioritiesCount = static_cast<size_t>(FiberJob::Priority::Count);
        eastl::array<u32, kPrioritiesCount> m_priorityRoundRobinIterations = {
            4,
            2,
            1
        };
        u32 m_priorityRoundRobinTotal = 0;
    };
} // KryneEngine