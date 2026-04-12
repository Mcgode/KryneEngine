/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Math/Simd/SimdTypes.hpp"

#if defined(__SSE2__)
#   include <emmintrin.h>
#   include <smmintrin.h>
#endif

namespace KryneEngine::Simd
{
    /**
     * @defgroup f32x4 Arithmetic Operations
     */

    KE_FORCEINLINE f32x4 Add(const f32x4 a, const f32x4 b)
    {
#if defined(__ARM_NEON)
        return vaddq_f32(a, b);
#elif defined(__SSE2__)
        return _mm_add_ps(a, b);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] + b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 Subtract(const f32x4 a, const f32x4 b)
    {
#if defined(__ARM_NEON)
        return vsubq_f32(a, b);
#elif defined(__SSE2__)
        return _mm_sub_ps(a, b);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] - b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 Multiply(const f32x4 a, const f32x4 b)
    {
#if defined(__ARM_NEON)
        return vmulq_f32(a, b);
#elif defined(__SSE2__)
        return _mm_mul_ps(a, b);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 Divide(const f32x4 a, const f32x4 b)
    {
#if defined(__ARM_NEON)
        return vdivq_f32(a, b);
#elif defined(__SSE2__)
        return _mm_div_ps(a, b);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] / b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 FusedMultiplyAdd(const f32x4 a, const f32x4 b, const f32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlaq_f32(c, a, b);
#elif defined(__SSE2__)
#   if defined(__AVX2__)
        return _mm_fmadd_ps(a, b, c);
#   else
        return _mm_add_ps(_mm_mul_ps(a, b), c);
#   endif
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] + c.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 FusedMultiplySubtract(const f32x4 a, const f32x4 b, const f32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlsq_f32(c, a, b);
#elif defined(__SSE2__)
#   if defined(__AVX2__)
        return _mm_fmsub_ps(a, b, c);
#   else
        const __m128 result = _mm_mul_ps(a, b);
        return _mm_sub_ps(result, c);
#   endif
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] - c.m_value[i];
        return result;
#endif
    }

    /**
     * @defgroup u32x4 Arithmetic Operations
     */
    
    KE_FORCEINLINE u32x4 Add(const u32x4 a, const u32x4 b)
    {
#if defined(__ARM_NEON)
        return vaddq_u32(a, b);
#elif defined(__SSE2__)
        return _mm_add_epi32(a, b);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] + b.m_value[i];
        return result;
#endif
    }
    
    KE_FORCEINLINE u32x4 Subtract(const u32x4 a, const u32x4 b)
    {
#if defined(__ARM_NEON)
        return vsubq_u32(a, b);
#elif defined(__SSE2__)
        return _mm_sub_epi32(a, b);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] - b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE u32x4 Multiply(const u32x4 a, const u32x4 b)
    {
#if defined(__ARM_NEON)
        return vmulq_u32(a, b);
#elif defined(__SSE2__)
        return _mm_mullo_epi32(a, b);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE u32x4 FusedMultiplyAdd(const u32x4 a, const u32x4 b, const u32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlaq_u32(c, a, b);
#elif defined(__SSE2__)
        return _mm_add_epi32(_mm_mullo_epi32(a, b), c);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] + c.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE u32x4 FusedMultiplySubtract(const u32x4 a, const u32x4 b, const u32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlsq_u32(c, a, b);
#elif defined(__SSE2__)
        return _mm_sub_epi32(_mm_mullo_epi32(a, b), c);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] - c.m_value[i];
        return result;
#endif
    }

    /**
     * @defgroup s32x4 Arithmetic Operations
     */

#if !defined(__SSE2__) // With SSE2, s32x4 and u32x4 are the same, so it would re-define the same functions
    KE_FORCEINLINE s32x4 Add(const s32x4 a, const s32x4 b)
    {
#if defined(__ARM_NEON)
        return vaddq_s32(a, b);
#elif defined(__SSE2__)
        return _mm_add_epi32(a, b);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] + b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 Subtract(const s32x4 a, const s32x4 b)
    {
#if defined(__ARM_NEON)
        return vsubq_s32(a, b);
#elif defined(__SSE2__)
        return _mm_sub_epi32(a, b);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] - b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 Multiply(const s32x4 a, const s32x4 b)
    {
#if defined(__ARM_NEON)
        return vmulq_s32(a, b);
#elif defined(__SSE2__)
        return _mm_mullo_epi32(a, b);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 FusedMultiplyAdd(const s32x4 a, const s32x4 b, const s32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlaq_s32(c, a, b);
#elif defined(__SSE2__)
        return _mm_add_epi32(_mm_mullo_epi32(a, b), c);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] + c.m_value[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 FusedMultiplySubtract(const s32x4 a, const s32x4 b, const s32x4 c)
    {
#if defined(__ARM_NEON)
        return vmlsq_s32(c, a, b);
#elif defined(__SSE2__)
        return _mm_sub_epi32(_mm_mullo_epi32(a, b), c);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = a.m_value[i] * b.m_value[i] - c.m_value[i];
        return result;
#endif
    }
#endif
}