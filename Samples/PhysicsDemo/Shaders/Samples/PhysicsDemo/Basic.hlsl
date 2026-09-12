/**
 * @file
 * @author Max Godefroy
 * @date 30/08/2026.
 */

#include "Platform.hlsl"
#include "Instances.hlsl"
#include "Pass.hlsl"


vkBinding(0, 0) ConstantBuffer<PassData> passData: register(b0, space0);
vkBinding(1, 0) StructuredBuffer<InstanceData> instanceData: register(t0, space0);


struct VsInput
{
    vkLocation(0) float3 position: POSITION0;
    vkLocation(1) float3 normal: NORMAL0;
    vkLocation(2) uint instanceId: BLENDINDICES0;
};

struct VsOutput
{
    float3 normal: NORMAL;
    float4 position: SV_POSITION;
};


VsOutput MainVs(const in VsInput _input)
{
    VsOutput output;

    const InstanceData data = instanceData[_input.instanceId];

    float3 position = _input.position;
    float3 normal = _input.normal;
    ApplyTransform(data, position, normal);

    output.normal = normal;
    output.position = mul(float4(position, 1.f), passData.m_viewProjectionMatrix);

    return output;
}


typedef VsOutput FsInput;

struct FsOutput
{
    float4 gBuffer0: SV_TARGET0;
    float4 gBuffer1: SV_TARGET1;
    float4 gBuffer2: SV_TARGET2;
};


FsOutput MainFs(FsInput _input)
{
    FsOutput output;

    output.gBuffer0 = float4(0.5f.xxx, 1);
    output.gBuffer1 = float4(normalize(_input.normal) * 0.5f + 0.5f, 0.f);
    output.gBuffer2 = float4(0.xxxx);

    return output;
}
