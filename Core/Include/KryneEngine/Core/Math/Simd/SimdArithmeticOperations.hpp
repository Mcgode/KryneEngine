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

    /**
     * @defgroup f32x4x3 Arithmetic Operations
     */

    KE_FORCEINLINE f32x4x3 Add(const f32x4x3& a, const f32x4x3& b)
    {
        f32x4x3 result;
#if defined(__ARM_NEON)
        result.val[0] = vaddq_f32(a.val[0], b.val[0]);
        result.val[1] = vaddq_f32(a.val[1], b.val[1]);
        result.val[2] = vaddq_f32(a.val[2], b.val[2]);
#else
        result[0] = Add(a[0], b[0]);
        result[1] = Add(a[1], b[1]);
        result[2] = Add(a[2], b[2]);
#endif
        return result;
    }

    KE_FORCEINLINE f32x4x3 Subtract(const f32x4x3& a, const f32x4x3& b)
    {
        f32x4x3 result;
#if defined(__ARM_NEON)
        result.val[0] = vsubq_f32(a.val[0], b.val[0]);
        result.val[1] = vsubq_f32(a.val[1], b.val[1]);
        result.val[2] = vsubq_f32(a.val[2], b.val[2]);
#else
        result[0] = Subtract(a[0], b[0]);
        result[1] = Subtract(a[1], b[1]);
        result[2] = Subtract(a[2], b[2]);
#endif
        return result;
    }

    /**
     * @defgroup f32x4x4 Arithmetic Operations
     */

    KE_FORCEINLINE f32x4x4 Add(const f32x4x4& a, const f32x4x4& b)
    {
        f32x4x4 result;
#if defined(__ARM_NEON)
        result.val[0] = vaddq_f32(a.val[0], b.val[0]);
        result.val[1] = vaddq_f32(a.val[1], b.val[1]);
        result.val[2] = vaddq_f32(a.val[2], b.val[2]);
        result.val[3] = vaddq_f32(a.val[3], b.val[3]);
#else
        result[0] = Add(a[0], b[0]);
        result[1] = Add(a[1], b[1]);
        result[2] = Add(a[2], b[2]);
        result[3] = Add(a[3], b[3]);
#endif
        return result;
    }

    KE_FORCEINLINE f32x4x4 Subtract(const f32x4x4& a, const f32x4x4& b)
    {
        f32x4x4 result;
#if defined(__ARM_NEON)
        result.val[0] = vsubq_f32(a.val[0], b.val[0]);
        result.val[1] = vsubq_f32(a.val[1], b.val[1]);
        result.val[2] = vsubq_f32(a.val[2], b.val[2]);
        result.val[3] = vsubq_f32(a.val[3], b.val[3]);
#else
        result[0] = Subtract(a[0], b[0]);
        result[1] = Subtract(a[1], b[1]);
        result[2] = Subtract(a[2], b[2]);
        result[3] = Subtract(a[3], b[3]);
#endif
        return result;
    }

    KE_FORCEINLINE void Transpose(f32x4x4& m)
    {
#if defined(__ARM_NEON)
        const float32x4x2_t t0 = vtrnq_f32(m.val[0], m.val[1]);
        const float32x4x2_t t1 = vtrnq_f32(m.val[2], m.val[3]);

        m.val[0] = vcombine_f32(vget_low_f32(t0.val[0]), vget_low_f32(t1.val[0]));
        m.val[1] = vcombine_f32(vget_low_f32(t0.val[1]), vget_low_f32(t1.val[1]));
        m.val[2] = vcombine_f32(vget_high_f32(t0.val[0]), vget_high_f32(t1.val[0]));
        m.val[3] = vcombine_f32(vget_high_f32(t0.val[1]), vget_high_f32(t1.val[1]));
#elif defined(__SSE2__)
        _MM_TRANSPOSE4_PS(m[0], m[1], m[2], m[3]);
#else
        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 j = i + 1; j < 4; ++j)
            {
                const float v = m[i][j];
                m[i][j] = m[j][i];
                m[j][i] = v;
            }
        }
#endif
    }

    KE_FORCEINLINE f32x4x4 Transposed(const f32x4x4& a)
    {
        f32x4x4 result;
        Transpose(result);
        return result;
    }

    KE_FORCEINLINE f32x4x4 Multiply(const f32x4x4& a, const f32x4x4& b)
    {
#if defined(__ARM_NEON)
        f32x4x4 result {};
        for (int i = 0; i < 4; ++i)
        {
            result.val[i] = vfmaq_laneq_f32(result.val[i], b.val[0], a.val[i], 0);
            result.val[i] = vfmaq_laneq_f32(result.val[i], b.val[1], a.val[i], 1);
            result.val[i] = vfmaq_laneq_f32(result.val[i], b.val[2], a.val[i], 2);
            result.val[i] = vfmaq_laneq_f32(result.val[i], b.val[3], a.val[i], 3);
        }
        return result;
#elif defined(__SSE2__)
        f32x4x4 result {};
        for (int i = 0; i < 4; ++i)
        {
            result[i] = _mm_mul_ps(b[i], _mm_swizzle(a[i], 0));
            result[i] = FusedMultiplyAdd(b[i], _mm_swizzle(a[i], 1), result[i]);
            result[i] = FusedMultiplyAdd(b[i], _mm_swizzle(a[i], 2), result[i]);
            result[i] = FusedMultiplyAdd(b[i], _mm_swizzle(a[i], 3), result[i]);
        }
#else
        f32x4x4 result {};
        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 j = 0; j < 4; ++j)
            {
                for (auto k = 0; k < 4; ++k)
                {
                    result[i][j] += a[i][k] * b[k][j];
                }
            }
        }
#endif
    }

    KE_FORCEINLINE f32x4 Multiply(const f32x4x4& m, const f32x4& v)
    {
#if defined(__ARM_NEON)
        f32x4 result;
        result = vfmaq_laneq_f32(result, m.val[0], v, 0);
        result = vfmaq_laneq_f32(result, m.val[1], v, 1);
        result = vfmaq_laneq_f32(result, m.val[2], v, 2);
        result = vfmaq_laneq_f32(result, m.val[3], v, 3);
        return result;
#elif defined(__SSE2__)
        const __m128 r0 = _mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0)), m[0]);
        const __m128 r1 = _mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)), m[1]);
        const __m128 r2 = _mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)), m[2]);
        const __m128 r3 = _mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)), m[3]);
        return _mm_add_ps(_mm_add_ps(r0, r1), _mm_add_ps(r2, r3));
#else
        f32x4 result;
        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 j = 0; j < 4; ++j)
            {
                result[i] += m[i][j] * v[j];
            }
        }
        return result;
#endif
    }
}