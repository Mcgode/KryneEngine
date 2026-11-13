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

struct Constants
{
    [[vk::binding(0)]] ConstantBuffer<FrameData> SceneConstants: register(b0, space0);
};

struct Parameters
{
    [[vk::binding(0)]] Texture2D<float4> GBuffer0: register(t0, space1);
    [[vk::binding(1)]] Texture2D<float4> GBuffer1: register(t1, space1);
    [[vk::binding(2)]] Texture2D<float> GBufferDepth: register(t2, space1);
    [[vk::binding(3)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> GBufferAmbient: register(u0, space1); // RGBA16_Float
};

[[vk::binding(0, 0)]] ParameterBlock<Constants> constants: register(space0);
[[vk::binding(0, 1)]] ParameterBlock<Parameters> parameters: register(space1);

[numthreads(8, 8, 1)]
void GiMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoordinates = id.xy;
    const uint2 resolution = uint2(constants.SceneConstants.m_fullscreen.m_screenResolution);

    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    parameters.GBufferAmbient[pixelCoordinates] = float4(0, 0, 0, 0);

    const float depthSs = parameters.GBufferDepth.Load(int3(pixelCoordinates, 0));

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates.xy, resolution);
    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * constants.SceneConstants.m_fullscreen.m_tanHalfFov,
        1.0f,
        ndc.y * constants.SceneConstants.m_fullscreen.m_tanHalfFov
    );

    const float4 vsToWsQuaternion = Quaternion::Conjugate(constants.SceneConstants.m_fullscreen.m_cameraQuaternion);
    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, - constants.SceneConstants.m_fullscreen.m_cameraTranslation.xyz);

    const float3 positionM = mul(float4(positionW, 1.0f), constants.SceneConstants.m_torusKnotInverseWorldMatrix).xyz;
    const float3 directionM = normalize(mul(float4(cameraW, 0.0f), constants.SceneConstants.m_torusKnotInverseWorldMatrix).xyz);

    const float minDist = constants.SceneConstants.m_fullscreen.m_cameraTranslation.y - 3;
    const float maxDist = constants.SceneConstants.m_fullscreen.m_cameraTranslation.y + 3;
    const uint maxSteps = 128;

    float t = minDist;
    bool hit = false;
    uint i = 0;
    for (; i < maxSteps; i++)
    {
        const float3 position = positionM + t * directionM;
        const float distance = SdTorusKnot(
            position,
            constants.SceneConstants.m_torusKnotRadius,
            constants.SceneConstants.m_torusKnotTubeRadius,
            constants.SceneConstants.m_torusKnotP,
            constants.SceneConstants.m_torusKnotQ);

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
        const float depthV = constants.SceneConstants.m_fullscreen.m_depthLinearizationConstants.x / (depthSs + constants.SceneConstants.m_fullscreen.m_depthLinearizationConstants.y);

        const float3 hitPositionW = mul(float4(hitPositionM, 1.f), constants.SceneConstants.m_torusWorldMatrix).xyz;
        const float3 hitPositionV = Quaternion::Apply(constants.SceneConstants.m_fullscreen.m_cameraQuaternion, hitPositionW) + constants.SceneConstants.m_fullscreen.m_cameraTranslation.xyz;

        if (depthV < hitPositionW.y)
        {
            return;
        }
        color = lerp(color, parameters.GBuffer0.Load(int3(pixelCoordinates, 0)).rgb * parameters.GBuffer1.Load(int3(pixelCoordinates, 0)).rgb, 0.2);
    }

    color = lerp(color, (float(i) / float(maxSteps - 1)).xxx, 0.99f);

    parameters.GBufferAmbient[pixelCoordinates] = float4(color, 1);
}