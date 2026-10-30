/**
* @file
 * @author Max Godefroy
 * @date 21/04/2026.
 */

#include "KryneEngine/Core/Platform/Cpu.hpp"

#if defined(__x86_64__)
#   include <cpuid.h>
#endif

#include <thread>

#include "KryneEngine/Core/Math/Simd/SimdCommon.hpp"

namespace KryneEngine::Platform
{
#if defined(__x86_64__)
    namespace
    {
        u64 ReadXcr0()
        {
            u32 low, high;
            __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
            return (static_cast<u64>(high) << 32) | low;
        }
    }
#endif

    void InitSimdFlags()
    {
#if defined(__ARM_NEON)
        Simd::g_simdSupport = Simd::SimdSupport::Neon;
#elif defined(__x86_64__)
        // Based on https://en.wikipedia.org/wiki/CPUID

        u32 eax, ebx, ecx, edx;

        // AVX, AVX2, FMA and F16C instructions are VEX-encoded, so they also need the OS to save AVX state
        // (OSXSAVE + XCR0 bits 1-2), or they fault even on a CPU that supports them.
        bool osSavesAvxState = false;

        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0)
        {
            osSavesAvxState = (ecx & (1 << 27)) && (ReadXcr0() & 0b110) == 0b110;

            if (edx & (1 << 26))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE2;
            if (ecx & (1 << 0))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE3;
            if (ecx & (1 << 9))
                Simd::g_simdSupport |= Simd::SimdSupport::SSSE3;
            if ((ecx & (1 << 12)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::FMA;
            if (ecx & (1 << 19))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE41;
            if (ecx & (1 << 20))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE42;
            if ((ecx & (1 << 28)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::AVX;
            if ((ecx & (1 << 29)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::F16C;
        }

        // AVX2 is reported by leaf 7, sub-leaf 0. `__get_cpuid_count` returns 0 if the CPU doesn't have that leaf.
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0)
        {
            if ((ebx & (1 << 5)) && osSavesAvxState)
                Simd::g_simdSupport |= Simd::SimdSupport::AVX2;
        }
#endif
    }

    u32 GetUsableCpuCoreCount()
    {
        // macOS has no cgroup/cpuset-style mechanism restricting a process to a core subset, so
        // hardware_concurrency() (backed by sysconf(_SC_NPROCESSORS_ONLN)) is already the usable
        // count.
        return std::thread::hardware_concurrency();
    }
}