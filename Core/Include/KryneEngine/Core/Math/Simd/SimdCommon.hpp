/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/BitUtils.hpp"

#if defined(__ARM_NEON)
#   define KE_SIMD_AVAILABLE 1
#elif defined(__x86_64__)
#   define KE_SIMD_AVAILABLE 1

#   define _mm_swizzle(vec, idx) _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(idx, idx, idx, idx))
#   define _mm_swizzle4(vec, i0, i1, i2, i3) _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(i0, i1, i2, i3))
#endif

#if !defined(KE_SIMD_AVAILABLE)
#   define KE_SIMD_AVAILABLE 0
#endif

namespace KryneEngine::Simd
{
    enum class SimdSupport: u16
    {
        None = 0,

        // x86_64 SIMD instruction sets
        SSE2 = 1 << 0,
        SSE3 = 1 << 1,
        SSSE3 = 1 << 2,
        SSE41 = 1 << 3,
        SSE42 = 1 << 4,
        AVX = 1 << 5,
        AVX2 = 1 << 6,
        FMA = 1 << 8,

        // ARM SIMD instruction set
        Neon = 1,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(SimdSupport);

    inline SimdSupport g_simdSupport =
#if defined(__ARM_NEON)
        SimdSupport::Neon;
#elif defined(__SSE2__)
        SimdSupport::SSE2;
#else
        SimdSupport::None;
#endif
}