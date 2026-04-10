/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp"

#include <KryneEngine/Core/Memory/Containers/LruCache.inl>

#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Modules/FileSystem/Archive.hpp"
#include "KryneEngine/Modules/FileSystem/Utils.hpp"

namespace KryneEngine::Modules::FileSystem
{
    VirtualFileSystem::VirtualFileSystem(const AllocatorInstance _allocator, const u32 _maxOpenFiles)
        : m_allocator(_allocator)
        , m_mountPointsTree(_allocator)
        , m_openFiles(_allocator, _maxOpenFiles)
    {
    }

    VirtualFileSystem::~VirtualFileSystem()
    {
        const auto lock = m_archiveMutex.AutoLock();
        for (const auto* archive: m_archives)
            m_openFiles.Release(archive->GetFileDescriptor());
    }

    bool VirtualFileSystem::MountArchive(const eastl::string_view _archivePath)
    {
        KE_ZoneScopedF("Mounting archive '%s'", _archivePath.data());

        Archive* archive;
        {
            ReadOnlyFile archiveFile = OpenReadOnlyFile(_archivePath, true);
            const StringHash hash(_archivePath);
            archive = Archive::Load(m_allocator, GetFileDescriptor(hash));
        }

        if (archive == nullptr)
            return false;

        eastl::string rawMountPoint(m_allocator);
        rawMountPoint = _archivePath.substr(0, _archivePath.find_last_of('/'));
        rawMountPoint += "/";
        rawMountPoint += archive->GetMountPoint().data();

        eastl::string mountPoint(m_allocator);
        NormalizePath(rawMountPoint, mountPoint);

        {
            const auto lock = m_archiveMutex.AutoLock();

            if (m_mountPointsTree.AddDirectory(mountPoint, archive))
            {
                m_archives.push_back(archive);
                return true;
            }
        }
        m_allocator.Delete(archive);
        return false;
    }

    ReadOnlyFile VirtualFileSystem::OpenReadOnlyFile(const eastl::string_view _filePath, const bool _skipVirtualMapping)
    {
        eastl::string filePath(m_allocator);
        NormalizePath(_filePath, filePath);

        if (!_skipVirtualMapping)
        {
            DirectoryTree::SpecificDirectoryResult result;
            {
                const auto lock = m_archiveMutex.AutoLock();
                result = m_mountPointsTree.FindMostSpecificDirectory(filePath);
            }
            if (result.m_directoryPtr != nullptr)
            {
                const auto* archive = static_cast<Archive*>(result.m_directoryPtr);
                const StringViewHash relativePathHash { result.m_relativePath };
                const auto it = archive->GetFileEntry(relativePathHash);
                if (it != nullptr)
                {
                    return {
                        this,
                        m_openFiles.Acquire(archive->GetFileDescriptor()),
                        it->m_offset,
                        it->m_size,
                        it->m_flags,
                    };
                }
            }
        }

        const std::filesystem::path path(filePath.data());

        if (!std::filesystem::exists(path))
        {
            return {};
        }

        const StringHash hash { filePath };
        Platform::ReadOnlyFileDescriptor* fileDescriptor = GetFileDescriptor(hash);

        if (fileDescriptor == nullptr)
        {
            return {};
        }
        if (!fileDescriptor->IsValid())
        {
            m_openFiles.Release(fileDescriptor);
            return {};
        }

        return {
            this,
            fileDescriptor,
            0,
            Platform::GetFileSize(*fileDescriptor),
            FileFlags::None
        };
    }

    Platform::ReadOnlyFileDescriptor* VirtualFileSystem::GetFileDescriptor(const StringHash& _hash)
    {
        return m_openFiles.Acquire(_hash,
            [this, &_hash](const bool _mustReclaim, Platform::ReadOnlyFileDescriptor* _fileDescriptor)
            {
                if (_mustReclaim)
                {
                    KE_ASSERT(_fileDescriptor != nullptr);
                    Platform::CloseReadOnlyFile(*_fileDescriptor, m_allocator);
                }

                *_fileDescriptor = Platform::OpenReadOnlyFile(_hash.m_string, m_allocator);
            });
    }
}
