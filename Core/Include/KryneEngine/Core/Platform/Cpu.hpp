/**
 * @file
 * @author Max Godefroy
 * @date 21/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Math/Simd/SimdSupport.hpp"

namespace KryneEngine::Platform
{
    /**
     * @brief Detects the SIMD instruction sets the CPU supports and the OS allows using.
     *
     * Used to initialize `Simd::g_simdSupport` before `main()`, so applications don't need to call it.
     */
    Simd::SimdSupport DetectSimdSupport();

    /**
     * @brief Retrieves the number of CPU cores this process can actually be scheduled on.
     *
     * Unlike `std::thread::hardware_concurrency()`, this accounts for OS-level restrictions on
     * which cores the process may use (e.g. a cgroup/cpuset limit on Linux, or a process affinity
     * mask on Windows), falling back to `std::thread::hardware_concurrency()` when no such
     * restriction can be determined.
     */
    u32 GetUsableCpuCoreCount();
}