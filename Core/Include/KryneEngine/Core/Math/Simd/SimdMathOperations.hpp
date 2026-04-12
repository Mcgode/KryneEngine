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
#endif

namespace KryneEngine::Simd
{
#if defined(__SSE2__)
    KE_FORCEINLINE __m128 Mat2Mul(__m128 a, __m128 b)
    {
        return
            _mm_add_ps(_mm_mul_ps(                          a, _mm_swizzle4(b, 3,0,3,0)),
                       _mm_mul_ps(_mm_swizzle4(a, 2,3,0,1), _mm_swizzle4(b, 1,2,1,2)));
    }
    // 2x2 row major Matrix adjugate multiply (A#)*B
    KE_FORCEINLINE __m128 Mat2AdjMul(__m128 a, __m128 b)
    {
        return
            _mm_sub_ps(_mm_mul_ps(_mm_swizzle4(a, 0,0,3,3), b),
                       _mm_mul_ps(_mm_swizzle4(a, 2,2,1,1), _mm_swizzle4(b, 1,0,3,2)));

    }
    // 2x2 row major Matrix multiply adjugate A*(B#)
    KE_FORCEINLINE __m128 Mat2MulAdj(__m128 a, __m128 b)
    {
        return
            _mm_sub_ps(_mm_mul_ps(                          a, _mm_swizzle4(b, 0,3,0,3)),
                       _mm_mul_ps(_mm_swizzle4(a, 2,3,0,1), _mm_swizzle4(b, 1,2,1,2)));
    }
#endif

    KE_FORCEINLINE f32x4x4 Inverse(const f32x4x4& m)
    {
#if defined (__ARM_NEON)

#elif defined(__SSE2__)
        // Based on https://lxjk.github.io/2017/09/03/Fast-4x4-Matrix-Inverse-with-SSE-SIMD-Explained.html

        const __m128 a = _mm_shuffle_ps(m[0], m[1], _MM_SHUFFLE(1,0,1,0));
        const __m128 b = _mm_shuffle_ps(m[0], m[1], _MM_SHUFFLE(3,2,3,2));
        const __m128 c = _mm_shuffle_ps(m[2], m[3], _MM_SHUFFLE(1,0,1,0));
        const __m128 d = _mm_shuffle_ps(m[2], m[3], _MM_SHUFFLE(3,2,3,2));

        const __m128 detSub = _mm_sub_ps(
            _mm_mul_ps(_mm_shuffle_ps(m[0], m[2], _MM_SHUFFLE(2,0,2,0)), _mm_shuffle_ps(m[1], m[3], _MM_SHUFFLE(3,1,3,1))),
            _mm_mul_ps(_mm_shuffle_ps(m[0], m[2], _MM_SHUFFLE(3,1,3,1)), _mm_shuffle_ps(m[1], m[3], _MM_SHUFFLE(2,0,2,0))));
        const __m128 detA = _mm_swizzle(detSub, 0);
        const __m128 detB = _mm_swizzle(detSub, 1);
        const __m128 detC = _mm_swizzle(detSub, 2);
        const __m128 detD = _mm_swizzle(detSub, 3);

        // let iM = 1/|M| * | X  Y |
        //                  | Z  W |

        // D#C
        __m128 d_c = Mat2AdjMul(d, c);
        // A#B
        __m128 a_b = Mat2AdjMul(a, b);
        // X# = |D|A - B(D#C)
        __m128 x_ = _mm_sub_ps(_mm_mul_ps(detD, a), Mat2Mul(b, d_c));
        // W# = |A|D - C(A#B)
        __m128 w_ = _mm_sub_ps(_mm_mul_ps(detA, d), Mat2Mul(c, a_b));

        // |M| = |A|*|D| + ... (continue later)
        __m128 detM = _mm_mul_ps(detA, detD);

        // Y# = |B|C - D(A#B)#
        __m128 y_ = _mm_sub_ps(_mm_mul_ps(detB, c), Mat2MulAdj(d, a_b));
        // Z# = |C|B - A(D#C)#
        __m128 z_ = _mm_sub_ps(_mm_mul_ps(detC, b), Mat2MulAdj(a, d_c));

        // |M| = |A|*|D| + |B|*|C| ... (continue later)
        detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

        // tr((A#B)(D#C))
        __m128 tr = _mm_mul_ps(a_b, _mm_swizzle4(d_c, 3, 1, 2, 0));
#   if defined(__SSE3__)
        tr = _mm_hadd_ps(tr, tr);
        tr = _mm_hadd_ps(tr, tr);
#   else
        // SSE2 fallback for double horizontal add
        // First hadd: tr = [tr3+tr2, tr3+tr2, tr1+tr0, tr1+tr0]
        __m128 temp = _mm_add_ps(tr, _mm_swizzle4(tr, 2, 3, 0, 1));
        // Second hadd: sum all elements into each component
        tr = _mm_add_ps(temp, _mm_swizzle4(temp, 1, 0, 3, 2));
#   endif
        // |M| = |A|*|D| + |B|*|C| - tr((A#B)(D#C)
        detM = _mm_sub_ps(detM, tr);

        const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
        // (1/|M|, -1/|M|, -1/|M|, 1/|M|)
        __m128 rDetM = _mm_div_ps(adjSignMask, detM);

        x_ = _mm_mul_ps(x_, rDetM);
        y_ = _mm_mul_ps(y_, rDetM);
        z_ = _mm_mul_ps(z_, rDetM);
        w_ = _mm_mul_ps(w_, rDetM);

        f32x4x4 result;
        result[0] = _mm_shuffle_ps(x_, y_, _MM_SHUFFLE(1,3,1,3));
        result[1] = _mm_shuffle_ps(x_, y_, _MM_SHUFFLE(0,2,0,2));
        result[2] = _mm_shuffle_ps(z_, w_, _MM_SHUFFLE(1,3,1,3));
        result[3] = _mm_shuffle_ps(z_, w_, _MM_SHUFFLE(0,2,0,2));
        return result;
#else
        // Implementation based on the one in the Assimp library

        const float a0 = m[0][0],
                a1 = m[0][1],
                a2 = m[0][2],
                a3 = m[0][3],
                b0 = m[1][0],
                b1 = m[1][1],
                b2 = m[1][2],
                b3 = m[1][3],
                c0 = m[2][0],
                c1 = m[2][1],
                c2 = m[2][2],
                c3 = m[2][3],
                d0 = m[3][0],
                d1 = m[3][1],
                d2 = m[3][2],
                d3 = m[3][3];

        const float determinant = a0*b1*c2*d3 - a0*b1*c3*d2 + a0*b2*c3*d1 - a0*b2*c1*d3 + a0*b3*c1*d2 - a0*b3*c2*d1
            - a1*b2*c3*d0 + a1*b2*c0*d3 - a1*b3*c0*d2 + a1*b3*c2*d0 - a1*b0*c2*d3 + a1*b0*c3*d2
            + a2*b3*c0*d1 - a2*b3*c1*d0 + a2*b0*c1*d3 - a2*b0*c3*d1 + a2*b1*c3*d0 - a2*b1*c0*d3
            - a3*b0*c1*d2 + a3*b0*c2*d1 - a3*b1*c2*d0 + a3*b1*c0*d2 - a3*b2*c0*d1 + a3*b2*c1*d0;
        const float invDet = 1.0f / determinant;

        f32x4x4 result;
        result[0][0] = invDet * (b1 * (c2 * d3 - c3 * d2) + b2 * (c3 * d1 - c1 * d3) + b3 * (c1 * d2 - c2 * d1));
        result[0][1] = -invDet * (a1 * (c2 * d3 - c3 * d2) + a2 * (c3 * d1 - c1 * d3) + a3 * (c1 * d2 - c2 * d1));
        result[0][2] = invDet * (a1 * (b2 * d3 - b3 * d2) + a2 * (b3 * d1 - b1 * d3) + a3 * (b1 * d2 - b2 * d1));
        result[0][3] = -invDet * (a1 * (b2 * c3 - b3 * c2) + a2 * (b3 * c1 - b1 * c3) + a3 * (b1 * c2 - b2 * c1));
        result[1][0] = -invDet * (b0 * (c2 * d3 - c3 * d2) + b2 * (c3 * d0 - c0 * d3) + b3 * (c0 * d2 - c2 * d0));
        result[1][1] = invDet * (a0 * (c2 * d3 - c3 * d2) + a2 * (c3 * d0 - c0 * d3) + a3 * (c0 * d2 - c2 * d0));
        result[1][2] = -invDet * (a0 * (b2 * d3 - b3 * d2) + a2 * (b3 * d0 - b0 * d3) + a3 * (b0 * d2 - b2 * d0));
        result[1][3] = invDet * (a0 * (b2 * c3 - b3 * c2) + a2 * (b3 * c0 - b0 * c3) + a3 * (b0 * c2 - b2 * c0));
        result[2][0] = invDet * (b0 * (c1 * d3 - c3 * d1) + b1 * (c3 * d0 - c0 * d3) + b3 * (c0 * d1 - c1 * d0));
        result[2][1] = -invDet * (a0 * (c1 * d3 - c3 * d1) + a1 * (c3 * d0 - c0 * d3) + a3 * (c0 * d1 - c1 * d0));
        result[2][2] = invDet * (a0 * (b1 * d3 - b3 * d1) + a1 * (b3 * d0 - b0 * d3) + a3 * (b0 * d1 - b1 * d0));
        result[2][3] = -invDet * (a0 * (b1 * c3 - b3 * c1) + a1 * (b3 * c0 - b0 * c3) + a3 * (b0 * c1 - b1 * c0));
        result[3][0] = -invDet * (b0 * (c1 * d2 - c2 * d1) + b1 * (c2 * d0 - c0 * d2) + b2 * (c0 * d1 - c1 * d0));
        result[3][1] = invDet * (a0 * (c1 * d2 - c2 * d1) + a1 * (c2 * d0 - c0 * d2) + a2 * (c0 * d1 - c1 * d0));
        result[3][2] = -invDet * (a0 * (b1 * d2 - b2 * d1) + a1 * (b2 * d0 - b0 * d2) + a2 * (b0 * d1 - b1 * d0));
        result[3][3] = invDet * (a0 * (b1 * c2 - b2 * c1) + a1 * (b2 * c0 - b0 * c2) + a2 * (b0 * c1 - b1 * c0));

        return result;
#endif
    }
}