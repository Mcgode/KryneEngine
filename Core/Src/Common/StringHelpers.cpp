/**
 * @file
 * @author Max Godefroy
 * @date 04/01/2026.
 */

#include "KryneEngine/Core/Common/StringHelpers.hpp"

#include "KryneEngine/Core/Common/BitUtils.hpp"

namespace KryneEngine
{
    Utf8Iterator::Utf8Iterator(eastl::string_view _string)
        : m_currentPtr(_string.data())
    {
    }

    Utf8Iterator& Utf8Iterator::operator++()
    {
        if (m_byteCount == 0)
            ReadUtf8Char();
        m_currentPtr += m_byteCount;
        m_byteCount = 0;
        m_currentChar = 0;
        return *this;
    }

    bool Utf8Iterator::operator==(const char* iterator) const
    {
        return m_currentPtr == iterator;
    }

    u32 Utf8Iterator::operator*()
    {
        if (m_byteCount == 0)
            ReadUtf8Char();
        return m_currentChar;
    }

    void Utf8Iterator::ReadUtf8Char()
    {
        const u32 firstByte = static_cast<u8>(m_currentPtr[0]);

        if (firstByte >> 7 == 0)
        {
            m_byteCount = 1;
            m_currentChar = firstByte;
        }
        else if (firstByte >> 5 == 0b110)
        {
            m_byteCount = 2;
            const u32 nextByte = static_cast<u8>(m_currentPtr[1]);
            KE_ASSERT(nextByte >> 6 == 0b10);
            m_currentChar =
                (BitUtils::BitMask<u32>(5) & firstByte) << 6
                | (BitUtils::BitMask<u32>(6) & nextByte);
        }
        else if (firstByte >> 4 == 0b1110)
        {
            m_byteCount = 3;
            const u32 byte1 = static_cast<u8>(m_currentPtr[1]);
            const u32 byte2 = static_cast<u8>(m_currentPtr[2]);
            KE_ASSERT(byte1 >> 6 == 0b10 && byte2 >> 6 == 0b10);
            m_currentChar =
                (BitUtils::BitMask<u32>(4) & firstByte) << 12
                | (BitUtils::BitMask<u32>(6) & byte1) << 6
                | (BitUtils::BitMask<u32>(6) & byte2);
        }
        else if (firstByte >> 3 == 0b11110)
        {
            m_byteCount = 4;
            const u32 byte1 = static_cast<u8>(m_currentPtr[1]);
            const u32 byte2 = static_cast<u8>(m_currentPtr[2]);
            const u32 byte3 = static_cast<u8>(m_currentPtr[3]);
            KE_ASSERT(byte1 >> 6 == 0b10 && byte2 >> 6 == 0b10 && byte3 >> 6 == 0b10);
            m_currentChar =
                (BitUtils::BitMask<u32>(3) & firstByte) << 18
                | (BitUtils::BitMask<u32>(6) & byte1) << 12
                | (BitUtils::BitMask<u32>(6) & byte2) << 6
                | (BitUtils::BitMask<u32>(6) & byte3);
        }
    }

    Utf16Iterator::Utf16Iterator(const eastl::wstring_view _string)
        : m_currentPtr(_string.data())
    {}

    Utf16Iterator& Utf16Iterator::operator++()
    {
        if (m_charCount == 0)
            ReadUtf16Char();
        m_currentPtr += m_charCount;
        m_charCount = 0;
        m_currentChar = 0;
        return *this;
    }

    bool Utf16Iterator::operator==(const wchar_t* iterator) const
    {
        return m_currentPtr == iterator;
    }

    u32 Utf16Iterator::operator*()
    {
        if (m_charCount == 0)
            ReadUtf16Char();
        return m_currentChar;
    }

    void Utf16Iterator::ReadUtf16Char()
    {
        const u32 firstShort = static_cast<u16>(m_currentPtr[0]);

        if (firstShort >= 0xd800 && firstShort <= 0xdbff)
        {
            m_charCount = 2;
            const u32 secondShort = static_cast<u16>(m_currentPtr[1]);
            KE_ASSERT(secondShort >= 0xDC00u && secondShort <= 0xDFFFu);
            m_currentChar = 0x10000u
                + ((firstShort - 0xD800u) << 10)
                + (secondShort - 0xDC00u);
        }
        else
        {
            m_charCount = 1;
            m_currentChar = firstShort;
        }
    }

    void UnicodeWriters::WriteUtf8Char(eastl::string& _string, u32 _unicodeChar)
    {
        if (_unicodeChar < 1 << 7)
        {
            _string.push_back(static_cast<char>(_unicodeChar));
        }
        else if (_unicodeChar < (1 << 11))
        {
            _string.push_back(static_cast<char>(0b1100'0000) | (_unicodeChar >> 6));
            _string.push_back(static_cast<char>(0b1000'0000) | (_unicodeChar & 0b0011'1111));
        }
        else if (_unicodeChar < (1 << 16))
        {
            _string.push_back(static_cast<char>(0b1110'0000) | (_unicodeChar >> 12));
            _string.push_back(static_cast<char>(0b1000'0000) | ((_unicodeChar >> 6) & 0b0011'1111));
            _string.push_back(static_cast<char>(0b1000'0000) | (_unicodeChar & 0b0011'1111));
        }
        else
        {
            _string.push_back(static_cast<char>(0b1111'0000) | (_unicodeChar >> 18));
            _string.push_back(static_cast<char>(0b1000'0000) | ((_unicodeChar >> 12) & 0b0011'1111));
            _string.push_back(static_cast<char>(0b1000'0000) | ((_unicodeChar >> 6) & 0b0011'1111));
            _string.push_back(static_cast<char>(0b1000'0000) | (_unicodeChar & 0b0011'1111));
        }
    }

    void UnicodeWriters::WriteUtf16Char(eastl::wstring& _string, u32 _unicodeChar) {}
} // namespace KryneEngine