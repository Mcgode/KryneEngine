/**
 * @file
 * @author Max Godefroy
 * @date 29/08/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Samples
{
    /**
     * @brief Constant buffer shared by every fullscreen pass (Sky, DeferredShading, ColorMapping).
     *
     * @details
     * Holds the union of the view/lighting data any of those passes may sample. Applications own the
     * GPU buffer; they just have to guarantee that its first bytes match this layout.
     *
     * Keep in sync with `FullscreenPassConstants` in
     * `Shaders/Samples/CommonLib/FullscreenPassConstants.hlsl`.
     */
    struct alignas(16) FullscreenPassConstants
    {
        float4 m_cameraQuaternion;

        float3 m_cameraTranslation;
        float m_tanHalfFov;

        float2 m_screenResolution;
        float2 m_depthLinearizationConstants;

        float3 m_sunLightDirection;
        float m_padding0;

        float3 m_sunDiffuse;
        float m_padding1;
    };
    static_assert(sizeof(FullscreenPassConstants) == 80, "FullscreenPassConstants must match its HLSL counterpart");

    namespace FullscreenPassCommon
    {
        /**
         * @brief Creates the descriptor set layout expected by the fullscreen passes for the shared
         * #FullscreenPassConstants buffer: a single constant buffer bound at (b0, space0).
         *
         * @param _graphicsContext The graphics context used to create the layout.
         * @param _bindingIndices Optional single-element output filled with the binding index of the
         * constant buffer (needed when writing the descriptor set).
         */
        [[nodiscard]] DescriptorSetLayoutHandle CreateConstantsDescriptorSetLayout(
            GraphicsContext* _graphicsContext,
            u32* _bindingIndices = nullptr);
    }
}
