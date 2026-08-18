// Hello triangle.slang

/*
 * An example highlighting some of Slang code highlighting
 */

#include "Platform.hlsl"

#define POS m_position: POSITION0;

struct IaToVs
{
    [[vk::location(0)]] float3 POS
    [[vk::location(1)]] float4 m_color: COLOR0;
};

struct VsToPs
{
    [[vk::location(0)]] float4 m_color: COLOR0;
    float4 m_position: SV_POSITION;
};

struct PsToOm
{
    [[vk::location(0)]] float4 m_color: SV_TARGET0;
};

func myFun()
{}

struct PushConstants
{
    float4x4 m_matrix;
}


[shader("vertex")]
VsToPs MainVS(IaToVs _input, [vk::push_constant] uniform PushConstants pushConstants)
{
    VsToPs output;

    output.m_color = _input.m_color;
    output.m_position = mul(pushConstants.m_matrix, float4(_input.m_position, 1));

    return output;
}

[shader("fragment")]
PsToOm MainPS(VsToPs _input)
{
    PsToOm output;

    myFun();
    output.m_color = _input.m_color;
    output.m_color = _input.m_color;

    return output;
}

struct PsInput
{
    [[vk::location(0)]] float4 color: COLOR0;
    [[vk::location(1)]] float2 uv: TEXCOORD0;
};

struct PsOutput
{
    [[vk::location(0)]] float4 color: SV_Target0;
};

struct DescriptorSet0 {
    Texture2D texture0: register(t0);
}

struct DescriptorSet1 {
    SamplerState sampler0: register(s0);
    Texture2D texture1: register(t1);
    RWTexture2D texture2;
    StructuredBuffer<uint> buffer0;
    RWByteAddressBuffer buffer1;
    ConstantBuffer<uint> buffer2;
}

[shader("fragment")]
PsOutput ImGuiMainFS(
    in PsInput input,
    [vk::binding(0, 0)] ParameterBlock<DescriptorSet0> descriptorSet0: register(space0),
    [vk::binding(0, 1)] ParameterBlock<DescriptorSet1> descriptorSet1: register(space1))
{
    PsOutput output;

    output.color = input.color * descriptorSet0.texture0.Sample(descriptorSet1.sampler0, input.uv);
    output.color *= descriptorSet1.texture1.Sample(descriptorSet1.sampler0, input.uv);

    return output;
};

// compute-simple.slang

struct ImageProcessingOptions
{
    float3 tintColor;
    float blurRadius;

    bool useLookupTable;
    StructuredBuffer<float4> lookupTable;
}

[shader("compute")]
[numthreads(8, 8)]
void processImage(
    uint3 threadID : SV_DispatchThreadID,
    uniform Texture2D inputImage,
    uniform RWTexture2D outputImage,
    uniform ImageProcessingOptions options)
{
    /* actual logic would go here */
}