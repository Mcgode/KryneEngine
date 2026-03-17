/**
 * @file
 * @author Max Godefroy
 * @date 03/04/2025.
 */

#include "DeferredShadingPass.hpp"
#include "FullscreenPassCommon.hpp"

#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include <fstream>

namespace KryneEngine::Samples::RenderGraphDemo
{

    DeferredShadingPass::DeferredShadingPass(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {
        static_assert(
            offsetof(DeferredShadingPass, m_sceneConstantsDescriptorSet) + sizeof(DescriptorSetHandle)
            == offsetof(DeferredShadingPass, m_textureDescriptors),
            "The two descriptor sets must be contiguous in memory, to load them in the same 2-wide span");
    }

    void DeferredShadingPass::Initialize(
        GraphicsContext* _graphicsContext,
        DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
        TextureViewHandle _gBufferAlbedo,
        TextureViewHandle _gBufferNormal,
        TextureViewHandle _gBufferDepth,
        TextureViewHandle _deferredShadows,
        TextureViewHandle _gBufferAmbient)
    {
        u32 indices[5];

        // Create texture descriptor set layout
        {
            const DescriptorBindingDesc bindings[] {
                // Albedo
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
                // Normal
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
                // Depth
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
                // Deferred shadows
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
                // Ambient
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment
                },
            };
            const DescriptorSetDesc desc {
                .m_bindings = bindings
            };

            m_texturesDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(desc, indices);
        }

        // Create and fill textures descriptor set
        {
            m_textureDescriptors = _graphicsContext->CreateDescriptorSet(m_texturesDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData gBufferAlbedoDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferAlbedo.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData gBufferNormalDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferNormal.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData gBufferDepthDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferDepth.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData deferredShadowsDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _deferredShadows.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData gBufferAmbientDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferAmbient.m_handle,
                }
            };
            const DescriptorSetWriteInfo writeInfo[] = {
                {
                    .m_index = indices[0],
                    .m_descriptorData = gBufferAlbedoDescriptorData
                },
                {
                    .m_index = indices[1],
                    .m_descriptorData = gBufferNormalDescriptorData
                },
                {
                    .m_index = indices[2],
                    .m_descriptorData = gBufferDepthDescriptorData
                },
                {
                    .m_index = indices[3],
                    .m_descriptorData = deferredShadowsDescriptorData
                },
                {
                    .m_index = indices[4],
                    .m_descriptorData = gBufferAmbientDescriptorData
                },
            };
            _graphicsContext->UpdateDescriptorSet(m_textureDescriptors, {writeInfo}, false);
        }

        // Create PSO layout
        {
            const DescriptorSetLayoutHandle descriptorSetLayouts[] {
                _sceneConstantsDescriptorSetLayout,
                m_texturesDescriptorSetLayout
            };
            const PushConstantDesc pushConstants[] {
                {
                    .m_sizeInBytes = sizeof(u32),
                    .m_visibility = ShaderVisibility::Vertex,
                }
            };

            const PipelineLayoutDesc layoutDesc = {
                .m_descriptorSets = descriptorSetLayouts,
                .m_pushConstants = pushConstants,
            };

            m_pipelineLayout = _graphicsContext->CreatePipelineLayout(layoutDesc);
        }
    }

    void DeferredShadingPass::Render(
        const Modules::RenderGraph::RenderGraph& _,
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

    void DeferredShadingPass::CreatePso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass)
    {
        if (m_pso != GenPool::kInvalidHandle)
            return;

        m_pso = FullscreenPassCommon::CreatePso(
            _graphicsContext,
            m_allocator,
            _renderPass,
            m_pipelineLayout,
            "Shaders/Samples/RenderGraphDemo/DeferredShading_DeferredShadingMain",
            "DeferredShadingMain",
            false);
    }
}