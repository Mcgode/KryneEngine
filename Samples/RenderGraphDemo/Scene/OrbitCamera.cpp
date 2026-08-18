/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2025.
 */

#include "OrbitCamera.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/RotationConversion.hpp>
#include <KryneEngine/Core/Math/Transform.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

namespace KryneEngine::Samples::RenderGraphDemo
{
    OrbitCamera::OrbitCamera(InputManager* _inputManager, float _aspectRatio)
        : m_aspectRatio(_aspectRatio)
    {
        m_mouseButtonInputCallbackId = _inputManager->RegisterMouseInputEventCallback(
            [this](const MouseInputEvent& _event)
            {
                switch (_event.m_mouseButton)
                {
                case MouseInputButton::Right:
                    if (_event.m_action == InputActionType::StartPress)
                        m_orbiting = true;
                    else if (_event.m_action == InputActionType::StopPress)
                        m_orbiting = false;
                    break;
                default:
                    break;
                }
            });

        m_cursorPositionCallbackId = _inputManager->RegisterCursorPosEventCallback(
            [this](float _x, float _y)
            {
                const float2 lastPosition = m_lastCursorPosition;
                m_lastCursorPosition = { _x, _y };
                m_deltaPosition = m_lastCursorPosition - lastPosition;
            });

        m_scrollCallbackId = _inputManager->RegisterScrollInputEventCallback(
            [this](float _x, float _y)
            {
                // TODO: Retrieve scrolling for zooming
            });
    }

    OrbitCamera::~OrbitCamera() = default;

    void OrbitCamera::Process()
    {
        if (m_orbiting)
        {
            m_matrixDirty = true;

            m_theta += m_deltaPosition.x * 0.1f;

            m_phi += m_deltaPosition.y * 0.1f;
            m_phi = eastl::clamp(m_phi, -90.0f, 90.0f);
        }

        // Reset delta position
        m_deltaPosition = { 0.0f, 0.0f };

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