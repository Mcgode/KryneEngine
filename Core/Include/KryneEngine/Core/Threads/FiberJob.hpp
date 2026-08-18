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
            /// kicked jobs and unkicked ones. This allows to define the priority of one over the other.
            static constexpr u8 kJobPriorityTypes = 2 * static_cast<u8>(Priority::Count);

            bool m_pendingStart;
            Priority m_priority;

            PriorityType(Priority _priority, bool _pendingStart)
                : m_priority(_priority)
                , m_pendingStart(_pendingStart)
            {
                KE_ASSERT(_priority != Priority::Count);
            }

            /// @details
            /// The lowest, the higher priority.
            /// Since we want to finish the kicked jobs before starting new ones, but still want to have the higher
            /// priority jobs before the lower ones, we also use priority to apply order.
            ///
            /// Resulting table is:
            /// - 0: High, kicked
            /// - 1: High, not kicked
            /// - 2: Medium, kicked
            /// - 3: Medium, not kicked
            /// - 4: Low, kicked
            /// - 5: Low, not kicked
            explicit operator u8() const
            {
                return
                        static_cast<u8>(m_pendingStart) |
                        (static_cast<u8>(m_priority) << 1);
            }
        };

        enum class Status
        {
            PendingStart,
            Running,
            Paused,
            Finished
        };

        friend class FibersManager;
        friend class FiberThread;
        friend class FiberContext;
        friend class SyncCounterPool;

    public:
        typedef void (JobFunc)(void*);

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
        [[nodiscard]] bool _HasContextAssigned() const { return m_contextId != kInvalidContextId; }

        void _SetContext(u16 _contextId, FiberContext *_context);

        void _ResetContext();

    private:
        JobFunc* m_functionPtr = nullptr;
        void* m_userData = nullptr;
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