/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Rendering/Shadows/CascadedShadowMap.hpp"

#include "Samples/CommonLib/CascadeConstants.h"

#include <EASTL/numeric.h>
#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace KryneEngine::Samples
{
    CascadedShadowMap::CascadedShadowMap(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_constantsBuffer(_allocator)
    {}

    CascadedShadowMap::~CascadedShadowMap()
    {
        if (m_constantsBufferViews != nullptr)
            m_allocator.deallocate(m_constantsBufferViews);
    }

    void CascadedShadowMap::Initialize(
        GraphicsContext* _graphicsContext,
        const u32 _cascadeCount,
        const u32 _resolution,
        const TextureFormat _format)
    {
        KE_ASSERT_MSG(_cascadeCount >= 1 && _cascadeCount <= kMaxCascades, "Invalid cascade count");

        m_cascadeCount = _cascadeCount;
        m_resolution = _resolution;
        m_format = _format;

        m_shadowArrayTexture = _graphicsContext->CreateTexture({
            .m_desc = {
                .m_dimensions = { _resolution, _resolution, 1 },
                .m_format = _format,
                .m_arraySize = static_cast<u16>(_cascadeCount),
                .m_type = TextureTypes::Array2D,
                .m_planes = TexturePlane::Depth,
#if !defined(KE_FINAL)
                .m_debugName = "Cascaded shadow map",
#endif
            },
            .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage
                | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
        });

        m_shadowArrayView = _graphicsContext->CreateTextureView({
            .m_texture = m_shadowArrayTexture,
            .m_arrayStart = 0,
            .m_arrayRange = static_cast<u16>(_cascadeCount),
            .m_format = _format,
            .m_viewType = TextureTypes::Array2D,
            .m_plane = TexturePlane::Depth,
            .m_accessType = TextureViewAccessType::Read,
#if !defined(KE_FINAL)
            .m_debugName = "Cascaded shadow map view",
#endif
        });

        for (u32 i = 0; i < _cascadeCount; i++)
        {
            char name[64];
            snprintf(name, sizeof(name), "Cascade %u RTV", i);
            m_cascades[i].m_rtv = _graphicsContext->CreateRenderTargetView({
                .m_texture = m_shadowArrayTexture,
                .m_format = _format,
                .m_type = TextureTypes::Array2D,
                .m_plane = TexturePlane::Depth,
                .m_arrayRangeStart = static_cast<u16>(i),
                .m_arrayRangeSize = 1,
#if !defined(KE_FINAL)
                .m_debugName = name,
#endif
            });
        }

        const u8 frameContextCount = _graphicsContext->GetFrameContextCount();
        m_constantsBuffer.Init(
            _graphicsContext,
            {
                .m_desc = {
                    .m_size = sizeof(CascadeConstants),
#if !defined(KE_FINAL)
                    .m_debugName = "CSM constants",
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer
                    | MemoryUsage::ConstantBuffer,
            },
            frameContextCount);

        m_constantsBufferViews = m_allocator.Allocate<BufferViewHandle>(frameContextCount);
        for (u32 i = 0; i < frameContextCount; i++)
        {
            char name[64];
            snprintf(name, sizeof(name), "CSM constants view %u", i);
            m_constantsBufferViews[i] = _graphicsContext->CreateBufferView({
                .m_buffer = m_constantsBuffer.GetBuffer(i),
                .m_size = sizeof(CascadeConstants),
                .m_stride = sizeof(CascadeConstants),
                .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                .m_debugName = name,
#endif
            });
        }
    }

    void CascadedShadowMap::Debug(bool* _windowOpen)
    {
        if (ImGui::Begin("Cascaded Shadow Map", _windowOpen))
        {
            ImGui::DragFloat("Normal offset bias (texels)", &m_shadowBiasConstantTexels, 0.1, 0, 0, "%.2f tx");
            ImGui::DragFloat("Normal offset grazing-angle scale", &m_shadowBiasSlopeScale, 0.1, 0, 0, "%.2f tx");
            ImGui::SliderFloat("Cascade blend band (fraction)", &m_cascadeBlendBandFraction, 0.f, 0.5f);

            ImGui::Separator();

            ImGui::Combo("Shadow technique", &m_shadowTechnique, "PCSS\0DPCF\0");

            switch (m_shadowTechnique)
            {
                case SHADOW_TECHNIQUE_PCSS:
                    ImGui::DragFloat("Light angle half tan", &m_pcssTanHalfLightAngle, 0.001f);
                    ImGui::SliderFloat("Min penumbra (texels)", &m_pcssMinPenumbraTexels, 0.f, 16.f, "%.1f tx");
                    ImGui::SliderFloat("Max penumbra (texels)", &m_pcssMaxPenumbraTexels, 1.f, 64.f, "%.1f tx");
                    m_pcssMaxPenumbraTexels = std::max(m_pcssMaxPenumbraTexels, m_pcssMinPenumbraTexels);
                    ImGui::SliderInt("Blocker search taps", &m_pcssBlockerSearchTaps, 1, 128, "%d", ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderInt("Filter taps", &m_pcssFilterTaps, 1, 128, "%d", ImGuiSliderFlags_Logarithmic);
                    break;
                case SHADOW_TECHNIQUE_DPCF:
                    ImGui::SliderFloat("DPCF kernel (texels)", &m_dpcfKernelTexels, 0.f, 32.f, "%.1f tx");
                    ImGui::SliderInt("DPCF taps (x4 samples)", &m_dpcfTaps, 1, 128, "%d", ImGuiSliderFlags_Logarithmic);
                    break;
                default:
                    break;
            }
        }
        ImGui::End();
    }

    void CascadedShadowMap::UpdateCascades(
        GraphicsContext* _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const Math::Quaternion& _cameraViewRotation,
        const float3& _cameraViewTranslation,
        const float _cameraTanHalfFovY,
        const float _cameraAspectRatio,
        const float _cameraNear,
        const float _maxShadowDistance,
        const float3& _lightDirection)
    {
        Math::Quaternion invRotation = _cameraViewRotation;
        invRotation.Conjugate();

        // Practical split scheme: blend of logarithmic and uniform splits between the camera's
        // near plane and the configured max shadow distance. Log-only splits make near cascades
        // very thin (great resolution close up, but a seam falls right where detail matters most
        // for ground contact shadows); uniform-only splits waste resolution far away. Blending
        // the two is the standard middle ground.
        constexpr float kLambda = 0.6f;
        float splitsNear[kMaxCascades];
        float splitsFar[kMaxCascades];
        for (u32 i = 0; i < m_cascadeCount; i++)
        {
            const float p = static_cast<float>(i + 1) / static_cast<float>(m_cascadeCount);
            const float logSplit = _cameraNear * std::pow(_maxShadowDistance / _cameraNear, p);
            const float uniformSplit = _cameraNear + (_maxShadowDistance - _cameraNear) * p;
            splitsFar[i] = eastl::lerp(uniformSplit, logSplit, kLambda);
            splitsNear[i] = (i == 0) ? _cameraNear : splitsFar[i - 1];
        }

        // Light-local basis: forward is the direction the light travels (toward the scene); right/
        // up are picked from an arbitrary stable hint vector, falling back to avoid the degenerate
        // near-parallel case (sun near-straight up or down).
        float3 lightForward = _lightDirection;
        lightForward.Normalize();
        float3 lightUpHint = Math::UpVector();
        if (std::abs(float3::Dot(lightForward, lightUpHint)) > 0.99f)
        {
            lightUpHint = Math::RightVector();
        }
        // Operand order matters here: it must reproduce the same handedness relation as the
        // engine's own canonical basis (cross(RightVector(), ForwardVector()) == UpVector()) or
        // the resulting light basis comes out mirrored. That mirroring doesn't affect where
        // shadow-map samples land (the resolve pass derives its UV from the same matrix), but it
        // does flip triangle winding after the light's view/projection transform - which flips
        // which faces the shadow PSO's back-face culling keeps, silently turning "render the
        // faces toward the light" into "render the faces away from the light".
        float3 lightRight = float3::CrossProduct(lightForward, lightUpHint);
        lightRight.Normalize();
        float3 lightUp = float3::CrossProduct(lightRight, lightForward);
        lightUp.Normalize();

        // Extends a cascade's near bound backward so casters just outside the visible frustum
        // slice (but still between the light and it) aren't culled out of the shadow map. A fixed
        // margin is a simplification - a tighter fit would derive this from the actual scene
        // bounds instead.
        constexpr float kNearPadding = 50.f;

        for (u32 i = 0; i < m_cascadeCount; i++)
        {
            // The camera's view space here uses OrbitCamera/PerspectiveProjection's axis
            // convention (Z-up default coordinate system): index 0 = right, index 1 = forward/
            // depth, index 2 = up. Frustum corners are computed directly in that local space, then
            // rotated into world space via the inverse view rotation (posWorld = conjugate(q) *
            // (posView - t), solved from posView = q * posWorld + t).
            float3 corners[8];
            u32 cornerCount = 0;
            const float depths[2] = { splitsNear[i], splitsFar[i] };
            for (const float depth : depths)
            {
                const float halfHeight = depth * _cameraTanHalfFovY;
                const float halfWidth = halfHeight * _cameraAspectRatio;
                const float signs[2] = { -1.f, 1.f };
                for (const float sx : signs)
                {
                    for (const float sz : signs)
                    {
                        const float3 localPoint { sx * halfWidth, depth, sz * halfHeight };
                        corners[cornerCount++] = invRotation.ApplyTo(localPoint - _cameraViewTranslation);
                    }
                }
            }

            // Bound the slice with a sphere (centred on the frustum slice's view axis, at the
            // midpoint between its near and far splits) rather than a tight per-axis AABB of the
            // corners. A sphere's radius depends only on the slice's near/far/fov/aspect, not on
            // which way the camera is currently facing, so the cascade's world-space size stays
            // constant as the camera rotates - a tight AABB's extents change with orientation,
            // which is what causes shadow-map texels to visibly swim/shimmer over the scene when
            // just turning the camera in place, independent of any actual camera movement.
            const float3 sphereCenter = invRotation.ApplyTo(
                float3 { 0.f, (splitsNear[i] + splitsFar[i]) * 0.5f, 0.f } - _cameraViewTranslation);
            float radius = 0.f;
            for (const float3& corner : corners)
            {
                const float3 delta = corner - sphereCenter;
                const float distance2 = float3::Dot(delta, delta);
                if (distance2 > radius * radius)
                    radius = std::sqrt(distance2);
            }

            float centerX = float3::Dot(sphereCenter, lightRight);
            float centerY = float3::Dot(sphereCenter, lightUp);
            const float centerZ = float3::Dot(sphereCenter, lightForward);

            // Snap the sphere centre to whole shadow-map texels, in light space. Without this,
            // the fitted bounds - and so where each world point lands within a texel - shift by a
            // sub-texel amount every frame as the camera moves, which is what causes shimmering
            // even when the sphere-bound radius above is otherwise stable.
            const float texelWorldSize = (2.f * radius) / static_cast<float>(m_resolution);
            if (texelWorldSize > 0.f)
            {
                centerX = std::floor(centerX / texelWorldSize) * texelWorldSize;
                centerY = std::floor(centerY / texelWorldSize) * texelWorldSize;
            }

            const float minX = centerX - radius, maxX = centerX + radius;
            const float minY = centerY - radius, maxY = centerY + radius;
            const float minZ = centerZ - radius - kNearPadding, maxZ = centerZ + radius;

            Cascade& cascade = m_cascades[i];
            cascade.m_splitFar = splitsFar[i];
            cascade.m_texelWorldSize = texelWorldSize;
            cascade.m_depthRangeInv = 1.f / (maxZ - minZ);

            // Light view matrix: pure rotation into the light's local basis, no translation - the
            // bounds above are already expressed relative to the world origin. Row placement
            // matches Math::PerspectiveProjection/OrthographicProjection's own view-space axis
            // convention (right is always row 0; up/forward fall on whichever of rows 1/2 the
            // default coordinate system calls "up" vs "forward" - row 2/row 1 respectively for
            // the engine's default right-handed Z-up), so the two compose correctly.
            cascade.m_viewMatrix = float4x4 {
                lightRight.x,   lightRight.y,   lightRight.z,   0.f,
                lightForward.x, lightForward.y, lightForward.z, 0.f,
                lightUp.x,      lightUp.y,      lightUp.z,      0.f,
                0.f,            0.f,            0.f,            1.f,
            };

            cascade.m_projectionMatrix = Math::OrthographicProjection<float4x4>(
                minX, maxX, minY, maxY, minZ, maxZ, false);
        }

        auto* constants = static_cast<CascadeConstants*>(
            m_constantsBuffer.Map(_graphicsContext, _graphicsContext->GetCurrentFrameContextIndex()));

        for (u32 i = 0; i < m_cascadeCount; i++)
        {
            constants->m_cascadeViewProj[i] = m_cascades[i].m_projectionMatrix * m_cascades[i].m_viewMatrix;
            constants->m_cascadeSplitDepths[i] = m_cascades[i].m_splitFar;
            constants->m_cascadeTexelWorldSize[i] = m_cascades[i].m_texelWorldSize;
            constants->m_cascadeDepthRangeInv[i] = m_cascades[i].m_depthRangeInv;
        }
        constants->m_lightForward = lightForward;
        constants->m_cascadeBlendBandFraction = m_cascadeBlendBandFraction;

        constants->m_cascadeCount = m_cascadeCount;
        constants->m_shadowBiasConstantTexels = m_shadowBiasConstantTexels;
        constants->m_shadowBiasSlopeScale = m_shadowBiasSlopeScale;
        constants->m_shadowTechnique = static_cast<u32>(m_shadowTechnique);

        constants->m_pcssTanHalfLightAngle = m_pcssTanHalfLightAngle;
        constants->m_dpcfKernelTexels = m_dpcfKernelTexels;
        constants->m_pcssMinPenumbraTexels = m_pcssMinPenumbraTexels;
        constants->m_pcssMaxPenumbraTexels = m_pcssMaxPenumbraTexels;
        constants->m_pcssBlockerSearchTaps = static_cast<u32>(m_pcssBlockerSearchTaps);
        constants->m_pcssFilterTaps = static_cast<u32>(m_pcssFilterTaps);
        constants->m_dpcfTaps = static_cast<u32>(m_dpcfTaps);

        m_constantsBuffer.Unmap(_graphicsContext);
        m_constantsBuffer.PrepareBuffers(
            _graphicsContext,
            _transferEncoder,
            BarrierAccessFlags::ConstantBuffer,
            _graphicsContext->GetCurrentFrameContextIndex());
    }
}
