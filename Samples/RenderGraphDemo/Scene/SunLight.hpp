/**
 * @file
 * @author Max Godefroy
 * @date 29/03/2025.
 */

#pragma once

#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class SunLight
    {
    public:
        SunLight();

        void Process();

        [[nodiscard]] const float3& GetDirection() const { return m_direction; }
        [[nodiscard]] float3 GetDiffuse() const { return m_color * m_intensity; }

    private:
        float m_theta = 0;
        float m_phi = 0;
        float3 m_direction;
        float3 m_color;
        float m_intensity;
        bool m_windowOpen = true;
    };
}
