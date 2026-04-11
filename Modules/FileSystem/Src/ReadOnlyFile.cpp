/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp"

#include <KryneEngine/Core/Memory/Containers/LruCache.inl>

#include "KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp"

namespace KryneEngine::Modules::FileSystem
{
    ReadOnlyFile::~ReadOnlyFile()
    {
        m_fileSystem->m_openFiles.Release(m_fileDescriptor);
    }

    size_t ReadOnlyFile::Read(const size_t _offset, const eastl::span<std::byte> _buffer) const
    {
        KE_ASSERT(_offset < m_size);

        if (_buffer.empty()) [[unlikely]]
            return 0;

        return Platform::ReadFile(
            *m_fileDescriptor,
            _offset + m_baseOffset,
            { _buffer.data(), eastl::min(_buffer.size(), m_size - _offset) });
    }

    ReadOnlyFile::ReadOnlyFile(
        VirtualFileSystem* _fileSystem,
        Platform::ReadOnlyFileDescriptor* _fileDescriptor,
        const size_t _baseOffset,
        const size_t _size,
        const FileFlags _flags)
            : m_fileSystem(_fileSystem)
            , m_fileDescriptor(_fileDescriptor)
            , m_baseOffset(_baseOffset)
            , m_size(_size)
            , m_flags(_flags)
    {}
}
