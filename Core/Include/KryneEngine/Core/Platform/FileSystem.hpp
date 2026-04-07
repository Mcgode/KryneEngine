/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <filesystem>
#include <EASTL/span.h>
#include <EASTL/string_view.h>

#include "KryneEngine/Core/Platform/Helpers.hpp"

namespace KryneEngine::Platform
{
    struct DirectoryMonitorHandle: OpaqueHandle {};

    struct DirectoryMonitorCreateInfo
    {
        eastl::span<eastl::string_view> m_directories;
        eastl::string_view m_threadName;

        eastl::function<void(eastl::string_view)> m_fileCreatedCallback;
        eastl::function<void(eastl::string_view)> m_fileModifiedCallback;
        eastl::function<void(eastl::string_view, eastl::string_view)> m_fileRenamedCallback;
        eastl::function<void(eastl::string_view)> m_fileDeletedCallback;
    };

    [[nodiscard]] DirectoryMonitorHandle CreateDirectoryMonitor(
        const DirectoryMonitorCreateInfo& _info,
        AllocatorInstance _allocator);

    void DestroyDirectoryMonitor(DirectoryMonitorHandle _handle, AllocatorInstance _allocator);

    std::filesystem::path GetDefaultConfigDirectory(
        eastl::string_view _appName,
        bool _systemConfig = false);


    /**
     * @brief Opaque handle to a read-only file descriptor that can be read from concurrently.
     */
    struct ReadOnlyFileDescriptor: OpaqueHandle {};

    [[nodiscard]] ReadOnlyFileDescriptor OpenReadOnlyFile(eastl::string_view _path, AllocatorInstance _allocator);

    [[nodiscard]] size_t GetFileSize(ReadOnlyFileDescriptor _fd);
    [[nodiscard]] size_t ReadFile(ReadOnlyFileDescriptor _fd, size_t _position, eastl::span<std::byte> _dstBuffer);

    void CloseReadOnlyFile(ReadOnlyFileDescriptor _fd, AllocatorInstance _allocator);
}
