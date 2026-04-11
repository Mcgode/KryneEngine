/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include "KryneEngine/Core/Platform/FileSystem.hpp"

namespace KryneEngine::Platform
{
    DirectoryMonitorHandle CreateDirectoryMonitor(
        const DirectoryMonitorCreateInfo& _info,
        const AllocatorInstance _allocator)
    {
        return { OpaqueHandle { nullptr } };
    }

    void DestroyDirectoryMonitor(const DirectoryMonitorHandle _handle, const AllocatorInstance _allocator)
    {

    }

    std::filesystem::path GetDefaultConfigDirectory(
        const eastl::string_view _appName,
        const bool _systemConfig)
    {
        return {};
    }

    ReadOnlyFileDescriptor OpenReadOnlyFile(const eastl::string_view _path, const AllocatorInstance _allocator)
    {
        return { OpaqueHandle { nullptr } };
    }

    size_t GetFileSize(const ReadOnlyFileDescriptor _fd)
    {
        return 0;
    }

    size_t ReadFile(const ReadOnlyFileDescriptor _fd, const size_t _position, const eastl::span<std::byte> _dstBuffer)
    {
        return 0;
    }

    void CloseReadOnlyFile(const ReadOnlyFileDescriptor _fd, const AllocatorInstance _allocator)
    {

    }
}