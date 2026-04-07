/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <cstdint>

namespace KryneEngine::Platform
{
    /**
     * @brief A utility handle struct that handles both an opaque pointer and an error state, packed into a single word
     */
    struct OpaqueHandle
    {
        enum class Error
        {
            Unknown = 0,
            NameInUse,
            NoHost,
            InvalidPath,
            AccessDenied,
            PathTooLong,
            SystemLimit,
        };

        void* m_handle = nullptr;

        explicit OpaqueHandle(void* _handle): m_handle(_handle) {}
        explicit OpaqueHandle(Error _error)
        {
            reinterpret_cast<uintptr_t&>(m_handle) = 1 | static_cast<uintptr_t>(_error) << 1;
        }

        [[nodiscard]] bool IsValid() const
        {
            return (reinterpret_cast<uintptr_t>(m_handle) & 1) == 0;
        }

        [[nodiscard]] Error GetError() const
        {
            return static_cast<Error>(reinterpret_cast<uintptr_t>(m_handle) >> 1);
        }
    };
}
