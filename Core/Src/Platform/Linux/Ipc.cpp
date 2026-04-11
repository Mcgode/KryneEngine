/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include "KryneEngine/Core/Platform/Platform.hpp"

namespace KryneEngine::Platform
{
    LocalIpcHost HostLocalIpc(
        const eastl::string_view _connectionName,
        const AllocatorInstance _allocator,
        const u32 _maxConnections)
    {
        return { OpaqueHandle { nullptr } };
    }

    void AcceptConnectionLocalIpc(const LocalIpcHost _connection, const u32 _clientIdx)
    {

    }

    size_t ReceiveLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer, const u32 _clientIdx)
    {
        return 0;
    }

    size_t SendLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer, const u32 _clientIdx)
    {
        return 0;
    }

    void CloseLocalIpc(const LocalIpcHost _connection)
    {

    }

    LocalIpcClient ConnectToLocalIpc(const eastl::string_view _connectionName, const AllocatorInstance _allocator)
    {
        return { OpaqueHandle { nullptr } };
    }

    size_t ReceiveLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer)
    {
        return 0;
    }

    size_t SendLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer)
    {
        return 0;
    }

    void CloseLocalIpcClient(const LocalIpcClient _connection, const AllocatorInstance _allocator)
    {

    }
}