/**
 * @file
 * @author Max Godefroy
 * @date 28/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"
#include "KryneEngine/Core/Memory/Allocators/StackAllocator.hpp"
#include "KryneEngine/Core/Memory/IntrusivePtr.hpp"

#include <thread>

namespace KryneEngine
{
    /**
     * @brief Provides each thread with its own lazily-created scratch (stack) allocator.
     *
     * @details
     * The first call to GetScratchAllocator() on a given thread creates that thread's StackAllocator, sized from
     * SetInitialScratchAllocatorSize() and backed by the allocator passed to SetParentAllocator(); further calls on
     * the same thread reuse it. The allocator is released automatically when its owning thread exits, or explicitly
     * through FreeThreadAllocator().
     *
     * @warning SetParentAllocator() and SetInitialScratchAllocatorSize() only affect threads that haven't yet
     * created their scratch allocator; call them before any thread requests one.
     */
    class GlobalScratchAllocator
    {
    public:
        /**
         * @brief RAII handle to the calling thread's scratch allocator, popping back to the state it captured on
         * construction once it goes out of scope.
         *
         * @warning Must be destroyed on the same thread it was created on, and is neither copyable nor movable.
         *
         * @warning Must not be held across anything that can yield the current job (waiting on a sync counter,
         * job-system I/O, etc.): the underlying allocator is tied to the OS thread that created it, and the job
         * may resume on a different thread afterward, leaving the scope pointing at the wrong thread's allocator.
         */
        struct ScopedScratchAllocator
        {
            friend class GlobalScratchAllocator;

            ~ScopedScratchAllocator();

            KE_DEFINE_COPY_MOVE_SEMANTICS(ScopedScratchAllocator, delete, delete);

            StackAllocator::Scope m_scope;

            [[nodiscard]] AllocatorInstance GetAllocator() const { return m_scope.GetAllocator(); }

        private:
            explicit ScopedScratchAllocator(StackAllocator* _allocator);

#if !defined(KE_FINAL)
            std::thread::id m_threadId {};
#endif
        };

        /**
         * @brief Sets the allocator used to create each thread's scratch allocator.
         *
         * @warning Only applies to threads that create their scratch allocator after this call; threads that
         * already have one keep the parent allocator they were created with.
         */
        static void SetParentAllocator(AllocatorInstance _parentAllocator);

        /**
         * @brief Sets the initial heap size used when a thread's scratch allocator is first created.
         *
         * @warning Only applies to threads that haven't created their scratch allocator yet.
         */
        static void SetInitialScratchAllocatorSize(size_t _size);

        /**
         * @brief Retrieves the calling thread's scratch allocator, creating it on first call.
         */
        static ScopedScratchAllocator GetScratchAllocator();

        /**
         * @brief Releases the calling thread's scratch allocator, freeing its heaps.
         *
         * @warning Must not be called while a ScopedScratchAllocator obtained on this thread is still alive.
         */
        static void FreeThreadAllocator();

    private:
        static constexpr size_t kDefaultInitialScratchAllocatorSize = 64 << 10; // 64 KiB

        thread_local static IntrusiveUniquePtr<StackAllocator> tl_scratchAllocator;
        static AllocatorInstance s_parentAllocator;
        static size_t s_initialScratchAllocatorSize;
    };
} // namespace KryneEngine
