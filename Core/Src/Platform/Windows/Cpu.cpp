/**
* @file
 * @author Max Godefroy
 * @date 21/04/2026.
 */

#include "KryneEngine/Core/Platform/Cpu.hpp"

#if defined(__x86_64__)
#   include <intrin.h>
#endif

#include "KryneEngine/Core/Math/Simd/SimdCommon.hpp"

namespace KryneEngine::Platform
{
    void InitSimdFlags()
    {
#if defined(__x86_64__)
        // Based on https://en.wikipedia.org/wiki/CPUID

        int regs[4];

        {
            __cpuidex(regs, 1, 0);

            if (regs[3] & (1 << 26))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE2;
            if (regs[2] & (1 << 0))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE3;
            if (regs[2] & (1 << 9))
                Simd::g_simdSupport |= Simd::SimdSupport::SSSE3;
            if (regs[2] & (1 << 19))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE41;
            if (regs[2] & (1 << 20))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE42;
            if (regs[2] & (1 << 28))
                Simd::g_simdSupport |= Simd::SimdSupport::AVX;
        }

        {
            __cpuidex(regs, 7, 0);

            if (regs[1] & (1 << 5))
                Simd::g_simdSupport |= Simd::SimdSupport::AVX2;
        }
#endif
    }
}
