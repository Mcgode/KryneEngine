/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Platform/Cpu.hpp"

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
    // Detected before `main()`, without any call from the application: the initializer is compiled into every object
    // file that uses this variable, so it can't be dropped when linking the static library.
    // Code running during static initialization may still read `None`, which only selects the scalar fallbacks.
    inline SimdSupport g_simdSupport = Platform::DetectSimdSupport();
}