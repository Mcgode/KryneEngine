#include "Platform.hlsl"

struct IaToVs
{
    vkLocation(0) float3 m_position: POSITION0;
    vkLocation(1) float3 m_color: COLOR0;
};

struct VsToPs
{
    vkLocation(0) float4 m_color: COLOR0;
    float4 m_position: SV_POSITION;
};

struct PsToOm
{
    vkLocation(0) float4 m_color: SV_TARGET0;
};

VsToPs MainVS(IaToVs _input)
{
    VsToPs output;

    output.m_color = float4(_input.m_color, 1.f);
    output.m_position = float4(_input.m_position, 1);

    return output;
}

PsToOm MainPS(VsToPs _input)
{
    PsToOm output;

    output.m_color = _input.m_color;

    return output;
}