/**
* @file
 * @author Max Godefroy
 * @date 26/12/2025.
 */

#include "Common.hlsl"
#include "Math/ColorConversion.hlsl"
#include "Math/CoordinateTransforms.hlsl"

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 pixelPosition : POSITION0;
    nointerpolation uint2 cornerRadii : TEXCOORD0;
    nointerpolation uint2 width : TEXCOORD1;
    nointerpolation uint backgroundColor : COLOR;
    nointerpolation float2 rectHalfSize: POSITION1;
};

typedef VsOutput FsInput;

[shader("vertex")]
VsOutput BorderVs(VsInput _input)
{
    VsOutput output;

    const float2 rectCenter = unpackHalf2x16ToFloat(_input.packedRect.x);
    const float2 rectHalfSize = unpackHalf2x16ToFloat(_input.packedRect.y);

    output.backgroundColor = _input.packedColor;
    output.cornerRadii = _input.packedData.xy;
    output.width = _input.packedData.zw;
    output.rectHalfSize = rectHalfSize;

    const float2 baseVertexPos = vertices[_input.vertexId];
    const float2 relativePixelPosition = baseVertexPos * rectHalfSize;
    const float2 absolutePixelPosition = rectCenter + relativePixelPosition;
    const float2 ndcPosition = UvToNdc(absolutePixelPosition / constantsBlock.viewportConstants.viewportSize);

    output.pixelPosition = relativePixelPosition;
    output.position = mul(float4(ndcPosition, 0.f, 1.f), constantsBlock.viewportConstants.ndcProjectionMatrix);

    return output;
}

[shader("pixel")]
float4 BorderFs(FsInput _input): SV_TARGET0
{
    float4 width = float4(unpackHalf2x16ToFloat(_input.width.x), unpackHalf2x16ToFloat(_input.width.y));
    width.x = _input.pixelPosition.y < 0.f ? width.x : width.y; // Select between top and bottom border width
    width.y = _input.pixelPosition.x < 0.f ? width.z : width.w; // Select between left and right border width

    // Rounded box SDF calculations based on https://www.shadertoy.com/view/4cG3R1
    const float2 cornerRadii = unpackHalf2x16ToFloat(_input.pixelPosition.y < 0.f ? _input.cornerRadii.x : _input.cornerRadii.y);
    const float cornerRadius = _input.pixelPosition.x < 0.f ? cornerRadii.x : cornerRadii.y;

    const float2 q = abs(_input.pixelPosition) - _input.rectHalfSize + cornerRadius;

    float d;
    if (min(q.x, q.y) < 0.f)
    {
        const float2 ds = q + width.yx - cornerRadius;
        if (ds.x > ds.y)
        {
            d = ds.x - width.y;
            width.x = width.y; // use Left/Right width
        }
        else
        {
            d = ds.y - width.x;
            width.x = width.x; // use Top/Bottom width
        }
    }
    else
    {
        const float2 uv = float2(abs(q.x - q.y), q.x + q.y - cornerRadius) / cornerRadius;
        d = (length(uv - float2(0, -1)) - sqrt(2.f)) * cornerRadius * sqrt(0.5f);


        width.x = width.x != width.y
            ? lerp(width.x, width.y, smoothstep(0, 1, asin(normalize(q / cornerRadius).x) / 3.1415926535897932384626433f * 2))
            : width.x;
    }
    d = abs(d + width.x * 0.5f) - width.x * 0.5f ;

    const float2 dPixelPos = Ddx(_input.pixelPosition);
    const float scale = length(dPixelPos) * 0.5;

    const float4 color = SrgbToLinear(unpackUnorm4x8ToFloat(_input.backgroundColor));

    return lerp(color, float4(color.rgb, 0.f), smoothstep(0, 1, d / scale + 0.5f));
}