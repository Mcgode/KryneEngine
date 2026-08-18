/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FrameData.hlsl"
#include "TorusKnotSDF.hlsl"

vkBinding(0, 0) ConstantBuffer<FrameData> SceneConstants: register(b0, space0);

vkBinding(0, 1) Texture2D<float> GBufferDepth: register(t0, space1);
vkBinding(1, 1) RWTexture2D<float> DeferredShadows: register(u0, space1);

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
        return;
    }

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates.xy, resolution);

    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov
    );

    const float depthV = SceneConstants.m_depthLinearizationConstants.x / (depthSs + SceneConstants.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;
    const float4 vsToWsQuaternion = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - SceneConstants.m_cameraTranslation);
    const float3 directionW = -SceneConstants.m_sunLightDirection;

    const float3 positionM = mul(float4(positionW, 1.0f), SceneConstants.m_torusKnotInverseWorldMatrix).xyz;
    const float3 directionM = normalize(mul(float4(directionW, 0.0f), SceneConstants.m_torusKnotInverseWorldMatrix).xyz);

    const float minDist = 0.05f;
    const float maxDist = 10.f;
    const uint maxSteps = 32;

    float t = minDist;
    const float k = 8.f; // Soft shadow factor
    float shadow = 1.f;
    for (uint i = 0; i < maxSteps; i++)
    {
        const float3 position = positionM + t * directionM;
        const float distance = SdTorusKnot(
            position,
            SceneConstants.m_torusKnotRadius,
            SceneConstants.m_torusKnotTubeRadius,
            SceneConstants.m_torusKnotP,
            SceneConstants.m_torusKnotQ);

        if (distance <= 0.01f)
        {
            shadow = 0.f;
            break;
        }
        shadow = min(shadow, distance * k / t);
        t += distance;
    }

    DeferredShadows[pixelCoordinates] = shadow;
}