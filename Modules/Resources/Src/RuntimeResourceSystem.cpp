/**
 * @file
 * @author Max Godefroy
 * @date 16/01/2026.
 */

#include "KryneEngine/Modules/Resources/RuntimeResourceSystem.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/StableVector.inl>

#include "KryneEngine/Modules/Resources/IResourceLoader.hpp"

namespace KryneEngine::Modules::Resources
{
    RuntimeResourceSystem::RuntimeResourceSystem(AllocatorInstance _allocator, IResourceLoader* _loader)
        : m_allocator(_allocator)
        , m_resourceLoader(_loader)
        , m_resourceManagers(_allocator)
        , m_resourceEntriesMap(_allocator)
        , m_resourceEntries(_allocator)
    {}

    RuntimeResourceSystem::~RuntimeResourceSystem() = default;

    ResourceEntry* RuntimeResourceSystem::GetResourceEntry(const StringHash& _name, u64 _typeId)
    {
        {
            const auto lock = m_resourceEntriesLock.AutoReadLock();
            const auto it = m_resourceEntriesMap.find(_name);
            if (it != m_resourceEntriesMap.end())
            {
                KE_ASSERT(it->second->m_typeId == _typeId);
                return it->second;
            }
        }
        const auto lock = m_resourceEntriesLock.AutoWriteLock();
        ResourceEntry& entry = m_resourceEntries.EmplaceBack();
        entry.m_typeId = _typeId;
        m_resourceEntriesMap.emplace(_name, &entry);
        return &entry;
    }

    void RuntimeResourceSystem::RegisterResourceManager(IResourceManager* _resourceManager, ResourceTypeId _typeId)
    {
        const auto lock = m_resourceManagersLock.AutoWriteLock();
        KE_ASSERT(m_resourceManagers.find(_typeId) == m_resourceManagers.end());
        m_resourceManagers.emplace(_typeId, _resourceManager);
    }

    IResourceManager* RuntimeResourceSystem::GetResourceManager(const ResourceTypeId _typeId) const
    {
        const auto lock = m_resourceManagersLock.AutoReadLock();
        const auto it = m_resourceManagers.find(_typeId);
        return it != m_resourceManagers.end() ? it->second : nullptr;
    }

    void RuntimeResourceSystem::LoadResource(const StringHash& _name, ResourceEntry* _entry, const u64 _loadFlags)
    {
        IResourceManager* manager = nullptr;
        {
            const auto lock = m_resourceManagersLock.AutoReadLock();
            const auto it = m_resourceManagers.find(_entry->m_typeId);
            if (it == m_resourceManagers.end())
                return;
            manager = it->second;
        }
        m_resourceLoader->RequestLoad(_name, _entry, manager, _loadFlags);
    }
} // namespace KryneEngine::Modules::Resources