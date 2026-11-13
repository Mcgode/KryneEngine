/**
* @file
 * @author Max Godefroy
 * @date 08/12/2025.
 */

#pragma once

#include "Platform.hlsl"

struct ViewportConstants
{
    float4x4 ndcProjectionMatrix;
    float2 viewportSize;
};

struct Constants
{
    [[vk::binding(0)]] ConstantBuffer<ViewportConstants> viewportConstants: register(b0);
};

struct Parameters
{
    [[vk::binding(0)]] Texture2D textures[32]: register(t0);
    [[vk::binding(1)]] SamplerState samplers[8]: register(s0);
};

[[vk::binding(0, 0)]] ParameterBlock<Constants> constantsBlock: register(space0);
[[vk::binding(0, 1)]] ParameterBlock<Parameters> parametersBlock: register(space1);

static const float2 vertices[6] = {
    { -1.0f, -1.0f },
    { -1.0f,  1.0f },
    {  1.0f, -1.0f },
    {  1.0f,  1.0f },
    {  1.0f, -1.0f },
    { -1.0f,  1.0f },
};

struct VsInput
{
    [[vk::location(0)]] uint2 packedRect: POSITION;
    [[vk::location(1)]] uint packedColor: COLOR;
    [[vk::location(2)]] uint4 packedData: TEXCOORD;
    uint vertexId : SV_VulkanVertexID;
};