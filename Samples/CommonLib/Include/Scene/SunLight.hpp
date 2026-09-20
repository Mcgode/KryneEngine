/**
 * @file
 * @author Max Godefroy
 * @date 29/03/2025.
 */

#pragma once

#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Samples
{
    class SunLight
    {
    public:
        SunLight();

        void Process();

        [[nodiscard]] const float3& GetDirection() const { return m_direction; }
        [[nodiscard]] float3 GetDiffuse() const { return m_color * m_intensity; }

        void SetTheta(const float _theta) { m_theta = _theta; }
        void SetPhi(const float _phi) { m_phi = _phi; }

    private:
        float m_theta = 0;
        float m_phi = 0;
        float3 m_direction;
        float3 m_color;
        float m_intensity;
        bool m_windowOpen = true;
    };
}
