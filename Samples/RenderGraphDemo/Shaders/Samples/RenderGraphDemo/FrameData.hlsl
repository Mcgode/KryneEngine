/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

#include "FullscreenPassConstants.hlsl"

struct FrameData {
    // View/lighting data shared with the fullscreen passes. Must stay the first member so the buffer can
    // also be bound as a `FullscreenPassConstants` constant buffer.
    FullscreenPassConstants m_fullscreen;

    float4x4 m_torusWorldMatrix;

    float4x4 m_viewProjectionMatrix;

    float4x4 m_torusKnotInverseWorldMatrix;

    PackedFloat3 m_torusAlbedo; // Packed so m_torusKnotQ shares its 16 bytes on every target
    uint m_torusKnotQ;

    uint m_torusKnotP;
    float m_torusKnotTubeRadius;
    float m_torusKnotRadius;
    float m_torusRoughness;

    float m_torusMetalness;
    // Scalars rather than `uint[3]`: D3D cbuffer arrays use a 16-byte element stride
    uint m_padding0;
    uint m_padding1;
    uint m_padding2;
};
