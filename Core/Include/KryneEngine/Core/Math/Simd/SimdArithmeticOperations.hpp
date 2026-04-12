/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Math/Simd/SimdTypes.hpp"

#if defined(__SSE2__)
#   include <emmintrin.h>
#   include <smmintrin.h>
#   include <xmmintrin.h>
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

    KE_FORCEINLINE float ReduceSum(const f32x4 a)
    {
#if defined(__ARM_NEON)
        return vaddvq_f32(a);
#elif defined(__SSE2__)
#   if defined(__SSE3__)
        const f32x4 v = _mm_hadd_ps(a, a);
        return _mm_cvtss_f32(_mm_hadd_ps(v, v));
#   else
        __m128 high = _mm_movehl_ps(a, a);   // [v2, v3, v2, v3]
        __m128 v = _mm_add_ps(a, high);             // [v0+v2, v1+v3, ..., ...]
        high = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)); // broadcast lane 1
        v = _mm_add_ss(v, high);             // lane 0 = (v0+v2) + (v1+v3)
        return _mm_cvtss_f32(v);
#   endif
#else
        float result = 0.0f;
        for (int i = 0; i < 4; ++i)
            result += a.m_value[i];
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

    KE_FORCEINLINE u32 ReduceSum(const u32x4 a)
    {
#if defined(__ARM_NEON)
        return vaddvq_u32(a);
#elif defined(__SSE2__)
#   if defined(__SSE3__)
        const __m128i v = _mm_hadd_epi32(a, a);
        return static_cast<u32>(_mm_cvtsi128_si32(_mm_hadd_epi32(v, v)));
#   else
        __m128i high = _mm_unpackhi_epi64(a, a);   // [a2, a3, a2, a3]
        __m128i v = _mm_add_epi32(a, high);        // [a0+a2, a1+a3, ..., ...]
        high = _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 1, 1, 1));
        v = _mm_add_epi32(v, high);
        return static_cast<u32>(_mm_cvtsi128_si32(v));
#   endif
#else
        u32 result = 0;
        for (int i = 0; i < 4; ++i)
            result += a.m_value[i];
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

    KE_FORCEINLINE s32 ReduceSum(const s32x4 a)
    {
#if defined(__ARM_NEON)
        return vaddvq_s32(a);
#else
        s32 result = 0;
        for (int i = 0; i < 4; ++i)
            result += a.m_value[i];
        return result;
#endif
    }
#endif
}