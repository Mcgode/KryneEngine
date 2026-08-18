/**
 * @file
 * @author Max Godefroy
 * @date 03/04/2025.
 */

#pragma once

#include <KryneEngine/Modules/RenderGraph/Builder.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class DeferredShadingPass
    {
    public:
        DeferredShadingPass(AllocatorInstance _allocator);
        ~DeferredShadingPass() = default;

        void Initialize(
            GraphicsContext* _graphicsContext,
            DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
            TextureViewHandle _gBufferAlbedo,
            TextureViewHandle _gBufferNormal,
            TextureViewHandle _gBufferDepth,
            TextureViewHandle _deferredShadows,
            TextureViewHandle _gBufferAmbient);

        void UpdateSceneConstants(DescriptorSetHandle _sceneConstantsDescriptorSet)
        {
            m_sceneConstantsDescriptorSet = _sceneConstantsDescriptorSet;
        }

        void Render(const Modules::RenderGraph::RenderGraph&, const Modules::RenderGraph::PassExecutionData& _passExecutionData);

        void CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass);

    private:
        AllocatorInstance m_allocator;

        DescriptorSetLayoutHandle m_texturesDescriptorSetLayout {};
        DescriptorSetHandle m_sceneConstantsDescriptorSet {};
        DescriptorSetHandle m_textureDescriptors {};

        PipelineLayoutHandle m_pipelineLayout {};
        GraphicsPipelineHandle m_pso {};

    };
}
