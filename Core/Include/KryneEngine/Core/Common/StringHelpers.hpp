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
#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Math/Hashing.hpp"

namespace KryneEngine
{
    struct StringHashBase
    {
        explicit StringHashBase(u64 _value)
            : m_hash(_value)
        {}

        KE_DEFINE_COPY_MOVE_SEMANTICS(StringHashBase, default, default);

        u64 m_hash;

        static u64 Hash64(const eastl::string_view& _string)
        {
            return Hashing::Hash64(_string.data(), _string.size());
        }

        bool operator==(const StringHashBase &rhs) const
        {
            return m_hash == rhs.m_hash;
        }

        bool operator<(const StringHashBase &rhs) const
        {
            return m_hash < rhs.m_hash;
        }
    };

    struct StringHash: StringHashBase
    {
        StringHash(const u64 _hash, const eastl::string_view& _string, const AllocatorInstance _allocator)
            : StringHashBase(_hash)
            , m_string(_string, _allocator)
        {}

        explicit StringHash(const eastl::string_view& _string, const AllocatorInstance _allocator = {})
            : StringHash(Hash64(_string), _string, _allocator)
        {}

        KE_DEFINE_COPY_MOVE_SEMANTICS(StringHash, default, default);

        eastl::string m_string {};
    };

    struct StringViewHash: StringHashBase
    {
        StringViewHash(const u64 _hash, const eastl::string_view& _stringView)
            : StringHashBase(_hash)
            , m_stringView(_stringView)
        {}

        explicit StringViewHash(const eastl::string_view& _string): StringViewHash(Hash64(_string), _string) {}
        explicit StringViewHash(const StringHash& _stringHash): StringViewHash(_stringHash.m_hash, _stringHash.m_string) {}

        KE_DEFINE_COPY_MOVE_SEMANTICS(StringViewHash, default, default);

        eastl::string_view m_stringView {};
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

    struct Utf16Iterator
    {
        explicit Utf16Iterator(eastl::wstring_view _string);

        Utf16Iterator& operator++();
        bool operator==(const wchar_t* iterator) const;
        u32 operator*();

    private:
        void ReadUtf16Char();

        u32 m_currentChar = 0;
        u32 m_charCount = 0;
        const wchar_t* m_currentPtr = nullptr;
    };

    namespace UnicodeWriters
    {
        void WriteUtf8Char(eastl::string& _string, u32 _unicodeChar);
        void WriteUtf16Char(eastl::wstring& _string, u32 _unicodeChar);
    }

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

    namespace Hashing
    {
        template <>
        inline size_t HashKey<StringHashBase>(const StringHashBase& _val) { return static_cast<size_t>(_val.m_hash); }

        template <>
        inline size_t HashKey<StringHash>(const StringHash& _val) { return static_cast<size_t>(_val.m_hash); }

        template <>
        inline size_t HashKey<StringViewHash>(const StringViewHash& _val) { return static_cast<size_t>(_val.m_hash); }
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