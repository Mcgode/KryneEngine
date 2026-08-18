/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#include "ColorMappingPass.hpp"

#include "FullscreenPassCommon.hpp"

#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"

namespace KryneEngine::Samples::RenderGraphDemo
{
    ColorMappingPass::ColorMappingPass(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {
        static_assert(
            offsetof(ColorMappingPass, m_sceneConstantsDescriptorSet) + sizeof(DescriptorSetHandle)
                == offsetof(ColorMappingPass, m_inputColorDescriptorSet),
            "The two descriptor sets must be contiguous in memory, to load them in the same 2-wide span");
    }

    ColorMappingPass::~ColorMappingPass() = default;

    void ColorMappingPass::Initialize(
        GraphicsContext* _graphicsContext,
        DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
        TextureViewHandle _hdrSrv)
    {
        u32 indices[1];

        // Create input color descriptor set layout
        {
            const DescriptorBindingDesc bindings[] {
                // Input color
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
            };
            const DescriptorSetDesc desc {
                .m_bindings = bindings,
            };

            m_inputColorDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(desc, indices);
        }

        // Create and fill input color descriptor set
        {
            m_inputColorDescriptorSet = _graphicsContext->CreateDescriptorSet(m_inputColorDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData descriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _hdrSrv.m_handle,
                }
            };
            const DescriptorSetWriteInfo writeInfo[] = {
                {
                    .m_index = indices[0],
                    .m_descriptorData = descriptorData
                },
            };

            _graphicsContext->UpdateDescriptorSet(m_inputColorDescriptorSet, writeInfo, false);
        }

        // Create PSO layout
        {
            const DescriptorSetLayoutHandle descriptorSetLayouts[] {
                _sceneConstantsDescriptorSetLayout,
                m_inputColorDescriptorSetLayout
            };
            const PushConstantDesc pushConstants[] {
                {
                    .m_sizeInBytes = sizeof(u32),
                    .m_visibility = ShaderVisibility::Vertex,
                }
            };
            const PipelineLayoutDesc layoutDesc = {
                .m_descriptorSets = descriptorSetLayouts,
                .m_pushConstants = pushConstants
            };

            m_pipelineLayout = _graphicsContext->CreatePipelineLayout(layoutDesc);
        }
    }

    void ColorMappingPass::Render(
        const Modules::RenderGraph::RenderGraph&,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData)
    {
        if (m_pso == GenPool::kInvalidHandle)
            return;

        const uint2 viewportSize = _passExecutionData.m_graphicsContext->GetPresentFrameBufferSize();
        FullscreenPassCommon::Render(
            _passExecutionData.m_graphicsContext,
            _passExecutionData.m_commandList,
            viewportSize.x,
            viewportSize.y,
            1.f,
            m_pso,
            m_pipelineLayout,
            { &m_sceneConstantsDescriptorSet, 2 }); // Both handles are contiguous in memory
    }

    void ColorMappingPass::CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass)
    {
        if (m_pso != GenPool::kInvalidHandle)
            return;

        m_pso = FullscreenPassCommon::CreatePso(
            _graphicsContext,
            m_allocator,
            _renderPass,
            m_pipelineLayout,
            "Shaders/Samples/RenderGraphDemo/ColorMapping_ColorMappingMain",
            "ColorMappingMain",
            false);
    }
}