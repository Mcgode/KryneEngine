/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Math/Simd/SimdTypes.hpp"

namespace KryneEngine::Simd
{
    static constexpr size_t kCompareEqMaskElementWidthPot =
#if defined(__ARM_NEON)
        2;
#else
        0;
#endif

    KE_FORCEINLINE u64 CompareEqMask(u8x16 a, u8x16 b)
    {
#if defined(__ARM_NEON)
        // See https://developer.arm.com/community/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
        const uint8x16_t baseMask = vceqq_u8(a, b);
        const uint8x8_t compactedMask = vshrn_n_u16(vreinterpretq_u16_u8(baseMask), 4);
        return vget_lane_u64(vreinterpret_u64_u8(compactedMask), 0);
#elif defined (__SSE2__)
        return _mm_movemask_epi8(_mm_cmpeq_epi8(a, b));
#else
        u64 result = 0;
        for (u32 i = 0; i < 16; ++i)
        {
            if (a[i] == b[i])
                result |= (1ull << i);
        }
        return result;
#endif
    }
}