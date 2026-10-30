/**
* @file
 * @author Max Godefroy
 * @date 21/04/2026.
 */

#include "KryneEngine/Core/Platform/Cpu.hpp"

#if defined(__x86_64__)
#   include <cpuid.h>
#endif

#include <sched.h>
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
        // sched_getaffinity() reports the process' actual usable core set (the same number `nproc`
        // would print), honoring any cgroup/cpuset restriction -- unlike hardware_concurrency(),
        // which reports the host's total core count regardless of such a restriction.
        cpu_set_t set;
        CPU_ZERO(&set);
        if (sched_getaffinity(0, sizeof(set), &set) == 0)
        {
            const u32 usableCores = static_cast<u32>(CPU_COUNT(&set));
            if (usableCores > 0)
                return usableCores;
        }
        return std::thread::hardware_concurrency();
    }
}