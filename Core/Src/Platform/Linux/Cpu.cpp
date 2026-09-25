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
    void InitSimdFlags()
    {
#if defined(__ARM_NEON)
        Simd::g_simdSupport = Simd::SimdSupport::Neon;
#elif defined(__x86_64__)
        // Based on https://en.wikipedia.org/wiki/CPUID

        u32 eax, ebx, ecx, edx;

        {
            __get_cpuid(1, &eax, &ebx, &ecx, &edx);

            if (edx & (1 << 26))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE2;
            if (ecx & (1 << 0))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE3;
            if (ecx & (1 << 9))
                Simd::g_simdSupport |= Simd::SimdSupport::SSSE3;
            if (ecx & (1 << 12))
                Simd::g_simdSupport |= Simd::SimdSupport::FMA;
            if (ecx & (1 << 19))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE41;
            if (ecx & (1 << 20))
                Simd::g_simdSupport |= Simd::SimdSupport::SSE42;
            if (ecx & (1 << 28))
                Simd::g_simdSupport |= Simd::SimdSupport::AVX;
        }

        {
            __get_cpuid(1, &eax, &ebx, &ecx, &edx);

            if (ebx & (1 << 5))
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