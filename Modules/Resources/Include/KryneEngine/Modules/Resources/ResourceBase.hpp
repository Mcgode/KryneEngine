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
        using ResourceManagerType = ResourceManager;

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
}
