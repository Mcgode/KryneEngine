/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#pragma once

#include "Math/Constants.hlsl"

// Real-valued, second-order (L2) spherical-harmonics basis, in the standard game-engine
// ordering used by e.g. Sloan's "Stupid SH Tricks":
//   [0]    L00
//   [1..3] L1-1, L10, L11
//   [4..8] L2-2, L2-1, L20, L21, L22

static const uint kShCoeffCount = 9;

void EvalSHBasis9(const in float3 _d, out float _basis[9])
{
    _basis[0] = 0.282095f;

    _basis[1] = 0.488603f * _d.y;
    _basis[2] = 0.488603f * _d.z;
    _basis[3] = 0.488603f * _d.x;

    _basis[4] = 1.092548f * _d.x * _d.y;
    _basis[5] = 1.092548f * _d.y * _d.z;
    _basis[6] = 0.315392f * (3.0f * _d.z * _d.z - 1.0f);
    _basis[7] = 1.092548f * _d.x * _d.z;
    _basis[8] = 0.546274f * (_d.x * _d.x - _d.y * _d.y);
}

// Ramamoorthi & Hanrahan clamped-cosine-lobe convolution factor for SH band `l` of `_coeffIndex`.
// Pre-multiplying baked coefficients by this lets shading code reconstruct irradiance (not just
// radiance) with a plain SH dot product.
float ShCosineLobeA(const in uint _coeffIndex)
{
    if (_coeffIndex == 0)
        return kPi;
    if (_coeffIndex <= 3)
        return (2.0f * kPi) / 3.0f;
    return kPi / 4.0f;
}

// Reconstructs diffuse irradiance from a 9-coefficient SH buffer (already pre-multiplied by
// ShCosineLobeA at bake time) for surface normal `_n`.
float3 EvalIrradianceSH9(const in float3 _n, StructuredBuffer<float4> _sh)
{
    float basis[9];
    EvalSHBasis9(_n, basis);

    float3 irradiance = 0.0f.xxx;
    [unroll]
    for (uint i = 0; i < kShCoeffCount; ++i)
        irradiance += _sh[i].rgb * basis[i];

    return irradiance;
}
