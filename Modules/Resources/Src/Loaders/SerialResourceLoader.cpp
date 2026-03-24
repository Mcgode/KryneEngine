/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#include "KryneEngine/Modules/Resources/Loaders/SerialResourceLoader.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

#include <fstream>

namespace KryneEngine::Modules::Resources
{
    SerialResourceLoader::SerialResourceLoader(AllocatorInstance _allocator)
        : m_pendingRequests(_allocator)
    {}

    void SerialResourceLoader::RequestLoad(
        const StringHash& _path,
        ResourceEntry* _entry,
        IResourceManager* _resourceManager,
        const u64)
    {
        {
            const auto lock = m_lock.AutoLock();
            if (m_pendingRequests.find(_path) == m_pendingRequests.end())
                m_pendingRequests.emplace(_path);
            else
                return;
        }

        {
            const eastl::span loadedResourceData = _resourceManager->LoadResource(_entry, _path.m_string);
            if (loadedResourceData.empty())
            {
                _resourceManager->ReportFailedLoad(_entry, _path.m_string);
            }
            else
            {
                _resourceManager->FinalizeResourceLoading(_entry, loadedResourceData, _path.m_string);
            }
        }

        {
            const auto lock = m_lock.AutoLock();
            m_pendingRequests.erase(_path);
        }
    }
} // namespace KryneEngine::Modules::Resources