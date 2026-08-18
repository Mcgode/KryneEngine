/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#pragma once

#include <KryneEngine/Modules/RenderGraph/Builder.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class ColorMappingPass
    {
    public:
        explicit ColorMappingPass(AllocatorInstance _allocator);
        ~ColorMappingPass();

        void Initialize(
            GraphicsContext* _graphicsContext,
            DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
            TextureViewHandle _hdrSrv);

        void UpdateSceneConstants(DescriptorSetHandle _sceneConstantsDescriptorSet)
        {
            m_sceneConstantsDescriptorSet = _sceneConstantsDescriptorSet;
        }

        void Render(const Modules::RenderGraph::RenderGraph&, const Modules::RenderGraph::PassExecutionData& _passExecutionData);

        void CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass);

    private:
        AllocatorInstance m_allocator;

        DescriptorSetLayoutHandle m_inputColorDescriptorSetLayout {};
        DescriptorSetHandle m_sceneConstantsDescriptorSet {};
        DescriptorSetHandle m_inputColorDescriptorSet {};

        PipelineLayoutHandle m_pipelineLayout {};
        GraphicsPipelineHandle m_pso {};
    };
}
