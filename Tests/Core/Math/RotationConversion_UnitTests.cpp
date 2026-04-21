/**
 * @file
 * @author Max Godefroy
 * @date 13/03/2025.
 */

#if defined(__WIN32__)
#   include <corecrt_math_defines.h>
#endif
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Math/RotationConversion.hpp>
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

using namespace KryneEngine::Math;

namespace KryneEngine::Tests::Math
{
    template <EulerOrder Order>
    void PerformBasicEulerToQuaternionTest()
    {
        {
            // Identity quaternion should be equal to null euler
            const Quaternion q = FromEulerAngles<float, float, Order>(float3(0));
            EXPECT_EQ(Quaternion(), q);
        }

        {
            // Rotation around the X axis
            constexpr float angle = M_PI_2 * 0.8f;
            const Quaternion expected = Quaternion().FromAxisAngle(float3(1, 0, 0), angle);
            const Quaternion result = FromEulerAngles<float, float, Order>(float3(angle, 0, 0));
            EXPECT_EQ(expected, result);
        }

        {
            // Rotation around the Y axis
            constexpr float angle = M_PI_2 * 0.6f;
            const Quaternion expected = Quaternion().FromAxisAngle(float3(0, 1, 0), angle);
            const Quaternion result = FromEulerAngles<float, float, Order>(float3(0, angle, 0));
            EXPECT_EQ(expected, result);
        }

        {
            // Rotation around the Z axis
            constexpr float angle = M_PI_2 * 0.2f;
            const Quaternion expected = Quaternion().FromAxisAngle(float3(0, 0, 1), angle);
            const Quaternion result = FromEulerAngles<float, float, Order>(float3(0, 0, angle));
            EXPECT_EQ(expected, result);
        }
    }

    TEST(RotationConversion, EulerToQuaternion)
    {
        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        PerformBasicEulerToQuaternionTest<EulerOrder::XYZ>();
        PerformBasicEulerToQuaternionTest<EulerOrder::XZY>();
        PerformBasicEulerToQuaternionTest<EulerOrder::YXZ>();
        PerformBasicEulerToQuaternionTest<EulerOrder::YZX>();
        PerformBasicEulerToQuaternionTest<EulerOrder::ZXY>();
        PerformBasicEulerToQuaternionTest<EulerOrder::ZYX>();

        constexpr float x = M_PI * 1.8f;
        constexpr float y = M_PI * 0.8f;
        constexpr float z = M_PI * -0.3f;

        const Quaternion qx = Quaternion().FromAxisAngle(float3(1, 0, 0), x);
        const Quaternion qy = Quaternion().FromAxisAngle(float3(0, 1, 0), y);
        const Quaternion qz = Quaternion().FromAxisAngle(float3(0, 0, 1), z);

        {
            const Quaternion expectedXy = qx * qy;
            const Quaternion expectedYx = qy * qx;
            EXPECT_NE(expectedXy, expectedYx);

            const Quaternion resultXyz = FromEulerAngles<float, float, EulerOrder::XYZ>(float3(x, y, 0));
            const Quaternion resultXzy = FromEulerAngles<float, float, EulerOrder::XZY>(float3(x, y, 0));
            const Quaternion resultYxz = FromEulerAngles<float, float, EulerOrder::YXZ>(float3(x, y, 0));
            const Quaternion resultYzx = FromEulerAngles<float, float, EulerOrder::YZX>(float3(x, y, 0));
            const Quaternion resultZxy = FromEulerAngles<float, float, EulerOrder::ZXY>(float3(x, y, 0));
            const Quaternion resultZyx = FromEulerAngles<float, float, EulerOrder::ZYX>(float3(x, y, 0));

            EXPECT_EQ(expectedXy, resultXyz);
            EXPECT_EQ(expectedXy, resultXzy);
            EXPECT_EQ(expectedYx, resultYxz);
            EXPECT_EQ(expectedYx, resultYzx);
            EXPECT_EQ(expectedXy, resultZxy);
            EXPECT_EQ(expectedYx, resultZyx);
        }

        {
            const Quaternion expectedZy = qz * qy;
            const Quaternion expectedYz = qy * qz;
            EXPECT_NE(expectedZy, expectedYz);

            const Quaternion resultXyz = FromEulerAngles<float, float, EulerOrder::XYZ>(float3(0, y, z));
            const Quaternion resultXzy = FromEulerAngles<float, float, EulerOrder::XZY>(float3(0, y, z));
            const Quaternion resultYxz = FromEulerAngles<float, float, EulerOrder::YXZ>(float3(0, y, z));
            const Quaternion resultYzx = FromEulerAngles<float, float, EulerOrder::YZX>(float3(0, y, z));
            const Quaternion resultZxy = FromEulerAngles<float, float, EulerOrder::ZXY>(float3(0, y, z));
            const Quaternion resultZyx = FromEulerAngles<float, float, EulerOrder::ZYX>(float3(0, y, z));

            EXPECT_EQ(expectedYz, resultXyz);
            EXPECT_EQ(expectedZy, resultXzy);
            EXPECT_EQ(expectedYz, resultYxz);
            EXPECT_EQ(expectedYz, resultYzx);
            EXPECT_EQ(expectedZy, resultZxy);
            EXPECT_EQ(expectedZy, resultZyx);
        }

        {
            const Quaternion expectedXz = qx * qz;
            const Quaternion expectedZx = qz * qx;
            EXPECT_NE(expectedXz, expectedZx);

            const Quaternion resultXyz = FromEulerAngles<float, float, EulerOrder::XYZ>(float3(x, 0, z));
            const Quaternion resultXzy = FromEulerAngles<float, float, EulerOrder::XZY>(float3(x, 0, z));
            const Quaternion resultYxz = FromEulerAngles<float, float, EulerOrder::YXZ>(float3(x, 0, z));
            const Quaternion resultYzx = FromEulerAngles<float, float, EulerOrder::YZX>(float3(x, 0, z));
            const Quaternion resultZxy = FromEulerAngles<float, float, EulerOrder::ZXY>(float3(x, 0, z));
            const Quaternion resultZyx = FromEulerAngles<float, float, EulerOrder::ZYX>(float3(x, 0, z));

            EXPECT_EQ(expectedXz, resultXyz);
            EXPECT_EQ(expectedXz, resultXzy);
            EXPECT_EQ(expectedXz, resultYxz);
            EXPECT_EQ(expectedZx, resultYzx);
            EXPECT_EQ(expectedZx, resultZxy);
            EXPECT_EQ(expectedZx, resultZyx);
        }

        {
            const Quaternion resultXyz = FromEulerAngles<float, float, EulerOrder::XYZ>(float3(x, y, z));
            const Quaternion resultXzy = FromEulerAngles<float, float, EulerOrder::XZY>(float3(x, y, z));
            const Quaternion resultYxz = FromEulerAngles<float, float, EulerOrder::YXZ>(float3(x, y, z));
            const Quaternion resultYzx = FromEulerAngles<float, float, EulerOrder::YZX>(float3(x, y, z));
            const Quaternion resultZxy = FromEulerAngles<float, float, EulerOrder::ZXY>(float3(x, y, z));
            const Quaternion resultZyx = FromEulerAngles<float, float, EulerOrder::ZYX>(float3(x, y, z));

            EXPECT_EQ(qx * qy * qz, resultXyz);
            EXPECT_EQ(qx * qz * qy, resultXzy);
            EXPECT_EQ(qy * qx * qz, resultYxz);
            EXPECT_EQ(qy * qz * qx, resultYzx);
            EXPECT_EQ(qz * qx * qy, resultZxy);
            EXPECT_EQ(qz * qy * qx, resultZyx);
        }
    }

    TEST(RotationConversion, QuaternionToEuler)
    {
        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        constexpr auto testBijectiveTransform = [](float _x, float _y, float _z)
        {
            const float3 euler = float3(_x, _y, _z) * float3(M_PI);

            const auto eulerXyz = ToEulerAngles<float3, float, EulerOrder::XYZ>(
                FromEulerAngles<float, float, EulerOrder::XYZ>(euler));
            const auto eulerXzy = ToEulerAngles<float3, float, EulerOrder::XZY>(
                FromEulerAngles<float, float, EulerOrder::XZY>(euler));
            const auto eulerYxz = ToEulerAngles<float3, float, EulerOrder::YXZ>(
                FromEulerAngles<float, float, EulerOrder::YXZ>(euler));
            const auto eulerYzx = ToEulerAngles<float3, float, EulerOrder::YZX>(
                FromEulerAngles<float, float, EulerOrder::YZX>(euler));
            const auto eulerZxy = ToEulerAngles<float3, float, EulerOrder::ZXY>(
                FromEulerAngles<float, float, EulerOrder::ZXY>(euler));
            const auto eulerZyx = ToEulerAngles<float3, float, EulerOrder::ZYX>(
                FromEulerAngles<float, float, EulerOrder::ZYX>(euler));

            EXPECT_LT((euler - eulerXyz).Length(), Quaternion::kQuaternionEpsilon);
            EXPECT_LT((euler - eulerXzy).Length(), Quaternion::kQuaternionEpsilon);
            EXPECT_LT((euler - eulerYxz).Length(), Quaternion::kQuaternionEpsilon);
            EXPECT_LT((euler - eulerYzx).Length(), Quaternion::kQuaternionEpsilon);
            EXPECT_LT((euler - eulerZxy).Length(), Quaternion::kQuaternionEpsilon);
            EXPECT_LT((euler - eulerZyx).Length(), Quaternion::kQuaternionEpsilon);
        };

        testBijectiveTransform(0, 0, 0);
        testBijectiveTransform(-0.2f, 0.23f, -0.41f);
    }
}