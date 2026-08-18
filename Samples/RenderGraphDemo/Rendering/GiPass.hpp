/**
 * @file
 * @author Max Godefroy
 * @date 15/05/2025.
 */

#pragma once

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    class GiPass
    {
    public:
        explicit GiPass(AllocatorInstance _allocator);

        void Initialize(
            GraphicsContext* _graphicsContext,
            DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
            TextureViewHandle _gBufferAlbedo,
            TextureViewHandle _gBufferNormal,
            TextureViewHandle _gBufferDepth,
            TextureViewHandle _gBufferAmbient);

        void UpdateSceneConstants(DescriptorSetHandle _sceneConstantsDescriptorSet)
        {
            m_sceneConstantsDescriptorSet = _sceneConstantsDescriptorSet;
        }

        void Render(const Modules::RenderGraph::PassExecutionData& _passExecutionData);

    private:
        AllocatorInstance m_allocator;

        DescriptorSetHandle m_sceneConstantsDescriptorSet {};
        DescriptorSetHandle m_texturesDescriptorSet {};
        DescriptorSetLayoutHandle m_texturesDescriptorSetLayout {};

        PipelineLayoutHandle m_pipelineLayout {};
        ComputePipelineHandle m_pso {};
    };
}
