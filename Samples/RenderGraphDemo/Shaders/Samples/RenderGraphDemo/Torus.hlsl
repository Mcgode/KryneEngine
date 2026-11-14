/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#include "Platform.hlsl"
#include "FrameData.hlsl"

struct Parameters
{
    [[vk::binding(0, 0)]] ConstantBuffer<FrameData> frameData;
};
[[vk::binding(0, 0)]] ParameterBlock<Parameters> parameters: register(space0);

struct VsInput
{
    [[vk::location(0)]] float3 position: POSITION0;
    [[vk::location(1)]] float3 normal: NORMAL0;
};

struct VsOutput
{
    float3 normal: NORMAL;
    float4 position: SV_Position;
};

VsOutput MainVs(const VsInput _input)
{
    VsOutput output;

    output.normal = mul(float4(_input.normal, 0.f), parameters.frameData.m_torusWorldMatrix).xyz;

    output.position = mul(mul(float4(_input.position, 1.f), parameters.frameData.m_torusWorldMatrix), parameters.frameData.m_viewProjectionMatrix);

    return output;
}

typedef VsOutput FsInput;

struct FsOutput
{
    // GBuffer0: rgb = albedo, a = roughness
    float4 gBuffer0: SV_TARGET0;
    // GBuffer1: rgb = encoded world normal, a = metalness
    float4 gBuffer1: SV_TARGET1;
};

FsOutput MainFs(FsInput _input)
{
    FsOutput output;

    output.gBuffer0 = float4(parameters.frameData.m_torusAlbedo.xyz, parameters.frameData.m_torusRoughness);
    output.gBuffer1 = float4(normalize(_input.normal) * 0.5f + 0.5f, parameters.frameData.m_torusMetalness);

    return output;
}
