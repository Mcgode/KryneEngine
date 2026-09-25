/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#if defined(_WIN32)
// M_PI and friends are opt-in on Windows. <corecrt_math_defines.h> is MSVC-only;
// mingw exposes them from <math.h> under the same _USE_MATH_DEFINES switch.
#   define _USE_MATH_DEFINES
#   include <math.h>
#endif
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <gtest/gtest.h>

using namespace KryneEngine::Math;

namespace KryneEngine::Tests::Math
{
    // Pack32/Pack64 quantize the three smallest components of the quaternion to 10 and 20 bits
    // respectively, so a round trip through Pack/Unpack cannot be bit-exact. These bounds are a
    // safety margin over the theoretical worst-case quantization error for each format.
    constexpr float kPack32Epsilon = 5e-3f;
    constexpr float kPack64Epsilon = 1e-5f;

    // A representative set of quaternions, chosen so that each of the four components takes a
    // turn being the dominant one (positive and negative), plus a couple of arbitrary rotations
    // where none of the components are trivially zero.
    const Quaternion kTestQuaternions[] = {
        Quaternion(),
        Quaternion().FromAxisAngle(float3(1, 0, 0), M_PI_2 * 0.8f),
        Quaternion().FromAxisAngle(float3(0, 1, 0), M_PI_2 * 0.6f),
        Quaternion().FromAxisAngle(float3(0, 0, 1), M_PI_2 * 0.2f),
        // w == 0, x dominant
        Quaternion().FromAxisAngle(float3(1, 0, 0), static_cast<float>(M_PI)),
        // w == 0, y dominant
        Quaternion().FromAxisAngle(float3(0, 1, 0), static_cast<float>(M_PI)),
        // w == 0, z dominant
        Quaternion().FromAxisAngle(float3(0, 0, 1), static_cast<float>(M_PI)),
        // Negative dominant component (w < 0)
        Quaternion().FromAxisAngle(float3(1, 0, 0), static_cast<float>(M_PI) * 1.8f),
        // Arbitrary axis, no trivially-zero component
        Quaternion().FromAxisAngle(float3(1, 1, 1).Normalized(), 0.5f),
        Quaternion().FromAxisAngle(float3(1, -2, 3).Normalized(), 2.7f),
        Quaternion().FromAxisAngle(float3(-1, 0.5f, -0.25f).Normalized(), -1.9f),
    };

    // Pack32/Pack64 normalize the packed quaternion so its dominant component is positive.
    // Since a quaternion `q` and its negation `-q` represent the same rotation, align signs
    // before comparing component-wise, and return the resulting per-component distance.
    float QuaternionDistance(const Quaternion& _expected, const Quaternion& _actual)
    {
        const float sign = Quaternion::Dot(_expected, _actual) < 0.0f ? -1.0f : 1.0f;

        const float dw = _expected.w - sign * _actual.w;
        const float dx = _expected.x - sign * _actual.x;
        const float dy = _expected.y - sign * _actual.y;
        const float dz = _expected.z - sign * _actual.z;

        return std::sqrt(dw * dw + dx * dx + dy * dy + dz * dz);
    }

    void ExpectQuaternionNear(const Quaternion& _expected, const Quaternion& _actual, float _epsilon)
    {
        const float sign = Quaternion::Dot(_expected, _actual) < 0.0f ? -1.0f : 1.0f;

        EXPECT_NEAR(_expected.w, sign * _actual.w, _epsilon);
        EXPECT_NEAR(_expected.x, sign * _actual.x, _epsilon);
        EXPECT_NEAR(_expected.y, sign * _actual.y, _epsilon);
        EXPECT_NEAR(_expected.z, sign * _actual.z, _epsilon);
    }

    TEST(Quaternion, Pack32UnpackRoundTrip)
    {
        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        for (const Quaternion& q : kTestQuaternions)
        {
            const u32 packed = q.Pack32();
            const Quaternion unpacked = Quaternion::Unpack32(packed);
            ExpectQuaternionNear(q, unpacked, kPack32Epsilon);
        }
    }

    TEST(Quaternion, Pack64UnpackRoundTrip)
    {
        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        for (const Quaternion& q : kTestQuaternions)
        {
            const u64 packed = q.Pack64();
            const Quaternion unpacked = Quaternion::Unpack64(packed);
            ExpectQuaternionNear(q, unpacked, kPack64Epsilon);
        }
    }

    TEST(Quaternion, Pack64IsMorePreciseThanPack32)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const Quaternion q = Quaternion().FromAxisAngle(float3(1, -2, 3).Normalized(), 2.7f);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Quaternion unpacked32 = Quaternion::Unpack32(q.Pack32());
        const Quaternion unpacked64 = Quaternion::Unpack64(q.Pack64());

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        EXPECT_LT(QuaternionDistance(q, unpacked64), QuaternionDistance(q, unpacked32));
    }
}
