/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>

namespace KryneEngine::Samples
{
    /**
     * @brief Owns a cascaded shadow map: a depth-only `Texture2DArray`, one slice per cascade,
     * plus the per-cascade light view/projection matrices fit to slices of the main camera's
     * view frustum each frame.
     *
     * @details
     * This class only owns the shadow-caster depth *resource* and the matrix/constants math -
     * it does not know how to draw scene geometry into it. Callers are expected to, once per
     * frame: call #UpdateCascades, then render their shadow-casters into each #GetCascadeRtv
     * using the matching #GetCascadeViewMatrix/#GetCascadeProjectionMatrix (e.g. via a
     * `PassDispatcher` bound to a depth-only pass type), the same way the main camera drives a
     * GBuffer pass. #GetConstantsBufferView exposes the uploaded per-cascade matrices/splits to
     * whichever compute pass resolves the shadow test (see `DeferredShadowPass`).
     *
     * Not registered with any render graph itself: like `SkyAmbientPass`'s output buffer, the raw
     * texture/views returned here are meant to be registered into a `Modules::RenderGraph::Registry`
     * by the caller, once, right after #Initialize.
     */
    class CascadedShadowMap
    {
    public:
        static constexpr u32 kMaxCascades = 4;

        struct ConstantsBuffer
        {
            float4x4 m_cascadeViewProj[kMaxCascades];
            float4 m_cascadeSplitDepths { 0.f };    // view-space far distance of each cascade, one per component
            float4 m_cascadeTexelWorldSize { 0.f }; // world units covered by one shadow-map texel, one per cascade
            float4 m_cascadeDepthRangeInv { 0.f };  // 1 / (far - near) of each cascade's light-space depth range
            // Light forward direction, the same across every cascade (one directional light): used
            // for the normal-offset bias's N.L term in the shadow resolve shader. .w unused, kept
            // as float4 for constant-buffer alignment.
            float3 m_lightForward { 0.f };
            // Width of the dithered cascade-transition band, as a fraction of the split distance
            // it straddles (e.g. 0.1 = the band spans the last/first 10% of view-space depth on
            // either side of the split). Shared across every cascade, so a single float suffices -
            // reuses what would otherwise be m_lightForward's trailing alignment padding.
            float m_cascadeBlendBandFraction = 0.1f;
            float m_lightSizeUv = 0;    // PCSS light size, as a fraction of a cascade's shadow-map width
            u32 m_cascadeCount = 0;
            float m_shadowBiasConstantTexels = 1.f; // Base normal-offset bias, in shadow-map texels of the receiving cascade
            float m_shadowBiasSlopeScale = 3.f;     // Extra normal-offset added at grazing angles (scaled by 1 - N.L)
        };
        static_assert(sizeof(ConstantsBuffer) % 16 == 0, "ConstantsBuffer must match its HLSL counterpart's alignment");

        explicit CascadedShadowMap(AllocatorInstance _allocator);
        ~CascadedShadowMap();

        // Creates the cascade depth array texture, its per-slice RTVs, the whole-array SRV, and
        // the per-frame-context constants buffer. _cascadeCount must be in [1, kMaxCascades].
        void Initialize(
            GraphicsContext* _graphicsContext,
            u32 _cascadeCount,
            u32 _resolution,
            TextureFormat _format = TextureFormat::D16);

        void Debug();

        // Recomputes every cascade's light view/projection matrices (fit to slices of the main
        // camera's frustum between _cameraNear and _maxShadowDistance) and uploads them, along
        // with the per-cascade split distances, to this frame's constants buffer. Must be called
        // once per rendered frame, before both the cascade depth passes and the shadow-resolve
        // pass execute.
        // _cameraViewRotation/_cameraViewTranslation are the same values OrbitCamera exposes via
        // GetViewRotation()/GetViewTranslation() (i.e. posView = _cameraViewRotation * posWorld +
        // _cameraViewTranslation); _cameraTanHalfFovY the same convention as
        // `std::tan(OrbitCamera::GetFov() * 0.5f)`.
        void UpdateCascades(
            GraphicsContext* _graphicsContext,
            TransferCommandEncoderHandle _transferEncoder,
            const Math::Quaternion& _cameraViewRotation,
            const float3& _cameraViewTranslation,
            float _cameraTanHalfFovY,
            float _cameraAspectRatio,
            float _cameraNear,
            float _maxShadowDistance,
            const float3& _lightDirection);

        [[nodiscard]] u32 GetCascadeCount() const { return m_cascadeCount; }
        [[nodiscard]] u32 GetResolution() const { return m_resolution; }
        [[nodiscard]] TextureFormat GetFormat() const { return m_format; }
        [[nodiscard]] const float4x4& GetCascadeViewMatrix(const u32 _cascadeIndex) const { return m_cascades[_cascadeIndex].m_viewMatrix; }
        [[nodiscard]] const float4x4& GetCascadeProjectionMatrix(const u32 _cascadeIndex) const { return m_cascades[_cascadeIndex].m_projectionMatrix; }
        [[nodiscard]] RenderTargetViewHandle GetCascadeRtv(const u32 _cascadeIndex) const { return m_cascades[_cascadeIndex].m_rtv; }
        [[nodiscard]] TextureHandle GetShadowArrayTexture() const { return m_shadowArrayTexture; }
        [[nodiscard]] TextureViewHandle GetShadowArrayView() const { return m_shadowArrayView; }
        [[nodiscard]] BufferViewHandle GetConstantsBufferView(const u8 _frameIndex) const { return m_constantsBufferViews[_frameIndex]; }

    private:
        struct Cascade
        {
            float4x4 m_viewMatrix;
            float4x4 m_projectionMatrix;
            float m_splitFar = 0.f;
            float m_texelWorldSize = 0.f;
            float m_depthRangeInv = 0.f;
            RenderTargetViewHandle m_rtv {};
        };

        AllocatorInstance m_allocator;

        u32 m_cascadeCount = 0;
        u32 m_resolution = 0;
        TextureFormat m_format = TextureFormat::D16;

        TextureHandle m_shadowArrayTexture {};
        TextureViewHandle m_shadowArrayView {};
        Cascade m_cascades[kMaxCascades];
        float m_lightSizeUv = 0.05f;
        float m_shadowBiasConstantTexels = 1.f;
        float m_shadowBiasSlopeScale = 3.f;
        float m_cascadeBlendBandFraction = 0.1f;

        Modules::GraphicsUtils::DynamicBuffer m_constantsBuffer;
        BufferViewHandle* m_constantsBufferViews = nullptr;
    };
}
