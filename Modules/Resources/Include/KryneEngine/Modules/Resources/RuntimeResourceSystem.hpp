/**
 * @file
 * @author Max Godefroy
 * @date 16/01/2026.
 */

#pragma once

#include <EASTL/hash_map.h>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Memory/Containers/StableVector.hpp>
#include <KryneEngine/Core/Threads/RwSpinLock.hpp>

#include "KryneEngine/Modules/Resources/ResourceEntry.hpp"
#include "KryneEngine/Modules/Resources/ResourceTypeId.hpp"

namespace KryneEngine::Modules::Resources
{
    class IResourceLoader;
    class IResourceManager;

    class RuntimeResourceSystem
    {
    public:
        explicit RuntimeResourceSystem(AllocatorInstance _allocator, IResourceLoader* _resourceLoader);
        ~RuntimeResourceSystem();

        template <class Resource>
        void RegisterResourceManager(IResourceManager* _resourceManager)
        {
            RegisterResourceManager(_resourceManager, Resource::kTypeId);
        }

        template <class Resource, class ResourceManager>
        [[nodiscard]] ResourceManager* GetResourceManager()
        {
            return reinterpret_cast<ResourceManager*>(GetResourceManager(Resource::kTypeId));
        }

        template <class Resource>
        [[nodiscard]] ResourceEntry* GetResourceEntry(const StringHash& _name)
        {
            return GetResourceEntry(_name, Resource::kTypeId);
        }
        [[nodiscard]] ResourceEntry* GetResourceEntry(const StringHash& _name, u64 _typeId);

        template<class Enum>
        void LoadResource(const StringHash& _name, ResourceEntry* _entry, Enum _loadFlags)
        {
            LoadResource(_name, _entry, static_cast<u64>(_loadFlags));
        }

        /**
         * @brief Requests a resource load, and acquires a reference to the resource entry.
         *
         * @note
         * Load increments the entry ref count by one which the caller is responsible for.
         */
        void LoadResource(const StringHash& _name, ResourceEntry* _entry, u64 _loadFlags = 0);

        template <class Resource, class Enum>
        ResourceEntry* UseOrLoadResource(const StringHash& _name, const Enum _loadFlags)
        {
            ResourceEntry* entry = GetResourceEntry<Resource>(_name);
            if (!entry->TryUseResource())
                LoadResource(_name, entry, _loadFlags);
            return entry;
        }

    private:
        AllocatorInstance m_allocator;
        IResourceLoader* m_resourceLoader;

        eastl::vector_map<ResourceTypeId, IResourceManager*> m_resourceManagers;
        eastl::hash_map<StringHash, ResourceEntry*> m_resourceEntriesMap;
        StableVector<ResourceEntry, 255> m_resourceEntries;

        alignas(Threads::kCacheLineSize) mutable RwSpinLock m_resourceManagersLock {};
        alignas(Threads::kCacheLineSize) mutable RwSpinLock m_resourceEntriesLock {};

        void RegisterResourceManager(IResourceManager* _resourceManager, ResourceTypeId _typeId);
        IResourceManager* GetResourceManager(ResourceTypeId _typeId) const;
    };
}
