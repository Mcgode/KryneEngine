/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Math/Vector4.hpp>
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

using namespace KryneEngine::Math;

namespace KryneEngine::Tests::Math
{
    TEST(Vector, Creation)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        float2 float2Vec {};
        uint3 uint3Vec {};
        int4 int4Vec {};

        EXPECT_EQ(sizeof(float2Vec), 2 * sizeof(float));
        EXPECT_EQ(sizeof(uint3Vec), 3 * sizeof(u32));
        EXPECT_EQ(sizeof(int4Vec), 4 * sizeof(s32));

        int3_simd int3SimdVec {};
        float4_simd float4SimdVec {};

        EXPECT_EQ(sizeof(int3SimdVec), 16);
        EXPECT_EQ(sizeof(float4SimdVec), 16);

        // Check that padding is properly 0-initialized
        EXPECT_EQ((&int3SimdVec.x)[3], 0);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Vector, Equals)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const float4 vecA { 1, 2, 3, 4 };
            const float4 vecB {};
            EXPECT_NE(vecA, vecB);
        }

        {
            const float4_simd vecA { 1, 2, 3, 4 };
            const float4_simd vecB {};
            EXPECT_NE(vecA, vecB);
        }

        {
            const float4 vecA { 1 };
            const float4 vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        {
            const float4_simd vecA { 1 };
            const float4_simd vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        {
            const float3_simd vecA { 1 };
            const float3_simd vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        {
            const uint2 vecA { 1 };
            const uint2 vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        {
            const double4 vecA { 1 };
            const double4 vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        {
            const double4_simd vecA { 1 };
            const double4_simd vecB { 1 };
            EXPECT_EQ(vecA, vecB);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Vector, Add)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, 1 };

            const int2 result = vecA + vecB;
            EXPECT_EQ(result, int2(3));
        }

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, 1 };

            const int2 result = vecA + vecB;
            EXPECT_EQ(result, int2(3));
        }

        {
            const uint3 vecA { 3, 2, 1 };
            const uint3 vecB { 3 };

            const uint3 result = vecA + vecB;
            EXPECT_EQ(result, uint3(6, 5, 4));
        }

        {
            const uint3_simd vecA { 3, 2, 1 };
            const uint3_simd vecB { 3 };

            const uint3_simd result = vecA + vecB;
            EXPECT_EQ(result, uint3_simd(6, 5, 4));
        }

        {
            const float4 vecA { 1, 2, 3, 4 };
            const float4 vecB { 1};

            const float4 result = vecA + vecB;
            EXPECT_EQ(result, float4(2, 3, 4, 5));
        }

        {
            const float4_simd vecA { 1, 2, 3, 4 };
            const float4_simd vecB { 1};

            const float4_simd result = vecA + vecB;
            EXPECT_EQ(result, float4_simd(2, 3, 4, 5));
        }

        {
            const double4 vecA { 1, 2, 3, 4 };
            const double4 vecB { 1 };
            const double4 result = vecA + vecB;
            EXPECT_EQ(result, double4(2, 3, 4, 5));
        }

        {
            const double4_simd vecA { 1, 2, 3, 4 };
            const double4_simd vecB { 1 };
            const double4_simd result = vecA + vecB;
            EXPECT_EQ(result, double4_simd(2, 3, 4, 5));
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Vector, Subtract)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, 1 };

            const int2 result = vecA - vecB;
            EXPECT_EQ(result, int2(-1, 1));
        }

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, 1 };

            const int2 result = vecA - vecB;
            EXPECT_EQ(result, int2(-1, 1));
        }

        {
            const uint3 vecA { 3, 4, 5 };
            const uint3 vecB { 3 };

            const uint3 result = vecA - vecB;
            EXPECT_EQ(result, uint3(0, 1, 2));
        }

        {
            const uint3_simd vecA { 3, 4, 5 };
            const uint3_simd vecB { 3 };

            const uint3_simd result = vecA - vecB;
            EXPECT_EQ(result, uint3_simd(0, 1, 2));
        }

        {
            const float4 vecA { 1, 2, 3, 4 };
            const float4 vecB { 1};

            const float4 result = vecA - vecB;
            EXPECT_EQ(result, float4(0, 1, 2, 3));
        }

        {
            const float4_simd vecA { 1, 2, 3, 4 };
            const float4_simd vecB { 1};

            const float4_simd result = vecA - vecB;
            EXPECT_EQ(result, float4_simd(0, 1, 2, 3));
        }

        {
            const double4 vecA { 1, 2, 3, 4 };
            const double4 vecB { 1};

            const double4 result = vecA - vecB;
            EXPECT_EQ(result, double4(0, 1, 2, 3));
        }

        {
            const double4_simd vecA { 1, 2, 3, 4 };
            const double4_simd vecB { 1};

            const double4_simd result = vecA - vecB;
            EXPECT_EQ(result, double4_simd(0, 1, 2, 3));
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Vector, Multiply)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, -1 };

            const int2 result = vecA * vecB;
            EXPECT_EQ(result, int2(2, -2));
        }

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, -1 };

            const int2 result = vecA * vecB;
            EXPECT_EQ(result, int2(2, -2));
        }

        {
            const uint3 vecA { 3, 4, 5 };
            const uint3 vecB { 3 };

            const uint3 result = vecA * vecB;
            EXPECT_EQ(result, uint3(9, 12, 15));
        }

        {
            const uint3_simd vecA { 3, 4, 5 };
            const uint3_simd vecB { 3 };

            const uint3_simd result = vecA * vecB;
            EXPECT_EQ(result, uint3_simd(9, 12, 15));
        }

        {
            const float4 vecA { 1, 2, 3, -4 };
            const float4 vecB { 1.5f };

            const float4 result = vecA * vecB;
            EXPECT_EQ(result, float4(1.5f, 3.f, 4.5f, -6.f));
        }

        {
            const float4_simd vecA { 1, 2, 3, -4 };
            const float4_simd vecB { 1.5f };

            const float4_simd result = vecA * vecB;
            EXPECT_EQ(result, float4_simd(1.5f, 3.f, 4.5f, -6.f));
        }

        {
            const double4 vecA { 1, 2, 3, -4 };
            const double4 vecB { 1.5f };

            const double4 result = vecA * vecB;
            EXPECT_EQ(result, double4(1.5f, 3.f, 4.5f, -6.f));
        }

        {
            const double4_simd vecA { 1, 2, 3, -4 };
            const double4_simd vecB { 1.5f };

            const double4_simd result = vecA * vecB;
            EXPECT_EQ(result, double4_simd(1.5f, 3.f, 4.5f, -6.f));
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(Vector, Divide)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, -1 };

            const int2 result = vecA / vecB;
            EXPECT_EQ(result, int2(0, -2));
        }

        {
            const int2 vecA { 1, 2 };
            const int2 vecB { 2, -1 };

            const int2 result = vecA / vecB;
            EXPECT_EQ(result, int2(0, -2));
        }

        {
            const uint3 vecA { 3, 4, 6 };
            const uint3 vecB { 3 };

            const uint3 result = vecA / vecB;
            EXPECT_EQ(result, uint3(1, 1, 2));
        }

        {
            const uint3_simd vecA { 3, 4, 6 };
            const uint3_simd vecB { 3 };

            const uint3_simd result = vecA / vecB;
            EXPECT_EQ(result, uint3_simd(1, 1, 2));
        }

        {
            const float4 vecA { 1, 2, 3, -4 };
            const float4 vecB { 0.5f };

            const float4 result = vecA / vecB;
            EXPECT_EQ(result, float4(2, 4, 6, -8));
        }

        {
            const float4_simd vecA { 1, 2, 3, -4 };
            const float4_simd vecB { 0.5f };

            const float4_simd result = vecA / vecB;
            EXPECT_EQ(result, float4_simd(2, 4, 6, -8));
        }

        {
            const double4 vecA { 1, 2, 3, -4 };
            const double4 vecB { 0.5f };

            const double4 result = vecA / vecB;
            EXPECT_EQ(result, double4(2, 4, 6, -8));
        }

        {
            const double4_simd vecA { 1, 2, 3, -4 };
            const double4_simd vecB { 0.5f };

            const double4_simd result = vecA / vecB;
            EXPECT_EQ(result, double4_simd(2, 4, 6, -8));
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }
}