/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#pragma once

#include <EASTL/array.h>
#include <EASTL/unique_ptr.h>
#include <KryneEngine/Core/Threads/FiberJob.hpp>
#include <KryneEngine/Core/Threads/FiberThread.hpp>
#include <KryneEngine/Core/Threads/FiberTls.hpp>
#include <KryneEngine/Core/Threads/SyncCounterPool.hpp>

#include "EASTL/span.h"

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
        using Job = FiberJob*;

        explicit FibersManager(s32 _requestedThreadCount, AllocatorInstance _allocatorInstance);

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

        [[nodiscard]] SyncCounterId InitAndBatchJobs(
            u32 _jobCount,
            FiberJob::JobFunc* _jobFunc,
            void* _pUserData,
            size_t _userDataSize,
            FiberJob::Priority _priority = FiberJob::Priority::Medium,
            bool _useBigStack = false);

        [[nodiscard]] SyncCounterId InitAndBatchJobs(
            FiberJob::JobFunc* _jobFunc,
            void* _userData,
            u32 _jobCount = 1,
            FiberJob::Priority _priority = FiberJob::Priority::Medium,
            bool _useBigStack = false);

        template <class T>
        [[nodiscard]] SyncCounterId InitAndBatchJobs(
            u32 _jobCount,
            FiberJob::JobFunc* _jobFunc,
            T* _userData,
            FiberJob::Priority _priority = FiberJob::Priority::Medium,
            bool _useBigStack = false)
        {
            return InitAndBatchJobs(
                _jobCount,
                _jobFunc,
                reinterpret_cast<void*>(_userData),
                sizeof(T),
                _priority,
                _useBigStack);
        }

        void InitAndBatchNoCounterJobs(
            size_t _jobCount,
            FiberJob::JobFunc* _jobFunc,
            void *_userData,
            size_t _userDataStride,
            FiberJob::Priority _priority = FiberJob::Priority::Medium,
            bool _useBigStack = false);

        void InitAndBatchNoCounterJobs(
            FiberJob::JobFunc* _jobFunc,
            void *_userData,
            size_t _jobCount = 1,
            const FiberJob::Priority _priority = FiberJob::Priority::Medium,
            const bool _useBigStack = false)
        {
            InitAndBatchNoCounterJobs(_jobCount, _jobFunc, _userData, 0, _priority, _useBigStack);
        }

        template <class T>
        void InitAndBatchNoCounterJobs(
            size_t _jobCount,
            FiberJob::JobFunc* _jobFunc,
            eastl::span<T> _userData,
            const FiberJob::Priority _priority = FiberJob::Priority::Medium,
            const bool _useBigStack = false)
        {
            InitAndBatchNoWaitJobs(_jobCount, _jobFunc, _userData.data(), sizeof(T), _priority, _useBigStack);
        }

        [[nodiscard]] SyncCounterPool::AutoSyncCounter AcquireAutoSyncCounter(u32 _count = 1);

        void QueueJob(Job _job);

        void WaitForCounter(SyncCounterId _syncCounter);
        void ResetCounter(SyncCounterId _syncCounter);

        inline void WaitForCounterAndReset(SyncCounterId _syncCounter)
        {
            WaitForCounter(_syncCounter);
            ResetCounter(_syncCounter);
        }

        void YieldJob(Job _nextJob = nullptr);

        [[nodiscard]] IoQueryManager* GetIoQueryManager() const { return m_ioManager; }

    protected:

        bool _RetrieveNextJob(Job&job_, u16 _fiberIndex);

        void _OnContextSwitched();

        void _ThreadWaitForJob();

    private:
        using JobQueue = moodycamel::ConcurrentQueue<Job>;
        static constexpr u8 kJobQueuesCount = FiberJob::PriorityType::kJobPriorityTypes;
        eastl::array<JobQueue, kJobQueuesCount> m_jobQueues;

        using JobProducerTokenArray = eastl::array<moodycamel::ProducerToken, kJobQueuesCount>;
        FiberTls<JobProducerTokenArray> m_jobProducerTokens;

        using JobConsumerTokenArray = eastl::array<moodycamel::ConsumerToken, kJobQueuesCount>;
        FiberTls<JobConsumerTokenArray> m_jobConsumerTokens;

        DynamicArray<FiberThread> m_fiberThreads;

        FiberTls<Job> m_currentJobs;
        FiberTls<Job> m_nextJob;
        FiberTls<FiberContext> m_baseContexts;

        FiberContextAllocator* m_contextAllocator;

        SyncCounterPool m_syncCounterPool {};

        std::mutex m_waitMutex;
        std::condition_variable m_waitVariable;

        static thread_local FibersManager* s_manager;
        IoQueryManager* m_ioManager = nullptr;
    };
} // KryneEngine