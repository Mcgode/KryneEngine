/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#include "../../../Include/Rendering/Compute/SkyAmbientPass.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/Enums.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"

#include <EASTL/string.h>
#include <fstream>

namespace KryneEngine::Samples
{
    SkyAmbientPass::SkyAmbientPass(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    namespace
    {
        constexpr u32 kShCoeffCount = 9;
    }

    void SkyAmbientPass::Initialize(GraphicsContext* _graphicsContext)
    {
        m_skyAmbientBuffer = _graphicsContext->CreateBuffer(
            {
                .m_desc = {
                    .m_size = sizeof(float4) * kShCoeffCount,
#if !defined(KE_FINAL)
                    .m_debugName = "SkyAmbientBuffer",
#endif
                },
                .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ReadWriteBuffer,
            });

        m_skyAmbientBufferView = _graphicsContext->CreateBufferView({
            .m_buffer = m_skyAmbientBuffer,
            .m_size = sizeof(float4) * kShCoeffCount,
            .m_stride = sizeof(float4),
            .m_accessType = BufferViewAccessType::ReadWrite,
#if !defined(KE_FINAL)
            .m_debugName = "SkyAmbientBufferView",
#endif
        });

        {
            constexpr DescriptorBindingDesc bindings[] {
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Compute,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadWriteBuffer,
                    .m_visibility = ShaderVisibility::Compute,
                }
            };
            m_descriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                m_indices.Get());
        }

        m_descriptorSet = _graphicsContext->CreateDescriptorSet(m_descriptorSetLayout);

        {
            const DescriptorSetWriteInfo::DescriptorData writeData[] {
                {
                    .m_handle = m_skyAmbientBufferView.m_handle,
                }
            };

            const DescriptorSetWriteInfo writes[] = {
                {
                    .m_index = m_indices.m_skyAmbient,
                    .m_descriptorData = writeData,
                }
            };

            _graphicsContext->UpdateDescriptorSet(m_descriptorSet, writes, false);
        }

        {
            const DescriptorSetLayoutHandle sets[] { m_descriptorSetLayout };
            m_pipelineLayout = _graphicsContext->CreatePipelineLayout({ .m_descriptorSets = sets });
        }
    }

    void SkyAmbientPass::UpdateSceneConstants(
        GraphicsContext* _graphicsContext,
        const BufferViewHandle _sceneConstantsBufferView) const
    {
        const DescriptorSetWriteInfo::DescriptorData writeData[] {
            {
                .m_handle = _sceneConstantsBufferView.m_handle,
            }
        };

        const DescriptorSetWriteInfo writes[] = {
            {
                .m_index = m_indices.m_sceneConstants,
                .m_descriptorData = writeData,
            }
        };

        _graphicsContext->UpdateDescriptorSet(m_descriptorSet, writes, true);
    }

    void SkyAmbientPass::Dispatch(
        const Modules::RenderGraph::RenderGraph& /*_renderGraph*/,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData) const
    {
        if (m_pso == GenPool::kInvalidHandle)
            return;

        GraphicsContext* graphicsContext = _passExecutionData.m_graphicsContext;
        const ComputeCommandEncoderHandle computeEncoder = _passExecutionData.m_computeEncoder;

        const DescriptorSetHandle sets[] { m_descriptorSet };
        graphicsContext->SetComputePipeline(computeEncoder, m_pso);
        graphicsContext->SetComputeDescriptorSets(computeEncoder, m_pipelineLayout, sets);
        graphicsContext->Dispatch(computeEncoder, { 1, 1, 1 }, { 128, 1, 1 });
    }

    void SkyAmbientPass::CreatePso(GraphicsContext* _graphicsContext)
    {
        if (m_pso != GenPool::kInvalidHandle)
            return;

        char path[256];
        snprintf(
            path,
            sizeof(path),
            "Shaders/Samples/CommonLib/Sky/SkyLightingBake_SkyLightingBakeMain.%s",
            GraphicsContext::GetShaderFileExtension());

        std::ifstream file(path, std::ios::binary);
        KE_ASSERT_MSG(file, "Could not open sky ambient bake shader");

        file.seekg(0, std::ios::end);
        const size_t size = file.tellg();
        void* bytecode = m_allocator.allocate(size);
        file.seekg(0, std::ios::beg);
        KE_VERIFY(file.read(static_cast<char*>(bytecode), size));

        const ShaderModuleHandle module = _graphicsContext->RegisterShaderModule(bytecode, size);

        m_pso = _graphicsContext->CreateComputePipeline({
            .m_computeStage = {
                .m_shaderModule = module,
                .m_stage        = ShaderStage::Stage::Compute,
                .m_entryPoint   = "SkyLightingBakeMain",
            },
            .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
            .m_debugName = "SkyAmbientBakePSO",
#endif
        });

        _graphicsContext->FreeShaderModule(module);
        m_allocator.deallocate(bytecode);
    }
}
