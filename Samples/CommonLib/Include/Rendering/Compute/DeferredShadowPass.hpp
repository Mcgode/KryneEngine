/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>

namespace KryneEngine::Samples
{
    /**
     * @brief Resolves cascaded shadow map results into a screen-space shadow mask, using PCSS
     * (contact-hardening, distance-correct soft shadows) filtering.
     *
     * @details
     * Reads the GBuffer depth to reconstruct each pixel's world position, picks the cascade that
     * covers it, then runs a blocker search followed by a variable-radius PCF filter against that
     * cascade's depth slice. Reads `FullscreenPassConstants` (camera reconstruction + sun
     * direction) and `CascadedShadowMap::ConstantsBuffer` (per-cascade view-projection matrices +
     * split distances) as two of its own private constant buffers, following the same pattern as
     * `SkyAmbientPass` - callers own and update those buffers elsewhere and just hand in the
     * buffer views each frame via #UpdateSceneConstants.
     */
    class DeferredShadowPass
    {
    public:
        explicit DeferredShadowPass(AllocatorInstance _allocator);

        void Initialize(
            GraphicsContext* _graphicsContext,
            TextureViewHandle _gBufferDepth,
            TextureViewHandle _gBufferNormal,
            TextureViewHandle _shadowCascadeArray,
            TextureViewHandle _deferredShadows);

        void UpdateSceneConstants(
            GraphicsContext* _graphicsContext,
            BufferViewHandle _fullscreenConstantsBufferView,
            BufferViewHandle _cascadeConstantsBufferView) const;

        void CreatePso(GraphicsContext* _graphicsContext);

        void Dispatch(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData,
            uint2 _renderSize) const;

    private:
        AllocatorInstance m_allocator;

        DescriptorSetLayoutHandle m_descriptorSetLayout {};
        DescriptorSetHandle m_descriptorSet {};

        struct Indices
        {
            u32 m_fullscreenConstants;
            u32 m_cascadeConstants;
            u32 m_gBufferDepth;
            u32 m_gBufferNormal;
            u32 m_shadowCascades;
            u32 m_sampler;
            u32 m_output;

            [[nodiscard]] u32* Get() { return &m_fullscreenConstants; }
        } m_indices {};

        SamplerHandle m_sampler {};
        PipelineLayoutHandle m_pipelineLayout {};
        ComputePipelineHandle m_pso {};
    };
}
