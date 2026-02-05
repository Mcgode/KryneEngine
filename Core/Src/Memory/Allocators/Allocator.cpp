/**
 * @file
 * @author Max Godefroy
 * @date 15/02/2025.
 */

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

#include "KryneEngine/Core/Memory/Allocators/DefaultHeapHeapAllocationTracker.hpp"
#include "KryneEngine/Core/Platform/StdAlloc.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    void* AllocatorInstance::allocate(size_t _size, int _flags) const
    {
        void* ptr = nullptr;
        if (m_allocator)
        {
            ptr =  m_allocator->Allocate(_size, 0);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAllocN(ptr, _size, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::Malloc(_size);
#if KE_TRACK_DEFAULT_HEAP_ALLOCATIONS
            DefaultHeapHeapAllocationTracker::GetInstance().RegisterAllocation(ptr, _size, 0);
#endif
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAlloc(ptr, _size);
#endif
        }
        return ptr;
    }

    void* AllocatorInstance::allocate(size_t _size, size_t _alignment, size_t _alignmentOffset, int _flags) const
    {
        void* ptr;
        if (m_allocator)
        {
            ptr = m_allocator->Allocate(_size, _alignment);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAllocN(ptr, _size, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::MemAlign(_size, _alignment);
#if KE_TRACK_DEFAULT_HEAP_ALLOCATIONS
            DefaultHeapHeapAllocationTracker::GetInstance().RegisterAllocation(ptr, _size, _alignment);
#endif
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAlloc(ptr, _size);
#endif
        }
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + _alignmentOffset);
    }

    void AllocatorInstance::deallocate(void* _ptr, size_t _size) const
    {
        if (m_allocator)
        {
            m_allocator->Free(_ptr, _size);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyFreeN(_ptr, m_allocator->GetName());
#endif
        }
        else
        {
#if KE_TRACK_DEFAULT_HEAP_ALLOCATIONS
            DefaultHeapHeapAllocationTracker::GetInstance().RegisterDeallocation(_ptr);
#endif
            StdAlloc::Free(_ptr);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyFree(_ptr);
#endif
        }
    }
} // namespace KryneEngine