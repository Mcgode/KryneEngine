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
#include <KryneEngine/Core/Threads/LightweightMutex.hpp>

#include "KryneEngine/Modules/FileSystem/DirectoryTree.hpp"
#include "KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp"

namespace KryneEngine::Modules::FileSystem
{
    class Archive;

    class VirtualFileSystem
    {
        friend ReadOnlyFile;

    public:
        explicit VirtualFileSystem(AllocatorInstance _allocator, u32 _maxOpenFiles = 512);

        ~VirtualFileSystem();

        bool MountArchive(eastl::string_view _archivePath);

        ReadOnlyFile OpenReadOnlyFile(eastl::string_view _filePath, bool _skipVirtualMapping = false);

    private:
        AllocatorInstance m_allocator;
        eastl::vector<Archive*> m_archives;

        DirectoryTree m_mountPointsTree;

        LruCache<StringHash, Platform::ReadOnlyFileDescriptor> m_openFiles;
        LightweightMutex m_archiveMutex;

        Platform::ReadOnlyFileDescriptor* GetFileDescriptor(const StringHash& _hash);
    };
}
