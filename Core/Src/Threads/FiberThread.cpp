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

            FiberContext& context = _fiberManager->m_baseContexts.Load(_threadIndex);
            {
                TracyFiberEnter(context.m_name.c_str());

                // Mark current fiber context as running.
                context.m_mutex.ManualLock();

                KE_ASSERT(Threads::DisableThreadSignals());

                FibersManager::s_manager = _fiberManager;
                sThreadIndex = _threadIndex;
                sIsThread = true;
            }

            while (!m_shouldStop.load(std::memory_order::relaxed))
            {
                SwitchToNextJob(_fiberManager, nullptr);
            }

            // SwitchToNextJob() only unlocks a context's mutex on behalf of whichever context it is
            // switching *away from* -- see FiberContext::SwapContext()/RunFiber(). The loop above always
            // leaves this thread sitting on its own base context (SwitchToNextJob() early-outs without
            // swapping once both _currentJob and _nextJob are null, which is exactly what happens once
            // _TryRetrieveNextJob() starts returning null because m_shouldStop flipped), so nothing ever
            // performs that final swap-away to unlock it. Without this, the base context's mutex stays
            // locked for good once this OS thread exits, and ~FibersManager() destroying it while still
            // locked -- from a different thread -- is undefined behaviour.
            context.m_mutex.ManualUnlock();

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

        // A fiber must never be switched into the context it is already running on: that context's
        // mutex is already held by this very call stack, so locking it again in SwapContext() would
        // deadlock permanently. FibersManager::RetrieveNextJob() guards against the scheduler ever
        // producing this on its own; this assert only guards against a caller explicitly (and
        // incorrectly) passing the current job back in as `_nextJob`.
        KE_ASSERT_MSG(_nextJob != _currentJob, "A fiber cannot be switched into its own currently running context");

        _manager->m_statuses.Load(fiberIndex).m_nextJob = _nextJob;

        auto* currentContext = _currentJob == nullptr
                ? &_manager->m_baseContexts.Load(fiberIndex)
                : _currentJob->m_context;
        auto* nextContext = _nextJob == nullptr
                ? &_manager->m_baseContexts.Load(fiberIndex)
                : _nextJob->m_context;
        KE_ASSERT(nextContext != nullptr);

        // _currentJob is only still valid memory after FinalizeLeavingJob() below if it wasn't
        // Finished -- that's exactly when FinalizeLeavingJob() deletes it. Null out this thread's
        // own Status::m_currentJob *before* that happens (while _currentJob is still guaranteed
        // alive) whenever that's the case, so that whichever code eventually calls
        // OnContextSwitched() for this transition -- this same SwitchToNextJob() call resuming
        // later, or FiberContext::RunFiber()'s entry-point registration if _nextJob's context has
        // never been entered before -- never dereferences a dangling pointer. Status is the right
        // channel for this (rather than a parameter to OnContextSwitched()): it's already how
        // m_nextJob crosses this same jump_fcontext boundary, and unlike a parameter, it's reachable
        // from both of that call's possible landing points.
        if (_currentJob != nullptr && _currentJob->GetStatus() == FiberJob::Status::Finished)
        {
            _manager->m_statuses.Load(fiberIndex).m_currentJob = nullptr;
        }

        // Finalize (and, if it finished, free/delete) the job we are leaving now, while this
        // thread still exclusively owns currentContext (its mutex isn't released until the
        // SwapContext() call below). This must happen before that release: once released, another
        // thread may immediately resume and finish this same job via a later, legitimate
        // transition, finalizing it concurrently with us.
        _manager->FinalizeLeavingJob(_currentJob);

        currentContext->SwapContext(nextContext);

        _manager->OnContextSwitched();
    }

    void FiberThread::Stop(FibersManager& _manager)
    {
        // Set before notifying so that a thread woken by this (or already past ThreadWaitForJob(),
        // re-checking _TryRetrieveNextJob's own loop condition) sees it immediately.
        m_shouldStop.store(true, std::memory_order_release);
        _manager.m_waitVariable.notify_all();
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
        while(!m_shouldStop.load(std::memory_order::relaxed) && _busyWait);

        if (m_shouldStop.load(std::memory_order::relaxed) && job != nullptr)
        {
            // RetrieveNextJob() above can succeed (genuinely dequeuing job, removing it from the
            // shared queue) in the same instant m_shouldStop flips on another thread -- the loop's
            // own condition only stops *future* iterations, it can't un-dequeue this one. Running it
            // here would risk hanging shutdown indefinitely (it might depend on another job that
            // will now never get a chance to run, since every worker is stopping), but the old
            // behaviour of just dropping `job` on the floor leaked it, its context id (if it already
            // had one, e.g. a Paused job resuming), and its associated sync counter slot for good.
            // Put it back in the queue instead: FibersManager::DrainQueuedJobs() cleans up whatever
            // is still queued once every worker thread has actually stopped.
            _manager->QueueJob(job);
            job = nullptr;
        }

        return m_shouldStop.load(std::memory_order::relaxed) ? nullptr : job;
    }
} // KryneEngine