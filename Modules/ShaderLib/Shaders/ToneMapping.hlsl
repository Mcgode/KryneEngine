/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

// Functions taken from https://mini.gmshaders.com/p/tonemaps

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
float3 Tonemap_ACES(float3 _rawColor)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (_rawColor * (a * _rawColor + b)) / (_rawColor * (c * _rawColor + d) + e);
}

// Hable 2010, "Filmic Tonemapping Operators"
float3 Tonemap_Uncharted2(float3 _rawColor)
{
    _rawColor *= 16.0;
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((_rawColor*(A*_rawColor+C*B)+D*E)/(_rawColor*(A*_rawColor+B)+D*F))-E/F;
}

// Unreal 3, Documentation: "Color Grading"
// Adapted to be close to Tonemap_ACES, with similar range
float3 Tonemap_Unreal(float3 _rawColor)
{
    // Gamma 2.2 correction is baked in, don't use with sRGB conversion!
    return _rawColor / (_rawColor + 0.155) * 1.019;
}

float3 Tonemap_tanh(float3 _rawColor)
{
    _rawColor = clamp(_rawColor, -40.0, 40.0);
    return (exp(_rawColor) - exp(-_rawColor)) / (exp(_rawColor) + exp(-_rawColor));
}

// Wrensch 2023, "Minimal AgX Implementation" (https://iolite-engine.com/blog_posts/minimal_agx_implementation)
// Fit of Blender's AgX view transform. Input is linear Rec.709, output of AgX_Encode is display-encoded (~gamma 2.2).

// Polynomial fit of the default AgX contrast sigmoid, mean error^2: 3.6705141e-06
float3 AgX_DefaultContrastApprox(float3 _x)
{
    const float3 x2 = _x * _x;
    const float3 x4 = x2 * x2;
    return 15.5 * x4 * x2
        - 40.14 * x4 * _x
        + 31.96 * x4
        - 6.868 * x2 * _x
        + 0.4298 * x2
        + 0.1191 * _x
        - 0.00232;
}

float3 AgX_Encode(float3 _rawColor)
{
    // Rows here are the columns of the reference GLSL matrices, hence mul(vector, matrix)
    const float3x3 insetMatrix = float3x3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    float3 color = mul(_rawColor, insetMatrix);
    color = clamp(log2(max(color, 1e-10)), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    return AgX_DefaultContrastApprox(color);
}

// Optional ASC CDL look, applied between AgX_Encode and AgX_Decode
float3 AgX_LookPunchy(float3 _encodedColor)
{
    const float luma = dot(_encodedColor, float3(0.2126, 0.7152, 0.0722));
    const float3 color = pow(_encodedColor, 1.35);
    return luma + 1.4 * (color - luma);
}

// Back to linear Rec.709, to be written to an sRGB render target
float3 AgX_Decode(float3 _encodedColor)
{
    const float3x3 outsetMatrix = float3x3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

    const float3 color = mul(_encodedColor, outsetMatrix);
    return pow(saturate(color), 2.2);
}

float3 Tonemap_AgX(float3 _rawColor)
{
    return AgX_Decode(AgX_Encode(_rawColor));
}
