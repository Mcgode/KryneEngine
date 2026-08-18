/**
 * @file
 * @author Max Godefroy
 * @date 02/07/2022.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Threads/SyncCounterPool.hpp"

namespace KryneEngine
{
    class FiberContext;

    class FiberJob
    {
    public:
        enum class Priority: u8
        {
            High,
            Medium,
            Low,
            Count
        };

        struct PriorityType
        {
            /// @details
            /// There are `Priority::Count` base type of priorities, and for each of them we distinguish between
            /// suspended jobs and unstarted ones. This allows to define the priority of one over the other.
            static constexpr u8 kJobPriorityTypes = 1 + static_cast<u8>(Priority::Count);

            bool m_pendingStart;
            Priority m_priority;

            PriorityType(const Priority _priority, const bool _pendingStart)
                : m_pendingStart(_pendingStart)
                , m_priority(_priority)
            {
                KE_ASSERT(_priority != Priority::Count);
            }

            /// @details
            /// The lowest, the higher priority.
            /// We want to finish the suspended jobs before starting new ones.
            ///
            /// Resulting table is:
            /// - 0: Suspended
            /// - 1: High, unstarted
            /// - 2: Medium, unstarted
            /// - 3: Low, unstarted
            explicit operator u8() const
            {
                return m_pendingStart ? static_cast<u8>(m_priority) + 1 : 0;
            }
        };

        enum class Status
        {
            PendingStart,
            Running,
            Paused,
            Finished
        };

        struct Desc
        {
            eastl::function<void(u16)> m_function;
            u16 m_jobCount = 1;
            Priority m_priority = Priority::Medium;
            bool m_useBigStack = false;
        };

        friend class FibersManager;
        friend class FiberThread;
        friend class FiberContext;
        friend class SyncCounterPool;

        FiberJob();

        [[nodiscard]] Status GetStatus() const { return m_status.load(std::memory_order_acquire); }

        [[nodiscard]] PriorityType GetPriorityType() const
        {
            return { m_priority, m_status.load(std::memory_order_acquire) == Status::PendingStart };
        }

        [[nodiscard]] bool CanRun() const
        {
            const Status status = m_status.load(std::memory_order_acquire);
            return status == Status::PendingStart || status == Status::Paused;
        }

    protected:
        [[nodiscard]] bool HasContextAssigned() const { return m_contextId != kInvalidContextId; }

        void SetContext(u16 _contextId, FiberContext *_context);

        void ResetContext();

    private:
        eastl::function<void(u16)> m_function = nullptr;
        u16 m_jobIndex = 0;
        Priority m_priority = Priority::Medium;
        bool m_bigStack = false;

        std::atomic<Status> m_status { Status::PendingStart };
        std::atomic<s32> m_dependencyJobsRunning { 0 };

        static constexpr s32 kInvalidContextId = -1;
        s32 m_contextId = kInvalidContextId;
        FiberContext *m_context = nullptr;

        SyncCounterId m_associatedCounterId = kInvalidSyncCounterId;
    };
} // KryneEngine