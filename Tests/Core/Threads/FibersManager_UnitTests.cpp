/**
 * @file
 * @author Max Godefroy
 * @date 13/09/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "Utils/AssertUtils.hpp"

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

    // Regression test for stale SyncCounterId reuse in SyncCounterPool.
    //
    // A SyncCounterId's pool slot is freed (FreeCounter(), returning its index to the pool) and can
    // then be immediately reacquired (AcquireCounter()) for a completely unrelated, newer batch.
    // AcquireCounter()/FreeCounter() used to only be serialized against DecrementCounterValue()'s
    // own cleanup critical section, which isn't enough: under heavy concurrent churn, a decrement
    // meant for one batch could still land on the entry after it had already been recycled for a
    // newer one, silently corrupting that newer batch's counter (observable as
    // SyncCounterPool.cpp's `KE_VERIFY(value >= 0)` going negative).
    //
    // This creates many more concurrent "churners" than there are worker threads, each repeatedly
    // acquiring, waiting on, and freeing a small counter as fast as possible, to maximize how often
    // a freed slot gets reacquired for a new batch while some other thread might still be mid-way
    // through finishing up the old one. Before the fix (SyncCounterId carrying a generation tag,
    // validated by AddWaitingJob()/DecrementCounterValue()/FreeCounter()), this reliably corrupts a
    // counter within a handful of rounds; after the fix, a stale decrement is safely rejected
    // instead, and this always completes cleanly.
    TEST(FibersManager, StaleSyncCounterIdIsNeverReused)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(8, allocator);

        constexpr u32 kChurners = 16;
        constexpr u32 kRoundsPerChurner = 300;
        constexpr u32 kJobsPerRound = 2;

        std::atomic<u32> completedChurners = 0;

        const auto outerCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                for (u32 r = 0; r < kRoundsPerChurner; r++)
                {
                    const auto counter = FibersManager::GetInstance()->InitAndBatchJobs({
                        .m_function = [](u16) {},
                        .m_jobCount = kJobsPerRound,
                    });
                    FibersManager::GetInstance()->WaitForCounterAndReset(counter);
                }
                completedChurners.fetch_add(1, std::memory_order_relaxed);
            },
            .m_jobCount = kChurners,
        });

        fibersManager.WaitForCounterAndReset(outerCounter);

        EXPECT_EQ(completedChurners.load(), kChurners);
    }

    // Regression test for a race in SyncCounterPool::DecrementCounterValue()'s "wake waiters" path.
    //
    // That function's generation check and its atomic decrement of the counter are both lock-free
    // (deliberately, to keep this hot path cheap); only the cleanup that runs once the decrement
    // observes the counter hitting 0 takes entry.m_mutex, to walk and clear entry.m_waitingJobs.
    // An unbounded amount of time can pass between observing value == 0 and actually acquiring that
    // lock. If, in that window, this decrement's own waiter (if it had one) already left via
    // AddWaitingJob()'s lock-free fast path (it can see entry.m_counter == 0 without needing the
    // lock at all) -- AcquireCounter() may, by the time this decrement finally gets the lock, have
    // already reused the now-empty slot for a completely unrelated, newer generation, whose own
    // waiter is now the one sitting in entry.m_waitingJobs. Without re-checking the generation right
    // after acquiring the lock, this stale decrement would wake that newer generation's waiter
    // *before its own counter had actually reached 0*, letting it call FreeCounter() on a batch that
    // still has real, in-flight jobs -- which then silently recycles the slot out from under them,
    // so their own eventual decrements land on a generation that's already moved on several times.
    //
    // Empty-bodied jobs make each round's two jobs finish about as fast as possible, relative to the
    // waiting churner registering itself, which is exactly what it takes for the fast path above to
    // be taken; many more churners and rounds than FibersManager_UnitTests' other pool-churn test
    // maximizes how often that races against a new generation's AddWaitingJob() on the same slot.
    // Before the fix, this corrupts a counter (observable as SyncCounterPool.cpp's generation-
    // mismatch check rejecting a decrement) in roughly 2 out of every 5 runs, since it depends on
    // the OS scheduler actually delaying a decrement's lock acquisition past a slot's reuse; after
    // the fix, a decrement that goes stale while waiting for the lock is safely dropped instead, and
    // every churner always completes all of its rounds.
    TEST(FibersManager, DecrementCounterValueNeverWakesAStaleWaiter)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(8, allocator);

        constexpr u32 kChurners = 48;
        constexpr u32 kRoundsPerChurner = 800;
        constexpr u32 kJobsPerRound = 2;

        std::atomic<u32> completedChurners = 0;

        const auto outerCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                for (u32 r = 0; r < kRoundsPerChurner; r++)
                {
                    const auto counter = FibersManager::GetInstance()->InitAndBatchJobs({
                        .m_function = [](u16) {},
                        .m_jobCount = kJobsPerRound,
                    });
                    FibersManager::GetInstance()->WaitForCounterAndReset(counter);
                }
                completedChurners.fetch_add(1, std::memory_order_relaxed);
            },
            .m_jobCount = kChurners,
        });

        fibersManager.WaitForCounterAndReset(outerCounter);

        EXPECT_EQ(completedChurners.load(), kChurners);
    }

    // Regression test for a use-after-free in FibersManager::OnContextSwitched().
    //
    // FiberThread::SwitchToNextJob() calls FinalizeLeavingJob() on the job being left *before* the
    // actual context switch -- deliberately, since that's what lets it delete a Finished job's
    // memory while this thread still exclusively owns its context (see the #NEW/double-free fix
    // this session). OnContextSwitched() then runs, immediately after the switch, to update this
    // thread's own bookkeeping of which job it's now running -- including clearing the *previous*
    // job's ownership marker. If that previous job just finished, FinalizeLeavingJob() already freed
    // it moments earlier, so touching it here is a heap-use-after-free, not a race that needs
    // unlucky timing to hit: it reproduces on the very first job *any* test runs to completion.
    //
    // This is about as minimal a case as it gets -- one small batch of trivial jobs, run to
    // completion once. Before the fix, this (and in fact every other test in this file) reliably
    // crashes under ASAN with a heap-use-after-free in OnContextSwitched()'s write to
    // FiberJob::m_ownerThread; after the fix, it always completes cleanly.
    TEST(FibersManager, OnContextSwitchedNeverTouchesAFinishedJob)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(4, allocator);

        constexpr u32 kIterations = 50;
        constexpr u32 kJobsPerIteration = 4;

        for (u32 i = 0; i < kIterations; i++)
        {
            std::atomic<u32> ranCount = 0;

            const auto counter = fibersManager.InitAndBatchJobs({
                .m_function = [&](u16) { ranCount.fetch_add(1, std::memory_order_relaxed); },
                .m_jobCount = kJobsPerIteration,
            });

            fibersManager.WaitForCounterAndReset(counter);

            EXPECT_EQ(ranCount.load(), kJobsPerIteration);
        }
    }

    // Regression test for waiting on more than one counter at once permanently stranding the job.
    //
    // WaitForCounters()'s fiber branch used to call YieldJob() as soon as the *first* AddWaitingJob()
    // call in its loop returned false (meaning: not yet resolved) -- but YieldJob() context-switches
    // away, and control only returns to this function once something calls QueueJob() on this job.
    // That meant any counters after the first unresolved one were never registered as waiting on at
    // all. m_dependencyJobsRunning (pre-loaded with the full count before the loop) could then only
    // ever be decremented by whichever single counter the job happened to register for, and
    // DecrementCounterValue() only requeues a job once ITS decrement brings that count from 1 to 0 --
    // which, starting from N > 1 and with only one counter ever able to decrement it, it never does.
    // Permanent hang.
    //
    // This puts a deliberately slow counter first in the span passed to WaitForCounters(), so it's
    // certain not to have resolved yet when AddWaitingJob() is called on it -- exactly what it takes
    // to hit the bug -- paired with a second, trivial counter that must also be registered and waited
    // on for the waiter to ever complete. Before the fix, this hangs forever; after the fix, the
    // waiter always completes once both counters resolve.
    TEST(FibersManager, WaitForCountersRegistersAllCountersBeforeYielding)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(4, allocator);

        std::atomic<bool> waiterCompleted = false;

        const auto outerCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                const auto slowCounter = FibersManager::GetInstance()->InitAndBatchJobs({
                    .m_function = [](u16) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); },
                    .m_jobCount = 1,
                });
                const auto trivialCounter = FibersManager::GetInstance()->InitAndBatchJobs({
                    .m_function = [](u16) {},
                    .m_jobCount = 1,
                });

                // slowCounter must be first: the bug only triggers when the *first* counter in the
                // span is the one that isn't resolved yet at registration time.
                const SyncCounterId counters[] = { slowCounter, trivialCounter };
                FibersManager::GetInstance()->WaitForCounters(counters);

                FibersManager::GetInstance()->ResetCounter(slowCounter);
                FibersManager::GetInstance()->ResetCounter(trivialCounter);

                waiterCompleted.store(true, std::memory_order_release);
            },
            .m_jobCount = 1,
        });

        fibersManager.WaitForCounterAndReset(outerCounter);

        EXPECT_TRUE(waiterCompleted.load());
    }

    // Regression test for RetrieveNextJob() handing back a job with no context assigned when the
    // fiber stack pool is exhausted.
    //
    // RetrieveNextJob() allocates a job's FiberContext lazily, the first time it's about to actually
    // run it. If FiberContextAllocator::Allocate() fails (the small-stack pool -- 128 contexts, see
    // Core/Src/Threads/Internal/FiberContext.hpp -- is exhausted), there used to be no else branch:
    // the job was still handed back with job_->m_context == nullptr, and SwitchToNextJob() would
    // then try to SwapContext() into that null pointer.
    //
    // Allocate() itself still asserts on the same exhaustion (deliberately -- it's recoverable, but
    // still worth surfacing loudly in debug builds), so this wraps the exhausting section in a
    // ScopedAssertCatcher the same way FiberContextAllocator.Allocate does, to verify the *caller's*
    // handling without that expected, intentional assert aborting the test run.
    //
    // Each of the jobs below waits on one shared, not-yet-resolved counter rather than running to
    // completion (which would free its context right away and never exhaust the pool) or busy-
    // yielding in a loop (which would instead starve new jobs out, since the scheduler's "resuming"
    // queue is unconditionally checked before any other -- a separate, real scheduling concern, but
    // not this one). Waiting on a counter parks a job off every scheduler queue until it resolves,
    // which is exactly what it takes to hold 128+ contexts at once using only a handful of worker
    // threads. Before the fix, this reliably crashes (a null-pointer SwapContext()); after the fix,
    // the excess jobs are simply requeued and retried once a context frees up (tripping Allocate()'s
    // caught assert at least once along the way, confirming the pool really was exhausted), and
    // every single one of them eventually completes.
    TEST(FibersManager, RetrieveNextJobRequeuesOnAllocationFailure)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(8, allocator);

        // kSmallStackCount (FiberContext.hpp) is 128; comfortably exceed it.
        constexpr u32 kJobsToStart = 128 + 16;

        std::atomic<bool> gateReleased = false;
        std::atomic<u32> completedCount = 0;

        ScopedAssertCatcher catcher;

        // One job that the kJobsToStart jobs below all wait on -- sleeping (not spinning) so it
        // doesn't itself compete for CPU with the worker threads trying to start the others.
        const auto gateCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                while (!gateReleased.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            },
            .m_jobCount = 1,
        });

        const auto outerCounter = fibersManager.InitAndBatchJobs({
            .m_function = [&](u16)
            {
                FibersManager::GetInstance()->WaitForCounter(gateCounter);
                completedCount.fetch_add(1, std::memory_order_relaxed);
            },
            .m_jobCount = kJobsToStart,
        });

        // Give the scheduler time to start as many of these jobs as the context pool allows --
        // without this, the rest might not even be attempted before the gate opens.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        gateReleased.store(true, std::memory_order_release);

        fibersManager.WaitForCounterAndReset(outerCounter);
        fibersManager.ResetCounter(gateCounter);

        EXPECT_EQ(completedCount.load(), kJobsToStart);
        // Confirms the pool was actually exhausted (not just that everything happened to complete
        // for some unrelated reason) -- at least one "Out of Fiber stacks!" should have been caught.
        EXPECT_FALSE(catcher.GetCaughtMessages().empty());
    }

    // Regression test for #13: a sufficiently negative requested thread count used to underflow
    // FibersManager's internal u16 thread count instead of clamping it to 1.
    //
    // The constructor computes `fiberThreadCount -= eastl::min<u16>(abs(_requestedThreadCount),
    // _requestedThreadCount - 1)`, intending the second argument to bound the subtraction so the
    // result can't go below 1. But `_requestedThreadCount - 1` is computed on the (negative)
    // request itself, not on `fiberThreadCount`: converted to u16, a negative value wraps around to
    // something like 65,523, which the min() against `abs(_requestedThreadCount)` (always far
    // smaller in practice) never actually binds. So the subtraction runs unclamped, and once
    // abs(_requestedThreadCount) reaches or exceeds the machine's hardware_concurrency(),
    // `fiberThreadCount` (itself a u16) underflows to a huge value instead of bottoming out at 1 --
    // which would then have this constructor try to spin up tens of thousands of real OS threads.
    //
    // This deliberately does NOT also run the old, reverted code here to confirm the crash: doing
    // so would actually attempt to create ~64k+ OS threads on whatever machine runs this suite,
    // which is a real resource-exhaustion risk rather than a safe thing to assert against. The fix
    // itself was instead verified by inspection (see the artifact's #13 writeup) and by this test
    // passing with the corrected clamp, which compares against `fiberThreadCount - 1` instead.
    //
    // -10000 is arbitrary but comfortably exceeds any real hardware_concurrency(), so the clamp is
    // exercised regardless of how many cores the test machine reports.
    TEST(FibersManager, NegativeRequestedThreadCountClampsToOne)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(-10000, allocator);

        EXPECT_EQ(fibersManager.GetFiberThreadCount(), 1);
    }

    // Regression test for #15's follow-up: InitAndBatchJobs()/InitAndBatchJobsNoCounter() used to
    // copy the batch's callable into every job. The final design shares one instance across every
    // job in the batch instead (FiberJob::SharedFunction, held via IntrusiveSharedPtr): the callable
    // is moved into it exactly once, and every job just holds a pointer to it, bumping a cheap
    // atomic refcount rather than copying the (potentially heap-allocating) callable at all.
    //
    // This verifies the *exact* copy count, not just that the batch still runs correctly: a
    // captured CopyCounter member increments a shared counter from its copy constructor only (its
    // move constructor is untouched), so any copy of the callable anywhere in this path -- whether
    // from InitAndBatchJobs() itself, or from AllocatorInstance::New() failing to forward its
    // constructor arguments (a real bug this test caught: New() used to build T(_args...) from its
    // named forwarding-reference parameters directly, which are lvalues as expressions regardless
    // of their deduced reference type, silently downgrading every intended move into a copy) --
    // shows up immediately as a nonzero count.
    TEST(FibersManager, InitAndBatchJobsNeverCopiesTheSharedCallable)
    {
        AllocatorInstance allocator{};
        FibersManager fibersManager(4, allocator);

        struct CopyCounter
        {
            std::shared_ptr<std::atomic<u32>> m_count;

            explicit CopyCounter(std::shared_ptr<std::atomic<u32>> _count) : m_count(eastl::move(_count)) {}
            CopyCounter(const CopyCounter& _other) : m_count(_other.m_count)
            {
                m_count->fetch_add(1, std::memory_order_relaxed);
            }
            CopyCounter(CopyCounter&&) noexcept = default;
        };

        const auto copyCount = std::make_shared<std::atomic<u32>>(0);
        constexpr u32 kJobCount = 5;
        std::atomic<u32> ranCount = 0;

        FiberJob::Desc desc{
            .m_function = [tracker = CopyCounter(copyCount), &ranCount](u16)
            {
                ranCount.fetch_add(1, std::memory_order_relaxed);
            },
            .m_jobCount = kJobCount,
        };

        // Only measure what InitAndBatchJobs() itself does with the callable -- not the setup above
        // (constructing CopyCounter, capturing it into the lambda, building the eastl::function, or
        // constructing desc), none of which this test is about.
        copyCount->store(0, std::memory_order_relaxed);

        const auto counter = fibersManager.InitAndBatchJobs(eastl::move(desc));
        fibersManager.WaitForCounterAndReset(counter);

        EXPECT_EQ(ranCount.load(), kJobCount);
        // All 5 jobs share one instance of the callable -- not one copy each, not even kJobCount - 1.
        EXPECT_EQ(copyCount->load(), 0u);
    }
} // KryneEngine::Tests
