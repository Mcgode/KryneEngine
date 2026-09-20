/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>

namespace KryneEngine::Samples::PhysicsDemo
{
    /**
     * @brief Placeholder compute pass for deferred shadows.
     *
     * @details
     * Reads the GBuffer depth and, for every pixel that isn't the far plane, writes an
     * unshadowed value (1) to the deferred shadows target. Meant to be replaced later with
     * actual shadow evaluation against the physics scene.
     */
    class DeferredShadowPass
    {
    public:
        explicit DeferredShadowPass(AllocatorInstance _allocator);

        void Initialize(
            GraphicsContext* _graphicsContext,
            TextureViewHandle _gBufferDepth,
            TextureViewHandle _deferredShadows);

        void CreatePso(GraphicsContext* _graphicsContext);

        void Dispatch(
            const Modules::RenderGraph::RenderGraph& _renderGraph,
            const Modules::RenderGraph::PassExecutionData& _passExecutionData,
            uint2 _renderSize) const;

    private:
        AllocatorInstance m_allocator;

        DescriptorSetLayoutHandle m_descriptorSetLayout {};
        DescriptorSetHandle m_descriptorSet {};

        PipelineLayoutHandle m_pipelineLayout {};
        ComputePipelineHandle m_pso {};
    };
}
