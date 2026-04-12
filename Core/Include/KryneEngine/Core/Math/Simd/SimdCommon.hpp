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
#endif

#if !defined(KE_SIMD_AVAILABLE)
#   define KE_SIMD_AVAILABLE 0
#endif

namespace KryneEngine::Simd
{
    static constexpr bool kIsAvailable = KE_SIMD_AVAILABLE;
}