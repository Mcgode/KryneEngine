/**
* @file
 * @author Max Godefroy
 * @date 06/01/2026.
 */

#define vkPushConstant [[vk::push_constant]]
#define vkLocation(index) [[vk::location(index)]]

struct ViewportConstants
{
    float4x4 ndcProjectionMatrix;
    float2 viewportSize;
};

struct ViewportConstantsBlock
{
    ConstantBuffer<ViewportConstants> viewportConstants;
}

struct TexturesDescriptorSet
{
    Texture2D textures[32];
    SamplerState samplers[8];
}

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
    uint vertexId : SV_VertexID;
};

float2 ScreenSpaceToUv(const in float2 _ssCoords, const in float2 _resolution)
{
    return _ssCoords / _resolution;
}

float2 ScreenSpaceToUv(const in uint2 _pixelCoordinates, const in float2 _resolution)
{
    return ScreenSpaceToUv(float2(_pixelCoordinates) + 0.5, _resolution);
}

float2 UvToNdc(const in float2 _uvCoords)
{
    return float2(
        2.f * _uvCoords.x - 1.f,
        1.f - 2.f * _uvCoords.y
    );
}

float2 ScreenSpaceToNdc(const in float2 _ssCoords, const in float2 _resolution)
{
    return UvToNdc(ScreenSpaceToUv(_ssCoords, _resolution));
}

float2 ScreenSpaceToNdc(const in uint2 _pixelCoordinates, const in float2 _resolution)
{
    return UvToNdc(ScreenSpaceToUv(_pixelCoordinates, _resolution));
}

// Based on https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 LinearToSrgb(const in float3 _lrgb)
{
    const float3 s1 = sqrt(_lrgb);
    const float3 s2 = sqrt(s1);
    const float3 s3 = sqrt(s2);

    return 0.585122381 * s1 + 0.783140355 * s2 - 0.368262736 * s3;
}

// Based on https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 SrgbtoLinear(const in float3 _srgb)
{
    return _srgb * (_srgb * (_srgb * 0.305306011 + 0.682171111) + 0.012522878);
}

float4 LinearToSrgb(const in float4 _lrgba)
{
    return float4(LinearToSrgb(_lrgba.rgb), _lrgba.a);
}

float4 SrgbToLinear(const in float4 _srgba)
{
    return float4(SrgbtoLinear(_srgba.rgb), _srgba.a);
}

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 pixelPosition : POSITION0;
    float2 samplePixelCoords: TEXCOORD0;
    nointerpolation uint backgroundColor : COLOR;
    nointerpolation uint pixelRange: TEXCOORD1;
};

typedef VsOutput FsInput;

ParameterBlock<ViewportConstantsBlock> viewportConstantsBlock;

[shader("vertex")]
VsOutput TextVs(VsInput _input)
{
    VsOutput output;

    const float2 rectCenter = unpackHalf2x16ToFloat(_input.packedRect.x);
    const float2 rectHalfSize = unpackHalf2x16ToFloat(_input.packedRect.y);

    output.backgroundColor = _input.packedColor;

    const float2 baseVertexPos = vertices[_input.vertexId];
    const float2 relativePixelPosition = baseVertexPos * rectHalfSize;
    const float2 absolutePixelPosition = rectCenter + relativePixelPosition;
    const float2 ndcPosition = UvToNdc(absolutePixelPosition / viewportConstantsBlock.viewportConstants.viewportSize);

    const float2 msdfPixelCoords = float2(
        baseVertexPos.x < 0.f ? _input.packedData.x & 0xffff : _input.packedData.y & 0xffff,
        baseVertexPos.y < 0.f ? _input.packedData.x >> 16    : _input.packedData.y >> 16);
    output.samplePixelCoords = msdfPixelCoords;
    output.pixelRange = _input.packedData.z;

    output.pixelPosition = relativePixelPosition;
    output.position = mul(float4(ndcPosition, 0.f, 1.f), viewportConstantsBlock.viewportConstants.ndcProjectionMatrix);

    return output;
}

float2 sqr(const in float2 _v)
{
    return _v * _v;
}

struct Text { Texture2D texture; };
struct Samp { SamplerState sampler; };
ParameterBlock<TexturesDescriptorSet> tds;

[shader("fragment")]
float4 TextFs(FsInput _input, ParameterBlock<Text> text, ParameterBlock<Samp> samp): SV_TARGET0
{
    uint w, h;
    tds.textures[0].GetDimensions(w, h);
    const float2 uv = _input.samplePixelCoords / float2(w, h);
    const float3 msdfSample = text.texture.Sample(samp.sampler, uv).rgb;
    const float median = max(min(msdfSample.r, msdfSample.g), min(max(msdfSample.r, msdfSample.g), msdfSample.b));

    const float2 unitRange = float2(_input.pixelRange.xx) / float2(w, h);
    const float2 screenTextSize = 1.f / sqrt(sqr(ddx(uv)) + sqr(ddy(uv)));
    const float screenPxRange = max(0.5f * dot(unitRange, screenTextSize), 1.f);

    const float d = screenPxRange * (median - 0.5f);

    const float4 tintColor = SrgbToLinear(unpackUnorm4x8ToFloat(_input.backgroundColor));

    return lerp(float4(tintColor.rgb, 0.f), tintColor, saturate(d + 0.5));
}