/**
 * @file
 * @author Max Godefroy
 * @date 15/05/2025.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FrameData.hlsl"
#include "TorusKnotSDF.hlsl"

vkBinding(0, 0) ConstantBuffer<FrameData> SceneConstants: register(b0, space0);

vkBinding(0, 1) Texture2D<float4> GBufferAlbedo: register(t0, space1);
vkBinding(1, 1) Texture2D<float4> GBufferNormal: register(t1, space1);
vkBinding(2, 1) Texture2D<float> GBufferDepth: register(t2, space1);
vkBinding(3, 1) RWTexture2D<float4> GBufferAmbient: register(u0, space1);

[numthreads(8, 8, 1)]
void GiMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoordinates = id.xy;
    const uint2 resolution = uint2(SceneConstants.m_screenResolution);

    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    GBufferAmbient[pixelCoordinates] = float4(0, 0, 0, 0);

    const float depthSs = GBufferDepth.Load(int3(pixelCoordinates, 0));

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates.xy, resolution);
    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov
    );

    const float4 vsToWsQuaternion = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, - SceneConstants.m_cameraTranslation);

    const float3 positionM = mul(float4(positionW, 1.0f), SceneConstants.m_torusKnotInverseWorldMatrix).xyz;
    const float3 directionM = normalize(mul(float4(cameraW, 0.0f), SceneConstants.m_torusKnotInverseWorldMatrix).xyz);

    const float minDist = SceneConstants.m_cameraTranslation.y - 3;
    const float maxDist = SceneConstants.m_cameraTranslation.y + 3;
    const uint maxSteps = 128;

    float t = minDist;
    bool hit = false;
    uint i = 0;
    for (; i < maxSteps; i++)
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
            hit = true;
            break;
        }
        t += distance;
        if (t > maxDist)
        {
            break;
        }
    }

    if (!hit)
    {
        return;
    }

    const float3 hitPositionM = positionM + t * directionM;

    float3 color = float3(1, 1, 1);
    if (depthSs != 1)
    {
        const float depthV = SceneConstants.m_depthLinearizationConstants.x / (depthSs + SceneConstants.m_depthLinearizationConstants.y);

        const float3 hitPositionW = mul(float4(hitPositionM, 1.f), SceneConstants.m_torusWorldMatrix).xyz;
        const float3 hitPositionV = Quaternion::Apply(SceneConstants.m_cameraQuaternion, hitPositionW) + SceneConstants.m_cameraTranslation;

        if (depthV < hitPositionW.y)
        {
            return;
        }
        color = lerp(color, GBufferAlbedo.Load(int3(pixelCoordinates, 0)).rgb * GBufferNormal.Load(int3(pixelCoordinates, 0)).rgb, 0.2);
    }

    color = lerp(color, (float(i) / float(maxSteps - 1)).xxx, 0.99f);

    GBufferAmbient[pixelCoordinates] = float4(color, 1);
}