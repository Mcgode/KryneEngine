/**
 * @file
 * @author Max Godefroy
 * @date 02/04/2022.
 */

#pragma once

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <string>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Hashing.hpp"

namespace KryneEngine
{
    struct StringHash
    {
        explicit StringHash(u64 _value)
            : m_hash(_value)
        {}

        explicit StringHash(const eastl::string_view& _string, const AllocatorInstance _allocator = {})
            : m_hash(Hash64(_string))
            , m_string(_string, _allocator)
        {}

        StringHash(const StringHash& _other) = default;
        StringHash(StringHash&& _other) noexcept = default;

        StringHash& operator=(const StringHash& _other) = default;
        StringHash& operator=(StringHash&& _other) noexcept = default;

        u64 m_hash;
        eastl::string m_string {};

        static u64 Hash64(const eastl::string_view& _string)
        {
            return Hashing::Hash64(_string.data(), _string.size());
        }

        bool operator==(const StringHash &rhs) const
        {
            return m_hash == rhs.m_hash;
        }

        bool operator<(const StringHash &rhs) const
        {
            return m_hash < rhs.m_hash;
        }
    };

    struct Utf8Iterator
    {
        explicit Utf8Iterator(eastl::string_view _string);

        Utf8Iterator& operator++();
        bool operator==(const char* iterator) const;
        u32 operator*();

    private:
        void ReadUtf8Char();

        u32 m_currentChar = 0;
        u32 m_byteCount = 0;
        const char* m_currentPtr = nullptr;
    };

    namespace StringHelpers
    {
        template <class Container, bool Reserve = true>
        eastl::vector<const char*> RetrieveStringPointerContainer(const Container& _container)
        {
            static_assert(
                    std::is_same<typename Container::value_type, eastl::string>::value ||
                    std::is_same<typename Container::value_type, std::string>::value,
                    "Container value type should be a string"
            );
            eastl::vector<const char*> result;

            if (Reserve)
            {
                result.reserve(_container.size());
            }

            for (const auto& str: _container)
            {
                result.push_back(str.c_str());
            }

            return result;
        }
    }
}

namespace eastl
{
    template <>
    struct hash<KryneEngine::StringHash>
    {
        size_t operator()(const KryneEngine::StringHash& _val) const { return static_cast<size_t>(_val.m_hash); }
    };
}