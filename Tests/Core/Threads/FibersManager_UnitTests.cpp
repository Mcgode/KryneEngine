/**
 * @file
 * @author Max Godefroy
 * @date 13/09/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <atomic>

namespace KryneEngine::Tests
{
    // Regression test for a bug in FibersManager::WaitForCounters()'s non-fiber branch (the one
    // taken when it's called from a thread that isn't one of the fiber worker threads, e.g. the
    // main thread driving the engine's frame loop).
    //
    // The spawned helper job used to notify a condition variable without holding its mutex, and
    // the waiting thread used a bare (predicate-less) wait(). This combination could:
    //  - lose the wakeup entirely and hang forever, if notify_one() ran before the wait() call;
    //  - return early on a spurious wakeup, after which the waiting thread would destroy the
    //    mutex/condition variable/span that the still-running helper job was about to touch
    //    (use-after-free), or unwind while a notify_one() call on the condition variable it owns
    //    was still in flight on another thread (undefined behaviour).
    //
    // This exercises exactly that call path (fibersManager.WaitForCounterAndReset() from the main,
    // non-fiber thread) many times in a row. Before the fix, this would reliably hang or crash
    // (under ASAN/TSAN) within a small number of iterations; after the fix it should always
    // complete quickly.
    TEST(FibersManager, WaitForCountersFromNonFiberThread)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(4, allocator);

        constexpr u32 kIterations = 200;
        constexpr u32 kJobsPerIteration = 4;

        for (u32 i = 0; i < kIterations; i++)
        {
            std::atomic<u32> ranCount = 0;

            const auto counter = fibersManager.InitAndBatchJobs({
                .m_function = [&](u16) { ranCount.fetch_add(1, std::memory_order_relaxed); },
                .m_jobCount = kJobsPerIteration,
            });

            // This call runs on this test's thread, which is not a fiber thread: it takes the
            // branch of WaitForCounters() that this test is meant to exercise.
            fibersManager.WaitForCounterAndReset(counter);

            EXPECT_EQ(ranCount.load(), kJobsPerIteration);
        }
    }

    // Regression test for a self-deadlock/use-after-free race in the scheduler.
    //
    // A fiber that calls WaitForCounters() is marked Paused and queued as a dependency of the
    // counter(s) it's waiting on; it only actually yields (context-switches away) afterwards.
    // If every one of those dependencies resolves fast enough, the last one to resolve calls
    // QueueJob() on a *different* thread, putting the (still technically "current", not-yet-
    // switched-away-from) job back into the resuming-jobs queue. If that same fiber thread then
    // dequeues its own job as the "next" job to switch to, FiberContext::SwapContext() tries to
    // lock that job's context mutex, which this very call stack already holds, and deadlocks
    // permanently; depending on timing it can also manifest as a double-free/use-after-free in
    // FibersManager::OnContextSwitched(), since the same job then gets torn down twice.
    //
    // This creates many short-lived dependency chains across more worker threads than there are
    // waiting jobs, specifically to maximize the odds that a dependency resolves on a different
    // thread than the one currently running the job waiting on it, which is what is needed to hit
    // the race. Before the fix, this reliably hangs or crashes (ASAN heap-use-after-free) within a
    // handful of iterations; after the fix it should always complete quickly.
    TEST(FibersManager, WaitingJobIsNeverHandedBackToItselfAsNextJob)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(8, allocator);

        constexpr u32 kWaitingJobs = 2;
        constexpr u32 kRoundsPerJob = 200;
        constexpr u32 kDependenciesPerRound = 4;

        std::atomic<u32> completedJobs = 0;

        const auto outerCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                for (u32 r = 0; r < kRoundsPerJob; r++)
                {
                    const auto counter = FibersManager::GetInstance()->InitAndBatchJobs({
                        .m_function = [](u16) {},
                        .m_jobCount = kDependenciesPerRound,
                    });
                    FibersManager::GetInstance()->WaitForCounterAndReset(counter);
                }
                completedJobs.fetch_add(1, std::memory_order_relaxed);
            },
            .m_jobCount = kWaitingJobs,
        });

        fibersManager.WaitForCounterAndReset(outerCounter);

        EXPECT_EQ(completedJobs.load(), kWaitingJobs);
    }
} // KryneEngine::Tests
