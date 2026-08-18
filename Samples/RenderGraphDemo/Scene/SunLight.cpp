/**
 * @file
 * @author Max Godefroy
 * @date 29/03/2025.
 */

#include "SunLight.hpp"

#include <imgui.h>

namespace KryneEngine::Samples::RenderGraphDemo
{
    SunLight::SunLight()
    {
        m_direction = float3(0, 0, -1.0f).Normalized();
        m_color = float3(1.0f, 1.0f, 1.0f);
        m_intensity = 1.0f;
    }

    void SunLight::Process()
    {
       if (ImGui::Begin("Sun Light", &m_windowOpen))
       {
           ImGui::SliderFloat("Theta", &m_theta, -180.0f, 180.0f);
           ImGui::SliderFloat("Phi", &m_phi, -90.0f, 90.0f);

           ImGui::ColorEdit3("Color", m_color.GetPtr());
           ImGui::DragFloat("Intensity", &m_intensity, 0.1f, 0.0f);

           ImGui::End();
       }

       m_direction = float3 {
           sinf(m_theta * 3.1415f / 180.f),
           cosf(m_theta * 3.1415f / 180.f) * sinf(m_phi * 3.1415f / 180.f),
           -cosf(m_theta * 3.1415f / 180.f) * cosf(m_phi * 3.1415f / 180.f)
       };
    }
}