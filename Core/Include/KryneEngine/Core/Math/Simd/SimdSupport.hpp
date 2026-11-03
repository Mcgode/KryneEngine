/**
 * @file
 * @author Max Godefroy
 * @date 30/10/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/BitUtils.hpp"

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
        F16C = 1 << 7,
        FMA = 1 << 8,

        // ARM SIMD instruction set
        Neon = 1,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(SimdSupport);
}
