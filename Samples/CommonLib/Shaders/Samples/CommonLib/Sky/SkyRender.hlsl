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

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants;

Output SkyMain(Input _input)
{
    Output output;

    const float2 resolution = SceneConstants.m_screenResolution;
    const float2 ndc        = ScreenSpaceToNdc(_input.screenPosition.xy, resolution);

    const float  aspect  = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov
    );

    const float4 vsToWsQuat = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    const float3 cameraW    = Quaternion::Apply(vsToWsQuat, normalize(cameraV));
    const float3 eyePosW    = SceneConstants.m_cameraTranslation + float3(0, 0, kAtmospherePlanetRadius);

    const AtmoRay rayW = { eyePosW, cameraW };

    output.color = float4(AtmoGetIncidentLight(rayW, SceneConstants.m_sunLightDirection, SceneConstants.m_sunDiffuse) * kPi, 1);

    return output;
}
