/**
* @file
 * @author Max Godefroy
 * @date 06/01/2026.
 */


#include "Common.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/ColorConversion.hlsl"

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 pixelPosition : POSITION0;
    float2 samplePixelCoords: TEXCOORD0;
    nointerpolation uint backgroundColor : COLOR;
    nointerpolation uint pixelRange: TEXCOORD1;
};

typedef VsOutput FsInput;

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
    const float2 ndcPosition = UvToNdc(absolutePixelPosition / constantsBlock.viewportConstants.viewportSize);

    const float2 msdfPixelCoords = float2(
        baseVertexPos.x < 0.f ? _input.packedData.x & 0xffff : _input.packedData.y & 0xffff,
        baseVertexPos.y < 0.f ? _input.packedData.x >> 16    : _input.packedData.y >> 16);
    output.samplePixelCoords = msdfPixelCoords;
    output.pixelRange = _input.packedData.z;

    output.pixelPosition = relativePixelPosition;
    output.position = mul(float4(ndcPosition, 0.f, 1.f), constantsBlock.viewportConstants.ndcProjectionMatrix);

    return output;
}

float2 sqr(const in float2 _v)
{
    return _v * _v;
}

[shader("pixel")]
float4 TextFs(FsInput _input): SV_TARGET0
{
    uint w, h;
    parametersBlock.textures[0].GetDimensions(w, h);
    const float2 uv = _input.samplePixelCoords / float2(w, h);
    const float3 msdfSample = parametersBlock.textures[0].Sample(parametersBlock.samplers[0], uv).rgb;
    const float median = max(min(msdfSample.r, msdfSample.g), min(max(msdfSample.r, msdfSample.g), msdfSample.b));

    const float2 unitRange = float2(_input.pixelRange.xx) / float2(w, h);
    const float2 screenTextSize = 1.f / sqrt(sqr(Ddx(uv)) + sqr(Ddy(uv)));
    const float screenPxRange = max(0.5f * dot(unitRange, screenTextSize), 1.f);

    const float d = screenPxRange * (median - 0.5f);

    const float4 tintColor = SrgbToLinear(unpackUnorm4x8ToFloat(_input.backgroundColor));

    return lerp(float4(tintColor.rgb, 0.f), tintColor, saturate(d + 0.5));
}