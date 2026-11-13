/**
* @file
 * @author Max Godefroy
 * @date 02/01/2026.
 */

#include "Common.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/ColorConversion.hlsl"

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 pixelPosition : POSITION0;
    float2 sampleUv: TEXCOORD0;
    nointerpolation uint2 cornerRadii : TEXCOORD1;
    nointerpolation uint sampleIds : TEXCOORD2;
    nointerpolation uint backgroundColor : COLOR;
    nointerpolation float2 rectHalfSize: POSITION1;
};

typedef VsOutput FsInput;

[shader("vertex")]
VsOutput ImageVs(VsInput _input)
{
    VsOutput output;

    const float2 rectCenter = unpackHalf2x16ToFloat(_input.packedRect.x);
    const float2 rectHalfSize = unpackHalf2x16ToFloat(_input.packedRect.y);

    output.backgroundColor = _input.packedColor;
    output.cornerRadii = uint2(_input.packedData.x >> 8, _input.packedData.y);
    output.sampleIds = _input.packedData.x & 0xff;
    output.rectHalfSize = rectHalfSize;

    const float2 baseVertexPos = vertices[_input.vertexId];
    const float2 relativePixelPosition = baseVertexPos * rectHalfSize;
    const float2 absolutePixelPosition = rectCenter + relativePixelPosition;
    const float2 ndcPosition = UvToNdc(absolutePixelPosition / constantsBlock.viewportConstants.viewportSize);

    const float2 relativeUv = baseVertexPos * unpackHalf2x16ToFloat(_input.packedData.w);
    const float2 absoluteUv = relativeUv + unpackHalf2x16ToFloat(_input.packedData.z);
    output.sampleUv = absoluteUv;

    output.pixelPosition = relativePixelPosition;
    output.position = mul(float4(ndcPosition, 0.f, 1.f), constantsBlock.viewportConstants.ndcProjectionMatrix);

    return output;
}

// Rounded box SDF calculations based on https://www.shadertoy.com/view/4cG3R1
float SDRoundedBox(const float2 _position, const float2 _halfSize, float4 _cornerRadii)
{
    _cornerRadii.xy = _position.y < 0.f ? _cornerRadii.xy : _cornerRadii.zw;
    const float cornerRadius = _position.x < 0.f ? _cornerRadii.x : _cornerRadii.y;
    const float2 q = abs(_position) - _halfSize + cornerRadius;

    if (min(q.x, q.y) < 0.f)
        return max(q.x, q.y) - cornerRadius;

    const float2 uv = float2(abs(q.x - q.y), q.x + q.y - cornerRadius) / cornerRadius;
    return (length(uv - float2(0, -1)) - sqrt(2.f)) * cornerRadius * sqrt(0.5f);
}

[shader("pixel")]
float4 ImageFs(FsInput _input): SV_TARGET0
{
    const float d = SDRoundedBox(
        _input.pixelPosition,
        _input.rectHalfSize,
        float4(
            _input.cornerRadii.x & 0x0fff,
            _input.cornerRadii.x >> 12,
            _input.cornerRadii.y & 0x0fff,
            _input.cornerRadii.y >> 12
        ));

    const float2 dPixelPos = Ddx(_input.pixelPosition);
    const float scale = length(dPixelPos) * 0.5;

    const float4 tintColor = SrgbToLinear(unpackUnorm4x8ToFloat(_input.backgroundColor));

    const uint textureId = _input.sampleIds & 0x1f;
    const uint samplerId = _input.sampleIds >> 5;
    const float4 color = parametersBlock.textures[textureId].Sample(parametersBlock.samplers[samplerId], _input.sampleUv) * tintColor;

    return lerp(color, float4(color.rgb, 0.f), saturate(d / scale + 0.5f));
}