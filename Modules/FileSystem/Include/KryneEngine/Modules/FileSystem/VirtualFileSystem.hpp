/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/Containers/LruCache.hpp>
#include <KryneEngine/Core/Platform/FileSystem.hpp>

#include "KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp"

namespace KryneEngine::Modules::FileSystem
{
    class VirtualFileSystem
    {
        friend ReadOnlyFile;

    public:
        explicit VirtualFileSystem(AllocatorInstance _allocator, u32 _maxOpenFiles = 512);

        ReadOnlyFile OpenReadOnlyFile(eastl::string_view _filePath);

    private:
        AllocatorInstance m_allocator;
        LruCache<StringHash, Platform::ReadOnlyFileDescriptor> m_openFiles;
    };
}
