/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2025.
 */

#include "Scene/OrbitCamera.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/RotationConversion.hpp>
#include <KryneEngine/Core/Math/Transform.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

namespace KryneEngine::Samples
{
    OrbitCamera::OrbitCamera(const float _aspectRatio)
        : m_aspectRatio(_aspectRatio)
    {
        // TODO: Retrieve scrolling for zooming (InputManager::GetScrollDelta)
    }

    OrbitCamera::~OrbitCamera() = default;

    void OrbitCamera::Process()
    {
        if (InputManager::Get().IsMouseButtonPressed(MouseInputButton::Right))
        {
            const float2 delta = InputManager::Get().GetCursorDelta();

            m_matrixDirty = true;

            m_theta += delta.x * 0.1f;

            m_phi += delta.y * 0.1f;
            m_phi = eastl::clamp(m_phi, -90.0f, 90.0f);
        }

        if (!m_matrixDirty)
        {
            return;
        }

        Math::Quaternion yaw, pitch;
        yaw.FromAxisAngle(Math::UpVector(), m_theta * M_PI / 180.0f);
        pitch.FromAxisAngle(Math::RightVector(), m_phi * M_PI / 180.0f);

        m_viewRotation = pitch * yaw;
        m_viewTranslation = Math::ForwardVector() * m_distance - m_focusPosition;

        auto viewMatrix = ToMatrix44<float4x4_simd>(ToMatrix33<float3x3>(m_viewRotation));
        Math::SetTranslation(viewMatrix, m_viewTranslation);

        m_projectionViewMatrix = float4x4(
            Math::PerspectiveProjection<float4x4_simd>(
                m_fov,
                m_aspectRatio,
                m_near,
                INFINITY,
                true
            ) * viewMatrix);

        m_depthLinearizeConstants = Math::ComputePerspectiveDepthLinearizationConstants(m_near, INFINITY, true);

        m_matrixDirty = false;
    }
}