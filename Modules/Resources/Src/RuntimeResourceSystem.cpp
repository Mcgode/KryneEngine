/**
 * @file
 * @author Max Godefroy
 * @date 16/01/2026.
 */

#include "KryneEngine/Modules/Resources/RuntimeResourceSystem.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/StableVector.inl>

#include "KryneEngine/Modules/Resources/IResourceLoader.hpp"
#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

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
        // Force increment to unlock the resource entry if it was at zero.
        // The first ref is the caller's responsibility.
        // The second ref is the resource loader's responsibility. It is added to avoid the resource being unloaded
        // during loading.
        s64 value = _entry->m_refCount.load(std::memory_order::relaxed);
        for (;;)
        {
            if (value == ResourceEntry::kUnloading)
            {
                Threads::CpuYield();
                value = _entry->m_refCount.load(std::memory_order::relaxed);
            }
            else if (_entry->m_refCount.compare_exchange_weak(
                value,
                value + 2,
                std::memory_order::release,
                std::memory_order::relaxed))
            {
                break;
            }
        }

        // On rare occasions, two threads can call LoadResource at about the same time for an unloaded entry. To avoid
        // unnecessary dual loads, we allow to abort early.
        // This can also trigger on voluntary reloads, so we ask the resource loader if it is allowed to reload.
        if (value != 0 && !m_resourceLoader->AllowReload(_loadFlags))
        {
            KE_VERIFY(_entry->m_refCount.fetch_sub(1, std::memory_order::acq_rel) > 1);
            return;
        }

        IResourceManager* manager = nullptr;
        {
            const auto lock = m_resourceManagersLock.AutoReadLock();
            const auto it = m_resourceManagers.find(_entry->m_typeId);
            VERIFY_OR_RETURN_VOID(it != m_resourceManagers.end());
            manager = it->second;
        }
        m_resourceLoader->RequestLoad(_name, _entry, manager, _loadFlags);
    }
} // namespace KryneEngine::Modules::Resources