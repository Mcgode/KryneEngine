/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include "KryneEngine/Core/Platform/Platform.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace KryneEngine::Platform
{
    KE_FORCEINLINE void SetupName(sockaddr_un& _address, const eastl::string_view _connectionName)
    {
        _address.sun_family = AF_UNIX;
        constexpr char prefix[] = "/tmp/ke/";
        KE_ASSERT_MSG(_connectionName.size() + sizeof(prefix) < sizeof(_address.sun_path) - 1, "Connection name too long");
        snprintf(
            _address.sun_path,
            sizeof(_address.sun_path),
            "%s%s",
            prefix,
            _connectionName.data());
    }

    struct LinuxHostLocalIpcConnection
    {
        s32 m_socketId {};
        u32 m_maxConnections {};
        sockaddr_un m_address {};

        struct Client
        {
            s32 m_clientId {};
        };

        Client* GetClients()
        {
            return reinterpret_cast<Client*>(this + 1);
        }

        Client* GetClient(const u32 _index)
        {
            return GetClients() + _index;
        }
    };

    LocalIpcHost HostLocalIpc(
        const eastl::string_view _connectionName,
        const AllocatorInstance _allocator,
        const u32 _maxConnections)
    {
        const size_t allocSize = sizeof(LinuxHostLocalIpcConnection) + _maxConnections * sizeof(LinuxHostLocalIpcConnection::Client);
        void* buffer = _allocator.allocate(allocSize, sizeof(size_t));
        new (buffer) LinuxHostLocalIpcConnection;
        auto* ipcConnection = static_cast<LinuxHostLocalIpcConnection*>(buffer);

        ipcConnection->m_maxConnections = _maxConnections;

        ipcConnection->m_socketId = socket(AF_UNIX, SOCK_STREAM, 0);
        KE_ASSERT_MSG(ipcConnection->m_socketId != -1, "Failed to create socket");

        SetupName(ipcConnection->m_address, _connectionName);

        // If there is an existing socket file, assume it is stale and try to remove it
        if (access(ipcConnection->m_address.sun_path, F_OK) == 0)
        {
            if (unlink(ipcConnection->m_address.sun_path) == -1)
            {
                _allocator.deallocate(buffer, allocSize);
                switch (errno)
                {
                case EBUSY:
                    return { OpaqueHandle { OpaqueHandle::Error::NameInUse } };
                default:
                    KE_ERROR("Unhandled error: %d", errno);
                    return { OpaqueHandle { OpaqueHandle::Error::Unknown } };
                }
            }
        }

        KE_VERIFY(bind(
            ipcConnection->m_socketId,
            reinterpret_cast<sockaddr*>(&ipcConnection->m_address),
            sizeof(ipcConnection->m_address) != -1));

        KE_VERIFY(listen(ipcConnection->m_socketId, static_cast<s32>(_maxConnections)) != -1);

        return { OpaqueHandle { ipcConnection } };
    }

    void AcceptConnectionLocalIpc(const LocalIpcHost _connection, const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* ipcConnection = static_cast<LinuxHostLocalIpcConnection*>(_connection.m_handle);

        KE_ASSERT(ipcConnection->m_maxConnections > _clientIdx);

        LinuxHostLocalIpcConnection::Client* client = ipcConnection->GetClient(_clientIdx);

        client->m_clientId = accept(ipcConnection->m_socketId, nullptr, nullptr);
        KE_ASSERT_MSG(client->m_clientId != -1, "Failed to accept connection");
    }

    size_t ReceiveLocalIpc(const LocalIpcHost _connection, const eastl::span<char> _buffer, const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* ipcConnection = static_cast<LinuxHostLocalIpcConnection*>(_connection.m_handle);

        KE_ASSERT(ipcConnection->m_maxConnections > _clientIdx);
        const LinuxHostLocalIpcConnection::Client* client = ipcConnection->GetClient(_clientIdx);

        return read(client->m_clientId, _buffer.data(), _buffer.size());
    }

    size_t SendLocalIpc(const LocalIpcHost _connection, const eastl::span<char> _buffer, const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* ipcConnection = static_cast<LinuxHostLocalIpcConnection*>(_connection.m_handle);

        KE_ASSERT(ipcConnection->m_maxConnections > _clientIdx);
        const LinuxHostLocalIpcConnection::Client* client = ipcConnection->GetClient(_clientIdx);

        const ssize_t result = send(client->m_clientId, _buffer.data(), _buffer.size(), MSG_NOSIGNAL);

        KE_ASSERT(result != -1 || errno == EPIPE);

        return result > 0 ? result : 0;
    }

    void CloseLocalIpc(const LocalIpcHost _connection, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_connection.m_handle != nullptr);

        auto* ipcConnection = static_cast<LinuxHostLocalIpcConnection*>(_connection.m_handle);
        KE_VERIFY(unlink(ipcConnection->m_address.sun_path) != -1);
        KE_VERIFY(close(ipcConnection->m_socketId) != -1);

        _allocator.Delete(ipcConnection);
    }

    struct LinuxClientIpcConnection
    {
        s32 m_socketId {};
        s32 m_serverId {};
    };

    LocalIpcClient ConnectToLocalIpc(const eastl::string_view _connectionName, const AllocatorInstance _allocator)
    {
        auto* clientConnection = _allocator.New<LinuxClientIpcConnection>();

        sockaddr_un address {};
        SetupName(address, _connectionName);
        clientConnection->m_serverId = connect(clientConnection->m_socketId, reinterpret_cast<sockaddr*>(&address), sizeof(address));

        if (clientConnection->m_serverId == -1)
        {
            _allocator.Delete(clientConnection);
            return { OpaqueHandle { OpaqueHandle::Error::NoHost } };
        }

        return { OpaqueHandle { clientConnection } };
    }

    size_t ReceiveLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        const auto* clientConnection = static_cast<LinuxClientIpcConnection*>(_connection.m_handle);

        return recv(clientConnection->m_serverId, _buffer.data(), _buffer.size(), 0);
    }

    size_t SendLocalIpc(const LocalIpcClient _connection, eastl::span<char> _buffer)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        const auto* clientConnection = static_cast<LinuxClientIpcConnection*>(_connection.m_handle);

        const ssize_t result = send(clientConnection->m_serverId, _buffer.data(), _buffer.size(), MSG_NOSIGNAL);
        KE_ASSERT(result != -1 || errno == EPIPE);

        return result > 0 ? result : 0;
    }

    void CloseLocalIpcClient(const LocalIpcClient _connection, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* clientConnection = static_cast<LinuxClientIpcConnection*>(_connection.m_handle);
        KE_VERIFY(close(clientConnection->m_socketId) != -1);
        _allocator.Delete(clientConnection);
    }
}