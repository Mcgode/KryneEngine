/**
 * @file
 * @author Max Godefroy
 * @date 02/07/2022.
 */

#include "FiberContext.hpp"

#include <EASTL/allocator.h>

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Threads/FibersManager.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

#if defined(HAS_ASAN)
extern "C" {
    void __sanitizer_start_switch_fiber(void** fake_stack_save,
                                        const void* stack_bottom,
                                        size_t stack_size);
    void __sanitizer_finish_switch_fiber(void* fake_stack_save,
                                         const void** stack_bottom_old,
                                         size_t* stack_size_old);
}
#endif

namespace KryneEngine
{
    void FiberContext::SwapContext(FiberContext *_new)
    {
        TracyFiberLeave;

        // Make sure the next fiber is indeed paused before executing it
        _new->m_mutex.ManualLock();

#if defined(HAS_ASAN)
        // This pointer will live on this stack frame.
        void *fake_stack_save = nullptr;
        __sanitizer_start_switch_fiber(
            &fake_stack_save,
            _new->m_stackBottom,
            _new->m_stackSize);
#endif

        const boost::context::detail::transfer_t t = boost::context::detail::jump_fcontext(
            _new->m_context,
            this);

        if (KE_VERIFY(t.data != nullptr))
        {
            auto* fiberContext = static_cast<FiberContext*>(t.data);
            fiberContext->m_context = t.fctx;
            fiberContext->m_mutex.ManualUnlock(); // Mark previous fiber as free to be used again.
        }

#if defined(HAS_ASAN)
        // When we return from the context switch we indicate that to ASAN.
        __sanitizer_finish_switch_fiber(
            fake_stack_save,
            nullptr,
            nullptr);
#endif

        TracyFiberEnter(m_name.c_str());
    }

    void FiberContext::RunFiber(boost::context::detail::transfer_t _transfer)
    {

        const auto fibersManager = FibersManager::GetInstance();
        VERIFY_OR_RETURN_VOID(fibersManager != nullptr);
        fibersManager->_OnContextSwitched();

        if (KE_VERIFY(_transfer.data != nullptr))
        {
            auto* fiberContext = static_cast<FiberContext*>(_transfer.data);
            fiberContext->m_context = _transfer.fctx;
            fiberContext->m_mutex.ManualUnlock(); // Mark previous fiber as free to be used again.

#if defined(HAS_ASAN)
            __sanitizer_finish_switch_fiber(
                nullptr,
                &fiberContext->m_stackBottom,
                &fiberContext->m_stackSize);
#endif
        }

        while (true)
        {
            auto* job = fibersManager->GetCurrentJob();

            TracyFiberEnter(job->m_context->m_name.c_str());

            if (KE_VERIFY(job->m_status.load(std::memory_order_acquire) == FiberJob::Status::PendingStart))
            {
                job->m_status.store(FiberJob::Status::Running, std::memory_order_release);
                job->m_function(job->m_jobIndex);
                job->m_status.store(FiberJob::Status::Finished, std::memory_order_release);
            }

            fibersManager->YieldJob();
        }
    }

    FiberContextAllocator::FiberContextAllocator(AllocatorInstance _allocator)
    {
        {
            const auto smallLock = m_availableSmallContextsIds.m_spinLock.AutoLock();
            const auto bigLock = m_availableBigContextsIds.m_spinLock.AutoLock();

            m_availableSmallContextsIds.m_priorityQueue.get_container().set_allocator(_allocator);
            m_availableBigContextsIds.m_priorityQueue.get_container().set_allocator(_allocator);

            m_availableSmallContextsIds.m_priorityQueue.get_container().reserve(kSmallStackCount);
            for (u16 i = 0; i < kSmallStackCount; i++)
            {
                m_availableSmallContextsIds.m_priorityQueue.push(i);
            }

            m_availableBigContextsIds.m_priorityQueue.get_container().reserve(kBigStackCount);
            for (u16 i = 0; i < kBigStackCount; i++)
            {
                m_availableBigContextsIds.m_priorityQueue.push(i + kSmallStackCount);
            }

            m_smallStacks = static_cast<SmallStack*>(_allocator.allocate(
                sizeof(SmallStack) * static_cast<size_t>(kSmallStackCount),
                kStackAlignment));
            m_bigStacks = static_cast<BigStack*>(_allocator.allocate(
                sizeof(BigStack) * static_cast<size_t>(kBigStackCount),
                kStackAlignment));

            for (u32 i = 0; i < m_contexts.size(); i++)
            {
                auto& context = m_contexts[i];

                if (i < kSmallStackCount)
                {
                    context.m_context = make_fcontext(
                        m_smallStacks + i + 1, // Do a +1, because stack begins from the end
                        sizeof(SmallStack),
                        FiberContext::RunFiber);
                    context.m_name.sprintf("Fiber %d", i);
#if defined(HAS_ASAN)
                    context.m_stackBottom = m_smallStacks + i,
                    context.m_stackSize = sizeof(SmallStack);
#endif
                }
                else
                {
                    const u16 index = i - kSmallStackCount;
                    context.m_context = make_fcontext(
                        m_bigStacks + index + 1, // Do a +1, because stack begins from the end
                        sizeof(BigStack),
                        FiberContext::RunFiber);
                    context.m_name.sprintf("Big Fiber %d", index);
#if defined(HAS_ASAN)
                    context.m_stackBottom = m_bigStacks + index;
                    context.m_stackSize = sizeof(BigStack);
#endif
                }
            }
        }
    }

    FiberContextAllocator::~FiberContextAllocator()
    {
        AllocatorInstance allocator {};
        allocator.deallocate(m_smallStacks);
        allocator.deallocate(m_bigStacks);
    }

    bool FiberContextAllocator::Allocate(bool _bigStack, u16 &id_)
    {
        auto& queue = _bigStack
                ? m_availableBigContextsIds
                : m_availableSmallContextsIds;

        const auto lock = queue.m_spinLock.AutoLock();
        IF_NOT_VERIFY_MSG(!queue.m_priorityQueue.empty(), "Out of Fiber stacks!")
        {
            return false;
        }
        queue.m_priorityQueue.pop(id_);
        return true;
    }

    void FiberContextAllocator::Free(u16 _id)
    {
        VERIFY_OR_RETURN_VOID(_id < m_contexts.size());

        if (_id < kSmallStackCount)
        {
            const auto lock = m_availableSmallContextsIds.m_spinLock.AutoLock();
            m_availableSmallContextsIds.m_priorityQueue.push(_id);
        }
        else
        {
            const auto lock = m_availableBigContextsIds.m_spinLock.AutoLock();
            m_availableBigContextsIds.m_priorityQueue.push(_id);
        }
    }

    FiberContext *FiberContextAllocator::GetContext(u16 _id)
    {
        VERIFY_OR_RETURN(_id < m_contexts.size(), nullptr);
        return &m_contexts[_id];
    }
}
