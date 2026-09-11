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
    // Model-space position, forwarded so the pixel shader can identify the cube face without
    // SV_PrimitiveID (which would require the geometry shader SPIR-V capability).
    float3 ModelPosition : POSITION0;
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
    output.ModelPosition = _input.Position;
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

typedef VsOutput FsInput;

[shader("pixel")]
float4 MainFS(const in FsInput _input) : SV_TARGET
{
    // The cube is axis-aligned in model space with +/-1 corners: on any given face, that face's
    // axis component interpolates to exactly +/-1 while the other two stay within [-1, 1].
    const float3 position = _input.ModelPosition;
    const float3 absPosition = abs(position);

    uint face;
    if (absPosition.x >= absPosition.y && absPosition.x >= absPosition.z)
    {
        face = position.x < 0.f ? 4 : 1;
    }
    else if (absPosition.y >= absPosition.z)
    {
        face = position.y < 0.f ? 2 : 5;
    }
    else
    {
        face = position.z < 0.f ? 0 : 3;
    }

    return float4(colors[face], 1.f);
}
