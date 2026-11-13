/**
 * @file
 * @author Max Godefroy
 * @date 29/08/2026.
 */

#pragma once

#include "Platform.hlsl"

// Constant buffer shared by every fullscreen pass (Sky, DeferredShading, ColorMapping).
// It holds the union of the view/lighting data any of those passes may sample.
// Keep the layout in sync with `KryneEngine::Samples::FullscreenPassConstants` (C++).
struct FullscreenPassConstants
{
    float4 m_cameraQuaternion;

    PackedFloat3 m_cameraTranslation; // Packed so m_tanHalfFov shares its 16 bytes on every target
    float m_tanHalfFov;

    float2 m_screenResolution;
    float2 m_depthLinearizationConstants;

    float3 m_sunLightDirection;

    float3 m_sunDiffuse;
};
