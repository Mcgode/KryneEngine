/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Math/Simd/SimdCommon.hpp"

#if defined(__ARM_NEON)
#   include <arm_neon.h>
#elif defined(__SSE2__)
#   include <xmmintrin.h>
#   include <EASTL/array.h>
#else
#   include <EASTL/array.h>
#   include "KryneEngine/Core/Common/Types.hpp"
#endif

namespace KryneEngine::Simd
{
    using f32x4 =
#if defined(__ARM_NEON)
        float32x4_t
#elif defined(__SSE2__)
        __m128
#else
        struct { alignas(16) eastl::array<float, 4> m_value; }
#endif
    ;

    using u32x4 =
#if defined(__ARM_NEON)
        uint32x4_t
#elif defined(__SSE2__)
        __m128i
#else
        struct { alignas(16) eastl::array<u32, 4> m_value; }
#endif
    ;

    using s32x4 =
#if defined(__ARM_NEON)
        int32x4_t
#elif defined(__SSE2__)
        __m128i
#else
        struct { alignas(16) eastl::array<s32, 4> m_value; }
#endif
    ;

    using f32x4x3 =
#if defined(__ARM_NEON)
        float32x4x3_t
#else
        eastl::array<f32x4, 3>
#endif
    ;

    using f32x4x4 =
#if defined(__ARM_NEON)
        float32x4x4_t
#else
        eastl::array<f32x4, 4>
#endif
    ;

    using u8x16 =
#if defined(__ARM_NEON)
        uint8x16_t
#elif defined(__SSE2__)
        __m128i
#else
        eastl::array<u8, 16>
#endif
    ;
}