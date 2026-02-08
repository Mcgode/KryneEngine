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
    thread_local FibersManager* FibersManager::s_manager = nullptr;

    FibersManager::FibersManager(s32 _requestedThreadCount, AllocatorInstance _allocator)
        : m_fiberThreads(_allocator)
        , m_jobProducerTokens(_allocator)
        , m_jobConsumerTokens(_allocator)
        , m_currentJobs(_allocator)
        , m_nextJob(_allocator)
        , m_baseContexts(_allocator)
    {
        KE_ZoneScopedFunction("FibersManager::FibersManager()");

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
        };

        KE_ASSERT_MSG(fiberThreadCount > 0, "You need at least one fiber thread");

        // Resize array first!
        // This size is used to init the FiberTls objects,
        m_fiberThreads.Resize(fiberThreadCount);

        // Init FiberTls objects before initializing the threads, to avoid racing conditions.
        {
            m_jobProducerTokens.InitFunc(this, [this](JobProducerTokenArray &_array)
            {
                for (u64 i = 0; i < _array.size(); i++)
                {
                    // Do in-place memory init, else it will try to interpret uninitialized memory as a valid object.
                    ::new(&_array[i]) moodycamel::ProducerToken(m_jobQueues[i]);
                }
            });

            m_jobConsumerTokens.InitFunc(this, [this](JobConsumerTokenArray &_array)
            {
                for (u64 i = 0; i < _array.size(); i++)
                {
                    // Do in-place memory init, else it will try to interpret uninitialized memory as a valid object.
                    ::new(&_array[i]) moodycamel::ConsumerToken(m_jobQueues[i]);
                }
            });

            m_currentJobs.Init(this, nullptr);
            m_nextJob.Init(this, nullptr);
            m_baseContexts.InitFunc(
                this,
                [](FiberContext& _context) {
                    ::new(&_context) FiberContext();
                });

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

    void FibersManager::QueueJob(Job _job)
    {
        VERIFY_OR_RETURN_VOID(_job != nullptr);

        KE_ASSERT(_job->CanRun());

        const u8 priorityId = (u8)_job->GetPriorityType();
        if (FiberThread::IsFiberThread())
        {
            auto& producerToken = m_jobProducerTokens.Load()[priorityId];
            m_jobQueues[priorityId].enqueue(producerToken, _job);
        }
        else
        {
            m_jobQueues[priorityId].enqueue(_job);
        }
        m_waitVariable.notify_one();
    }

    bool FibersManager::_RetrieveNextJob(Job& job_, u16 _fiberIndex)
    {
        auto& consumerTokens = m_jobConsumerTokens.Load(_fiberIndex);
        for (s64 i = 0; i < static_cast<s64>(kJobQueuesCount); i++)
        {
            if (m_jobQueues[i].try_dequeue(consumerTokens[i], job_))
            {
                if (!job_->_HasContextAssigned())
                {
                    KE_ASSERT(job_->GetStatus() == FiberJob::Status::PendingStart);

                    u16 id;
                    if (m_contextAllocator->Allocate(job_->m_bigStack, id))
                    {
                        job_->_SetContext(id, m_contextAllocator->GetContext(id));
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
                return true;
            }
        }
        return false;
    }

    FibersManager *FibersManager::GetInstance()
    {
        return s_manager;
    }

    void FibersManager::SetInstance(FibersManager* _instance)
    {
        s_manager = _instance;
    }

    FibersManager::~FibersManager()
    {
        for (auto& fiberThread: m_fiberThreads)
        {
            fiberThread.Stop(m_waitVariable);
        }
        // Make sure to end and join all the fiber threads before anything else.
        m_fiberThreads.Clear();
        m_fiberThreads.GetAllocator().Delete(m_contextAllocator);
    }

    FiberJob *FibersManager::GetCurrentJob()
    {
        return m_currentJobs.Load();
    }

    void FibersManager::YieldJob(Job _nextJob)
    {
        const auto fiberIndex = FiberThread::GetCurrentFiberThreadIndex();
        auto* currentJob = m_currentJobs.Load(fiberIndex);

        if (currentJob != nullptr && currentJob->GetStatus() == FiberJob::Status::Running)
        {
            currentJob->m_status.store(FiberJob::Status::Paused, std::memory_order_release);
            QueueJob(currentJob);
        }

        IF_NOT_VERIFY(_nextJob == nullptr || _nextJob->CanRun())
        {
            _nextJob = nullptr;
        }

        m_fiberThreads[fiberIndex].SwitchToNextJob(this, currentJob, _nextJob);
    }

    void FibersManager::_OnContextSwitched()
    {
        const auto fiberIndex = FiberThread::GetCurrentFiberThreadIndex();

        FiberJob* oldJob = m_currentJobs.Load(fiberIndex);
        FiberJob* newJob = m_nextJob.Load(fiberIndex);

        if (oldJob != nullptr && oldJob->GetStatus() == FiberJob::Status::Finished)
        {
            if (oldJob->m_associatedCounterId != kInvalidSyncCounterId)
            {
                // Decrement counter
                m_syncCounterPool.DecrementCounterValue(oldJob->m_associatedCounterId);
            }

            m_contextAllocator->Free(oldJob->m_contextId);

            oldJob->_ResetContext();
            m_fiberThreads.GetAllocator().Delete(oldJob);
        }

        m_currentJobs.Load(fiberIndex) = newJob;
        m_nextJob.Load(fiberIndex) = nullptr;
    }

    SyncCounterId FibersManager::InitAndBatchJobs(
        u32 _jobCount,
        FiberJob::JobFunc* _jobFunc,
        void* _pUserData,
        size_t _userDataSize,
        FiberJob::Priority _priority,
        bool _useBigStack)
    {
        const auto syncCounter = m_syncCounterPool.AcquireCounter(_jobCount);

        VERIFY_OR_RETURN(syncCounter != kInvalidSyncCounterId, kInvalidSyncCounterId);

        auto pUserData = reinterpret_cast<uintptr_t>(_pUserData);

        AllocatorInstance allocator = m_fiberThreads.GetAllocator();

        for (u32 i = 0; i < _jobCount; i++)
        {
            auto* job = allocator.New<FiberJob>();
            job->m_functionPtr = _jobFunc;
            job->m_userData = reinterpret_cast<void*>(pUserData + _userDataSize * i);
            job->m_priority = _priority;
            job->m_bigStack = _useBigStack;
            job->m_associatedCounterId = syncCounter;
            QueueJob(job);
        }

        return syncCounter;
    }

    SyncCounterId FibersManager::InitAndBatchJobs(
        FiberJob::JobFunc *_jobFunc,
        void *_userData,
        u32 _jobCount,
        FiberJob::Priority _priority,
        bool _useBigStack)
    {
        // We reuse the InitAndBatchJobs, but we just make sure there is no per-job shift by setting the data size to 0
        return InitAndBatchJobs(
            _jobCount,
            _jobFunc,
            _userData,
            0,
            _priority,
            _useBigStack);
    }

    void FibersManager::InitAndBatchNoCounterJobs(
        const size_t _jobCount,
        FiberJob::JobFunc* _jobFunc,
        void* _userData,
        const size_t _userDataStride,
        const FiberJob::Priority _priority,
        const bool _useBigStack)
    {
        const auto userData = reinterpret_cast<uintptr_t>(_userData);

        const AllocatorInstance allocator = m_fiberThreads.GetAllocator();

        for (u32 i = 0; i < _jobCount; i++)
        {
            auto* job = allocator.New<FiberJob>();
            job->m_functionPtr = _jobFunc;
            job->m_userData = reinterpret_cast<void*>(userData + _userDataStride * i);
            job->m_priority = _priority;
            job->m_bigStack = _useBigStack;
            job->m_associatedCounterId = kInvalidSyncCounterId;
            QueueJob(job);
        }
    }

    SyncCounterPool::AutoSyncCounter FibersManager::AcquireAutoSyncCounter(u32 _count)
    {
        return eastl::move(m_syncCounterPool.AcquireAutoCounter(_count));
    }

    void FibersManager::WaitForCounter(SyncCounterId _syncCounter)
    {
        if (FiberThread::IsFiberThread())
        {
            auto* currentJob = GetCurrentJob();
            if (!m_syncCounterPool.AddWaitingJob(_syncCounter, currentJob))
            {
                YieldJob();
            }
        }
        else
        {
            KE_ZoneScopedFunction("FibersManager::WaitForCounter");

            TracyLockable(std::mutex, waitMutex);
            struct Data {
                std::condition_variable_any m_waitVariable {};
                SyncCounterId m_syncCounterId;
            } data;

            data.m_syncCounterId = _syncCounter;

            constexpr auto jobFunction = [](void* _dataPtr)
            {
                auto* data = static_cast<Data*>(_dataPtr);
                FibersManager::GetInstance()->WaitForCounter(data->m_syncCounterId);
                data->m_waitVariable.notify_one();
            };
            SyncCounterId id = InitAndBatchJobs(jobFunction, &data);

            std::unique_lock<LockableBase(std::mutex)> lock(waitMutex);
            data.m_waitVariable.wait(lock);

            ResetCounter(id);
        }
    }

    void FibersManager::ResetCounter(SyncCounterId _syncCounter)
    {
        m_syncCounterPool.FreeCounter(_syncCounter);
    }

    void FibersManager::_ThreadWaitForJob()
    {
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitVariable.wait(lock); // Allow spurious wakeup.
    }
}