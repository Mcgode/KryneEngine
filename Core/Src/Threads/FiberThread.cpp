/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#include "KryneEngine/Core/Threads/FiberThread.hpp"

#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Core/Threads/FibersManager.hpp"
#include "KryneEngine/Core/Threads/HelperFunctions.hpp"
#include "Threads/Internal/FiberContext.hpp"

namespace KryneEngine
{
    thread_local FiberThread::ThreadIndex FiberThread::sThreadIndex = 0;
    thread_local bool FiberThread::sIsThread = false;

    FiberThread::FiberThread(FibersManager *_fiberManager, u16 _threadIndex)
    {
        m_name.sprintf("Fiber thread %d", _threadIndex);

        m_thread = std::thread([this, _fiberManager, _threadIndex]()
        {
            tracy::SetThreadName(m_name.c_str());

            {
                FiberContext& context = _fiberManager->m_baseContexts.Load(_threadIndex);
                TracyFiberEnter(context.m_name.c_str());

                // Mark current fiber context as running.
                context.m_mutex.ManualLock();

                KE_ASSERT(Threads::DisableThreadSignals());

                FibersManager::s_manager = _fiberManager;
                sThreadIndex = _threadIndex;
                sIsThread = true;
            }

            while (!m_shouldStop)
            {
                SwitchToNextJob(_fiberManager, nullptr);
            }

            TracyFiberLeave;
        });

        KE_ASSERT(Threads::SetThreadHardwareAffinity(m_thread, _threadIndex));
    }

    FiberThread::~FiberThread()
    {
        KE_ASSERT_MSG(!m_thread.joinable(), "Should have been stopped beforehand");
    }

    FiberThread::ThreadIndex FiberThread::GetCurrentFiberThreadIndex()
    {
        return sThreadIndex;
    }

    bool FiberThread::IsFiberThread()
    {
        return sIsThread;
    }

    void FiberThread::SwitchToNextJob(FibersManager *_manager, FiberJob* _currentJob, FiberJob *_nextJob)
    {
        const auto fiberIndex = GetCurrentFiberThreadIndex();

        if (_nextJob == nullptr)
        {
            _nextJob = _TryRetrieveNextJob(_manager, fiberIndex, _currentJob == nullptr);
        }

        // Happens when shutting down.
        if (_nextJob == nullptr && _currentJob == nullptr)
        {
            return;
        }

        _manager->m_statuses.Load(fiberIndex).m_nextJob = _nextJob;

        auto* currentContext = _currentJob == nullptr
                ? &_manager->m_baseContexts.Load(fiberIndex)
                : _currentJob->m_context;
        auto* nextContext = _nextJob == nullptr
                ? &_manager->m_baseContexts.Load(fiberIndex)
                : _nextJob->m_context;
        KE_ASSERT(nextContext != nullptr);

        currentContext->SwapContext(nextContext);

        _manager->OnContextSwitched();
    }

    void FiberThread::Stop(std::condition_variable &_waitVariable)
    {
        m_shouldStop = true;
        _waitVariable.notify_all();
        m_thread.join();
    }

    FiberJob *FiberThread::_TryRetrieveNextJob(FibersManager *_manager, u16 _threadIndex, bool _busyWait)
    {
        FiberJob* job = nullptr;

        u32 i = 0;
        do
        {
            if (FibersManager::GetInstance()->RetrieveNextJob(job, _threadIndex))
            {
                break;
            }
            else if (i >= kRetrieveSpinCountBeforeThreadWait)
            {
                FibersManager::GetInstance()->ThreadWaitForJob();
                i = 0;
            }
            else if (_busyWait)
            {
                Threads::CpuYield();
                i++;
            }
        }
        while(!m_shouldStop && _busyWait);

        return m_shouldStop ? nullptr : job;
    }
} // KryneEngine