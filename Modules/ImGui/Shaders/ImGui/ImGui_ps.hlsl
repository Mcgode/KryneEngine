#include "Platform.hlsl"

struct PsInput
{
    [[vk::location(0)]] float4 color: COLOR0;
    [[vk::location(1)]] float2 uv: TEXCOORD0;
};

struct PsOutput
{
    [[vk::location(0)]] float4 color: SV_Target0;
};

struct Parameters
{
    [[vk::binding(0)]] Texture2D texture0: register(t0);
    [[vk::binding(1)]] SamplerState sampler0: register(s0);
};
[[vk::binding(0, 0)]] ParameterBlock<Parameters> parameters: register(space0);

PsOutput MainPS(in PsInput input)
{
    PsOutput output;

    output.color = input.color * parameters.texture0.Sample(parameters.sampler0, input.uv);

    return output;
};