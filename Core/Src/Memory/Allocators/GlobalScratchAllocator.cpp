/**
 * @file
 * @author Max Godefroy
 * @date 28/09/2026.
 */

#include "KryneEngine/Core/Memory/Allocators/GlobalScratchAllocator.hpp"

namespace KryneEngine
{
    thread_local IntrusiveUniquePtr<StackAllocator> GlobalScratchAllocator::tl_scratchAllocator {};
    AllocatorInstance GlobalScratchAllocator::s_parentAllocator {};
    size_t GlobalScratchAllocator::s_initialScratchAllocatorSize = kDefaultInitialScratchAllocatorSize;

    GlobalScratchAllocator::ScopedScratchAllocator::~ScopedScratchAllocator()
    {
#if !defined(KE_FINAL)
        KE_ASSERT_MSG(std::this_thread::get_id() == m_threadId, "The scoped allocator must be released in the same thread it was created in");
#endif
        m_scope.~Scope();
    }

    GlobalScratchAllocator::ScopedScratchAllocator::ScopedScratchAllocator(StackAllocator* _allocator)
        : m_scope(_allocator->Scoped())
    {
#if !defined(KE_FINAL)
        m_threadId = std::this_thread::get_id();
#endif
    }

    void GlobalScratchAllocator::SetParentAllocator(const AllocatorInstance _parentAllocator)
    {
        s_parentAllocator = _parentAllocator;
    }

    void GlobalScratchAllocator::SetInitialScratchAllocatorSize(const size_t _size)
    {
        s_initialScratchAllocatorSize = _size;
    }

    GlobalScratchAllocator::ScopedScratchAllocator GlobalScratchAllocator::GetScratchAllocator()
    {
        if (tl_scratchAllocator == nullptr)
        {
            char name[256];
            std::thread::id id = std::this_thread::get_id();
            snprintf(name, sizeof(name), "ScratchAllocator_0x%lx", *reinterpret_cast<size_t*>(&id));
            tl_scratchAllocator.Reset(s_parentAllocator.New<StackAllocator>(
                s_parentAllocator,
                s_initialScratchAllocatorSize,
                5,
                name));
        }

        return ScopedScratchAllocator(tl_scratchAllocator.Get());
    }

    void GlobalScratchAllocator::FreeThreadAllocator()
    {
        KE_ASSERT(tl_scratchAllocator == nullptr || tl_scratchAllocator->GetStackIndex() == 0);
        tl_scratchAllocator.Reset();
    }
} // KryneEngine