/**
 * @file
 * @author Max Godefroy
 * @date 12/01/2026.
 */

#pragma once

#include <EASTL/span.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine::Platform
{
    /**
     * @defgroup IPC methods
     * @{
     */

    /**
     * @brief Opaque handle that represents a local inter-process communication (IPC) connection as a host.
     */
    struct LocalIpcHost
    {
        void* m_handle = nullptr;
    };

    /**
     * @brief Opaque handle that represents a local inter-process communication (IPC) connection as a client.
     */
    struct LocalIpcClient
    {
        void* m_handle = nullptr;
    };

    /**
     * @brief Hosts a local IPC connection for communication between processes and primes it for receiving connections.
     *
     * @param _connectionName The unique name of the connection to host.
     * @param _allocator The allocator to use for memory management.
     * @param _maxConnections The maximum number of connections that can be accepted by the host.
     * @return An opaque handle for referencing the server.
     */
    LocalIpcHost HostLocalIpc(eastl::string_view _connectionName, AllocatorInstance _allocator, u32 _maxConnections = 1);

    /**
     * @brief A blocking call to wait for the next connection attempt.
     *
     * @param _connection The connection to accept a client on.
     * @param _clientIdx The index of the client to accept.
     */
    void AcceptConnectionLocalIpc(LocalIpcHost _connection, u32 _clientIdx = 0);


    /**
     * @brief Receives data from a local IPC connection.
     *
     * This function reads data from a specified local IPC connection, targeting
     * a client index, and stores it in the provided buffer.
     *
     * @param _connection The local IPC connection handle.
     * @param _buffer A writable buffer for storing the received data.
     * @param _clientIdx The index of the client within the local IPC connection.
     *
     * @return The size of data read into the buffer, in bytes.
     *         If value is 0, the connection was closed.
     */
    size_t ReceiveLocalIpc(LocalIpcHost _connection, eastl::span<char> _buffer, u32 _clientIdx = 0);

    /**
     * Sends data to a specific client via a local IPC connection.
     *
     * @param _connection The local IPC host connection to use for sending data.
     * @param _buffer The buffer containing the data to be sent.
     * @param _clientIdx The index of the target client within the local IPC host connection.
     * @return The number of bytes successfully sent, or 0 if the connection is closed.
     */
    size_t SendLocalIpc(LocalIpcHost _connection, eastl::span<char> _buffer, u32 _clientIdx = 0);

    /**
     * @brief Closes an existing local IPC host.
     *
     * @details
     * This function safely closes a local inter-process communication (IPC) connection, performs cleanup operations,
     * and deallocates any resources tied to the connection.
     *
     * @param _connection Specifies the local IPC connection to close. This connection should have a valid handle.
     * @param _allocator Specifies the allocator used to deallocate resources associated with the connection.
     */
    void CloseLocalIpcHost(LocalIpcHost _connection, AllocatorInstance _allocator);

    /**
     * @brief Establishes a connection to a local inter-process communication (IPC) endpoint.
     *
     * @param _connectionName The name of the IPC endpoint to connect to.
     * @param _allocator The allocator instance used to create and manage the connection.
     * @return A `LocalIpcClient` object representing the established connection. If the connection
     *         fails, an empty `LocalIpcClient` is returned.
     */
    LocalIpcClient ConnectToLocalIpc(eastl::string_view _connectionName, AllocatorInstance _allocator);

    /**
     * Receives data from the server via a local IPC connection.
     *
     * @param _connection Represents the local IPC client from which the data is to be received.
     * @param _buffer A buffer to store the data received from the IPC client. The buffer's size
     *        determines the maximum amount of data that can be received in one go.
     * @return The number of bytes successfully received into the provided buffer.
     *         Returns 0 if the connection is closed.
     */
    size_t ReceiveLocalIpc(LocalIpcClient _connection, eastl::span<char> _buffer);

    /**
     * Sends data to the server via a local IPC connection.
     *
     * @param _connection The local IPC client connection through which the data will be sent.
     * @param _buffer A span containing the data to be sent.
     * @return The number of bytes successfully sent. Returns 0 if the connection is closed.
     */
    size_t SendLocalIpc(LocalIpcClient _connection, eastl::span<char> _buffer);

    /**
     * @brief Closes a local IPC client connection and deallocates its resources.
     *
     * @details
     * This function ensures that the IPC client connection is properly closed, its socket is released,
     * and the memory associated with the connection is deallocated using the specified allocator.
     *
     * @param _connection The local IPC client to be closed.
     * @param _allocator The memory allocator instance responsible for deallocating the resources
     *        associated with the IPC client connection.
     */
    void CloseLocalIpcClient(LocalIpcClient _connection, AllocatorInstance _allocator);

    /**
     * @}
     */

    /**
     * @defgroup Font methods
     * @{
     */

    struct FontMetrics
    {
        double m_ascender;
        double m_descender;
        double m_lineHeight;
        double m_unitPerEm;
    };

    struct GlyphMetrics
    {
        double4 m_bounds {};
        double m_advance {};
    };

    using FontGlyphMetricsFunction = void (*)(const FontMetrics&, const GlyphMetrics&, void*);
    using FontNewContourFunction = void (*)(const double2&, void*);
    using FontNewEdgeFunction = void (*)(const double2&, void*);
    using FontNewConicFunction = void (*)(const double2&, const double2&, void*);
    using FontNewCubicFunction = void (*)(const double2&, const double2&, const double2&, void*);
    using FontEndContourFunction = void (*)(void*);

    /**
     * @brief A function that retrieves glyph data from the system default font.
     *
     * @returns `true` if the glyph was successfully retrieved, `false` otherwise.
     */
    bool RetrieveSystemDefaultGlyph(
        u32 _unicodeCodePoint,
        void* _userData,
        FontGlyphMetricsFunction _fontMetrics,
        FontNewContourFunction _newContour,
        FontNewEdgeFunction _newEdge,
        FontNewConicFunction _newConic,
        FontNewCubicFunction _newCubic,
        FontEndContourFunction _endContour,
        bool _verticalLayout = false);

    /**
     * @}
     */
}