/**
 * @file
 * @author Max Godefroy
 * @date 29/07/2024.
 */

#pragma once

#include "KryneEngine/Core/Common/BitUtils.hpp"
#include "KryneEngine/Core/Common/Utils/Macros.hpp"

namespace KryneEngine::Alignment
{
    template <class T>
    inline constexpr bool IsAligned(T _value, T _alignment)
    {
        return (_alignment != 0) && (_value % _alignment) == 0;
    }

    template <class T>
    inline constexpr T AlignUp(T _value, T _alignment)
    {
        return (_alignment != 0) ? ((_value + _alignment - 1) / _alignment) * _alignment : 0;
    }

    template <class T>
    inline constexpr T AlignUpPot(T _value, u8 _pot)
    {
        return ((_value + BitUtils::BitMask<T>(_pot)) >> _pot) << _pot;
    }

    template <class T>
    inline constexpr T AlignDown(T _value, T _alignment)
    {
        return (_alignment != 0) ? ((_value) / _alignment) * _alignment : 0;
    }

    template <class T>
    inline constexpr T AlignDownPot(T _value, u8 _pot)
    {
        return _value & ~BitUtils::BitMask<T>(_pot);
    }

    constexpr inline u64 NextPowerOfTwo(u64 _value)
    {
        _value--;
        _value |= _value >> 1;
        _value |= _value >> 2;
        _value |= _value >> 4;
        _value |= _value >> 8;
        _value |= _value >> 16;
        _value |= _value >> 32;
        return ++_value;
    }

    constexpr KE_FORCEINLINE bool IsPowerOfTwo(u64 _value)
    {
        return _value && !(_value & (_value - 1));
    }
}
