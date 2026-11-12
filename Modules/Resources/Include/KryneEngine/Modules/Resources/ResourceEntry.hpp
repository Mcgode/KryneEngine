/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Assert.hpp"


#include <KryneEngine/Core/Memory/IntrusivePtr.hpp>
#include <atomic>

namespace KryneEngine::Modules::Resources
{
    struct ResourceEntry
    {
        std::atomic<void*> m_resource = nullptr;
        std::atomic<size_t> m_version = 0;
        u64 m_typeId = 0;

        template <class Resource> requires IsAllocatorIntrusible<Resource> && (!IsRefCountIntrusible<Resource>)
        Resource* UseResource() const
        {
            KE_ASSERT(Resource::kTypeId == m_typeId);
            return static_cast<Resource*>(m_resource.load(std::memory_order::acquire));
        }

        /**
         * @brief Takes a reference on the loaded resource.
         * @return The resource, or an empty pointer if none is loaded or it is being released.
         *
         * @warning Only safe if the resource's manager keeps a resource from being deleted during the call, for
         * example with a lock taken both here and in the resource's destructor. See
         * `IntrusiveSharedPtr::TryAcquire()`.
         */
        template <class Resource> requires IsAllocatorIntrusible<Resource> && IsRefCountIntrusible<Resource>
        IntrusiveSharedPtr<Resource> UseResource() const
        {
            KE_ASSERT(Resource::kTypeId == m_typeId);
            return IntrusiveSharedPtr<Resource>::TryAcquire(
                static_cast<Resource*>(m_resource.load(std::memory_order::acquire)));
        }
    };
}