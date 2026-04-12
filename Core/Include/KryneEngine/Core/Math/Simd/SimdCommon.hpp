/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#if defined(__ARM_NEON)
#   define KE_SIMD_AVAILABLE 1
#elif defined(__SSE2__)
#   define KE_SIMD_AVAILABLE 1

#   define _mm_swizzle(vec, idx) _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(idx, idx, idx, idx))
#   define _mm_swizzle4(vec, i0, i1, i2, i3) _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(i0, i1, i2, i3))
#endif

#if !defined(KE_SIMD_AVAILABLE)
#   define KE_SIMD_AVAILABLE 0
#endif

namespace KryneEngine::Simd
{
    static constexpr bool kIsAvailable = KE_SIMD_AVAILABLE;
}