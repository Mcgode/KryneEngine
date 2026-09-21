/**
 * @file
 * @author Max Godefroy
 * @date 21/09/2026.
 */

#pragma once

#if defined(__cplusplus)
#   include <KryneEngine/Core/Math/Vector.hpp>
#   include <KryneEngine/Core/Math/Matrix.hpp>

using namespace KryneEngine;
using uint = u32;
#endif

#define CSM_MAX_CASCADES 4

enum ShadowTechnique
{
    SHADOW_TECHNIQUE_PCSS = 0,
    SHADOW_TECHNIQUE_DPCF = 1,
};

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

    uint m_cascadeCount;
    float m_shadowBiasConstantTexels; // Base normal-offset bias, in shadow-map texels of the receiving cascade
    float m_shadowBiasSlopeScale;     // Extra normal-offset added at grazing angles (scaled by 1 - N.L)
    uint m_shadowTechnique;

    float m_pcssTanHalfLightAngle;            // PCSS light size, as a fraction of a cascade's shadow-map width
    // DPCF's fixed filter kernel radius, in shadow-map texels of the receiving cascade. Unlike
    // PCSS's penumbra, this does not track the true per-pixel penumbra size - it's tuned once as a
    // plausible maximum. Unused when m_shadowTechnique is PCSS.
    float m_dpcfKernelTexels;
    uint m_padding[2];
};