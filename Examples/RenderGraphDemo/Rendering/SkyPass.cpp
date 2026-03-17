/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#include "SkyPass.hpp"

#include "FullscreenPassCommon.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"

namespace KryneEngine::Samples::RenderGraphDemo
{
    SkyPass::SkyPass(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    void SkyPass::Initialize(
        GraphicsContext* _graphicsContext,
        DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout)
    {
        const PushConstantDesc pushConstants[] {
            {
                .m_sizeInBytes = sizeof(float),
                .m_visibility = ShaderVisibility::Vertex,
            }
        };
        m_pipelineLayout = _graphicsContext->CreatePipelineLayout(PipelineLayoutDesc {
            .m_descriptorSets = { &_sceneConstantsDescriptorSetLayout, 1 },
            .m_pushConstants = pushConstants,
        });
    }

    void SkyPass::Render(
        const Modules::RenderGraph::RenderGraph&,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData)
    {
        KE_ASSERT_MSG(m_pso != GenPool::kInvalidHandle, "PSO not created");

        const uint2 viewportSize = _passExecutionData.m_graphicsContext->GetPresentFrameBufferSize();
        FullscreenPassCommon::Render(
            _passExecutionData.m_graphicsContext,
            _passExecutionData.m_commandList,
            viewportSize.x,
            viewportSize.y,
            0.f,
            m_pso,
            m_pipelineLayout,
            { &m_sceneConstantsDescriptorSet, 1 }
        );
    }

    void SkyPass::CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass)
    {
        if (m_pso != GenPool::kInvalidHandle)
            return;

        m_pso = FullscreenPassCommon::CreatePso(
            _graphicsContext,
            m_allocator,
            _renderPass,
            m_pipelineLayout,
            "Shaders/Samples/RenderGraphDemo/Sky/SkyRender_SkyMain",
            "SkyMain",
            true);
    }
}