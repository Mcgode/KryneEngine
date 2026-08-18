/**
* @file
 * @author Max Godefroy
 * @date 15/01/2026.
 */

struct VsInput
{
    float3 Position : POSITION;
};

struct VsOutput
{
    float4 Position : SV_POSITION;
};

struct UiCubeData
{
    float4x4 MvpMatrix;
};

ConstantBuffer<UiCubeData> uiCubeData;

[shader("vertex")]
VsOutput MainVS(VsInput _input)
{
    VsOutput output;
    output.Position = mul(float4(_input.Position, 1.f), uiCubeData.MvpMatrix);
    return output;
}

static const float3 colors[6] = {
    float3(1, 0, 0),
    float3(0, 1, 0),
    float3(0, 0, 1),
    float3(0, 1, 1),
    float3(1, 0, 1),
    float3(1, 1, 0),
};

[shader("pixel")]
float4 MainFS(uint _primitiveId: SV_PrimitiveId) : SV_TARGET
{
    return float4(colors[_primitiveId / 2], 1.f);
}