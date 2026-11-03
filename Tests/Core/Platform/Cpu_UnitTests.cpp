/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2026.
 */

#include <KryneEngine/Core/Math/Simd/SimdCommon.hpp>
#include <KryneEngine/Core/Platform/Cpu.hpp>
#include <gtest/gtest.h>
#include <thread>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    TEST(Cpu, GetSimdCapabilities)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Simd::SimdSupport detected = Platform::DetectSimdSupport();

        // -----------------------------------------------------------------------
        // Check
        // -----------------------------------------------------------------------

        // `g_simdSupport` is initialized before main(), without the test calling anything
        EXPECT_EQ(detected, Simd::g_simdSupport);

        // Neon on ARM, at least SSE2 on x86-64
        EXPECT_NE(detected, Simd::SimdSupport::None);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Cpu, GetUsableCpuCoreCount)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const u32 usableCores = Platform::GetUsableCpuCoreCount();

        // -----------------------------------------------------------------------
        // Check
        // -----------------------------------------------------------------------

        EXPECT_GT(usableCores, 0u);
        EXPECT_LE(usableCores, std::thread::hardware_concurrency());

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }
}