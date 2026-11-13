/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Assert.hpp"


#include <KryneEngine/Core/Memory/IntrusivePtr.hpp>
#include <KryneEngine/Core/Threads/HelperFunctions.hpp>
#include <atomic>
#include <limits>

namespace KryneEngine::Modules::Resources
{
    struct ResourceEntry
    {
        std::atomic<void*> m_resource = nullptr;
        std::atomic<size_t> m_version = 0;
        u64 m_typeId = 0;
        std::atomic<s64> m_refCount = 0;

        static constexpr s64 kUnloading = std::numeric_limits<s64>::min();

        /**
         * @return `false` if the entry is unloaded. If it is, the caller should load the resource.
         */
        bool TryUseResource()
        {
            s64 current = m_refCount.load(std::memory_order::relaxed);

            // Safely try to increment the ref count from a value greater than zero.
            // Stall while an unload is in progress.
            for (;;)
            {
                if (current == kUnloading)
                {
                    Threads::CpuYield();
                    current = m_refCount.load(std::memory_order::relaxed);
                }
                else if (current == 0)
                {
                    return false;
                }
                else if (m_refCount.compare_exchange_weak(
                    current,
                    current + 1,
                    std::memory_order::acquire,
                    std::memory_order::relaxed))
                {
                    return true;
                }
            }
        }

        template <class Resource> requires IsAllocatorIntrusible<Resource>
        Resource* GetResource() const
        {
            KE_ASSERT(Resource::kTypeId == m_typeId);
            return static_cast<Resource*>(m_resource.load(std::memory_order::acquire));
        }

        template <class Resource> requires IsAllocatorIntrusible<Resource>
        void ReleaseResource()
        {
            KE_ASSERT(Resource::kTypeId == m_typeId);

            s64 current = m_refCount.load(std::memory_order::acquire);
            s64 next;
            do
            {
                KE_ASSERT_MSG(current > 0, "More releases than uses");
                next = current == 1 ? kUnloading : current - 1;
            }
            while (!m_refCount.compare_exchange_weak(
                current,
                next,
                std::memory_order::acq_rel,
                std::memory_order::relaxed));

            if (next == kUnloading)
            {
                auto* resource = static_cast<Resource*>(m_resource.exchange(nullptr, std::memory_order::acq_rel));
                if (resource != nullptr)
                {
                    if constexpr (IsAllocatorVarIntrusible<Resource>)
                    {
                        resource->m_allocator.Delete(resource);
                    }
                    else
                    {
                        resource->GetAllocator().Delete(resource);
                    }
                }
                m_refCount.store(0, std::memory_order::release);
            }
        }
    };
}