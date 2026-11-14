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

struct Constants
{
    [[vk::binding(0)]] ConstantBuffer<FrameData> SceneConstants: register(b0);
};

struct Parameters
{
    [[vk::binding(0)]] Texture2D<float> GBufferDepth: register(t0);
    [[vk::binding(1)]] RWTexture2D<unorm float> DeferredShadows: register(u0); // R8_UNorm
};

[[vk::binding(0, 0)]] ParameterBlock<Constants> constants: register(space0);
[[vk::binding(0, 1)]] ParameterBlock<Parameters> parameters: register(space1);

[numthreads(8, 8, 1)]
void DeferredShadowsMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoordinates = id.xy;
    const uint2 resolution = uint2(constants.SceneConstants.m_fullscreen.m_screenResolution);

    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    const float depthSs = parameters.GBufferDepth.Load(int3(pixelCoordinates, 0));

    if (depthSs == 0.f)
    {
        return;
    }

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates.xy, resolution);

    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * constants.SceneConstants.m_fullscreen.m_tanHalfFov,
        1.0f,
        ndc.y * constants.SceneConstants.m_fullscreen.m_tanHalfFov
    );

    const float depthV = constants.SceneConstants.m_fullscreen.m_depthLinearizationConstants.x / (depthSs + constants.SceneConstants.m_fullscreen.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;
    const float4 vsToWsQuaternion = Quaternion::Conjugate(constants.SceneConstants.m_fullscreen.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - constants.SceneConstants.m_fullscreen.m_cameraTranslation.xyz);
    const float3 directionW = -constants.SceneConstants.m_fullscreen.m_sunLightDirection;

    const float3 positionM = mul(float4(positionW, 1.0f), constants.SceneConstants.m_torusKnotInverseWorldMatrix).xyz;
    const float3 directionM = normalize(mul(float4(directionW, 0.0f), constants.SceneConstants.m_torusKnotInverseWorldMatrix).xyz);

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
            constants.SceneConstants.m_torusKnotRadius,
            constants.SceneConstants.m_torusKnotTubeRadius,
            constants.SceneConstants.m_torusKnotP,
            constants.SceneConstants.m_torusKnotQ);

        if (distance <= 0.01f)
        {
            shadow = 0.f;
            break;
        }
        shadow = min(shadow, distance * k / t);
        t += distance;
    }

    parameters.DeferredShadows[pixelCoordinates] = shadow;
}