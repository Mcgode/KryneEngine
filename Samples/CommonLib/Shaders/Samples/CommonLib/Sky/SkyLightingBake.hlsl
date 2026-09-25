/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

// Integrates sky (and ground) radiance into a 2nd-order (9-coefficient) spherical-harmonics
// irradiance map, replacing a plain dual-hemisphere top/bottom average. Unlike a hemisphere
// lerp, SH keeps directional information beyond the up axis, so it can represent the sun-side
// vs anti-sun-side asymmetry of a sunset sky instead of just a vertical gradient.
//
// Dispatch: 1 group x 128 threads, one direction sampled per thread over the full sphere.
// Output buffer layout (9 x float4, one per SH coefficient, colour in .rgb):
//   [0]    = L00               (band 0)
//   [1..3] = L1-1, L10, L11    (band 1)
//   [4..8] = L2-2, L2-1, L20, L21, L22 (band 2)
// Coefficients are pre-multiplied by the clamped-cosine convolution factor (ShCosineLobeA), so
// DeferredShading.hlsl can reconstruct diffuse irradiance with a plain SH dot product.

#include "Platform.hlsl"
#include "Math/Constants.hlsl"
#include "Atmosphere.hlsli"
#include "SphericalHarmonics.hlsli"
#include "../FullscreenPassConstants.hlsl"

static const uint kSampleCount = 128;

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants : register(b0, space0);
vkBinding(1, 0) RWStructuredBuffer<float4> SkyAmbientOut : register(u0, space0);

groupshared float3 gs_radiance[kSampleCount];
groupshared float3 gs_reduce[kSampleCount];

[numthreads(kSampleCount, 1, 1)]
void SkyLightingBakeMain(uint3 LocalID : SV_GroupThreadID)
{
    // Full-sphere Fibonacci lattice: cosTheta spans [-1, 1] uniformly across the whole sphere.
    static const float kGoldenAngle = 2.399963229728653f; // 2*pi*(1 - 1/phi)

    const uint i = LocalID.x;

    const float cosTheta = 1.0f - 2.0f * (float(i) + 0.5f) / float(kSampleCount);
    const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    const float phi      = float(i) * kGoldenAngle;
    const float3 dir     = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Sampled unconditionally over the full sphere, with height clamped to ground level (this eye
    // sits exactly at height 0, unlike the interactive sky render's real camera), so downward
    // directions smoothly extinguish toward black instead of needing a hard sky/ground branch
    // here. Avoiding that branch matters for SH quality: a genuine step discontinuity is exactly
    // what a low-order (L2) SH basis rings badly on.
    const float3 eyePosW = float3(0.0f, 0.0f, kAtmospherePlanetRadius);
    const AtmoRay rayW   = { eyePosW, dir };
    const float3  radiance = AtmoGetIncidentLight(
        rayW, SceneConstants.m_sunLightDirection, SceneConstants.m_sunDiffuse, /* _clampHeightToGround */ true);

    gs_radiance[i] = radiance * kPi;

    float basis[kShCoeffCount];
    EvalSHBasis9(dir, basis);

    // Uniform solid-angle weight (equal-area lattice over the full 4*pi sphere).
    const float sampleWeight = (4.0f * kPi) / float(kSampleCount);

    GroupMemoryBarrierWithGroupSync();

    for (uint c = 0; c < kShCoeffCount; ++c)
    {
        gs_reduce[i] = gs_radiance[i] * basis[c] * sampleWeight;
        GroupMemoryBarrierWithGroupSync();

        for (uint stride = kSampleCount / 2; stride >= 1; stride >>= 1)
        {
            if (i < stride)
                gs_reduce[i] += gs_reduce[i + stride];
            GroupMemoryBarrierWithGroupSync();
        }

        if (i == 0)
            SkyAmbientOut[c] = float4(gs_reduce[0] * ShCosineLobeA(c), 1.0f);

        GroupMemoryBarrierWithGroupSync();
    }
}
