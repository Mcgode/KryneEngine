/**
 * @file
 * @author Max Godefroy
 * @date 10/02/2025.
 */

#include "KryneEngine/Core/Common/BitUtils.hpp"

namespace
{
    using namespace KryneEngine;

    u8 ComputeMsb(u64 _value)
    {
        u8 msb = 0;
        if (_value >= 1ull << 32)
        {
            msb += 32;
            _value >>= 32;
        }
        if (_value >= 1ull << 16)
        {
            msb += 16;
            _value >>= 16;
        }
        if (_value >= 1ull << 8)
        {
            msb += 8;
            _value >>= 8;
        }
        if (_value >= 1ull << 4)
        {
            msb += 4;
            _value >>= 4;
        }
        if (_value >= 1ull << 2)
        {
            msb += 2;
            _value >>= 2;
        }
        if (_value >= 1ull << 1)
        {
            msb += 1;
        }
        return msb;
    }
}

namespace KryneEngine::BitUtils
{
    u8 GetMostSignificantBit(KryneEngine::u64 _value)
    {
#if defined(__has_builtin)
#   if __has_builtin(__builtin_clzll)
        return 63 - __builtin_clzll(_value);
#   else
        return ComputeMsb(_value);
#   endif
#else
        return ComputeMsb(_value);
#endif
    }

    u8 GetLeastSignificantBit(KryneEngine::u64 _value)
    {
        [[maybe_unused]] constexpr auto computeLsb = [](u64 _value) {
            const u64 isolatedLsb = _value & -_value;
            return ComputeMsb(isolatedLsb);
        };
#if defined(__has_builtin) && __has_builtin(__builtin_ctzll)
        return __builtin_ctzll(_value);
#else
        return computeLsb(_value);
#endif
    }
}