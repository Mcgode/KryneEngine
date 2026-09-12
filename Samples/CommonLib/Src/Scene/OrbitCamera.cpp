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

#include <atomic>

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

    void OrbitCamera::UpdatePose()
    {
        if (InputManager::Get().IsMouseButtonPressed(MouseInputButton::Right))
        {
            const float2 delta = InputManager::Get().GetCursorDelta();

            m_theta += delta.x * 0.1f;

            m_phi += delta.y * 0.1f;
            m_phi = eastl::clamp(m_phi, -90.0f, 90.0f);
        }

        Math::Quaternion yaw, pitch;
        yaw.FromAxisAngle(Math::UpVector(), m_theta * M_PI / 180.0f);
        pitch.FromAxisAngle(Math::RightVector(), m_phi * M_PI / 180.0f);

        const Pose pose {
            .m_translation = Math::ForwardVector() * m_distance - m_focusPosition,
            .m_rotation = pitch * yaw,
        };

        const u8 active = std::atomic_ref(m_activePoseSlot).load(std::memory_order::relaxed);
        const u8 back = 1 - active;
        m_poseSlots[back] = pose;
        std::atomic_ref(m_activePoseSlot).store(back, std::memory_order::release);
    }

    void OrbitCamera::BuildMatrices(const float3& _translation, const Math::Quaternion& _rotation)
    {
        auto viewMatrix = ToMatrix44<float4x4_simd>(ToMatrix33<float3x3>(_rotation));
        Math::SetTranslation(viewMatrix, _translation);

        const auto projectionMatrix = Math::PerspectiveProjection<float4x4_simd>(
            m_fov,
            m_aspectRatio,
            m_near,
            INFINITY,
            true);

        m_viewMatrix = float4x4(viewMatrix);
        m_projectionMatrix = float4x4(projectionMatrix);
        m_projectionViewMatrix = float4x4(projectionMatrix * viewMatrix);

        m_depthLinearizeConstants = Math::ComputePerspectiveDepthLinearizationConstants(m_near, INFINITY, true);

        m_renderTranslation = _translation;
        m_renderRotation = _rotation;
    }

    void OrbitCamera::SyncRenderTransform(const float _alpha)
    {
        const u8 active = std::atomic_ref(m_activePoseSlot).load(std::memory_order::acquire);
        const Pose current = m_poseSlots[active];

        if (!m_hasPreviousRenderPose)
        {
            m_previousRenderPose = current;
            m_hasPreviousRenderPose = true;
        }

        Math::Quaternion rotation = m_previousRenderPose.m_rotation;
        rotation.Slerp(current.m_rotation, _alpha);
        const float3 translation = m_previousRenderPose.m_translation
            + (current.m_translation - m_previousRenderPose.m_translation) * _alpha;

        BuildMatrices(translation, rotation);

        m_previousRenderPose = current;
    }

    void OrbitCamera::Process()
    {
        UpdatePose();
        SyncRenderTransform(1.f);
    }
}
