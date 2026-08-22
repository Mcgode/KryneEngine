/**
 * @file
 * @author Max Godefroy
 * @date 30/08/2026.
 */

#include "Platform.hlsl"
#include "Instances.hlsl"
#include "Camera.hlsl"


vkBinding(0, 0) ConstantBuffer<Camera> camera: register(b0, space0);
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
    const float4x4 worldMat = UnpackTransform(data);

    output.normal = mul(float4(_input.normal, 0.f), worldMat).xyz;

    output.position = mul(mul(float4(_input.position, 1.f), worldMat), camera.m_viewProjMat);

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

    output.albedo = float4(1, 1, 1, 1);
    output.normal = float4(normalize(_input.normal) * 0.5f + 0.5f, 0.f);

    return output;
}
