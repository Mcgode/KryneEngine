/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Rendering/Shadows/CascadedShadowMap.hpp"

#include <EASTL/numeric.h>
#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <cfloat>
#include <cmath>

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
                    .m_size = sizeof(ConstantsBuffer),
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
                .m_size = sizeof(ConstantsBuffer),
                .m_stride = sizeof(ConstantsBuffer),
                .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                .m_debugName = name,
#endif
            });
        }
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
        float3 lightRight = float3::CrossProduct(lightUpHint, lightForward);
        lightRight.Normalize();
        float3 lightUp = float3::CrossProduct(lightForward, lightRight);
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

            float minX = FLT_MAX, maxX = -FLT_MAX;
            float minY = FLT_MAX, maxY = -FLT_MAX;
            float minZ = FLT_MAX, maxZ = -FLT_MAX;
            for (const float3& corner : corners)
            {
                const float x = float3::Dot(corner, lightRight);
                const float y = float3::Dot(corner, lightUp);
                const float z = float3::Dot(corner, lightForward);
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                if (z < minZ) minZ = z;
                if (z > maxZ) maxZ = z;
            }
            minZ -= kNearPadding;

            Cascade& cascade = m_cascades[i];
            cascade.m_splitFar = splitsFar[i];

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

        auto* constants = static_cast<ConstantsBuffer*>(
            m_constantsBuffer.Map(_graphicsContext, _graphicsContext->GetCurrentFrameContextIndex()));

        for (u32 i = 0; i < m_cascadeCount; i++)
        {
            constants->m_cascadeViewProj[i] = m_cascades[i].m_projectionMatrix * m_cascades[i].m_viewMatrix;
            constants->m_cascadeSplitDepths[i] = m_cascades[i].m_splitFar;
        }
        constants->m_cascadeCount = m_cascadeCount;

        m_constantsBuffer.Unmap(_graphicsContext);
        m_constantsBuffer.PrepareBuffers(
            _graphicsContext,
            _transferEncoder,
            BarrierAccessFlags::ConstantBuffer,
            _graphicsContext->GetCurrentFrameContextIndex());
    }
}
