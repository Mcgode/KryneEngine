/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#pragma once

#if defined(__cplusplus)
#   include <KryneEngine/Core/Common/Types.hpp>

using namespace KryneEngine;
using uint = u32;
#endif

// Tunables for AmbientOcclusionPass's GTAO-derived screen-space ambient occlusion.
// Keep the layout in sync with the member fields `AmbientOcclusionPass::UpdateConstants` writes
// from (C++).
struct AmbientOcclusionConstants
{
    float m_effectRadius;             // World-space AO radius.
    float m_falloffRangeFraction;     // Fraction of the radius over which occlusion fades to zero.
    float m_sampleDistributionPower;  // > 1 biases samples toward the outer radius, < 1 toward the center.
    float m_thinOccluderCompensation; // [0, 1]; stops thin occluders from reading as infinitely thick.

    float m_finalValuePower;          // Contrast power applied to the averaged visibility term.
    float m_denoiseBlurBeta;          // Edge-aware denoise centre-sample weight.
    float m_intensity;                // 0 fades the effect out entirely, 1 is the full computed term.
    uint m_sliceCount;

    uint m_stepsPerSlice;
    float m_padding0;
    float m_padding1;
    float m_padding2;
};
