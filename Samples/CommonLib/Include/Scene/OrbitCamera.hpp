/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2025.
 */

#pragma once

#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>

namespace KryneEngine::Samples
{
    class OrbitCamera
    {
    public:
        explicit OrbitCamera(float _aspectRatio);
        ~OrbitCamera();

        // Reads input and updates the camera's orbit angles, then publishes the resulting
        // translation/rotation for SyncRenderTransform() to pick up. In a split game/render
        // thread architecture, this must only be called from the game/fixed-step thread - it's
        // the input-reading counterpart to whatever thread/lock InputManager::Update() runs
        // under, and must stay paired with it the same way.
        void UpdatePose();

        // Interpolates between the last two poses published by UpdatePose() by _alpha (expected
        // in [0, 1]) and rebuilds the view/projection matrices from the result. Must only be
        // called from the render/main thread, once per rendered frame.
        void SyncRenderTransform(float _alpha);

        // Convenience for callers with no separate game/render thread: UpdatePose() immediately
        // followed by SyncRenderTransform(1.f) (no interpolation, since there's only one pose).
        void Process();

        [[nodiscard]] const float& GetFov() const { return m_fov; }
        [[nodiscard]] const float& GetNear() const { return m_near; }
        [[nodiscard]] const float2& GetDepthLinearizeConstants() const { return m_depthLinearizeConstants; }
        [[nodiscard]] const float3& GetViewTranslation() const { return m_renderTranslation; }
        [[nodiscard]] const Math::Quaternion& GetViewRotation() const { return m_renderRotation; }
        [[nodiscard]] const float4x4& GetProjectionViewMatrix() const { return m_projectionViewMatrix; }
        [[nodiscard]] const float4x4& GetViewMatrix() const { return m_viewMatrix; }
        [[nodiscard]] const float4x4& GetProjectionMatrix() const { return m_projectionMatrix; }

    private:
        struct Pose
        {
            float3 m_translation {};
            Math::Quaternion m_rotation {};
        };

        void BuildMatrices(const float3& _translation, const Math::Quaternion& _rotation);

        float3 m_focusPosition {};
        float m_distance = 10.0f;
        float m_theta = 0.0f; // game-thread-only
        float m_phi = 0.0f;   // game-thread-only
        float m_near = 0.1f;
        float m_fov = 45.0f;
        float m_aspectRatio;

        // Published by UpdatePose() (game thread), consumed by SyncRenderTransform() (render
        // thread): a lock-free single-writer/single-reader double buffer, same idea as
        // WorldObjectSystem's own transform hand-off, but without a queue - there's only ever one
        // camera to publish, not a reusable set of entities.
        Pose m_poseSlots[2] {};
        u8 m_activePoseSlot = 0; // accessed via std::atomic_ref; see UpdatePose()/SyncRenderTransform()
        Pose m_previousRenderPose {}; // render-thread-owned, for interpolation
        bool m_hasPreviousRenderPose = false;

        // Render-thread-owned results; safe to read from the render thread via the getters above.
        float2 m_depthLinearizeConstants {};
        float3 m_renderTranslation {};
        Math::Quaternion m_renderRotation {};
        float4x4 m_projectionViewMatrix {};
        float4x4 m_viewMatrix {};
        float4x4 m_projectionMatrix {};
    };
}
