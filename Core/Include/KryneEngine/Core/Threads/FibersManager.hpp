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

    protected:

        bool RetrieveNextJob(FiberJob*& job_, u16 _fiberIndex);

        void OnContextSwitched();

        void ThreadWaitForJob();

    private:
        using JobQueue = moodycamel::ConcurrentQueue<FiberJob*>;
        static constexpr u8 kJobQueuesCount = FiberJob::PriorityType::kJobPriorityTypes;
        eastl::array<JobQueue, kJobQueuesCount> m_jobQueues;

        using JobProducerTokenArray = eastl::array<moodycamel::ProducerToken, kJobQueuesCount>;
        FiberTls<JobProducerTokenArray> m_jobProducerTokens;

        using JobConsumerTokenArray = eastl::array<moodycamel::ConsumerToken, kJobQueuesCount>;
        FiberTls<JobConsumerTokenArray> m_jobConsumerTokens;

        DynamicArray<FiberThread> m_fiberThreads;

        FiberTls<FiberJob*> m_currentJobs;
        FiberTls<FiberJob*> m_nextJob;
        FiberTls<FiberContext> m_baseContexts;

        FiberContextAllocator* m_contextAllocator;

        SyncCounterPool m_syncCounterPool {};

        std::mutex m_waitMutex;
        std::condition_variable m_waitVariable;

        static thread_local FibersManager* s_manager;
        IoQueryManager* m_ioManager = nullptr;
    };
} // KryneEngine