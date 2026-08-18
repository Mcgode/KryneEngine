/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2025.
 */

#pragma once

#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class OrbitCamera
    {
    public:
        OrbitCamera(InputManager* _inputManager, float _aspectRatio);
        ~OrbitCamera();

        void Process();

        [[nodiscard]] const float& GetFov() const { return m_fov; }
        [[nodiscard]] const float2& GetDepthLinearizeConstants() const { return m_depthLinearizeConstants; }
        [[nodiscard]] const float3& GetViewTranslation() const { return m_viewTranslation; }
        [[nodiscard]] const Math::Quaternion& GetViewRotation() const { return m_viewRotation; }
        [[nodiscard]] const float4x4& GetProjectionViewMatrix() const { return m_projectionViewMatrix; }

    private:
        float3 m_focusPosition {};
        float m_distance = 10.0f;
        float m_theta = 0.0f;
        float m_phi = 0.0f;
        float m_near = 0.1f;
        float m_fov = 45.0f;
        float m_aspectRatio;

        bool m_matrixDirty = true;

        float2 m_depthLinearizeConstants {};
        float3 m_viewTranslation {};
        Math::Quaternion m_viewRotation {};
        float4x4 m_projectionViewMatrix {};

        u32 m_mouseButtonInputCallbackId;
        u32 m_cursorPositionCallbackId;
        u32 m_scrollCallbackId;

        bool m_orbiting = false;
        float2 m_lastCursorPosition {};
        float2 m_deltaPosition {};
    };
}
