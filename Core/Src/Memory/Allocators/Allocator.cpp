/**
 * @file
 * @author Max Godefroy
 * @date 15/02/2025.
 */

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

#include "KryneEngine/Core/Platform/StdAlloc.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

#if !defined(KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS)
#   define KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS 0
#endif

namespace KryneEngine
{
    IAllocator::IAllocator(const char* _name, const bool _customProfiling)
        : m_customProfiling(_customProfiling)
    {
        std::snprintf(m_name, sizeof(m_name), "%s", _name);
    }

    void* AllocatorInstance::allocate(const size_t _size, const int _flags) const
    {
        void* ptr = nullptr;
        if (m_allocator)
        {
            ptr =  m_allocator->Allocate(_size, 0);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            if (!m_allocator->IsCustomProfiling())
                TracyAllocNS(ptr, _size, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::Malloc(_size);
#if KE_PROFILE_MEMORY_ALLOCATIONS
           TracyAllocS(ptr, _size, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS);
#endif
        }
        return ptr;
    }

    void* AllocatorInstance::allocate(const size_t _size, const size_t _alignment, const size_t _alignmentOffset, const int _flags) const
    {
        void* ptr;
        if (m_allocator)
        {
            ptr = m_allocator->Allocate(_size, _alignment);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            if (!m_allocator->IsCustomProfiling())
                TracyAllocNS(ptr, _size, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::MemAlign(_size, _alignment);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAllocS(ptr, _size, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS);
#endif
        }
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + _alignmentOffset);
    }

    void AllocatorInstance::deallocate(void* _ptr, const size_t _size) const
    {
        if (m_allocator)
        {
            // Cache m_allocator locally before calling Free(): this AllocatorInstance can itself be
            // a sub-object living inside the very block being freed here (e.g. TlsfAllocator::Destroy()
            // calls _allocator->m_parentAllocator.deallocate(_allocator, ...) -- freeing _allocator's
            // own memory via a member of _allocator). Free() poisons that entire span under ASAN, and
            // re-reading m_allocator through `this` afterwards -- as this used to, for the profiling
            // check below -- reads through memory that was just poisoned by the call this same
            // expression made one line earlier. The local copy sidesteps that entirely.
            IAllocator* const allocator = m_allocator;
#if KE_PROFILE_MEMORY_ALLOCATIONS
            // Report the free to Tracy *before* performing the real free: once Free() returns, another
            // thread's allocate() can immediately reuse this address, and if its TracyAllocNS reaches the
            // profiler before this thread's TracyFreeNS does, Tracy sees an allocation for an address it
            // still considers live ("already tracked and not freed").
            if (!allocator->IsCustomProfiling())
                TracyFreeNS(_ptr, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS, allocator->GetName());
#endif
            allocator->Free(_ptr, _size);
        }
        else
        {
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyFreeS(_ptr, KE_PROFILE_MEMORY_ALLOCATIONS_CALLSTACKS);
#endif
            StdAlloc::Free(_ptr);
        }
    }
} // namespace KryneEngine