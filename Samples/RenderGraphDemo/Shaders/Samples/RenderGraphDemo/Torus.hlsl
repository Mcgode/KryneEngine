/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#include "Platform.hlsl"
#include "FrameData.hlsl"

vkBinding(0, 0) ConstantBuffer<FrameData> frameData;

struct VsInput
{
    vkLocation(0) float3 position: POSITION0;
    vkLocation(1) float3 normal: NORMAL0;
};

struct VsOutput
{
    float3 normal: NORMAL;
    float4 position: SV_Position;
};

VsOutput MainVs(const VsInput _input)
{
    VsOutput output;

    output.normal = mul(float4(_input.normal, 0.f), frameData.m_torusWorldMatrix).xyz;

    output.position = mul(mul(float4(_input.position, 1.f), frameData.m_torusWorldMatrix), frameData.m_viewProjectionMatrix);

    return output;
}

typedef VsOutput FsInput;

struct FsOutput
{
    float4 albedo: SV_TARGET0;
    float4 normal: SV_TARGET1;
};

FsOutput MainFs(FsInput _input)
{
    FsOutput output;

    output.albedo = float4(frameData.m_torusAlbedo, 0.f);
    output.normal = float4(normalize(_input.normal) * 0.5f + 0.5f, 0.f);

    return output;
}
