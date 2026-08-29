/**
 * @file
 * @author Max Godefroy
 * @date 02/04/2025.
 */

#include "Platform.hlsl"
#include "Lighting/PbrBsdf.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FullscreenPassConstants.hlsl"

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> constants : register(b0, space0);

// GBuffer0: rgb = albedo, a = roughness
vkBinding(0, 1) Texture2D<float4> gBuffer0 : register(t0, space1);
// GBuffer1: rgb = encoded world normal, a = metalness
vkBinding(1, 1) Texture2D<float4> gBuffer1 : register(t1, space1);
vkBinding(2, 1) Texture2D<float4> gBufferDepth : register(t2, space1);
vkBinding(3, 1) Texture2D<float4> deferredShadows : register(t3, space1);
vkBinding(4, 1) Texture2D<float4> gBufferLight : register(t4, space1);

struct FsInput
{
    float4 position: SV_POSITION;
};

struct FsOutput
{
    float4 color: SV_Target0;
};

FsOutput DeferredShadingMain(const in FsInput _input)
{
    FsOutput _output;

    const float2 resolution = constants.m_screenResolution;
    const float2 ndc = ScreenSpaceToNdc(_input.position.xy, resolution);

    const float aspect = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * constants.m_tanHalfFov,
        1.0f,
        ndc.y * constants.m_tanHalfFov
    );

    uint2 pixelCoords = uint2(_input.position.xy);
    const float depthSs = gBufferDepth.Load(int3(pixelCoords, 0)).r;

    if (depthSs == 0)
        discard;

    const float depthV = constants.m_depthLinearizationConstants.x / (depthSs + constants.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;

    const float4 vsToWsQuaternion = Quaternion::Conjugate(constants.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - constants.m_cameraTranslation);

    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float4 gBuffer0Sample = gBuffer0.Load(int3(pixelCoords, 0));
    const float4 gBuffer1Sample = gBuffer1.Load(int3(pixelCoords, 0));

    const float3 normalW = gBuffer1Sample.rgb * 2.f - 1.f;
    const float3 albedo = gBuffer0Sample.rgb;
    const float roughness = gBuffer0Sample.a;
    const float metalness = gBuffer1Sample.a;

    float3 directLighting = saturate(dot(normalW, -constants.m_sunLightDirection)) * constants.m_sunDiffuse;
    const float shadow = deferredShadows.Load(int3(pixelCoords, 0)).r;
    directLighting *= shadow;

    const float3 diffuseColor = albedo * (1.f - metalness);
    const float3 specularColor = lerp(0.04f.xxx, albedo, metalness.xxx);

    const float3 diffuse = diffuseColor * (directLighting + gBufferLight.Load(int3(pixelCoords, 0)).rgb);
    const float3 specular = directLighting * BRDFSpecularGGX(
        -constants.m_sunLightDirection,
        cameraW,
        normalW,
        specularColor,
        roughness);

    _output.color.xyz = diffuse + specular;

    return _output;
}