/**
 * @file
 * @author Max Godefroy
 * @date 21/03/2025.
 */

#pragma once

#include "EASTL/string.h"
#include "KryneEngine/Core/Math/Matrix44.hpp"
#include "KryneEngine/Core/Math/Quaternion.hpp"
#include "KryneEngine/Core/Math/RotationConversion.hpp"

namespace KryneEngine::Math
{
    template<Matrix44Type Mat44, Vector3Type Vec3, QuaternionType Quat>
    Mat44 ComputeTransformMatrix(const Vec3& _position, const Quat& _rotation, const Vec3& _scale)
    {
        auto r = ToMatrix33<Matrix33Base<typename Mat44::ScalarType, Mat44::kRowMajorLayout>>(_rotation);
        return Mat44 {
            r.Get(0, 0) * _scale.x, r.Get(0, 1) * _scale.y, r.Get(0, 2) * _scale.z, _position.x,
            r.Get(1, 0) * _scale.x, r.Get(1, 1) * _scale.y, r.Get(1, 2) * _scale.z, _position.y,
            r.Get(2, 0) * _scale.x, r.Get(2, 1) * _scale.y, r.Get(2, 2) * _scale.z, _position.z,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    }

    template<Matrix44Type Mat44, Vector3Type Vec3>
    Mat44& SetTranslation(Mat44& _matrix, const Vec3& _position)
    {
        _matrix.Get(0, 3) = _position.x;
        _matrix.Get(1, 3) = _position.y;
        _matrix.Get(2, 3) = _position.z;
        return _matrix;
    }
}