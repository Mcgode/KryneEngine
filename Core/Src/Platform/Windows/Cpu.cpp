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
#if defined(__x86_64__)
    namespace
    {
        // Clang and GCC only allow the `_xgetbv` intrinsic in code compiled for XSAVE
#   if defined(__clang__) || defined(__GNUC__)
        __attribute__((target("xsave")))
#   endif
        u64 ReadXcr0()
        {
            return _xgetbv(0);
        }
    }
#endif

    void InitSimdFlags()
    {
#if defined(__ARM_NEON)
        Simd::g_simdSupport = Simd::SimdSupport::Neon;
#elif defined(__x86_64__)
        // Based on https://en.wikipedia.org/wiki/CPUID

        int regs[4];

        // Unlike `__get_cpuid_count`, `__cpuidex` doesn't check that a leaf exists, so read the highest one first
        __cpuidex(regs, 0, 0);
        const int maxLeaf = regs[0];

        // AVX, AVX2, FMA and F16C instructions are VEX-encoded, so they also need the OS to save AVX state
        // (OSXSAVE + XCR0 bits 1-2), or they fault even on a CPU that supports them.
        bool osSavesAvxState = false;

        if (maxLeaf >= 1)
        {
            __cpuidex(regs, 1, 0);

            osSavesAvxState = (regs[2] & (1 << 27)) && (ReadXcr0() & 0b110) == 0b110;

            if (regs[3] & (1 << 26))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE2;
            if (regs[2] & (1 << 0))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE3;
            if (regs[2] & (1 << 9))
                Simd::g_simdSupport |= Simd::SimdSupport::SSSE3;
            if ((regs[2] & (1 << 12)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::FMA;
            if (regs[2] & (1 << 19))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE41;
            if (regs[2] & (1 << 20))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE42;
            if ((regs[2] & (1 << 28)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::AVX;
            if ((regs[2] & (1 << 29)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::F16C;
        }

        if (maxLeaf >= 7)
        {
            __cpuidex(regs, 7, 0);

            if ((regs[1] & (1 << 5)) && osSavesAvxState)
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
