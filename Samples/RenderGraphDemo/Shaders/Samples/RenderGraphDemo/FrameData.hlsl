/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

struct FrameData {
    float4x4 m_torusWorldMatrix;

    float4x4 m_viewProjectionMatrix;

    float4x4 m_torusKnotInverseWorldMatrix;

    float3 m_torusAlbedo;
    uint m_torusKnotQ;

    float3 m_sunLightDirection;
    uint m_torusKnotP;

    float3 m_sunDiffuse;
    float m_tanHalfFov;

    float2 m_screenResolution;
    float2 m_depthLinearizationConstants;

    float4 m_cameraQuaternion;

    float3 m_cameraTranslation;
    float m_torusKnotTubeRadius;

    float m_torusKnotRadius;
    float m_torusRoughness;
    float m_torusMetalness;
    uint m_padding[1];
};