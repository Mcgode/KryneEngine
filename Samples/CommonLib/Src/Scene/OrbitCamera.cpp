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
    namespace
    {
        constexpr float kZoomSpeed = 0.1f;
        constexpr float kMinDistance = 1.0f;
        constexpr float kMaxDistance = 100.0f;
    }

    OrbitCamera::OrbitCamera(const float _aspectRatio)
        : m_aspectRatio(_aspectRatio)
    {
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

        const float scrollDelta = InputManager::Get().GetScrollDelta().y;
        m_distance -= scrollDelta * m_distance * kZoomSpeed;
        m_distance = eastl::clamp(m_distance, kMinDistance, kMaxDistance);

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

    Math::Ray OrbitCamera::GetPickingRay(const float2 _ndc, const bool _nearPlaneShift)
    {
        const u8 active = std::atomic_ref(m_activePoseSlot).load(std::memory_order::acquire);
        const Pose& pose = m_poseSlots[active];

        // Same local basis (right/forward/up) as the view-space direction reconstructed in
        // DeferredShading.hlsl, from screen-space NDC.
        const float halfFovTan = std::tan(m_fov * 0.5f);
        const float3 localDirection(
            _ndc.x * m_aspectRatio * halfFovTan,
            1.0f,
            _ndc.y * halfFovTan);

        Math::Quaternion viewToWorld = pose.m_rotation;
        viewToWorld.Conjugate();

        const float3 worldDirection = viewToWorld.ApplyTo(localDirection);

        return {
            viewToWorld.ApplyTo(pose.m_translation * -1.f)
                + (_nearPlaneShift ? worldDirection * m_near : float3(0.f)),
            worldDirection.Normalized(),
        };
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
