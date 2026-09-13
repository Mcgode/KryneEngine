/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#pragma once

#include <atomic>
#include <thread>
#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine
{
    class FibersManager;
    class FiberJob;

    class FiberThread
    {
    public:
        explicit FiberThread(FibersManager *_fiberManager, u16 _threadIndex);

        virtual ~FiberThread();

        using ThreadIndex = u16;
        [[nodiscard]] static ThreadIndex GetCurrentFiberThreadIndex();
        [[nodiscard]] static bool IsFiberThread();

        void SwitchToNextJob(FibersManager *_manager, FiberJob *_currentJob, FiberJob *_nextJob = nullptr);

        void Stop(FibersManager& _manager);

    private:
        std::atomic<bool> m_shouldStop = false;
        std::thread m_thread;
        eastl::string m_name;

        static constexpr u32 kRetrieveSpinCountBeforeThreadWait = 50;

        static thread_local ThreadIndex sThreadIndex;
        static thread_local bool sIsThread;

        FiberJob *_TryRetrieveNextJob(FibersManager *_manager, u16 _threadIndex, bool _busyWait);
    };
} // KryneEngine