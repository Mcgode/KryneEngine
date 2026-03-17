/**
 * @file
 * @author Max Godefroy
 * @date 30/04/2025.
 */

#include "DeferredShadowPass.hpp"

#include <fstream>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    DeferredShadowPass::DeferredShadowPass(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {
        static_assert(
            offsetof(DeferredShadowPass, m_sceneConstantsDescriptorSet) + sizeof(DescriptorSetHandle)
                == offsetof(DeferredShadowPass, m_texturesDescriptorSet),
            "The two descriptor sets must be contiguous in memory, to load them in the same 2-wide span");
    }

    void DeferredShadowPass::Initialize(
        GraphicsContext* _graphicsContext,
        DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
        TextureViewHandle _gBufferDepth,
        TextureViewHandle _deferredShadows)
    {
        u32 indices[2];

        // Create texture descriptor set layout
        {
            const DescriptorBindingDesc bindings[] {
                // GBuffer depth
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // Deferred shadows
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadWriteTexture,
                    .m_visibility = ShaderVisibility::Compute,
                }
            };
            const DescriptorSetDesc desc {
                .m_bindings = bindings,
            };

            m_texturesDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(desc, indices);
        }

        // Create and fill textures descriptor set
        {
            m_texturesDescriptorSet = _graphicsContext->CreateDescriptorSet(m_texturesDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData gBufferDepthDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferDepth.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData deferredShadowsDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::UnorderedAccess,
                    .m_handle = _deferredShadows.m_handle,
            }};
            const DescriptorSetWriteInfo writeInfo[] = {
                {
                    .m_index = indices[0],
                    .m_descriptorData = gBufferDepthDescriptorData
                },
                {
                    .m_index = indices[1],
                    .m_descriptorData = deferredShadowsDescriptorData
                },
            };

            _graphicsContext->UpdateDescriptorSet(m_texturesDescriptorSet, {writeInfo}, false);
        }

        // Create PSO layout
        {
            const DescriptorSetLayoutHandle descriptorSetLayouts[] {
                _sceneConstantsDescriptorSetLayout,
                m_texturesDescriptorSetLayout
            };

            m_pipelineLayout = _graphicsContext->CreatePipelineLayout(
                {
                    .m_descriptorSets = descriptorSetLayouts,
                });
        }

        // Create compute PSO
        {
            auto shaderPath = eastl::string("Shaders/Samples/RenderGraphDemo/DeferredShadows_DeferredShadowsMain")
                              + "." + _graphicsContext->GetShaderFileExtension();;

            std::ifstream file(shaderPath.c_str(), std::ios::binary);
            KE_ASSERT(file);

            file.seekg(0, std::ios::end);
            const size_t size = file.tellg();
            void* data = m_allocator.allocate(size);
            file.seekg(0, std::ios::beg);

            KE_VERIFY(file.read(reinterpret_cast<char*>(data), size));

            const ShaderModuleHandle shaderModule = _graphicsContext->RegisterShaderModule(data, size);

            m_pso = _graphicsContext->CreateComputePipeline({
                .m_computeStage = {
                    .m_shaderModule = shaderModule,
                    .m_stage = ShaderStage::Stage::Compute,
                    .m_entryPoint = "DeferredShadowsMain",
                },
                .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
                .m_debugName = "DeferredShadowPSO",
#endif
            });

            _graphicsContext->FreeShaderModule(shaderModule);
            m_allocator.deallocate(data);
        }
    }

    void DeferredShadowPass::Render(const Modules::RenderGraph::PassExecutionData& _passExecutionData)
    {
        if (m_pso == GenPool::kInvalidHandle)
            return;

        const GraphicsCommon::ApplicationInfo& appInfo = _passExecutionData.m_graphicsContext->GetApplicationInfo();

        _passExecutionData.m_graphicsContext->SetComputePipeline(
            _passExecutionData.m_commandList,
            m_pso);
        _passExecutionData.m_graphicsContext->SetComputeDescriptorSets(
            _passExecutionData.m_commandList,
            m_pipelineLayout,
            { &m_sceneConstantsDescriptorSet, 2 });

        const uint2 viewportSize = _passExecutionData.m_graphicsContext->GetPresentFrameBufferSize();
        _passExecutionData.m_graphicsContext->Dispatch(
            _passExecutionData.m_commandList,
            uint3 {
                (viewportSize.x + 7) / 8,
                (viewportSize.y + 7) / 8,
                1
            },
            uint3 { 8, 8, 1 });
    }
}