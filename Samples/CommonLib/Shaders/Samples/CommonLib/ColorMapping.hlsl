/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#include "Platform.hlsl"
#include "ToneMapping.hlsl"
#include "FrameData.hlsl"

vkBinding(0, 0) ConstantBuffer<FrameData> frameData: register(b0, space0);

vkBinding(0, 1) Texture2D<float4> inputColor: register(t0, space1);

struct FsInput
{
    float4 position: SV_Position;
};

struct FsOutput
{
    float4 color: SV_Target0;
};

FsOutput ColorMappingMain(const FsInput _input)
{
    FsOutput output;

    output.color.rgb = Tonemap_ACES(inputColor.Load(int3(_input.position.xy, 0)).rgb);
    output.color.a = 1.f;

    return output;
}