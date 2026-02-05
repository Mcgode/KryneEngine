/**
 * @file
 * @author Max Godefroy
 * @date 05/03/2025.
 */

#pragma once

#include <EASTL/span.h>

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine::Hashing
{
    namespace Murmur2
    {
        static constexpr u64 kMurmurSeed = 0x9E37'79B9'7F4A'7C15ull;
        static constexpr u64 kMurmurPrime = 14'313'749'767'032'793'493u;
        static constexpr u64 kMurmurShift = 47u;

        // https://github.com/abrandoned/murmur2/blob/master/MurmurHash2.c
        constexpr u64 Murmur2Hash64(const char* _data, const u64 _size, u64 _base)
        {
            u64 hash = _base;

            IF_CONSTEVAL
            {
                const u64 blocks = _size >> 3;
                for (u32 i = 0; i < blocks; ++i)
                {
                    u64 block = static_cast<u64>(_data[8 * i + 0])
                        | static_cast<u64>(_data[8 * i + 1]) << 8
                        | static_cast<u64>(_data[8 * i + 2]) << 16
                        | static_cast<u64>(_data[8 * i + 3]) << 24
                        | static_cast<u64>(_data[8 * i + 4]) << 32
                        | static_cast<u64>(_data[8 * i + 5]) << 40
                        | static_cast<u64>(_data[8 * i + 6]) << 48
                        | static_cast<u64>(_data[8 * i + 7]) << 56;

                    block *= kMurmurPrime;
                    block ^= block >> kMurmurShift;
                    block *= kMurmurPrime;

                    hash ^= block;
                    hash *= kMurmurPrime;
                }
            }
            else
            {
                const auto* data = reinterpret_cast<const u64*>(_data);
                const auto* end = data + _size / 8;
                while (data != end)
                {
                    u64 k = *data++;

                    k *= kMurmurPrime;
                    k ^= k >> kMurmurShift;
                    k *= kMurmurPrime;

                    hash ^= k;
                    hash *= kMurmurPrime;
                }
            }

            const char* data = _data + ((_size >> 3) << 3);
            switch (_size & 7)
            {
            case 7: hash ^= static_cast<u64>(data[6]) << 48;
            case 6: hash ^= static_cast<u64>(data[5]) << 40;
            case 5: hash ^= static_cast<u64>(data[4]) << 32;
            case 4: hash ^= static_cast<u64>(data[3]) << 24;
            case 3: hash ^= static_cast<u64>(data[2]) << 16;
            case 2: hash ^= static_cast<u64>(data[1]) << 8;
            case 1: hash ^= static_cast<u64>(data[0]);
                hash *= kMurmurPrime;
            }

            hash ^= hash >> kMurmurShift;
            hash *= kMurmurPrime;
            hash ^= hash >> kMurmurShift;

            return hash;
        }

        constexpr u64 Murmur2Hash64(const char* _data, const u64 _size)
        {
            return Murmur2Hash64(_data, _size, kMurmurSeed ^ (_size * kMurmurPrime));
        }
    }

    constexpr u64 Hash64(const char* _data, size_t _size)
    {
        return Murmur2::Murmur2Hash64(_data, _size);
    }

    constexpr u64 Hash64Append(const char* _data, size_t _size, u64 _accumulatedHash)
    {
        return Murmur2::Murmur2Hash64(_data, _size, _accumulatedHash);
    }

    template <size_t N>
    constexpr u64 Hash64(const char (&_data)[N])
    {
        return Hash64(_data, N - 1);
    }

    template<typename T>
    u64 Hash64(const T* _value)
    {
        return Hash64(reinterpret_cast<const char*>(_value), sizeof(T));
    }

    template<typename T>
    u64 Hash64Append(const T* _value, u64 _accumulatedHash)
    {
        return Hash64Append(reinterpret_cast<const char*>(_value), sizeof(T), _accumulatedHash);
    }

    template<typename T>
    u64 Hash64(const T& _value)
    {
        return Hash64(&_value);
    }

    template<typename T>
    u64 Hash64Append(const T& _value, u64 _accumulatedHash)
    {
        return Hash64Append(&_value, _accumulatedHash);
    }

    template<typename T>
    u64 Hash64(const T* _data, size_t _count)
    {
        return Hash64(reinterpret_cast<const char*>(_data), _count * sizeof(T));
    }

    template<typename T>
    u64 Hash64Append(const T* _data, size_t _count, u64 _accumulatedHash)
    {
        return Hash64Append(reinterpret_cast<const char*>(_data), _count * sizeof(T), _accumulatedHash);
    }

    template<typename T>
    u64 Hash64(eastl::span<T> _span)
    {
        return Hash64(reinterpret_cast<const char*>(_span.data()), _span.size() * sizeof(T));
    }

    template<typename T>
    u64 Hash64Append(eastl::span<T> _span, u64 _accumulatedHash)
    {
        return Hash64Append(reinterpret_cast<const char*>(_span.data()), _span.size() * sizeof(T), _accumulatedHash);
    }
}