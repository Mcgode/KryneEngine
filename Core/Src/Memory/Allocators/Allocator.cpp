/**
 * @file
 * @author Max Godefroy
 * @date 15/02/2025.
 */

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

#include "KryneEngine/Core/Platform/StdAlloc.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

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
                TracyAllocN(ptr, _size, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::Malloc(_size);
#if KE_PROFILE_MEMORY_ALLOCATIONS
           TracyAlloc(ptr, _size);
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
                TracyAllocN(ptr, _size, m_allocator->GetName());
#endif
        }
        else
        {
            ptr = StdAlloc::MemAlign(_size, _alignment);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyAlloc(ptr, _size);
#endif
        }
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + _alignmentOffset);
    }

    void AllocatorInstance::deallocate(void* _ptr, const size_t _size) const
    {
        if (m_allocator)
        {
            m_allocator->Free(_ptr, _size);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            if (!m_allocator->IsCustomProfiling())
                TracyFreeN(_ptr, m_allocator->GetName());
#endif
        }
        else
        {
            StdAlloc::Free(_ptr);
#if KE_PROFILE_MEMORY_ALLOCATIONS
            TracyFree(_ptr);
#endif
        }
    }
} // namespace KryneEngine