/**
 * @file
 * @author Max Godefroy
 * @date 14/11/2026.
 */

#pragma once

#include <KryneEngine/Core/Math/Transform.hpp>
#include <box3d/math_functions.h>


namespace KryneEngine::Modules::Box3D
{
    KE_FORCEINLINE float3 ToMathVector(const b3Vec3& _b3Vec3)
    {
        return {
            _b3Vec3.x,
            _b3Vec3.y,
            _b3Vec3.z
        };
    }

    KE_FORCEINLINE Math::Quaternion ToMathQuaternion(const b3Quat& _b3Quat)
    {
        return {
            _b3Quat.s,
            _b3Quat.v.x,
            _b3Quat.v.y,
            _b3Quat.v.z
        };
    }

    KE_FORCEINLINE Math::Transform ToMathTransform(const b3Transform& _b3Transform)
    {
        return {
            .m_position = ToMathVector(_b3Transform.p),
            .m_rotation = ToMathQuaternion(_b3Transform.q)
        };
    }

    KE_FORCEINLINE b3Vec3 ToB3Vec(const float3& _float3)
    {
        return {
            .x = _float3.x,
            .y = _float3.y,
            .z = _float3.z
        };
    }
}