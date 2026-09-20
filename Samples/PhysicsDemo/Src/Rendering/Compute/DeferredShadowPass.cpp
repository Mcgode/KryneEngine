/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "DeferredShadowPass.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <fstream>

namespace KryneEngine::Samples::PhysicsDemo
{
    DeferredShadowPass::DeferredShadowPass(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    void DeferredShadowPass::Initialize(
        GraphicsContext* _graphicsContext,
        const TextureViewHandle _gBufferDepth,
        const TextureViewHandle _deferredShadows)
    {
        u32 indices[2];

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
                },
            };
            m_descriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                indices);
        }

        m_descriptorSet = _graphicsContext->CreateDescriptorSet(m_descriptorSetLayout);

        {
            const DescriptorSetWriteInfo::DescriptorData gBufferDepthData[] {
                {
                    .m_textureLayout = TextureLayout::ShaderResource,
                    .m_handle = _gBufferDepth.m_handle,
                }
            };
            const DescriptorSetWriteInfo::DescriptorData deferredShadowsData[] {
                {
                    .m_textureLayout = TextureLayout::UnorderedAccess,
                    .m_handle = _deferredShadows.m_handle,
                }
            };
            const DescriptorSetWriteInfo writes[] {
                {
                    .m_index = indices[0],
                    .m_descriptorData = gBufferDepthData,
                },
                {
                    .m_index = indices[1],
                    .m_descriptorData = deferredShadowsData,
                },
            };

            _graphicsContext->UpdateDescriptorSet(m_descriptorSet, writes, false);
        }

        {
            const DescriptorSetLayoutHandle sets[] { m_descriptorSetLayout };
            m_pipelineLayout = _graphicsContext->CreatePipelineLayout({ .m_descriptorSets = sets });
        }
    }

    void DeferredShadowPass::CreatePso(GraphicsContext* _graphicsContext)
    {
        if (m_pso != GenPool::kInvalidHandle)
            return;

        char path[256];
        snprintf(
            path,
            sizeof(path),
            "Shaders/Samples/PhysicsDemo/DeferredShadows_DeferredShadowsMain.%s",
            GraphicsContext::GetShaderFileExtension());

        std::ifstream file(path, std::ios::binary);
        KE_ASSERT_MSG(file, "Could not open deferred shadows shader");

        file.seekg(0, std::ios::end);
        const size_t size = file.tellg();
        void* bytecode = m_allocator.allocate(size);
        file.seekg(0, std::ios::beg);
        KE_VERIFY(file.read(static_cast<char*>(bytecode), size));

        const ShaderModuleHandle module = _graphicsContext->RegisterShaderModule(bytecode, size);

        m_pso = _graphicsContext->CreateComputePipeline({
            .m_computeStage = {
                .m_shaderModule = module,
                .m_stage = ShaderStage::Stage::Compute,
                .m_entryPoint = "DeferredShadowsMain",
            },
            .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
            .m_debugName = "DeferredShadowPSO",
#endif
        });

        _graphicsContext->FreeShaderModule(module);
        m_allocator.deallocate(bytecode);
    }

    void DeferredShadowPass::Dispatch(
        const Modules::RenderGraph::RenderGraph& /*_renderGraph*/,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData,
        const uint2 _renderSize) const
    {
        if (m_pso == GenPool::kInvalidHandle)
            return;

        GraphicsContext* graphicsContext = _passExecutionData.m_graphicsContext;
        const ComputeCommandEncoderHandle computeEncoder = _passExecutionData.m_computeEncoder;

        const DescriptorSetHandle sets[] { m_descriptorSet };
        graphicsContext->SetComputePipeline(computeEncoder, m_pso);
        graphicsContext->SetComputeDescriptorSets(computeEncoder, m_pipelineLayout, sets);
        graphicsContext->Dispatch(
            computeEncoder,
            uint3 { (_renderSize.x + 7) / 8, (_renderSize.y + 7) / 8, 1 },
            uint3 { 8, 8, 1 });
    }
}
