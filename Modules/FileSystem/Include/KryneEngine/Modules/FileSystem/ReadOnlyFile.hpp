/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <KryneEngine/Core/Platform/FileSystem.hpp>

#include "KryneEngine/Modules/FileSystem/Flags.hpp"

namespace KryneEngine::Modules::FileSystem
{
    class VirtualFileSystem;

    /**
     * @brief Represents a read-only file within a virtual file system.
     *
     * @details
     * This abstraction is used to represent both full-fledged files and spans of a larger file (like an archive).
     * It provides a unified interface for reading data from these different types of files, abstracting away the
     * underlying implementation details.
     *
     * It is entirely thread-safe to use as long as the file is not written to at the same time.
     */
    class ReadOnlyFile
    {
        friend VirtualFileSystem;

    public:
        ReadOnlyFile() = default;

        ~ReadOnlyFile();

        [[nodiscard]] bool IsValid() const { return m_fileDescriptor != nullptr; }

        [[nodiscard]] size_t GetSize() const { return m_size; }

        [[nodiscard]] const FileFlags& GetFlags() const { return m_flags; }

        size_t Read(size_t _offset, eastl::span<std::byte> _buffer) const;

        template <class T>
        size_t ReadT(const size_t _offset, T* _ptr, const size_t _count = 1) const
        {
            return Read(_offset, { reinterpret_cast<std::byte*>(_ptr), _count * sizeof(T) });
        }

    private:
        VirtualFileSystem* m_fileSystem = nullptr;
        Platform::ReadOnlyFileDescriptor* m_fileDescriptor = nullptr;
        size_t m_baseOffset {};
        size_t m_size {};
        FileFlags m_flags {};

        ReadOnlyFile(
            VirtualFileSystem* _fileSystem,
            Platform::ReadOnlyFileDescriptor* _fileDescriptor,
            size_t _baseOffset,
            size_t _size,
            FileFlags _flags);
    };
}
