/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>

namespace KryneEngine::Tests::Math
{
    using namespace KryneEngine::Math;

    // All points below are expressed in "view space", using the same axis convention
    // PerspectiveProjection/OrthographicProjection expect for the engine's default coordinate
    // system (right-handed, Z-up): input index 0 = right, index 1 = forward/depth, index 2 = up.
    // The output clip-space point uses the usual (x, y, z, w) = (screen X, screen Y/up, depth, 1)
    // layout - i.e. output.y is "up" and output.z is depth, regardless of which input index feeds
    // each one.
    namespace Orthographic
    {
        // left = -2, right = 2, bottom = -3, top = 3, near = 2, far = 10
        constexpr float kLeft = -2.f, kRight = 2.f, kBottom = -3.f, kTop = 3.f, kNear = 2.f, kFar = 10.f;

        // Near-bottom-left corner, near-top-right corner, and the volume's centre.
        constexpr float4 nearBottomLeft { kLeft, kNear, kBottom, 1.f };
        constexpr float4 farTopRight { kRight, kFar, kTop, 1.f };
        constexpr float4 centre { 0.f, (kNear + kFar) * 0.5f, 0.f, 1.f };

        TEST(Projection, Orthographic_NonReversedDepth)
        {
            const auto proj = OrthographicProjection<float4x4>(kLeft, kRight, kBottom, kTop, kNear, kFar, false);

            const float4 near = proj * float4(nearBottomLeft);
            EXPECT_FLOAT_EQ(near.x, -1.f);
            EXPECT_FLOAT_EQ(near.y, -1.f);
            EXPECT_FLOAT_EQ(near.z, 0.f); // depth at the near plane
            EXPECT_FLOAT_EQ(near.w, 1.f);

            const float4 far = proj * float4(farTopRight);
            EXPECT_FLOAT_EQ(far.x, 1.f);
            EXPECT_FLOAT_EQ(far.y, 1.f);
            EXPECT_FLOAT_EQ(far.z, 1.f); // depth at the far plane
            EXPECT_FLOAT_EQ(far.w, 1.f);

            const float4 mid = proj * float4(centre);
            EXPECT_FLOAT_EQ(mid.x, 0.f);
            EXPECT_FLOAT_EQ(mid.y, 0.f);
            EXPECT_FLOAT_EQ(mid.z, 0.5f); // halfway between near and far
            EXPECT_FLOAT_EQ(mid.w, 1.f);
        }

        TEST(Projection, Orthographic_ReversedDepth)
        {
            const auto proj = OrthographicProjection<float4x4>(kLeft, kRight, kBottom, kTop, kNear, kFar, true);

            const float4 near = proj * float4(nearBottomLeft);
            EXPECT_FLOAT_EQ(near.x, -1.f);
            EXPECT_FLOAT_EQ(near.y, -1.f);
            EXPECT_FLOAT_EQ(near.z, 1.f); // reversed: depth 1 at the near plane

            const float4 far = proj * float4(farTopRight);
            EXPECT_FLOAT_EQ(far.x, 1.f);
            EXPECT_FLOAT_EQ(far.y, 1.f);
            EXPECT_FLOAT_EQ(far.z, 0.f); // reversed: depth 0 at the far plane

            const float4 mid = proj * float4(centre);
            EXPECT_FLOAT_EQ(mid.z, 0.5f);
        }

        TEST(Projection, Orthographic_AsymmetricBounds)
        {
            // Off-centre volume: left/right and bottom/top no longer straddle 0, so the matrix's
            // translation terms are actually exercised (the symmetric case above always leaves
            // them at 0).
            constexpr float left = 1.f, right = 5.f, bottom = -6.f, top = -2.f, near = 0.f, far = 4.f;
            const auto proj = OrthographicProjection<float4x4>(left, right, bottom, top, near, far, false);

            const float4 minCorner = proj * float4 { left, near, bottom, 1.f };
            EXPECT_FLOAT_EQ(minCorner.x, -1.f);
            EXPECT_FLOAT_EQ(minCorner.y, -1.f);
            EXPECT_FLOAT_EQ(minCorner.z, 0.f);

            const float4 maxCorner = proj * float4 { right, far, top, 1.f };
            EXPECT_FLOAT_EQ(maxCorner.x, 1.f);
            EXPECT_FLOAT_EQ(maxCorner.y, 1.f);
            EXPECT_FLOAT_EQ(maxCorner.z, 1.f);
        }

        TEST(Projection, Orthographic_NoPerspectiveDivide)
        {
            // Unlike PerspectiveProjection, w must stay 1 regardless of the input point, since
            // there is no perspective divide to encode.
            const auto proj = OrthographicProjection<float4x4>(kLeft, kRight, kBottom, kTop, kNear, kFar, false);

            constexpr float4 arbitraryPoint { 1.5f, 7.f, -1.2f, 1.f };
            const float4 result = proj * arbitraryPoint;
            EXPECT_FLOAT_EQ(result.w, 1.f);
        }
    }

    // Unlike the tests above, which hand-pick the "view space" input axis order for the default
    // coordinate system, these build points purely from each coordinate system's own
    // Right/Up/Forward vectors - i.e. a "world space" point for a camera/light sitting at the
    // origin with no rotation. That's the actual promise OrthographicProjection makes (bounds map
    // to the documented clip-space output regardless of CS), independent of - and so a genuine
    // check on - the projYCol/projZCol axis-selection it uses internally.
    namespace OrthographicCoordinateSystems
    {
        constexpr float kLeft = -2.f, kRight = 2.f, kBottom = -3.f, kTop = 3.f, kNear = 2.f, kFar = 10.f;

        template<CoordinateSystem CS>
        void TestOrthographic()
        {
            const auto proj = OrthographicProjection<float4x4, CS>(kLeft, kRight, kBottom, kTop, kNear, kFar, false);

            const auto worldPoint = [](const float _right, const float _up, const float _forward)
            {
                const float3 p = RightVector(CS) * _right + UpVector(CS) * _up + ForwardVector(CS) * _forward;
                return float4(p, 1.f);
            };

            const float4 nearBottomLeft = proj * worldPoint(kLeft, kBottom, kNear);
            EXPECT_FLOAT_EQ(nearBottomLeft.x, -1.f) << "Left bound";
            EXPECT_FLOAT_EQ(nearBottomLeft.y, -1.f) << "Bottom bound";
            EXPECT_FLOAT_EQ(nearBottomLeft.z, 0.f) << "Near plane";
            EXPECT_FLOAT_EQ(nearBottomLeft.w, 1.f);

            const float4 farTopRight = proj * worldPoint(kRight, kTop, kFar);
            EXPECT_FLOAT_EQ(farTopRight.x, 1.f) << "Right bound";
            EXPECT_FLOAT_EQ(farTopRight.y, 1.f) << "Top bound";
            EXPECT_FLOAT_EQ(farTopRight.z, 1.f) << "Far plane";

            const float4 mid = proj * worldPoint(0.f, 0.f, (kNear + kFar) * 0.5f);
            EXPECT_FLOAT_EQ(mid.x, 0.f);
            EXPECT_FLOAT_EQ(mid.y, 0.f);
            EXPECT_FLOAT_EQ(mid.z, 0.5f) << "Halfway between near and far";
        }

        TEST(Projection, Orthographic_CoordinateSystem_LeftHandedYUp)
        {
            TestOrthographic<CoordinateSystem::LeftHandedYUp>();
        }

        TEST(Projection, Orthographic_CoordinateSystem_LeftHandedZUp)
        {
            TestOrthographic<CoordinateSystem::LeftHandedZUp>();
        }

        TEST(Projection, Orthographic_CoordinateSystem_RightHandedYUp)
        {
            TestOrthographic<CoordinateSystem::RightHandedYUp>();
        }

        TEST(Projection, Orthographic_CoordinateSystem_RightHandedZUp)
        {
            TestOrthographic<CoordinateSystem::RightHandedZUp>();
        }
    }
}
