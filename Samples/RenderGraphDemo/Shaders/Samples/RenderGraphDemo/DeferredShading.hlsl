/**
 * @file
 * @author Max Godefroy
 * @date 02/04/2025.
 */

#include "Platform.hlsl"
#include "Lighting/PbrBsdf.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FrameData.hlsl"

vkBinding(0, 0) ConstantBuffer<FrameData> frameData : register(b0, space0);

vkBinding(0, 1) Texture2D<float4> gBufferAlbedo : register(t0, space1);
vkBinding(1, 1) Texture2D<float4> gBufferNormal : register(t1, space1);
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

    const float2 resolution = frameData.m_screenResolution;
    const float2 ndc = ScreenSpaceToNdc(_input.position.xy, resolution);

    const float aspect = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * frameData.m_tanHalfFov,
        1.0f,
        ndc.y * frameData.m_tanHalfFov
    );

    uint2 pixelCoords = uint2(_input.position.xy);
    const float depthSs = gBufferDepth.Load(int3(pixelCoords, 0)).r;

    if (depthSs == 0)
        discard;

    const float depthV = frameData.m_depthLinearizationConstants.x / (depthSs + frameData.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;

    const float4 vsToWsQuaternion = Quaternion::Conjugate(frameData.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - frameData.m_cameraTranslation);

    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float3 normalW = gBufferNormal.Load(int3(pixelCoords, 0)).rgb * 2.f - 1.f;
    const float3 albedo = gBufferAlbedo.Load(int3(pixelCoords, 0)).rgb;

    float3 directLighting = saturate(dot(normalW, -frameData.m_sunLightDirection)) * frameData.m_sunDiffuse;
    const float shadow = deferredShadows.Load(int3(pixelCoords, 0)).r;
    directLighting *= shadow;

    const float3 diffuseColor = albedo * (1.f - frameData.m_torusMetalness);
    const float3 specularColor = lerp(0.04f.xxx, albedo, frameData.m_torusMetalness.xxx);

    const float3 diffuse = diffuseColor * (directLighting + gBufferLight.Load(int3(pixelCoords, 0)).rgb);
    const float3 specular = directLighting * BRDFSpecularGGX(
        -frameData.m_sunLightDirection,
        cameraW,
        normalW,
        specularColor,
        frameData.m_torusRoughness);

    _output.color.xyz = diffuse + specular;

    return _output;
}