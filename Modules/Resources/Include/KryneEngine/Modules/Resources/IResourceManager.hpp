/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include <EASTL/span.h>
#include <EASTL/string_view.h>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

namespace KryneEngine::Modules::FileSystem
{
    class ReadOnlyFile;
}

namespace KryneEngine::Modules::Resources
{
    struct ResourceEntry;

    class IResourceManager
    {
    public:
        virtual ~IResourceManager() = default;

        virtual eastl::span<std::byte> LoadResource(ResourceEntry* _entry, const FileSystem::ReadOnlyFile& _file);
        virtual void FinalizeResourceLoading(ResourceEntry* _entry, eastl::span<std::byte> _loadedResourceData, eastl::string_view _path) = 0;
        virtual void ReportFailedLoad(ResourceEntry* _entry, eastl::string_view _path) = 0;

        [[nodiscard]] virtual AllocatorInstance GetAllocator() const = 0;
    };
}
