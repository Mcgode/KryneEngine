/**
 * @file
 * @author
 * @date 01/03/2026.
 */

#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Platform/Platform.hpp"
#include "KryneEngine/Core/Platform/Windows.h"

namespace KryneEngine::Platform
{
    KE_FORCEINLINE void SetupName(char str[256], const eastl::string_view _connectionName)
    {
        constexpr char prefix[] = "\\\\.\\pipe\\";
        KE_ASSERT(_connectionName.size() + sizeof(prefix) < sizeof(str) - 1);
        sprintf_s(str, sizeof(str), "%s%s", prefix, _connectionName.data());
    }

    struct WindowsHostLocalIpcConnection
    {
        u32 m_maxConnections;

        HANDLE& GetPipeHandle(const u32 _clientId)
        {
            return reinterpret_cast<HANDLE*>(this + 1)[_clientId];
        }
    };

    struct WindowsClientIpcConnection
    {
        HANDLE m_pipeHandle;
    };

    LocalIpcHost HostLocalIpc(
        const eastl::string_view _connectionName,
        const AllocatorInstance _allocator,
        const u32 _maxConnections)
    {
        void* buffer = _allocator.allocate(sizeof(WindowsHostLocalIpcConnection) + _maxConnections * sizeof(HANDLE), sizeof(size_t));
        new (buffer) WindowsHostLocalIpcConnection;
        auto* connection = static_cast<WindowsHostLocalIpcConnection*>(buffer);

        connection->m_maxConnections = _maxConnections;

        char name[256] = {};
        SetupName(name, _connectionName);

        // Check if the pipe name is used
        {
            HANDLE pipe = CreateFileA(
                name,
                GENERIC_READ,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

            if (pipe != INVALID_HANDLE_VALUE)
            {
                CloseHandle(pipe);
                return { OpaqueHandle { OpaqueHandle::Error::NameInUse } };
            }

            DWORD error = GetLastError();
            if (error == ERROR_PIPE_BUSY)
                return { OpaqueHandle { OpaqueHandle::Error::NameInUse } };
            else if (error != ERROR_FILE_NOT_FOUND)
            {
                KE_ERROR("Unhandled error: %d", error);
                return { OpaqueHandle { OpaqueHandle::Error::Unknown } };
            }
        }

        // Create the Named Pipe
        for (auto i = 0u; i < _maxConnections; i++)
        {
            connection->GetPipeHandle(i) = CreateNamedPipeA(
                name,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                4096,
                4096,
                0,
                NULL);

            KE_ASSERT_MSG(connection->GetPipeHandle(i) != INVALID_HANDLE_VALUE, "Failed to create named pipe");
        }

        return { OpaqueHandle { connection } };
    }

    void AcceptConnectionLocalIpc(const LocalIpcHost _connection, const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* connection = static_cast<WindowsHostLocalIpcConnection*>(_connection.m_handle);

        BOOL connected = ConnectNamedPipe(connection->GetPipeHandle(_clientIdx), NULL);
        KE_ASSERT_MSG(connected || GetLastError() == ERROR_PIPE_CONNECTED, "Failed to accept pipe connection");
    }

    size_t ReceiveLocalIpc(
        const LocalIpcHost _connection,
        const eastl::span<char> _buffer,
        const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        const auto* connection = static_cast<WindowsHostLocalIpcConnection*>(_connection.m_handle);

        DWORD bytesRead = 0;
        const BOOL success = ReadFile(
            connection->GetPipeHandle(_clientIdx),
            _buffer.data(),
            static_cast<DWORD>(_buffer.size()),
            &bytesRead,
            NULL);

        KE_ASSERT_MSG(success || GetLastError() == ERROR_BROKEN_PIPE, "Failed to read from pipe");
        return success ? bytesRead : 0;
    }

    size_t SendLocalIpc(
        const LocalIpcHost _connection,
        const eastl::span<char> _buffer,
        const u32 _clientIdx)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* connection = static_cast<WindowsHostLocalIpcConnection*>(_connection.m_handle);

        DWORD bytesWritten = 0;
        BOOL success = WriteFile(
            connection->GetPipeHandle(_clientIdx),
            _buffer.data(),
            static_cast<DWORD>(_buffer.size()),
            &bytesWritten,
            NULL);

        KE_ASSERT_MSG(success || GetLastError() == ERROR_BROKEN_PIPE, "Failed to write to pipe");
        return success ? bytesWritten : 0;
    }

    void CloseLocalIpcHost(const LocalIpcHost _connection, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* connection = static_cast<WindowsHostLocalIpcConnection*>(_connection.m_handle);

        for (auto i = 0u; i < connection->m_maxConnections; i++)
        {
            DisconnectNamedPipe(connection->GetPipeHandle(i));
            CloseHandle(connection->GetPipeHandle(i));
        }
        _allocator.Delete(connection);
    }

    LocalIpcClient ConnectToLocalIpc(
        const eastl::string_view _connectionName,
        const AllocatorInstance _allocator)
    {
        auto* clientConnection = _allocator.New<WindowsClientIpcConnection>();

        char pipeName[256] = {};
        SetupName(pipeName, _connectionName);

        // Try to open client-side connection to named pipe
        clientConnection->m_pipeHandle = CreateFileA(
            pipeName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (clientConnection->m_pipeHandle == INVALID_HANDLE_VALUE)
        {
            _allocator.Delete(clientConnection);
            return { OpaqueHandle { OpaqueHandle::Error::NoHost } };
        }

        return { OpaqueHandle { clientConnection } };
    }

    size_t ReceiveLocalIpc(const LocalIpcClient _connection, const eastl::span<char> _buffer)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        const auto* clientConnection = static_cast<WindowsClientIpcConnection*>(_connection.m_handle);

        DWORD bytesRead = 0;
        BOOL success = ReadFile(
            clientConnection->m_pipeHandle,
            _buffer.data(),
            static_cast<DWORD>(_buffer.size()),
            &bytesRead,
            NULL);

        KE_ASSERT_MSG(success || GetLastError() == ERROR_BROKEN_PIPE, "Failed to read from pipe");
        return success ? bytesRead : 0;
    }

    size_t SendLocalIpc(const LocalIpcClient _connection, const eastl::span<char> _buffer)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        const auto* clientConnection = static_cast<WindowsClientIpcConnection*>(_connection.m_handle);

        DWORD bytesWritten = 0;
        BOOL success = WriteFile(
            clientConnection->m_pipeHandle,
            _buffer.data(),
            static_cast<DWORD>(_buffer.size()),
            &bytesWritten,
            NULL);

        KE_ASSERT_MSG(success || GetLastError() == ERROR_BROKEN_PIPE, "Failed to write to pipe");
        return success ? bytesWritten : 0;
    }

    void CloseLocalIpcClient(const LocalIpcClient _connection, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_connection.m_handle != nullptr);
        auto* clientConnection = static_cast<WindowsClientIpcConnection*>(_connection.m_handle);

        CloseHandle(clientConnection->m_pipeHandle);
        _allocator.Delete(clientConnection);
    }

}