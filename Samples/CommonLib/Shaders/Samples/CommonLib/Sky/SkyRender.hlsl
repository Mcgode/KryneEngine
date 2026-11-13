/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#include "Platform.hlsl"
#include "Atmosphere.hlsli"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "../FullscreenPassConstants.hlsl"

struct Input  { float4 screenPosition: SV_Position; };
struct Output { float4 color: SV_Target0; };

struct Parameters
{
    [[vk::binding(0)]] ConstantBuffer<FullscreenPassConstants> SceneConstants: register(b0);
};
[[vk::binding(0, 0)]] ParameterBlock<Parameters> parameters: register(space0);

Output SkyMain(Input _input)
{
    Output output;

    const float2 resolution = parameters.SceneConstants.m_screenResolution;
    const float2 ndc        = ScreenSpaceToNdc(_input.screenPosition.xy, resolution);

    const float  aspect  = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * parameters.SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * parameters.SceneConstants.m_tanHalfFov
    );

    const float4 vsToWsQuat = Quaternion::Conjugate(parameters.SceneConstants.m_cameraQuaternion);
    const float3 cameraW    = Quaternion::Apply(vsToWsQuat, normalize(cameraV));
    const float3 eyePosW    = parameters.SceneConstants.m_cameraTranslation.xyz + float3(0, 0, kAtmospherePlanetRadius);

    const AtmoRay rayW = { eyePosW, cameraW };

    output.color = float4(AtmoGetIncidentLight(
            rayW,
            parameters.SceneConstants.m_sunLightDirection,
            parameters.SceneConstants.m_sunDiffuse) * kPi,
             1);

    return output;
}
