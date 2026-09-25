/**
* @file
 * @author Max Godefroy
 * @date 21/04/2026.
 */

#include "KryneEngine/Core/Platform/Cpu.hpp"

#if defined(__x86_64__)
#   include <intrin.h>
#endif

#include <bit>
#include <thread>
#include <windows.h>

#include "KryneEngine/Core/Math/Simd/SimdCommon.hpp"

namespace KryneEngine::Platform
{
    void InitSimdFlags()
    {
#if defined(__ARM_NEON)
        Simd::g_simdSupport = Simd::SimdSupport::Neon;
#elif defined(__x86_64__)
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
            if (regs[2] & (1 << 12))
                Simd::g_simdSupport |= Simd::SimdSupport::FMA;
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

    u32 GetUsableCpuCoreCount()
    {
        // The process affinity mask reports the cores this process is actually allowed to run on
        // (e.g. restricted via `start /affinity` or job object limits) -- unlike
        // hardware_concurrency(), which reports the host's total core count regardless of such a
        // restriction.
        DWORD_PTR processMask, systemMask;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) != 0)
        {
            const u32 usableCores = static_cast<u32>(std::popcount(static_cast<u64>(processMask)));
            if (usableCores > 0)
                return usableCores;
        }
        return std::thread::hardware_concurrency();
    }
}
