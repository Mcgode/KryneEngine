/**
 * @file
 * @author Max Godefroy
 * @date 16/01/2026.
 */

#pragma once


#include <KryneEngine/Core/Math/Hashing.hpp>

namespace KryneEngine::Modules::Resources
{
    using ResourceTypeId = u64;

    constexpr ResourceTypeId GenerateResourceTypeId(const char* _name, size_t _size)
    {
        return Hashing::Murmur2::Murmur2Hash64(_name, _size);
    }

    template <size_t N>
    constexpr ResourceTypeId GenerateResourceTypeId(const char (&_name)[N])
    {
        constexpr size_t len = (N == 0 ? 0 : N - 1); // N is >=1 for string literal
        return GenerateResourceTypeId(_name, len);
    }
}