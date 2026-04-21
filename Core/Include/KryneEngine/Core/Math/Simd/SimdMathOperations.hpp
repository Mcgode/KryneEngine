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
#if defined(__ARM_NEON)
    KE_FORCEINLINE f32x4 Mat2Mul(const f32x4 a, const f32x4 b)
    {
        const float32x4x2_t trnA = vtrnq_f32(a, a);  // [ a0, a0, a2, a2 ] [ a1, a1, a3, a3 ]
        const f32x4 a0033 = vcombine_f32(vget_low_f32(trnA.val[0]), vget_high_f32(trnA.val[1]));
        const f32x4 a1122 = vcombine_f32(vget_low_f32(trnA.val[1]), vget_high_f32(trnA.val[0]));

        const f32x4 b2301 = vcombine_f32(vget_high_f32(b), vget_low_f32(b));

        const float32x4_t result = vaddq_f32(
            vmulq_f32(a0033, b),
            vmulq_f32(a1122, b2301));
        return result;
    }

    KE_FORCEINLINE f32x4 Mat2AdjMul(const f32x4 a, const f32x4 b)
    {
        const float32x4x2_t trnA = vtrnq_f32(a, a);  // [ a0, a0, a2, a2 ] [ a1, a1, a3, a3 ]
        const f32x4 a3300 = vcombine_f32(vget_high_f32(trnA.val[1]), vget_low_f32(trnA.val[0]));
        const f32x4 a1122 = vcombine_f32(vget_low_f32(trnA.val[1]), vget_high_f32(trnA.val[0]));

        const f32x4 b2301 = vcombine_f32(vget_high_f32(b), vget_low_f32(b));

        return vsubq_f32(
            vmulq_f32(a3300, b),
            vmulq_f32(a1122, b2301));
    }

    KE_FORCEINLINE f32x4 Mat2MulAdj(const f32x4 a, const f32x4 b)
    {
        const f32x4 revB = vrev64q_f32(b);  // [ b1, b0, b3, b2 ]

        const f32x4 temp0 = vcombine_f32(vget_high_f32(b), vget_high_f32(b)); // [ b2, b3, b2, b3 ]
        const f32x4 temp1 = vcombine_f32(vget_low_f32(revB), vget_low_f32(revB));   // [ b1, b0, b1, b0 ]

        const f32x4 b2121 = vtrn1q_f32(temp0, temp1);   // [ b2, b1, b2, b1 ]
        const f32x4 b3030 = vtrn2q_f32(temp0, temp1);   // [ b3, b0, b3, b0 ]

        const f32x4 a1032 = vrev64q_f32(a); // [ a1, a0, a3, a2 ]

        return vsubq_f32(
            vmulq_f32(a,     b3030),
            vmulq_f32(a1032, b2121));
    }
#elif defined(__SSE2__)
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
        const f32x4 a = vcombine_f32(vget_low_f32(m.val[0]), vget_low_f32(m.val[1]));
        const f32x4 b = vcombine_f32(vget_high_f32(m.val[0]), vget_high_f32(m.val[1]));
        const f32x4 c = vcombine_f32(vget_low_f32(m.val[2]), vget_low_f32(m.val[3]));
        const f32x4 d = vcombine_f32(vget_high_f32(m.val[2]), vget_high_f32(m.val[3]));

        const float32x4x2_t tr0 = vtrnq_f32(m.val[0], m.val[2]); // [ m00, m20, m02, m22 ] [ m01, m21, m03, m23 ]
                                                                 // -> [ a0, c0, b0, d0 ] [ a1, c1, b1, d1 ]
        const float32x4x2_t tr1 = vtrnq_f32(m.val[1], m.val[3]); // [ m10, m30, m12, m32 ] [ m11, m31, m13, m33 ]
                                                                 // -> [ a2, c2, b2, d2 ] [ a3, c3, b3, d3 ]
        // Calculating [ det(A), det(C), det(B), det(D) ]
        // det(A) = a0 * a3 - a1 * a2
        const f32x4 detSub = vsubq_f32(
            vmulq_f32(tr0.val[0], tr1.val[1]),  // x0 * x3
            vmulq_f32(tr0.val[1], tr1.val[0])); // x1 * x2

        const f32x4 detA = vdupq_laneq_f32(detSub, 0);
        const f32x4 detB = vdupq_laneq_f32(detSub, 2);
        const f32x4 detC = vdupq_laneq_f32(detSub, 1);
        const f32x4 detD = vdupq_laneq_f32(detSub, 3);

        const f32x4 d_c = Mat2AdjMul(d, c);
        const f32x4 a_b = Mat2AdjMul(a, b);

        // X# = 1 / detM * (detD * a - b * (d# * c))
        f32x4 xAdj = vsubq_f32(vmulq_f32(detD, a), Mat2Mul(b, d_c));

        // W# = 1 / det(m) * (detA * d - c * (a# * b))
        f32x4 wAdj = vsubq_f32(vmulq_f32(detA, d), Mat2Mul(c, a_b));

        // detM = detA * detD + ...
        f32x4 detM = vmulq_f32(detA, detD);

        f32x4 yAdj = vsubq_f32(vmulq_f32(detB, c), Mat2MulAdj(d, a_b));
        f32x4 zAdj = vsubq_f32(vmulq_f32(detC, b), Mat2MulAdj(a, d_c));

        // ... + detB * detC - ...
        detM = vaddq_f32(detM, vmulq_f32(detB, detC));

        {
            // tr(i * j) = i0 * j0 + i1 * j2 + i2 * j1 + i3 * j3

            const float32x2_t d_c_hi = vget_high_f32(d_c); // [ dc2, dc3 ]
            const f32x4 d_c_2323 = vcombine_f32(d_c_hi, d_c_hi); // [ dc2, dc3, dc2, dc3 ]
            const f32x4 d_c_0213 = vzip1q_f32(d_c, d_c_2323); // [ dc0, dc2, dc1, dc3 ]

            f32x4 tr = vmulq_f32(a_b, d_c_0213);
            tr =  vdupq_n_f32(vaddvq_f32(tr));

            // ... - tr( (a# * b) * (d# * c) )
            detM = vsubq_f32(detM, tr);
        }

        constexpr f32x4 adjSignMask { 1.f, -1.f, -1.f, 1.f };
        const f32x4 rDetM = vdivq_f32(adjSignMask, detM);

        xAdj = vmulq_f32(xAdj, rDetM);
        yAdj = vmulq_f32(yAdj, rDetM);
        zAdj = vmulq_f32(zAdj, rDetM);
        wAdj = vmulq_f32(wAdj, rDetM);

        f32x4x4 result;
        {
            const f32x4 revX = vrev64q_f32(xAdj);               // [ x1, x0, x3, x2 ]
            const f32x4 revY = vrev64q_f32(yAdj);               // [ y1, y0, y3, y2 ]
            const float32x2_t revX_lo = vget_low_f32(revX);     // [ x1, x0 ]
            const float32x2_t revX_hi = vget_high_f32(revX);    // [ x3, x2 ]
            const float32x2_t revY_lo = vget_low_f32(revY);     // [ y1, y0 ]
            const float32x2_t revY_hi = vget_high_f32(revY);    // [ y3, y2 ]
            const f32x4 temp0 = vcombine_f32(revX_hi, revY_hi); // [ x3, x2, y3, y2 ]
            const f32x4 temp1 = vcombine_f32(revX_lo, revY_lo); // [ x1, x0, y1, y0 ]
            result.val[0] = vtrn1q_f32(temp0, temp1);           // [ x3, x1, y3, y1 ]
            result.val[1] = vtrn2q_f32(temp0, temp1);           // [ x2, x0, y2, y0 ]
        }
        {
            const f32x4 revZ = vrev64q_f32(zAdj);               // [ z1, z0, z3, z2 ]
            const f32x4 revW = vrev64q_f32(wAdj);               // [ w1, w0, w3, w2 ]
            const float32x2_t revZ_lo = vget_low_f32(revZ);     // [ z1, v0 ]
            const float32x2_t revZ_hi = vget_high_f32(revZ);    // [ z3, z2 ]
            const float32x2_t revW_lo = vget_low_f32(revW);     // [ w1, w0 ]
            const float32x2_t revW_hi = vget_high_f32(revW);    // [ w3, w2 ]
            const f32x4 temp0 = vcombine_f32(revZ_hi, revW_hi); // [ z3, z2, w3, w2 ]
            const f32x4 temp1 = vcombine_f32(revZ_lo, revW_lo); // [ z1, z0, w1, w0 ]
            result.val[2] = vtrn1q_f32(temp0, temp1);           // [ z3, z1, w3, w1 ]
            result.val[3] = vtrn2q_f32(temp0, temp1);           // [ z2, z0, w2, w0 ]
        }
        return result;
#elif defined(__SSE2__)
        // Based on https://lxjk.github.io/2017/09/03/Fast-4x4-Matrix-Inverse-with-SSE-SIMD-Explained.html

        const __m128 a = _mm_movelh_ps(m[0], m[1]);
        const __m128 b = _mm_movehl_ps(m[1], m[0]);
        const __m128 c = _mm_movelh_ps(m[2], m[3]);
        const __m128 d = _mm_movehl_ps(m[3], m[2]);

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
        __m128 xAdj = _mm_sub_ps(_mm_mul_ps(detD, a), Mat2Mul(b, d_c));
        // W# = |A|D - C(A#B)
        __m128 wAdj = _mm_sub_ps(_mm_mul_ps(detA, d), Mat2Mul(c, a_b));

        // |M| = |A|*|D| + ... (continue later)
        __m128 detM = _mm_mul_ps(detA, detD);

        // Y# = |B|C - D(A#B)#
        __m128 yAdj = _mm_sub_ps(_mm_mul_ps(detB, c), Mat2MulAdj(d, a_b));
        // Z# = |C|B - A(D#C)#
        __m128 zAdj = _mm_sub_ps(_mm_mul_ps(detC, b), Mat2MulAdj(a, d_c));

        // |M| = |A|*|D| + |B|*|C| ... (continue later)
        detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

        // tr((A#B)(D#C))
        __m128 tr = _mm_mul_ps(a_b, _mm_swizzle4(d_c, 3, 1, 2, 0));
#   if defined(__SSE3__)
        tr = _mm_hadd_ps(tr, tr);
        tr = _mm_hadd_ps(tr, tr);
#   else
        // SSE2 fallback for double horizontal add
        // First hadd: tr = [tr0+tr1, tr0+tr1, tr2+tr3, tr2+tr3]
        __m128 temp = _mm_add_ps(tr, _mm_swizzle4(tr, 2, 3, 0, 1));
        // Second hadd: sum all elements into each component
        tr = _mm_add_ps(temp, _mm_swizzle4(temp, 1, 0, 3, 2));
#   endif
        // |M| = |A|*|D| + |B|*|C| - tr((A#B)(D#C)
        detM = _mm_sub_ps(detM, tr);

        const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
        // (1/|M|, -1/|M|, -1/|M|, 1/|M|)
        __m128 rDetM = _mm_div_ps(adjSignMask, detM);

        xAdj = _mm_mul_ps(xAdj, rDetM);
        yAdj = _mm_mul_ps(yAdj, rDetM);
        zAdj = _mm_mul_ps(zAdj, rDetM);
        wAdj = _mm_mul_ps(wAdj, rDetM);

        f32x4x4 result;
        result[0] = _mm_shuffle_ps(xAdj, yAdj, _MM_SHUFFLE(1,3,1,3));
        result[1] = _mm_shuffle_ps(xAdj, yAdj, _MM_SHUFFLE(0,2,0,2));
        result[2] = _mm_shuffle_ps(zAdj, wAdj, _MM_SHUFFLE(1,3,1,3));
        result[3] = _mm_shuffle_ps(zAdj, wAdj, _MM_SHUFFLE(0,2,0,2));
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