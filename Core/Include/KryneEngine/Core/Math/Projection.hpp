/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Math/Matrix44.hpp"
#include "KryneEngine/Core/Math/Vector4.hpp"
#include "KryneEngine/Core/Math/CoordinateSystem.hpp"

namespace KryneEngine::Math
{
    template<Matrix44Type Mat44 = float4x4, CoordinateSystem CS = kDefaultCoordinateSystem>
    Mat44 PerspectiveProjection(
        const float _fov,
        const float _aspect,
        const float _near,
        const float _far,
        const bool _reversedDepth)
    {
        // Based on https://iolite-engine.com/blog_posts/reverse_z_cheatsheet

        const float e = 1.0f / tanf(_fov * 0.5f);
        const float n = _near;
        const float f = _far;

        Mat44 baseMat {
            e / _aspect, 0.f, 0.f, 0.f,
            0.f,         0.f, 0.f, 0.f,
            0.f,         0.f, 0.f, 0.f,
            0.f,         0.f, 0.f, 0.f,
        };

        constexpr size_t projYCol = IsZUp(CS) ? 2 : 1;

        baseMat.Get(1, projYCol) = e;

        constexpr size_t projZCol = IsZUp(CS) ? 1 : 2;
        constexpr bool thirdAxisForward = IsZUp(CS) ^ IsLeftHanded(CS);

        baseMat.Get(3, projZCol) = thirdAxisForward ? 1 : -1;

        if (_far == INFINITY)
        {
            {
                const float value = _reversedDepth ? 0 : 1;
                baseMat.Get(2, projZCol) = thirdAxisForward ? value : -value;
            }
            {
                baseMat.Get(2, 3) = _reversedDepth ? n : -n;
            }
        }
        else
        {
            {
                const float num = _reversedDepth ? -n : f;
                baseMat.Get(2, projZCol) = (thirdAxisForward ? num : -num) / (f - n);
            }
            {
                const float num = _reversedDepth ? f * n : -f * n;
                baseMat.Get(2, 3) = num / (f - n);
            }
        }

        return baseMat;
    }

    /**
     * @brief Builds an orthographic projection matrix, following the same view-space axis
     * convention as #PerspectiveProjection (right is always the first input axis; which of the
     * other two is "up" vs "forward" depends on `CS`), so it can be paired with the exact same
     * kind of plain rotation+translation view matrix.
     *
     * @param _left Left bound of the view volume, in view-space right-axis units.
     * @param _right Right bound of the view volume, in view-space right-axis units.
     * @param _bottom Bottom bound of the view volume, in view-space up-axis units.
     * @param _top Top bound of the view volume, in view-space up-axis units.
     * @param _near Near clipping plane distance, in view-space forward-axis units.
     * @param _far Far clipping plane distance, in view-space forward-axis units.
     * @param _reversedDepth A boolean indicating whether reversed depth (1 at near, 0 at far) is
     * being used; otherwise depth is 0 at near, 1 at far.
     */
    template<Matrix44Type Mat44 = float4x4, CoordinateSystem CS = kDefaultCoordinateSystem>
    Mat44 OrthographicProjection(
        const float _left,
        const float _right,
        const float _bottom,
        const float _top,
        const float _near,
        const float _far,
        const bool _reversedDepth)
    {
        Mat44 baseMat {
            0.f, 0.f, 0.f, 0.f,
            0.f, 0.f, 0.f, 0.f,
            0.f, 0.f, 0.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        constexpr size_t projYCol = IsZUp(CS) ? 2 : 1;
        constexpr size_t projZCol = IsZUp(CS) ? 1 : 2;
        constexpr bool thirdAxisForward = IsZUp(CS) ^ IsLeftHanded(CS);

        baseMat.Get(0, 0) = 2.f / (_right - _left);
        baseMat.Get(0, 3) = -(_right + _left) / (_right - _left);

        baseMat.Get(1, projYCol) = 2.f / (_top - _bottom);
        baseMat.Get(1, 3) = -(_top + _bottom) / (_top - _bottom);

        const float n = _near;
        const float f = _far;

        {
            const float value = (_reversedDepth ? -1.f : 1.f) / (f - n);
            baseMat.Get(2, projZCol) = thirdAxisForward ? value : -value;
        }
        // Unlike the scale term above, the offset here must not flip sign with thirdAxisForward:
        // it depends only on where near/far sit along the (already CS-signed) forward axis, not
        // on which physical direction that axis points in.
        baseMat.Get(2, 3) = _reversedDepth ? f / (f - n) : -n / (f - n);

        return baseMat;
    }

    /**
     * @brief Computes the depth linearization constants for a perspective projection.
     *
     * @details
     * This function calculates the constants used for linearizing the depth values in a
     * perspective projection matrix. The calculation differs based on whether reversed
     * depth is being used and whether the far plane is at infinity.
     *
     * To retrieve the view space depth, perform the following operation:
     * `const float depthVs = c.x / (depthSs + c.y)`
     *
     * @tparam Vec2 Specifies the return type, which must meet the `Vector2Type` concept.
     * Default type is `float2`.
     *
     * @param _near The near clipping plane distance of the perspective projection.
     * @param _far The far clipping plane distance of the perspective projection.
     * Can be set to infinity.
     * @param _reversedDepth A boolean indicating whether reversed depth (1 at near,
     * 0 at far) is being used.
     *
     * @return A vector of type `Vec2` containing the calculated depth linearization
     * constants. The first component represents a scale factor, and the second
     * component represents an offset for the linearization formula.
     */
    template <Vector2Type Vec2 = float2>
    Vec2 ComputePerspectiveDepthLinearizationConstants(
        const float _near,
        const float _far,
        const bool _reversedDepth)
    {
        if (_reversedDepth)
        {
            if (_far == INFINITY)
            {
                return Vec2 { _near, 0 };
            }
            else
            {
                return Vec2 { (_near * _far) / (_far - _near), _near / (_far - _near) };
            }
        }
        else
        {
            if (_far == INFINITY)
            {
                return Vec2 { -_near, -1 };
            }
            else
            {
                return Vec2 { -(_near * _far) / (_far - _near), -_far / (_far - _near) };
            }
        }
    }
}