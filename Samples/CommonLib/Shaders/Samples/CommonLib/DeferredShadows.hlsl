/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "FullscreenPassConstants.hlsl"
#include "CascadeConstants.h"

vkBinding(0, 0) ConstantBuffer<FullscreenPassConstants> SceneConstants: register(b0, space0);
vkBinding(1, 0) ConstantBuffer<CascadeConstants> Cascades: register(b1, space0);
vkBinding(2, 0) Texture2D<float> GBufferDepth: register(t0, space0);
vkBinding(3, 0) Texture2D<float4> GBufferNormal: register(t1, space0);
vkBinding(4, 0) Texture2DArray<float> ShadowCascades: register(t2, space0);
vkBinding(5, 0) SamplerState Sampler: register(s0, space0);
vkBinding(6, 0) RWTexture2D<float> DeferredShadows: register(u0, space0);

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

    const float4 depths = ShadowCascades.GatherRed(Sampler, float3(_uv, _cascadeIndex));
    // Gather4 pattern is:
    // | W | Z |
    // | X | Y |
    // See https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/gather4--sm5---asm-
    return lerp(lerp(depths.w, depths.z, f.x), lerp(depths.x, depths.y, f.x), f.y);
}

// Classic PCSS (Fernando, "Percentage-Closer Soft Shadows", NVIDIA 2005): unlike DPCF's single
// fixed-radius pass, this resizes its filter kernel to the true per-pixel penumbra, found via a
// separate blocker-search pass first. For a directional (orthographic) light, the eventual
// penumbra radius is tan(0.5 * angularSize) * gap, where gap is the world-space distance between
// receiver and blocker along the light's forward axis.
float ResolvePcss(
    const in uint _cascadeIndex,
    const in float2 _shadowUv,
    const in float _receiverDepth,
    const in float _receiverWorldDepth,
    const in float _texelWorldSize,
    const in float _worldToUv,
    const in float _depthToWorld,
    const in float _rotation)
{
    // --- PCSS step 1: blocker search ---
    // The true gap isn't known yet - that's exactly what this search is for - so use the
    // receiver's own distance from the cascade's near plane as a conservative stand-in: a blocker
    // can never be further from the receiver, along the light axis, than the receiver already is
    // from the near plane.
    const float searchRadiusWorld = clamp(
        Cascades.m_pcssTanHalfLightAngle * _receiverWorldDepth,
        Cascades.m_pcssMinPenumbraTexels * _texelWorldSize,
        Cascades.m_pcssMaxPenumbraTexels * _texelWorldSize);
    const float searchRadiusUv = searchRadiusWorld * _worldToUv;

    float blockerSum = 0.f;
    uint blockerCount = 0;
    for (uint i = 0; i < Cascades.m_pcssBlockerSearchTaps; i++)
    {
        const float2 offset = VogelDiskSample(i, Cascades.m_pcssBlockerSearchTaps, _rotation) * searchRadiusUv;
        const float2 sampleUv = _shadowUv + offset;
        const float sampleDepth = any(sampleUv <= 0.f) || any(sampleUv >= 1.f)
            ? 1.f :
            SampleDepthBilinear(_cascadeIndex, sampleUv);

        if (sampleDepth < _receiverDepth)
        {
            blockerSum += sampleDepth;
            blockerCount++;
        }
    }

    if (blockerCount == 0)
    {
        // No occluders found within the search radius: fully lit.
        return 1.f;
    }

    const float avgBlockerDepth = blockerSum / float(blockerCount);
    const float avgBlockerWorldDepth = avgBlockerDepth * _depthToWorld;

    // --- PCSS step 2: penumbra size + filtered shadow test ---
    const float gapWorld = _receiverWorldDepth - avgBlockerWorldDepth;
    const float penumbraWorld = clamp(
        Cascades.m_pcssTanHalfLightAngle * gapWorld,
        Cascades.m_pcssMinPenumbraTexels * _texelWorldSize,
        Cascades.m_pcssMaxPenumbraTexels * _texelWorldSize);
    const float penumbraUv = penumbraWorld * _worldToUv;

    // Percentage-closer filter over the penumbra disk: each tap is a plain binary occluded/lit
    // test (no artificial softening of the individual comparison) - the softness of the final
    // result comes entirely from averaging many taps spread across the penumbra radius, which is
    // what actually makes the softening physically track the estimated penumbra size.
    float visibleTaps = 0.f;
    for (uint j = 0; j < Cascades.m_pcssFilterTaps; j++)
    {
        const float2 offset = VogelDiskSample(j, Cascades.m_pcssFilterTaps, _rotation) * penumbraUv;
        const float2 sampleUv = _shadowUv + offset;
        if (any(sampleUv <= 0.f) || any(sampleUv >= 1.f))
        {
            visibleTaps += 1.f;
            continue;
        }

        const float sampleDepth = SampleDepthBilinear(_cascadeIndex, sampleUv);
        visibleTaps += sampleDepth >= _receiverDepth ? 1.f : 0.f;
    }

    return visibleTaps / float(Cascades.m_pcssFilterTaps);
}

// Dilated Percentage Closer Filtering (Myers, "Shadows of Cold War: A Scalable Approach to
// Shadowing", GDC 2021): where PCSS resizes its filter kernel to the true per-pixel penumbra
// (found via a separate blocker-search pass), DPCF instead samples a single, fixed-size kernel -
// sized off Cascades.m_dpcfKernelTexels, not derived per pixel - and fakes the "hardens near
// contact" look by reshaping the *response curve* of the raw PCF ratio using statistics gathered
// for free from that same one pass (how many of its raw samples are occluders, and how close their
// average is to the receiver). No second pass, no dependent search: this is the whole point of the
// technique - trading the physical accuracy of a true per-pixel penumbra for a much cheaper single
// fixed-radius loop.
float ResolveDpcf(
    const in uint _cascadeIndex,
    const in float2 _shadowUv,
    const in float _receiverDepth,
    const in float _texelWorldSize,
    const in float _worldToUv,
    const in float _rotation)
{
    const float kernelUv = Cascades.m_dpcfKernelTexels * _texelWorldSize * _worldToUv;

    float occluderCount = 0.f;
    float occluderGapSum = 0.f;
    for (uint i = 0; i < Cascades.m_dpcfTaps; i++)
    {
        const float2 offset = VogelDiskSample(i, Cascades.m_dpcfTaps, _rotation) * kernelUv;
        const float2 sampleUv = _shadowUv + offset;
        const float4 depths = any(sampleUv <= 0.f) || any(sampleUv >= 1.f)
            ? float4(1.f, 1.f, 1.f, 1.f)
            : ShadowCascades.GatherRed(Sampler, float3(sampleUv, _cascadeIndex));

        for (uint d = 0; d < 4; d++)
        {
            // Positive when depths[d] is closer to the light than the receiver, i.e. an occluder -
            // same sign convention as the PCSS blocker search above.
            const float gap = _receiverDepth - depths[d];
            const float occluder = gap > 0.f ? 1.f : 0.f;
            occluderCount += occluder;
            occluderGapSum += gap * occluder;
        }
    }

    const float totalSamples = float(Cascades.m_dpcfTaps * 4);
    if (occluderCount == 0.f)
    {
        // No occluders in the fixed kernel: fully lit, same fast-out as the PCSS path.
        return 1.f;
    }
    if (occluderCount == totalSamples)
    {
        // Every sample occluded: fully shadowed, no point running the curve remap below.
        return 0.f;
    }

    const float occluderAvgGap = occluderGapSum / occluderCount;
    // 0 when the average occluder sits right at the receiver (contact), 1 when it's as far from
    // the receiver as the receiver itself is from the light - i.e. how close to a hard contact
    // this pixel's shadow is.
    const float contactWeight = saturate(occluderAvgGap / _receiverDepth);

    float percentageOccluded = occluderCount / totalSamples;

    // Remap through -1..1 so a cubic curve can push values away from the midpoint (sharper
    // transition) without disturbing the 0/1 endpoints, then blend that sharpened curve in as
    // contactWeight -> 0 (occluder close: hardened) and back out to the raw linear PCF ratio as
    // contactWeight -> 1 (occluder far: soft, ordinary PCF).
    percentageOccluded = 2.f * percentageOccluded - 1.f;
    const float occludedSign = sign(percentageOccluded);
    percentageOccluded = 1.f - occludedSign * percentageOccluded;
    const float hardened = percentageOccluded * percentageOccluded * percentageOccluded;
    percentageOccluded = lerp(hardened, percentageOccluded, contactWeight);
    percentageOccluded = 1.f - percentageOccluded;
    percentageOccluded *= occludedSign;
    percentageOccluded = 0.5f * percentageOccluded + 0.5f;

    return 1.f - percentageOccluded;
}

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
    // We cap the receiver depth to 1, as it is the max value of the depth buffer. Going over it would
    // result in incorrect shadowing.
    const float receiverDepth = min(lightClip.z, 1);
    const float receiverWorldDepth = receiverDepth * depthToWorld;

    const float rotation = InterleavedGradientNoise(float2(pixelCoordinates)) * 6.2831853f;

    const float visibility = (Cascades.m_shadowTechnique == ShadowTechnique::SHADOW_TECHNIQUE_DPCF)
        ? ResolveDpcf(cascadeIndex, shadowUv, receiverDepth, texelWorldSize, worldToUv, rotation)
        : ResolvePcss(cascadeIndex, shadowUv, receiverDepth, receiverWorldDepth, texelWorldSize, worldToUv, depthToWorld, rotation);

    DeferredShadows[pixelCoordinates] = visibility;
}
