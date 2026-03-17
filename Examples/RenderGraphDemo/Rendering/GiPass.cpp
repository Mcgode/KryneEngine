/**
 * @file
 * @author Max Godefroy
 * @date 15/05/2025.
 */

#include "GiPass.hpp"

#include <fstream>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>

namespace KryneEngine::Samples::RenderGraphDemo
{
    GiPass::GiPass(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {
        static_assert(
            offsetof(GiPass, m_sceneConstantsDescriptorSet) + sizeof(DescriptorSetHandle)
                == offsetof(GiPass, m_texturesDescriptorSet),
            "The two descriptor sets must be contiguous in memory, to load them in the same 2-wide span");
    }

    void GiPass::Initialize(
        GraphicsContext* _graphicsContext,
        DescriptorSetLayoutHandle _sceneConstantsDescriptorSetLayout,
        TextureViewHandle _gBufferAlbedo,
        TextureViewHandle _gBufferNormal,
        TextureViewHandle _gBufferDepth,
        TextureViewHandle _gBufferAmbient)
    {
        u32 indices[4];

        // Create texture descriptor set layout
        {
            const DescriptorBindingDesc bindings[] {
                // GBuffer albedo
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // GBuffer normal
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // GBuffer depth
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // GBuffer ambient
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
            const DescriptorSetWriteInfo::DescriptorData gBufferAmbientDescriptorData[] {
                {
                    .m_textureLayout = TextureLayout::UnorderedAccess,
                    .m_handle = _gBufferAmbient.m_handle,
                }};
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
                    .m_descriptorData = gBufferAmbientDescriptorData
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
            auto shaderPath = eastl::string("Shaders/Samples/RenderGraphDemo/Gi_GiMain")
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
                    .m_entryPoint = "GiMain",
                },
                .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
                .m_debugName = "GiPassPSO",
#endif
            });

            _graphicsContext->FreeShaderModule(shaderModule);
            m_allocator.deallocate(data);
        }
    }

    void GiPass::Render(const Modules::RenderGraph::PassExecutionData& _passExecutionData)
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