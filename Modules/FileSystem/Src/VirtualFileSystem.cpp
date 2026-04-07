/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp"

namespace KryneEngine::Modules::FileSystem
{
    VirtualFileSystem::VirtualFileSystem(const AllocatorInstance _allocator, const u32 _maxOpenFiles)
        : m_allocator(_allocator)
        , m_openFiles(_allocator, _maxOpenFiles)
    {}

    ReadOnlyFile VirtualFileSystem::OpenReadOnlyFile(const eastl::string_view _filePath)
    {
        const std::filesystem::path path(_filePath.data());

        if (!std::filesystem::exists(path))
        {
            return {};
        }

        const StringHash hash { path.lexically_normal().c_str() };
        Platform::ReadOnlyFileDescriptor* fileDescriptor = m_openFiles.Acquire(hash,
            [this](const bool _mustReclaim, const StringHash& _key, Platform::ReadOnlyFileDescriptor* _fileDescriptor)
            {
                if (_mustReclaim)
                {
                    KE_ASSERT(_fileDescriptor != nullptr);
                    Platform::CloseReadOnlyFile(*_fileDescriptor, m_allocator);
                }

                *_fileDescriptor = Platform::OpenReadOnlyFile(_key.m_string, m_allocator);
            });

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
}
