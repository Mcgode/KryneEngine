/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"
#include "KryneEngine/Modules/RenderGraph/RenderGraph.hpp"

namespace KryneEngine::Samples::RenderGraphDemo
{
    class SkyPass
    {
    public:
        SkyPass(AllocatorInstance _allocator);

        void Initialize(GraphicsContext* _graphicsContext, DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout);

        void UpdateSceneConstants(DescriptorSetHandle _sceneConstantsDescriptorSet)
        {
            m_sceneConstantsDescriptorSet = _sceneConstantsDescriptorSet;
        }

        void Render(const Modules::RenderGraph::RenderGraph&, const Modules::RenderGraph::PassExecutionData& _passExecutionData);
        void CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass);

    private:
        AllocatorInstance m_allocator;
        DescriptorSetHandle m_sceneConstantsDescriptorSet {};
        PipelineLayoutHandle m_pipelineLayout {};
        GraphicsPipelineHandle m_pso {};
    };
}
