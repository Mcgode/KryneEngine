/**
* @file
 * @author Max Godefroy
 * @date 27/03/2025.
 */

#include "Platform.hlsl"

struct VsInput
{
    uint vertexIndex : SV_VertexID;
};

struct VsOutput
{
    float4 position : SV_POSITION;
};

struct Parameters
{
    float m_depth;
};

vkPushConstant
ConstantBuffer<Parameters> parameters;

VsOutput FullScreenMain(VsInput input)
{
    VsOutput output;

    output.position = float4(
        input.vertexIndex == 1 ? 3.f : -1.f,
        input.vertexIndex == 2 ? 3.f : -1.f,
        parameters.m_depth,
        1.f
    );

    return output;
}