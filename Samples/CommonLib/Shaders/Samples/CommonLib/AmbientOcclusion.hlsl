/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FullscreenPassConstants.hlsl"
#include "AmbientOcclusionConstants.h"

// Ground-truth ambient occlusion (Jimenez et al., "Practical Real-Time Strategies for Accurate
// Indirect Occlusion"), ported from Intel's XeGTAO reference implementation
// (https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli) to
// this engine's Z-up, Y-forward view space (OrbitCamera's convention: view-space index 0 = right,
// 1 = forward/depth, 2 = up - see CascadedShadowMap::UpdateCascades) rather than XeGTAO's
// X-right/Y-up/Z-forward one. Wherever the reference treats view-space .xy as the screen-plane
// and .z as depth, this port instead treats .xz as the screen-plane and .y as depth.
// Simplified relative to the reference: no depth mip-chain prefilter pass (samples GBuffer depth
// directly at mip 0 - fine at the sample counts affordable here), no bent-normal output, and no
// half-float packing.

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants: register(b0, space0);
vkBinding(1, 0) ConstantBuffer<AmbientOcclusionConstants> AoConstants: register(b1, space0);
vkBinding(2, 0) Texture2D<float> GBufferDepth: register(t0, space0);
vkBinding(3, 0) Texture2D<float4> GBufferNormal: register(t1, space0);
vkBinding(4, 0) RWTexture2D<unorm float> AoTermOut: register(u0, space0);
vkBinding(5, 0) RWTexture2D<unorm float> AoEdgesOut: register(u1, space0);

static const float kPi = 3.14159265358979323846f;
static const float kHalfPi = 1.57079632679489661923f;

float LinearizeDepth(const in float _depthSs)
{
    return SceneConstants.m_depthLinearizationConstants.x / (_depthSs + SceneConstants.m_depthLinearizationConstants.y);
}

// View-space position for a given (fractional) pixel coordinate and its already-linearized
// view-space depth. Matches the cameraV construction used throughout DeferredShading.hlsl/
// DeferredShadows.hlsl: X = right, Y = forward (depth), Z = up.
float3 ComputeViewPosition(const in float2 _pixelCoord, const in float2 _resolution, const in float _aspect, const in float _viewDepth)
{
    const float2 ndc = ScreenSpaceToNdc(_pixelCoord, _resolution);
    const float3 cameraV = float3(ndc.x * _aspect * SceneConstants.m_tanHalfFov, 1.f, ndc.y * SceneConstants.m_tanHalfFov);
    return _viewDepth * cameraV;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html
float FastSqrt(const in float _x)
{
    return asfloat(0x1fbd1df5 + (asint(_x) >> 1));
}

// Input [-1, 1], output [0, PI].
// https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float FastACos(const in float _x)
{
    const float x = abs(_x);
    float res = -0.156583f * x + kHalfPi;
    res *= FastSqrt(1.f - x);
    return (_x >= 0.f) ? res : kPi - res;
}

float4 CalculateEdges(const in float _centerZ, const in float _leftZ, const in float _rightZ, const in float _topZ, const in float _bottomZ)
{
    float4 edgesLRTB = float4(_leftZ, _rightZ, _topZ, _bottomZ) - _centerZ;

    const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5f;
    const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5f;
    const float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
    return saturate(1.25f - edgesLRTB / (_centerZ * 0.011f));
}

// Packs 4x 2-bit edge weights (0, 1/3, 2/3, 1) into a single normalized byte.
float PackEdges(const in float4 _edgesLRTB)
{
    const float4 rounded = round(saturate(_edgesLRTB) * 3.f);
    return dot(rounded, float4(64.f / 255.f, 16.f / 255.f, 4.f / 255.f, 1.f / 255.f));
}

[numthreads(8, 8, 1)]
void AmbientOcclusionMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoord = id.xy;
    const uint2 resolution = uint2(SceneConstants.m_screenResolution);

    if (any(pixelCoord >= resolution))
    {
        return;
    }

    const float depthSs = GBufferDepth.Load(int3(pixelCoord, 0));
    if (depthSs == 0.f)
    {
        // Reversed-Z: 0 is the far plane / background, nothing to occlude.
        AoTermOut[pixelCoord] = 1.f;
        AoEdgesOut[pixelCoord] = 0.f;
        return;
    }

    const float2 resolutionF = SceneConstants.m_screenResolution;
    const float aspect = resolutionF.x / resolutionF.y;

    const int2 pc = int2(pixelCoord);
    const int2 clampMax = int2(resolution) - 1;

    const float centerZ = LinearizeDepth(depthSs);
    const float leftZ = LinearizeDepth(GBufferDepth.Load(int3(clamp(pc + int2(-1, 0), 0, clampMax), 0)));
    const float rightZ = LinearizeDepth(GBufferDepth.Load(int3(clamp(pc + int2(1, 0), 0, clampMax), 0)));
    const float topZ = LinearizeDepth(GBufferDepth.Load(int3(clamp(pc + int2(0, -1), 0, clampMax), 0)));
    const float bottomZ = LinearizeDepth(GBufferDepth.Load(int3(clamp(pc + int2(0, 1), 0, clampMax), 0)));

    const float4 edgesLRTB = CalculateEdges(centerZ, leftZ, rightZ, topZ, bottomZ);
    AoEdgesOut[pixelCoord] = PackEdges(edgesLRTB);

    const float3 positionV = ComputeViewPosition(float2(pixelCoord) + 0.5f, resolutionF, aspect, centerZ);
    const float3 viewVec = normalize(-positionV);

    const float4 normalSample = GBufferNormal.Load(int3(pixelCoord, 0));
    const float3 normalW = normalSample.rgb * 2.f - 1.f;
    const float3 viewspaceNormal = normalize(Quaternion::Apply(SceneConstants.m_cameraQuaternion, normalW));

    const float effectRadius = AoConstants.m_effectRadius;
    const float sampleDistributionPower = AoConstants.m_sampleDistributionPower;
    const float thinOccluderCompensation = AoConstants.m_thinOccluderCompensation;
    const float falloffRange = max(AoConstants.m_falloffRangeFraction * effectRadius, 1e-4f);
    const float falloffFrom = effectRadius * (1.f - AoConstants.m_falloffRangeFraction);
    const float falloffMul = -1.f / falloffRange;
    const float falloffAdd = falloffFrom / falloffRange + 1.f;

    // World-space size of one pixel, in X, at the current depth - used as a proxy for the local
    // screen-to-world scale (the reference also only ever uses the X component of this).
    const float pixelWorldSize = centerZ * aspect * SceneConstants.m_tanHalfFov * 2.f / resolutionF.x;
    const float screenspaceRadius = effectRadius / max(pixelWorldSize, 1e-6f);

    // Avoid sampling the center pixel.
    const float pixelTooCloseThreshold = 1.3f;
    const float minS = pixelTooCloseThreshold / max(screenspaceRadius, 1e-6f);

    // Interleaved gradient noise (Jorge Jimenez, "Next Generation Post Processing in Call of Duty:
    // Advanced Warfare"), same construction as DeferredShadows.hlsl: a cheap, temporally-stable
    // (no TAA to resolve it away) per-pixel hash, used here to jitter both the slice angle and the
    // per-step radius, same role as XeGTAO's spatio-temporal blue noise input.
    const float2 noiseMagic = float2(0.06711056f, 0.00583715f);
    const float noiseSlice = frac(52.9829189f * frac(dot(float2(pixelCoord), noiseMagic)));
    const float noiseSample = frac(52.9829189f * frac(dot(float2(pixelCoord) + 11.7f, noiseMagic)));

    const uint sliceCount = max(AoConstants.m_sliceCount, 1u);
    const uint stepsPerSlice = max(AoConstants.m_stepsPerSlice, 1u);

    // Fade out for small screen radii - not enough resolvable samples to be meaningful, and about
    // to disappear entirely anyway.
    float visibility = saturate((10.f - screenspaceRadius) / 100.f) * 0.5f;

    for (uint slice = 0; slice < sliceCount; slice++)
    {
        const float sliceK = (float(slice) + noiseSlice) / float(sliceCount);
        const float phi = sliceK * kPi;
        float sinPhi, cosPhi;
        sincos(phi, sinPhi, cosPhi);

        // Pixel-space offset direction for actual sampling; screen Y grows downward here, hence
        // the sign flip relative to directionVec below (whose Z component follows NDC's
        // positive-up convention).
        const float2 omega = float2(cosPhi, -sinPhi) * screenspaceRadius;

        // Screen-plane axes are X (right) and Z (up) in this engine's view space, depth is Y -
        // unlike XeGTAO's Z-forward convention, where the screen plane is XY and depth is Z.
        const float3 directionVec = float3(cosPhi, 0.f, sinPhi);
        const float3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;
        const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));
        const float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

        const float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));
        const float projectedNormalVecLengthRaw = length(projectedNormalVec);
        const float cosNorm = saturate(dot(projectedNormalVec, viewVec) / max(projectedNormalVecLengthRaw, 1e-6f));
        const float n = signNorm * FastACos(cosNorm);

        const float lowHorizonCos0 = cos(n + kHalfPi);
        const float lowHorizonCos1 = cos(n - kHalfPi);
        float horizonCos0 = lowHorizonCos0;
        float horizonCos1 = lowHorizonCos1;

        for (uint step = 0; step < stepsPerSlice; step++)
        {
            // R1 sequence noise (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
            const float stepBaseNoise = float(slice + step * stepsPerSlice) * 0.6180339887498948f;
            const float stepNoise = frac(noiseSample + stepBaseNoise);

            float s = (float(step) + stepNoise) / float(stepsPerSlice);
            s = pow(s, sampleDistributionPower);
            s += minS;

            // Snap to whole pixels: avoids interpolation artifacts, and lets us sample via Load
            // instead of needing a sampler resource at all.
            const int2 sampleOffsetPixels = int2(round(s * omega));
            const int2 pixel0 = clamp(pc + sampleOffsetPixels, 0, clampMax);
            const int2 pixel1 = clamp(pc - sampleOffsetPixels, 0, clampMax);

            const float sz0 = LinearizeDepth(GBufferDepth.Load(int3(pixel0, 0)));
            const float sz1 = LinearizeDepth(GBufferDepth.Load(int3(pixel1, 0)));

            const float3 samplePos0 = ComputeViewPosition(float2(pixel0) + 0.5f, resolutionF, aspect, sz0);
            const float3 samplePos1 = ComputeViewPosition(float2(pixel1) + 0.5f, resolutionF, aspect, sz1);

            const float3 sampleDelta0 = samplePos0 - positionV;
            const float3 sampleDelta1 = samplePos1 - positionV;
            const float sampleDist0 = length(sampleDelta0);
            const float sampleDist1 = length(sampleDelta1);

            const float3 sampleHorizonVec0 = sampleDelta0 / max(sampleDist0, 1e-6f);
            const float3 sampleHorizonVec1 = sampleDelta1 / max(sampleDist1, 1e-6f);

            // Thin-occluder heuristic: scale the depth-axis (Y here) component before measuring
            // distance, so a thin occluder just in front of the receiver is discarded sooner
            // instead of reading as an infinitely thick wall.
            const float falloffBase0 = length(float3(sampleDelta0.x, sampleDelta0.y * (1.f + thinOccluderCompensation), sampleDelta0.z));
            const float falloffBase1 = length(float3(sampleDelta1.x, sampleDelta1.y * (1.f + thinOccluderCompensation), sampleDelta1.z));
            const float weight0 = saturate(falloffBase0 * falloffMul + falloffAdd);
            const float weight1 = saturate(falloffBase1 * falloffMul + falloffAdd);

            float shc0 = dot(sampleHorizonVec0, viewVec);
            float shc1 = dot(sampleHorizonVec1, viewVec);
            shc0 = lerp(lowHorizonCos0, shc0, weight0);
            shc1 = lerp(lowHorizonCos1, shc1, weight1);

            horizonCos0 = max(horizonCos0, shc0);
            horizonCos1 = max(horizonCos1, shc1);
        }

        // Compensates a slight overdarkening on high slopes (see XeGTAO's own comment on this same fudge).
        const float projectedNormalVecLength = lerp(projectedNormalVecLengthRaw, 1.f, 0.05f);

        const float h0 = -FastACos(horizonCos1);
        const float h1 = FastACos(horizonCos0);
        const float iarc0 = (cosNorm + 2.f * h0 * sin(n) - cos(2.f * h0 - n)) / 4.f;
        const float iarc1 = (cosNorm + 2.f * h1 * sin(n) - cos(2.f * h1 - n)) / 4.f;
        visibility += projectedNormalVecLength * (iarc0 + iarc1);
    }

    visibility /= float(sliceCount);
    visibility = pow(max(visibility, 0.f), AoConstants.m_finalValuePower);
    // Disallow total occlusion: the pixel is visible, so it must be receiving *some* light.
    visibility = max(0.03f, visibility);
    visibility = lerp(1.f, visibility, AoConstants.m_intensity);

    AoTermOut[pixelCoord] = visibility;
}
