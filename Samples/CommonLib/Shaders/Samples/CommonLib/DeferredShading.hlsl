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
#include "Sky/SphericalHarmonics.hlsli"

struct Constants
{
    [[vk::binding(0)]] ConstantBuffer<FullscreenPassConstants> constants : register(b0);
};

struct Parameters
{
    // GBuffer0: rgb = albedo, a = roughness
    [[vk::binding(0)]] Texture2D<float4> gBuffer0 : register(t0);
    // GBuffer1: rgb = encoded world normal, a = metalness
    [[vk::binding(1)]] Texture2D<float4> gBuffer1 : register(t1);
    [[vk::binding(2)]] Texture2D<float4> gBufferDepth : register(t2);
    [[vk::binding(3)]] Texture2D<float4> deferredShadows : register(t3);
    [[vk::binding(4)]] Texture2D<float4> gBufferLight : register(t4);
    // Sky ambient buffer: 9 pre-convolved SH coefficients (see SphericalHarmonics.hlsli).
    [[vk::binding(5)]] StructuredBuffer<float4> skyAmbient : register(t5);
    // Ambient occlusion: AmbientOcclusionPass's final term, 1 = fully visible, 0 = fully occluded.
    [[vk::binding(6)]] Texture2D<float> ambientOcclusion : register(t6);
};

[[vk::binding(0, 0)]] ParameterBlock<Constants> constantsBlock: register(space0);
[[vk::binding(0, 1)]] ParameterBlock<Parameters> parametersBlock: register(space1);

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

    const float2 resolution = constantsBlock.constants.m_screenResolution;
    const float2 ndc = ScreenSpaceToNdc(_input.position.xy, resolution);

    const float aspect = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * constantsBlock.constants.m_tanHalfFov,
        1.0f,
        ndc.y * constantsBlock.constants.m_tanHalfFov
    );

    uint2 pixelCoords = uint2(_input.position.xy);
    const float depthSs = parametersBlock.gBufferDepth.Load(int3(pixelCoords, 0)).r;

    if (depthSs == 0)
        discard;

    const float depthV = constantsBlock.constants.m_depthLinearizationConstants.x / (depthSs + constantsBlock.constants.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;

    const float4 vsToWsQuaternion = Quaternion::Conjugate(constantsBlock.constants.m_cameraQuaternion);
    const float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - constantsBlock.constants.m_cameraTranslation.xyz);

    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float4 gBuffer0Sample = parametersBlock.gBuffer0.Load(int3(pixelCoords, 0));
    const float4 gBuffer1Sample = parametersBlock.gBuffer1.Load(int3(pixelCoords, 0));

    const float3 normalW = gBuffer1Sample.rgb * 2.f - 1.f;
    const float3 albedo = gBuffer0Sample.rgb;
    const float roughness = gBuffer0Sample.a;
    const float metalness = gBuffer1Sample.a;

    float3 directLighting = saturate(dot(normalW, -constantsBlock.constants.m_sunLightDirection)) * constantsBlock.constants.m_sunDiffuse;
    const float shadow = parametersBlock.deferredShadows.Load(int3(pixelCoords, 0)).r;
    directLighting *= shadow;

    const float3 diffuseColor = albedo * (1.f - metalness);
    const float3 specularColor = lerp(0.04f.xxx, albedo, metalness.xxx);

    // Second-order SH ambient: keeps directional information beyond the up axis, so it captures
    // sun-side vs anti-sun-side sky asymmetry (e.g. sunsets) instead of just a vertical gradient.
    // Clamped to zero: a truncated SH reconstruction of a sharply-varying function can ring
    // slightly negative, which isn't physical for an irradiance term.
    const float3 skyAmbientIrradiance = max(EvalIrradianceSH9(normalW, parametersBlock.skyAmbient), 0.0f.xxx);

    // Ambient occlusion only darkens the indirect/ambient terms (baked light + sky irradiance),
    // not the direct sun contribution below - it approximates nearby geometry blocking incoming
    // ambient light, not a substitute for a direct shadow.
    const float ao = parametersBlock.ambientOcclusion.Load(int3(pixelCoords, 0)).r;
    const float3 indirectDiffuse = (parametersBlock.gBufferLight.Load(int3(pixelCoords, 0)).rgb + skyAmbientIrradiance) * ao;

    const float3 diffuse = diffuseColor * (directLighting + indirectDiffuse);
    const float3 specular = directLighting * BRDFSpecularGGX(
        -constantsBlock.constants.m_sunLightDirection,
        cameraW,
        normalW,
        specularColor,
        roughness);

    _output.color.xyz = diffuse + specular;

    return _output;
}
