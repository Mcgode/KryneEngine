/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine::Math
{
    struct Ray
    {
        float3 m_origin { 0.f };
        float3 m_direction { 0.f, 0.f, 1.f };

        Ray() = default;
        Ray(const float3& _origin, const float3& _direction): m_origin(_origin), m_direction(_direction) {}

        [[nodiscard]] float3 GetPoint(const float _distance) const
        {
            return m_origin + m_direction * _distance;
        }
    };
}
