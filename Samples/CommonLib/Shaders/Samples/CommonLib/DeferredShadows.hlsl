/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FullscreenPassConstants.hlsl"

#define CSM_MAX_CASCADES 4

// Keep in sync with `KryneEngine::Samples::CascadedShadowMap::ConstantsBuffer` (C++).
struct CascadeConstants
{
    float4x4 m_cascadeViewProj[CSM_MAX_CASCADES];
    float4 m_cascadeSplitDepths;    // view-space far distance of each cascade, one per component
    float4 m_cascadeTexelWorldSize; // world units covered by one shadow-map texel, one per cascade
    float4 m_cascadeDepthRangeInv;  // 1 / (far - near) of each cascade's light-space depth range
    // Light forward direction, the same across every cascade (one directional light).
    float3 m_lightForward;
    // Width of the dithered cascade-transition band, as a fraction of the split distance it
    // straddles. Shared across every cascade, so a single float suffices - reuses what would
    // otherwise be m_lightForward's trailing alignment padding.
    float m_cascadeBlendBandFraction;
    float m_lightSizeUv;            // PCSS light size, as a fraction of a cascade's shadow-map width
    uint m_cascadeCount;
    float m_shadowBiasConstantTexels; // Base normal-offset bias, in shadow-map texels of the receiving cascade
    float m_shadowBiasSlopeScale;     // Extra normal-offset added at grazing angles (scaled by 1 - N.L)
};

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants: register(b0, space0);
vkBinding(1, 0) ConstantBuffer<CascadeConstants> Cascades: register(b1, space0);
vkBinding(2, 0) Texture2D<float> GBufferDepth: register(t0, space0);
vkBinding(3, 0) Texture2D<float4> GBufferNormal: register(t1, space0);
vkBinding(4, 0) Texture2DArray<float> ShadowCascades: register(t2, space0);
vkBinding(5, 0) RWTexture2D<float> DeferredShadows: register(u0, space0);

// Interleaved gradient noise (Jorge Jimenez, "Next Generation Post Processing in Call of Duty:
// Advanced Warfare"): a cheap per-pixel pseudo-random value, stable across frames (no temporal
// accumulation/TAA needed to resolve it away) used both to rotate the PCF sample pattern below
// and to dither the cascade transition (see SelectCascade).
float InterleavedGradientNoise(const in float2 _pixelCoord)
{
    const float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(_pixelCoord, magic.xy)));
}

// Picks which cascade a pixel resolves its shadow against, dithering between cascade i and i+1
// over a band straddling their shared split distance instead of hard-cutting at it. Without this,
// a pixel flips instantaneously from one cascade's shadow-map resolution/bias to another's as it
// crosses the split, which reads as a visible seam; dithering trades that hard seam for a
// stipple-noise transition, at no extra shadow-map sampling cost (the rest of the resolve still
// only ever touches the one cascade index this function returns).
uint SelectCascade(const in float _viewDepth, const in float2 _pixelCoord)
{
    for (uint i = 0; i < Cascades.m_cascadeCount - 1; i++)
    {
        const float splitFar = Cascades.m_cascadeSplitDepths[i];
        const float halfBand = 0.5f * Cascades.m_cascadeBlendBandFraction * splitFar;
        const float bandStart = splitFar - halfBand;

        if (_viewDepth < bandStart)
        {
            return i;
        }

        const float bandEnd = splitFar + halfBand;
        if (_viewDepth < bandEnd)
        {
            // Inside the transition band: dither between i and i+1 based on how far across the
            // band _viewDepth sits, using a noise hash decorrelated from the PCF kernel rotation's
            // (same function, different input) so the two dither patterns don't visibly align.
            const float t = saturate((_viewDepth - bandStart) / max(bandEnd - bandStart, 1e-5f));
            const float ditherNoise = InterleavedGradientNoise(_pixelCoord + 17.f);
            return ditherNoise < t ? i + 1 : i;
        }
    }
    return Cascades.m_cascadeCount - 1;
}

// One sample of a Vogel disk: N points distributed over a unit disk via the golden angle, giving
// a low-discrepancy (no clumping, no grid artifacts) pattern for any sample count, unlike a
// rounded NxN grid. _rotation (radians) rotates the whole pattern, so different pixels sample
// different sub-texel offsets instead of all snapping to the same few positions - this is what
// actually removes the blockiness a small fixed grid produces.
float2 VogelDiskSample(const in uint _index, const in uint _count, const in float _rotation)
{
    const float kGoldenAngle = 2.39996323f;
    const float r = sqrt((float(_index) + 0.5f) / float(_count));
    const float theta = float(_index) * kGoldenAngle + _rotation;
    float s, c;
    sincos(theta, s, c);
    return r * float2(c, s);
}

// Bilinearly filtered raw depth, across the 4 nearest texels, instead of a single nearest-texel
// point sample (Load). A point sample is piecewise-constant over each texel, so as the receiver
// slides continuously across the screen, any value derived from it (blocker average, shadow
// test) can only change in discrete per-texel-crossing jumps; bilinear filtering makes it vary
// continuously instead, which is what actually removes the blocky/stepped look - not just at the
// final shadow test, but anywhere a shadow-map depth value feeds into a later computation (e.g.
// the blocker search's average, which otherwise stair-steps the penumbra size it drives).
float SampleDepthBilinear(const in uint _cascadeIndex, const in float2 _uv)
{
    uint width, height, elements;
    ShadowCascades.GetDimensions(width, height, elements);

    const float2 texelCoord = _uv * float2(width, height) - 0.5f;
    const float2 base = floor(texelCoord);
    const float2 f = texelCoord - base;
    const int2 baseInt = int2(base);

    const float d00 = ShadowCascades.Load(int4(baseInt + int2(0, 0), _cascadeIndex, 0));
    const float d10 = ShadowCascades.Load(int4(baseInt + int2(1, 0), _cascadeIndex, 0));
    const float d01 = ShadowCascades.Load(int4(baseInt + int2(0, 1), _cascadeIndex, 0));
    const float d11 = ShadowCascades.Load(int4(baseInt + int2(1, 1), _cascadeIndex, 0));

    return lerp(lerp(d00, d10, f.x), lerp(d01, d11, f.x), f.y);
}

static const uint kBlockerSearchTaps = 32;
static const uint kFilterTaps = 64;

// Clamp the blocker-search and penumbra radii to a few shadow-map texels at minimum (below that,
// PCF can only smooth the transition between texels, not the underlying per-texel step pattern
// the depth buffer already recorded, so a smaller radius just reads as a jagged silhouette) and
// to a generous number of texels at maximum (an unbounded radius eventually starts averaging in
// completely unrelated, unshadowed geometry near a contact point, which reads as the shadow
// detaching from its caster - "peter-panning" - even though it is really just an over-wide blur).
static const float kMinPenumbraTexels = 2.5f;
static const float kMaxPenumbraTexels = 64.f;

[numthreads(8, 8, 1)]
void DeferredShadowsMain(const uint3 id: SV_DispatchThreadID)
{
    const uint2 pixelCoordinates = id.xy;
    const uint2 resolution = uint2(SceneConstants.m_screenResolution);

    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    const float depthSs = GBufferDepth.Load(int3(pixelCoordinates, 0));
    if (depthSs == 0.f)
    {
        // Reversed-Z: 0 is the far plane / background, nothing to shadow.
        return;
    }

    const float2 ndc = ScreenSpaceToNdc(pixelCoordinates, resolution);
    const float aspect = float(resolution.x) / float(resolution.y);
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov);

    const float depthV = SceneConstants.m_depthLinearizationConstants.x / (depthSs + SceneConstants.m_depthLinearizationConstants.y);
    const float3 positionV = depthV * cameraV;
    const float4 vsToWsQuaternion = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    float3 positionW = Quaternion::Apply(vsToWsQuaternion, positionV - SceneConstants.m_cameraTranslation);

    const float4 gBufferNormalSample = GBufferNormal.Load(int3(pixelCoordinates, 0));
    const float3 normalW = gBufferNormalSample.rgb * 2.f - 1.f;

    const uint cascadeIndex = SelectCascade(depthV, float2(pixelCoordinates));
    const float texelWorldSize = Cascades.m_cascadeTexelWorldSize[cascadeIndex];

    // Normal offset bias (as used e.g. by Unity's cascaded shadow maps): nudge the receiver along
    // its own surface normal, in world space, before reprojecting it into the light - rather than
    // fudging the depth comparison after the fact. This moves *which* shadow-map texel ends up
    // being tested, sidestepping self-shadowing acne at any surface slope without needing a
    // depth-bias term that blows up at grazing angles. The offset grows at grazing angles (low
    // N.L) via m_shadowBiasSlopeScale, since those are exactly the surfaces a flat offset alone
    // undercorrects - similar in spirit to a slope-scaled depth bias, but applied geometrically.
    const float nDotL = saturate(dot(normalW, -Cascades.m_lightForward.xyz));
    const float normalOffsetTexels = Cascades.m_shadowBiasConstantTexels
        + Cascades.m_shadowBiasSlopeScale * (1.f - nDotL);
    positionW += normalW * texelWorldSize * normalOffsetTexels;

    const float4 lightClip = mul(float4(positionW, 1.f), Cascades.m_cascadeViewProj[cascadeIndex]);
    const float2 shadowUv = float2(lightClip.x * 0.5f + 0.5f, 0.5f - lightClip.y * 0.5f);

    if (any(shadowUv < 0.f) || any(shadowUv > 1.f))
    {
        // Outside this cascade's fitted bounds (e.g. past the max shadow distance): no data,
        // default to unshadowed rather than an incorrect hard edge.
        DeferredShadows[pixelCoordinates] = 1.f;
        return;
    }

    uint shadowWidth, shadowHeight, shadowElements;
    ShadowCascades.GetDimensions(shadowWidth, shadowHeight, shadowElements);

    // World units covered by the whole cascade, and the reverse: how much normalized [0, 1]
    // shadow-map depth changes per world unit along the light's forward axis. Every PCSS distance
    // below is carried in world space and only converted to UV/depth space at the point of use -
    // mixing normalized depth or UV values into what's supposed to be an angular/world-space size
    // is what forces unexplained fudge factors elsewhere to compensate for the resulting wrong
    // units.
    const float orthoWidth = texelWorldSize * float(shadowWidth);
    const float worldToUv = 1.f / orthoWidth;
    const float depthToWorld = 1.f / Cascades.m_cascadeDepthRangeInv[cascadeIndex];

    // No depth bias left to apply here: self-shadowing acne is already handled geometrically, by
    // the normal offset applied to positionW above, before it was ever reprojected into the light.
    const float receiverDepth = lightClip.z;
    const float receiverWorldDepth = receiverDepth * depthToWorld;

    const float rotation = InterleavedGradientNoise(float2(pixelCoordinates)) * 6.2831853f;

    // --- PCSS step 1: blocker search ---
    // For a directional (orthographic) light, the eventual penumbra radius is
    // tan(0.5 * angularSize) * gap, where gap is the world-space distance between receiver and
    // blocker along the light's forward axis (see step 2 below). The true gap isn't known yet -
    // that's exactly what this search is for - so use the receiver's own distance from the
    // cascade's near plane as a conservative stand-in: a blocker can never be further from the
    // receiver, along the light axis, than the receiver already is from the near plane.
    const float searchRadiusWorld = clamp(
        Cascades.m_lightSizeUv * receiverWorldDepth,
        kMinPenumbraTexels * texelWorldSize,
        kMaxPenumbraTexels * texelWorldSize);
    const float searchRadiusUv = searchRadiusWorld * worldToUv;

    float blockerSum = 0.f;
    uint blockerCount = 0;
    for (uint i = 0; i < kBlockerSearchTaps; i++)
    {
        const float2 offset = VogelDiskSample(i, kBlockerSearchTaps, rotation) * searchRadiusUv;
        const float2 sampleUv = shadowUv + offset;
        const float sampleDepth = any(sampleUv <= 0.f) || any(sampleUv >= 1.f)
            ? 1.f :
            SampleDepthBilinear(cascadeIndex, sampleUv);

        if (sampleDepth != 1.f && sampleDepth < receiverDepth)
        {
            blockerSum += sampleDepth;
            blockerCount++;
        }
    }

    if (blockerCount == 0)
    {
        // No occluders found within the search radius: fully lit.
        DeferredShadows[pixelCoordinates] = 1.f;
        return;
    }

    const float avgBlockerDepth = blockerSum / float(blockerCount);
    const float avgBlockerWorldDepth = avgBlockerDepth * depthToWorld;

    // --- PCSS step 2: penumbra size + filtered shadow test ---
    const float gapWorld = receiverWorldDepth - avgBlockerWorldDepth;
    const float penumbraWorld = clamp(
        Cascades.m_lightSizeUv * gapWorld,
        kMinPenumbraTexels * texelWorldSize,
        kMaxPenumbraTexels * texelWorldSize);
    const float penumbraUv = penumbraWorld * worldToUv;

    // Percentage-closer filter over the penumbra disk: each tap is a plain binary occluded/lit
    // test (no artificial softening of the individual comparison) - the softness of the final
    // result comes entirely from averaging many taps spread across the penumbra radius, which is
    // what actually makes the softening physically track the estimated penumbra size.
    float visibleTaps = 0.f;
    for (uint j = 0; j < kFilterTaps; j++)
    {
        const float2 offset = VogelDiskSample(j, kFilterTaps, rotation) * penumbraUv;
        const float2 sampleUv = shadowUv + offset;
        if (any(sampleUv <= 0.f) || any(sampleUv >= 1.f))
        {
            visibleTaps += 1.f;
            continue;
        }

        const float sampleDepth = SampleDepthBilinear(cascadeIndex, sampleUv);
        visibleTaps += sampleDepth >= receiverDepth ? 1.f : 0.f;
    }

    DeferredShadows[pixelCoordinates] = visibleTaps / float(kFilterTaps);
}
