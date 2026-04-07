/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/StringHelpers.hpp>

namespace KryneEngine::Modules::FileSystem
{
    class VirtualFileSystem;
}

namespace KryneEngine::Modules::Resources
{
    class IResourceManager;
    struct ResourceEntry;

    class IResourceLoader
    {
    public:
        virtual ~IResourceLoader() = default;

        virtual void RequestLoad(
            const StringHash& _path,
            ResourceEntry* _entry,
            IResourceManager* _resourceManager,
            u64 _loadFlags) = 0;

    protected:
        explicit IResourceLoader(FileSystem::VirtualFileSystem* _vfs): m_vfs(_vfs) {}

        FileSystem::VirtualFileSystem* m_vfs = nullptr;
    };
}
