/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2026.
 */

#include <KryneEngine/Core/Math/Simd/SimdCommon.hpp>
#include <KryneEngine/Core/Platform/Cpu.hpp>
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    TEST(Cpu, GetSimdCapabilities)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const Simd::SimdSupport capabilities = Simd::g_simdSupport;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        Simd::g_simdSupport = Simd::SimdSupport::None;

        Platform::InitSimdFlags();

        EXPECT_EQ(capabilities, Simd::g_simdSupport & capabilities);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }
}