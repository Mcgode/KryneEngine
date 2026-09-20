/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FullscreenPassConstants.hlsl"

#define CSM_MAX_CASCADES 4

// Keep in sync with `KryneEngine::Samples::CascadedShadowMap::ConstantsBuffer` (C++).
struct CascadeConstants
{
    float4x4 m_cascadeViewProj[CSM_MAX_CASCADES];
    float4 m_cascadeSplitDepths; // view-space far distance of each cascade, one per component
    float m_lightSizeUv;         // PCSS light size, as a fraction of a cascade's shadow-map width
    uint m_cascadeCount;
    float2 m_padding;
};

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants: register(b0, space0);
vkBinding(1, 0) ConstantBuffer<CascadeConstants> Cascades: register(b1, space0);
vkBinding(2, 0) Texture2D<float> GBufferDepth: register(t0, space0);
vkBinding(3, 0) Texture2DArray<float> ShadowCascades: register(t1, space0);
vkBinding(4, 0) RWTexture2D<float> DeferredShadows: register(u0, space0);

uint SelectCascade(const in float _viewDepth)
{
    for (uint i = 0; i < Cascades.m_cascadeCount - 1; i++)
    {
        if (_viewDepth < Cascades.m_cascadeSplitDepths[i])
        {
            return i;
        }
    }
    return Cascades.m_cascadeCount - 1;
}

// A sparse 3x3 grid (corners + edge midpoints + center) rather than a dense NxN kernel, reused
// both for the PCSS blocker search and (re-scaled) for the final PCF filter - cheap, at the cost
// of some blocker/penumbra under-sampling versus a denser kernel.
static const int2 kKernelOffsets[9] = {
    int2(-1, -1), int2(0, -1), int2(1, -1),
    int2(-1,  0), int2(0,  0), int2(1,  0),
    int2(-1,  1), int2(0,  1), int2(1,  1),
};

[numthreads(8, 8, 1)]
void DeferredShadowsMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoordinates = id.xy;
    const uint2 resolution = uint2(SceneConstants.m_screenResolution);

    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    const float depthSs = GBufferDepth.Load(int3(pixelCoordinates, 0));
    if (depthSs == 0.f)
    {
        // Reversed-Z: 0 is the far plane / background, nothing to shadow.
        return;
    }

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates, resolution);
    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov);

    const float depthV = SceneConstants.m_depthLinearizationConstants.x / (depthSs + SceneConstants.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;
    const float4 vsToWsQuaternion = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - SceneConstants.m_cameraTranslation);

    const uint cascadeIndex = SelectCascade(depthV);

    const float4 lightClip = mul(float4(positionW, 1.f), Cascades.m_cascadeViewProj[cascadeIndex]);
    const float2 shadowUv = float2(lightClip.x * 0.5f + 0.5f, 0.5f - lightClip.y * 0.5f);
    const float receiverDepth = lightClip.z;

    if (any(shadowUv < 0.f) || any(shadowUv > 1.f))
    {
        // Outside this cascade's fitted bounds (e.g. past the max shadow distance): no data,
        // default to unshadowed rather than an incorrect hard edge.
        DeferredShadows[pixelCoordinates] = 1.f;
        return;
    }

    uint shadowWidth, shadowHeight, shadowElements;
    ShadowCascades.GetDimensions(shadowWidth, shadowHeight, shadowElements);

    const int2 shadowPixel = int2(shadowUv * float2(shadowWidth, shadowHeight));

    // --- PCSS step 1: blocker search ---
    const float searchRadiusTexels = max(1.f, Cascades.m_lightSizeUv * float(shadowWidth));
    float blockerSum = 0.f;
    uint blockerCount = 0;
    for (uint i = 0; i < 9; i++)
    {
        const int2 samplePixel = shadowPixel + int2(round(float2(kKernelOffsets[i]) * searchRadiusTexels));
        const float sampleDepth = ShadowCascades.Load(int4(samplePixel, cascadeIndex, 0));
        if (sampleDepth < receiverDepth)
        {
            blockerSum += sampleDepth;
            blockerCount++;
        }
    }

    if (blockerCount == 0)
    {
        // No occluders found within the search radius: fully lit.
        DeferredShadows[pixelCoordinates] = 1.f;
        return;
    }

    const float avgBlockerDepth = blockerSum / float(blockerCount);

    // --- PCSS step 2: penumbra size + PCF ---
    // For a directional (orthographic) light, penumbra width scales with the light's angular
    // size times the receiver/blocker gap distance, not with a perspective receiver/blocker
    // depth ratio the way a point/area light's PCSS formula would use. m_lightSizeUv stands in
    // for that angular size, expressed directly in cascade-UV units as a simplification (it does
    // not separately account for each cascade's own world-space depth range) - tune it per scene
    // rather than treating it as physically exact.
    const float penumbraTexels = clamp(
        (receiverDepth - avgBlockerDepth) * Cascades.m_lightSizeUv * float(shadowWidth),
        1.f,
        float(shadowWidth) * 0.25f);

    float shadow = 0.f;
    for (uint j = 0; j < 9; j++)
    {
        const int2 samplePixel = shadowPixel + int2(round(float2(kKernelOffsets[j]) * penumbraTexels));
        const float sampleDepth = ShadowCascades.Load(int4(samplePixel, cascadeIndex, 0));
        shadow += sampleDepth >= receiverDepth ? 1.f : 0.f;
    }
    shadow /= 9.f;

    DeferredShadows[pixelCoordinates] = shadow;
}
