/**
 * @file
 * @author Max Godefroy
 * @date 18/01/2026.
 */

#pragma once

#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

#include "KryneEngine/Modules/Resources/ResourceEntry.hpp"
#include "KryneEngine/Modules/Resources/ResourceTypeId.hpp"

namespace KryneEngine::Modules::Resources
{
#define KE_DECLARE_RESOURCE_TYPE(name) static constexpr KryneEngine::Modules::Resources::ResourceTypeId kTypeId = KryneEngine::Modules::Resources::GenerateResourceTypeId(name)

    template <class ResourceManager>
    class ResourceBase
    {
        friend ResourceManager;

    public:
        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

        [[nodiscard]] ResourceManager* GetResourceManager() const { return m_resourceManager; }

        [[nodiscard]] u64 GetVersion() const { return m_version; }

        virtual ~ResourceBase() = default;

    protected:
        AllocatorInstance m_allocator;
        ResourceManager* m_resourceManager;
        size_t m_version;

        ResourceBase(const AllocatorInstance _allocator, ResourceManager* _resourceManager, const size_t _version)
            : m_allocator(_allocator)
            , m_resourceManager(_resourceManager)
            , m_version(_version)
        {}
    };

    template <class ResourceManager>
    class RefCountedResourceBase: public ResourceBase<ResourceManager>
    {
        friend ResourceManager;

    public:
        s32 m_refCount = 0;

        ~RefCountedResourceBase() override
        {
            // Only clear the entry if it still holds this resource: a newer one may have been loaded into it since
            // this one was released. The entry stores the most derived pointer, which is this one for a resource
            // deriving from this class first.
            void* expected = static_cast<void*>(this);
            m_entry->m_resource.compare_exchange_strong(expected, nullptr, std::memory_order::acq_rel);
        }

    protected:
        ResourceEntry* m_entry;

        RefCountedResourceBase(
            AllocatorInstance _allocator,
            ResourceManager* _resourceManager,
            ResourceEntry* _entry,
            size_t _version)
                : ResourceBase<ResourceManager>(_allocator, _resourceManager, _version)
                , m_entry(_entry)
        {}
    };
}
